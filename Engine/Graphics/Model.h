#pragma once
#include <string>
#include <vector>
#include <GL/glut.h>

struct SimpleVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
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

    void setPosition(float x, float y, float z);
    void setScale(float sx, float sy, float sz);
    void setColor(float r, float g, float b);
    void setColorRGBA(float r, float g, float b, float a);
    void clearColorOverride();
    void setRotation(float x, float y, float z);
    void addRotation(float x, float y, float z);
    
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

    // Material used when the environment is enabled
    void setMaterial(float specular, float shininess, float emissive = 0.0f);
    
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
    };

    std::vector<Mesh> meshes;
    float posX, posY, posZ;
    float scaleX, scaleY, scaleZ;
    float rotX, rotY, rotZ;
    
    float specular = 0.25f;
    float shininess = 32.0f;
    float emissive = 0.0f;

    bool useColorOverride = false;
    float overrideR = 1.0f;
    float overrideG = 1.0f;
    float overrideB = 1.0f;
    float overrideA = 1.0f;
};
