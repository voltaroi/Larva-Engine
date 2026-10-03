#include "Music.h"
#include "Synth.h"
#include "Engine/Core/Random.h"
#include <cmath>
#include <algorithm>

namespace Synth
{
    MusicTrack::MusicTrack(float seconds, bool loop) : seconds(seconds), loop(loop) {}

    float MusicTrack::midi(int note)
    {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    void MusicTrack::note(Instrument instrument, float start, float duration, float frequency, float velocity, float echo)
    {
        notes.push_back({instrument, start, duration, frequency, velocity, echo});
    }

    void MusicTrack::duck(float start, float depth, float release)
    {
        ducks.push_back(start);
        ducks.push_back(depth);
        ducks.push_back(release);
    }

    void MusicTrack::setEcho(float delaySeconds, float feedback, float level)
    {
        echoDelay = delaySeconds;
        echoFeedback = feedback;
        echoLevel = level;
    }

    // Dent de scie et carré naïfs (le filtre passe-bas qui suit adoucit le repliement)
    static float saw(float phase) { return 2.0f * (phase - std::floor(phase)) - 1.0f; }
    static float square(float phase, float width = 0.5f) { return phase - std::floor(phase) < width ? 1.0f : -1.0f; }

    // Durée réelle d'un son (avec son relâchement)
    static float tailOf(Instrument i, float duration)
    {
        switch (i)
        {
        case Instrument::Kick: return 0.45f;
        case Instrument::Snare: return 0.35f;
        case Instrument::HiHat: return 0.12f;
        case Instrument::OpenHat: return 0.5f;
        case Instrument::Bass: return duration + 0.06f;
        case Instrument::Pad: return duration + 0.9f;
        case Instrument::Pluck: return duration + 0.35f;
        case Instrument::Lead: return duration + 0.3f;
        }
        return duration;
    }

    // Enveloppe attaque / maintien / relâchement
    static float asr(float t, float duration, float attack, float release)
    {
        float a = attack > 0.0f ? std::min(1.0f, t / attack) : 1.0f;
        float r = t > duration ? std::max(0.0f, 1.0f - (t - duration) / release) : 1.0f;
        return a * r;
    }

    std::vector<float> MusicTrack::render(float peak) const
    {
        const int n = std::max(1, (int)(seconds * RATE));
        std::vector<float> drums(n, 0.0f), melodic(n, 0.0f), send(n, 0.0f);
        auto at = [&](std::vector<float> &bus, long i) -> float *
        {
            if (loop)
                return &bus[((i % n) + n) % n];
            return i >= 0 && i < n ? &bus[i] : nullptr;
        };

        unsigned int seed = 1;
        for (const Note &nt : notes)
        {
            Random rng(seed++ * 2654435761u);
            const long first = (long)(nt.start * RATE);
            const int len = (int)(tailOf(nt.instrument, nt.duration) * RATE);
            const bool drum = nt.instrument == Instrument::Kick || nt.instrument == Instrument::Snare ||
                              nt.instrument == Instrument::HiHat || nt.instrument == Instrument::OpenHat;
            std::vector<float> &bus = drum ? drums : melodic;
            StateVariableFilter filter(1000.0f, 1.0f), filter2(1000.0f, 1.0f);
            float p1 = 0.0f, p2 = 0.0f, p3 = 0.0f, p4 = 0.0f;
            const float f = nt.frequency;
            for (int k = 0; k < len; ++k)
            {
                float t = (float)k / RATE, s = 0.0f;
                switch (nt.instrument)
                {
                case Instrument::Kick:
                {
                    float freq = 45.0f + 115.0f * std::exp(-t / 0.03f);
                    p1 += freq / RATE;
                    s = std::sin(TWO_PI * p1) * std::exp(-t / 0.3f) + rng.nextSigned() * std::exp(-t / 0.004f) * 0.25f;
                    break;
                }
                case Instrument::Snare:
                {
                    filter.set(1900.0f, 0.9f);
                    float noise = filter.process(rng.nextSigned());
                    s = noise * std::exp(-t / 0.11f) * 1.3f + std::sin(TWO_PI * 185.0f * t) * std::exp(-t / 0.05f) * 0.5f;
                    break;
                }
                case Instrument::HiHat:
                case Instrument::OpenHat:
                {
                    filter.set(7500.0f, 0.8f);
                    filter.process(rng.nextSigned());
                    s = filter.high * std::exp(-t / (nt.instrument == Instrument::HiHat ? 0.03f : 0.2f)) * 0.6f;
                    break;
                }
                case Instrument::Bass:
                {
                    p1 += f / RATE;
                    p2 += f * 1.006f / RATE;
                    p3 += f * 0.5f / RATE;
                    filter.set(220.0f + 1400.0f * std::exp(-t / 0.09f), 1.4f);
                    filter.process(saw(p1) + saw(p2));
                    s = (filter.low * 0.7f + std::sin(TWO_PI * p3) * 0.5f) * asr(t, nt.duration, 0.004f, 0.05f);
                    break;
                }
                case Instrument::Pad:
                {
                    p1 += f * 0.9954f / RATE;
                    p2 += f / RATE;
                    p3 += f * 1.0046f / RATE;
                    filter.set(900.0f + 500.0f * std::sin(TWO_PI * 0.15f * (nt.start + t)), 0.8f);
                    filter.process((saw(p1) + saw(p2) + saw(p3)) / 3.0f);
                    s = filter.low * asr(t, nt.duration, 0.45f, 0.9f);
                    break;
                }
                case Instrument::Pluck:
                {
                    p1 += f / RATE;
                    filter.set(500.0f + 3800.0f * std::exp(-t / 0.08f), 1.6f);
                    filter.process(square(p1, 0.3f));
                    s = filter.low * std::exp(-t / 0.22f) * asr(t, nt.duration + 0.3f, 0.002f, 0.05f);
                    break;
                }
                case Instrument::Lead:
                {
                    float vib = 1.0f + 0.005f * std::sin(TWO_PI * 5.5f * t) * std::min(1.0f, std::max(0.0f, (t - 0.15f) / 0.3f));
                    p1 += f * vib / RATE;
                    p2 += f * vib * 1.004f / RATE;
                    p4 += f * vib * 2.0f / RATE;
                    filter.set(2400.0f + 1200.0f * std::exp(-t / 0.15f), 1.1f);
                    filter.process(saw(p1) * 0.6f + square(p2) * 0.4f + std::sin(TWO_PI * p4) * 0.15f);
                    s = filter.low * asr(t, nt.duration, 0.015f, 0.28f);
                    break;
                }
                }
                s *= nt.velocity;
                if (float *o = at(bus, first + k))
                    *o += s;
                if (nt.echo > 0.0f)
                    if (float *o = at(send, first + k))
                        *o += s * nt.echo;
            }
        }

        // Pompage des instruments mélodiques par la grosse caisse
        std::vector<float> gain(n, 1.0f);
        for (size_t d = 0; d + 2 < ducks.size(); d += 3)
        {
            long first = (long)(ducks[d] * RATE);
            int len = (int)(ducks[d + 2] * RATE * 3.0f);
            for (int k = 0; k < len; ++k)
            {
                float t = (float)k / RATE;
                float g = 1.0f - ducks[d + 1] * std::exp(-t / ducks[d + 2]) * std::min(1.0f, t / 0.005f + 0.3f);
                if (float *o = at(gain, first + k))
                    *o = std::min(*o, g);
            }
        }

        // Écho : en boucle, deux passages pour que la traîne de la fin revienne au début
        std::vector<float> echo(n, 0.0f);
        const int delay = std::max(1, (int)(echoDelay * RATE));
        StateVariableFilter tone(2500.0f, 0.7f);
        for (int pass = 0; pass < (loop ? 2 : 1); ++pass)
            for (int i = 0; i < n; ++i)
            {
                float back = 0.0f;
                if (i >= delay)
                    back = echo[i - delay];
                else if (loop)
                    back = echo[i - delay + n];
                tone.process(back);
                echo[i] = send[i] + tone.low * echoFeedback;
            }

        std::vector<float> out(n);
        for (int i = 0; i < n; ++i)
        {
            float m = drums[i] * 0.8f + melodic[i] * gain[i] * 0.6f + (echo[i] - send[i]) * echoLevel;
            out[i] = std::tanh(m * 0.9f); // légère saturation : colle le mélange
        }
        normalize(out, peak);
        return out;
    }
}
