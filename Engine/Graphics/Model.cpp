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
#include <map>
#include <unordered_map>

static GLuint g_modelProgram = 0;
static GLuint g_shadowProgram = 0;
static GLuint g_shadowMapFBO = 0;
static GLuint g_shadowMapTexture = 0;
static const int SHADOW_WIDTH = 1024;
static const int SHADOW_HEIGHT = 1024;
static glm::mat4 g_lightSpaceMatrix = glm::mat4(1.0f);
static glm::vec3 g_lightPos = glm::vec3(5.0f, 10.0f, 5.0f);
static bool g_shadowPass = false;
static GLint g_prevViewport[4] = {0, 0, 0, 0};

// Cached program uniform locations to avoid repeated glGetUniformLocation calls
struct ModelProgInfo
{
    GLuint prog = 0;
    GLint locModel = -1;
    GLint locView = -1;
    GLint locProj = -1;
    GLint locLightSpace = -1;
    GLint locLightPos = -1;
    GLint locViewPos = -1;
    GLint locLightColor = -1;
    GLint locObjectColor = -1;
    GLint locObjectAlpha = -1;
    GLint locHasTexture = -1;
    GLint locDiffuseTexture = -1;
    GLint locShadowMap = -1;
    GLint locNormalMatrix = -1;
};
static ModelProgInfo g_modelInfo;

// Cached shadow program locations
static GLint g_shadowModelLoc = -1;
static GLint g_shadowObjectAlphaLoc = -1;
static GLint g_shadowLightSpaceLoc = -1;

// Cached per-frame matrices (set once per frame)
static glm::mat4 g_cachedView = glm::mat4(1.0f);
static glm::mat4 g_cachedProjection = glm::mat4(1.0f);
static glm::vec3 g_cachedViewPos = glm::vec3(0.0f);
static GLuint g_blurProgram = 0;
static GLuint g_quadVAO = 0;
static GLuint g_quadVBO = 0;
static GLuint g_shadowTempTexture = 0;
static GLuint g_blurFBO = 0;
// Cached blur uniform locations
static GLint g_blurInputLoc = -1;
static GLint g_blurDirLoc = -1;

static void initFullScreenQuad()
{
    if (g_quadVAO != 0)
        return;
    float quadVertices[] = {
        // positions   // texcoords
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        -1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
    };
    glGenVertexArrays(1, &g_quadVAO);
    glGenBuffers(1, &g_quadVBO);
    glBindVertexArray(g_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
}

// Create GPU buffers (VAO/VBO/EBO) for a mesh
static void setupMeshBuffers(Model::Mesh &m)
{
    if (m.buffersInitialized)
        return;

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);
    glGenBuffers(1, &m.EBO);

    glBindVertexArray(m.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, m.verts.size() * sizeof(SimpleVertex), m.verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(unsigned int), m.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void *)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void *)(6 * sizeof(float)));

    // Unbind VAO after setup
    glBindVertexArray(0);
    m.buffersInitialized = true;
}

// forward declarations so helper functions can be used earlier in this file
static bool loadTextFile(const std::string &path, std::string &out);
static GLuint compileShader(GLenum type, const std::string &src);

