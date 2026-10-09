// =============================================================================
//  Piradex Curve Analyzer — núcleo DSP (sem dependências do JUCE)
//
//  Medição por excitação PERIÓDICA (período = tamanho da FFT):
//    • o Gerador emite um sinal conhecido que se repete exatamente a cada N amostras
//    • o Analisador captura N amostras à saída do plugin/equipamento em teste
//    • como o sinal é periódico, a FFT de qualquer bloco de N amostras dá o
//      espetro exato sem janela e sem "leakage" -> H(f) = Y(f) / X(f)
//    • o atraso (latência) aparece só como fase linear, detetado pelo pico da
//      resposta impulsional (Auto Sync) e removido da fase mostrada.
//
//  Modo Música (dry/wet por sidechain): estimador H1 de Welch
//      H = Sxy / Sxx   e   coerência = |Sxy|² / (Sxx·Syy)
//  com alinhamento automático do dry por GCC-PHAT.
//
//  Modo Harmónicos: seno exatamente no centro de um bin -> harmónicos caem
//  exatamente em bins inteiros, leitura sem leakage. THD, THD+N, par/ímpar.
// =============================================================================
#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <random>
#include <limits>

namespace pca
{
using cpx = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;

inline bool isPowerOfTwo (int n) { return n > 0 && (n & (n - 1)) == 0; }
inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double powToDb (double p)  { return 10.0 * std::log10 (std::max (p, 1.0e-30)); }
inline double ampToDb (double a)  { return 20.0 * std::log10 (std::max (a, 1.0e-15)); }

// -----------------------------------------------------------------------------
//  FFT complexa radix-2 (iterativa, tabelas pré-calculadas)
// -----------------------------------------------------------------------------
class FFT
{
public:
    void init (int n)
    {
        if (n == N) return;
        N = n;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) (n / 2));
        for (int k = 0; k < n / 2; ++k)
            tw[(size_t) k] = std::polar (1.0, -2.0 * kPi * k / n);
    }

    int size() const { return N; }

    void forward (std::vector<cpx>& a) const { transform (a, false); }
    void inverse (std::vector<cpx>& a) const
    {
        transform (a, true);
        const double s = 1.0 / N;
        for (auto& v : a) v *= s;
    }

    // FFT real: x[N] -> X[N/2+1]
    void realForward (const float* x, std::vector<cpx>& X, std::vector<cpx>& work, const float* window = nullptr) const
    {
        work.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            work[(size_t) i] = cpx (window ? (double) x[i] * window[i] : (double) x[i], 0.0);
        forward (work);
        X.assign (work.begin(), work.begin() + N / 2 + 1);
    }

    // Espetro hermitiano X[N/2+1] -> sinal real (parte real) em work[N]
    void realInverse (const std::vector<cpx>& X, std::vector<cpx>& work) const
    {
        work.assign ((size_t) N, cpx (0.0, 0.0));
        for (int k = 0; k <= N / 2; ++k) work[(size_t) k] = X[(size_t) k];
        for (int k = 1; k < N / 2; ++k)  work[(size_t) (N - k)] = std::conj (X[(size_t) k]);
        inverse (work);
    }

private:
    void transform (std::vector<cpx>& a, bool inv) const
    {
        for (int i = 0; i < N; ++i)
            if (i < rev[(size_t) i]) std::swap (a[(size_t) i], a[(size_t) rev[(size_t) i]]);

        for (int len = 2; len <= N; len <<= 1)
        {
            const int half = len >> 1, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int j = 0; j < half; ++j)
                {
                    cpx w = tw[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const cpx u = a[(size_t) (i + j)];
                    const cpx v = a[(size_t) (i + j + half)] * w;
                    a[(size_t) (i + j)]        = u + v;
                    a[(size_t) (i + j + half)] = u - v;
                }
        }
    }

    int N = 0;
    std::vector<int> rev;
    std::vector<cpx> tw;
};

// -----------------------------------------------------------------------------
//  Sinais de teste
// -----------------------------------------------------------------------------
enum class Signal : int { Impulse = 0, LogSweep = 1, PinkNoise = 2, Sine = 3 };

