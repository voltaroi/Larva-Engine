#pragma once
#ifndef __SHADER_PROGRAM__
#define __SHADER_PROGRAM__
#include <string>
#include <unordered_map>

// Programme GLSL (vertex + fragment) chargé depuis le .pak du jeu ou depuis le disque.
// Un bloc commun (fonctions, uniforms partagés) peut être inséré après la directive #version ;
// "#line 1" garde des numéros de ligne d'erreur qui correspondent au fichier.
class ShaderProgram
{
public:
    // Lit un fichier texte (ResourcePak d'abord, puis disque)
    static bool LoadText(const std::string &path, std::string &out);

    bool load(const std::string &vertPath, const std::string &fragPath, const std::string &prelude = "",
              const std::string &version = "#version 330 core");
    bool loadFromSource(const std::string &vertSource, const std::string &fragSource, const std::string &name = "shader");
    void destroy();

    bool isValid() const { return program != 0; }
    unsigned int id() const { return program; }
    void use() const;

    // Emplacement d'un uniform (mis en cache)
    int loc(const char *name);

    void setInt(const char *name, int v);
    void setFloat(const char *name, float v);
    void setVec2(const char *name, float x, float y);
    void setVec3(const char *name, float x, float y, float z);
    void setVec3(const char *name, const float v[3]);
    void setMat4(const char *name, const float m[16]);
    // Lie une texture 2D sur une unité et donne l'unité au sampler
    void setTexture(const char *name, unsigned int texture, int unit);

private:
    unsigned int program = 0;
    std::unordered_map<std::string, int> locations;
};

#endif
