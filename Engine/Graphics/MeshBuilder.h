#pragma once
#ifndef __MESH_BUILDER__
#define __MESH_BUILDER__
#include <vector>
#include "Model.h"

// Construction de géométrie sur le CPU (routes, murs, décor fusionné...) puis envoi dans un Model.
// Fusionner beaucoup d'objets statiques en un seul mesh réduit fortement le nombre d'appels de dessin.
class MeshBuilder
{
public:
    std::vector<SimpleVertex> verts;
    std::vector<unsigned int> indices;

    bool empty() const { return indices.empty(); }
    void clear();

    // Quadrilatère a-b-c-d (deux triangles) avec une normale commune
    void quad(const float a[3], const float b[3], const float c[3], const float d[3], float nx, float ny, float nz);

    // Copie un mesh en appliquant échelle -> rotation X -> rotation Y -> translation (angles en degrés)
    void addTransformed(const Model::Mesh &src, float px, float py, float pz,
                        float sx, float sy, float sz, float rotXDeg, float rotYDeg);

    // Boîte posée au sol (y = 0) le long du segment a -> b : épaisseur 2 * halfThick, hauteur height
    void segmentBox(float ax, float az, float bx, float bz, float halfThick, float height);

    // Cône vertical (sans fond) : base centrée en (cx, baseY, cz), pointe à baseY + height
    void cone(float cx, float baseY, float cz, float radius, float height, int segments = 10);

    // Boîte centrée en (cx, cy, cz), demi-dimensions hx, hy, hz
    void box(float cx, float cy, float cz, float hx, float hy, float hz);

    // Sphère (ou ellipsoïde) centrée en (cx, cy, cz)
    void sphere(float cx, float cy, float cz, float rx, float ry, float rz, int rings = 8, int segments = 12);

    // Tore centré à l'origine dans le plan XY (axe Z) : volant, pneu, anneau...
    void torus(float majorRadius, float minorRadius, int segments = 24, int sides = 8);

    // Tube (cylindre) du point a au point b, fermé aux deux bouts si caps
    void tube(const float a[3], const float b[3], float radius, int sides = 8, bool caps = true);

    // Boîte aux arêtes arrondies (rayon radius, segments par quart de cercle) centrée en (cx, cy, cz)
    void roundedBox(float cx, float cy, float cz, float hx, float hy, float hz, float radius, int segments = 3);

    // Solide de révolution autour de l'axe Y passant par (cx, cy, cz). profile : suite de points (rayon, hauteur)
    // parcourue de bas en haut par l'extérieur (le côté gauche du parcours est l'intérieur). Les normales sont
    // lissées entre deux segments du profil sauf si l'angle dépasse creaseDeg (arête vive).
    void lathe(const std::vector<float> &profile, int segments = 24, float cx = 0.0f, float cy = 0.0f, float cz = 0.0f,
               float creaseDeg = 35.0f);

    // Crée les buffers GPU du modèle (transformation neutre) avec une couleur unie
    void uploadTo(Model &model, float r, float g, float b) const;
};

#endif