struct ExcitationSpec
{
    int    N       = 8192;
    double fs      = 48000.0;
    Signal signal  = Signal::LogSweep;
    double levelDb = -18.0;
    double sineHz  = 1000.0;

    bool operator== (const ExcitationSpec& o) const
    {
        return N == o.N && std::abs (fs - o.fs) < 1.0e-6 && signal == o.signal
            && std::abs (levelDb - o.levelDb) < 1.0e-6 && std::abs (sineHz - o.sineHz) < 1.0e-6;
    }
    bool operator!= (const ExcitationSpec& o) const { return ! (*this == o); }
};

// Frequência do seno é arredondada ao centro do bin mais próximo -> sem leakage.
inline int sineBinFor (double hz, int N, double fs)
{
    const int k = (int) std::lround (hz * N / fs);
    return std::clamp (k, 1, N / 2 - 1);
}

struct Excitation
{
    ExcitationSpec   spec;
    std::vector<float> x;       // um período (N amostras)
    std::vector<cpx>   X;       // espetro (N/2+1)
    std::vector<char>  valid;   // bins com energia suficiente para dividir
    int sineBin = 0;
};

inline Excitation makeExcitation (const ExcitationSpec& s)
{
    Excitation e;
    e.spec = s;
    const int N = s.N;
    const double A = dbToGain (s.levelDb);
    FFT fft; fft.init (N);
    std::vector<cpx> work;
    e.x.assign ((size_t) N, 0.0f);

    // Bin mínimo com energia "rosa" (~10 Hz) — abaixo disto mantém-se a magnitude
    const int k1 = std::max (1, (int) std::lround (10.0 * N / s.fs));
    const int kh = N / 2;

    switch (s.signal)
    {
        case Signal::Impulse:
            e.x[0] = (float) A;
            break;

        case Signal::Sine:
        {
            e.sineBin = sineBinFor (s.sineHz, N, s.fs);
            for (int n = 0; n < N; ++n)
                e.x[(size_t) n] = (float) (A * std::sin (2.0 * kPi * (double) e.sineBin * n / N));
            break;
        }

        case Signal::LogSweep:
        case Signal::PinkNoise:
        {
            std::vector<cpx> S ((size_t) kh + 1, cpx (0.0, 0.0));
            if (s.signal == Signal::LogSweep)
            {
                // Sweep logarítmico sintetizado na frequência:
                //   |X(k)| ∝ 1/sqrt(k)  (espetro rosa -> mais energia nos graves)
                //   atraso de grupo τ(k) cresce com log(k) -> varrimento grave->agudo
                const double T = 0.92 * N;
                double phase = 0.0;
                for (int k = 1; k < kh; ++k)
                {
                    const double kk  = (double) std::max (k, k1);
                    const double tau = (k <= k1) ? 0.0 : T * std::log (kk / k1) / std::log ((double) kh / k1);
                    phase -= 2.0 * kPi * tau / N;
                    S[(size_t) k] = std::polar (1.0 / std::sqrt (kk), phase);
                }
            }
            else
            {
                std::mt19937 rng (0x5EEDu);
                std::uniform_real_distribution<double> ph (-kPi, kPi);
                for (int k = 1; k < kh; ++k)
                {
                    const double kk = (double) std::max (k, k1);
                    S[(size_t) k] = std::polar (1.0 / std::sqrt (kk), ph (rng));
                }
            }
            fft.realInverse (S, work);
            double peak = 0.0;
            for (int n = 0; n < N; ++n) peak = std::max (peak, std::abs (work[(size_t) n].real()));
            const double g = peak > 0.0 ? A / peak : 0.0;
            for (int n = 0; n < N; ++n) e.x[(size_t) n] = (float) (work[(size_t) n].real() * g);
            break;
        }
    }

    fft.realForward (e.x.data(), e.X, work);
    double maxP = 0.0;
    for (auto& v : e.X) maxP = std::max (maxP, std::norm (v));
    e.valid.assign (e.X.size(), 0);
    for (size_t k = 1; k + 1 < e.X.size(); ++k)
        e.valid[k] = (std::norm (e.X[k]) > maxP * 1.0e-12) ? 1 : 0;
    if (s.signal == Signal::Sine)
    {
        std::fill (e.valid.begin(), e.valid.end(), 0);
        e.valid[(size_t) e.sineBin] = 1;
    }
    return e;
}

