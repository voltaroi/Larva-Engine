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

    vec2 snapped = floor(projCoords.xy * 128.0) / 128.0;
    float angle = blueNoiseSample(snapped) * 6.2831853;
    float ca = cos(angle);
    float sa = sin(angle);
    mat2 rot = mat2(ca, -sa, sa, ca);

    float lightSizeUV = LIGHT_WORLD_SIZE / LIGHT_FRUSTUM_WIDTH;
    float filterRadius = mix(1.0 * texelSize.x, 7.5 * texelSize.x, clamp((receiverDepth - NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE), 0.0, 1.0));

    float shadow = 0.0;
    const float ESM_K = 90.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = rot * (poissonDisk[i] * filterRadius * lightSizeUV * 2.0);
        float sampleDepth = texture(shadowMap, projCoords.xy + offset).r;
        float vis = esmVisibility(receiverDepth, sampleDepth, ESM_K);
        shadow += 1.0 - vis;
    }
    shadow /= 16.0;
    return shadow;
}

void main()
{
    vec3 baseColor = objectColor;
    if (hasTexture) {
        vec4 texColor = texture(diffuseTexture, TexCoord);
        baseColor = texColor.rgb * objectColor;
    }

    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;

    float shadow = esmShadow(FragPosLightSpace, norm, lightDir);
    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * baseColor;
    FragColor = vec4(result, objectAlpha);
}
