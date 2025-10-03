#ifndef __MODEL__
#define __MODEL__
#include <GL/glew.h>
#include <GL/glut.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <fstream>
#include <iostream>

class Model
{
private:
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    aiMesh* mesh;

public:

    void draw();
    bool fileExists(const std::string& filepath);
    Model LoadModel(const std::string& path);
};

#endif
