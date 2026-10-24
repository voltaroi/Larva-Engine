#pragma once
#include <string>
#include <vector>
#include <GL/glut.h>

struct SimpleVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    // Depth priority of the piece this vertex belongs to, + 1 (MeshBuilder fills it); 0: the model's own priority
    float priority = 0.0f;
};

class Model {
public:
    // Optional physically-inspired lighting shared by every model (disabled by default).
    // When enabled: directional sun, sky/ground ambient, sky reflections, atmospheric fog and cloud shadows.
    struct Environment {
        bool enabled = false;
        float sunDir[3] = {-0.45f, 0.62f, -0.35f};    // direction towards the sun
        float sunColor[3] = {3.0f, 2.85f, 2.6f};       // linear HDR (includes intensity)
        float skyAmbient[3] = {0.28f, 0.38f, 0.55f};
        float groundAmbient[3] = {0.16f, 0.14f, 0.1f};
        float zenithColor[3] = {0.12f, 0.3f, 0.72f};
        float fogColor[3] = {0.62f, 0.72f, 0.86f};     // also the horizon colour of the sky
        float fogDensity = 0.0025f;
        float cloudShadow = 0.0f;                      // 0..1 darkening under clouds
        float cloudCoverage = 0.45f;                   // 0..1
        float cloudScale = 0.004f;
        float cloudHeight = 250.0f;
        float cloudOffset[2] = {0.0f, 0.0f};           // animate to move clouds
        bool linearOutput = false;                     // true: HDR output, tone mapping done by a post-process
    };

    Model();
    ~Model();

    bool loadFromFile(const std::string &path);
    // Build the model from procedural geometry (single mesh, uploaded to the GPU immediately)
    bool createFromData(const std::vector<SimpleVertex> &verts, const std::vector<unsigned int> &indices);

    void draw();
    // draw() with an already computed model matrix (column-major)
    void drawWithMatrix(const float matrix[16]);

    // Many copies of the model in one draw call: each instance has its own matrix (column-major), colour and material.
    // Texture, detail map and PBR are not used. Works in the shadow pass as well.
    struct Instance
    {
        float model[16];
        float color[4];
        float material[4]; // specular, shininess, emissive, (depth priority: filled by drawInstanced)
    };
    void drawInstanced(const Instance *instances, int count);

    void setPosition(float x, float y, float z);
    void setScale(float sx, float sy, float sz);
    void setColor(float r, float g, float b);
    void setColorRGBA(float r, float g, float b, float a);
    void clearColorOverride();
    void setRotation(float x, float y, float z);
    void addRotation(float x, float y, float z);
    // Transformation parente (colonne majeure) appliquée avant position / rotation / échelle :
    // pour dessiner les pièces d'un objet composé dans son repère local (voiture inclinée sur une pente...)
    void setParentTransform(const float matrix[16]);
    void clearParentTransform();
    
    static void BeginShadowPass();
    static void EndShadowPass();
    static void SetLightPosition(float x, float y, float z);
    static void SetLightTarget(float x, float y, float z);
    static void SetFrameUniforms(const float view[16], const float projection[16]);

    static void SetEnvironment(const Environment &env);
    static const Environment &GetEnvironment();
    // Area covered by the shadow map (orthographic half size and depth range around the light)
    static void SetShadowArea(float halfSize, float nearPlane, float farPlane);
    // For custom shaders that want to receive the same shadows
    static unsigned int GetShadowMapTexture();
    static void GetLightSpaceMatrix(float out[16]);

    // Point lights (environment lighting only, no shadows). posRadius: x, y, z, radius of influence for each
    // light; colors: linear HDR r, g, b (includes intensity). count = 0 turns them off (default).
    // dirCone (optional): x, y, z axis and cosine of the half angle for each light. A cosine <= -1 (the default)
    // is an omnidirectional light; otherwise the light is a spot with a slower falloff (headlights).
    static const int MAX_POINT_LIGHTS = 32;
    static void SetPointLights(int count, const float *posRadius, const float *colors, const float *dirCone = nullptr);

