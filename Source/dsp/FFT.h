// EQMotion — FFT real (radix-2, precisão dupla), sem dependências externas
#pragma once
#include "Types.h"
#include <complex>
#include <vector>

namespace eqm
{
class RealFFT
{
public:
    using cpx = std::complex<double>;

    explicit RealFFT (int size = 1024) { setSize (size); }

    void setSize (int size)
    {
        N = size; H = N / 2;
        logH = 0; while ((1 << logH) < H) ++logH;
        tw.resize ((size_t) H / 2 + 1);
        for (int i = 0; i <= H / 2; ++i) tw[(size_t) i] = std::polar (1.0, -kTwoPi * i / H);
        rtw.resize ((size_t) H + 1);
        for (int k = 0; k <= H; ++k) rtw[(size_t) k] = std::polar (1.0, -kTwoPi * k / N);
        rev.resize ((size_t) H);
        for (int i = 0; i < H; ++i)
        {
            int r = 0;
            for (int b = 0; b < logH; ++b) if (i & (1 << b)) r |= 1 << (logH - 1 - b);
            rev[(size_t) i] = r;
        }
        work.assign ((size_t) H, cpx());
    }
    int size() const { return N; }

    // in: N amostras reais → out: N/2+1 bins
    void forward (const double* in, cpx* out)
    {
        for (int i = 0; i < H; ++i) work[(size_t) rev[(size_t) i]] = cpx (in[2 * i], in[2 * i + 1]);
        transform (false);
        const cpx z0 = work[0];
        out[0] = cpx (z0.real() + z0.imag(), 0.0);
        out[H] = cpx (z0.real() - z0.imag(), 0.0);
        for (int k = 1; k < H; ++k)
        {
            const cpx a = work[(size_t) k], b = std::conj (work[(size_t) (H - k)]);
            const cpx e = 0.5 * (a + b), o = cpx (0, -0.5) * (a - b);
            out[k] = e + rtw[(size_t) k] * o;
        }
    }

    // in: N/2+1 bins → out: N amostras reais (escalado por 1/N)
    void inverse (const cpx* in, double* out)
    {
        for (int k = 0; k < H; ++k)
        {
            const cpx a = in[k], b = std::conj (in[H - k]);
            const cpx e = 0.5 * (a + b), o = 0.5 * (a - b) * std::conj (rtw[(size_t) k]);
            work[(size_t) rev[(size_t) k]] = e + cpx (0, 1) * o;
        }
        transform (true);
        const double s = 1.0 / H;
        for (int i = 0; i < H; ++i) { out[2 * i] = work[(size_t) i].real() * s; out[2 * i + 1] = work[(size_t) i].imag() * s; }
    }

private:
    void transform (bool inv)
    {
        for (int len = 2; len <= H; len <<= 1)
        {
            const int half = len >> 1, step = H / len;
            for (int i = 0; i < H; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const int ti = j * step;
                    cpx w = ti <= H / 2 ? tw[(size_t) ti] : -std::conj (tw[(size_t) (H / 2 - (ti - H / 2))]);
                    if (inv) w = std::conj (w);
                    const cpx u = work[(size_t) (i + j)], v = work[(size_t) (i + j + half)] * w;
                    work[(size_t) (i + j)] = u + v;
                    work[(size_t) (i + j + half)] = u - v;
                }
        }
    }

    int N = 0, H = 0, logH = 0;
    std::vector<cpx> tw, rtw, work;
    std::vector<int> rev;
};
} // namespace eqm
