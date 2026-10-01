#pragma once
#ifndef __COLLISION_2D__
#define __COLLISION_2D__
#include <vector>

// Collisions dans le plan XZ pour des objets représentés par des cercles (véhicules, personnages vus de dessus)
namespace Collision2D
{
    struct Segment
    {
        float ax, az, bx, bz;
    };

    // Boîte orientée : centre, demi-tailles et rotation autour de Y (radians, même convention que Model)
    struct OrientedBox
    {
        float x, z;
        float hx, hz;
        float rotY;
    };

    // Repousse le cercle (x, z, radius) hors d'une capsule (segment épaissi de segRadius).
    // Renvoie true en cas de contact ; (nx, nz) reçoit la normale de sortie.
    bool pushCircleFromSegment(float &x, float &z, float radius, const Segment &seg, float segRadius, float &nx, float &nz);

    // Repousse le cercle hors d'une boîte orientée (y compris s'il est déjà à l'intérieur).
    bool pushCircleFromBox(float &x, float &z, float radius, const OrientedBox &box, float &nx, float &nz);

    // Grille uniforme de segments pour ne tester que ceux proches d'un point
    class SegmentGrid
    {
    public:
        // Zone couverte : carré [origin, origin + cells * cellSize] sur X et Z
        void reset(float origin, float extent, int cells);
        // margin : distance supplémentaire à laquelle le segment doit être trouvé
        void insert(int index, const Segment &seg, float margin);
        // Indices des segments de la cellule contenant (x, z) ; vide hors de la grille
        const std::vector<int> &query(float x, float z) const;

    private:
        float origin = 0.0f;
        float cellSize = 1.0f;
        int cells = 0;
        std::vector<std::vector<int>> grid;
        std::vector<int> empty;
    };
}

#endif
