// SpaceCore.h — núcleo DSP do MIXSPACE by Piradex
// C++17 puro (sem JUCE) para poder ser testado com g++ no GitHub Actions.
//
// Ideia: STFT (Hann-raiz, 75 % de sobreposição) sobre o MAIN e o SIDECHAIN.
// Em cada frame o motor compara, região a região (64–128 bandas logarítmicas),
// a energia do Sidechain com a do Main e só reduz onde há colisão real:
//
//   importância(b) = quanto a banda b é dominante no Sidechain    (FOCUS)
//   presença(b)    = quanto o Main ocupa essa mesma banda
//   colisão(b)     = importância × presença × nível do Sidechain
//   alvo(b)        = RANGE × tanh(k · SPACE · colisão) / tanh(k)  (teto suave)
//
// O alvo passa por attack/release por banda, é interpolado em log-frequência
// para cada bin (curva contínua, sem degraus) e aplicado só ao Main.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace mxs
{
constexpr double kPi = 3.14159265358979323846;

enum class StereoMode { Stereo = 0, Mid, Side, Left, Right };

struct Params
{
    float space = 25.0f;        // 0..100 %
    float rangeDb = 6.0f;       // 0..12 dB (teto de redução)
    float focus = 40.0f;        // 0..100 %
    float lowCutHz = 80.0f;     // limite inferior da zona activa
    float highCutHz = 16000.0f; // limite superior da zona activa
    float attackMs = 15.0f;
    float releaseMs = 180.0f;
    float smooth = 65.0f;       // 0 = cirúrgico, 100 = largo
    StereoMode stereo = StereoMode::Stereo;
};

//==============================================================================
// FFT complexa radix-2 (in-place, re/im separados).
class Fft
{
public:
    void prepare (int size)
    {
        n = size;
        bits = 0;
        while ((1 << bits) < n) ++bits;

        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }

        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
        {
            cosT[(size_t) k] = (float) std::cos (2.0 * kPi * k / n);
            sinT[(size_t) k] = (float) std::sin (2.0 * kPi * k / n);
        }
    }

    int size() const { return n; }

    // X[k] = Σ x[t]·e^(−i2πkt/N)
    void forward (float* re, float* im) const { run (re, im, -1.0f); }

    // Inversa normalizada (divide por N)
    void inverse (float* re, float* im) const
    {
        run (re, im, 1.0f);
        const float s = 1.0f / (float) n;
        for (int i = 0; i < n; ++i) { re[i] *= s; im[i] *= s; }
    }

private:
    void run (float* re, float* im, float sign) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[(size_t) (k * step)];
                    const float wi = sign * sinT[(size_t) (k * step)];
                    const int a = i + k, b = a + half;
                    const float vr = re[b] * wr - im[b] * wi;
                    const float vi = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - vr;  im[b] = im[a] - vi;
                    re[a] += vr;         im[a] += vi;
                }
            }
        }
    }

    int n = 0, bits = 0;
    std::vector<int> rev;
    std::vector<float> cosT, sinT;
};

//==============================================================================
// Tamanho da FFT por modo de qualidade, escalado com a sample rate
// (mantém a mesma resolução em Hz e em ms a 44.1/48/88.2/96/176.4/192 kHz).
inline int fftSizeFor (int qualityMode, double sampleRate)
{
    const int base = qualityMode == 0 ? 1024 : (qualityMode == 2 ? 4096 : 2048); // LIVE / MIX / MASTER
    int mult = 1;
    while (sampleRate / (48000.0 * mult) > 1.45) mult *= 2;
    return base * mult;
}

//==============================================================================
class SpaceEngine
{
public:
    static constexpr int kMaxBands = 128;

    void prepare (double sampleRate, int fftSize)
    {
        sr = sampleRate;
        N = fftSize;
        mask = N - 1;
        hop = N / 4;
        fft.prepare (N);

        const size_t n = (size_t) N, half = n / 2 + 1;
        win.resize (n);
        double wsum = 0;
        for (size_t t = 0; t < n; ++t)
        {
            win[t] = (float) std::sqrt (0.5 * (1.0 - std::cos (2.0 * kPi * (double) t / (double) N)));
            wsum += win[t];
        }
        olaScale = (float) (2.0 * hop / (double) N);         // Σ Hann com hop N/4 = 2
        powerNorm = (float) std::pow (2.0 / wsum, 2.0);      // seno de amplitude A → potência ≈ A²

        for (auto* v : { &fifoL, &fifoR, &fifoS, &accL, &accR, &aRe, &aIm, &sRe, &sIm })
            v->assign (n, 0.0f);
        for (auto* v : { &Lr, &Li, &Rr, &Ri, &pMain, &pSc, &binGain })
            v->assign (half, 0.0f);

        buildBands();
        reset();
    }

