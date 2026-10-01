#pragma once
#ifndef __RANDOM__
#define __RANDOM__
#include <cstdlib>

// Générateur pseudo-aléatoire déterministe (congruentiel linéaire) : même graine = même suite,
// utile pour la génération procédurale (décor, sons synthétisés...).
class Random
{
public:
    explicit Random(unsigned int seed = 1u) : state(seed) {}

    void seed(unsigned int s) { state = s; }

    unsigned int next()
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    // [0, 1]
    float next01() { return ((next() >> 8) & 0xFFFF) / 65535.0f; }
    // [-1, 1]
    float nextSigned() { return next01() * 2.0f - 1.0f; }
    // [a, b]
    float range(float a, float b) { return a + (b - a) * next01(); }

    // Tirage non déterministe basé sur std::rand (à initialiser avec std::srand)
    static float global(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }

private:
    unsigned int state;
};

#endif
