#include <GL/glew.h>
#include "Model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include <iostream>
#include <cstring>

static GLuint g_modelProgram = 0;

static GLuint compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
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

static GLuint createModelProgram() {
    if (g_modelProgram) return g_modelProgram;

    const char *vs = R"GLSL(
        #version 120
        varying vec3 vNormal;
        varying vec2 vTex;
        varying vec3 vPos;
        void main() {
            vNormal = normalize(gl_NormalMatrix * gl_Normal);
            vTex = gl_MultiTexCoord0.xy;
            vPos = vec3(gl_ModelViewMatrix * gl_Vertex);
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        }
    )GLSL";

    const char *fs = R"GLSL(
        #version 120
        varying vec3 vNormal;
        varying vec2 vTex;
        varying vec3 vPos;
        uniform sampler2D uTex;
        uniform vec3 uColor;
        uniform vec3 uLightDir;
        void main() {
            vec3 N = normalize(vNormal);
            vec3 L = normalize(uLightDir);
            float diff = max(dot(N, L), 0.0);
            vec4 tex = texture2D(uTex, vTex);
            vec3 base = tex.rgb * uColor;
            vec3 col = base * (0.2 + 0.8 * diff);
            gl_FragColor = vec4(col, tex.a);
        }
    )GLSL";

    GLuint vsId = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fsId = compileShader(GL_FRAGMENT_SHADER, fs);
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
    : posX(0.0f), posY(0.0f), posZ(0.0f), scaleX(1.0f), scaleY(1.0f), scaleZ(1.0f)
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
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path,
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
                if (!full.empty()) data = stbi_load(full.c_str(), &w, &h, &channels, STBI_rgb_alpha);
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

void Model::draw()
{
    glPushMatrix();
    glTranslatef(posX, posY, posZ);
    glScalef(scaleX, scaleY, scaleZ);

    for (const auto &m : meshes) {
        GLuint prog = createModelProgram();
        if (m.hasTexture && m.textureId != 0 && prog != 0) {
            glUseProgram(prog);
            GLint locTex = glGetUniformLocation(prog, "uTex");
            GLint locColor = glGetUniformLocation(prog, "uColor");
            GLint locLight = glGetUniformLocation(prog, "uLightDir");
            if (locTex >= 0) glUniform1i(locTex, 0);
            if (locColor >= 0) glUniform3f(locColor, m.diffuseR, m.diffuseG, m.diffuseB);
            if (locLight >= 0) glUniform3f(locLight, 0.0f, 0.707f, 0.707f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m.textureId);

            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_NORMAL_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);

            std::vector<float> vbuf;
            std::vector<float> nbuf;
            std::vector<float> tbuf;
            vbuf.reserve(m.verts.size()*3);
            nbuf.reserve(m.verts.size()*3);
            tbuf.reserve(m.verts.size()*2);
            for (const auto &vv : m.verts) {
                vbuf.push_back(vv.x); vbuf.push_back(vv.y); vbuf.push_back(vv.z);
                nbuf.push_back(vv.nx); nbuf.push_back(vv.ny); nbuf.push_back(vv.nz);
                tbuf.push_back(vv.u); tbuf.push_back(vv.v);
            }

            glVertexPointer(3, GL_FLOAT, 0, vbuf.data());
            glNormalPointer(GL_FLOAT, 0, nbuf.data());
            glTexCoordPointer(2, GL_FLOAT, 0, tbuf.data());

            glDrawElements(GL_TRIANGLES, (GLsizei)m.indices.size(), GL_UNSIGNED_INT, m.indices.data());

            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);

            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        } else {
            if (m.hasTexture && m.textureId != 0) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m.textureId);
                glColor3f(1.0f, 1.0f, 1.0f);
            } else {
                glDisable(GL_TEXTURE_2D);
                glColor3f(m.diffuseR, m.diffuseG, m.diffuseB);
            }

            glBegin(GL_TRIANGLES);
            for (size_t i = 0; i < m.indices.size(); ++i) {
                unsigned int idx = m.indices[i];
                if (idx < m.verts.size()) {
                    const SimpleVertex &v = m.verts[idx];
                    glNormal3f(v.nx, v.ny, v.nz);
                    if (m.hasTexture) glTexCoord2f(v.u, v.v);
                    glVertex3f(v.x, v.y, v.z);
                }
            }
            glEnd();

            if (m.hasTexture && m.textureId != 0) {
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
            }
        }
    }

    glPopMatrix();
}
