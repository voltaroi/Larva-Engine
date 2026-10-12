#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
// Instanced draws (Model::drawInstanced): model matrix, colour and material per instance
layout(location = 4) in vec4 iModel0;
layout(location = 5) in vec4 iModel1;
layout(location = 6) in vec4 iModel2;
layout(location = 7) in vec4 iModel3;
layout(location = 8) in vec4 iColor;
layout(location = 9) in vec4 iMaterial; // specular, shininess, emissive

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
uniform bool uInstanced;
flat out vec4 InstColor;
flat out vec3 InstMaterial;

void main()
{
    mat4 m = model;
    mat3 nm = normalMatrix;
    vec3 scale = uObjectScale;
    InstColor = vec4(1.0);
    InstMaterial = vec3(0.0);
    if (uInstanced)
    {
        m = mat4(iModel0, iModel1, iModel2, iModel3);
        nm = transpose(inverse(mat3(m)));
        scale = vec3(length(m[0].xyz), length(m[1].xyz), length(m[2].xyz));
        InstColor = iColor;
        InstMaterial = iMaterial.xyz;
    }
    vec4 worldPos = m * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = nm * aNormal;
    TexCoord = aTexCoord;
    vec3 sc = max(abs(scale), vec3(1e-4));
    LocalPos = aPos * scale;
    LocalNormal = aNormal / sc;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * worldPos;
}
