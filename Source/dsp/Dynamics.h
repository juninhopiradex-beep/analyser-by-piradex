// =============================================================================
//  EQMotion — dinâmica por banda
//
//  detetor (filtro focado na banda, sinal da pista ou sidechain)
//    → envolvente (Peak ou RMS, ataque / hold / release)
//    → lei de transferência (limiar, ratio, joelho, acima/abaixo)
//    → desvio de ganho limitado pelo "range" (com sinal)
//  O desvio é somado ao ganho estático da banda pelos motores (IIR e fase linear),
//  por isso a parte dinâmica pode ser desenhada à parte da curva estática.
// =============================================================================
#pragma once
#include "Biquad.h"
#include <atomic>

namespace eqm
{
// Lei de ganho (pura, testável): devolve o desvio em dB
inline double dynamicGainDb (double levelDb, double thresholdDb, double ratio, double kneeDb,
                             double rangeDb, DynDirection dir)
{
    const double over = dir == DynDirection::Above ? levelDb - thresholdDb : thresholdDb - levelDb;
    const double slope = 1.0 - 1.0 / std::max (1.0, ratio);
    const double W = std::max (0.0, kneeDb);
    double amount;
    if (2.0 * over <= -W)      amount = 0.0;
    else if (2.0 * over >= W)  amount = over * slope;
    else                       amount = slope * (over + 0.5 * W) * (over + 0.5 * W) / (2.0 * W);
    amount = std::min (amount, std::abs (rangeDb));
    return rangeDb < 0 ? -amount : amount;
}

class BandDetector
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { fl.reset(); fr.reset(); env = 0; holdCount = 0; }

    // Recalcula o filtro de foco quando a banda muda (barato; feito por sub-bloco só se mudou)
    void configure (const BandParams& b)
    {
        if (b.type == cfgType && b.freq == cfgFreq && b.q == cfgQ && b.attackMs == cfgA
            && b.releaseMs == cfgR && b.holdMs == cfgH && fs == cfgFs)
            return;
        cfgType = b.type; cfgFreq = b.freq; cfgQ = b.q; cfgA = b.attackMs; cfgR = b.releaseMs; cfgH = b.holdMs; cfgFs = fs;

        AnalogSection a;
        switch (b.type)
        {
            case FilterType::LowShelf:  a.d2 = 1; a.d1 = 1.4142; a.d0 = 1; a.n0 = 1; a.n1 = 0; a.n2 = 0; break; // passa-baixo
            case FilterType::HighShelf: a.d2 = 1; a.d1 = 1.4142; a.d0 = 1; a.n2 = 1; a.n1 = 0; a.n0 = 0; break; // passa-alto
            case FilterType::Tilt:      a = AnalogSection(); break;                                          // sinal inteiro
            default:
            {
                const double q = clampv (b.q, 0.5, 12.0);
                a.n2 = 0; a.n1 = 1.0 / q; a.n0 = 0; a.d2 = 1; a.d1 = 1.0 / q; a.d0 = 1;  // passa-banda 0 dB
                break;
            }
        }
        const auto c = (b.type == FilterType::Tilt) ? BiquadCoeffs() : bilinear (a, b.freq, fs);
        fl.c = c; fr.c = c;
        attCoeff = std::exp (-1.0 / (std::max (0.01, b.attackMs) * 0.001 * fs));
        relCoeff = std::exp (-1.0 / (std::max (1.0, b.releaseMs) * 0.001 * fs));
        holdSamples = (int) (b.holdMs * 0.001 * fs);
    }

    // l, r: sinal do detetor (pista ou sidechain), n amostras
    void process (const double* l, const double* r, int n, Placement place, DetectorMode mode) noexcept
    {
        const bool rms = mode == DetectorMode::RMS;
        for (int i = 0; i < n; ++i)
        {
            double v;
            switch (place)
            {
                case Placement::Left:  v = std::abs (fl.process (l[i])); break;
                case Placement::Right: v = std::abs (fr.process (r[i])); break;
                case Placement::Mid:   v = std::abs (fl.process (0.5 * (l[i] + r[i]))); break;
                case Placement::Side:  v = std::abs (fl.process (0.5 * (l[i] - r[i]))); break;
                default:  // estéreo ligado: o mesmo ganho nos dois canais (sem a imagem "a passear")
                {
                    const double a = fl.process (l[i]), b = fr.process (r[i]);
                    v = rms ? std::sqrt (0.5 * (a * a + b * b)) : std::max (std::abs (a), std::abs (b));
                    break;
                }
            }
            if (rms) v = v * v;
            if (v > env) { env = v + (env - v) * attCoeff; holdCount = holdSamples; }
            else if (holdCount > 0) --holdCount;
            else env = v + (env - v) * relCoeff;
        }
        fl.sanitize(); fr.sanitize();
        if (! std::isfinite (env)) env = 0;
    }

    double levelDb (DetectorMode mode) const
    {
        const double lin = mode == DetectorMode::RMS ? std::sqrt (std::max (0.0, env)) : env;
        return gainToDb (lin, -150.0);
    }

private:
    double fs = 48000;
    Biquad fl, fr;
    double env = 0, attCoeff = 0, relCoeff = 0;
    int holdSamples = 0, holdCount = 0;
    FilterType cfgType = FilterType::NumTypes;
    double cfgFreq = -1, cfgQ = -1, cfgA = -1, cfgR = -1, cfgH = -1, cfgFs = -1;
};

