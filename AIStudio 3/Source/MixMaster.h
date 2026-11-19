#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>

// Cadena automática de mezcla de voces + mastering.
namespace MixMaster
{
struct Settings
{
    float targetLufs     = -14.0f; // objetivo de loudness (aprox. LUFS)
    float ceilingDb      = -1.0f;  // techo de pico
    float vocalOverBeatDb = 4.0f;  // la voz sobre el beat (RMS)
    float reverbWet      = 0.14f;
    bool  duck           = true;   // baja el beat cuando canta la voz
};

using Coeffs = juce::dsp::IIR::Coefficients<float>;

inline void iir (juce::AudioBuffer<float>& b, Coeffs::Ptr c)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        juce::dsp::IIR::Filter<float> f (c);
        f.reset();
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i) d[i] = f.processSample (d[i]);
    }
}

template <typename P>
inline void runDsp (P& p, juce::AudioBuffer<float>& b)
{
    juce::dsp::AudioBlock<float> blk (b);
    for (size_t s = 0; s < blk.getNumSamples(); s += 4096)
    {
        auto sub = blk.getSubBlock (s, std::min<size_t> (4096, blk.getNumSamples() - s));
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        p.process (ctx);
    }
}

inline void compress (juce::AudioBuffer<float>& b, double sr, float thr, float ratio, float atk, float rel)
{
    juce::dsp::Compressor<float> c;
    c.prepare ({ sr, 4096u, (juce::uint32) b.getNumChannels() });
    c.setThreshold (thr);
    c.setRatio (ratio);
    c.setAttack (atk);
    c.setRelease (rel);
    runDsp (c, b);
}

inline float rmsDb (const juce::AudioBuffer<float>& b, float gate)
{
    double sum = 0.0;
    long cnt = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (std::abs (d[i]) > gate) { sum += (double) d[i] * d[i]; ++cnt; }
    }
    return cnt == 0 ? -100.0f : (float) (10.0 * std::log10 (sum / (double) cnt + 1e-12));
}

// Loudness integrado aproximado (K-weighting + puertas de BS.1770 simplificadas).
inline double measureLufs (const juce::AudioBuffer<float>& in, double sr)
{
    juce::AudioBuffer<float> k;
    k.makeCopyOf (in);
    iir (k, Coeffs::makeHighShelf (sr, 1500.0, 0.707f, 1.585f));
    iir (k, Coeffs::makeHighPass (sr, 38.0, 0.5f));

    const int block = (int) (sr * 0.4), hop = (int) (sr * 0.1);
    if (k.getNumSamples() < block) return -70.0;

    std::vector<double> energies;
    for (int s = 0; s + block <= k.getNumSamples(); s += hop)
    {
        double e = 0.0;
        for (int ch = 0; ch < k.getNumChannels(); ++ch)
        {
            auto* d = k.getReadPointer (ch) + s;
            double ms = 0.0;
            for (int i = 0; i < block; ++i) ms += (double) d[i] * d[i];
            e += ms / block;
        }
        energies.push_back (e);
    }
    auto toLufs = [] (double e) { return -0.691 + 10.0 * std::log10 (e + 1e-12); };

    double sum = 0.0; int n = 0;
    for (double e : energies) if (toLufs (e) > -70.0) { sum += e; ++n; }
    if (n == 0) return -70.0;
    const double relGate = toLufs (sum / n) - 10.0;

    sum = 0.0; n = 0;
    for (double e : energies) if (toLufs (e) > relGate) { sum += e; ++n; }
    return n == 0 ? -70.0 : toLufs (sum / n);
}

inline void deEss (juce::AudioBuffer<float>& v, double sr)
{
    juce::AudioBuffer<float> det;
    det.makeCopyOf (v);
    iir (det, Coeffs::makeHighPass (sr, 6000.0, 0.707f));

    const float thr = 0.05f;
    const float a = std::exp (-1.0f / (float) (sr * 0.001));
    const float r = std::exp (-1.0f / (float) (sr * 0.05));
    float env = 0.f;
    for (int i = 0; i < v.getNumSamples(); ++i)
    {
        float lvl = 0.f;
        for (int ch = 0; ch < det.getNumChannels(); ++ch) lvl = std::max (lvl, std::abs (det.getSample (ch, i)));
        env = lvl > env ? a * env + (1.f - a) * lvl : r * env + (1.f - r) * lvl;
        const float g = env > thr ? std::pow (thr / env, 0.6f) : 1.f;
        for (int ch = 0; ch < v.getNumChannels(); ++ch) v.setSample (ch, i, v.getSample (ch, i) * g);
    }
}

