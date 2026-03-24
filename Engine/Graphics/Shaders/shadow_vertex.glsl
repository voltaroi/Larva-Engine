#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

out vec4 FragPosLightSpace;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = FragPosLightSpace;
}