static GLuint createBlurProgram()
{
    if (g_blurProgram)
        return g_blurProgram;
    std::string vsSrc, fsSrc;
    if (!loadTextFile("Shaders/blur_vertex.glsl", vsSrc))
    {
        std::cerr << "Failed to load Shaders/blur_vertex.glsl" << std::endl;
        return 0;
    }
    if (!loadTextFile("Shaders/blur_fragment.glsl", fsSrc))
    {
        std::cerr << "Failed to load Shaders/blur_fragment.glsl" << std::endl;
        return 0;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs)
    {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Blur shader link error:" << buf << std::endl;
        glDeleteProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    g_blurProgram = prog;
    // Cache blur uniform locations
    g_blurInputLoc = glGetUniformLocation(g_blurProgram, "inputTex");
    g_blurDirLoc = glGetUniformLocation(g_blurProgram, "dir");
    return g_blurProgram;
}

static bool loadTextFile(const std::string &path, std::string &out)
{
    out.clear();

    // Try PAK first
    std::vector<unsigned char> data;
    if (ResourcePak::IsInitialized() && ResourcePak::LoadFile(path, data))
    {
        out.assign(reinterpret_cast<const char *>(data.data()), data.size());
        return true;
    }

    std::ifstream f(path, std::ios::binary);
    if (f.is_open())
    {
        f.seekg(0, std::ios::end);
        std::streampos size = f.tellg();
        f.seekg(0, std::ios::beg);
        out.resize((size_t)size);
        f.read(out.data(), size);
        return true;
    }

    return false;
}

static GLuint compileShader(GLenum type, const std::string &src)
{
    const char *csrc = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &csrc, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        std::cerr << "Shader compile error: " << buf << std::endl;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static void initShadowMap()
{
    if (g_shadowMapFBO != 0)
        return;

    glGenFramebuffers(1, &g_shadowMapFBO);
    glGenTextures(1, &g_shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, g_shadowMapTexture);
    // Use RG16F (half float) moments texture for Variance Shadow Maps (VSM) to save memory/bandwidth
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RG, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Create depth renderbuffer for depth testing while rendering shadow moments
    GLuint rboDepth = 0;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT);

    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_shadowMapTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Shadow FBO not complete" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// perform separable blur
static void blurShadowMap()
{
    if (g_shadowMapTexture == 0)
        return;
    // ensure blur program and quad exist
    if (!createBlurProgram())
        return;
    initFullScreenQuad();

    // create temp texture and FBO if needed
    if (g_shadowTempTexture == 0)
    {
        glGenTextures(1, &g_shadowTempTexture);
        glBindTexture(GL_TEXTURE_2D, g_shadowTempTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RG, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    if (g_blurFBO == 0)
    {
        glGenFramebuffers(1, &g_blurFBO);
    }

    glUseProgram(g_blurProgram);
    // Use cached uniform locations to avoid repeated glGetUniformLocation calls
    GLint inputLoc = g_blurInputLoc;
    GLint dirLoc = g_blurDirLoc;

    // Horizontal pass
    glBindFramebuffer(GL_FRAMEBUFFER, g_blurFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_shadowTempTexture, 0);
    GLenum db = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &db);
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_shadowMapTexture);
    glUniform1i(inputLoc, 0);
    glUniform2f(dirLoc, 1.0f, 0.0f);

    glBindVertexArray(g_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Vertical pass
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_shadowMapTexture, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_shadowTempTexture);
    glUniform1i(inputLoc, 0);
    glUniform2f(dirLoc, 0.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // restore
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
}

static GLuint createShadowProgram()
{
    if (g_shadowProgram)
        return g_shadowProgram;

    std::string vsSrc, fsSrc;
    if (!loadTextFile("Shaders/shadow_vertex.glsl", vsSrc))
    {
        std::cerr << "Failed to load Shaders/shadow_vertex.glsl" << std::endl;
        return 0;
    }
    if (!loadTextFile("Shaders/shadow_fragment.glsl", fsSrc))
    {
        std::cerr << "Failed to load Shaders/shadow_fragment.glsl" << std::endl;
        return 0;
    }

    GLuint vsId = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsId = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vsId || !fsId)
    {
        if (vsId)
            glDeleteShader(vsId);
        if (fsId)
            glDeleteShader(fsId);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsId);
    glAttachShader(prog, fsId);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Shadow shader link error: " << buf << std::endl;
        glDeleteProgram(prog);
        glDeleteShader(vsId);
        glDeleteShader(fsId);
        return 0;
    }
    glDetachShader(prog, vsId);
    glDetachShader(prog, fsId);
    glDeleteShader(vsId);
    glDeleteShader(fsId);
    g_shadowProgram = prog;
    // Cache common uniform locations for shadow program
    g_shadowModelLoc = glGetUniformLocation(g_shadowProgram, "model");
    g_shadowObjectAlphaLoc = glGetUniformLocation(g_shadowProgram, "objectAlpha");
    g_shadowLightSpaceLoc = glGetUniformLocation(g_shadowProgram, "lightSpaceMatrix");
    return g_shadowProgram;
}

static GLuint createModelProgram()
{
    if (g_modelProgram)
        return g_modelProgram;

    std::string vsSrc, fsSrc;
    if (!loadTextFile("Shaders/model_vertex.glsl", vsSrc))
    {
        std::cerr << "Failed to load Shaders/model_vertex.glsl" << std::endl;
        return 0;
    }
    if (!loadTextFile("Shaders/model_fragment.glsl", fsSrc))
    {
        std::cerr << "Failed to load Shaders/model_fragment.glsl" << std::endl;
        return 0;
    }

    GLuint vsId = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsId = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vsId || !fsId)
    {
        if (vsId)
            glDeleteShader(vsId);
        if (fsId)
            glDeleteShader(fsId);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsId);
    glAttachShader(prog, fsId);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Shader link error: " << buf << std::endl;
        glDeleteProgram(prog);
        glDeleteShader(vsId);
        glDeleteShader(fsId);
        return 0;
    }
    glDetachShader(prog, vsId);
    glDetachShader(prog, fsId);
    glDeleteShader(vsId);
    glDeleteShader(fsId);
    g_modelProgram = prog;
    // Cache uniform locations
    g_modelInfo.prog = g_modelProgram;
    g_modelInfo.locModel = glGetUniformLocation(g_modelProgram, "model");
    g_modelInfo.locView = glGetUniformLocation(g_modelProgram, "view");
    g_modelInfo.locProj = glGetUniformLocation(g_modelProgram, "projection");
    g_modelInfo.locLightSpace = glGetUniformLocation(g_modelProgram, "lightSpaceMatrix");
    g_modelInfo.locLightPos = glGetUniformLocation(g_modelProgram, "lightPos");
    g_modelInfo.locViewPos = glGetUniformLocation(g_modelProgram, "viewPos");
    g_modelInfo.locLightColor = glGetUniformLocation(g_modelProgram, "lightColor");
    g_modelInfo.locObjectColor = glGetUniformLocation(g_modelProgram, "objectColor");
    g_modelInfo.locObjectAlpha = glGetUniformLocation(g_modelProgram, "objectAlpha");
    g_modelInfo.locHasTexture = glGetUniformLocation(g_modelProgram, "hasTexture");
    g_modelInfo.locDiffuseTexture = glGetUniformLocation(g_modelProgram, "diffuseTexture");
    g_modelInfo.locShadowMap = glGetUniformLocation(g_modelProgram, "shadowMap");
    g_modelInfo.locNormalMatrix = glGetUniformLocation(g_modelProgram, "normalMatrix");
    return g_modelProgram;
}