inline void vocalChain (juce::AudioBuffer<float>& v, double sr, float reverbWet)
{
    using juce::Decibels;
    iir (v, Coeffs::makeHighPass (sr, 90.0, 0.707f));
    iir (v, Coeffs::makePeakFilter (sr, 300.0, 1.0f, Decibels::decibelsToGain (-2.5f)));
    iir (v, Coeffs::makePeakFilter (sr, 3500.0, 0.8f, Decibels::decibelsToGain (2.5f)));
    iir (v, Coeffs::makeHighShelf (sr, 11000.0, 0.707f, Decibels::decibelsToGain (2.5f)));
    deEss (v, sr);
    compress (v, sr, -18.0f, 3.0f, 8.0f, 120.0f);

    juce::dsp::Reverb rev;
    juce::Reverb::Parameters p;
    p.roomSize = 0.35f; p.damping = 0.6f; p.wetLevel = reverbWet; p.dryLevel = 1.0f; p.width = 1.0f;
    rev.setParameters (p);
    rev.prepare ({ sr, 4096u, 2u });
    runDsp (rev, v);
}

inline void master (juce::AudioBuffer<float>& b, double sr, const Settings& st)
{
    using juce::Decibels;
    iir (b, Coeffs::makeHighPass (sr, 30.0, 0.707f));
    iir (b, Coeffs::makeLowShelf (sr, 100.0, 0.707f, Decibels::decibelsToGain (1.2f)));
    iir (b, Coeffs::makeHighShelf (sr, 10000.0, 0.707f, Decibels::decibelsToGain (1.5f)));
    compress (b, sr, -14.0f, 2.0f, 30.0f, 200.0f);

    const juce::AudioBuffer<float> pre (b);
    float gainDb = (float) (st.targetLufs - measureLufs (pre, sr));
    const float ceilLin = Decibels::decibelsToGain (st.ceilingDb);

    for (int iter = 0; iter < 3; ++iter)
    {
        b.makeCopyOf (pre);
        b.applyGain (Decibels::decibelsToGain (gainDb));

        juce::dsp::Limiter<float> lim;
        lim.prepare ({ sr, 4096u, 2u });
        lim.setThreshold (st.ceilingDb);
        lim.setRelease (100.0f);
        runDsp (lim, b);

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* d = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i) d[i] = juce::jlimit (-ceilLin, ceilLin, d[i]);
        }
        const double got = measureLufs (b, sr);
        if (std::abs (got - st.targetLufs) < 0.3) break;
        gainDb += (float) (st.targetLufs - got);
    }
}

// beat y voz -> mezcla masterizada (estéreo). La voz puede ser mono o estéreo.
inline juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& beat, const juce::AudioBuffer<float>& vox,
                                     double sr, const Settings& st)
{
    const int len = std::max (beat.getNumSamples(), vox.getNumSamples());
    const int bn = beat.getNumSamples();

    juce::AudioBuffer<float> v (2, len);
    v.clear();
    for (int ch = 0; ch < 2; ++ch)
        v.copyFrom (ch, 0, vox, std::min (ch, vox.getNumChannels() - 1), 0, vox.getNumSamples());

    vocalChain (v, sr, st.reverbWet);

    juce::AudioBuffer<float> b (2, len);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        auto* s = beat.getReadPointer (std::min (ch, beat.getNumChannels() - 1));
        for (int i = 0; i < len; ++i) d[i] = s[i % bn];
    }

    const float vdb = rmsDb (v, 0.01f);
    const float bdb = rmsDb (b, 0.0f);
    v.applyGain (juce::Decibels::decibelsToGain (bdb + st.vocalOverBeatDb - vdb));

    if (st.duck)
    {
        const float a = std::exp (-1.0f / (float) (sr * 0.01));
        const float r = std::exp (-1.0f / (float) (sr * 0.25));
        float env = 0.f;
        for (int i = 0; i < len; ++i)
        {
            const float e = std::max (std::abs (v.getSample (0, i)), std::abs (v.getSample (1, i)));
            env = e > env ? a * env + (1.f - a) * e : r * env + (1.f - r) * e;
            const float g = 1.0f - 0.3f * juce::jlimit (0.f, 1.f, env / 0.15f);
            b.setSample (0, i, b.getSample (0, i) * g);
            b.setSample (1, i, b.getSample (1, i) * g);
        }
    }

    juce::AudioBuffer<float> mix (2, len);
    for (int ch = 0; ch < 2; ++ch)
    {
        mix.copyFrom (ch, 0, b, ch, 0, len);
        mix.addFrom (ch, 0, v, ch, 0, len);
    }
    master (mix, sr, st);
    return mix;
}
} // namespace MixMaster
