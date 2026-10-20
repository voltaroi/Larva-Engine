#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;
in vec3 LocalPos;
in vec3 LocalNormal;

out vec4 FragColor;

uniform vec3 objectColor;
uniform float objectAlpha;
// Instanced draws (Model::drawInstanced) carry colour and material per instance
uniform bool uInstanced;
flat in vec4 InstColor;
flat in vec3 InstMaterial; // specular, shininess, emissive
vec3 gObjectColor;
float gObjectAlpha, gSpecular, gShininess, gEmissive;
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
    vec3 baseColor = hasTexture ? texColor.rgb : gObjectColor;
    
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
    return vec4(result, gObjectAlpha);
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
// Point lights (Model::SetPointLights) : xyz position, w radius ; colour in linear HDR
const int MAX_POINT_LIGHTS = 32;
uniform int uPointCount;
uniform vec4 uPointPos[MAX_POINT_LIGHTS];
uniform vec3 uPointColor[MAX_POINT_LIGHTS];
uniform vec4 uPointDir[MAX_POINT_LIGHTS]; // spot: xyz axis, w cosine of the half angle (<= -1: omnidirectional)
// Physically based material (Model::setPBR) and object-space detail texture (Model::setDetail)
uniform mat3 normalMatrix;
uniform vec3 uObjectScale;
uniform bool uPBR;
uniform float uRoughness;
uniform float uMetallic;
uniform bool uHasDetail;
uniform sampler2D uDetail;
uniform float uDetailScale;
uniform float uDetailStrength;
const float PI = 3.14159265;
// Point light shadows (Model::SetPointShadowGrid / SetShadowCasters): occlusion grid seen from above (RGBA: two
// obstacle bands [bottom, top] per cell, top <= bottom: none) and vertical capsules (characters)
uniform bool uGridOn;
uniform sampler2D uGrid;
uniform vec4 uGridInfo; // origin x, origin z, cell size
const int MAX_CASTERS = 24;
uniform int uCasterCount;
uniform vec4 uCaster[MAX_CASTERS]; // x, bottom y, z, radius
uniform float uCasterTop[MAX_CASTERS];

// Apparent size of a lamp (m): the penumbra at an obstacle grows with its distance to the lit point (a contact
// shadow is sharp, a far one is soft)
const float LAMP_SIZE = 0.4;

// How deep (m, <= 0: not at all) the ray's height range [lo, hi] goes into the obstacle band o = [bottom, top]
float bandDepth(vec2 o, float lo, float hi)
{
    return o.y > o.x ? min(hi, o.y) - max(lo, o.x) : -1.0;
}

