#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

// Generador procedural de beats por género (batería + bajo/808 + acordes).
namespace Beat
{
struct Spec
{
    juce::String name;
    int bpmMin, bpmMax;
    bool minor;
    const char* kick;   // 16 pasos, 'x' = golpe
    const char* snare;
    const char* hat;
    const char* bass;
    float swing;        // 0..0.3
    bool hatRolls;
    bool longKick;      // true = 808 largo (trap/drill)
    bool stabs;         // true = acordes cortos, false = pad sostenido
    float chordLevel;
};

inline const std::vector<Spec>& genres()
{
    static const std::vector<Spec> g = {
        { "Trap",       130, 150, true,  "x.....x...x.....", "........x.......", "x.x.x.x.x.x.x.x.", "x.....x...x.....", 0.00f, true,  true,  false, 0.12f },
        { "Drill",      138, 145, true,  "x.......x...x...", "............x...", "x.x.x.x.x.x.x.x.", "x.......x...x...", 0.00f, true,  true,  false, 0.10f },
        { "Reggaeton",   88, 100, true,  "x...x...x...x...", "...x..x....x..x.", "x.x.x.x.x.x.x.x.", "x.....x.x.....x.", 0.00f, false, false, true,  0.14f },
        { "Hip-Hop",     85,  95, true,  "x.....x..x......", "....x.......x...", "x.x.x.x.x.x.x.x.", "x.....x..x......", 0.12f, false, false, false, 0.16f },
        { "Pop",        100, 120, false, "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "x.x...x.x.x...x.", 0.00f, false, false, true,  0.16f },
        { "House",      120, 126, true,  "x...x...x...x...", "....x.......x...", "..x...x...x...x.", "..x...x...x...x.", 0.00f, false, false, true,  0.15f },
        { "Lo-fi",       70,  85, false, "x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "x.......x.......", 0.20f, false, false, false, 0.22f },
        { "R&B",         65,  80, true,  "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "x.......x.x.....", 0.10f, false, false, false, 0.20f },
        { "Afrobeats",   98, 108, true,  "x.....x..x..x...", "....x.......x...", "x.x.xxx.x.x.xxx.", "x.....x..x..x...", 0.05f, false, false, true,  0.14f },
    };
    return g;
}

inline float mtof (int m) { return 440.0f * std::pow (2.0f, (float) (m - 69) / 12.0f); }

inline void fadeTail (std::vector<float>& v, int n = 256)
{
    n = std::min (n, (int) v.size());
    for (int i = 0; i < n; ++i)
        v[v.size() - 1 - (size_t) i] *= (float) i / (float) n;
}

inline std::vector<float> makeKick (double sr, bool longK)
{
    const int n = (int) (sr * (longK ? 0.7 : 0.35));
    std::vector<float> v ((size_t) n);
    double ph = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double f = (longK ? 42.0 : 48.0) + (longK ? 90.0 : 120.0) * std::exp (-t * (longK ? 18.0 : 28.0));
        ph += juce::MathConstants<double>::twoPi * f / sr;
        const double env = std::exp (-t * (longK ? 4.5 : 11.0));
        v[(size_t) i] = std::tanh (1.6f * (float) (std::sin (ph) * env));
    }
    fadeTail (v);
    return v;
}

inline std::vector<float> makeSnare (double sr, std::mt19937& rng)
{
    std::uniform_real_distribution<float> U (-1.f, 1.f);
    const int n = (int) (sr * 0.22);
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const float noise = U (rng) * (float) std::exp (-t * 22.0);
        const float tone = (float) (std::sin (juce::MathConstants<double>::twoPi * 185.0 * t) * std::exp (-t * 30.0));
        v[(size_t) i] = 0.6f * noise + 0.4f * tone;
    }
    fadeTail (v);
    return v;
}

inline std::vector<float> makeHat (double sr, bool open, std::mt19937& rng)
{
    std::uniform_real_distribution<float> U (-1.f, 1.f);
    const int n = (int) (sr * (open ? 0.25 : 0.06));
    std::vector<float> v ((size_t) n);
    float prev = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const float x = U (rng);
        const float hp = x - prev; // paso alto simple
        prev = x;
        v[(size_t) i] = hp * 0.5f * (float) std::exp (-t * (open ? 18.0 : 90.0));
    }
    fadeTail (v, 64);
    return v;
}

inline std::vector<float> makeBass (double sr, float f, int n, bool longK)
{
    std::vector<float> v ((size_t) n);
    double ph = 0.0;
    const double dec = longK ? 1.6 : 3.5;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double fi = f * (longK ? (1.0 + 0.5 * std::exp (-t * 40.0)) : 1.0);
        ph += juce::MathConstants<double>::twoPi * fi / sr;
        const float att = (float) std::min (1.0, t / 0.004);
        const float env = att * (float) std::exp (-t * dec);
        const float s = (float) (std::sin (ph) + 0.25 * std::sin (2.0 * ph));
        v[(size_t) i] = std::tanh (1.8f * s * env);
    }
    fadeTail (v, 512);
    return v;
}

