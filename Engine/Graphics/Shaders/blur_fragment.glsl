#version 330 core

in vec2 TexCoord;
out vec2 FragColor; // RG moments

uniform sampler2D inputTex;
uniform vec2 dir; // blur direction in UV space (e.g., vec2(1,0) or vec2(0,1))

const float w[9] = float[](0.05, 0.09, 0.12, 0.15, 0.18, 0.15, 0.12, 0.09, 0.05);

void main() {
    vec2 texelSize = 1.0 / textureSize(inputTex, 0);
    vec2 sum = vec2(0.0);
    for (int i = -4; i <= 4; ++i) {
        vec2 off = TexCoord + dir * float(i) * texelSize;
        sum += texture(inputTex, off).rg * w[i + 4];
    }
    FragColor = sum;
}