Model::Model()
    : posX(0.0f), posY(0.0f), posZ(0.0f), scaleX(1.0f), scaleY(1.0f), scaleZ(1.0f),
      rotX(0.0f), rotY(0.0f), rotZ(0.0f)
{
}

Model::~Model()
{
    for (auto &m : meshes)
    {
        if (m.textureId != 0)
        {
            GLuint id = (GLuint)m.textureId;
            glDeleteTextures(1, &id);
            m.textureId = 0;
        }
        if (m.VAO != 0)
            glDeleteVertexArrays(1, &m.VAO);
        if (m.VBO != 0)
            glDeleteBuffers(1, &m.VBO);
        if (m.EBO != 0)
            glDeleteBuffers(1, &m.EBO);
    }
}

static void processAiMesh(aiMesh *mesh, const aiScene *scene, Model::Mesh &out)
{
    out.verts.clear();
    out.indices.clear();

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        SimpleVertex v{};
        v.x = mesh->mVertices[i].x;
        v.y = mesh->mVertices[i].y;
        v.z = mesh->mVertices[i].z;

        if (mesh->HasNormals())
        {
            v.nx = mesh->mNormals[i].x;
            v.ny = mesh->mNormals[i].y;
            v.nz = mesh->mNormals[i].z;
        }
        else
        {
            v.nx = v.ny = 0.0f;
            v.nz = 1.0f;
        }

        if (mesh->mTextureCoords[0])
        {
            v.u = mesh->mTextureCoords[0][i].x;
            v.v = mesh->mTextureCoords[0][i].y;
        }
        else
        {
            v.u = v.v = 0.0f;
        }

        out.verts.push_back(v);
    }

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        aiFace &face = mesh->mFaces[f];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            out.indices.push_back(face.mIndices[j]);
        }
    }
}