inline std::vector<float> makeChord (double sr, const std::vector<float>& freqs, int n, bool stab)
{
    std::vector<float> v ((size_t) n, 0.f);
    float amps[7];
    for (int k = 1; k <= 6; ++k) amps[k] = 1.0f / std::pow ((float) k, 1.4f);

    for (float f : freqs)
        for (float det : { -0.003f, 0.003f })
        {
            double ph = 0.0;
            const double inc = juce::MathConstants<double>::twoPi * f * (1.0 + det) / sr;
            for (int i = 0; i < n; ++i)
            {
                ph += inc;
                float s = 0.f;
                for (int k = 1; k <= 6; ++k) s += amps[k] * (float) std::sin ((double) k * ph);
                v[(size_t) i] += s;
            }
        }

    const float norm = 1.0f / ((float) freqs.size() * 2.0f);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const float att = (float) std::min (1.0, t / (stab ? 0.002 : 0.15));
        const float env = att * (float) std::exp (-t * (stab ? 9.0 : 0.4));
        v[(size_t) i] *= norm * env;
    }
    fadeTail (v, (int) (sr * 0.08));
    return v;
}

// Devuelve un bucle estéreo de 'bars' compases (la cola se pliega al inicio para que el bucle sea continuo).
inline juce::AudioBuffer<float> render (const Spec& s, int bpm, int key, int bars, double sr, int seed)
{
    std::mt19937 rng ((unsigned) seed);
    std::uniform_real_distribution<float> U (0.f, 1.f);

    const int stepN   = (int) (sr * 60.0 / bpm / 4.0);
    const int loopLen = stepN * 16 * bars;
    const int tail    = (int) (sr * 2.0);

    juce::AudioBuffer<float> out (2, loopLen + tail);
    out.clear();

    auto place = [&] (const std::vector<float>& snd, int start, float gain, float pan)
    {
        if (start < 0 || start >= out.getNumSamples()) return;
        const float a  = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = gain * std::cos (a) * 1.414f;
        const float gr = gain * std::sin (a) * 1.414f;
        auto* L = out.getWritePointer (0);
        auto* R = out.getWritePointer (1);
        const int n = std::min ((int) snd.size(), out.getNumSamples() - start);
        for (int i = 0; i < n; ++i)
        {
            L[start + i] += snd[(size_t) i] * gl;
            R[start + i] += snd[(size_t) i] * gr;
        }
    };

    struct Ch { int off; bool minor; };
    const std::vector<Ch> prog = s.minor
        ? std::vector<Ch> { { 0, true }, { 8, false }, { 3, false }, { 10, false } }
        : std::vector<Ch> { { 0, false }, { 7, false }, { 9, true }, { 5, false } };

    const auto kick   = makeKick (sr, s.longKick);
    const auto snare  = makeSnare (sr, rng);
    const auto hatC   = makeHat (sr, false, rng);
    const auto hatO   = makeHat (sr, true, rng);

    std::vector<std::vector<float>> chordCache (4);
    auto chordFreqs = [&] (const Ch& c)
    {
        const int root = 60 + key + c.off;
        return std::vector<float> { mtof (root), mtof (root + (c.minor ? 3 : 4)), mtof (root + 7) };
    };

    auto hit = [] (const char* p, int step) { return p[step % 16] == 'x'; };

    for (int bar = 0; bar < bars; ++bar)
    {
        const int ci = bar % 4;
        const Ch ch = prog[(size_t) ci];
        const int barStart = bar * 16 * stepN;

        // Acordes
        if (! s.stabs)
        {
            if (chordCache[(size_t) ci].empty())
                chordCache[(size_t) ci] = makeChord (sr, chordFreqs (ch), 16 * stepN, false);
            place (chordCache[(size_t) ci], barStart, s.chordLevel, 0.f);
        }
        else
        {
            if (chordCache[(size_t) ci].empty())
                chordCache[(size_t) ci] = makeChord (sr, chordFreqs (ch), (int) (sr * 0.3), true);
            for (int st : { 0, 3, 6, 10 })
                place (chordCache[(size_t) ci], barStart + st * stepN, s.chordLevel, 0.f);
        }

        // Nota del bajo / 808
        int r = 24 + key + ch.off;
        while (r > 35) r -= 12;
        const float bf = mtof (r);

        for (int step = 0; step < 16; ++step)
        {
            const int pos = barStart + step * stepN + ((step & 1) ? (int) (s.swing * (float) stepN) : 0);

            if (hit (s.kick, step))
                place (kick, pos, s.longKick ? 0.35f : 0.9f, 0.f);

            if (hit (s.snare, step))
                place (snare, pos, 0.55f, 0.f);

            if (hit (s.hat, step))
            {
                const float vel = (step % 4 == 0) ? 1.0f : 0.6f + 0.4f * U (rng);
                const float pan = (U (rng) - 0.5f) * 0.3f;
                place (hatC, pos, 0.25f * vel, pan);

                if (s.hatRolls && step % 4 == 3 && U (rng) < 0.5f)
                {
                    place (hatC, pos + stepN / 3, 0.18f, pan);
                    place (hatC, pos + 2 * stepN / 3, 0.15f, -pan);
                }
            }
            if (step == 14 && U (rng) < 0.3f)
                place (hatO, pos, 0.18f, 0.2f);

            if (hit (s.bass, step))
            {
                int dist = 1;
                while (dist < 16 && ! hit (s.bass, step + dist)) ++dist;
                const int lenSteps = std::max (s.longKick ? 4 : 3, dist);
                place (makeBass (sr, bf, lenSteps * stepN, s.longKick), pos, 0.6f, 0.f);
            }
        }
    }

    // Plegar la cola al inicio para un bucle continuo
    for (int c = 0; c < 2; ++c)
    {
        auto* d = out.getWritePointer (c);
        for (int i = 0; i < tail; ++i) d[i] += d[loopLen + i];
    }
    out.setSize (2, loopLen, true);

    const float peak = out.getMagnitude (0, loopLen);
    if (peak > 0.0001f) out.applyGain (0.89f / peak);
    return out;
}
} // namespace Beat
