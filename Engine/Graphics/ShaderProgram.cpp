#include <GL/glew.h>
#include "ShaderProgram.h"
#include "ResourcePak.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

bool ShaderProgram::LoadText(const std::string &path, std::string &out)
{
    std::vector<unsigned char> data;
    if (ResourcePak::IsInitialized() && ResourcePak::LoadFile(path, data))
    {
        out.assign(data.begin(), data.end());
        return true;
    }
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static GLuint compileStage(GLenum type, const std::string &src, const std::string &name)
{
    GLuint s = glCreateShader(type);
    const char *c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Erreur de compilation " << name << ":\n" << log << std::endl;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool ShaderProgram::load(const std::string &vertPath, const std::string &fragPath, const std::string &prelude, const std::string &version)
{
    std::string vs, fs;
    if (!LoadText(vertPath, vs) || !LoadText(fragPath, fs))
    {
        std::cerr << "[Shader] Introuvable : " << vertPath << " / " << fragPath << std::endl;
        return false;
    }
    const std::string header = version + "\n" + prelude + "\n#line 1\n";
    return loadFromSource(header + vs, header + fs, fragPath);
}

bool ShaderProgram::loadFromSource(const std::string &vertSource, const std::string &fragSource, const std::string &name)
{
    GLuint v = compileStage(GL_VERTEX_SHADER, vertSource, name + " (vertex)");
    GLuint f = compileStage(GL_FRAGMENT_SHADER, fragSource, name + " (fragment)");
    if (!v || !f)
    {
        if (v)
            glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        return false;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Erreur d'edition de liens " << name << ":\n" << log << std::endl;
        glDeleteProgram(prog);
        return false;
    }
    destroy();
    program = prog;
    return true;
}

void ShaderProgram::destroy()
{
    if (program)
        glDeleteProgram(program);
    program = 0;
    locations.clear();
}

void ShaderProgram::use() const
{
    glUseProgram(program);
}

int ShaderProgram::loc(const char *name)
{
    auto it = locations.find(name);
    if (it != locations.end())
        return it->second;
    int l = glGetUniformLocation(program, name);
    locations[name] = l;
    return l;
}

void ShaderProgram::setInt(const char *name, int v) { glUniform1i(loc(name), v); }
void ShaderProgram::setFloat(const char *name, float v) { glUniform1f(loc(name), v); }
void ShaderProgram::setVec2(const char *name, float x, float y) { glUniform2f(loc(name), x, y); }
void ShaderProgram::setVec3(const char *name, float x, float y, float z) { glUniform3f(loc(name), x, y, z); }
void ShaderProgram::setVec3(const char *name, const float v[3]) { glUniform3fv(loc(name), 1, v); }
void ShaderProgram::setMat4(const char *name, const float m[16]) { glUniformMatrix4fv(loc(name), 1, GL_FALSE, m); }

void ShaderProgram::setTexture(const char *name, unsigned int texture, int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(loc(name), unit);
    glActiveTexture(GL_TEXTURE0);
}
