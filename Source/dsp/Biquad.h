// EQMotion — biquad em forma direta transposta II, precisão dupla
#pragma once
#include "FilterDesign.h"
#include <vector>

namespace eqm
{
struct Biquad
{
    BiquadCoeffs c;
    double s1 = 0, s2 = 0;

    inline double process (double x) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() noexcept { s1 = s2 = 0; }
    // proteção contra NaN/Inf e denormais (chamar uma vez por bloco)
    void sanitize() noexcept
    {
        if (! std::isfinite (s1) || ! std::isfinite (s2)) { s1 = s2 = 0; return; }
        if (std::abs (s1) < 1e-30) s1 = 0;
        if (std::abs (s2) < 1e-30) s2 = 0;
    }
};

// Linha de atraso inteira (memória alocada em prepare, nunca na thread de áudio)
struct DelayLine
{
    std::vector<double> buf;
    int mask = 0, pos = 0, delay = 0;

    void prepare (int maxDelay)
    {
        int size = 1;
        while (size < maxDelay + 2) size <<= 1;
        buf.assign ((size_t) size, 0.0);
        mask = size - 1;
        pos = 0;
    }
    void setDelay (int d) { delay = clampv (d, 0, mask - 1); }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); pos = 0; }
    inline double process (double x) noexcept
    {
        if (delay == 0) return x;
        buf[(size_t) pos] = x;
        const double y = buf[(size_t) ((pos - delay) & mask)];
        pos = (pos + 1) & mask;
        return y;
    }
};

// Suavização de um parâmetro (1 polo)
struct Smoothed
{
    double value = 0, target = 0, coeff = 0;
    void setTime (double seconds, double rate) { coeff = seconds <= 0 ? 0.0 : std::exp (-1.0 / (seconds * rate)); }
    void reset (double v) { value = target = v; }
    inline double next() noexcept { value = target + (value - target) * coeff; return value; }
    bool settled (double eps) const { return std::abs (value - target) < eps; }
};

// Rampa linear (chega exatamente ao alvo: bypass, Delta, Listen sem resíduos)
struct Ramp
{
    double value = 0, target = 0, step = 0.001;
    void setTime (double seconds, double rate) { step = 1.0 / std::max (1.0, seconds * rate); }
    void reset (double v) { value = target = v; }
    inline double next() noexcept
    {
        if (value < target) value = std::min (target, value + step);
        else if (value > target) value = std::max (target, value - step);
        return value;
    }
};
} // namespace eqm