    void reset()
    {
        for (auto* v : { &fifoL, &fifoR, &fifoS, &accL, &accR })
            std::fill (v->begin(), v->end(), 0.0f);
        std::fill (std::begin (redDb), std::end (redDb), 0.0f);
        std::fill (std::begin (mainDb), std::end (mainDb), -200.0f);
        std::fill (std::begin (scDb), std::end (scDb), -200.0f);
        pos = 0;
        hopCount = 0;
        masking = 0.0f;
        avgRedDb = 0.0f;
        maxRedDbNow = 0.0f;
        scActive = false;
        frames = 0;
    }

    void setParams (const Params& p) { params = p; }
    void setAnalysisWanted (bool b) { analysisWanted = b; }

    int getLatency() const { return N; }
    int getFftSize() const { return N; }
    int getHop() const { return hop; }

    // Processa um bloco. inR / sc podem ser nullptr (main mono / sem sidechain).
    // wet* = saída processada, dry* = entrada atrasada pela mesma latência,
    // scDelayed = sidechain atrasado (para SC LISTEN). Todos com latência = N.
    void process (const float* inL, const float* inR, const float* sc,
                  float* wetL, float* wetR, float* dryL, float* dryR, float* scDelayed,
                  int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const size_t p = (size_t) pos;
            dryL[i] = fifoL[p];
            dryR[i] = fifoR[p];
            scDelayed[i] = fifoS[p];

            fifoL[p] = inL[i];
            fifoR[p] = inR != nullptr ? inR[i] : 0.0f;
            fifoS[p] = sc != nullptr ? sc[i] : 0.0f;

            wetL[i] = accL[p];  accL[p] = 0.0f;
            wetR[i] = accR[p];  accR[p] = 0.0f;

            pos = (pos + 1) & mask;
            if (++hopCount == hop)
            {
                hopCount = 0;
                processFrame (inR != nullptr);
            }
        }
    }

    // ---------------- análise (para o display e o masking meter)
    int numBands() const { return nb; }
    float bandFreq (int b) const { return bandCenter[b]; }
    float bandMainDb (int b) const { return mainDb[b]; }
    float bandScDb (int b) const { return scDb[b]; }
    float bandRedDb (int b) const { return redDb[b]; }
    float getMasking() const { return masking; }          // 0..1
    float getAverageReductionDb() const { return avgRedDb; }
    float getMaxReductionDb() const { return maxRedDbNow; }
    bool  isSidechainActive() const { return scActive; }
    unsigned getFrameCount() const { return frames; }