// Pico (circular) de uma resposta impulsional real com interpolação parabólica.
// Devolve atraso em amostras no intervalo [-N/2, N/2) e a proeminência (pico/rms).
inline double findImpulsePeak (const std::vector<cpx>& ir, double* prominence = nullptr)
{
    const int N = (int) ir.size();
    int idx = 0; double best = -1.0, sum2 = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double v = std::abs (ir[(size_t) i].real());
        sum2 += v * v;
        if (v > best) { best = v; idx = i; }
    }
    if (prominence)
    {
        const double rms = std::sqrt (sum2 / std::max (1, N));
        *prominence = rms > 0.0 ? best / rms : 0.0;
    }
    const double l = std::abs (ir[(size_t) ((idx - 1 + N) % N)].real());
    const double r = std::abs (ir[(size_t) ((idx + 1) % N)].real());
    const double den = l - 2.0 * best + r;
    double frac = (std::abs (den) > 1.0e-20) ? 0.5 * (l - r) / den : 0.0;
    frac = std::clamp (frac, -0.5, 0.5);
    double d = idx + frac;
    if (d >= N / 2) d -= N;
    return d;
}

// Média: n>0 -> média móvel exponencial com janela n; n==0 -> média infinita
inline double averagingAlpha (int count, int avgN)
{
    return avgN > 0 ? 1.0 / std::min (count, avgN) : 1.0 / count;
}

// -----------------------------------------------------------------------------
//  Analisador de resposta (Impulso / Sweep / Ruído rosa)
// -----------------------------------------------------------------------------
class ResponseAnalyzer
{
public:
    void configure (const Excitation& ex)
    {
        exc = &ex;
        N = ex.spec.N;
        fft.init (N);
        Yavg.assign ((size_t) N / 2 + 1, cpx (0.0, 0.0));
        reset();
    }

    void reset() { std::fill (Yavg.begin(), Yavg.end(), cpx (0.0, 0.0)); count = 0; refShift = 0; }
    void setAverages (int n) { avgN = n; }
    int  frames() const { return count; }
    double lastLevelDb() const { return lastDb; }

    // Um período completo (N amostras) capturado à saída do dispositivo
    // Devolve false se o bloco foi rejeitado (silêncio).
    bool addFrame (const float* y)
    {
        double s2 = 0.0;
        for (int i = 0; i < N; ++i) s2 += (double) y[i] * y[i];
        lastDb = powToDb (s2 / N);
        if (lastDb < -110.0) return false;      // sem sinal -> não estraga a média

        fft.realForward (y, Y, work);

        // Alinhamento robusto: se o host parar/recomeçar, o desfasamento circular
        // entre gerador e analisador muda. Cada bloco é realinhado ao primeiro.
        computeRatio (Y, Hf);
        fft.realInverse (Hf, work);
        double prom = 0.0;
        const int p = (int) std::lround (findImpulsePeak (work, &prom));
        if (count == 0) refShift = p;
        int shift = p - refShift;
        if (shift >= N / 2) shift -= N;
        if (shift < -N / 2) shift += N;
        if (prom > 8.0 && shift != 0)
            for (int k = 0; k <= N / 2; ++k)
                Y[(size_t) k] *= std::polar (1.0, 2.0 * kPi * k * shift / N);

        ++count;
        const double a = averagingAlpha (count, avgN);
        for (int k = 0; k <= N / 2; ++k) Yavg[(size_t) k] += a * (Y[(size_t) k] - Yavg[(size_t) k]);
        return true;
    }

    // H = Ymédio / X
    void transfer (std::vector<cpx>& H, std::vector<char>& valid) const
    {
        computeRatio (Yavg, H);
        valid = exc->valid;
    }

    // Atraso total (circular) estimado pelo pico da resposta impulsional.
    // Arredondado à amostra: latências digitais (plugin, interface, conversores)
    // são inteiras; o resto (atraso de grupo do EQ/analógico) faz parte da curva.
    double estimateDelay (const std::vector<cpx>& H)
    {
        fft.realInverse (H, work);
        return (double) std::lround (findImpulsePeak (work));
    }