    // Point light shadows (environment lighting only, optional; the model shader must support them).
    // Occlusion grid seen from above: width x height cells of cellSize metres, cell (0, 0) starting at (originX, originZ).
    // cells: 4 floats per cell (row by row), two vertical obstacle bands [bottom, top] in metres (top <= bottom: none).
    // The shader marches from each lit point towards each point light through the grid. cells = nullptr turns it off.
    static void SetPointShadowGrid(const float *cells, int width, int height, float originX, float originZ, float cellSize);
    // Turn the grid on or off without uploading it again (menus, editor...)
    static void EnablePointShadowGrid(bool enabled);
    // Bodies casting point light shadows: vertical capsules, 5 floats each (x, z, bottom y, top y, radius)
    static const int MAX_SHADOW_CASTERS = 24;
    static void SetShadowCasters(int count, const float *capsules);

    // Point light shadow maps (environment lighting): real depth cube maps, one per shadowed light ("slot"), drawn by
    // the caller from the light. Two sets of maps: set 0 is a cache the caller fills once with what never moves, set 1 is
    // what the shader samples (CopyPointShadowCache, then the moving casters drawn on top). A light without a slot falls
    // back on the grid above. Faces: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z.
    static const int MAX_SHADOW_LAMPS = 16;
    // Allocates the maps (lamps = 0 frees them). Returns false when the GPU cannot (the grid stays in use).
    static bool SetupPointShadowMaps(int lamps, int resolution);
    static int PointShadowLamps();
    // Draw into one face of one slot: the next draw() / drawInstanced() calls render depth seen from the light.
    // Geometry closer than clearRadius to the light is ignored (the fixture around the bulb, the machine it sits in).
    static void BeginPointShadowFace(int set, int slot, int face, const float pos[3], float farDist, float clearRadius);
    static void EndPointShadowFaces();
    // Copies the cache (set 0) of a slot into the maps the shader reads (set 1); faceMask: bit k for face k
    static void CopyPointShadowCache(int slot, int faceMask = 63);
    // Camera of a face (to skip casters outside it): forward and up axes, and the tangent of the half angle
    static void PointShadowFaceAxes(int face, float forward[3], float up[3], float &tanHalf);
    // Light i of SetPointLights samples slot slotOfLight[i] (-1: none); slots: position and far distance of their light
    static void SetPointShadowSlots(int count, const int *slotOfLight, const float *slotPosFar);
    // Filter: 0 hard (4 taps), 1 soft (fixed width), 2 contact hardening (PCSS), 3 PCSS with more samples.
    // lampSize: size of the lights in metres (the bigger, the softer the shadow far from its caster)
    static void SetPointShadowFilter(int quality, float lampSize);
    // Distance (m) from the camera within which the maps are filtered; farther, a cheaper filter, then the grid (0: no limit)
    static void SetPointShadowRange(float meters);
    // Turn the shadow maps on or off without freeing them
    static void EnablePointShadowMaps(bool enabled);
    // Brightness applied before tone mapping (1: unchanged)
    static void SetExposure(float exposure);
    // Procedural dirt on everything drawn with the environment (colour variation, grime, ash, rust streaks, grain bump):
    // 0 = off (default), 1 = light, 2 = strong
    static void SetGrime(float strength);
    // Depth prepass (opt-in, environment lighting): between BeginDepthPrepass and EndDepthPrepass, draw() and
    // drawInstanced() are recorded instead of drawn. EndDepthPrepass first writes the depth of the opaque ones with a
    // trivial shader, then draws everything in order with the full shader (depth test GL_LEQUAL): each visible pixel
    // is lit once instead of once per overlapping surface. Opaque: no blending, depth test and depth writes on (state
    // read at each draw) and alpha 1. The model vertex shader must declare "invariant gl_Position".
    // Call SetFrameUniforms before; other OpenGL drawing in between is not deferred (it would end up underneath).
    static void BeginDepthPrepass();
    static void EndDepthPrepass();
    // false: Begin/EndDepthPrepass do nothing (draws are immediate, as without them)
    static void SetDepthPrepassEnabled(bool enabled);
    // Screen-space ambient occlusion (environment lighting, perspective camera): computed in EndDepthPrepass from the
    // depth of the prepass, so only what is drawn between Begin/EndDepthPrepass receives it. Corners, creases and the
    // ground under objects lose part of their ambient and bounce light. strength: 0 = off (default) .. 1;
    // radius: reach in metres; direct: share also applied to direct light (0 = physically right, more = more contrast)
    static void SetAmbientOcclusion(float strength, float radius = 0.6f, float direct = 0.0f);
    // Bounce light of omnidirectional point lights (environment lighting): each one also lights its surroundings softly,
    // without direction, as the light sent back by the walls and the floor around it. It does not go through what
    // blocks the lamp (shadow maps or grid). 0 = off (default), 1 = normal
    static void SetBounceLight(float strength);