// Visibility (0..1) of the point light at Lpos from the lit point P (normal N): walls, closed doors, raised floors
// and objects of the grid, then the characters. An obstacle the ray only grazes (a corner, the top of a wall, the
// edge of a table) lets part of the light through, over the width of the penumbra there.
float pointShadow(vec3 P, vec3 N, vec3 Lpos)
{
    if (!uGridOn)
        return 1.0;
    vec3 start = P + N * 0.06;
    float len = length(Lpos - start);
    ivec2 size = textureSize(uGrid, 0);
    vec2 a = (start.xz - uGridInfo.xy) / uGridInfo.z;
    vec2 b = (Lpos.xz - uGridInfo.xy) / uGridInfo.z;
    vec2 d = b - a;
    ivec2 cell = ivec2(floor(a));
    ivec2 last = ivec2(floor(b));
    ivec2 stepDir = ivec2(d.x > 0.0 ? 1 : -1, d.y > 0.0 ? 1 : -1);
    vec2 inv = vec2(abs(d.x) > 1e-6 ? 1.0 / abs(d.x) : 1e9, abs(d.y) > 1e-6 ? 1.0 / abs(d.y) : 1e9);
    vec2 tMax = vec2(d.x > 0.0 ? (float(cell.x) + 1.0 - a.x) : (a.x - float(cell.x)),
                     d.y > 0.0 ? (float(cell.y) + 1.0 - a.y) : (a.y - float(cell.y))) * inv;
    float tEnter = 0.0;
    float vis = 1.0, runCover = 0.0, runChord = 0.0;
    // Not tested: the first cell (the lit surface itself) and any cell left within 25 cm of it, and the lamp's own
    // metre of floor (its fixture, the machine it lights)
    ivec2 lampCell = ivec2(floor(Lpos.xz));
    for (int i = 0; i < 160; ++i)
    {
        if (cell == last)
            break;
        float tExit = min(min(tMax.x, tMax.y), 1.0);
        bool blocked = false;
        bool lampOwn = ivec2(floor(uGridInfo.xy + (vec2(cell) + 0.5) * uGridInfo.z)) == lampCell;
        if (i > 0 && tExit * len > 0.25 && !lampOwn && cell.x >= 0 && cell.y >= 0 && cell.x < size.x && cell.y < size.y)
        {
            vec4 o = texelFetch(uGrid, cell, 0);
            float ya = mix(start.y, Lpos.y, tEnter), yb = mix(start.y, Lpos.y, tExit);
            float lo = min(ya, yb), hi = max(ya, yb);
            float depth = max(bandDepth(o.xy, lo, hi), bandDepth(o.zw, lo, hi));
            if (depth > 0.0)
            {
                // Across the obstacle (the blocked cells in a row): a ray clipping a corner crosses it on a short chord
                runChord += (tExit - tEnter) * len;
                depth = min(depth, runChord * 0.5);
                float pen = max(LAMP_SIZE * 0.5 * (tEnter + tExit), 0.015);
                runCover = max(runCover, smoothstep(0.0, pen, depth));
                if (runCover > 0.99)
                    return 0.0;
                blocked = true;
            }
        }
        if (!blocked)
        {
            vis *= 1.0 - runCover;
            runCover = 0.0, runChord = 0.0;
        }
        if (tMax.x < tMax.y)
        {
            cell.x += stepDir.x;
            tMax.x += inv.x;
        }
        else
        {
            cell.y += stepDir.y;
            tMax.y += inv.y;
        }
        tEnter = tExit;
        if (tEnter >= 1.0)
            break;
    }
    vis *= 1.0 - runCover;
    // Characters: the ray passes close to the axis of a capsule, within its height
    vec2 sd = Lpos.xz - start.xz;
    float len2 = max(dot(sd, sd), 1e-6);
    for (int c = 0; c < uCasterCount; ++c)
    {
        vec4 k = uCaster[c];
        vec2 axis = vec2(k.x, k.z);
        // The lit point belongs to this body: no shadow of itself
        if (distance(start.xz, axis) < k.w + 0.12 && start.y > k.y - 0.1 && start.y < uCasterTop[c] + 0.15)
            continue;
        float t = dot(axis - start.xz, sd) / len2;
        if (t <= 0.0 || t >= 1.0)
            continue;
        float pen = LAMP_SIZE * t + 0.04;
        float y = mix(start.y, Lpos.y, t);
        float dist = distance(start.xz + sd * t, axis);
        // A body is a column narrowing toward the head: soft sides, soft top
        float side = 1.0 - smoothstep(k.w * 0.75 - pen * 0.5, k.w * 0.75 + pen * 0.5, dist);
        float tall = smoothstep(k.y - pen * 0.5, k.y + pen * 0.5, y) * (1.0 - smoothstep(uCasterTop[c] - 0.15 - pen * 0.5, uCasterTop[c] - 0.15 + pen * 0.5, y));
        vis *= 1.0 - 0.9 * side * tall;
    }
    return vis;
}

// Point light shadow maps (Model::SetupPointShadowMaps): for each slot, six faces of a depth cube map seen from its light
// (depth = distance / far), in one texture array (layer = slot * 6 + face)
const int MAX_SHADOW_LAMPS = 16;
uniform bool uCubeOn;
uniform sampler2DArrayShadow uCubeShadow; // hardware comparison, bilinear
uniform sampler2DArray uCubeDepth;        // raw depth (blocker search)
uniform vec4 uCubeInfo;                   // resolution, tangent of the half angle of a face, lamp size (m), filter quality
uniform int uPointSlot[MAX_POINT_LIGHTS]; // slot of each point light, -1: none (grid)
uniform vec4 uSlotLamp[MAX_SHADOW_LAMPS]; // position and far distance of the light of each slot
uniform float uExposure;
uniform float uShadowRange; // au-dela (m de la camera), la grille au lieu des cartes d'ombre

