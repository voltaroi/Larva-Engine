#version 330 core
// Same position as the depth prepass of Model (Model::BeginDepthPrepass)
invariant gl_Position;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
// Depth priority of the MeshBuilder piece + 1 (0: the draw's, uDepthPriority); instances: iMaterial.w
layout(location = 3) in float aPriority;
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
// Depth priority (Model::DepthPriority): of two surfaces at the same place, the higher one stays in front. Same
// computation as the depth prepass of Model (PREPASS_VS)
uniform float uDepthPriority;
uniform vec2 uVertexSnap; // > 0: vertices snap to this grid of the screen (Model::SetRetroGeometry). Same in PREPASS_VS
// Foliage in the wind (Model::setFoliage, Model::SetWind). Same in PREPASS_VS
uniform vec4 uWind;        // time, direction x, z, strength
uniform float uFoliageSway;
const float DEPTH_STEP = 2.0 / 16777215.0; // one step of a 24 bit depth buffer

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
noperspective out vec2 TexCoordFlat; // no perspective correction (Model::SetRetroGeometry)
out vec4 FragPosLightSpace;
out vec3 LocalPos;    // object space, scaled (before rotation)
out vec3 LocalNormal;
uniform bool uInstanced;
flat out vec4 InstColor;
flat out vec3 InstMaterial;

vec3 foliageBend(vec3 p, float along)
{
    float across = dot(p.xz, uWind.yz);
    float gust = 0.55 + 0.45 * sin(across * 0.09 - uWind.x * 0.6);                   // a wide front crossing the place
    float sway = sin(across * 0.5 - uWind.x * 1.7 + p.y * 0.3);                      // the branch
    float flutter = sin(uWind.x * 5.3 + p.x * 2.1 + p.z * 1.7 + p.y * 1.3);          // the leaves
    float reach = clamp(along, 0.0, 1.0);
    vec3 down = vec3(uWind.y, 0.0, uWind.z), aside = vec3(-uWind.z, 0.0, uWind.y);
    return (down * gust * (0.6 + 0.4 * sway) + (aside * 0.15 + vec3(0.0, 0.22, 0.0)) * flutter * (gust + 0.3)) * reach * reach * uFoliageSway * uWind.w;
}

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
    if (uFoliageSway > 0.0)
        worldPos.xyz += foliageBend(worldPos.xyz, aTexCoord.y);
    FragPos = worldPos.xyz;
    Normal = nm * aNormal;
    TexCoord = aTexCoord;
    TexCoordFlat = aTexCoord;
    vec3 sc = max(abs(scale), vec3(1e-4));
    LocalPos = aPos * scale;
    LocalNormal = aNormal / sc;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * worldPos;
    float priority = uInstanced ? iMaterial.w : (aPriority > 0.0 ? aPriority - 1.0 : uDepthPriority);
    gl_Position.z -= priority * DEPTH_STEP * gl_Position.w;
    if (uVertexSnap.x > 0.0 && gl_Position.w > 0.0)
        gl_Position.xy = floor(gl_Position.xy / gl_Position.w * uVertexSnap + 0.5) / uVertexSnap * gl_Position.w;
}
