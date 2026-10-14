// =============================================================================
//  EQMotion — motor de fase linear
//
//  A curva (magnitude analógica exata de todas as bandas, com os ganhos
//  dinâmicos do momento) é convertida, em cada salto, num FIR simétrico de K
//  coeficientes (janela de Blackman) e aplicada por convolução FFT
//  overlap-add. Sem pré-ringing escondido: latência = R + (K−1)/2, reportada ao DAW.
//
//  Colocações L/R/M/S: por bin, o processamento é a matriz real 2×2
//      [L'] = [ a(m+s)/2   b(m−s)/2 ] [L]
//      [R']   [ a(m−s)/2   b(m+s)/2 ] [R]
//  (a, b = curvas estéreo+L / estéreo+R; m, s = curvas Mid / Side), por isso
//  continua a ser fase linear e custa 2 FFT + 2 IFFT por salto.
// =============================================================================
#pragma once
#include "Dynamics.h"
#include "FFT.h"
#include "IIREngine.h"
#include <vector>

namespace eqm
{
struct LinearQualitySpec { int kernel, fft, hop; };

inline LinearQualitySpec linearSpec (LinearQuality q)
{
    switch (q)
    {
        case LinearQuality::Low:  return { 2047, 4096, 512 };
        case LinearQuality::High: return { 8191, 16384, 2048 };
        default:                  return { 4095, 8192, 1024 };
    }
}
inline int linearLatency (LinearQuality q) { const auto s = linearSpec (q); return s.hop + (s.kernel - 1) / 2; }

class LinearPhaseEngine
{
public:
    using cpx = std::complex<double>;

    void prepare (double sampleRate, LinearQuality quality)
    {
        fs = sampleRate;
        spec = linearSpec (quality);
        K = spec.kernel; N = spec.fft; R = spec.hop; Nd = K + 1;
        bins = N / 2 + 1; dbins = Nd / 2 + 1;
        fftN.setSize (N); fftD.setSize (Nd);

        window.resize ((size_t) K);
        for (int n = 0; n < K; ++n)
            window[(size_t) n] = 0.42 - 0.5 * std::cos (kTwoPi * n / (K - 1)) + 0.08 * std::cos (2.0 * kTwoPi * n / (K - 1));

        for (auto& v : bandDb) v.assign ((size_t) dbins, 0.0);
        dhDb.assign ((size_t) dbins, 0.0);
        for (auto* t : { &tA, &tB, &tC, &tD }) t->assign ((size_t) dbins, 0.0);
        for (auto& k : kern) k.assign ((size_t) bins, cpx());
        for (auto& v : curve) v.assign ((size_t) dbins, 0.0);
        tmpD.assign ((size_t) Nd, 0.0); tmpN.assign ((size_t) N, 0.0);
        specD.assign ((size_t) dbins, cpx());
        for (int c = 0; c < 2; ++c)
        {
            inBuf[c].assign ((size_t) R, 0.0); outBuf[c].assign ((size_t) R, 0.0);
            acc[c].assign ((size_t) N, 0.0);
            X[c].assign ((size_t) bins, cpx());
        }
        Y.assign ((size_t) bins, cpx());
        dyn.prepare (fs);
        reset();
    }

    int latency() const { return R + (K - 1) / 2; }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (inBuf[c].begin(), inBuf[c].end(), 0.0);
            std::fill (outBuf[c].begin(), outBuf[c].end(), 0.0);
            std::fill (acc[c].begin(), acc[c].end(), 0.0);
        }
        pos = 0;
        for (auto& c : cache) c.valid = false;
        kernelsValid = false;
        dyn.reset();
    }

    void process (double* L, double* Rr, const double* scL, const double* scR, int n,
                  const BandArray& bands, const GlobalDynSettings& g) noexcept
    {
        dyn.setDeharshTiming (g.deharshAttackMs, g.deharshReleaseMs);
        const bool scOk = scL != nullptr && scR != nullptr;
        constexpr int sub = 16;
        for (int start = 0; start < n; start += sub)
        {
            const int m = std::min (sub, n - start);
            dyn.process (bands, L + start, Rr + start, scOk ? scL + start : nullptr, scOk ? scR + start : nullptr,
                         scOk, m, g.deharshOn, g.deharshAmount);
            for (int i = start; i < start + m; ++i)
            {
                inBuf[0][(size_t) pos] = L[i];
                inBuf[1][(size_t) pos] = Rr[i];
                L[i]  = outBuf[0][(size_t) pos];
                Rr[i] = outBuf[1][(size_t) pos];
                if (++pos == R) { pos = 0; hop (bands, g); }
            }
        }
    }

    double bandDeltaDb (int b) const { return dyn.delta (b); }
    double bandLevelDb (int b) const { return dyn.level (b); }
    double deharshDeltaDb() const    { return dyn.deharshDeltaDb(); }

