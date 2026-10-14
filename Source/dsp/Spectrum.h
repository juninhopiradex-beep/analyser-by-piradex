// =============================================================================
//  EQMotion — analisador de espetro (corre numa thread de trabalho, nunca na de áudio)
// =============================================================================
#pragma once
#include "FFT.h"
#include <vector>

namespace eqm
{
// Grelha logarítmica de 1/48 de oitava (20 Hz → 20 kHz) usada pelas Smart Ops e
// pelas médias de longo prazo partilhadas entre instâncias.
struct LogGrid
{
    static constexpr int kPerOct = 48;
    static constexpr double kF0 = 20.0;
    static int size() { return (int) (std::log2 (20000.0 / kF0) * kPerOct) + 1; }   // 479
    static double freq (int i) { return kF0 * std::pow (2.0, (double) i / kPerOct); }
    static double index (double f) { return std::log2 (std::max (f, 1.0) / kF0) * kPerOct; }
};

// Converte potências por bin (0..N/2) para a grelha (média de potência dentro de ±1/96 oct;
// abaixo da resolução do FFT interpola entre bins)
inline void binsToGrid (const double* pow, int numBins, double fs, int fftSize, std::vector<double>& outDb)
{
    const int G = LogGrid::size();
    outDb.resize ((size_t) G);
    const double binHz = fs / fftSize;
    for (int i = 0; i < G; ++i)
    {
        const double f = LogGrid::freq (i);
        const double lo = f * std::pow (2.0, -0.5 / LogGrid::kPerOct), hi = f * std::pow (2.0, 0.5 / LogGrid::kPerOct);
        int k0 = (int) std::ceil (lo / binHz), k1 = (int) std::floor (hi / binHz);
        k0 = std::max (k0, 1); k1 = std::min (k1, numBins - 1);
        double p;
        if (k1 >= k0)
        {
            double s = 0; for (int k = k0; k <= k1; ++k) s += pow[k];
            p = s / (k1 - k0 + 1);
        }
        else
        {
            const double kf = f / binHz;
            const int a = clampv ((int) std::floor (kf), 1, numBins - 2);
            const double t = clampv (kf - a, 0.0, 1.0);
            p = pow[a] * (1 - t) + pow[a + 1] * t;
        }
        outDb[(size_t) i] = 10.0 * std::log10 (p + 1e-24);
    }
}

// -----------------------------------------------------------------------------
class SpectrumAnalyzer
{
public:
    enum class Speed { Slow = 0, Medium, Fast };

    void configure (double sampleRate, int fftSize, int displayPoints, double fMin, double fMax)
    {
        fs = sampleRate; N = fftSize; P = displayPoints; lo = fMin; hi = std::min (fMax, fs * 0.5);
        hop = std::min (N / 4, 1024);
        fft.setSize (N);
        win.resize ((size_t) N);
        double wsum = 0;
        for (int n = 0; n < N; ++n)
        {
            const double x = kTwoPi * n / N;   // Blackman-Harris de 4 termos
            win[(size_t) n] = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2 * x) - 0.01168 * std::cos (3 * x);
            wsum += win[(size_t) n];
        }
        norm = 2.0 / wsum;                     // um seno de amplitude A aparece a 20·log10(A)
        ring.assign ((size_t) N, 0.0); ringPos = 0; sinceHop = 0;
        frame.assign ((size_t) N, 0.0);
        spec.assign ((size_t) N / 2 + 1, {});
        power.assign ((size_t) N / 2 + 1, 0.0);
        avgPower.assign ((size_t) N / 2 + 1, 0.0);
        display.assign ((size_t) P, -160.0f);
        peak.assign ((size_t) P, -160.0f);
        pointBins.resize ((size_t) P);
        for (int i = 0; i < P; ++i)
        {
            const double f = freqAt (i);
            const double half = std::pow (hi / lo, 0.5 / (P - 1));
            pointBins[(size_t) i] = { f / half * N / fs, f * half * N / fs };
        }
        frameReady = false;
    }

    double freqAt (int i) const { return lo * std::pow (hi / lo, (double) i / (P - 1)); }
    int points() const { return P; }
    int fftSize() const { return N; }

    void setSpeed (Speed s) { speed = s; }
    void setSlope (double dbPerOct) { slope = dbPerOct; }
    void resetPeaks() { std::fill (peak.begin(), peak.end(), -160.0f); }

    // Acrescenta amostras; devolve true quando o ecrã foi atualizado
    bool push (const float* x, int n)
    {
        bool updated = false;
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = x[i];
            ringPos = (ringPos + 1) % N;
            if (++sinceHop >= hop) { sinceHop = 0; analyse(); updated = true; }
        }
        return updated;
    }

    const std::vector<float>& displayDb() const { return display; }
    const std::vector<float>& peakDb() const { return peak; }
    const std::vector<double>& averagedPower() const { return avgPower; }   // por bin, média lenta
    const std::vector<double>& lastPower() const { return power; }
    double sampleRate() const { return fs; }

private:
    void analyse()
    {
        for (int n = 0; n < N; ++n) frame[(size_t) n] = ring[(size_t) ((ringPos + n) % N)] * win[(size_t) n];
        fft.forward (frame.data(), spec.data());
        const double dt = (double) hop / fs;
        const double avgK = std::exp (-dt / 8.0);   // média de longo prazo (8 s)
        for (size_t k = 0; k < power.size(); ++k)
        {
            power[k] = std::norm (spec[k]) * norm * norm;
            avgPower[k] = power[k] + (avgPower[k] - power[k]) * (frameReady ? avgK : 0.0);
        }
        frameReady = true;

        const double fallDbPerSec = speed == Speed::Slow ? 10.0 : speed == Speed::Medium ? 24.0 : 60.0;
        const double riseK = speed == Speed::Slow ? 0.35 : speed == Speed::Medium ? 0.6 : 0.9;
        const double fall = fallDbPerSec * dt;
        const double peakFall = 3.0 * dt;
        for (int i = 0; i < P; ++i)
        {
            const auto [b0, b1] = pointBins[(size_t) i];
            double p;
            int k0 = (int) std::ceil (b0), k1 = (int) std::floor (b1);
            k0 = std::max (k0, 1); k1 = std::min (k1, (int) power.size() - 1);
            if (k1 >= k0) { p = 0; for (int k = k0; k <= k1; ++k) p = std::max (p, power[(size_t) k]); }
            else
            {
                const double kf = 0.5 * (b0 + b1);
                const int a = clampv ((int) std::floor (kf), 1, (int) power.size() - 2);
                const double t = clampv (kf - a, 0.0, 1.0);
                p = power[(size_t) a] * (1 - t) + power[(size_t) a + 1] * t;   // interpolação suave nos graves
            }
            double db = 10.0 * std::log10 (p + 1e-20) + slope * std::log2 (freqAt (i) / 1000.0);
            db = std::max (db, -160.0);
            float& d = display[(size_t) i];
            if (db > d) d = (float) (d + (db - d) * riseK);
            else        d = (float) std::max (db, d - fall);
            float& pk = peak[(size_t) i];
            pk = std::max ((float) db, pk - (float) peakFall);
        }
    }

    double fs = 48000, lo = 10, hi = 24000, norm = 1, slope = 4.5;
    int N = 4096, P = 400, hop = 1024, ringPos = 0, sinceHop = 0;
    Speed speed = Speed::Medium;
    RealFFT fft;
    std::vector<double> win, ring, frame, power, avgPower;
    std::vector<std::complex<double>> spec;
    std::vector<float> display, peak;
    std::vector<std::pair<double, double>> pointBins;
    bool frameReady = false;
};
} // namespace eqm
