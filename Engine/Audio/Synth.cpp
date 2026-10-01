#include "Synth.h"
#include "Engine/Core/Random.h"
#include <cmath>
#include <algorithm>

void Synth::StateVariableFilter::set(float frequency, float resonance)
{
    w = 2.0f * std::sin(3.14159265f * frequency / RATE);
    q = resonance;
}

float Synth::StateVariableFilter::process(float in)
{
    low += w * band;
    high = in - low - band / q;
    band += w * high;
    return band;
}

void Synth::normalize(std::vector<float> &s, float peak)
{
    float m = 1e-6f;
    for (float v : s)
        m = std::max(m, std::fabs(v));
    for (float &v : s)
        v *= peak / m;
}

void Synth::makeSeamless(std::vector<float> &s, int fade)
{
    int n = (int)s.size();
    if (fade <= 0 || fade >= n)
        return;
    for (int i = 0; i < fade; ++i)
    {
        float t = (float)i / fade;
        s[i] = s[i] * t + s[n - fade + i] * (1.0f - t);
    }
    s.resize(n - fade);
}

std::vector<float> Synth::tone(float freq, float duration, float octaveMix, float peak)
{
    int n = (int)(RATE * duration);
    std::vector<float> s(n);
    for (int i = 0; i < n; ++i)
    {
        float t = (float)i / RATE;
        float env = std::min(1.0f, t / 0.005f) * std::min(1.0f, (duration - t) / 0.04f);
        s[i] = env * (std::sin(TWO_PI * freq * t) + octaveMix * std::sin(TWO_PI * freq * 2.0f * t));
    }
    normalize(s, peak);
    return s;
}

std::vector<float> Synth::engineLoop(int f0, unsigned int seed)
{
    std::vector<float> s(RATE);
    Random rng(seed);
    std::vector<float> jitter(f0);
    for (int i = 0; i < f0; ++i)
        jitter[i] = 0.75f + 0.5f * (rng.nextSigned() * 0.5f + 0.5f);

    float lp = 0.0f;
    for (int i = 0; i < RATE; ++i)
    {
        float t = (float)i / RATE;
        float cyclePos = t * f0;
        int cycle = (int)cyclePos % f0;
        float phase = cyclePos - std::floor(cyclePos);

        float v = 0.0f;
        for (int k = 1; k <= 10; ++k)
            v += std::sin(TWO_PI * k * f0 * t + k * 0.7f) / std::pow((float)k, 0.85f);
        v += 0.45f * std::sin(TWO_PI * (f0 / 2) * t); // irrégularité du cycle 4 temps

        // Impulsion de combustion bruitée, nulle au début et à la fin de chaque cycle
        float envelope = std::pow(std::sin(3.14159265f * phase), 6.0f) * jitter[cycle];
        lp += (rng.nextSigned() - lp) * 0.25f;
        v += envelope * (1.6f + lp * 1.2f);

        s[i] = std::tanh(v * 0.55f); // saturation douce : le "grain" du moteur
    }
    normalize(s, 0.85f);
    return s;
}

std::vector<float> Synth::rumbleLoop(unsigned int seed)
{
    const int n = RATE + 4000;
    std::vector<float> s(n);
    Random rng(seed);
    float brown = 0.0f, lp = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        brown = brown * 0.995f + rng.nextSigned() * 0.08f;
        lp += (rng.nextSigned() - lp) * 0.05f;
        s[i] = brown + lp * 0.8f;
    }
    makeSeamless(s, 4000);
    normalize(s, 0.8f);
    return s;
}

std::vector<float> Synth::filteredNoiseLoop(float frequency, float resonance, unsigned int seed, float peak)
{
    const int n = RATE + 4000;
    std::vector<float> s(n);
    Random rng(seed);
    StateVariableFilter filter(frequency, resonance);
    for (int i = 0; i < n; ++i)
        s[i] = filter.process(rng.nextSigned());
    makeSeamless(s, 4000);
    normalize(s, peak);
    return s;
}

std::vector<float> Synth::impact(unsigned int seed, float duration)
{
    int n = (int)(RATE * duration);
    std::vector<float> s(n);
    Random rng(seed);
    float lp = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float t = (float)i / RATE;
        lp += (rng.nextSigned() - lp) * 0.2f;
        float v = lp * std::exp(-t / 0.05f) * 2.0f;
        v += std::sin(TWO_PI * 55.0f * t) * std::exp(-t / 0.12f) * 1.2f;
        v += (std::sin(TWO_PI * 720.0f * t) + 0.6f * std::sin(TWO_PI * 1490.0f * t)) * std::exp(-t / 0.07f) * 0.35f;
        s[i] = std::tanh(v);
    }
    normalize(s, 0.9f);
    return s;
}
