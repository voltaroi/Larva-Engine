#pragma once
#ifndef __LEVEL_SCENE__
#define __LEVEL_SCENE__
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Level.h"
#include "../Core/AABB.h"
#include "../Graphics/Model.h"

// Niveau prêt à afficher : charge un .lvl (game.pak en priorité, puis disque),
// charge chaque modèle une seule fois et le dessine pour toutes les entités qui l'utilisent.
//
// Utilisation en jeu :
//   LevelScene scene;
//   scene.load("Levels/Main.lvl");
//   // chaque image, entre Model::BeginShadowPass / EndShadowPass puis après SetFrameUniforms :
//   scene.draw();
//   // ou tout en un, après avoir placé la caméra (gluLookAt...) :
//   scene.render();
class LevelScene
{
public:
    Level level;

    bool load(const std::string &path);
    // Reprend un niveau déjà en mémoire (éditeur, niveau généré...)
    void setLevel(const Level &newLevel);
    // Charge les modèles manquants (à rappeler si des entités ont été ajoutées ou leur modèle changé)
    void refreshModels();
    // Libère les modèles chargés
    void unloadModels();

    // Applique la lumière et la zone d'ombre du niveau à Model
    void applyLighting() const;
    // Dessine les entités visibles (dans la passe d'ombre ou la passe normale, comme Model::draw)
    void draw();
    // Passe d'ombre + passe normale, avec les matrices OpenGL courantes (GL_MODELVIEW / GL_PROJECTION)
    void render();

    // Boîtes de collision (alignées sur les axes) des entités marquées "collision"
    std::vector<AABB> collisionBoxes() const;

    // Boîte locale du modèle (avant transformation) ; boîte unité pour les entités sans modèle
    bool localBounds(const LevelEntity &entity, float outMin[3], float outMax[3]) const;
    // Boîte englobante en coordonnées monde
    AABB worldBounds(const LevelEntity &entity) const;
    // Matrice monde de l'entité (colonne majeure, même ordre que Model::draw)
    static void entityMatrix(const LevelEntity &entity, float out[16]);

    // Lancer de rayon (dir normalisée) contre les triangles des entités visibles (boîte si le modèle manque).
    // Renvoie l'index de l'entité la plus proche, -1 sinon. includeHelpers : PlayerStart et Marker aussi.
    int raycast(const float origin[3], const float dir[3], float &outDistance, int ignoreEntity = -1, bool includeHelpers = false) const;

    // Le modèle de ce chemin s'est-il chargé ?
    bool isModelLoaded(const std::string &path) const;

    const std::string &lastError() const { return error; }

private:
    struct ModelSlot
    {
        std::unique_ptr<Model> model; // nullptr si le chargement a échoué
        float boundsMin[3] = {-0.5f, -0.5f, -0.5f};
        float boundsMax[3] = {0.5f, 0.5f, 0.5f};
    };
    std::map<std::string, ModelSlot> models;
    std::string error;

    const ModelSlot *slotFor(const std::string &path) const;
};

#endif
