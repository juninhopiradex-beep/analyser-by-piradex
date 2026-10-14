// =============================================================================
//  EQMotion — desenho dos filtros
//
//  Cada banda é descrita por protótipos analógicos de 1.ª/2.ª ordem
//  normalizados em s/ω0. A partir deles:
//    • Zero Latency / Natural → transformada bilinear com pre-warp em f0
//      (no modo Natural o motor corre a ≥176,4 kHz, por isso a resposta
//       coincide com a analógica até 20 kHz, sem "cramping" perto de Nyquist);
//    • Linear Phase → magnitude analógica exata, convertida num FIR simétrico.
//  As mesmas funções servem o gráfico, para o que se vê ser o que se ouve.
// =============================================================================
#pragma once
#include "Types.h"
#include <complex>

namespace eqm
{
// Secção analógica normalizada: H(s) = (n2 s² + n1 s + n0) / (d2 s² + d1 s + d0), s = jω/ω0
struct AnalogSection
{
    double n2 = 0, n1 = 0, n0 = 1, d2 = 0, d1 = 0, d0 = 1;
    bool firstOrder() const { return n2 == 0.0 && d2 == 0.0; }

    std::complex<double> eval (double wNorm) const
    {
        const std::complex<double> s (0.0, wNorm);
        return (n2 * s * s + n1 * s + n0) / (d2 * s * s + d1 * s + d0);
    }
};

struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    std::complex<double> eval (double w) const   // w em rad/amostra
    {
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

constexpr int kMaxSections = 9;   // 16.ª ordem = 8 biquads (+1 de 1.ª ordem de reserva)

struct SectionList
{
    std::array<AnalogSection, kMaxSections> s;
    int count = 0;
    void add (const AnalogSection& a) { if (count < kMaxSections) s[(size_t) count++] = a; }
};

// -----------------------------------------------------------------------------
//  Protótipo Chebyshev tipo II de 16.ª ordem (100 dB de rejeição), normalizado
//  para −3 dB em ω = 1. Usado pelo corte "Brickwall".
// -----------------------------------------------------------------------------
struct BrickwallPrototype
{
    struct Pair { double wp, qp, wz; };
    std::array<Pair, 8> pairs {};

    BrickwallPrototype()
    {
        constexpr int N = 16;
        const double As = 100.0;
        const double delta = 1.0 / std::sqrt (std::pow (10.0, As / 10.0) - 1.0);
        const double mu = std::asinh (1.0 / delta) / N;
        const double w3 = 1.0 / std::cosh (std::acosh (1.0 / delta) / N);   // −3 dB com borda de rejeição em 1
        for (int k = 1; k <= N / 2; ++k)
        {
            const double th = (2.0 * k - 1.0) * kPi / (2.0 * N);
            const std::complex<double> pc (-std::sinh (mu) * std::sin (th), std::cosh (mu) * std::cos (th));
            const std::complex<double> p = 1.0 / pc;
            const double wp = std::abs (p);
            const double qp = wp / (-2.0 * p.real());
            const double wz = 1.0 / std::cos (th);
            pairs[(size_t) k - 1] = { wp / w3, qp, wz / w3 };
        }
    }

    static const BrickwallPrototype& get() { static const BrickwallPrototype p; return p; }
};

// -----------------------------------------------------------------------------
//  Secções analógicas de uma banda (com o ganho efetivo já decidido)
// -----------------------------------------------------------------------------
inline void butterworthCut (SectionList& out, int order, bool highpass, double userQ)
{
    // Q das secções de Butterworth; o Q do utilizador ajusta a ressonância
    // da secção menos ressonante (num corte de 12 dB/oct é exatamente o Q pedido).
    const double qScale = userQ / 0.70710678118654752;
    const int pairs = order / 2;
    int lowestIdx = -1;
    double lowestQ = 1e9;
    std::array<double, 8> qs {};
    for (int k = 1; k <= pairs; ++k)
    {
        const double psi = (order % 2 == 0) ? (2.0 * k - 1.0) * kPi / (2.0 * order) : k * kPi / order;
        qs[(size_t) k - 1] = 1.0 / (2.0 * std::cos (psi));
        if (qs[(size_t) k - 1] < lowestQ) { lowestQ = qs[(size_t) k - 1]; lowestIdx = k - 1; }
    }
    if (lowestIdx >= 0) qs[(size_t) lowestIdx] = clampv (qs[(size_t) lowestIdx] * qScale, 0.1, 30.0);

    if (order % 2 == 1)
    {
        AnalogSection a;
        if (highpass) { a.n1 = 1; a.n0 = 0; } else { a.n1 = 0; a.n0 = 1; }
        a.d1 = 1; a.d0 = 1;
        out.add (a);
    }
    for (int k = 0; k < pairs; ++k)
    {
        AnalogSection a;
        a.d2 = 1; a.d1 = 1.0 / qs[(size_t) k]; a.d0 = 1;
        if (highpass) { a.n2 = 1; a.n1 = 0; a.n0 = 0; } else { a.n2 = 0; a.n1 = 0; a.n0 = 1; }
        out.add (a);
    }
}

inline void brickwallCut (SectionList& out, bool highpass)
{
    for (auto& p : BrickwallPrototype::get().pairs)
    {
        const double norm = (p.wp * p.wp) / (p.wz * p.wz);   // ganho unitário na banda passante
        AnalogSection a;
        if (! highpass)
        {
            a.n2 = norm; a.n1 = 0; a.n0 = norm * p.wz * p.wz;
            a.d2 = 1;    a.d1 = p.wp / p.qp; a.d0 = p.wp * p.wp;
        }
        else   // s → 1/s
        {
            a.n2 = norm * p.wz * p.wz; a.n1 = 0; a.n0 = norm;
            a.d2 = p.wp * p.wp;        a.d1 = p.wp / p.qp; a.d0 = 1;
        }
        out.add (a);
    }
}

inline SectionList designAnalog (const BandParams& b, double effectiveGainDb)
{
    SectionList out;
    const double Q = clampv (b.q, kMinQ, kMaxQ);
    const double A = std::pow (10.0, effectiveGainDb / 40.0);
    AnalogSection a;
    switch (b.type)
    {
        case FilterType::Bell:
            a.n2 = 1; a.n1 = A / Q; a.n0 = 1;
            a.d2 = 1; a.d1 = 1.0 / (A * Q); a.d0 = 1;
            out.add (a); break;
        case FilterType::LowShelf:
        {
            const double sa = std::sqrt (A);
            a.n2 = A;     a.n1 = A * sa / Q; a.n0 = A * A;
            a.d2 = A;     a.d1 = sa / Q;     a.d0 = 1;
            out.add (a); break;
        }
        case FilterType::HighShelf:
        {
            const double sa = std::sqrt (A);
            a.n2 = A * A; a.n1 = A * sa / Q; a.n0 = A;
            a.d2 = 1;     a.d1 = sa / Q;     a.d0 = A;
            out.add (a); break;
        }
        case FilterType::Notch:
            a.n2 = 1; a.n1 = 0; a.n0 = 1; a.d2 = 1; a.d1 = 1.0 / Q; a.d0 = 1;
            out.add (a); break;
        case FilterType::BandPass:
            a.n2 = 0; a.n1 = 1.0 / Q; a.n0 = 0; a.d2 = 1; a.d1 = 1.0 / Q; a.d0 = 1;
            out.add (a); break;
        case FilterType::AllPass:
            a.n2 = 1; a.n1 = -1.0 / Q; a.n0 = 1; a.d2 = 1; a.d1 = 1.0 / Q; a.d0 = 1;
            out.add (a); break;
        case FilterType::Tilt:   // 1.ª ordem: −g/2 em graves, +g/2 em agudos
            a.n1 = A; a.n0 = 1; a.d1 = 1; a.d0 = A;
            out.add (a); break;
        case FilterType::LowCut:
        case FilterType::HighCut:
        {
            const bool hp = b.type == FilterType::LowCut;
            if (b.slope == Slope::Brickwall) brickwallCut (out, hp);
            else butterworthCut (out, slopeOrder (b.slope), hp, Q);
            break;
        }
        default: break;
    }
    return out;
}

// Transformada bilinear com pre-warp em f0 (que é limitado abaixo de Nyquist)
inline BiquadCoeffs bilinear (const AnalogSection& a, double f0, double fs)
{
    const double fw = clampv (f0, 1.0, 0.49 * fs);
    const double c = 1.0 / std::tan (kPi * fw / fs);   // (s/ω0) = c·(1−z⁻¹)/(1+z⁻¹)
    BiquadCoeffs r;
    if (a.firstOrder())
    {
        const double B0 = a.n1 * c + a.n0, B1 = a.n0 - a.n1 * c;
        const double A0 = a.d1 * c + a.d0, A1 = a.d0 - a.d1 * c;
        r.b0 = B0 / A0; r.b1 = B1 / A0; r.b2 = 0; r.a1 = A1 / A0; r.a2 = 0;
        return r;
    }
    const double cc = c * c;
    const double B0 = a.n2 * cc + a.n1 * c + a.n0, B1 = 2.0 * (a.n0 - a.n2 * cc), B2 = a.n2 * cc - a.n1 * c + a.n0;
    const double A0 = a.d2 * cc + a.d1 * c + a.d0, A1 = 2.0 * (a.d0 - a.d2 * cc), A2 = a.d2 * cc - a.d1 * c + a.d0;
    r.b0 = B0 / A0; r.b1 = B1 / A0; r.b2 = B2 / A0; r.a1 = A1 / A0; r.a2 = A2 / A0;
    return r;
}

struct DigitalBand
{
    std::array<BiquadCoeffs, kMaxSections> c;
    int count = 0;
};

inline DigitalBand designDigital (const BandParams& b, double effectiveGainDb, double fs)
{
    const auto an = designAnalog (b, effectiveGainDb);
    DigitalBand d;
    d.count = an.count;
    for (int i = 0; i < an.count; ++i)
        d.c[(size_t) i] = bilinear (an.s[(size_t) i], b.freq, fs);
    return d;
}

// Magnitudes (dB) para o gráfico, o modo de fase linear e as Smart Ops
inline double analogMagnitudeDb (const SectionList& sl, double f, double f0)
{
    const double w = f / f0;
    double mag2 = 1.0;
    for (int i = 0; i < sl.count; ++i) mag2 *= std::norm (sl.s[(size_t) i].eval (w));
    return 10.0 * std::log10 (std::max (mag2, 1e-30));
}

inline double digitalMagnitudeDb (const DigitalBand& d, double f, double fs)
{
    if (f >= 0.5 * fs) f = 0.4999 * fs;
    const double w = kTwoPi * f / fs;
    double mag2 = 1.0;
    for (int i = 0; i < d.count; ++i) mag2 *= std::norm (d.c[(size_t) i].eval (w));
    return 10.0 * std::log10 (std::max (mag2, 1e-30));
}

// Ganho efetivo: escala global × ganho estático + desvio dinâmico
inline double effectiveGain (const BandParams& b, double gainScale, double dynamicDeltaDb)
{
    if (! typeHasGain (b.type)) return 0.0;
    return clampv (b.gainDb * gainScale + dynamicDeltaDb, -kMaxGainDb - 24.0, kMaxGainDb + 24.0);
}
} // namespace eqm