    // Resposta impulsional média (para exportação / visualização)
    void impulseResponse (std::vector<float>& ir)
    {
        std::vector<cpx> H; std::vector<char> v;
        transfer (H, v);
        fft.realInverse (H, work);
        ir.resize ((size_t) N);
        for (int i = 0; i < N; ++i) ir[(size_t) i] = (float) work[(size_t) i].real();
    }

private:
    void computeRatio (const std::vector<cpx>& Yin, std::vector<cpx>& H) const
    {
        H.assign ((size_t) N / 2 + 1, cpx (0.0, 0.0));
        for (int k = 0; k <= N / 2; ++k)
            if (exc->valid[(size_t) k]) H[(size_t) k] = Yin[(size_t) k] / exc->X[(size_t) k];
    }

    const Excitation* exc = nullptr;
    int N = 0, count = 0, avgN = 8, refShift = 0;
    double lastDb = -200.0;
    FFT fft;
    std::vector<cpx> Y, Yavg, Hf, work;
};

// -----------------------------------------------------------------------------
//  Analisador de harmónicos (seno no centro do bin)
// -----------------------------------------------------------------------------
struct HarmonicResult
{
    static constexpr int kMaxH = 10;
    double f0 = 0.0;
    double ampDb[kMaxH + 1] {};     // dBFS de cada harmónico (índice 1..10), -200 se acima de Nyquist
    double relDb[kMaxH + 1] {};     // dBc (relativo à fundamental)
    bool   present[kMaxH + 1] {};
    double thdPct = 0.0, thdnPct = 0.0;
    double evenDb = -200.0, oddDb = -200.0;   // energia harmónicos pares / ímpares (dBc)
    double gainDb = 0.0;                      // ganho na fundamental
    double noiseFloorDb = -200.0;
};

class HarmonicAnalyzer
{
public:
    void configure (const Excitation& ex)
    {
        exc = &ex;
        N = ex.spec.N;
        fs = ex.spec.fs;
        k0 = ex.sineBin;
        inAmp = dbToGain (ex.spec.levelDb);
        fft.init (N);
        Yavg.assign ((size_t) N / 2 + 1, cpx (0.0, 0.0));
        reset();
    }

    void reset() { std::fill (Yavg.begin(), Yavg.end(), cpx (0.0, 0.0)); count = 0; }
    void setAverages (int n) { avgN = n; }
    int  frames() const { return count; }
    double lastLevelDb() const { return lastDb; }

    bool addFrame (const float* y)
    {
        double s2 = 0.0;
        for (int i = 0; i < N; ++i) s2 += (double) y[i] * y[i];
        lastDb = powToDb (s2 / N);
        if (lastDb < -110.0) return false;

        fft.realForward (y, Y, work);

        // Alinhar à fase da fundamental: harmónicos de um sistema estacionário
        // estão presos em fase à fundamental, por isso rodar todos os bins pelo
        // mesmo atraso alinha harmónicos e deixa o ruído cair com a média.
        const double ph = std::arg (Y[(size_t) k0]);
        if (count == 0) refPhase = ph;
        const double w0  = 2.0 * kPi * k0 / N;
        double dphi = ph - refPhase;
        while (dphi >  kPi) dphi -= 2.0 * kPi;
        while (dphi < -kPi) dphi += 2.0 * kPi;
        const double tau = dphi / w0;     // atraso fracionário equivalente
        for (int k = 0; k <= N / 2; ++k)
            Y[(size_t) k] *= std::polar (1.0, 2.0 * kPi * k * tau / N);

        ++count;
        const double a = averagingAlpha (count, avgN);
        for (int k = 0; k <= N / 2; ++k) Yavg[(size_t) k] += a * (Y[(size_t) k] - Yavg[(size_t) k]);
        return true;
    }

    // Amplitude de pico por bin (seno de amplitude A -> A)
    void amplitudeSpectrum (std::vector<double>& amp) const
    {
        amp.resize ((size_t) N / 2 + 1);
        for (int k = 0; k <= N / 2; ++k)
            amp[(size_t) k] = std::abs (Yavg[(size_t) k]) * (k == 0 || k == N / 2 ? 1.0 : 2.0) / N;
    }

