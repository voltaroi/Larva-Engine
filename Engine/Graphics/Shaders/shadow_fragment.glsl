#version 330 core
uniform float objectAlpha;

layout(location = 0) out vec2 Moment;

in vec4 FragPosLightSpace;

void main()
{
    // Discard nearly transparent fragments
    if (objectAlpha < 0.05) discard;

    // Compute depth in light clip space (0..1)
    float depth = FragPosLightSpace.z / FragPosLightSpace.w;
    depth = depth * 0.5 + 0.5;

    // Optionally bias depth a bit based on transparency
    depth += (1.0 - objectAlpha) * 0.0005;

    // Store first and second moment
    Moment = vec2(depth, depth * depth);
}
