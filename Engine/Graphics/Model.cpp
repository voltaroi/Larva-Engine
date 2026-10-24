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
#include <algorithm>

static GLuint g_modelProgram = 0;
static GLuint g_shadowProgram = 0;
static GLuint g_shadowMapFBO = 0;
static GLuint g_shadowMapTexture = 0;
static const int SHADOW_WIDTH = 2048;
static const int SHADOW_HEIGHT = 2048;
static float g_shadowHalfSize = 20.0f;
static float g_shadowNear = 1.0f;
static float g_shadowFar = 50.0f;
static Model::Environment g_environment;
// Point lights (environment lighting)
static int g_pointCount = 0;
static float g_pointPos[Model::MAX_POINT_LIGHTS * 4];
static float g_pointColor[Model::MAX_POINT_LIGHTS * 3];
// Point light shadows: occlusion grid (RGBA32F texture, two obstacle bands per cell) and shadow casting capsules
static GLuint g_gridTexture = 0;
static bool g_gridOn = false;
static int g_gridW = 0, g_gridH = 0;
static float g_gridOrigin[2] = {0.0f, 0.0f}, g_gridCell = 1.0f;
static int g_casterCount = 0;
static float g_caster[Model::MAX_SHADOW_CASTERS * 4];
static float g_casterTop[Model::MAX_SHADOW_CASTERS];
static float g_pointDir[Model::MAX_POINT_LIGHTS * 4];
// Point light shadow maps: depth texture arrays (6 layers per slot), cache (0) and sampled (1)
static GLuint g_cubeTex[2] = {0, 0};
static GLuint g_cubeFBO[2] = {0, 0};
static GLuint g_cubeSamplerCmp = 0, g_cubeSamplerRaw = 0;
static GLuint g_cubeProgram = 0;
static GLint g_cubeLocModel = -1, g_cubeLocAlpha = -1, g_cubeLocInstanced = -1, g_cubeLocViewProj = -1, g_cubeLocLamp = -1, g_cubeLocClear = -1;
static int g_cubeLamps = 0, g_cubeRes = 0;
static bool g_cubeOn = false;
static bool g_cubePass = false;
static GLint g_cubePrevFBO = 0;
static bool g_cubePrevCull = false;
static int g_cubeQuality = 2;
static float g_cubeLampSize = 0.25f;
static float g_shadowRange = 0.0f; // 0: no limit
static int g_pointSlot[Model::MAX_POINT_LIGHTS];
static float g_slotLamp[Model::MAX_SHADOW_LAMPS * 4];
static float g_exposure = 1.0f;
static float g_grime = 0.0f;
static glm::mat4 g_lightSpaceMatrix = glm::mat4(1.0f);
static glm::vec3 g_lightPos = glm::vec3(5.0f, 10.0f, 5.0f);
static glm::vec3 g_lightTarget = glm::vec3(0.0f, 0.0f, 0.0f);
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
    GLint locEnv = -1;
    GLint locSunDir = -1;
    GLint locSunColor = -1;
    GLint locSkyAmbient = -1;
    GLint locGroundAmbient = -1;
    GLint locZenith = -1;
    GLint locFogColor = -1;
    GLint locFogDensity = -1;
    GLint locCloudShadow = -1;
    GLint locCloudCoverage = -1;
    GLint locCloudScale = -1;
    GLint locCloudHeight = -1;
    GLint locCloudOffset = -1;
    GLint locLinearOutput = -1;
    GLint locSpecular = -1;
    GLint locShininess = -1;
    GLint locEmissive = -1;
    GLint locPointCount = -1;
    GLint locPointPos = -1;
    GLint locPointDir = -1;
    GLint locPointColor = -1;
    GLint locPBR = -1;
    GLint locRoughness = -1;
    GLint locMetallic = -1;
    GLint locHasDetail = -1;
    GLint locDetail = -1;
    GLint locDetailScale = -1;
    GLint locDetailStrength = -1;
    GLint locObjectScale = -1;
    GLint locGridOn = -1;
    GLint locGrid = -1;
    GLint locGridInfo = -1;
    GLint locCasterCount = -1;
    GLint locCaster = -1;
    GLint locCasterTop = -1;
    GLint locCubeOn = -1;
    GLint locCubeShadow = -1;
    GLint locCubeDepth = -1;
    GLint locCubeInfo = -1;
    GLint locPointSlot = -1;
    GLint locSlotLamp = -1;
    GLint locExposure = -1;
    GLint locGrime = -1;
    GLint locShadowRange = -1;
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RG, GL_FLOAT, nullptr);
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RG, GL_FLOAT, nullptr);
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
    g_modelInfo.locEnv = glGetUniformLocation(g_modelProgram, "uEnv");
    g_modelInfo.locSunDir = glGetUniformLocation(g_modelProgram, "uSunDir");
    g_modelInfo.locSunColor = glGetUniformLocation(g_modelProgram, "uSunColor");
    g_modelInfo.locSkyAmbient = glGetUniformLocation(g_modelProgram, "uSkyAmbient");
    g_modelInfo.locGroundAmbient = glGetUniformLocation(g_modelProgram, "uGroundAmbient");
    g_modelInfo.locZenith = glGetUniformLocation(g_modelProgram, "uZenithColor");
    g_modelInfo.locFogColor = glGetUniformLocation(g_modelProgram, "uFogColor");
    g_modelInfo.locFogDensity = glGetUniformLocation(g_modelProgram, "uFogDensity");
    g_modelInfo.locCloudShadow = glGetUniformLocation(g_modelProgram, "uCloudShadow");
    g_modelInfo.locCloudCoverage = glGetUniformLocation(g_modelProgram, "uCloudCoverage");
    g_modelInfo.locCloudScale = glGetUniformLocation(g_modelProgram, "uCloudScale");
    g_modelInfo.locCloudHeight = glGetUniformLocation(g_modelProgram, "uCloudHeight");
    g_modelInfo.locCloudOffset = glGetUniformLocation(g_modelProgram, "uCloudOffset");
    g_modelInfo.locLinearOutput = glGetUniformLocation(g_modelProgram, "uLinearOutput");
    g_modelInfo.locSpecular = glGetUniformLocation(g_modelProgram, "uSpecular");
    g_modelInfo.locShininess = glGetUniformLocation(g_modelProgram, "uShininess");
    g_modelInfo.locEmissive = glGetUniformLocation(g_modelProgram, "uEmissive");
    g_modelInfo.locPointCount = glGetUniformLocation(g_modelProgram, "uPointCount");
    g_modelInfo.locPointPos = glGetUniformLocation(g_modelProgram, "uPointPos");
    g_modelInfo.locPointDir = glGetUniformLocation(g_modelProgram, "uPointDir");
    g_modelInfo.locPointColor = glGetUniformLocation(g_modelProgram, "uPointColor");
    g_modelInfo.locPBR = glGetUniformLocation(g_modelProgram, "uPBR");
    g_modelInfo.locRoughness = glGetUniformLocation(g_modelProgram, "uRoughness");
    g_modelInfo.locMetallic = glGetUniformLocation(g_modelProgram, "uMetallic");
    g_modelInfo.locHasDetail = glGetUniformLocation(g_modelProgram, "uHasDetail");
    g_modelInfo.locDetail = glGetUniformLocation(g_modelProgram, "uDetail");
    g_modelInfo.locDetailScale = glGetUniformLocation(g_modelProgram, "uDetailScale");
    g_modelInfo.locDetailStrength = glGetUniformLocation(g_modelProgram, "uDetailStrength");
    g_modelInfo.locObjectScale = glGetUniformLocation(g_modelProgram, "uObjectScale");
    g_modelInfo.locGridOn = glGetUniformLocation(g_modelProgram, "uGridOn");
    g_modelInfo.locGrid = glGetUniformLocation(g_modelProgram, "uGrid");
    g_modelInfo.locGridInfo = glGetUniformLocation(g_modelProgram, "uGridInfo");
    g_modelInfo.locCasterCount = glGetUniformLocation(g_modelProgram, "uCasterCount");
    g_modelInfo.locCaster = glGetUniformLocation(g_modelProgram, "uCaster");
    g_modelInfo.locCasterTop = glGetUniformLocation(g_modelProgram, "uCasterTop");
    g_modelInfo.locCubeOn = glGetUniformLocation(g_modelProgram, "uCubeOn");
    g_modelInfo.locCubeShadow = glGetUniformLocation(g_modelProgram, "uCubeShadow");
    g_modelInfo.locCubeDepth = glGetUniformLocation(g_modelProgram, "uCubeDepth");
    g_modelInfo.locCubeInfo = glGetUniformLocation(g_modelProgram, "uCubeInfo");
    g_modelInfo.locPointSlot = glGetUniformLocation(g_modelProgram, "uPointSlot");
    g_modelInfo.locSlotLamp = glGetUniformLocation(g_modelProgram, "uSlotLamp");
    g_modelInfo.locExposure = glGetUniformLocation(g_modelProgram, "uExposure");
    g_modelInfo.locGrime = glGetUniformLocation(g_modelProgram, "uGrime");
    g_modelInfo.locShadowRange = glGetUniformLocation(g_modelProgram, "uShadowRange");
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

