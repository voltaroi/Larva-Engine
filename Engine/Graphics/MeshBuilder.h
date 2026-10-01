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

    // Crée les buffers GPU du modèle (transformation neutre) avec une couleur unie
    void uploadTo(Model &model, float r, float g, float b) const;
};

#endif
