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
public:
    struct Mesh {
        std::vector<SimpleVertex> verts;
        std::vector<unsigned int> indices;
        unsigned int textureId = 0;
        float diffuseR = 1.0f, diffuseG = 1.0f, diffuseB = 1.0f;
        bool hasTexture = false;
    };

    std::vector<Mesh> meshes;
    float posX, posY, posZ;
    float scaleX, scaleY, scaleZ;
};
