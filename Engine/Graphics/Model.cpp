#include <GL/glew.h>
#include "Model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <fstream>
#include "ResourcePak.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

static GLuint g_modelProgram = 0;
static GLuint g_shadowProgram = 0;
static GLuint g_shadowMapFBO = 0;
static GLuint g_shadowMapTexture = 0;
static const int SHADOW_WIDTH = 4096;
static const int SHADOW_HEIGHT = 4096;
static glm::mat4 g_lightSpaceMatrix = glm::mat4(1.0f);
static glm::vec3 g_lightPos = glm::vec3(5.0f, 10.0f, 5.0f);
static bool g_shadowPass = false;
static GLint g_prevViewport[4] = {0,0,0,0};

static bool loadTextFile(const std::string &path, std::string &out)
{
    out.clear();

    // Try PAK first
    std::vector<unsigned char> data;
    if (ResourcePak::IsInitialized() && ResourcePak::LoadFile(path, data)) {
        out.assign(reinterpret_cast<const char*>(data.data()), data.size());
        return true;
    }

    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
        f.seekg(0, std::ios::end);
        std::streampos size = f.tellg();
        f.seekg(0, std::ios::beg);
        out.resize((size_t)size);
        f.read(out.data(), size);
        return true;
    }

    return false;
}

static GLuint compileShader(GLenum type, const std::string &src) {
    const char *csrc = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &csrc, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        std::cerr << "Shader compile error: " << buf << std::endl;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static void initShadowMap() {
    if (g_shadowMapFBO != 0) return;
    
    glGenFramebuffers(1, &g_shadowMapFBO);
    glGenTextures(1, &g_shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, g_shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_shadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static GLuint createShadowProgram() {
    if (g_shadowProgram) return g_shadowProgram;

    std::string vsSrc, fsSrc;
    if (!loadTextFile("Shaders/shadow_vertex.glsl", vsSrc)) {
        std::cerr << "Failed to load Shaders/shadow_vertex.glsl" << std::endl;
        return 0;
    }
    if (!loadTextFile("Shaders/shadow_fragment.glsl", fsSrc)) {
        std::cerr << "Failed to load Shaders/shadow_fragment.glsl" << std::endl;
        return 0;
    }

    GLuint vsId = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsId = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vsId || !fsId) {
        if (vsId) glDeleteShader(vsId);
        if (fsId) glDeleteShader(fsId);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsId);
    glAttachShader(prog, fsId);
    glLinkProgram(prog);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Shadow shader link error: " << buf << std::endl;
        glDeleteProgram(prog);
        glDeleteShader(vsId); glDeleteShader(fsId);
        return 0;
    }
    glDetachShader(prog, vsId); glDetachShader(prog, fsId);
    glDeleteShader(vsId); glDeleteShader(fsId);
    g_shadowProgram = prog;
    return g_shadowProgram;
}

static GLuint createModelProgram() {
    if (g_modelProgram) return g_modelProgram;

    std::string vsSrc, fsSrc;
    if (!loadTextFile("Shaders/model_vertex.glsl", vsSrc)) {
        std::cerr << "Failed to load Shaders/model_vertex.glsl" << std::endl;
        return 0;
    }
    if (!loadTextFile("Shaders/model_fragment.glsl", fsSrc)) {
        std::cerr << "Failed to load Shaders/model_fragment.glsl" << std::endl;
        return 0;
    }

    GLuint vsId = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsId = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vsId || !fsId) {
        if (vsId) glDeleteShader(vsId);
        if (fsId) glDeleteShader(fsId);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsId);
    glAttachShader(prog, fsId);
    glLinkProgram(prog);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Shader link error: " << buf << std::endl;
        glDeleteProgram(prog);
        glDeleteShader(vsId); glDeleteShader(fsId);
        return 0;
    }
    glDetachShader(prog, vsId); glDetachShader(prog, fsId);
    glDeleteShader(vsId); glDeleteShader(fsId);
    g_modelProgram = prog;
    return g_modelProgram;
}

Model::Model()
    : posX(0.0f), posY(0.0f), posZ(0.0f), scaleX(1.0f), scaleY(1.0f), scaleZ(1.0f),
      rotX(0.0f), rotY(0.0f), rotZ(0.0f)
{
}

Model::~Model()
{
    for (auto &m : meshes) {
        if (m.textureId != 0) {
            GLuint id = (GLuint)m.textureId;
            glDeleteTextures(1, &id);
            m.textureId = 0;
        }
        if (m.VAO != 0) glDeleteVertexArrays(1, &m.VAO);
        if (m.VBO != 0) glDeleteBuffers(1, &m.VBO);
        if (m.EBO != 0) glDeleteBuffers(1, &m.EBO);
    }
}

