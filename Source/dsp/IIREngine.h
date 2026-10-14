// =============================================================================
//  EQMotion — motor IIR (modos Zero Latency e Natural)
//
//  • 24 bandas em série, biquads TDF-II em precisão dupla
//  • suavização no domínio dos parâmetros (log-frequência, dB, log-Q) por
//    sub-bloco de 16–64 amostras; coeficientes só recalculados quando mudam
//  • mudanças de forma (tipo, inclinação, colocação, ligar/desligar) passam por
//    uma rampa de mistura de 15 ms → sem cliques
//  • colocação por banda: estéreo, L, R, Mid ou Side (L/R ↔ M/S exato por amostra)
//  • lookahead opcional: o detetor vê o sinal antes do caminho principal
// =============================================================================
#pragma once
#include "Dynamics.h"

namespace eqm
{
struct GlobalDynSettings
{
    double gainScale = 1.0;
    bool   deharshOn = false;
    double deharshAmount = 0.5, deharshAttackMs = 5.0, deharshReleaseMs = 80.0;
};

class IIREngine
{
public:
    static constexpr int kMaxSub = 64;

    void prepare (double sampleRate, int subBlockSize, int maxLookaheadSamples)
    {
        fs = sampleRate;
        sub = clampv (subBlockSize, 4, kMaxSub);
        const double subRate = fs / sub;
        for (auto& st : states)
        {
            st.f.setTime (0.012, subRate); st.g.setTime (0.012, subRate); st.q.setTime (0.012, subRate);
            st.mix.setTime (0.005, subRate);
        }
        dhMix.setTime (0.005, subRate);
        dyn.prepare (fs);
        laL.prepare (maxLookaheadSamples); laR.prepare (maxLookaheadSamples);
        reset();
    }

    void setLookahead (int samples) { laL.setDelay (samples); laR.setDelay (samples); }
    int  lookahead() const { return laL.delay; }

    void reset()
    {
        for (auto& st : states)
        {
            for (auto& ch : st.sec) for (auto& s : ch) s.reset();
            st.applied = BandParams();
            st.applied.active = false;
            st.mix.reset (0.0);
            st.designed = false;
            st.primed = false;
        }
        dh.fill (Biquad());
        dhMix.reset (0.0);
        dyn.reset();
        laL.reset(); laR.reset();
    }

    // Processa no próprio buffer. sc* pode ser nullptr quando não há sidechain.
    void process (double* L, double* R, const double* scL, const double* scR, int n,
                  const BandArray& target, const GlobalDynSettings& g) noexcept
    {
        dyn.setDeharshTiming (g.deharshAttackMs, g.deharshReleaseMs);
        const bool scOk = scL != nullptr && scR != nullptr;
        double detL[kMaxSub], detR[kMaxSub];
        for (int start = 0; start < n; start += sub)
        {
            const int m = std::min (sub, n - start);
            double* l = L + start;
            double* r = R + start;
            for (int i = 0; i < m; ++i) { detL[i] = l[i]; detR[i] = r[i]; }

            dyn.process (target, detL, detR, scOk ? scL + start : nullptr, scOk ? scR + start : nullptr,
                         scOk, m, g.deharshOn, g.deharshAmount);

            if (laL.delay > 0)
                for (int i = 0; i < m; ++i) { l[i] = laL.process (l[i]); r[i] = laR.process (r[i]); }

            for (int b = 0; b < kMaxBands; ++b)
                processBand (b, target[(size_t) b], l, r, m, g.gainScale);

            processDeharsh (l, r, m, g.deharshOn);
        }
        for (auto& st : states) for (auto& ch : st.sec) for (auto& s : ch) s.sanitize();
        for (auto& s : dh) s.sanitize();
    }

    double bandDeltaDb (int b) const   { return dyn.delta (b); }
    double bandLevelDb (int b) const   { return dyn.level (b); }
    double deharshDeltaDb() const      { return dyn.deharshDeltaDb(); }
    double sampleRate() const          { return fs; }

private:
    struct BandState
    {
        BandParams applied;              // forma em uso (tipo, inclinação, colocação)
        Smoothed f, g, q, mix;           // f e q em log
        std::array<std::array<Biquad, kMaxSections>, 2> sec;
        int count = 0;
        bool designed = false, primed = false;
        double lastF = -1, lastG = 1e9, lastQ = -1;
    };

    void design (BandState& st, double freq, double gainDb, double q)
    {
        BandParams p = st.applied;
        p.freq = freq; p.q = q;
        const auto d = designDigital (p, gainDb, fs);
        st.count = d.count;
        for (int i = 0; i < d.count; ++i) { st.sec[0][(size_t) i].c = d.c[(size_t) i]; st.sec[1][(size_t) i].c = d.c[(size_t) i]; }
        st.lastF = freq; st.lastG = gainDb; st.lastQ = q;
        st.designed = true;
    }

