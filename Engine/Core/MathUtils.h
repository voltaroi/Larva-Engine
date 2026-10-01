#pragma once
#ifndef __MATH_UTILS__
#define __MATH_UTILS__
#include <cmath>
#include <algorithm>

// Petites fonctions mathématiques communes (angles en radians)
namespace MathUtils
{
    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 6.28318531f;
    constexpr float DEG2RAD = PI / 180.0f;
    constexpr float RAD2DEG = 180.0f / PI;

    // Ramène un angle dans [-PI, PI]
    inline float wrapAngle(float a)
    {
        while (a > PI)
            a -= TWO_PI;
        while (a < -PI)
            a += TWO_PI;
        return a;
    }

    inline float clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
    inline float clamp01(float v) { return clamp(v, 0.0f, 1.0f); }
    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    inline float smoothstep(float edge0, float edge1, float x)
    {
        float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    // Facteur de rattrapage exponentiel indépendant du framerate : value += (target - value) * damp(rate, dt)
    inline float damp(float rate, float dt) { return 1.0f - std::exp(-rate * dt); }

    // Offset local -> monde : Rz(roll) puis Ry(yaw), même ordre que Model::draw (avant = +Z local)
    inline void rotateRollYaw(float lx, float ly, float lz, float yaw, float roll, float &wx, float &wy, float &wz)
    {
        float cr = std::cos(roll), sr = std::sin(roll);
        float x1 = lx * cr - ly * sr;
        float y1 = lx * sr + ly * cr;
        float cy = std::cos(yaw), sy = std::sin(yaw);
        wx = x1 * cy + lz * sy;
        wy = y1;
        wz = -x1 * sy + lz * cy;
    }

    // Cinématique inverse à deux segments (bras, jambe) : position de l'articulation du milieu (coude, genou)
    // pour que root -> joint (longueur len1) -> target (len2) atteigne la cible. L'articulation plie du côté
    // du point pole. Renvoie false si la cible est hors de portée (le membre est alors tendu vers elle).
    inline bool solveTwoBone(const float root[3], const float target[3], float len1, float len2,
                             const float pole[3], float joint[3])
    {
        float d[3] = {target[0] - root[0], target[1] - root[1], target[2] - root[2]};
        float dist = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (dist < 1e-6f)
        {
            d[0] = 0.0f, d[1] = 0.0f, d[2] = 1.0f;
            dist = 1e-6f;
        }
        else
            for (float &c : d)
                c /= dist;
        bool reachable = dist <= len1 + len2 && dist >= std::fabs(len1 - len2);
        dist = clamp(dist, std::fabs(len1 - len2) + 1e-4f, len1 + len2 - 1e-4f);
        // Projection de l'articulation sur l'axe root -> target, puis écart perpendiculaire
        float along = (len1 * len1 - len2 * len2 + dist * dist) / (2.0f * dist);
        float side = std::sqrt(std::max(0.0f, len1 * len1 - along * along));
        float p[3] = {pole[0] - root[0], pole[1] - root[1], pole[2] - root[2]};
        float dp = p[0] * d[0] + p[1] * d[1] + p[2] * d[2];
        float bend[3] = {p[0] - d[0] * dp, p[1] - d[1] * dp, p[2] - d[2] * dp};
        float bl = std::sqrt(bend[0] * bend[0] + bend[1] * bend[1] + bend[2] * bend[2]);
        if (bl < 1e-6f)
        {
            // Pôle dans l'axe : n'importe quelle perpendiculaire
            bend[0] = -d[1], bend[1] = d[0], bend[2] = 0.0f;
            bl = std::sqrt(bend[0] * bend[0] + bend[1] * bend[1]);
            if (bl < 1e-6f)
                bend[0] = 1.0f, bend[1] = 0.0f, bl = 1.0f;
        }
        for (int i = 0; i < 3; ++i)
            joint[i] = root[i] + d[i] * along + bend[i] / bl * side;
        return reachable;
    }
}

#endif