    HarmonicResult analyse() const
    {
        HarmonicResult r;
        std::vector<double> amp;
        amplitudeSpectrum (amp);
        r.f0 = (double) k0 * fs / N;
        const double a1 = std::max (amp[(size_t) k0], 1.0e-15);
        double harm2 = 0.0, even2 = 0.0, odd2 = 0.0;
        for (int h = 1; h <= HarmonicResult::kMaxH; ++h)
        {
            const int kb = h * k0;
            if (kb >= N / 2) { r.present[h] = false; r.ampDb[h] = r.relDb[h] = -200.0; continue; }
            r.present[h] = true;
            const double a = amp[(size_t) kb];
            r.ampDb[h] = ampToDb (a);
            r.relDb[h] = ampToDb (a / a1);
            if (h >= 2)
            {
                harm2 += a * a;
                (h % 2 == 0 ? even2 : odd2) += a * a;
            }
        }
        double all2 = 0.0;
        std::vector<double> others;
        others.reserve ((size_t) N / 2);
        for (int k = 1; k < N / 2; ++k)
        {
            if (k == k0) continue;
            all2 += amp[(size_t) k] * amp[(size_t) k];
            bool isH = false;
            for (int h = 2; h <= HarmonicResult::kMaxH; ++h) if (k == h * k0) isH = true;
            if (! isH) others.push_back (amp[(size_t) k]);
        }
        r.thdPct  = 100.0 * std::sqrt (harm2) / a1;
        r.thdnPct = 100.0 * std::sqrt (all2) / a1;
        r.evenDb  = powToDb (even2 / (a1 * a1));
        r.oddDb   = powToDb (odd2  / (a1 * a1));
        r.gainDb  = ampToDb (a1 / std::max (inAmp, 1.0e-15));
        if (! others.empty())
        {
            std::nth_element (others.begin(), others.begin() + (long) others.size() / 2, others.end());
            r.noiseFloorDb = ampToDb (others[others.size() / 2]);
        }
        return r;
    }

private:
    const Excitation* exc = nullptr;
    int N = 0, k0 = 1, count = 0, avgN = 8;
    double fs = 48000.0, inAmp = 1.0, refPhase = 0.0, lastDb = -200.0;
    FFT fft;
    std::vector<cpx> Y, Yavg, work;
};

// -----------------------------------------------------------------------------
//  Analisador de transferência dry/wet (música real, via sidechain)
// -----------------------------------------------------------------------------
class TransferAnalyzer
{
public:
    void configure (int n)
    {
        N = n;
        fft.init (N);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));
        Sxy.assign ((size_t) N / 2 + 1, cpx (0.0, 0.0));
        Sxx.assign ((size_t) N / 2 + 1, 0.0);
        Syy.assign ((size_t) N / 2 + 1, 0.0);
        reset();
    }

    void reset()
    {
        std::fill (Sxy.begin(), Sxy.end(), cpx (0.0, 0.0));
        std::fill (Sxx.begin(), Sxx.end(), 0.0);
        std::fill (Syy.begin(), Syy.end(), 0.0);
        count = 0;
    }
    void setAverages (int n) { avgN = n; }
    int  frames() const { return count; }
    double dryLevelDb() const { return dryDb; }
    double wetLevelDb() const { return wetDb; }

    bool addFrames (const float* dry, const float* wet)
    {
        double d2 = 0.0, w2 = 0.0;
        for (int i = 0; i < N; ++i) { d2 += (double) dry[i] * dry[i]; w2 += (double) wet[i] * wet[i]; }
        dryDb = powToDb (d2 / N);
        wetDb = powToDb (w2 / N);
        if (dryDb < -90.0) return false;   // silêncio na música -> ignorar

        fft.realForward (dry, X, work, win.data());
        fft.realForward (wet, Y, work, win.data());
        ++count;
        const double a = averagingAlpha (count, avgN);
        for (int k = 0; k <= N / 2; ++k)
        {
            Sxy[(size_t) k] += a * (std::conj (X[(size_t) k]) * Y[(size_t) k] - Sxy[(size_t) k]);
            Sxx[(size_t) k] += a * (std::norm (X[(size_t) k]) - Sxx[(size_t) k]);
            Syy[(size_t) k] += a * (std::norm (Y[(size_t) k]) - Syy[(size_t) k]);
        }
        return true;
    }

    void transfer (std::vector<cpx>& H, std::vector<char>& valid, std::vector<double>& coh) const
    {
        const size_t M = (size_t) N / 2 + 1;
        H.assign (M, cpx (0.0, 0.0));
        valid.assign (M, 0);
        coh.assign (M, 0.0);
        double maxXX = 0.0;
        for (auto v : Sxx) maxXX = std::max (maxXX, v);
        const double floor = std::max (maxXX * 1.0e-9, 1.0e-20);   // -90 dB relativo
        for (size_t k = 1; k + 1 < M; ++k)
        {
            if (Sxx[k] <= floor) continue;
            valid[k] = 1;
            H[k] = Sxy[k] / Sxx[k];
            coh[k] = std::norm (Sxy[k]) / std::max (Sxx[k] * Syy[k], 1.0e-30);
        }
    }

    // Atraso residual do wet face ao dry (GCC-PHAT). Positivo -> wet chega mais tarde.
    int residualLag (double* prominence = nullptr)
    {
        std::vector<cpx> P ((size_t) N / 2 + 1, cpx (0.0, 0.0));
        double maxXX = 0.0;
        for (auto v : Sxx) maxXX = std::max (maxXX, v);
        for (int k = 1; k < N / 2; ++k)
        {
            const double m = std::abs (Sxy[(size_t) k]);
            if (m > 0.0 && Sxx[(size_t) k] > maxXX * 1.0e-9) P[(size_t) k] = Sxy[(size_t) k] / m;
        }
        fft.realInverse (P, work);
        return (int) std::lround (findImpulsePeak (work, prominence));
    }

