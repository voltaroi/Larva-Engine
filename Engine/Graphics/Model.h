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
    Model();
    ~Model();

    bool loadFromFile(const std::string &path);

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
    static void SetFrameUniforms(const float view[16], const float projection[16]);
    
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
    
    bool useColorOverride = false;
    float overrideR = 1.0f;
    float overrideG = 1.0f;
    float overrideB = 1.0f;
    float overrideA = 1.0f;
};
