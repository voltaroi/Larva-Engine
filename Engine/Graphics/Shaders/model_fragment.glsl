#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform vec3 objectColor;
uniform float objectAlpha;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform sampler2D shadowMap;
uniform sampler2D diffuseTexture;
uniform bool hasTexture;

// Poisson disk (16 samples) used with ESM filtering
const vec2 poissonDisk[16] = vec2[16](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

// Tunables
const float LIGHT_WORLD_SIZE = 0.09;  // Smaller source for tighter edges
const float LIGHT_FRUSTUM_WIDTH = 40.0; // Matches ortho width used in C++
const float NEAR_PLANE = 1.0;
const float FAR_PLANE = 50.0;
const float NORMAL_OFFSET = 0.0015;

// 4x4 blue-noise seeds for rotation
const float blueNoise[16] = float[](
    0.15, 0.83, 0.37, 0.62,
    0.91, 0.28, 0.48, 0.04,
    0.56, 0.12, 0.71, 0.34,
    0.26, 0.97, 0.08, 0.68
);

float blueNoiseSample(vec2 uv)
{
    ivec2 p = ivec2(mod(floor(uv * 1024.0), 4.0));
    int idx = (p.y * 4 + p.x) & 15;
    return blueNoise[idx];
}

float computeBias(vec3 normal, vec3 lightDir)
{
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    return mix(0.0003, 0.0018, 1.0 - cosTheta);
}

float esmVisibility(float receiverDepth, float sampleDepth, float k)
{
    float v = exp(k * (sampleDepth - receiverDepth));
    return clamp(v, 0.0, 1.0);
}

float esmShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    projCoords.xy = clamp(projCoords.xy, vec2(0.001), vec2(0.999));

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float bias = computeBias(normal, lightDir);
    float receiverDepth = currentDepth - bias - NORMAL_OFFSET;

    // derived from proj coords to rotate poisson disk jitter smoothly.
    float angle = blueNoiseSample(projCoords.xy) * 6.2831853;    float ca = cos(angle);
    float sa = sin(angle);
    mat2 rot = mat2(ca, -sa, sa, ca);

    float lightSizeUV = LIGHT_WORLD_SIZE / LIGHT_FRUSTUM_WIDTH;
    // Increase filter radius to produce softer penumbra
    float filterRadius = mix(1.0 * texelSize.x, 12.0 * texelSize.x, clamp((receiverDepth - NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE), 0.0, 1.0));

    // VSM: sample moments (depth, depth^2) and use Chebyshev inequality
    float shadow = 0.0;
    int samples = int(mix(4.0, 24.0, clamp(receiverDepth, 0.0, 1.0)));
    for (int i = 0; i < samples; ++i) {
        vec2 offset = rot * (poissonDisk[i] * filterRadius * lightSizeUV * 3.0);
        vec2 moments = texture(shadowMap, projCoords.xy + offset).rg;
        float mean = moments.x;
        float mean2 = moments.y;
        float variance = mean2 - mean * mean;
        // larger min variance to reduce light bleeding artifacts
        variance = max(variance, 0.0005);
        float d = receiverDepth;
        // Chebyshev upper bound on probability that sampleDepth <= d
        float p = variance / (variance + (d - mean) * (d - mean));
        p = clamp(p, 0.0, 1.0);
        float contrib = (d <= mean) ? 0.0 : (1.0 - p);
        shadow += contrib;
    }
    shadow /= float(samples);
    // Slight smoothing for final blend
    shadow = smoothstep(0.0, 1.0, shadow);
    return shadow;
}

vec4 legacyShade()
{
    vec4 texColor = texture(diffuseTexture, TexCoord);
    vec3 baseColor = hasTexture ? texColor.rgb : objectColor;
    
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor * baseColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = max(dot(viewDir, reflectDir), 0.0);
    spec = spec * spec; // ^2
    spec = spec * spec; // ^4
    spec = spec * spec; // ^8
    vec3 specular = specularStrength * spec * lightColor;

    float shadow = esmShadow(FragPosLightSpace, norm, lightDir);
    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * baseColor;
    return vec4(result, objectAlpha);
}

// ============================================================================
//  Environment lighting (enabled with Model::SetEnvironment)
//  Keep cloudDensity() identical to the one used by custom game shaders so that
//  cloud shadows line up on every surface.
// ============================================================================
uniform int uEnv;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uSkyAmbient;
uniform vec3 uGroundAmbient;
uniform vec3 uZenithColor;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uCloudShadow;
uniform float uCloudCoverage;
uniform float uCloudScale;
uniform float uCloudHeight;
uniform vec2 uCloudOffset;
uniform bool uLinearOutput;
uniform float uSpecular;
uniform float uShininess;
uniform float uEmissive;

