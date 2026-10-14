// EQMotion — cálculo das curvas mostradas (o que se vê é o que o motor faz)
#pragma once
#include "dsp/FilterDesign.h"
#include "dsp/Dynamics.h"
#include <vector>

namespace curve
{
using namespace eqm;

// Taxa a que o motor IIR corre no modo Natural
inline int naturalOversampling (double fs) { return fs <= 50000.0 ? 4 : fs <= 100000.0 ? 2 : 1; }

// Resposta (dB) de uma banda nas frequências pedidas, segundo o modo de fase
inline void bandResponse (const BandParams& b, double gainDb, PhaseMode mode, double fs,
                          const std::vector<double>& freqs, std::vector<float>& out)
{
    out.resize (freqs.size());
    if (! b.active) { std::fill (out.begin(), out.end(), 0.0f); return; }
    if (mode == PhaseMode::Linear)
    {
        const auto sl = designAnalog (b, gainDb);
        const double f0 = clampv (b.freq, kMinFreq, kMaxFreq);
        for (size_t i = 0; i < freqs.size(); ++i)
            out[i] = (float) clampv (analogMagnitudeDb (sl, std::min (freqs[i], 0.5 * fs), f0), -200.0, 80.0);
        return;
    }
    const double rate = mode == PhaseMode::Natural ? fs * naturalOversampling (fs) : fs;
    const auto d = designDigital (b, gainDb, rate);
    for (size_t i = 0; i < freqs.size(); ++i)
        out[i] = (float) clampv (digitalMagnitudeDb (d, std::min (freqs[i], 0.4999 * fs), rate), -200.0, 80.0);
}

// Frequências logarítmicas para o ecrã
inline std::vector<double> logFreqs (int n, double lo, double hi)
{
    std::vector<double> f ((size_t) n);
    for (int i = 0; i < n; ++i) f[(size_t) i] = lo * std::pow (hi / lo, (double) i / (n - 1));
    return f;
}

// Curva total de um domínio (colocações incluídas: estéreo + as do domínio)
inline void totalCurve (const BandArray& bands, const std::array<double, kMaxBands>& gains, PhaseMode mode, double fs,
                        const std::vector<double>& freqs, Placement domain, std::vector<float>& out, std::vector<float>& tmp)
{
    out.assign (freqs.size(), 0.0f);
    for (int b = 0; b < kMaxBands; ++b)
    {
        const auto& p = bands[(size_t) b];
        if (! p.isProcessing()) continue;
        const bool include = p.placement == Placement::Stereo ? (domain == Placement::Stereo || domain == Placement::Left || domain == Placement::Right)
                                                              : p.placement == domain;
        if (! include) continue;
        bandResponse (p, gains[(size_t) b], mode, fs, freqs, tmp);
        for (size_t i = 0; i < out.size(); ++i) out[i] += tmp[i];
    }
}
} // namespace curve
