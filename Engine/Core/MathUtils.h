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
}

#endif
