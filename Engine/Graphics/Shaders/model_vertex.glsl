#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform mat3 normalMatrix;
uniform vec3 uObjectScale; // Model scale: detail textures keep their size on scaled objects

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;
out vec3 LocalPos;    // object space, scaled (before rotation)
out vec3 LocalNormal;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalMatrix * aNormal;
    TexCoord = aTexCoord;
    vec3 sc = max(abs(uObjectScale), vec3(1e-4));
    LocalPos = aPos * uObjectScale;
    LocalNormal = aNormal / sc;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * worldPos;
}