    void processBand (int b, const BandParams& t, double* l, double* r, int m, double gainScale) noexcept
    {
        auto& st = states[(size_t) b];
        const bool wantOn = t.isProcessing();
        const bool shapeChanged = st.applied.type != t.type || st.applied.slope != t.slope
                                || st.applied.placement != t.placement;

        if (! st.primed || (st.mix.value < 1e-4 && st.mix.target <= 0.0))
        {
            // banda parada: adota a forma e os valores-alvo de imediato
            st.applied = t;
            st.f.reset (std::log (t.freq)); st.g.reset (t.gainDb * gainScale); st.q.reset (std::log (t.q));
            for (auto& ch : st.sec) for (auto& s : ch) s.reset();
            st.designed = false;
            st.primed = true;
        }

        if (wantOn && ! shapeChanged) st.mix.target = 1.0;
        else                          st.mix.target = 0.0;   // desliga (ou passa por zero para mudar de forma)

        if (st.mix.value < 1e-4 && st.mix.target <= 0.0) { st.mix.value = 0.0; return; }

        st.f.target = std::log (clampv (t.freq, kMinFreq, kMaxFreq));
        st.g.target = t.gainDb * gainScale;
        st.q.target = std::log (clampv (t.q, kMinQ, kMaxQ));
        const double fNow = std::exp (st.f.next()), gNow = st.g.next(), qNow = std::exp (st.q.next());
        const double dynDb = t.isDynamic() && ! shapeChanged ? dyn.delta (b) : 0.0;
        const double eff = clampv (gNow + dynDb, -54.0, 54.0);

        if (! st.designed || std::abs (fNow - st.lastF) > st.lastF * 1e-6 || std::abs (eff - st.lastG) > 1e-4
            || std::abs (qNow - st.lastQ) > st.lastQ * 1e-6)
            design (st, fNow, eff, qNow);

        const double m0 = st.mix.value, m1 = st.mix.next();
        const double dm = (m1 - m0) / m;
        const bool full = m0 >= 1.0 - 1e-9 && m1 >= 1.0 - 1e-9;
        auto& s0 = st.sec[0];
        auto& s1 = st.sec[1];
        const int cnt = st.count;
        auto run = [cnt] (std::array<Biquad, kMaxSections>& s, double x) noexcept
        {
            for (int k = 0; k < cnt; ++k) x = s[(size_t) k].process (x);
            return x;
        };

        for (int i = 0; i < m; ++i)
        {
            const double w = full ? 1.0 : m0 + dm * (i + 1);
            double& xl = l[i];
            double& xr = r[i];
            switch (st.applied.placement)
            {
                case Placement::Left:  xl += w * (run (s0, xl) - xl); break;
                case Placement::Right: xr += w * (run (s1, xr) - xr); break;
                case Placement::Mid:
                case Placement::Side:
                {
                    double mid = 0.5 * (xl + xr), side = 0.5 * (xl - xr);
                    if (st.applied.placement == Placement::Mid) mid += w * (run (s0, mid) - mid);
                    else                                         side += w * (run (s0, side) - side);
                    xl = mid + side; xr = mid - side;
                    break;
                }
                default:
                    xl += w * (run (s0, xl) - xl);
                    xr += w * (run (s1, xr) - xr);
                    break;
            }
        }
    }

    void processDeharsh (double* l, double* r, int m, bool on) noexcept
    {
        dhMix.target = on ? 1.0 : 0.0;
        if (dhMix.value < 1e-4 && ! on) { dhMix.value = 0; dhDesigned = false; return; }
        const double d = dyn.deharshDeltaDb();
        if (! dhDesigned || std::abs (d - dhLast) > 1e-3)
        {
            const auto dd = designDigital (deharshBand (d), d, fs);
            dh[0].c = dh[1].c = dd.c[0];
            dhLast = d; dhDesigned = true;
        }
        const double m0 = dhMix.value, m1 = dhMix.next(), dm = (m1 - m0) / m;
        for (int i = 0; i < m; ++i)
        {
            const double w = m0 + dm * (i + 1);
            l[i] += w * (dh[0].process (l[i]) - l[i]);
            r[i] += w * (dh[1].process (r[i]) - r[i]);
        }
    }

    double fs = 48000;
    int sub = 16;
    std::array<BandState, kMaxBands> states;
    DynamicsBank dyn;
    DelayLine laL, laR;
    std::array<Biquad, 2> dh;
    Smoothed dhMix;
    double dhLast = 0;
    bool dhDesigned = false;
};
} // namespace eqm
