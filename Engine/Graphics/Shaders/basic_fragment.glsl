#version 330 core

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2DShadow shadowMap;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

#define PI 3.14159265359
#define POISSON_COUNT 16
#define LIGHT_SIZE_UV 3.0

const vec2 poissonDisk[POISSON_COUNT] = vec2[](
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

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

mat2 rot2(float a)
{
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map or beyond light frustum: lit
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    // Slope-scale depth bias to reduce acne
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float bias = max(0.0015 * (1.0 - dot(norm, lightDir)), 0.0007);

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    // --- PCSS: 1) Blocker search ---
    float searchRadius = LIGHT_SIZE_UV * 2.0 * texelSize.x;
    float avgBlocker = 0.0;
    int blockerCount = 0;
    float angle = rand(projCoords.xy * 1024.0) * 2.0 * PI;
    mat2 R = rot2(angle);
    for (int i = 0; i < POISSON_COUNT; ++i)
    {
        vec2 offset = R * normalize(poissonDisk[i]) * searchRadius;
        float d = texture(shadowMap, vec3(projCoords.xy + offset, currentDepth - bias));
        if (d < 0.5) // treat sample as blocker when comparison indicates shadow
        {
            // We don't have raw depth in compare mode; approximate using currentDepth
            avgBlocker += (currentDepth - bias);
            blockerCount++;
        }
    }
    if (blockerCount == 0)
        return 0.0;
    avgBlocker /= float(blockerCount);

    // --- PCSS: 2) Penumbra size estimation ---
    float penumbra = (currentDepth - avgBlocker) / max(avgBlocker, 1e-5);
    float filterRadius = clamp(penumbra * LIGHT_SIZE_UV, 1.0 * texelSize.x, 8.0 * texelSize.x);

    // --- PCSS: 3) Filtered PCF with variable radius ---
    float shadow = 0.0;
    for (int i = 0; i < POISSON_COUNT; ++i)
    {
        vec2 offset = R * normalize(poissonDisk[i]) * filterRadius;
        float contrib = texture(shadowMap, vec3(projCoords.xy + offset, currentDepth - bias));
        // contrib is 0..1 where 1 means lit; convert to shadow amount
        shadow += (1.0 - contrib);
    }
    shadow /= float(POISSON_COUNT);

    return shadow;
}

void main()
{
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Shadow calculation
    float shadow = ShadowCalculation(FragPosLightSpace);
    
    // Combine with shadow
    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * vec3(texture(texture1, TexCoord));
    
    FragColor = vec4(result, 1.0);
}
