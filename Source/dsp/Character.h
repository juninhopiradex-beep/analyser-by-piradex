// =============================================================================
//  EQMotion — Character (Clean / Subtle / Warm), Auto gain e filtro de Listen
// =============================================================================
#pragma once
#include "Biquad.h"

namespace eqm
{
// -----------------------------------------------------------------------------
//  Character: y = x + h(x). O sinal principal passa intacto; só a pequena parte
//  harmónica h(x) é gerada, com anti-aliasing por antiderivada (ADAA de 1.ª ordem),
//  sem latência. Clean não toca no sinal (bit a bit).
// -----------------------------------------------------------------------------
enum class Character : int { Clean = 0, Subtle, Warm };

class CharacterStage
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dcCoeff = std::exp (-kTwoPi * 5.0 / fs);
        BandParams lo; lo.active = true; lo.type = FilterType::LowShelf;  lo.freq = 70.0;    lo.q = 0.6;
        BandParams hi; hi.active = true; hi.type = FilterType::HighShelf; hi.freq = 15000.0; hi.q = 0.6;
        const auto dl = designDigital (lo, 0.5, fs), dh = designDigital (hi, -0.7, fs);
        for (int c = 0; c < 2; ++c) { warmLo[c].c = dl.c[0]; warmHi[c].c = dh.c[0]; }
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { x1[c] = 0; dcx[c] = dcy[c] = 0; warmLo[c].reset(); warmHi[c].reset(); }
        mix.reset (0.0);
    }

    void process (double* L, double* R, int n, Character ch) noexcept
    {
        const int mode = (int) ch;
        if (mode != activeMode)
        {
            // muda de carácter passando por "limpo" para não haver salto
            mix.target = 0.0;
            if (mix.value < 1e-4) { activeMode = mode; for (int c = 0; c < 2; ++c) { x1[c] = 0; warmLo[c].reset(); warmHi[c].reset(); } }
        }
        else mix.target = mode == 0 ? 0.0 : 1.0;
        mix.coeff = std::exp (-1.0 / (0.01 * fs));

        if (activeMode == 0 && mix.value < 1e-6 && mix.target == 0.0) { mix.value = 0; return; }   // Clean: intacto

        const Shape s = shape (activeMode);
        double* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const double w = mix.next();
            for (int c = 0; c < 2; ++c)
            {
                double x = io[c][i];
                double base = x;
                if (activeMode == 2) base = warmHi[c].process (warmLo[c].process (x));   // ligeira cor tonal
                const double hx = adaa (base, x1[c], s);
                x1[c] = base;
                // bloqueio de DC só na parte harmónica (as harmónicas pares geram DC)
                const double hp = hx - dcx[c] + dcCoeff * dcy[c];
                dcx[c] = hx; dcy[c] = hp;
                io[c][i] = x + w * ((base - x) + hp);
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            warmLo[c].sanitize(); warmHi[c].sanitize();
            if (! std::isfinite (dcy[c])) { dcx[c] = dcy[c] = 0; x1[c] = 0; }
        }
    }

    struct Shape { double g, kOdd, g2, kEven; };
    static Shape shape (int mode)
    {
        return mode == 2 ? Shape { 0.9, 0.19, 1.2, 0.07 } : Shape { 0.6, 0.10, 1.0, 0.025 };
    }

    // parte harmónica h(x) e a sua antiderivada H(x)
    static double h (double x, const Shape& s)
    {
        return s.kOdd * (std::tanh (s.g * x) / s.g - x) + s.kEven * (1.0 - 1.0 / std::cosh (s.g2 * x));
    }
    static double logCosh (double a) { a = std::abs (a); return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531; }
    static double H (double x, const Shape& s)
    {
        return s.kOdd * (logCosh (s.g * x) / (s.g * s.g) - 0.5 * x * x)
             + s.kEven * (x - (2.0 / s.g2) * std::atan (std::tanh (0.5 * s.g2 * x)));
    }
    static double adaa (double x, double xPrev, const Shape& s)
    {
        const double d = x - xPrev;
        if (std::abs (d) < 1e-5) return h (0.5 * (x + xPrev), s);
        return (H (x, s) - H (xPrev, s)) / d;
    }

private:
    double fs = 48000, dcCoeff = 0.999;
    double x1[2] {}, dcx[2] {}, dcy[2] {};
    Biquad warmLo[2], warmHi[2];
    Smoothed mix;
    int activeMode = 0;
};