const vec3 CUBE_F[6] = vec3[6](vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0), vec3(0, -1, 0), vec3(0, 0, 1), vec3(0, 0, -1));
const vec3 CUBE_U[6] = vec3[6](vec3(0, 1, 0), vec3(0, 1, 0), vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 1, 0), vec3(0, 1, 0));

// Texture coordinates and layer of the direction v (from the light) in slot s
vec3 cubeCoord(int s, vec3 v)
{
    vec3 a = abs(v);
    int face;
    float major;
    if (a.x >= a.y && a.x >= a.z)
    {
        face = v.x > 0.0 ? 0 : 1;
        major = a.x;
    }
    else if (a.y >= a.z)
    {
        face = v.y > 0.0 ? 2 : 3;
        major = a.y;
    }
    else
    {
        face = v.z > 0.0 ? 4 : 5;
        major = a.z;
    }
    vec3 F = CUBE_F[face], U = CUBE_U[face];
    vec3 R = cross(F, U);
    vec2 uv = vec2(dot(v, R), dot(v, U)) / (uCubeInfo.y * max(major, 1e-5)) * 0.5 + 0.5;
    return vec3(uv, float(s * 6 + face));
}

// 1: the point Q is lit by the light of slot s (bilinear comparison: smooth at the scale of a texel)
float cubeTest(int s, vec4 lamp, vec3 Q, float bias)
{
    vec3 v = Q - lamp.xyz;
    float ref = (length(v) - bias) / lamp.w;
    return texture(uCubeShadow, vec4(cubeCoord(s, v), ref));
}