static void processAiMesh(aiMesh *mesh, const aiScene *scene, Model::Mesh &out)
{
    out.verts.clear();
    out.indices.clear();

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        SimpleVertex v{};
        v.x = mesh->mVertices[i].x;
        v.y = mesh->mVertices[i].y;
        v.z = mesh->mVertices[i].z;

        if (mesh->HasNormals()) {
            v.nx = mesh->mNormals[i].x;
            v.ny = mesh->mNormals[i].y;
            v.nz = mesh->mNormals[i].z;
        } else {
            v.nx = v.ny = 0.0f; v.nz = 1.0f;
        }

        if (mesh->mTextureCoords[0]) {
            v.u = mesh->mTextureCoords[0][i].x;
            v.v = mesh->mTextureCoords[0][i].y;
        } else {
            v.u = v.v = 0.0f;
        }

        out.verts.push_back(v);
    }

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        aiFace &face = mesh->mFaces[f];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            out.indices.push_back(face.mIndices[j]);
        }
    }
}

bool Model::loadFromFile(const std::string &path)
{
    std::string actualPath = path;
    std::vector<unsigned char> modelData;
    std::string tempFile;
    
    // Try to load from PAK first
    if (ResourcePak::IsInitialized() && ResourcePak::LoadFile(path, modelData)) {
        size_t dotPos = path.find_last_of('.');
        std::string extension = (dotPos != std::string::npos) ? path.substr(dotPos) : ".tmp";
        
        tempFile = "temp_model" + extension;
        std::ofstream tmpFileStream(tempFile, std::ios::binary);
        tmpFileStream.write(reinterpret_cast<char*>(modelData.data()), modelData.size());
        tmpFileStream.close();
        
        actualPath = tempFile;
    }
    
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(actualPath,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp error loading " << path << ": " << importer.GetErrorString() << std::endl;
        return false;
    }

    std::cerr << "Model: scene loaded. meshes=" << scene->mNumMeshes << " materials=" << scene->mNumMaterials << " embedded_textures=" << scene->mNumTextures << "\n";

    meshes.clear();

    struct MatInfo { unsigned int texId; float r,g,b; bool hasTex; };
    std::vector<MatInfo> matInfos;
    matInfos.resize(scene->mNumMaterials);

    std::string dir;
    size_t p = path.find_last_of('/');
    size_t q = path.find_last_of('\\');
    size_t pos = std::string::npos;
    if (p != std::string::npos && q != std::string::npos) pos = std::max(p,q);
    else if (p != std::string::npos) pos = p;
    else if (q != std::string::npos) pos = q;
    if (pos != std::string::npos) dir = path.substr(0, pos+1);
    else dir = "";

    for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi) {
        aiMaterial *mat = scene->mMaterials[mi];
        MatInfo info; info.texId = 0; info.hasTex = false; info.r = info.g = info.b = 1.0f;

        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            std::string texName = texPath.C_Str();
            std::string full = dir + texName;
            std::cerr << "Model: material " << mi << " diffuse texture='" << texName << "' resolved='" << full << "'\n";

            if (!texName.empty() && texName[0] == '*') {
                int idx = atoi(texName.c_str()+1);
                if (idx >= 0 && idx < (int)scene->mNumTextures) {
                    aiTexture *atex = scene->mTextures[idx];
                    if (atex) {
                        if (atex->mHeight == 0) {
                            std::cerr << "Model: using embedded compressed texture index=" << idx << " size=" << atex->mWidth << " bytes\n";
                            int w,h,ch;
                            unsigned char *data = stbi_load_from_memory((const unsigned char*)atex->pcData, atex->mWidth, &w, &h, &ch, STBI_rgb_alpha);
                            if (data) {
                                GLuint tex = 0;
                                glGenTextures(1, &tex);
                                glBindTexture(GL_TEXTURE_2D, tex);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                                glBindTexture(GL_TEXTURE_2D, 0);
                                stbi_image_free(data);
                                info.texId = (unsigned int)tex;
                                info.hasTex = true;
                            } else {
                                std::cerr << "Model: failed to decode embedded compressed texture\n";
                            }
                        } else {
                            int w = atex->mWidth;
                            int h = atex->mHeight;
                            const unsigned char *pixels = (const unsigned char*)atex->pcData;
                            if (pixels) {
                                GLuint tex = 0;
                                glGenTextures(1, &tex);
                                glBindTexture(GL_TEXTURE_2D, tex);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                                glBindTexture(GL_TEXTURE_2D, 0);
                                info.texId = (unsigned int)tex;
                                info.hasTex = true;
                            } else {
                                std::cerr << "Model: embedded texture has no pixel data\n";
                            }
                        }
                    }
                } else {
                    std::cerr << "Model: embedded texture index out of range: " << idx << "\n";
                }
            } else {
                int w,h,channels;
                unsigned char *data = nullptr;
                
                // Try to load from PAK first
                if (ResourcePak::IsInitialized()) {
                    std::vector<unsigned char> texData;
                    if (ResourcePak::LoadFile(full, texData)) {
                        data = stbi_load_from_memory(texData.data(), texData.size(), &w, &h, &channels, STBI_rgb_alpha);
                        std::cerr << "Model: loaded texture from PAK: '" << full << "'\n";
                    }
                }
                
                // Try disk loading if PAK didn't work
                if (!data && !full.empty()) data = stbi_load(full.c_str(), &w, &h, &channels, STBI_rgb_alpha);
                if (!data && !texName.empty()) {
                    std::cerr << "Model: trying fallback texture name='" << texName << "'\n";
                    data = stbi_load(texName.c_str(), &w, &h, &channels, STBI_rgb_alpha);
                }
                if (data) {
                    GLuint tex = 0;
                    glGenTextures(1, &tex);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    stbi_image_free(data);
                    info.texId = (unsigned int)tex;
                    info.hasTex = true;
                    std::cerr << "Model: loaded texture from disk: '" << (full.empty() ? texName : full) << "'\n";
                } else {
                    bool usedEmbedded = false;
                    if (scene->mNumTextures > 0) {
                        for (unsigned int et = 0; et < scene->mNumTextures; ++et) {
                            aiTexture *atex = scene->mTextures[et];
                            if (!atex) continue;
                            bool tryThis = false;
                            if (atex->mFilename.length > 0) {
                                std::string fname = atex->mFilename.C_Str();
                                size_t p1 = fname.find_last_of('/');
                                size_t p2 = fname.find_last_of('\\');
                                size_t pos = std::string::npos;
                                if (p1!=std::string::npos && p2!=std::string::npos) pos = std::max(p1,p2);
                                else if (p1!=std::string::npos) pos = p1;
                                else if (p2!=std::string::npos) pos = p2;
                                std::string base = (pos==std::string::npos) ? fname : fname.substr(pos+1);
                                std::string texBase = texName;
                                size_t t1 = texBase.find_last_of('/');
                                size_t t2 = texBase.find_last_of('\\');
                                size_t tpos = std::string::npos;
                                if (t1!=std::string::npos && t2!=std::string::npos) tpos = std::max(t1,t2);
                                else if (t1!=std::string::npos) tpos = t1;
                                else if (t2!=std::string::npos) tpos = t2;
                                if (tpos!=std::string::npos) texBase = texBase.substr(tpos+1);
                                if (!base.empty() && !texBase.empty() && base == texBase) tryThis = true;
                            } else {
                                tryThis = true;
                            }

                            if (!tryThis) continue;

                            if (atex->mHeight == 0) {
                                int w,h,ch;
                                unsigned char *edata = stbi_load_from_memory((const unsigned char*)atex->pcData, atex->mWidth, &w, &h, &ch, STBI_rgb_alpha);
                                if (edata) {
                                    GLuint tex = 0;
                                    glGenTextures(1, &tex);
                                    glBindTexture(GL_TEXTURE_2D, tex);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, edata);
                                    glBindTexture(GL_TEXTURE_2D, 0);
                                    stbi_image_free(edata);
                                    info.texId = (unsigned int)tex;
                                    info.hasTex = true;
                                    usedEmbedded = true;
                                    std::cerr << "Model: used embedded texture index=" << et << " as fallback\n";
                                    break;
                                }
                            } else {
                                int w = atex->mWidth;
                                int h = atex->mHeight;
                                const unsigned char *pixels = (const unsigned char*)atex->pcData;
                                if (pixels) {
                                    GLuint tex = 0;
                                    glGenTextures(1, &tex);
                                    glBindTexture(GL_TEXTURE_2D, tex);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                                    glBindTexture(GL_TEXTURE_2D, 0);
                                    info.texId = (unsigned int)tex;
                                    info.hasTex = true;
                                    usedEmbedded = true;
                                    std::cerr << "Model: used embedded raw texture index=" << et << " as fallback\n";
                                    break;
                                }
                            }
                        }
                    }
                    if (!usedEmbedded) {
                        std::cerr << "Model: could not load texture '" << full << "' (tried fallback as '" << texName << "')\n";
                    }
                }
            }
        }

        if (!info.hasTex) {
            aiColor4D color(1.0f,1.0f,1.0f,1.0f);
            if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color)) {
                info.r = color.r; info.g = color.g; info.b = color.b;
            }
        }

        matInfos[mi] = info;
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh *am = scene->mMeshes[i];
        std::cerr << "Model: mesh[" << i << "] vertices=" << am->mNumVertices << " faces=" << am->mNumFaces << " matIndex=" << am->mMaterialIndex;
        std::cerr << " hasNormals=" << (am->HasNormals() ? 1 : 0) << " hasTexCoords0=" << (am->mTextureCoords[0] ? 1 : 0) << "\n";
        Mesh m;
        processAiMesh(am, scene, m);
        unsigned int matIdx = am->mMaterialIndex;
        if (matIdx < matInfos.size()) {
            m.textureId = matInfos[matIdx].texId;
            m.hasTexture = matInfos[matIdx].hasTex;
            m.diffuseR = matInfos[matIdx].r;
            m.diffuseG = matInfos[matIdx].g;
            m.diffuseB = matInfos[matIdx].b;
        }
        meshes.push_back(std::move(m));
    }

    // Clean up temp file if we created one
    if (!tempFile.empty()) {
        std::remove(tempFile.c_str());
    }

    std::cout << "Loaded model '" << path << "' with " << meshes.size() << " mesh(es)" << std::endl;
    return true;
}