void Model::setParentTransform(const float matrix[16])
{
    for (int i = 0; i < 16; ++i)
        parentMatrix[i] = matrix[i];
    hasParent = true;
}

void Model::clearParentTransform()
{
    hasParent = false;
}

void Model::draw()
{
    // Create model matrix
    glm::mat4 model = hasParent ? glm::make_mat4(parentMatrix) : glm::mat4(1.0f);
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
        // Sun shadow map, or a face of a point light shadow map (BeginPointShadowFace)
        GLint locModel = g_cubeLocModel, locAlpha = g_cubeLocAlpha;
        if (!g_cubePass)
        {
            if (createShadowProgram() == 0)
                return;
            locModel = g_shadowModelLoc, locAlpha = g_shadowObjectAlphaLoc;
        }

        // Pass model matrix and alpha
        if (locModel >= 0)
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        float alpha = useColorOverride ? overrideA : 1.0f;
        if (locAlpha >= 0)
            glUniform1f(locAlpha, alpha);

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
    if (g_modelInfo.locSpecular >= 0)
        glUniform1f(g_modelInfo.locSpecular, specular);
    if (g_modelInfo.locShininess >= 0)
        glUniform1f(g_modelInfo.locShininess, shininess);
    if (g_modelInfo.locEmissive >= 0)
        glUniform1f(g_modelInfo.locEmissive, emissive);
    if (g_modelInfo.locPBR >= 0)
        glUniform1i(g_modelInfo.locPBR, pbr ? 1 : 0);
    if (pbr)
    {
        glUniform1f(g_modelInfo.locRoughness, roughness);
        glUniform1f(g_modelInfo.locMetallic, metallic);
    }
    if (g_modelInfo.locObjectScale >= 0)
        glUniform3f(g_modelInfo.locObjectScale, scaleX, scaleY, scaleZ);
    if (g_modelInfo.locHasDetail >= 0)
        glUniform1i(g_modelInfo.locHasDetail, detailTexture != 0 ? 1 : 0);
    if (detailTexture != 0)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, detailTexture);
        glUniform1i(g_modelInfo.locDetail, 2);
        glUniform1f(g_modelInfo.locDetailScale, detailScale);
        glUniform1f(g_modelInfo.locDetailStrength, detailStrength);
        glActiveTexture(GL_TEXTURE0);
    }

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