bool Model::loadFromFile(const std::string &path)
{
    std::string actualPath = path;
    std::vector<unsigned char> modelData;
    // Try to load from PAK first (into memory)
    if (ResourcePak::IsInitialized() && ResourcePak::LoadFile(path, modelData))
    {
        // keep modelData and call ReadFileFromMemory below
    }

    Assimp::Importer importer;
    const aiScene *scene = nullptr;
    unsigned int assimpFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices;
    if (!modelData.empty())
    {
        size_t dotPos = path.find_last_of('.');
        std::string hint = (dotPos != std::string::npos && dotPos + 1 < path.size()) ? path.substr(dotPos + 1) : "";
        scene = importer.ReadFileFromMemory(modelData.data(), modelData.size(), assimpFlags, hint.c_str());
    }
    else
    {
        scene = importer.ReadFile(actualPath, assimpFlags);
    }

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "Assimp error loading " << path << ": " << importer.GetErrorString() << std::endl;
        return false;
    }

    std::cerr << "Model: scene loaded. meshes=" << scene->mNumMeshes << " materials=" << scene->mNumMaterials << " embedded_textures=" << scene->mNumTextures << "\n";

    meshes.clear();

    struct MatInfo
    {
        unsigned int texId;
        float r, g, b;
        bool hasTex;
    };
    std::vector<MatInfo> matInfos;
    matInfos.resize(scene->mNumMaterials);

    std::string dir;
    size_t p = path.find_last_of('/');
    size_t q = path.find_last_of('\\');
    size_t pos = std::string::npos;
    if (p != std::string::npos && q != std::string::npos)
        pos = std::max(p, q);
    else if (p != std::string::npos)
        pos = p;
    else if (q != std::string::npos)
        pos = q;
    if (pos != std::string::npos)
        dir = path.substr(0, pos + 1);
    else
        dir = "";

    for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi)
    {
        aiMaterial *mat = scene->mMaterials[mi];
        MatInfo info;
        info.texId = 0;
        info.hasTex = false;
        info.r = info.g = info.b = 1.0f;

        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        {
            std::string texName = texPath.C_Str();
            std::string full = dir + texName;
            std::cerr << "Model: material " << mi << " diffuse texture='" << texName << "' resolved='" << full << "'\n";

            if (!texName.empty() && texName[0] == '*')
            {
                int idx = atoi(texName.c_str() + 1);
                if (idx >= 0 && idx < (int)scene->mNumTextures)
                {
                    aiTexture *atex = scene->mTextures[idx];
                    if (atex)
                    {
                        if (atex->mHeight == 0)
                        {
                            std::cerr << "Model: using embedded compressed texture index=" << idx << " size=" << atex->mWidth << " bytes\n";
                            int w, h, ch;
                            unsigned char *data = stbi_load_from_memory((const unsigned char *)atex->pcData, atex->mWidth, &w, &h, &ch, STBI_rgb_alpha);
                            if (data)
                            {
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
                            }
                            else
                            {
                                std::cerr << "Model: failed to decode embedded compressed texture\n";
                            }
                        }
                        else
                        {
                            int w = atex->mWidth;
                            int h = atex->mHeight;
                            const unsigned char *pixels = (const unsigned char *)atex->pcData;
                            if (pixels)
                            {
                                GLuint tex = 0;
                                glGenTextures(1, &tex);
                                glBindTexture(GL_TEXTURE_2D, tex);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                                glBindTexture(GL_TEXTURE_2D, 0);
                                info.texId = (unsigned int)tex;
                                info.hasTex = true;
                            }
                            else
                            {
                                std::cerr << "Model: embedded texture has no pixel data\n";
                            }
                        }
                    }
                }
                else
                {
                    std::cerr << "Model: embedded texture index out of range: " << idx << "\n";
                }
            }
            else
            {
                int w, h, channels;
                unsigned char *data = nullptr;

                // Try to load from PAK first
                if (ResourcePak::IsInitialized())
                {
                    std::vector<unsigned char> texData;
                    if (ResourcePak::LoadFile(full, texData))
                    {
                        data = stbi_load_from_memory(texData.data(), texData.size(), &w, &h, &channels, STBI_rgb_alpha);
                        std::cerr << "Model: loaded texture from PAK: '" << full << "'\n";
                    }
                }

                // Try disk loading if PAK didn't work
                if (!data && !full.empty())
                    data = stbi_load(full.c_str(), &w, &h, &channels, STBI_rgb_alpha);
                if (!data && !texName.empty())
                {
                    std::cerr << "Model: trying fallback texture name='" << texName << "'\n";
                    data = stbi_load(texName.c_str(), &w, &h, &channels, STBI_rgb_alpha);
                }
                if (data)
                {
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
                }
                else
                {
                    bool usedEmbedded = false;
                    if (scene->mNumTextures > 0)
                    {
                        for (unsigned int et = 0; et < scene->mNumTextures; ++et)
                        {
                            aiTexture *atex = scene->mTextures[et];
                            if (!atex)
                                continue;
                            bool tryThis = false;
                            if (atex->mFilename.length > 0)
                            {
                                std::string fname = atex->mFilename.C_Str();
                                size_t p1 = fname.find_last_of('/');
                                size_t p2 = fname.find_last_of('\\');
                                size_t pos = std::string::npos;
                                if (p1 != std::string::npos && p2 != std::string::npos)
                                    pos = std::max(p1, p2);
                                else if (p1 != std::string::npos)
                                    pos = p1;
                                else if (p2 != std::string::npos)
                                    pos = p2;
                                std::string base = (pos == std::string::npos) ? fname : fname.substr(pos + 1);
                                std::string texBase = texName;
                                size_t t1 = texBase.find_last_of('/');
                                size_t t2 = texBase.find_last_of('\\');
                                size_t tpos = std::string::npos;
                                if (t1 != std::string::npos && t2 != std::string::npos)
                                    tpos = std::max(t1, t2);
                                else if (t1 != std::string::npos)
                                    tpos = t1;
                                else if (t2 != std::string::npos)
                                    tpos = t2;
                                if (tpos != std::string::npos)
                                    texBase = texBase.substr(tpos + 1);
                                if (!base.empty() && !texBase.empty() && base == texBase)
                                    tryThis = true;
                            }
                            else
                            {
                                tryThis = true;
                            }

                            if (!tryThis)
                                continue;

                            if (atex->mHeight == 0)
                            {
                                int w, h, ch;
                                unsigned char *edata = stbi_load_from_memory((const unsigned char *)atex->pcData, atex->mWidth, &w, &h, &ch, STBI_rgb_alpha);
                                if (edata)
                                {
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
                            }
                            else
                            {
                                int w = atex->mWidth;
                                int h = atex->mHeight;
                                const unsigned char *pixels = (const unsigned char *)atex->pcData;
                                if (pixels)
                                {
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
                    if (!usedEmbedded)
                    {
                        std::cerr << "Model: could not load texture '" << full << "' (tried fallback as '" << texName << "')\n";
                    }
                }
            }
        }

        if (!info.hasTex)
        {
            aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
            if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color))
            {
                info.r = color.r;
                info.g = color.g;
                info.b = color.b;
            }
        }

        matInfos[mi] = info;

        std::cerr << "Model: material " << mi << " -> Texture ID: " << info.texId
                  << " hasTex=" << (info.hasTex ? "true" : "false") << std::endl;
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh *am = scene->mMeshes[i];
        std::cerr << "Model: mesh[" << i << "] vertices=" << am->mNumVertices << " faces=" << am->mNumFaces << " matIndex=" << am->mMaterialIndex;
        std::cerr << " hasNormals=" << (am->HasNormals() ? 1 : 0) << " hasTexCoords0=" << (am->mTextureCoords[0] ? 1 : 0) << "\n";
        Mesh m;
        processAiMesh(am, scene, m);
        unsigned int matIdx = am->mMaterialIndex;
        if (matIdx < matInfos.size())
        {
            m.textureId = matInfos[matIdx].texId;
            m.hasTexture = matInfos[matIdx].hasTex;
            m.diffuseR = matInfos[matIdx].r;
            m.diffuseG = matInfos[matIdx].g;
            m.diffuseB = matInfos[matIdx].b;
        }
        meshes.push_back(std::move(m));
    }

    // Merge meshes that share the same texture/material to reduce draw calls.
    // Keying by (textureId, diffuseR/G/B, hasTexture) — meshes with identical
    // material/texture are concatenated (verts + indices with offset).
    {
        std::map<std::tuple<GLuint, float, float, float, bool>, Model::Mesh> merged;
        for (auto &m : meshes)
        {
            auto key = std::make_tuple((GLuint)m.textureId, m.diffuseR, m.diffuseG, m.diffuseB, m.hasTexture);
            auto it = merged.find(key);
            if (it == merged.end())
            {
                Model::Mesh mm;
                mm.textureId = m.textureId;
                mm.hasTexture = m.hasTexture;
                mm.diffuseR = m.diffuseR;
                mm.diffuseG = m.diffuseG;
                mm.diffuseB = m.diffuseB;
                merged.emplace(key, std::move(mm));
                it = merged.find(key);
            }
            Model::Mesh &dst = it->second;
            unsigned int base = (unsigned int)dst.verts.size();
            // append vertices
            dst.verts.insert(dst.verts.end(), m.verts.begin(), m.verts.end());
            // append indices with offset
            for (auto idx : m.indices)
                dst.indices.push_back(idx + base);
        }

        // Replace meshes with merged ones
        std::vector<Model::Mesh> newMeshes;
        newMeshes.reserve(merged.size());
        for (auto &p : merged)
            newMeshes.push_back(std::move(p.second));
        meshes.swap(newMeshes);
    }

    // Initialize GPU buffers for each merged mesh
    for (auto &m : meshes)
        setupMeshBuffers(m);

    std::cout << "Loaded model '" << path << "' with " << meshes.size() << " merged mesh(es)" << std::endl;
    return true;
}

void Model::setPosition(float x, float y, float z)
{
    posX = x;
    posY = y;
    posZ = z;
}

void Model::setScale(float sx, float sy, float sz)
{
    scaleX = sx;
    scaleY = sy;
    scaleZ = sz;
}

void Model::setColor(float r, float g, float b)
{
    float cr = r / 255.0f;
    float cg = g / 255.0f;
    float cb = b / 255.0f;
    for (auto &mesh : meshes)
    {
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
    // Batch meshes by texture to reduce texture binds
    std::unordered_map<GLuint, std::vector<Mesh *>> batches;
    for (auto &m : meshes)
    {
        GLuint key = (m.hasTexture && m.textureId != 0) ? m.textureId : 0u;
        batches[key].push_back(&m);
    }

    // Shadow pass: set model matrix and draw using same batching logic
    if (g_shadowPass)
    {
        GLuint shadowProg = createShadowProgram();
        if (shadowProg == 0)
            return;

        // Pass model matrix and alpha
        if (g_shadowModelLoc >= 0)
            glUniformMatrix4fv(g_shadowModelLoc, 1, GL_FALSE, glm::value_ptr(model));
        float alpha = useColorOverride ? overrideA : 1.0f;
        if (g_shadowObjectAlphaLoc >= 0)
            glUniform1f(g_shadowObjectAlphaLoc, alpha);

        // Use same batch grouping but no textures needed for shadow shader
        for (auto &entry : batches)
        {
            auto &vec = entry.second;
            if (vec.empty())
                continue;
            if (vec.size() == 1)
            {
                Mesh &m = *vec[0];
                glBindVertexArray(m.VAO);
                glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0);
            }
            else
            {
                for (auto *pm : vec)
                {
                    Mesh &m = *pm;
                    glBindVertexArray(m.VAO);
                    glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

        // Unbind VAO once after shadow pass
        glBindVertexArray(0);
        return;
    }

    // Normal rendering pass
    GLuint program = createModelProgram();
    if (program == 0)
        return;

    glUseProgram(program);

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    // Set model matrix using cached uniform location
    if (g_modelInfo.locModel >= 0)
        glUniformMatrix4fv(g_modelInfo.locModel, 1, GL_FALSE, glm::value_ptr(model));

    if (g_modelInfo.locNormalMatrix >= 0)
        glUniformMatrix3fv(g_modelInfo.locNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    for (auto &entry : batches)
    {
        GLuint tex = entry.first;
        auto &vec = entry.second;

        if (g_modelInfo.locHasTexture >= 0)
        {
            if (tex != 0)
            {
                glUniform1i(g_modelInfo.locHasTexture, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                if (g_modelInfo.locDiffuseTexture >= 0)
                    glUniform1i(g_modelInfo.locDiffuseTexture, 0);
            }
            else
            {
                glUniform1i(g_modelInfo.locHasTexture, 0);
            }
        }

        // Cache previous uniform values to avoid redundant driver calls
        glm::vec3 prevColor(-1.0f, -1.0f, -1.0f);
        float prevAlpha = -1.0f;

        if (vec.empty())
            ;
        else if (vec.size() == 1)
        {
            Mesh &m = *vec[0];
            // Compute object color & alpha for single merged mesh
            glm::vec3 objectColor = useColorOverride ? glm::vec3(overrideR, overrideG, overrideB) : glm::vec3(m.diffuseR, m.diffuseG, m.diffuseB);
            float alpha = useColorOverride ? overrideA : 1.0f;
            if (g_modelInfo.locObjectColor >= 0 && (objectColor != prevColor))
            {
                glUniform3f(g_modelInfo.locObjectColor, objectColor.x, objectColor.y, objectColor.z);
                prevColor = objectColor;
            }
            if (g_modelInfo.locObjectAlpha >= 0 && alpha != prevAlpha)
            {
                glUniform1f(g_modelInfo.locObjectAlpha, alpha);
                prevAlpha = alpha;
            }

            glBindVertexArray(m.VAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0);
        }
        else
        {
            for (auto *pm : vec)
            {
                Mesh &m = *pm;

                // Compute object color & alpha per mesh
                glm::vec3 objectColor;
                float alpha = 1.0f;
                if (useColorOverride)
                {
                    objectColor = glm::vec3(overrideR, overrideG, overrideB);
                    alpha = overrideA;
                }
                else
                {
                    objectColor = glm::vec3(m.diffuseR, m.diffuseG, m.diffuseB);
                }

                // Only update uniforms if changed
                if (g_modelInfo.locObjectColor >= 0 && (objectColor != prevColor))
                {
                    glUniform3f(g_modelInfo.locObjectColor, objectColor.x, objectColor.y, objectColor.z);
                    prevColor = objectColor;
                }
                if (g_modelInfo.locObjectAlpha >= 0 && alpha != prevAlpha)
                {
                    glUniform1f(g_modelInfo.locObjectAlpha, alpha);
                    prevAlpha = alpha;
                }

                glBindVertexArray(m.VAO);
                glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0);
            }
        }

        // Unbind texture after batch
        if (tex != 0)
        {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Unbind VAO once after all batches
    glBindVertexArray(0);

    glUseProgram(0);
}

// Set view/projection matrices and some global uniforms once per frame
void Model::SetFrameUniforms(const float view[16], const float projection[16])
{
    // cache
    g_cachedView = glm::make_mat4(view);
    g_cachedProjection = glm::make_mat4(projection);
    g_cachedViewPos = glm::vec3(view[12], view[13], view[14]);

    GLuint program = createModelProgram();
    if (program == 0)
        return;

    glUseProgram(program);
    if (g_modelInfo.locView >= 0)
        glUniformMatrix4fv(g_modelInfo.locView, 1, GL_FALSE, glm::value_ptr(g_cachedView));
    if (g_modelInfo.locProj >= 0)
        glUniformMatrix4fv(g_modelInfo.locProj, 1, GL_FALSE, glm::value_ptr(g_cachedProjection));
    if (g_modelInfo.locLightSpace >= 0)
        glUniformMatrix4fv(g_modelInfo.locLightSpace, 1, GL_FALSE, glm::value_ptr(g_lightSpaceMatrix));
    if (g_modelInfo.locLightPos >= 0)
        glUniform3f(g_modelInfo.locLightPos, g_lightPos.x, g_lightPos.y, g_lightPos.z);
    if (g_modelInfo.locViewPos >= 0)
        glUniform3f(g_modelInfo.locViewPos, g_cachedViewPos.x, g_cachedViewPos.y, g_cachedViewPos.z);
    if (g_modelInfo.locLightColor >= 0)
        glUniform3f(g_modelInfo.locLightColor, 1.0f, 1.0f, 1.0f);

    if (g_shadowMapTexture != 0 && g_modelInfo.locShadowMap >= 0)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_shadowMapTexture);
        glUniform1i(g_modelInfo.locShadowMap, 1);
    }
    glUseProgram(0);
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

    // Render to shadow map
    glGetIntegerv(GL_VIEWPORT, g_prevViewport);
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Use shadow shader
    GLuint shadowProg = createShadowProgram();
    if (shadowProg != 0)
    {
        glUseProgram(shadowProg);
        if (g_shadowLightSpaceLoc >= 0)
            glUniformMatrix4fv(g_shadowLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(g_lightSpaceMatrix));
    }

    g_shadowPass = true;
}

void Model::EndShadowPass()
{
    g_shadowPass = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    blurShadowMap();

    // Restore previous viewport saved in BeginShadowPass
    glViewport(g_prevViewport[0], g_prevViewport[1], g_prevViewport[2], g_prevViewport[3]);
}

void Model::SetLightPosition(float x, float y, float z)
{
    g_lightPos = glm::vec3(x, y, z);
}