// -----------------------------------------------------------------------------
//  De-harsh: corta de forma larga a zona 2–8 kHz só quando ela sobe acima do
//  equilíbrio de longo prazo do próprio sinal (limiar contextual, aprendido em contínuo).
// -----------------------------------------------------------------------------
class DeHarshDetector
{
public:
    static constexpr double kFreq = 4200.0, kQ = 0.75;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        AnalogSection bp; bp.n2 = 0; bp.n1 = 1.0 / 0.9; bp.n0 = 0; bp.d2 = 1; bp.d1 = 1.0 / 0.9; bp.d0 = 1;
        harshL.c = harshR.c = bilinear (bp, 4500.0, fs);
        AnalogSection hp; hp.n2 = 1; hp.n1 = 0; hp.n0 = 0; hp.d2 = 1; hp.d1 = 1.4142; hp.d0 = 1;
        broadL.c = broadR.c = bilinear (hp, 60.0, fs);
        slowCoeff = std::exp (-1.0 / (2.5 * fs));
        reset();
    }
    void reset() { harshL.reset(); harshR.reset(); broadL.reset(); broadR.reset(); eH = eB = 0; slowRatio = 0; primed = false; deltaDb = 0; }
    void setTiming (double attackMs, double releaseMs)
    {
        att = std::exp (-1.0 / (std::max (0.1, attackMs) * 0.001 * fs));
        rel = std::exp (-1.0 / (std::max (1.0, releaseMs) * 0.001 * fs));
    }

    void process (const double* l, const double* r, int n, double amount) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double h1 = harshL.process (l[i]), h2 = harshR.process (r[i]);
            const double b1 = broadL.process (l[i]), b2 = broadR.process (r[i]);
            const double vh = 0.5 * (h1 * h1 + h2 * h2), vb = 0.5 * (b1 * b1 + b2 * b2);
            eH = vh + (eH - vh) * (vh > eH ? att : rel);
            eB = vb + (eB - vb) * (vb > eB ? att : rel);
        }
        harshL.sanitize(); harshR.sanitize(); broadL.sanitize(); broadR.sanitize();

        const double hDb = 10.0 * std::log10 (eH + 1e-20), bDb = 10.0 * std::log10 (eB + 1e-20);
        const double ratio = hDb - bDb;
        if (bDb > -70.0)
        {
            const double k = std::pow (slowCoeff, (double) n);
            if (! primed) { slowRatio = ratio; primed = true; }
            // a referência sobe devagar e só desce depressa o suficiente para seguir mudanças de secção
            slowRatio = ratio + (slowRatio - ratio) * k;
            const double harsh = ratio - slowRatio - 1.0;   // margem de 1 dB
            deltaDb = -clampv (harsh * 1.4, 0.0, 10.0) * clampv (amount, 0.0, 1.0);
        }
        else deltaDb = deltaDb * 0.9;
    }

    double currentDeltaDb() const { return deltaDb; }

private:
    double fs = 48000, att = 0.9, rel = 0.99, slowCoeff = 0.999;
    Biquad harshL, harshR, broadL, broadR;
    double eH = 0, eB = 0, slowRatio = 0, deltaDb = 0;
    bool primed = false;
};

// -----------------------------------------------------------------------------
//  Banco de dinâmica partilhado pelos dois motores
// -----------------------------------------------------------------------------
class DynamicsBank
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& d : det) d.prepare (fs);
        deharsh.prepare (fs);
        deltas.fill (0.0);
    }
    void reset() { for (auto& d : det) d.reset(); deharsh.reset(); deltas.fill (0.0); }

    // Atualiza os detetores com um sub-bloco e calcula os desvios dinâmicos de cada banda
    void process (const BandArray& bands, const double* inL, const double* inR,
                  const double* scL, const double* scR, bool scAvailable, int n,
                  bool deharshOn, double deharshAmount) noexcept
    {
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& p = bands[(size_t) b];
            if (! (p.isProcessing() && p.isDynamic())) { deltas[(size_t) b] = 0.0; continue; }
            auto& d = det[(size_t) b];
            d.configure (p);
            const bool useSc = p.source == DetectorSource::Sidechain && scAvailable;
            d.process (useSc ? scL : inL, useSc ? scR : inR, n, p.placement, p.detMode);
            deltas[(size_t) b] = dynamicGainDb (d.levelDb (p.detMode), p.thresholdDb, p.ratio, p.kneeDb,
                                                p.rangeDb, p.direction);
            levels[(size_t) b] = d.levelDb (p.detMode);
        }
        if (deharshOn) deharsh.process (inL, inR, n, deharshAmount);
        deharshDelta = deharshOn ? deharsh.currentDeltaDb() : 0.0;
    }

    double delta (int band) const { return deltas[(size_t) band]; }
    double level (int band) const { return levels[(size_t) band]; }
    double deharshDeltaDb() const { return deharshDelta; }
    void setDeharshTiming (double a, double r) { deharsh.setTiming (a, r); }

private:
    double fs = 48000;
    std::array<BandDetector, kMaxBands> det;
    std::array<double, kMaxBands> deltas {}, levels {};
    DeHarshDetector deharsh;
    double deharshDelta = 0;
};

inline BandParams deharshBand (double deltaDb)
{
    BandParams b;
    b.active = true; b.type = FilterType::Bell; b.freq = DeHarshDetector::kFreq; b.q = DeHarshDetector::kQ;
    b.gainDb = deltaDb;
    return b;
}
} // namespace eqm