void Model::drawInstanced(const Instance *instances, int count)
{
    if (count <= 0 || !instances)
        return;
    static GLuint instanceVBO = 0;
    if (!instanceVBO)
        glGenBuffers(1, &instanceVBO);
    GLint locInstanced = -1;
    if (g_shadowPass && g_cubePass)
    {
        locInstanced = g_cubeLocInstanced;
        if (g_cubeLocAlpha >= 0)
            glUniform1f(g_cubeLocAlpha, 1.0f);
    }
    else if (g_shadowPass)
    {
        GLuint shadowProg = createShadowProgram();
        if (shadowProg == 0)
            return;
        static GLint shadowInstancedLoc = -2;
        if (shadowInstancedLoc == -2)
            shadowInstancedLoc = glGetUniformLocation(shadowProg, "uInstanced");
        locInstanced = shadowInstancedLoc;
        if (g_shadowObjectAlphaLoc >= 0)
            glUniform1f(g_shadowObjectAlphaLoc, 1.0f);
    }
    else
    {
        GLuint program = createModelProgram();
        if (program == 0)
            return;
        glUseProgram(program);
        static GLint modelInstancedLoc = -2;
        if (modelInstancedLoc == -2)
            modelInstancedLoc = glGetUniformLocation(program, "uInstanced");
        locInstanced = modelInstancedLoc;
        if (g_modelInfo.locPBR >= 0)
            glUniform1i(g_modelInfo.locPBR, 0);
        if (g_modelInfo.locHasDetail >= 0)
            glUniform1i(g_modelInfo.locHasDetail, 0);
        if (g_modelInfo.locHasTexture >= 0)
            glUniform1i(g_modelInfo.locHasTexture, 0);
    }
    if (locInstanced < 0)
        return; // shaders without instancing support
    glUniform1i(locInstanced, 1);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)count * sizeof(Instance), instances, GL_STREAM_DRAW);
    for (auto &m : meshes)
    {
        if (!m.buffersInitialized)
            continue;
        glBindVertexArray(m.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        for (int k = 0; k < 6; ++k)
        {
            glEnableVertexAttribArray(4 + k);
            glVertexAttribPointer(4 + k, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void *)(k * 4 * sizeof(float)));
            glVertexAttribDivisor(4 + k, 1);
        }
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, 0, count);
        // draw() uses the same VAO without instancing
        for (int k = 0; k < 6; ++k)
            glDisableVertexAttribArray(4 + k);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUniform1i(locInstanced, 0);
    if (!g_shadowPass)
        glUseProgram(0);
}