    // Depth priority: of two surfaces at the same place (closer than the depth buffer can tell apart, which makes stripes
    // that move with the camera), the one with the higher priority always stays in front. Every draw gets one by itself:
    // the smaller object wins (a poster in front of its wall, a plank in front of its post; size classes of a factor 4,
    // from 3 cm to 30 m), then a fixed order taken from the object (model and colour, instance colour, piece of a
    // MeshBuilder mesh). The surface is moved toward the camera by a few steps of the depth buffer only (the biggest
    // objects not at all). Returns the priority of an object of this bounding radius (m); tieSeed: any fixed number.
    static float DepthPriority(float radius, unsigned int tieSeed);

    // Material used when the environment is enabled
    void setMaterial(float specular, float shininess, float emissive = 0.0f);
    // Physically based material (environment lighting only, opt-in): GGX specular, metallic workflow.
    // roughness 0 (mirror) .. 1 (matte); metallic 0 (paint, plastic...) .. 1 (bare metal). The emissive
    // value of setMaterial still applies. clearPBR() returns to the specular / shininess model.
    void setPBR(float roughness, float metallic);
    void clearPBR();
    // Detail texture (environment lighting only), projected in object space on the three axes (no UVs
    // needed, follows the object when it moves; scale is taken into account so the pattern is not stretched).
    // Texel: R, G = tangent-space normal (0.5 = flat), B = roughness offset (0.5 = none, PBR only),
    // A = cavity / ambient occlusion (1 = none). scale: repetitions per unit; strength: 0..1 (normal and
    // cavity intensity). texture = 0 turns it off.
    void setDetail(unsigned int texture, float scale = 1.0f, float strength = 1.0f);
    
public:
    struct Mesh {
        std::vector<SimpleVertex> verts;
        std::vector<unsigned int> indices;
        unsigned int textureId = 0;
        float diffuseR = 1.0f, diffuseG = 1.0f, diffuseB = 1.0f;
        bool hasTexture = false;
        
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int EBO = 0;
        bool buffersInitialized = false;
        float boundsMin[3] = {0.0f, 0.0f, 0.0f}, boundsMax[3] = {0.0f, 0.0f, 0.0f}; // local box, set with the buffers
    };

    std::vector<Mesh> meshes;
    float posX, posY, posZ;
    float scaleX, scaleY, scaleZ;
    float rotX, rotY, rotZ;
    bool hasParent = false;
    float parentMatrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    
    float specular = 0.25f;
    float shininess = 32.0f;
    float emissive = 0.0f;
    bool pbr = false;
    float roughness = 0.5f;
    float metallic = 0.0f;
    unsigned int detailTexture = 0;
    float detailScale = 1.0f;
    float detailStrength = 1.0f;

    bool useColorOverride = false;
    float overrideR = 1.0f;
    float overrideG = 1.0f;
    float overrideB = 1.0f;
    float overrideA = 1.0f;
};
