#pragma once
#ifndef __DRAW_2D__
#define __DRAW_2D__
#include <vector>

// Dessin vectoriel 2D à l'écran (même repère que UI : pixels, origine en bas à gauche) :
// traits, cercles, arcs, rectangles, triangles et flèches pour les interfaces, HUD et éditeurs.
//
// Utilisation :
//   Draw2D::setColor(1, 0.5f, 0.1f);
//   Draw2D::setLineWidth(3);
//   Draw2D::circle(x, y, 20, false);
namespace Draw2D
{
    // Couleur et épaisseur des prochains tracés (active aussi le mélange alpha et coupe les textures)
    void setColor(float r, float g, float b, float a = 1.0f);
    void setLineWidth(float width);

    void line(float x0, float y0, float x1, float y1);
    // Points à plat : x0, y0, x1, y1... ; closed relie le dernier point au premier
    void polyline(const std::vector<float> &points, bool closed = false);
    // Polygone convexe plein (même format de points)
    void polygon(const std::vector<float> &points);

    void rect(float x0, float y0, float x1, float y1, bool filled);
    void circle(float cx, float cy, float radius, bool filled, int segments = 24);
    // Arc de cercle entre deux angles (radians, sens trigonométrique)
    void arc(float cx, float cy, float radius, float angle0, float angle1, int segments = 24);
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2);
    // Flèche pleine centrée en (cx, cy) pointant vers la direction (dx, dy)
    void arrow(float cx, float cy, float size, float dx, float dy);
}

#endif