private:
    int N = 0, count = 0, avgN = 16;
    double dryDb = -200.0, wetDb = -200.0;
    FFT fft;
    std::vector<float> win;
    std::vector<cpx> X, Y, Sxy, work;
    std::vector<double> Sxx, Syy;
};

// -----------------------------------------------------------------------------
//  Curva para ecrã: pontos em escala logarítmica, com suavização fracionária
// -----------------------------------------------------------------------------
struct Curve
{
    std::vector<float> freq, magDb, phaseDeg, coh;
    std::vector<char>  valid;
    void clear() { freq.clear(); magDb.clear(); phaseDeg.clear(); coh.clear(); valid.clear(); }
    bool empty() const { return freq.empty(); }
};

// H: espetro (N/2+1). delay: atraso (amostras) removido da fase.
// smoothOct: 0 = nenhuma (apenas média por píxel), senão largura em oitavas (1/3, 1/6...)
inline void buildLogCurve (const std::vector<cpx>& H, const std::vector<char>& valid,
                           const std::vector<double>* coh, int N, double fs, double delay,
                           int points, double fmin, double fmax, double smoothOct, Curve& out)
{
    out.clear();
    const int M = N / 2 + 1;
    if ((int) H.size() < M || points < 2) return;

    // Somas prefixas: potência, complexo com fase corrigida, coerência, nº de bins válidos
    std::vector<double> pP ((size_t) M + 1, 0.0), pC ((size_t) M + 1, 0.0), pN ((size_t) M + 1, 0.0);
    std::vector<cpx>    pH ((size_t) M + 1, cpx (0.0, 0.0));
    std::vector<cpx>    Hc ((size_t) M, cpx (0.0, 0.0));
    for (int k = 0; k < M; ++k)
    {
        const bool v = valid[(size_t) k] != 0;
        const cpx hc = v ? H[(size_t) k] * std::polar (1.0, 2.0 * kPi * k * delay / N) : cpx (0.0, 0.0);
        Hc[(size_t) k] = hc;
        pP[(size_t) k + 1] = pP[(size_t) k] + (v ? std::norm (hc) : 0.0);
        pH[(size_t) k + 1] = pH[(size_t) k] + hc;
        pC[(size_t) k + 1] = pC[(size_t) k] + (v && coh ? (*coh)[(size_t) k] : 0.0);
        pN[(size_t) k + 1] = pN[(size_t) k] + (v ? 1.0 : 0.0);
    }

    const double binHz = fs / N;
    const double lr = std::log (fmax / fmin);
    const double pixOct = (lr / std::log (2.0)) / (points - 1);
    const double halfOct = 0.5 * std::max (smoothOct, pixOct);

    out.freq.resize ((size_t) points);
    out.magDb.resize ((size_t) points);
    out.phaseDeg.resize ((size_t) points);
    out.coh.resize ((size_t) points);
    out.valid.resize ((size_t) points);

    for (int i = 0; i < points; ++i)
    {
        const double f = fmin * std::exp (lr * i / (points - 1));
        out.freq[(size_t) i] = (float) f;
        const double lo = f * std::pow (2.0, -halfOct), hi = f * std::pow (2.0, halfOct);
        int kLo = std::max (1, (int) std::ceil (lo / binHz));
        int kHi = std::min (M - 2, (int) std::floor (hi / binHz));

        double P = 0.0, C = 0.0, n = 0.0; cpx Hs (0.0, 0.0);
        if (kHi >= kLo)
        {
            n  = pN[(size_t) kHi + 1] - pN[(size_t) kLo];
            P  = pP[(size_t) kHi + 1] - pP[(size_t) kLo];
            Hs = pH[(size_t) kHi + 1] - pH[(size_t) kLo];
            C  = pC[(size_t) kHi + 1] - pC[(size_t) kLo];
        }
        if (n > 0.0)
        {
            out.magDb[(size_t) i]    = (float) powToDb (P / n);
            out.phaseDeg[(size_t) i] = (float) (std::arg (Hs) * 180.0 / kPi);
            out.coh[(size_t) i]      = (float) (C / n);
            out.valid[(size_t) i]    = 1;
        }
        else
        {
            // Nenhum bin dentro do intervalo (graves com FFT curta): interpolar vizinhos
            const double kf = f / binHz;
            const int a = (int) std::floor (kf), b = a + 1;
            if (a >= 1 && b <= M - 2 && valid[(size_t) a] && valid[(size_t) b])
            {
                const double t = kf - a;
                const cpx h = Hc[(size_t) a] * (1.0 - t) + Hc[(size_t) b] * t;
                const double m = std::abs (Hc[(size_t) a]) * (1.0 - t) + std::abs (Hc[(size_t) b]) * t;
                out.magDb[(size_t) i]    = (float) ampToDb (m);
                out.phaseDeg[(size_t) i] = (float) (std::arg (h) * 180.0 / kPi);
                out.coh[(size_t) i]      = coh ? (float) ((*coh)[(size_t) a] * (1.0 - t) + (*coh)[(size_t) b] * t) : 0.0f;
                out.valid[(size_t) i]    = 1;
            }
            else
            {
                out.magDb[(size_t) i] = out.phaseDeg[(size_t) i] = out.coh[(size_t) i] = 0.0f;
                out.valid[(size_t) i] = 0;
            }
        }
    }
}