// Set view/projection matrices and some global uniforms once per frame
void Model::SetFrameUniforms(const float view[16], const float projection[16])
{
    // cache
    g_cachedView = glm::make_mat4(view);
    g_cachedProjection = glm::make_mat4(projection);
    g_cachedViewPos = glm::vec3(glm::inverse(g_cachedView)[3]);

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
        glActiveTexture(GL_TEXTURE0);
    }

    const Model::Environment &e = g_environment;
    if (g_modelInfo.locEnv >= 0)
        glUniform1i(g_modelInfo.locEnv, e.enabled ? 1 : 0);
    if (e.enabled)
    {
        glm::vec3 sun = glm::normalize(glm::vec3(e.sunDir[0], e.sunDir[1], e.sunDir[2]));
        glUniform3f(g_modelInfo.locSunDir, sun.x, sun.y, sun.z);
        glUniform3fv(g_modelInfo.locSunColor, 1, e.sunColor);
        glUniform3fv(g_modelInfo.locSkyAmbient, 1, e.skyAmbient);
        glUniform3fv(g_modelInfo.locGroundAmbient, 1, e.groundAmbient);
        glUniform3fv(g_modelInfo.locZenith, 1, e.zenithColor);
        glUniform3fv(g_modelInfo.locFogColor, 1, e.fogColor);
        glUniform1f(g_modelInfo.locFogDensity, e.fogDensity);
        glUniform1f(g_modelInfo.locCloudShadow, e.cloudShadow);
        glUniform1f(g_modelInfo.locCloudCoverage, e.cloudCoverage);
        glUniform1f(g_modelInfo.locCloudScale, e.cloudScale);
        glUniform1f(g_modelInfo.locCloudHeight, e.cloudHeight);
        glUniform2fv(g_modelInfo.locCloudOffset, 1, e.cloudOffset);
        glUniform1i(g_modelInfo.locLinearOutput, e.linearOutput ? 1 : 0);
    }
    if (g_modelInfo.locPointCount >= 0)
        glUniform1i(g_modelInfo.locPointCount, e.enabled ? g_pointCount : 0);
    if (e.enabled && g_pointCount > 0)
    {
        if (g_modelInfo.locPointPos >= 0)
            glUniform4fv(g_modelInfo.locPointPos, g_pointCount, g_pointPos);
        if (g_modelInfo.locPointColor >= 0)
            glUniform3fv(g_modelInfo.locPointColor, g_pointCount, g_pointColor);
        if (g_modelInfo.locPointDir >= 0)
            glUniform4fv(g_modelInfo.locPointDir, g_pointCount, g_pointDir);
    }
    // Point light shadows: grid on texture unit 3, capsules as uniform arrays
    const bool grid = e.enabled && g_gridOn && g_gridTexture != 0 && g_modelInfo.locGrid >= 0;
    if (g_modelInfo.locGridOn >= 0)
        glUniform1i(g_modelInfo.locGridOn, grid ? 1 : 0);
    if (grid)
    {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_gridTexture);
        glUniform1i(g_modelInfo.locGrid, 3);
        glActiveTexture(GL_TEXTURE0);
        if (g_modelInfo.locGridInfo >= 0)
            glUniform4f(g_modelInfo.locGridInfo, g_gridOrigin[0], g_gridOrigin[1], g_gridCell, 0.0f);
    }
    if (g_modelInfo.locCasterCount >= 0)
        glUniform1i(g_modelInfo.locCasterCount, grid ? g_casterCount : 0);
    if (grid && g_casterCount > 0)
    {
        if (g_modelInfo.locCaster >= 0)
            glUniform4fv(g_modelInfo.locCaster, g_casterCount, g_caster);
        if (g_modelInfo.locCasterTop >= 0)
            glUniform1fv(g_modelInfo.locCasterTop, g_casterCount, g_casterTop);
    }
    // Point light shadow maps: the same texture on units 6 (hardware comparison) and 7 (raw depth, blocker search)
    const bool cube = e.enabled && g_cubeOn && g_cubeTex[1] != 0 && g_modelInfo.locCubeShadow >= 0;
    if (g_modelInfo.locCubeOn >= 0)
        glUniform1i(g_modelInfo.locCubeOn, cube ? 1 : 0);
    if (cube)
    {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_cubeTex[1]);
        glBindSampler(6, g_cubeSamplerCmp);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_cubeTex[1]);
        glBindSampler(7, g_cubeSamplerRaw);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(g_modelInfo.locCubeShadow, 6);
        if (g_modelInfo.locCubeDepth >= 0)
            glUniform1i(g_modelInfo.locCubeDepth, 7);
        float tanHalf = 1.0f;
        float f[3], u[3];
        Model::PointShadowFaceAxes(0, f, u, tanHalf);
        if (g_modelInfo.locCubeInfo >= 0)
            glUniform4f(g_modelInfo.locCubeInfo, (float)g_cubeRes, tanHalf, g_cubeLampSize, (float)g_cubeQuality);
        if (g_modelInfo.locPointSlot >= 0)
            glUniform1iv(g_modelInfo.locPointSlot, Model::MAX_POINT_LIGHTS, g_pointSlot);
        if (g_modelInfo.locSlotLamp >= 0)
            glUniform4fv(g_modelInfo.locSlotLamp, Model::MAX_SHADOW_LAMPS, g_slotLamp);
    }
    if (g_modelInfo.locShadowRange >= 0)
        glUniform1f(g_modelInfo.locShadowRange, g_shadowRange > 0.0f ? g_shadowRange : 1e6f);
    if (g_modelInfo.locExposure >= 0)
        glUniform1f(g_modelInfo.locExposure, g_exposure);
    if (g_modelInfo.locGrime >= 0)
        glUniform1f(g_modelInfo.locGrime, g_grime);
    glUseProgram(0);
}

