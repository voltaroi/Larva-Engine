#version 330 core

uniform float objectAlpha;

void main()
{
    // Discard fragments with very low alpha
    if (objectAlpha < 0.1) {
        discard;
    }
    
    // Modulate depth based on alpha to create lighter shadows for transparent objects
    // Higher alpha (more opaque) = darker shadow (normal depth)
    // Lower alpha (more transparent) = lighter shadow (pushed back depth)
    float depthOffset = (1.0 - objectAlpha) * 0.01;
    gl_FragDepth = gl_FragCoord.z + depthOffset;
}