void Model::setPosition(float x, float y, float z)
{
    posX = x; posY = y; posZ = z;
}

void Model::setScale(float sx, float sy, float sz)
{
    scaleX = sx; scaleY = sy; scaleZ = sz;
}

void Model::setColor(float r, float g, float b)
{
    float cr = r / 255.0f;
    float cg = g / 255.0f;
    float cb = b / 255.0f;
    for (auto &mesh : meshes) {
        mesh.diffuseR = cr;
        mesh.diffuseG = cg;
        mesh.diffuseB = cb;
    }
}

void Model::setColorRGBA(float r, float g, float b, float a)
{
    useColorOverride = true;
    overrideR = r;
    overrideG = g;
    overrideB = b;
    overrideA = a;
}

void Model::clearColorOverride()
{
    useColorOverride = false;
}

void Model::setRotation(float x, float y, float z)
{
    rotX = x;
    rotY = y;
    rotZ = z;
}

void Model::addRotation(float x, float y, float z)
{
    rotX += x;
    rotY += y;
    rotZ += z;
}

void Model::draw()
{
    // Create model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(posX, posY, posZ));
    model = glm::rotate(model, glm::radians(rotX), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(scaleX, scaleY, scaleZ));

    // Shadow pass
    if (g_shadowPass) {
        GLuint shadowProg = createShadowProgram();
        if (shadowProg == 0) return;
        
        GLint modelLoc = glGetUniformLocation(shadowProg, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        // Pass alpha to shadow shader
        float alpha = useColorOverride ? overrideA : 1.0f;
        GLint objectAlphaLoc = glGetUniformLocation(shadowProg, "objectAlpha");
        glUniform1f(objectAlphaLoc, alpha);
        
        for (auto &m : meshes) {
            // Initialize buffers on first use
            if (!m.buffersInitialized) {
                glGenVertexArrays(1, &m.VAO);
                glGenBuffers(1, &m.VBO);
                glGenBuffers(1, &m.EBO);
                
                glBindVertexArray(m.VAO);
                
                // Upload vertex data
                glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
                glBufferData(GL_ARRAY_BUFFER, m.verts.size() * sizeof(SimpleVertex), m.verts.data(), GL_STATIC_DRAW);
                
                // Upload index data
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(unsigned int), m.indices.data(), GL_STATIC_DRAW);
                
                // Position attribute
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)0);
                
                // Normal attribute
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)(3 * sizeof(float)));
                
                // TexCoord attribute
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)(6 * sizeof(float)));
                
                glBindVertexArray(0);
                m.buffersInitialized = true;
            }
            
            glBindVertexArray(m.VAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        return;
    }

    // Normal rendering pass
    GLuint program = createModelProgram();
    if (program == 0) return;

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program);

    // Set uniforms
    GLint modelLoc = glGetUniformLocation(program, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // Get current OpenGL matrices
    GLfloat viewMatrix[16];
    GLfloat projectionMatrix[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
    glGetFloatv(GL_PROJECTION_MATRIX, projectionMatrix);

    glm::mat4 view = glm::make_mat4(viewMatrix);
    glm::mat4 projection = glm::make_mat4(projectionMatrix);

    GLint viewLoc = glGetUniformLocation(program, "view");
    GLint projLoc = glGetUniformLocation(program, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Light space matrix for shadows
    GLint lightSpaceLoc = glGetUniformLocation(program, "lightSpaceMatrix");
    glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, glm::value_ptr(g_lightSpaceMatrix));

    // Light uniforms
    GLint lightPosLoc = glGetUniformLocation(program, "lightPos");
    GLint viewPosLoc = glGetUniformLocation(program, "viewPos");
    GLint lightColorLoc = glGetUniformLocation(program, "lightColor");
    GLint objectColorLoc = glGetUniformLocation(program, "objectColor");

    glm::vec3 viewPos = glm::vec3(viewMatrix[12], viewMatrix[13], viewMatrix[14]);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

    glUniform3f(lightPosLoc, g_lightPos.x, g_lightPos.y, g_lightPos.z);
    glUniform3f(viewPosLoc, viewPos.x, viewPos.y, viewPos.z);
    glUniform3f(lightColorLoc, lightColor.x, lightColor.y, lightColor.z);

    // Bind shadow map to texture unit 1
    if (g_shadowMapTexture != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_shadowMapTexture);
        GLint shadowMapLoc = glGetUniformLocation(program, "shadowMap");
        glUniform1i(shadowMapLoc, 1);
    }

    for (auto &m : meshes) {
        // Initialize buffers on first use
        if (!m.buffersInitialized) {
            glGenVertexArrays(1, &m.VAO);
            glGenBuffers(1, &m.VBO);
            glGenBuffers(1, &m.EBO);

            glBindVertexArray(m.VAO);

            // Upload vertex data
            glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
            glBufferData(GL_ARRAY_BUFFER, m.verts.size() * sizeof(SimpleVertex), m.verts.data(), GL_STATIC_DRAW);

            // Upload index data
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(unsigned int), m.indices.data(), GL_STATIC_DRAW);

            // Position attribute
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)0);

            // Normal attribute
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)(3 * sizeof(float)));

            // TexCoord attribute
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)(6 * sizeof(float)));

            glBindVertexArray(0);
            m.buffersInitialized = true;
        }

        // Set object color
        glm::vec3 objectColor;
        float alpha = 1.0f;
        if (useColorOverride) {
            objectColor = glm::vec3(overrideR, overrideG, overrideB);
            alpha = overrideA;
        } else {
            objectColor = glm::vec3(m.diffuseR, m.diffuseG, m.diffuseB);
        }
        glUniform3f(objectColorLoc, objectColor.x, objectColor.y, objectColor.z);
        GLint objectAlphaLoc = glGetUniformLocation(program, "objectAlpha");
        glUniform1f(objectAlphaLoc, alpha);

        // Set texture
        GLint hasTexLoc = glGetUniformLocation(program, "hasTexture");
        if (m.hasTexture && m.textureId != 0) {
            glUniform1i(hasTexLoc, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m.textureId);
            GLint diffuseTexLoc = glGetUniformLocation(program, "diffuseTexture");
            glUniform1i(diffuseTexLoc, 0);
        } else {
            glUniform1i(hasTexLoc, 0);
        }

        glBindVertexArray(m.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    glUseProgram(0);
    glDisable(GL_BLEND);
}

// Static methods for shadow mapping
void Model::BeginShadowPass()
{
    // Initialize shadow map if not done yet
    initShadowMap();

    // Calculate light space matrix
    glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 50.0f);
    glm::mat4 lightView = glm::lookAt(g_lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    g_lightSpaceMatrix = lightProjection * lightView;

    // Render to shadow map (save and set viewport)
    glGetIntegerv(GL_VIEWPORT, g_prevViewport);
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Use shadow shader
    GLuint shadowProg = createShadowProgram();
    if (shadowProg != 0) {
        glUseProgram(shadowProg);
        GLint lightSpaceLoc = glGetUniformLocation(shadowProg, "lightSpaceMatrix");
        glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, glm::value_ptr(g_lightSpaceMatrix));
    }

    g_shadowPass = true;
}

void Model::EndShadowPass()
{
    g_shadowPass = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    // Restore previous viewport saved in BeginShadowPass
    glViewport(g_prevViewport[0], g_prevViewport[1], g_prevViewport[2], g_prevViewport[3]);
}

void Model::SetLightPosition(float x, float y, float z)
{
    g_lightPos = glm::vec3(x, y, z);
}