private:
    //--------------------------------------------------------------------------
    // Bandas: 12 por oitava até 2 kHz, 8 por oitava daí até 20 kHz
    // (mais resolução nos graves/médios). Bandas mais estreitas que um bin
    // são fundidas — nos graves a resolução real é a da FFT.
    void buildBands()
    {
        const double binHz = sr / N;
        const int lastBin = N / 2;
        nb = 0;

        int startBin = std::max (1, (int) std::ceil (20.0 / binHz));
        double f = 20.0;
        while (f < 20000.0 && nb < kMaxBands)
        {
            const double bpo = f < 2000.0 ? 12.0 : 8.0;
            f = std::min (20000.0, f * std::pow (2.0, 1.0 / bpo));
            const int endBin = std::min (lastBin + 1, (int) std::ceil (f / binHz));
            if (endBin > startBin)
            {
                bandLo[nb] = startBin;
                bandHi[nb] = endBin;
                bandCenter[nb] = (float) (binHz * std::sqrt ((double) startBin * (double) (endBin - 1)));
                bandLog2[nb] = std::log2 (bandCenter[nb]);
                ++nb;
                startBin = endBin;
            }
            if (startBin > lastBin) break;
        }

        // bin → (banda, fracção) para interpolação em log-frequência
        binBand.assign ((size_t) lastBin + 1, 0);
        binFrac.assign ((size_t) lastBin + 1, 0.0f);
        int j = 0;
        for (int k = 1; k <= lastBin; ++k)
        {
            const float lf = (float) std::log2 (k * binHz);
            while (j < nb - 2 && lf >= bandLog2[j + 1]) ++j;
            float fr = 0.0f;
            if (nb > 1)
                fr = std::clamp ((lf - bandLog2[j]) / (bandLog2[j + 1] - bandLog2[j]), 0.0f, 1.0f);
            binBand[(size_t) k] = j;
            binFrac[(size_t) k] = fr;
        }
        binBand[0] = binBand[1];
        binFrac[0] = 0.0f;
    }

    static float smoothstep (float x)
    {
        x = std::clamp (x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    //--------------------------------------------------------------------------
    void processFrame (bool stereoMain)
    {
        ++frames;
        const size_t n = (size_t) N, half = n / 2;

        double scMs = 0.0;
        for (size_t t = 0; t < n; ++t)
        {
            const size_t idx = (size_t) ((pos + (int) t) & mask);
            const float w = win[t];
            aRe[t] = fifoL[idx] * w;
            aIm[t] = fifoR[idx] * w;
            const float s = fifoS[idx];
            scMs += (double) s * s;
            sRe[t] = s * w;
            sIm[t] = 0.0f;
        }
        scMs /= (double) n;
        const float scLevelDb = (float) (10.0 * std::log10 (scMs + 1e-30));
        scActive = scLevelDb > -100.0f;

        bool reductionIdle = true;
        for (int b = 0; b < nb; ++b)
            if (redDb[b] > 1.0e-5f) { reductionIdle = false; break; }

        // Caminho rápido: sem sidechain, sem redução pendente e sem display aberto.
        // Com ganho 1 a STFT reconstrói exactamente a entrada, por isso basta a sobreposição das janelas.
        if (! scActive && reductionIdle && ! analysisWanted)
        {
            std::fill (std::begin (redDb), std::end (redDb), 0.0f);
            masking *= 0.8f;
            avgRedDb = maxRedDbNow = 0.0f;
            olaAdd();
            return;
        }

        fft.forward (aRe.data(), aIm.data());
        if (scActive)
            fft.forward (sRe.data(), sIm.data());

        // separa L e R do espectro complexo (L + iR)
        for (size_t k = 0; k <= half; ++k)
        {
            const size_t m = (n - k) & (size_t) mask;
            Lr[k] = 0.5f * (aRe[k] + aRe[m]);
            Li[k] = 0.5f * (aIm[k] - aIm[m]);
            Rr[k] = 0.5f * (aIm[k] + aIm[m]);
            Ri[k] = 0.5f * (aRe[m] - aRe[k]);
        }

        // potência de detecção do Main conforme o modo estéreo
        const StereoMode mode = stereoMain ? params.stereo : StereoMode::Left;
        for (size_t k = 0; k <= half; ++k)
        {
            float pr, pi;
            switch (mode)
            {
                case StereoMode::Left:  pr = Lr[k]; pi = Li[k]; break;
                case StereoMode::Right: pr = Rr[k]; pi = Ri[k]; break;
                case StereoMode::Mid:   pr = 0.5f * (Lr[k] + Rr[k]); pi = 0.5f * (Li[k] + Ri[k]); break;
                case StereoMode::Side:  pr = 0.5f * (Lr[k] - Rr[k]); pi = 0.5f * (Li[k] - Ri[k]); break;
                case StereoMode::Stereo:
                default:
                {
                    const float pl = Lr[k] * Lr[k] + Li[k] * Li[k], prr = Rr[k] * Rr[k] + Ri[k] * Ri[k];
                    pMain[k] = 0.5f * (pl + prr) * powerNorm;
                    continue;
                }
            }
            pMain[k] = (pr * pr + pi * pi) * powerNorm;
        }

        if (scActive)
            for (size_t k = 0; k <= half; ++k)
                pSc[k] = (sRe[k] * sRe[k] + sIm[k] * sIm[k]) * powerNorm;

        // nível por banda: média geométrica entre a potência média e o bin mais forte.
        // Só a média dilui harmónicos (voz) nas bandas largas; só o pico exagera o ruído.
        for (int b = 0; b < nb; ++b)
        {
            double m = 0.0, s = 0.0, mPk = 0.0, sPk = 0.0;
            for (int k = bandLo[b]; k < bandHi[b]; ++k)
            {
                const double pm = pMain[(size_t) k];
                m += pm;  mPk = std::max (mPk, pm);
                if (scActive) { const double ps = pSc[(size_t) k]; s += ps; sPk = std::max (sPk, ps); }
            }
            const double cnt = (double) (bandHi[b] - bandLo[b]);
            mainDb[b] = (float) (5.0 * (std::log10 (m / cnt + 1e-20) + std::log10 (mPk + 1e-20)));
            scDb[b] = scActive ? (float) (5.0 * (std::log10 (s / cnt + 1e-20) + std::log10 (sPk + 1e-20))) : -200.0f;
        }

        computeReduction (scLevelDb);
        applyGains (mode, stereoMain);

        fft.inverse (aRe.data(), aIm.data());
        olaAdd();
    }

    void olaAdd()
    {
        for (size_t t = 0; t < (size_t) N; ++t)
        {
            const size_t idx = (size_t) ((pos + (int) t) & mask);
            const float w = win[t] * olaScale;
            accL[idx] += aRe[t] * w;
            accR[idx] += aIm[t] * w;
        }
    }

    //--------------------------------------------------------------------------
    void computeReduction (float scLevelDb)
    {
        const float space = std::clamp (params.space, 0.0f, 100.0f) / 100.0f;
        const float range = std::clamp (params.rangeDb, 0.0f, 24.0f);
        const float focus = std::clamp (params.focus, 0.0f, 100.0f) / 100.0f;
        const float smooth = std::clamp (params.smooth, 0.0f, 100.0f) / 100.0f;

        // zona activa: transição suave de ~1/3 de oitava em cada limite
        const float loL = std::log2 (std::max (10.0f, params.lowCutHz));
        const float hiL = std::log2 (std::max (10.0f, params.highCutHz));
        for (int b = 0; b < nb; ++b)
        {
            const float wl = std::clamp ((bandLog2[b] - loL) * 3.0f + 0.5f, 0.0f, 1.0f);
            const float wh = std::clamp ((hiL - bandLog2[b]) * 3.0f + 0.5f, 0.0f, 1.0f);
            zoneW[b] = wl * wh;
        }

        // "Dynamic depth": sidechain baixo reduz menos (−60 dBFS RMS → 0, −36 dBFS → total)
        const float scLevelW = scActive ? smoothstep ((scLevelDb + 60.0f) / 24.0f) : 0.0f;

        float scPeak = -200.0f;
        for (int b = 0; b < nb; ++b)
            if (zoneW[b] > 0.5f) scPeak = std::max (scPeak, scDb[b]);
        if (scPeak <= -199.0f)
            for (int b = 0; b < nb; ++b) scPeak = std::max (scPeak, scDb[b]);

        // FOCUS: janela relativa ao pico do sidechain (48 dB → 12 dB) e expoente (1.2 → 3.5).
        // Regiões muito abaixo do pico da voz (abas, ruído, respiração) quase não contam.
        const float window = 48.0f - 36.0f * focus;
        const float expo = 1.2f + 2.3f * focus;

        double num = 0.0, den = 0.0;
        for (int b = 0; b < nb; ++b)
        {
            float imp = 0.0f, pres = 0.0f;
            if (scActive)
            {
                imp = std::pow (std::clamp ((scDb[b] - (scPeak - window)) / window, 0.0f, 1.0f), expo);
                // presença do Main na mesma banda: 30 dB abaixo do sidechain → 0 (já não mascara),
                // a partir de 6 dB abaixo → colisão total
                pres = smoothstep ((mainDb[b] - scDb[b] + 30.0f) / 24.0f)
                     * std::clamp ((mainDb[b] + 96.0f) / 18.0f, 0.0f, 1.0f);
            }
            colRaw[b] = imp * pres * scLevelW;
            num += (double) (imp * pres * zoneW[b]);
            den += (double) (imp * zoneW[b]);
        }

        // SMOOTH: difusão espectral [¼ ½ ¼], até 8 passagens
        const int passes = (int) std::lround (smooth * 8.0f);
        for (int p = 0; p < passes; ++p)
        {
            for (int b = 0; b < nb; ++b)
            {
                const float l = colRaw[std::max (0, b - 1)], r = colRaw[std::min (nb - 1, b + 1)];
                tmp[b] = 0.25f * l + 0.5f * colRaw[b] + 0.25f * r;
            }
            std::copy (tmp, tmp + nb, colRaw);
        }

        const float driveK = 2.5f;
        const float norm = 1.0f / std::tanh (driveK);
        const float hopSec = (float) hop / (float) sr;
        const float aAtt = std::exp (-hopSec / std::max (0.0005f, params.attackMs * 0.001f));
        const float aRel = std::exp (-hopSec / std::max (0.0005f, params.releaseMs * 0.001f));

        double sumRed = 0.0, sumW = 0.0;
        float mx = 0.0f;
        for (int b = 0; b < nb; ++b)
        {
            const float c = colRaw[b] * zoneW[b];
            const float target = range * std::tanh (driveK * space * c) * norm;
            const float a = target > redDb[b] ? aAtt : aRel;
            float r = target + (redDb[b] - target) * a;
            if (r < 1.0e-6f) r = 0.0f;
            r = std::min (r, range);          // o teto RANGE nunca é ultrapassado
            redDb[b] = r;
            mx = std::max (mx, r);
            sumRed += (double) r * zoneW[b];
            sumW += zoneW[b];
        }
        maxRedDbNow = mx;
        avgRedDb = sumW > 0.0 ? (float) (sumRed / sumW) : 0.0f;

        const float mNow = (scActive && den > 1e-9) ? (float) (num / den) * scLevelW : 0.0f;
        masking += (mNow - masking) * (mNow > masking ? 0.5f : 0.15f);

        // ganhos por bin (interpolação linear em log-frequência entre centros de banda)
        const size_t half = (size_t) N / 2;
        for (size_t k = 0; k <= half; ++k)
        {
            const int j = binBand[k];
            const int j2 = std::min (nb - 1, j + 1);
            const float r = redDb[j] + (redDb[j2] - redDb[j]) * binFrac[k];
            binGain[k] = r > 0.0f ? std::pow (10.0f, -r / 20.0f) : 1.0f;
        }
    }

    void applyGains (StereoMode mode, bool stereoMain)
    {
        const size_t n = (size_t) N, half = n / 2;
        for (size_t k = 0; k <= half; ++k)
        {
            const float g = binGain[k];
            float lr = Lr[k], li = Li[k], rr = Rr[k], ri = Ri[k];

            if (! stereoMain) { lr *= g; li *= g; }
            else switch (mode)
            {
                case StereoMode::Stereo: lr *= g; li *= g; rr *= g; ri *= g; break;
                case StereoMode::Left:   lr *= g; li *= g; break;
                case StereoMode::Right:  rr *= g; ri *= g; break;
                case StereoMode::Mid:
                {
                    const float mr = 0.5f * (lr + rr) * g, mi = 0.5f * (li + ri) * g;
                    const float sr2 = 0.5f * (lr - rr), si = 0.5f * (li - ri);
                    lr = mr + sr2; li = mi + si; rr = mr - sr2; ri = mi - si;
                    break;
                }
                case StereoMode::Side:
                {
                    const float mr = 0.5f * (lr + rr), mi = 0.5f * (li + ri);
                    const float sr2 = 0.5f * (lr - rr) * g, si = 0.5f * (li - ri) * g;
                    lr = mr + sr2; li = mi + si; rr = mr - sr2; ri = mi - si;
                    break;
                }
            }

            // volta a juntar Z = L + iR (com simetria conjugada)
            aRe[k] = lr - ri;
            aIm[k] = li + rr;
            if (k > 0 && k < half)
            {
                aRe[n - k] = lr + ri;
                aIm[n - k] = rr - li;
            }
        }
    }

    //--------------------------------------------------------------------------
    double sr = 48000.0;
    int N = 2048, mask = 2047, hop = 512;
    int pos = 0, hopCount = 0;
    float olaScale = 0.5f, powerNorm = 1.0f;
    bool analysisWanted = true, scActive = false;
    unsigned frames = 0;
    Params params;
    Fft fft;

    std::vector<float> win, fifoL, fifoR, fifoS, accL, accR, aRe, aIm, sRe, sIm;
    std::vector<float> Lr, Li, Rr, Ri, pMain, pSc, binGain;
    std::vector<int> binBand;
    std::vector<float> binFrac;

    int nb = 0;
    int bandLo[kMaxBands] {}, bandHi[kMaxBands] {};
    float bandCenter[kMaxBands] {}, bandLog2[kMaxBands] {};
    float mainDb[kMaxBands] {}, scDb[kMaxBands] {}, redDb[kMaxBands] {};
    float zoneW[kMaxBands] {}, colRaw[kMaxBands] {}, tmp[kMaxBands] {};
    float masking = 0.0f, avgRedDb = 0.0f, maxRedDbNow = 0.0f;
};

} // namespace mxs