float envHash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float envNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(envHash(i), envHash(i + vec2(1.0, 0.0)), u.x),
               mix(envHash(i + vec2(0.0, 1.0)), envHash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float envFbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < 5; ++i)
    {
        v += a * envNoise(p);
        p = m * p;
        a *= 0.5;
    }
    return v;
}

float cloudDensity(vec2 xz)
{
    float n = envFbm(xz * uCloudScale + uCloudOffset);
    return smoothstep(1.0 - uCloudCoverage, 1.0 - uCloudCoverage + 0.3, n);
}

float cloudLight(vec3 p)
{
    vec3 L = normalize(uSunDir);
    vec2 xz = p.xz + L.xz * (uCloudHeight - p.y) / max(L.y, 0.1);
    return 1.0 - uCloudShadow * cloudDensity(xz);
}

vec3 envSky(vec3 d)
{
    float h = clamp(d.y, 0.0, 1.0);
    vec3 col = mix(uFogColor, uZenithColor, pow(h, 0.5));
    if (d.y < 0.0)
        col = mix(uFogColor, uGroundAmbient * 1.5, clamp(-d.y * 3.0, 0.0, 1.0));
    return col;
}

float envShadow(vec4 lightSpacePos, vec3 N, vec3 L)
{
    vec3 c = lightSpacePos.xyz / lightSpacePos.w * 0.5 + 0.5;
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0 || c.z > 1.0)
        return 0.0;
    vec2 moments = texture(shadowMap, c.xy).rg;
    float slope = 1.0 - clamp(dot(N, L), 0.0, 1.0);
    float d = c.z - (0.0008 + 0.0025 * slope);
    float shadow = 0.0;
    if (d > moments.x)
    {
        float variance = max(moments.y - moments.x * moments.x, 0.000004);
        float diff = d - moments.x;
        float pMax = variance / (variance + diff * diff);
        pMax = clamp((pMax - 0.35) / 0.65, 0.0, 1.0); // réduit les fuites de lumière du VSM
        shadow = 1.0 - pMax;
    }
    float edge = min(min(c.x, c.y), min(1.0 - c.x, 1.0 - c.y));
    return shadow * smoothstep(0.0, 0.06, edge);
}

vec3 acesTonemap(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec4 environmentShade()
{
    vec3 base = hasTexture ? texture(diffuseTexture, TexCoord).rgb : objectColor;
    vec3 albedo = pow(base, vec3(2.2));

    vec3 N = normalize(Normal);
    vec3 L = normalize(uSunDir);
    vec3 V = normalize(viewPos - FragPos);
    if (dot(N, V) < 0.0)
        N = -N; // faces vues de dos (meshes ouverts)

    float ndl = max(dot(N, L), 0.0);
    float visibility = (1.0 - envShadow(FragPosLightSpace, N, L)) * cloudLight(FragPos);

    vec3 direct = uSunColor * ndl * visibility;
    vec3 ambient = mix(uGroundAmbient, uSkyAmbient, N.y * 0.5 + 0.5);

    vec3 H = normalize(L + V);
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    float spec = pow(max(dot(N, H), 0.0), uShininess) * (uShininess + 8.0) / 25.13;
    vec3 specular = uSunColor * spec * uSpecular * mix(0.25, 1.0, fresnel) * ndl * visibility;
    vec3 reflection = envSky(reflect(-V, N)) * fresnel * uSpecular * 0.4;

    vec3 color = albedo * (direct + ambient) + specular + reflection + albedo * uEmissive * 4.0;

    // Brouillard atmosphérique, teinté par le soleil
    float dist = length(viewPos - FragPos);
    float fog = 1.0 - exp(-dist * uFogDensity);
    float sunAmount = pow(max(dot(-V, L), 0.0), 8.0);
    vec3 fogColor = mix(uFogColor, uFogColor + uSunColor * 0.15, sunAmount);
    color = mix(color, fogColor, fog);

    if (!uLinearOutput)
        color = pow(acesTonemap(color), vec3(1.0 / 2.2));
    return vec4(color, objectAlpha);
}

void main()
{
    FragColor = (uEnv == 1) ? environmentShade() : legacyShade();
}