// -----------------------------------------------------------------------------
//  Auto gain: compara entrada e saída com ponderação K (BS.1770) numa janela
//  longa (~1,5 s) e compensa devagar — é uma ajuda de comparação, não um
//  maximizador. Congela em silêncio. Limite ±18 dB.
// -----------------------------------------------------------------------------
class LoudnessMatch
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        BandParams shelf; shelf.active = true; shelf.type = FilterType::HighShelf; shelf.freq = 1681.97; shelf.q = 0.7072;
        BandParams hp;    hp.active = true;    hp.type = FilterType::LowCut;      hp.freq = 38.135;   hp.q = 0.5003; hp.slope = Slope::dB12;
        const auto ds = designDigital (shelf, 4.0, fs), dh = designDigital (hp, 0.0, fs);
        for (auto& k : kw) { k[0].c = ds.c[0]; k[1].c = dh.c[0]; }
        avgCoeff = std::exp (-1.0 / (1.5 * fs));
        gainSmooth.setTime (0.35, fs);
        reset();
    }
    void reset()
    {
        for (auto& k : kw) for (auto& b : k) b.reset();
        eIn = eOut = 0; gainSmooth.reset (1.0);
    }

    // in*: referência alinhada (seca), out*: saída processada (modificada no local se enabled)
    void process (const double* inL, const double* inR, double* outL, double* outR, int n, bool enabled) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double a = kw[0][1].process (kw[0][0].process (inL[i]));
            const double b = kw[1][1].process (kw[1][0].process (inR[i]));
            const double c = kw[2][1].process (kw[2][0].process (outL[i]));
            const double d = kw[3][1].process (kw[3][0].process (outR[i]));
            eIn  = (a * a + b * b) + (eIn - (a * a + b * b)) * avgCoeff;
            eOut = (c * c + d * d) + (eOut - (c * c + d * d)) * avgCoeff;
        }
        for (auto& k : kw) for (auto& bq : k) bq.sanitize();
        if (eIn > 2e-7 && eOut > 1e-12)
            gainSmooth.target = clampv (std::sqrt (eIn / eOut), dbToGain (-18.0), dbToGain (18.0));
        if (! enabled) gainSmooth.target = 1.0;
        gainSmooth.setTime (enabled ? 0.35 : 0.03, fs);   // desligar volta depressa a 0 dB
        if (gainSmooth.value == 1.0 && gainSmooth.target == 1.0) return;   // desligado e assente: intacto
        for (int i = 0; i < n; ++i)
        {
            const double g = gainSmooth.next();
            outL[i] *= g; outR[i] *= g;
        }
        if (std::abs (gainSmooth.value - gainSmooth.target) < 2e-5) gainSmooth.value = gainSmooth.target;
    }
    double currentGainDb() const { return gainToDb (gainSmooth.value); }

private:
    double fs = 48000, avgCoeff = 0.9999, eIn = 0, eOut = 0;
    std::array<std::array<Biquad, 2>, 4> kw;
    Smoothed gainSmooth;
};

// Filtro de "Listen" de uma banda: a zona que a banda trabalha
inline DigitalBand listenFilter (const BandParams& b, double fs)
{
    BandParams l = b;
    l.active = true; l.dynamic = false;
    switch (b.type)
    {
        case FilterType::LowShelf: case FilterType::HighCut:
            l.type = FilterType::LowCut;  l.slope = Slope::dB12; l.q = 0.7071;   // para HighCut: ouve o que é retirado (agudos)
            if (b.type == FilterType::LowShelf) l.type = FilterType::HighCut;    // LowShelf: ouve os graves
            break;
        case FilterType::HighShelf: case FilterType::LowCut:
            l.type = FilterType::HighCut; l.slope = Slope::dB12; l.q = 0.7071;   // para LowCut: ouve os graves retirados
            if (b.type == FilterType::HighShelf) l.type = FilterType::LowCut;    // HighShelf: ouve os agudos
            break;
        case FilterType::Tilt:
            l.type = FilterType::AllPass; l.q = 0.5; break;
        default:
            l.type = FilterType::BandPass; l.q = clampv (b.q, 0.7, 20.0); break;
    }
    return designDigital (l, 0.0, fs);
}
} // namespace eqm