// ---------------------------------------------------------------------------
//  Point light shadow maps
// ---------------------------------------------------------------------------
static const char *CUBE_VS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in vec4 iModel0;
layout(location = 5) in vec4 iModel1;
layout(location = 6) in vec4 iModel2;
layout(location = 7) in vec4 iModel3;
uniform bool uInstanced;
uniform mat4 model;
uniform mat4 uViewProj;
out vec3 vWorld;
void main()
{
    vec4 w = (uInstanced ? mat4(iModel0, iModel1, iModel2, iModel3) : model) * vec4(aPos, 1.0);
    vWorld = w.xyz;
    gl_Position = uViewProj * w;
}
)GLSL";

// Depth = distance to the light / far distance (linear: 16 bits are enough)
static const char *CUBE_FS = R"GLSL(
#version 330 core
uniform vec4 uLamp; // position, far distance
uniform float uClear;
uniform float objectAlpha;
in vec3 vWorld;
void main()
{
    if (objectAlpha < 0.05)
        discard;
    float d = length(vWorld - uLamp.xyz);
    if (d < uClear)
        discard;
    gl_FragDepth = clamp(d / uLamp.w, 0.0, 1.0);
}
)GLSL";

static const float CUBE_FORWARD[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
static const float CUBE_UP[6][3] = {{0, 1, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};

// Faces a little wider than 90 degrees: the filter of a point near an edge stays inside its face
static float cubeTanHalf()
{
    return g_cubeRes > 0 ? 1.0f / (1.0f - 8.0f / (float)g_cubeRes) : 1.0f;
}

void Model::PointShadowFaceAxes(int face, float forward[3], float up[3], float &tanHalf)
{
    face = std::max(0, std::min(5, face));
    for (int i = 0; i < 3; ++i)
        forward[i] = CUBE_FORWARD[face][i], up[i] = CUBE_UP[face][i];
    tanHalf = cubeTanHalf();
}

static GLuint createCubeProgram()
{
    if (g_cubeProgram)
        return g_cubeProgram;
    GLuint vs = compileShader(GL_VERTEX_SHADER, CUBE_VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, CUBE_FS);
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
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Point shadow shader link error: " << buf << std::endl;
        glDeleteProgram(prog);
        return 0;
    }
    g_cubeProgram = prog;
    g_cubeLocModel = glGetUniformLocation(prog, "model");
    g_cubeLocAlpha = glGetUniformLocation(prog, "objectAlpha");
    g_cubeLocInstanced = glGetUniformLocation(prog, "uInstanced");
    g_cubeLocViewProj = glGetUniformLocation(prog, "uViewProj");
    g_cubeLocLamp = glGetUniformLocation(prog, "uLamp");
    g_cubeLocClear = glGetUniformLocation(prog, "uClear");
    return prog;
}

static void freeCubeMaps()
{
    for (int s = 0; s < 2; ++s)
    {
        if (g_cubeTex[s])
            glDeleteTextures(1, &g_cubeTex[s]);
        if (g_cubeFBO[s])
            glDeleteFramebuffers(1, &g_cubeFBO[s]);
        g_cubeTex[s] = g_cubeFBO[s] = 0;
    }
    g_cubeLamps = g_cubeRes = 0;
}

bool Model::SetupPointShadowMaps(int lamps, int resolution)
{
    lamps = std::max(0, std::min(MAX_SHADOW_LAMPS, lamps));
    resolution = std::max(64, std::min(2048, resolution));
    if (lamps == g_cubeLamps && (lamps == 0 || resolution == g_cubeRes))
        return lamps == 0 || g_cubeTex[1] != 0;
    freeCubeMaps();
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
        g_pointSlot[i] = -1;
    if (lamps == 0)
        return true;
    if (!GLEW_VERSION_3_3 || createCubeProgram() == 0)
        return false;
    if (!g_cubeSamplerCmp)
    {
        glGenSamplers(1, &g_cubeSamplerCmp);
        glSamplerParameteri(g_cubeSamplerCmp, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(g_cubeSamplerCmp, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(g_cubeSamplerCmp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(g_cubeSamplerCmp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(g_cubeSamplerCmp, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(g_cubeSamplerCmp, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenSamplers(1, &g_cubeSamplerRaw);
        glSamplerParameteri(g_cubeSamplerRaw, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(g_cubeSamplerRaw, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(g_cubeSamplerRaw, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(g_cubeSamplerRaw, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(g_cubeSamplerRaw, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }
    while (glGetError() != GL_NO_ERROR)
        ;
    for (int s = 0; s < 2; ++s)
    {
        glGenTextures(1, &g_cubeTex[s]);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_cubeTex[s]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, resolution, resolution, lamps * 6, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &g_cubeFBO[s]);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    if (glGetError() != GL_NO_ERROR)
    {
        std::cerr << "Point shadow maps: allocation failed" << std::endl;
        freeCubeMaps();
        return false;
    }
    g_cubeLamps = lamps, g_cubeRes = resolution;
    // Everything far (lit) until the caller draws the maps
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    for (int s = 0; s < 2; ++s)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, g_cubeFBO[s]);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        for (int layer = 0; layer < lamps * 6; ++layer)
        {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_cubeTex[s], 0, layer);
            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    return true;
}

int Model::PointShadowLamps()
{
    return g_cubeLamps;
}

void Model::BeginPointShadowFace(int set, int slot, int face, const float pos[3], float farDist, float clearRadius)
{
    if (g_cubeLamps <= 0 || slot < 0 || slot >= g_cubeLamps || face < 0 || face > 5)
        return;
    set = set ? 1 : 0;
    if (!g_cubePass)
    {
        glGetIntegerv(GL_VIEWPORT, g_prevViewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_cubePrevFBO);
        g_cubePrevCull = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
        glUseProgram(g_cubeProgram);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_cubeFBO[set]);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_cubeTex[set], 0, slot * 6 + face);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glViewport(0, 0, g_cubeRes, g_cubeRes);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    if (set == 0)
    {
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
    // Camera of the face: rows right, up, -forward (OpenGL looks down -z)
    const glm::vec3 F(CUBE_FORWARD[face][0], CUBE_FORWARD[face][1], CUBE_FORWARD[face][2]);
    const glm::vec3 U(CUBE_UP[face][0], CUBE_UP[face][1], CUBE_UP[face][2]);
    const glm::vec3 R = glm::cross(F, U);
    const glm::vec3 L(pos[0], pos[1], pos[2]);
    glm::mat4 view(1.0f);
    for (int c = 0; c < 3; ++c)
    {
        view[c][0] = R[c];
        view[c][1] = U[c];
        view[c][2] = -F[c];
    }
    view[3][0] = -glm::dot(R, L);
    view[3][1] = -glm::dot(U, L);
    view[3][2] = glm::dot(F, L);
    const float n = 0.02f, f = std::max(farDist, 0.5f), t = cubeTanHalf();
    glm::mat4 proj(0.0f);
    proj[0][0] = 1.0f / t;
    proj[1][1] = 1.0f / t;
    proj[2][2] = -(f + n) / (f - n);
    proj[2][3] = -1.0f;
    proj[3][2] = -2.0f * f * n / (f - n);
    const glm::mat4 vp = proj * view;
    glUniformMatrix4fv(g_cubeLocViewProj, 1, GL_FALSE, glm::value_ptr(vp));
    glUniform4f(g_cubeLocLamp, pos[0], pos[1], pos[2], f);
    glUniform1f(g_cubeLocClear, clearRadius);
    g_cubePass = true;
    g_shadowPass = true;
}

void Model::EndPointShadowFaces()
{
    if (!g_cubePass)
        return;
    g_cubePass = false;
    g_shadowPass = false;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_cubePrevFBO);
    if (g_cubePrevCull)
        glEnable(GL_CULL_FACE);
    glUseProgram(0);
    glViewport(g_prevViewport[0], g_prevViewport[1], g_prevViewport[2], g_prevViewport[3]);
}

void Model::CopyPointShadowCache(int slot, int faceMask)
{
    if (g_cubeLamps <= 0 || slot < 0 || slot >= g_cubeLamps || (faceMask & 63) == 0)
        return;
    GLint prevRead = 0, prevDraw = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
    for (int face = 0; face < 6; ++face)
    {
        if (!((faceMask >> face) & 1))
            continue;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_cubeFBO[0]);
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_cubeTex[0], 0, slot * 6 + face);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_cubeFBO[1]);
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_cubeTex[1], 0, slot * 6 + face);
        glDrawBuffer(GL_NONE);
        glBlitFramebuffer(0, 0, g_cubeRes, g_cubeRes, 0, 0, g_cubeRes, g_cubeRes, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevRead);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
}

void Model::SetPointShadowSlots(int count, const int *slotOfLight, const float *slotPosFar)
{
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
        g_pointSlot[i] = (slotOfLight && i < count && slotOfLight[i] < g_cubeLamps) ? slotOfLight[i] : -1;
    if (slotPosFar)
        for (int i = 0; i < g_cubeLamps * 4; ++i)
            g_slotLamp[i] = slotPosFar[i];
}

void Model::SetPointShadowFilter(int quality, float lampSize)
{
    g_cubeQuality = std::max(0, std::min(3, quality));
    g_cubeLampSize = std::max(0.0f, lampSize);
}

void Model::SetPointShadowRange(float meters)
{
    g_shadowRange = std::max(0.0f, meters);
}

void Model::EnablePointShadowMaps(bool enabled)
{
    g_cubeOn = enabled && g_cubeLamps > 0;
}

void Model::SetExposure(float exposure)
{
    g_exposure = std::max(0.05f, exposure);
}

void Model::SetGrime(float strength)
{
    g_grime = std::max(0.0f, std::min(3.0f, strength));
}

void Model::SetPointShadowGrid(const float *cells, int width, int height, float originX, float originZ, float cellSize)
{
    if (!cells || width <= 0 || height <= 0)
    {
        g_gridOn = false;
        return;
    }
    if (g_gridTexture == 0)
        glGenTextures(1, &g_gridTexture);
    glBindTexture(GL_TEXTURE_2D, g_gridTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, cells);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gridW = width, g_gridH = height;
    g_gridOrigin[0] = originX, g_gridOrigin[1] = originZ;
    g_gridCell = cellSize > 0.0f ? cellSize : 1.0f;
    g_gridOn = true;
}

void Model::EnablePointShadowGrid(bool enabled)
{
    g_gridOn = enabled && g_gridTexture != 0 && g_gridW > 0;
}

void Model::SetShadowCasters(int count, const float *capsules)
{
    g_casterCount = std::max(0, std::min(MAX_SHADOW_CASTERS, count));
    for (int i = 0; i < g_casterCount; ++i)
    {
        const float *c = capsules + i * 5;
        g_caster[i * 4 + 0] = c[0];
        g_caster[i * 4 + 1] = c[2];
        g_caster[i * 4 + 2] = c[1];
        g_caster[i * 4 + 3] = c[4];
        g_casterTop[i] = c[3];
    }
}

// Static methods for shadow mapping
void Model::BeginShadowPass()
{
    // Initialize shadow map if not done yet
    initShadowMap();

    // Calculate light space matrix
    glm::mat4 lightProjection = glm::ortho(-g_shadowHalfSize, g_shadowHalfSize, -g_shadowHalfSize, g_shadowHalfSize, g_shadowNear, g_shadowFar);
    glm::mat4 lightView = glm::lookAt(g_lightPos, g_lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    g_lightSpaceMatrix = lightProjection * lightView;

    // Render to shadow map
    glGetIntegerv(GL_VIEWPORT, g_prevViewport);
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowMapFBO);
    // Moments of the far plane (depth = 1): texels not covered by any caster receive no shadow
    GLfloat previousClear[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClear);
    glClearColor(1.0f, 1.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(previousClear[0], previousClear[1], previousClear[2], previousClear[3]);

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

void Model::SetLightTarget(float x, float y, float z)
{
    g_lightTarget = glm::vec3(x, y, z);
}

bool Model::createFromData(const std::vector<SimpleVertex> &verts, const std::vector<unsigned int> &indices)
{
    for (auto &m : meshes)
    {
        if (m.VAO != 0)
            glDeleteVertexArrays(1, &m.VAO);
        if (m.VBO != 0)
            glDeleteBuffers(1, &m.VBO);
        if (m.EBO != 0)
            glDeleteBuffers(1, &m.EBO);
    }
    meshes.clear();

    if (verts.empty() || indices.empty())
        return false;

    Mesh m;
    m.verts = verts;
    m.indices = indices;
    meshes.push_back(std::move(m));
    setupMeshBuffers(meshes.back());
    return true;
}

void Model::SetPointLights(int count, const float *posRadius, const float *colors, const float *dirCone)
{
    g_pointCount = std::max(0, std::min(MAX_POINT_LIGHTS, count));
    for (int i = 0; i < g_pointCount; ++i)
        for (int k = 0; k < 4; ++k)
            g_pointDir[i * 4 + k] = dirCone ? dirCone[i * 4 + k] : (k == 3 ? -2.0f : 0.0f);
    for (int i = 0; i < g_pointCount * 4; ++i)
        g_pointPos[i] = posRadius[i];
    for (int i = 0; i < g_pointCount * 3; ++i)
        g_pointColor[i] = colors[i];
}

void Model::SetEnvironment(const Environment &env)
{
    g_environment = env;
}

const Model::Environment &Model::GetEnvironment()
{
    return g_environment;
}

void Model::SetShadowArea(float halfSize, float nearPlane, float farPlane)
{
    g_shadowHalfSize = halfSize;
    g_shadowNear = nearPlane;
    g_shadowFar = farPlane;
}

unsigned int Model::GetShadowMapTexture()
{
    return g_shadowMapTexture;
}

void Model::GetLightSpaceMatrix(float out[16])
{
    const float *m = glm::value_ptr(g_lightSpaceMatrix);
    for (int i = 0; i < 16; ++i)
        out[i] = m[i];
}

void Model::setMaterial(float specularStrength, float shininessValue, float emissiveValue)
{
    specular = specularStrength;
    shininess = shininessValue;
    emissive = emissiveValue;
}

void Model::setPBR(float roughnessValue, float metallicValue)
{
    pbr = true;
    roughness = std::max(0.02f, std::min(1.0f, roughnessValue));
    metallic = std::max(0.0f, std::min(1.0f, metallicValue));
}

void Model::clearPBR()
{
    pbr = false;
}

void Model::setDetail(unsigned int texture, float scale, float strength)
{
    detailTexture = texture;
    detailScale = scale;
    detailStrength = strength;
}
