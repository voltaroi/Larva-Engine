#pragma once
#ifndef __MUSIC__
#define __MUSIC__
#include <vector>

// Petit séquenceur de musique procédurale : on place des notes (batterie, basse, nappes, arpèges, mélodie)
// sur une durée donnée, puis render() synthétise le morceau en échantillons mono (pour AudioDevice::createBuffer).
// En mode boucle, les fins de notes et l'écho débordent sur le début : la boucle se rejoue sans coupure.
namespace Synth
{
    enum class Instrument
    {
        Kick,    // grosse caisse : sinus dont la hauteur chute
        Snare,   // caisse claire : bruit filtré + coup grave
        HiHat,   // charleston fermé
        OpenHat, // charleston ouvert
        Bass,    // deux dents de scie désaccordées, filtre qui se ferme
        Pad,     // nappe : trois dents de scie désaccordées, attaque lente
        Pluck,   // arpège pincé : onde carrée filtrée, courte
        Lead     // mélodie : scie + carré avec vibrato
    };

    class MusicTrack
    {
    public:
        explicit MusicTrack(float seconds, bool loop = true);

        // Fréquence d'une note MIDI (69 = la 440 Hz)
        static float midi(int note);

        // velocity : volume de la note ; echo : part envoyée dans l'écho
        void note(Instrument instrument, float start, float duration, float frequency, float velocity = 1.0f, float echo = 0.0f);
        // Pompage : les instruments mélodiques baissent un instant (à mettre sur chaque grosse caisse)
        void duck(float start, float depth = 0.45f, float release = 0.22f);
        void setEcho(float delaySeconds, float feedback = 0.35f, float level = 0.4f);

        // Synthèse du morceau (pic d'amplitude = peak)
        std::vector<float> render(float peak = 0.8f) const;

        float length() const { return seconds; }

    private:
        struct Note
        {
            Instrument instrument;
            float start, duration, frequency, velocity, echo;
        };
        float seconds;
        bool loop;
        float echoDelay = 0.4f, echoFeedback = 0.35f, echoLevel = 0.4f;
        std::vector<Note> notes;
        std::vector<float> ducks;
    };
}

#endif
