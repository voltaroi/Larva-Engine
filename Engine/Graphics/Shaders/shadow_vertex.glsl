#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 4) in vec4 iModel0;
layout(location = 5) in vec4 iModel1;
layout(location = 6) in vec4 iModel2;
layout(location = 7) in vec4 iModel3;
uniform bool uInstanced;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

out vec4 FragPosLightSpace;

void main()
{
    vec4 worldPos = (uInstanced ? mat4(iModel0, iModel1, iModel2, iModel3) : model) * vec4(aPos, 1.0);
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = FragPosLightSpace;
}
