#pragma once
#ifndef __SYNTH__
#define __SYNTH__
#include <vector>

// Synthèse sonore procédurale : génère des échantillons mono dans [-1, 1] (à donner à AudioDevice::createBuffer).
// Aucun fichier audio n'est nécessaire et les sons sont reproductibles (graines fixes).
namespace Synth
{
    const int RATE = 44100;
    const float TWO_PI = 6.28318531f;

    // Filtre à variables d'état (passe-bas, passe-bande, passe-haut en même temps)
    struct StateVariableFilter
    {
        float w = 0.0f, q = 1.0f;
        float low = 0.0f, band = 0.0f, high = 0.0f;

        StateVariableFilter(float frequency = 1000.0f, float resonance = 1.0f) { set(frequency, resonance); }
        void set(float frequency, float resonance);
        // Renvoie la sortie passe-bande (low et high restent lisibles)
        float process(float in);
    };

    // Passe-bas à un pôle : k dans ]0, 1], plus petit = plus sourd
    struct OnePole
    {
        float k = 0.5f, y = 0.0f;
        explicit OnePole(float k = 0.5f) : k(k) {}
        float process(float in) { return y += (in - y) * k; }
    };

    // Met le pic d'amplitude à peak
    void normalize(std::vector<float> &samples, float peak);
    // Fondu enchaîné fin -> début pour une boucle sans clic (raccourcit le son de fade échantillons)
    void makeSeamless(std::vector<float> &samples, int fade);

    // Bip : sinus + octave, attaque et relâchement courts
    std::vector<float> tone(float frequency, float duration, float octaveMix = 0.25f, float peak = 0.7f);

    // Moteur à explosions en boucle d'une seconde : harmoniques de la fréquence d'allumage f0 (entière,
    // pour que la boucle tombe juste), sous-harmonique du cycle 4 temps et combustions irrégulières.
    // Rejouer avec pitch = fréquence voulue / f0.
    std::vector<float> engineLoop(int f0, unsigned int seed);

    // Grondement grave en boucle (bruit brun) : roulement, graviers, vent...
    std::vector<float> rumbleLoop(unsigned int seed);

    // Bruit filtré en boucle autour d'une fréquence (souffle, sifflement d'air)
    std::vector<float> filteredNoiseLoop(float frequency, float resonance, unsigned int seed, float peak = 0.8f);

    // Choc sourd : bruit + coup grave + résonance métallique
    std::vector<float> impact(unsigned int seed, float duration = 0.5f);
}

#endif