private:
    struct BandCache { BandParams p; double gain = 0; bool valid = false; };

    void hop (const BandArray& bands, const GlobalDynSettings& g) noexcept
    {
        updateKernels (bands, g);

        for (int c = 0; c < 2; ++c)
        {
            std::fill (tmpN.begin(), tmpN.end(), 0.0);
            std::copy (inBuf[c].begin(), inBuf[c].end(), tmpN.begin());
            fftN.forward (tmpN.data(), X[(size_t) c].data());
        }
        for (int c = 0; c < 2; ++c)
        {
            const auto& kSelf  = kern[c == 0 ? 0 : 3];
            const auto& kCross = kern[c == 0 ? 1 : 2];
            const auto& xs = X[(size_t) c];
            const auto& xo = X[(size_t) (1 - c)];
            if (crossActive)
                for (int k = 0; k < bins; ++k) Y[(size_t) k] = kSelf[(size_t) k] * xs[(size_t) k] + kCross[(size_t) k] * xo[(size_t) k];
            else
                for (int k = 0; k < bins; ++k) Y[(size_t) k] = kSelf[(size_t) k] * xs[(size_t) k];
            fftN.inverse (Y.data(), tmpN.data());
            auto& a = acc[c];
            for (int i = 0; i < N; ++i) a[(size_t) i] += tmpN[(size_t) i];
            for (int i = 0; i < R; ++i) outBuf[c][(size_t) i] = std::isfinite (a[(size_t) i]) ? a[(size_t) i] : 0.0;
            std::move (a.begin() + R, a.end(), a.begin());
            std::fill (a.end() - R, a.end(), 0.0);
        }
    }

    // Magnitude analógica (dB) de uma banda na grelha de desenho
    void fillBandDb (int b, const BandParams& p, double gainDb)
    {
        auto& v = bandDb[(size_t) b];
        const auto sl = designAnalog (p, gainDb);
        const double f0 = clampv (p.freq, kMinFreq, kMaxFreq);
        for (int k = 0; k < dbins; ++k)
            v[(size_t) k] = clampv (analogMagnitudeDb (sl, (double) k * fs / Nd, f0), -150.0, 60.0);
    }

    void updateKernels (const BandArray& bands, const GlobalDynSettings& g) noexcept
    {
        bool changed = ! kernelsValid;
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& p = bands[(size_t) b];
            auto& c = cache[(size_t) b];
            const bool on = p.isProcessing();
            const double gain = on ? effectiveGain (p, g.gainScale, p.isDynamic() ? dyn.delta (b) : 0.0) : 0.0;
            const bool same = c.valid && c.p.isProcessing() == on
                && (! on || (c.p.type == p.type && c.p.slope == p.slope && c.p.placement == p.placement
                             && c.p.freq == p.freq && c.p.q == p.q && std::abs (c.gain - gain) < 0.02));
            if (same) continue;
            c.p = p; c.gain = gain; c.valid = true;
            if (on) fillBandDb (b, p, gain);
            changed = true;
        }
        const double dh = g.deharshOn ? dyn.deharshDeltaDb() : 0.0;
        if (std::abs (dh - dhLast) > 0.02 || (dh != 0.0) != dhOn)
        {
            dhLast = dh; dhOn = dh != 0.0;
            if (dhOn)
            {
                const auto sl = designAnalog (deharshBand (dh), dh);
                for (int k = 0; k < dbins; ++k) dhDb[(size_t) k] = analogMagnitudeDb (sl, (double) k * fs / Nd, DeHarshDetector::kFreq);
            }
            changed = true;
        }
        if (! changed) return;
        kernelsValid = true;

        // curvas por domínio: 0 = L (estéreo+L), 1 = R (estéreo+R), 2 = Mid, 3 = Side
        for (auto& cv : curve) std::fill (cv.begin(), cv.end(), 0.0);
        bool msUsed = false;
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& c = cache[(size_t) b];
            if (! c.p.isProcessing()) continue;
            const auto& v = bandDb[(size_t) b];
            auto addTo = [&] (int d) { auto& cv = curve[(size_t) d]; for (int k = 0; k < dbins; ++k) cv[(size_t) k] += v[(size_t) k]; };
            switch (c.p.placement)
            {
                case Placement::Left:  addTo (0); break;
                case Placement::Right: addTo (1); break;
                case Placement::Mid:   addTo (2); msUsed = true; break;
                case Placement::Side:  addTo (3); msUsed = true; break;
                default: addTo (0); addTo (1); break;
            }
        }
        if (dhOn) for (int d = 0; d < 2; ++d) for (int k = 0; k < dbins; ++k) curve[(size_t) d][(size_t) k] += dhDb[(size_t) k];

        crossActive = msUsed;
        // matriz por bin (valores reais) na grelha de desenho
        for (auto* t : { &tA, &tB, &tC, &tD }) std::fill (t->begin(), t->end(), 0.0);
        for (int k = 0; k < dbins; ++k)
        {
            const double a = dbToGain (curve[0][(size_t) k]), b = dbToGain (curve[1][(size_t) k]);
            if (msUsed)
            {
                const double m = dbToGain (curve[2][(size_t) k]), s = dbToGain (curve[3][(size_t) k]);
                tA[(size_t) k] = a * (m + s) * 0.5;   // L ← L
                tB[(size_t) k] = b * (m - s) * 0.5;   // L ← R
                tC[(size_t) k] = a * (m - s) * 0.5;   // R ← L
                tD[(size_t) k] = b * (m + s) * 0.5;   // R ← R
            }
            else { tA[(size_t) k] = a; tD[(size_t) k] = b; }
        }
        makeKernel (tA, kern[0]);
        makeKernel (tD, kern[3]);
        if (msUsed) { makeKernel (tB, kern[1]); makeKernel (tC, kern[2]); }
    }

    // magnitude real de fase zero → FIR simétrico de K coeficientes → espectro de N pontos
    void makeKernel (const std::vector<double>& mag, std::vector<cpx>& out)
    {
        for (int k = 0; k < dbins; ++k) specD[(size_t) k] = cpx (mag[(size_t) k], 0.0);
        fftD.inverse (specD.data(), tmpD.data());
        std::fill (tmpN.begin(), tmpN.end(), 0.0);
        const int c = (K - 1) / 2;
        for (int n = 0; n < K; ++n)
        {
            const int idx = ((n - c) % Nd + Nd) % Nd;
            tmpN[(size_t) n] = tmpD[(size_t) idx] * window[(size_t) n];
        }
        fftN.forward (tmpN.data(), out.data());
    }

    double fs = 48000;
    LinearQualitySpec spec {};
    int K = 4095, N = 8192, R = 1024, Nd = 4096, bins = 0, dbins = 0, pos = 0;
    RealFFT fftN, fftD;
    std::vector<double> window, tmpD, tmpN, dhDb, tA, tB, tC, tD;
    std::array<std::vector<double>, kMaxBands> bandDb;
    std::array<BandCache, kMaxBands> cache;
    std::array<std::vector<double>, 4> curve;
    std::array<std::vector<cpx>, 4> kern;
    std::vector<cpx> specD, Y;
    std::array<std::vector<double>, 2> inBuf, outBuf, acc;
    std::array<std::vector<cpx>, 2> X;
    bool kernelsValid = false, crossActive = false, dhOn = false;
    double dhLast = 0;
    DynamicsBank dyn;
};
} // namespace eqm