float noiseIGN(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// Sample k of n spread evenly on a unit disk (golden angle spiral), turned by the angle a
vec2 vogel(int k, int n, float a)
{
    float r = sqrt((float(k) + 0.5) / float(n));
    float t = float(k) * 2.3999632 + a;
    return vec2(cos(t), sin(t)) * r;
}

// Visibility (0..1) of the light of slot s from the lit point P (normal N). The filter samples points of the surface
// around P (each one compared at its own distance: no acne on wide penumbras). Contact hardening (PCSS): the average
// distance of the obstacles found around the ray gives the width of the penumbra, sharp at the foot of an obstacle,
// soft far from it, wider for a bigger lamp.
float cubeShadow(int s, vec3 P, vec3 N)
{
    vec4 lamp = uSlotLamp[s];
    vec3 toP = P - lamp.xyz;
    float dist = length(toP);
    if (dist >= lamp.w)
        return 1.0;
    vec3 dir = toP / max(dist, 1e-4);
    float texel = 2.0 * uCubeInfo.y * dist / uCubeInfo.x; // width of a texel at the lit point (m)
    float ndl = clamp(-dot(N, dir), 0.0, 1.0);
    vec3 Pb = P + N * texel * (0.8 + 1.5 * (1.0 - ndl));
    float bias = 0.012 + texel * 0.6;
    // Basis of the surface
    vec3 b1 = normalize(cross(N, abs(N.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
    vec3 b2 = cross(N, b1);
    float ang = noiseIGN(gl_FragCoord.xy) * 6.2831853;
    int q = int(uCubeInfo.w + 0.5);
    // Loin de la camera, un filtre plus simple (on n'y voit pas le detail de la penombre)
    float camDist = length(viewPos - P);
    if (camDist > uShadowRange * 0.5)
        q = 0;
    else if (camDist > uShadowRange * 0.25)
        q = min(q, 1);
    if (q == 0)
    {
        float sum = 0.0;
        for (int k = 0; k < 4; ++k)
        {
            vec2 o = vogel(k, 4, ang) * texel;
            sum += cubeTest(s, lamp, Pb + b1 * o.x + b2 * o.y, bias);
        }
        return sum * 0.25;
    }
    float radius = texel * 1.6;
    int taps = 16; // multiple de 8 (voir plus bas)
    if (q >= 2)
    {
        // Obstacles between the point and the light: those the light's disk can see around the ray
        float halfLamp = uCubeInfo.z * 0.5;
        float search = clamp(halfLamp * (dist / 0.6 - 1.0), texel * 2.0, 1.2);
        vec3 t1 = normalize(cross(dir, abs(dir.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
        vec3 t2 = cross(dir, t1);
        float ref = (length(Pb - lamp.xyz) - bias) / lamp.w;
        int searchTaps = q >= 3 ? 8 : 6;
        float sumDepth = 0.0, found = 0.0;
        for (int k = 0; k < searchTaps; ++k)
        {
            vec2 o = vogel(k, searchTaps, ang + 1.3) * search;
            vec3 v = Pb + t1 * o.x + t2 * o.y - lamp.xyz;
            float d = texture(uCubeDepth, cubeCoord(s, v)).r;
            if (d < ref)
                sumDepth += d, found += 1.0;
        }
        if (found < 0.5)
            return 1.0;
        float db = max(sumDepth / found * lamp.w, 0.05);
        radius = clamp(halfLamp * (dist - db) / db, texel * 1.2, 1.5);
        taps = q >= 3 ? 24 : 16;
    }
    // Les premiers echantillons (repartis sur tout le disque) tous dans l'ombre ou tous dans la lumiere : la plupart des
    // points ne sont pas dans une penombre, inutile de faire le reste
    float sum = 0.0;
    const int FIRST = 8;
    for (int k = 0; k < FIRST; ++k)
    {
        vec2 o = vogel(k * taps / FIRST, taps, ang) * radius;
        sum += cubeTest(s, lamp, Pb + b1 * o.x + b2 * o.y, bias + length(o) * 0.05);
    }
    if (sum < 0.001 || sum > float(FIRST) - 0.001)
        return sum / float(FIRST);
    for (int k = 0; k < taps; ++k)
    {
        if (k % (taps / FIRST) == 0)
            continue;
        vec2 o = vogel(k, taps, ang) * radius;
        sum += cubeTest(s, lamp, Pb + b1 * o.x + b2 * o.y, bias + length(o) * 0.05);
    }
    return sum / float(taps);
}

// Shadow of point light i: its shadow map if it has one, else the grid
float lightShadow(int i, vec3 P, vec3 N)
{
    if (uCubeOn)
    {
        int s = uPointSlot[i];
        if (s >= 0 && length(viewPos - P) < uShadowRange)
            return cubeShadow(s, P, N);
    }
    return pointShadow(P, N, uPointPos[i].xyz);
}

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

// Distance attenuation of point light i (Lp: unit vector towards it): inverse square, windowed to zero at the
// radius. Spots (headlights) fall off more slowly and only light inside their cone.
float pointAttenuation(int i, vec3 Lp, float d2, float r)
{
    float window = clamp(1.0 - d2 / (r * r), 0.0, 1.0);
    float cone = uPointDir[i].w;
    if (cone <= -1.0)
        return window * window / (1.0 + d2);
    return window * window / (1.0 + 0.004 * d2) * smoothstep(cone, cone + 0.12, dot(-Lp, uPointDir[i].xyz));
}

vec3 acesTonemap(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// Detail texture projected on the three object axes ("whiteout" blend of the tangent normals).
// N: world normal, replaced by the detailed one. roughOffset: B channel, cavity: A channel.
void applyDetail(inout vec3 N, out float roughOffset, out float cavity)
{
    roughOffset = 0.0;
    cavity = 1.0;
    if (!uHasDetail)
        return;
    vec3 n = normalize(LocalNormal);
    vec3 p = LocalPos * uDetailScale;
    vec3 w = pow(abs(n), vec3(4.0));
    w /= (w.x + w.y + w.z);
    vec4 tx = texture(uDetail, p.zy);
    vec4 ty = texture(uDetail, p.xz);
    vec4 tz = texture(uDetail, p.xy);
    vec2 dX = (tx.rg * 2.0 - 1.0) * uDetailStrength;
    vec2 dY = (ty.rg * 2.0 - 1.0) * uDetailStrength;
    vec2 dZ = (tz.rg * 2.0 - 1.0) * uDetailStrength;
    vec3 tnX = vec3(dX + n.zy, sqrt(max(1.0 - dot(dX, dX), 0.0)) * n.x);
    vec3 tnY = vec3(dY + n.xz, sqrt(max(1.0 - dot(dY, dY), 0.0)) * n.y);
    vec3 tnZ = vec3(dZ + n.xy, sqrt(max(1.0 - dot(dZ, dZ), 0.0)) * n.z);
    vec3 nl = normalize(tnX.zyx * w.x + tnY.xzy * w.y + tnZ.xyz * w.z);
    vec3 world = normalize(normalMatrix * (nl * uObjectScale));
    // Keep the geometric orientation (faces seen from behind are flipped later)
    N = dot(world, N) < 0.0 ? -world : world;
    vec4 t = tx * w.x + ty * w.y + tz * w.z;
    roughOffset = (t.b - 0.5) * 2.0;
    cavity = mix(1.0, t.a, uDetailStrength);
}

// GGX / Smith / Schlick, multiplied by PI so that a white lambertian surface receives "colour * N.L" as in
// the specular / shininess model. aLight: roughness widened by the size of the light (sphere lights).
vec3 pbrLight(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 F0, float rough, float metal, float aLight)
{
    float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0)
        return vec3(0.0);
    vec3 H = normalize(L + V);
    float NoV = max(dot(N, V), 1e-4);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
    float a = rough * rough;
    float a2 = max(aLight * aLight, 1e-6);
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d) * (a * a) / a2;
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float vis = 1.0 / ((NoV * (1.0 - k) + k) * (NoL * (1.0 - k) + k));
    vec3 spec = D * vis * F * 0.25;
    vec3 diffuse = (1.0 - F) * (1.0 - metal) * albedo / PI;
    return (diffuse + spec) * NoL * PI;
}

// Split-sum environment term without a lookup table (Karis, mobile approximation)
vec3 envBRDFApprox(vec3 F0, float rough, float NoV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = rough * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
    return F0 * AB.x + AB.y;
}

vec3 applyFog(vec3 color, vec3 V, vec3 L)
{
    float dist = length(viewPos - FragPos);
    float fog = 1.0 - exp(-dist * uFogDensity);
    float sunAmount = pow(max(dot(-V, L), 0.0), 8.0);
    vec3 fogColor = mix(uFogColor, uFogColor + uSunColor * 0.15, sunAmount);
    return mix(color, fogColor, fog);
}

vec4 pbrShade(vec3 albedo, vec3 N, vec3 V, vec3 L, float roughOffset, float cavity)
{
    float rough = clamp(uRoughness + roughOffset * 0.5, 0.04, 1.0);
    float metal = uMetallic;
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    float a = rough * rough;
    float NoV = max(dot(N, V), 1e-4);

    float visibility = (1.0 - envShadow(FragPosLightSpace, N, L)) * cloudLight(FragPos);
    vec3 color = uSunColor * pbrLight(N, V, L, albedo, F0, rough, metal, a) * visibility;

    // Point lights: small spheres (radius 0.25) so that glossy surfaces show a visible reflection
    vec3 nearLight = vec3(0.0);
    for (int i = 0; i < uPointCount; ++i)
    {
        vec3 toLight = uPointPos[i].xyz - FragPos;
        float d2 = dot(toLight, toLight);
        float r = uPointPos[i].w;
        if (d2 >= r * r)
            continue;
        float d = sqrt(d2);
        vec3 Lp = toLight / max(d, 0.0001);
        float att = pointAttenuation(i, Lp, d2, r);
        if (att < 0.0005)
            continue;
        if (dot(N, Lp) <= 0.0)
        {
            nearLight += uPointColor[i] * att; // de dos : pas de lumiere directe, donc pas d'ombre a chercher
            continue;
        }
        att *= lightShadow(i, FragPos, N);
        float aL = clamp(a + 0.25 / (2.0 * max(d, 0.05)), 0.0, 1.0);
        color += uPointColor[i] * att * pbrLight(N, V, Lp, albedo, F0, rough, metal, aL);
        nearLight += uPointColor[i] * att;
    }

    // Ambient: sky / ground, and a blurred reflection of the surroundings (sky, nearby lights)
    vec3 ambient = mix(uGroundAmbient, uSkyAmbient, N.y * 0.5 + 0.5);
    vec3 R = reflect(-V, N);
    vec3 envColor = mix(envSky(R), ambient, clamp(rough * 1.3, 0.0, 1.0)) + nearLight * 0.06;
    vec3 Fenv = envBRDFApprox(F0, rough, NoV);
    color += (albedo * (1.0 - metal) * ambient * (1.0 - Fenv) + envColor * Fenv) * cavity;
    color *= mix(0.55, 1.0, cavity); // deep grooves and seams also receive less direct light
    color += albedo * gEmissive * 4.0 * cavity; // emissive surfaces: the cavity channel is their pattern (screens)

    color = applyFog(color, V, L) * uExposure;
    if (!uLinearOutput)
        color = pow(acesTonemap(color), vec3(1.0 / 2.2));
    return vec4(color, gObjectAlpha);
}

vec4 environmentShade()
{
    vec3 base = hasTexture ? texture(diffuseTexture, TexCoord).rgb : gObjectColor;
    vec3 albedo = pow(base, vec3(2.2));

    vec3 N = normalize(Normal);
    vec3 L = normalize(uSunDir);
    vec3 V = normalize(viewPos - FragPos);
    float roughOffset, cavity;
    applyDetail(N, roughOffset, cavity);
    if (dot(normalize(Normal), V) < 0.0)
        N = -N; // faces vues de dos (meshes ouverts)

    if (uPBR)
        return pbrShade(albedo, N, V, L, roughOffset, cavity);
    albedo *= cavity;

    float ndl = max(dot(N, L), 0.0);
    float visibility = (1.0 - envShadow(FragPosLightSpace, N, L)) * cloudLight(FragPos);

    vec3 direct = uSunColor * ndl * visibility;
    vec3 ambient = mix(uGroundAmbient, uSkyAmbient, N.y * 0.5 + 0.5);

    vec3 H = normalize(L + V);
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    float spec = pow(max(dot(N, H), 0.0), gShininess) * (gShininess + 8.0) / 25.13;
    vec3 specular = uSunColor * spec * gSpecular * mix(0.25, 1.0, fresnel) * ndl * visibility;
    vec3 reflection = envSky(reflect(-V, N)) * fresnel * gSpecular * 0.4;

    vec3 color = albedo * (direct + ambient) + specular + reflection + albedo * gEmissive * 4.0;

    // Point lights: inverse square falloff, smoothly windowed to zero at the radius
    for (int i = 0; i < uPointCount; ++i)
    {
        vec3 toLight = uPointPos[i].xyz - FragPos;
        float d2 = dot(toLight, toLight);
        float r = uPointPos[i].w;
        if (d2 >= r * r)
            continue;
        float d = sqrt(d2);
        vec3 Lp = toLight / max(d, 0.0001);
        float att = pointAttenuation(i, Lp, d2, r);
        float nlp = max(dot(N, Lp), 0.0);
        if (att * nlp < 0.0005)
            continue;
        att *= lightShadow(i, FragPos, N);
        if (uPointDir[i].w > -1.0)
            nlp = sqrt(nlp); // a low headlight still lights the road far ahead (grazing incidence)
        vec3 Hp = normalize(Lp + V);
        float sp = pow(max(dot(N, Hp), 0.0), gShininess) * (gShininess + 8.0) / 25.13;
        color += uPointColor[i] * att * nlp * (albedo + sp * gSpecular * mix(0.25, 1.0, fresnel));
    }

    color = applyFog(color, V, L) * uExposure;
    if (!uLinearOutput)
        color = pow(acesTonemap(color), vec3(1.0 / 2.2));
    return vec4(color, gObjectAlpha);
}

void main()
{
    gObjectColor = uInstanced ? InstColor.rgb : objectColor;
    gObjectAlpha = uInstanced ? InstColor.a : objectAlpha;
    gSpecular = uInstanced ? InstMaterial.x : uSpecular;
    gShininess = uInstanced ? InstMaterial.y : uShininess;
    gEmissive = uInstanced ? InstMaterial.z : uEmissive;
    FragColor = (uEnv == 1) ? environmentShade() : legacyShade();
}
