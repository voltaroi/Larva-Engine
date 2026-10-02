#pragma once
#ifndef __LEVEL__
#define __LEVEL__
#include <map>
#include <string>
#include <vector>

// Description d'un niveau (map) : réglages globaux + liste d'entités placées.
// Pur C++ sans OpenGL : utilisable par le client, le serveur et l'éditeur.
//
// Format texte (.lvl), une propriété par ligne, lisible et facile à comparer dans git :
//   larva_level 1
//   name Main
//   sky 0.5 0.7 1
//   light_position 5 10 5
//   entity Floor
//   type mesh
//   model Models/cube.fbx
//   position 0 -5 0
//   scale 50 1 50
//   collision 1
//   end
struct LevelEntity
{
    enum class Type
    {
        Mesh,        // modèle 3D affiché
        PlayerStart, // point d'apparition du joueur
        Marker       // point invisible repéré par son nom ou son tag (logique de jeu)
    };

    std::string name;
    Type type = Type::Mesh;
    std::string model;                  // chemin dans les Assets (ex. "Models/cube.fbx")
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f}; // degrés, appliqués X puis Y puis Z (comme Model)
    float scale[3] = {1.0f, 1.0f, 1.0f};
    bool useColor = false;              // couleur forcée à la place de celle du modèle
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    bool collision = false;             // génère une boîte de collision en jeu
    bool visible = true;
    std::string tag;                    // libre, pour retrouver des groupes d'entités
    std::map<std::string, std::string> properties; // clés / valeurs libres pour le gameplay

    std::string property(const std::string &key, const std::string &fallback = "") const;

    static const char *typeName(Type type);
    static bool typeFromName(const std::string &name, Type &out);
};

class Level
{
public:
    std::string name = "Untitled";
    float skyColor[3] = {0.5f, 0.7f, 1.0f};
    float lightPosition[3] = {5.0f, 10.0f, 5.0f};
    float lightTarget[3] = {0.0f, 0.0f, 0.0f};
    float shadowArea = 20.0f; // demi-taille de la zone couverte par la carte d'ombres

    std::vector<LevelEntity> entities;

    void clear();

    bool loadFromFile(const std::string &path);
    bool loadFromString(const std::string &text);
    bool saveToFile(const std::string &path) const;
    std::string saveToString() const;

    LevelEntity *find(const std::string &entityName);
    const LevelEntity *find(const std::string &entityName) const;
    std::vector<const LevelEntity *> findByTag(const std::string &tag) const;
    // Premier PlayerStart du niveau (nullptr s'il n'y en a pas)
    const LevelEntity *playerStart() const;

    // Nom libre dérivé de base ("Cube" -> "Cube_2" si "Cube" existe déjà)
    std::string uniqueName(const std::string &base) const;

    // Dernière erreur de chargement (ligne fautive...)
    const std::string &lastError() const { return error; }

private:
    std::string error;
};

#endif