// Espetro de amplitude (dBFS) com retenção de pico por píxel — harmónicos não são "esmagados"
inline void buildPeakSpectrum (const std::vector<double>& amp, int N, double fs, int points,
                               double fmin, double fmax, Curve& out)
{
    out.clear();
    const int M = N / 2 + 1;
    const double binHz = fs / N, lr = std::log (fmax / fmin);
    const double halfOct = 0.5 * (lr / std::log (2.0)) / (points - 1);
    out.freq.resize ((size_t) points); out.magDb.resize ((size_t) points);
    out.phaseDeg.assign ((size_t) points, 0.0f); out.coh.assign ((size_t) points, 0.0f);
    out.valid.resize ((size_t) points);
    for (int i = 0; i < points; ++i)
    {
        const double f = fmin * std::exp (lr * i / (points - 1));
        out.freq[(size_t) i] = (float) f;
        int kLo = std::max (1, (int) std::ceil (f * std::pow (2.0, -halfOct) / binHz));
        int kHi = std::min (M - 2, (int) std::floor (f * std::pow (2.0, halfOct) / binHz));
        if (kHi < kLo) kLo = kHi = std::clamp ((int) std::lround (f / binHz), 1, M - 2);
        double m = 0.0;
        for (int k = kLo; k <= kHi; ++k) m = std::max (m, amp[(size_t) k]);
        out.magDb[(size_t) i] = (float) ampToDb (m);
        out.valid[(size_t) i] = 1;
    }
}

} // namespace pca
