// =============================================================================
//  EQMotion — Smart Ops: análise e sugestões
//
//  Princípios: nunca toca no áudio; aprende enquanto a música toca (≥2 s);
//  devolve um conjunto pequeno de bandas explicáveis e editáveis; o Amount
//  escala a força depois da análise; cortes e Q são limitados para nunca criar
//  dezenas de notches extremos.
// =============================================================================
#pragma once
#include "Spectrum.h"
#include "Dynamics.h"
#include <cstdio>
#include <string>
#include <vector>

namespace eqm
{
enum class SmartOp : int { Deresonate = 0, DeEss, AutoCut, MatchReference, Unmask, TonalBalance, NumOps };

inline const char* smartOpName (SmartOp op)
{
    switch (op)
    {
        case SmartOp::Deresonate:     return "Deresonate";
        case SmartOp::DeEss:          return "De-ess";
        case SmartOp::AutoCut:        return "Auto cortes (graves/agudos)";
        case SmartOp::MatchReference: return "Igualar referência";
        case SmartOp::Unmask:         return "Unmask (abrir espaço)";
        case SmartOp::TonalBalance:   return "Equilíbrio tonal";
        default: return "";
    }
}
inline bool smartOpNeedsSidechain (SmartOp op) { return op == SmartOp::Unmask; }
inline void smartOpDefaultRange (SmartOp op, double& from, double& to)
{
    switch (op)
    {
        case SmartOp::DeEss:  from = 4000; to = 12000; break;
        case SmartOp::Unmask: from = 120;  to = 8000;  break;
        default:              from = 20;   to = 20000; break;
    }
}

// Bandas de 1/12 de oitava (níveis por trama para limiares e sibilância)
struct TwelfthBands
{
    static constexpr int kCount = 120;
    static double centre (int j) { return 20.0 * std::pow (2.0, j / 12.0); }
    static double lowEdge (int j) { return 20.0 * std::pow (2.0, (j - 0.5) / 12.0); }
};

struct LearnResult
{
    double fs = 48000, seconds = 0;
    int frames = 0;
    bool hasSidechain = false;
    std::vector<double> mainDb, scDb;     // espetro médio na LogGrid (potência por bin, dB)
    std::vector<double> persistence;      // fração de tramas com pico local (LogGrid)
    std::vector<float>  mainRows, scRows; // frames × 120 (dB de potência da banda = média quadrática)
    std::vector<float>  mainAudio, scAudio; // áudio mono aprendido (até 90 s) para calibrar limiares com o detetor real
    static constexpr int kHop = 2048, kN = 8192;

    // Nível do detetor real da banda ao longo do tempo (um valor a cada 256 amostras)
    std::vector<double> detectorSeries (const BandParams& b, bool sidechain) const
    {
        const auto& a = sidechain ? scAudio : mainAudio;
        std::vector<double> out;
        if (a.empty()) return out;
        BandDetector d; d.prepare (fs);
        BandParams p = b; p.placement = Placement::Stereo;
        d.configure (p);
        double buf[256];
        out.reserve (a.size() / 256 + 1);
        for (size_t s = 0; s + 256 <= a.size(); s += 256)
        {
            for (int i = 0; i < 256; ++i) buf[i] = a[s + (size_t) i];
            d.process (buf, buf, 256, Placement::Stereo, p.detMode);
            out.push_back (d.levelDb (p.detMode));
        }
        return out;
    }
    int frameOfSeriesIndex (size_t k) const { return (int) std::floor (((double) k * 256.0 + 128.0 - kN / 2) / kHop); }

    // limiar de Otsu (máxima variância entre classes) numa série em dB
    static double otsu (const std::vector<double>& v)
    {
        std::vector<double> x; for (double d : v) if (d > -90.0) x.push_back (d);
        if (x.size() < 8) return percentile (v, 0.5);
        const double lo = *std::min_element (x.begin(), x.end()), hi = *std::max_element (x.begin(), x.end());
        if (hi - lo < 1e-6) return lo;
        constexpr int H = 128;
        std::array<double, H> hist {};
        for (double d : x) hist[(size_t) clampv ((int) ((d - lo) / (hi - lo) * (H - 1)), 0, H - 1)] += 1.0;
        double total = 0, sumAll = 0; for (int i = 0; i < H; ++i) { total += hist[(size_t) i]; sumAll += i * hist[(size_t) i]; }
        double wB = 0, sumB = 0, best = -1; int bi = H / 2;
        for (int i = 0; i < H; ++i)
        {
            wB += hist[(size_t) i]; if (wB == 0) continue;
            const double wF = total - wB; if (wF == 0) break;
            sumB += i * hist[(size_t) i];
            const double mB = sumB / wB, mF = (sumAll - sumB) / wF, between = wB * wF * (mB - mF) * (mB - mF);
            if (between > best) { best = between; bi = i; }
        }
        return lo + (bi + 0.5) / (H - 1) * (hi - lo);
    }

    static double percentile (std::vector<double> v, double pct)
    {
        if (v.empty()) return -60.0;
        std::sort (v.begin(), v.end());
        return v[(size_t) clampv ((int) std::round (pct * (v.size() - 1)), 0, (int) v.size() - 1)];
    }
    double meanLevelDb = -200;

    double rowDb (const std::vector<float>& rows, int frame, int band) const { return rows[(size_t) (frame * TwelfthBands::kCount + band)]; }

    // Nível (dB RMS) de uma zona centrada em f com Q, ao longo do tempo → percentil
    double bandLevelPercentile (bool sc, double f, double q, double pct) const
    {
        const auto& rows = sc ? scRows : mainRows;
        if (frames == 0 || rows.empty()) return -60.0;
        const double bwOct = 2.0 * std::asinh (1.0 / (2.0 * std::max (0.2, q))) / std::log (2.0);
        const double jc = std::log2 (f / 20.0) * 12.0;
        int j0 = (int) std::floor (jc - bwOct * 6.0 + 0.5), j1 = (int) std::floor (jc + bwOct * 6.0 + 0.5);
        j0 = clampv (j0, 0, TwelfthBands::kCount - 1); j1 = clampv (std::max (j1, j0), 0, TwelfthBands::kCount - 1);
        std::vector<double> lv;
        lv.reserve ((size_t) frames);
        for (int t = 0; t < frames; ++t)
        {
            double p = 0;
            for (int j = j0; j <= j1; ++j) p += std::pow (10.0, rowDb (rows, t, j) / 10.0);
            const double db = 10.0 * std::log10 (p + 1e-24);
            if (db > -90.0) lv.push_back (db);
        }
        if (lv.empty()) return -60.0;
        std::sort (lv.begin(), lv.end());
        return lv[(size_t) clampv ((int) (pct * (lv.size() - 1)), 0, (int) lv.size() - 1)];
    }
};

// -----------------------------------------------------------------------------
class SmartLearner
{
public:
    static constexpr int kN = 8192, kHop = 2048, kMaxFrames = 9000;

    SmartLearner()
    {
        fft.setSize (kN);
        win.resize (kN);
        double s2 = 0;
        for (int n = 0; n < kN; ++n) { win[(size_t) n] = 0.5 - 0.5 * std::cos (kTwoPi * n / kN); s2 += win[(size_t) n] * win[(size_t) n]; }
        powNorm = 2.0 / (kN * s2);   // Σ potência dos bins ≈ média quadrática do sinal
        frame.resize (kN); spec.resize (kN / 2 + 1); pw.resize (kN / 2 + 1);
        ringM.assign (kN, 0.0); ringS.assign (kN, 0.0);
    }

    void start (double sampleRate)
    {
        fs = sampleRate;
        r = LearnResult(); r.fs = fs;
        sumM.assign (kN / 2 + 1, 0.0); sumS.assign (kN / 2 + 1, 0.0);
        persist.assign ((size_t) LogGrid::size(), 0.0);
        r.mainRows.clear(); r.scRows.clear();
        r.mainAudio.clear(); r.scAudio.clear();
        r.mainAudio.reserve ((size_t) (fs * 30)); r.scAudio.reserve ((size_t) (fs * 30));
        r.mainRows.reserve ((size_t) 2000 * TwelfthBands::kCount);
        pos = 0; since = 0; filled = 0; scSeen = false; totalSamples = 0;
        std::fill (ringM.begin(), ringM.end(), 0.0); std::fill (ringS.begin(), ringS.end(), 0.0);
        running = true;
    }
    void stop() { running = false; }
    bool isRunning() const { return running; }
    double seconds() const { return totalSamples / fs; }
    int framesSoFar() const { return r.frames; }

    void feed (const float* mainMono, const float* scMono, int n, bool scAvailable)
    {
        if (! running) return;
        for (int i = 0; i < n; ++i)
        {
            ringM[(size_t) pos] = mainMono[i];
            if (r.mainAudio.size() < (size_t) (fs * 90))
            {
                r.mainAudio.push_back (mainMono[i]);
                r.scAudio.push_back (scAvailable && scMono ? scMono[i] : 0.0f);
            }
            ringS[(size_t) pos] = scAvailable && scMono ? scMono[i] : 0.0;
            pos = (pos + 1) % kN;
            if (filled < kN) ++filled;
            if (++since >= kHop && filled >= kN) { since = 0; analyseFrame (scAvailable); }
        }
        totalSamples += n;
    }

    LearnResult result() const
    {
        LearnResult out = r;
        out.seconds = totalSamples / fs;
        out.hasSidechain = scSeen;
        if (r.frames > 0)
        {
            std::vector<double> m (sumM.size()), s (sumS.size());
            for (size_t k = 0; k < m.size(); ++k) { m[k] = sumM[k] / r.frames; s[k] = sumS[k] / r.frames; }
            binsToGrid (m.data(), (int) m.size(), fs, kN, out.mainDb);
            binsToGrid (s.data(), (int) s.size(), fs, kN, out.scDb);
            out.persistence.resize (persist.size());
            for (size_t i = 0; i < persist.size(); ++i) out.persistence[i] = persist[i] / r.frames;
            double tot = 0; for (size_t k = 1; k < m.size(); ++k) tot += m[k];
            out.meanLevelDb = 10.0 * std::log10 (tot + 1e-24);
        }
        return out;
    }

private:
    void bandsOf (const std::vector<double>& p, std::vector<float>& rows)
    {
        const double binHz = fs / kN;
        for (int j = 0; j < TwelfthBands::kCount; ++j)
        {
            int k0 = (int) std::ceil (TwelfthBands::lowEdge (j) / binHz), k1 = (int) std::ceil (TwelfthBands::lowEdge (j + 1) / binHz) - 1;
            k0 = std::max (1, k0); k1 = std::min ((int) p.size() - 1, std::max (k1, k0));
            double s = 0; for (int k = k0; k <= k1; ++k) s += p[(size_t) k];
            rows.push_back ((float) (10.0 * std::log10 (s + 1e-24)));
        }
    }

    void analyseFrame (bool scAvailable)
    {
        if (r.frames >= kMaxFrames) return;
        // principal
        for (int n = 0; n < kN; ++n) frame[(size_t) n] = ringM[(size_t) ((pos + n) % kN)] * win[(size_t) n];
        fft.forward (frame.data(), spec.data());
        for (size_t k = 0; k < pw.size(); ++k) { pw[k] = std::norm (spec[k]) * powNorm; sumM[k] += pw[k]; }
        bandsOf (pw, r.mainRows);
        // persistência de picos estreitos (pico local ≥3 dB acima da mediana de ±1/6 oct)
        binsToGrid (pw.data(), (int) pw.size(), fs, kN, gridTmp);
        const int G = (int) gridTmp.size();
        for (int i = 2; i < G - 2; ++i)
        {
            const double v = gridTmp[(size_t) i];
            if (v < gridTmp[(size_t) i - 1] || v < gridTmp[(size_t) i + 1]) continue;
            med.clear();
            for (int j = std::max (0, i - 8); j <= std::min (G - 1, i + 8); ++j) med.push_back (gridTmp[(size_t) j]);
            std::nth_element (med.begin(), med.begin() + (long) med.size() / 2, med.end());
            if (v - med[med.size() / 2] > 3.0 && v > -100.0) persist[(size_t) i] += 1.0;
        }
        // sidechain
        if (scAvailable)
        {
            for (int n = 0; n < kN; ++n) frame[(size_t) n] = ringS[(size_t) ((pos + n) % kN)] * win[(size_t) n];
            fft.forward (frame.data(), spec.data());
            double e = 0;
            for (size_t k = 0; k < pw.size(); ++k) { pw[k] = std::norm (spec[k]) * powNorm; sumS[k] += pw[k]; e += pw[k]; }
            if (e > 1e-10) scSeen = true;
        }
        else std::fill (pw.begin(), pw.end(), 0.0);
        bandsOf (pw, r.scRows);
        ++r.frames;
    }

    double fs = 48000, powNorm = 1;
    RealFFT fft;
    std::vector<double> win, frame, pw, ringM, ringS, sumM, sumS, persist, gridTmp, med;
    std::vector<std::complex<double>> spec;
    LearnResult r;
    int pos = 0, since = 0, filled = 0;
    long long totalSamples = 0;
    bool running = false, scSeen = false;
};

// -----------------------------------------------------------------------------
//  Sugestões
// -----------------------------------------------------------------------------
struct SmartSettings
{
    SmartOp op = SmartOp::Deresonate;
    double fromHz = 20, toHz = 20000, amount = 1.0;
    std::vector<double> referenceDb;      // referência alternativa (outra instância), LogGrid
};

struct SmartSuggestion
{
    bool ok = false;
    std::vector<BandParams> bands;
    std::string summary;                  // texto para o painel (português)
};

namespace smartdetail
{
inline std::vector<double> smooth (const std::vector<double>& v, double octaves)
{
    const int G = (int) v.size(), h = std::max (1, (int) std::round (octaves * LogGrid::kPerOct * 0.5));
    std::vector<double> out ((size_t) G);
    for (int i = 0; i < G; ++i)
    {
        double s = 0; int c = 0;
        for (int j = std::max (0, i - h); j <= std::min (G - 1, i + h); ++j) { s += v[(size_t) j]; ++c; }
        out[(size_t) i] = s / c;
    }
    return out;
}
inline std::vector<double> movingMedian (const std::vector<double>& v, int half)
{
    const int G = (int) v.size();
    std::vector<double> out ((size_t) G), w;
    for (int i = 0; i < G; ++i)
    {
        w.assign (v.begin() + std::max (0, i - half), v.begin() + std::min (G, i + half + 1));
        std::nth_element (w.begin(), w.begin() + (long) w.size() / 2, w.end());
        out[(size_t) i] = w[w.size() / 2];
    }
    return out;
}
inline bool inRange (double f, const SmartSettings& s) { return f >= s.fromHz && f <= s.toHz; }
inline std::string hz (double f)
{
    char b[32];
    if (f >= 1000) std::snprintf (b, sizeof b, "%.1f kHz", f / 1000.0); else std::snprintf (b, sizeof b, "%.0f Hz", f);
    std::string s (b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
inline std::string bandsWord (size_t n) { return std::to_string (n) + (n == 1 ? " banda" : " bandas"); }

// largura (oitavas) à metade da proeminência à volta de um pico
inline double widthOct (const std::vector<double>& p, int i, double fraction = 0.5)
{
    const double lvl = p[(size_t) i] * fraction;
    int a = i, b = i;
    while (a > 0 && p[(size_t) a] > lvl) --a;
    while (b < (int) p.size() - 1 && p[(size_t) b] > lvl) ++b;
    return std::max (1, b - a) / (double) LogGrid::kPerOct;
}
inline double qFromWidth (double octaves) { const double t = std::pow (2.0, octaves); return std::sqrt (t) / (t - 1.0); }

inline BandParams bell (double f, double g, double q)
{
    BandParams b; b.active = true; b.type = FilterType::Bell; b.freq = f; b.gainDb = g; b.q = q; return b;
}
inline double curveDb (const BandParams& b, double f) { return analogMagnitudeDb (designAnalog (b, b.gainDb), f, b.freq); }

// ajusta bandas estáticas a um resíduo (dB na LogGrid): shelves para a tendência, depois sinos nos maiores desvios
inline std::vector<BandParams> fitResidual (std::vector<double> res, const SmartSettings& s, int maxBands, double maxGain,
                                            double lowShelfHz, double highShelfHz, double minQ, double maxQ)
{
    std::vector<BandParams> out;
    const int G = (int) res.size();
    auto subtract = [&] (const BandParams& b) { for (int i = 0; i < G; ++i) res[(size_t) i] -= curveDb (b, LogGrid::freq (i)); };
    auto avg = [&] (double f0, double f1) { double t = 0; int c = 0; for (int i = 0; i < G; ++i) { const double f = LogGrid::freq (i); if (f >= f0 && f <= f1 && inRange (f, s)) { t += res[(size_t) i]; ++c; } } return c ? t / c : 0.0; };

    const double lowAvg = avg (20, lowShelfHz * 0.7);
    if (std::abs (lowAvg) > 1.0 && inRange (lowShelfHz, s))
    {
        BandParams b; b.active = true; b.type = FilterType::LowShelf; b.freq = lowShelfHz; b.q = 0.7; b.gainDb = clampv (lowAvg, -maxGain, maxGain);
        out.push_back (b); subtract (b);
    }
    const double highAvg = avg (highShelfHz * 1.4, 20000);
    if (std::abs (highAvg) > 1.0 && inRange (highShelfHz, s))
    {
        BandParams b; b.active = true; b.type = FilterType::HighShelf; b.freq = highShelfHz; b.q = 0.7; b.gainDb = clampv (highAvg, -maxGain, maxGain);
        out.push_back (b); subtract (b);
    }
    while ((int) out.size() < maxBands)
    {
        int best = -1; double bv = 0;
        for (int i = 0; i < G; ++i)
            if (inRange (LogGrid::freq (i), s) && std::abs (res[(size_t) i]) > bv) { bv = std::abs (res[(size_t) i]); best = i; }
        if (best < 0 || bv < 0.8) break;
        std::vector<double> mag ((size_t) G);
        const double sign = res[(size_t) best] > 0 ? 1.0 : -1.0;
        for (int i = 0; i < G; ++i) mag[(size_t) i] = std::max (0.0, sign * res[(size_t) i]);
        const double q = clampv (qFromWidth (widthOct (mag, best)), minQ, maxQ);
        auto b = bell (LogGrid::freq (best), clampv (res[(size_t) best], -maxGain, maxGain), q);
        out.push_back (b); subtract (b);
    }
    return out;
}
} // namespace smartdetail

inline SmartSuggestion suggest (const LearnResult& r, const SmartSettings& s)
{
    using namespace smartdetail;
    SmartSuggestion out;
    const double amount = clampv (s.amount, 0.0, 1.0);
    if (r.frames < 8 || r.seconds < 2.0) { out.summary = "Ainda a aprender — toque pelo menos 2 segundos."; return out; }
    if (r.meanLevelDb < -75.0) { out.summary = "Sinal demasiado baixo para analisar."; return out; }
    const int G = (int) r.mainDb.size();

    switch (s.op)
    {
        case SmartOp::Deresonate:
        {
            const auto base = movingMedian (r.mainDb, 16);   // ±1/3 oct
            double maxLvl = -300; for (double v : r.mainDb) maxLvl = std::max (maxLvl, v);
            std::vector<double> prom ((size_t) G);
            for (int i = 0; i < G; ++i) prom[(size_t) i] = std::max (0.0, r.mainDb[(size_t) i] - base[(size_t) i]);
            struct Cand { int i; double score, prom; };
            std::vector<Cand> cands;
            for (int i = 2; i < G - 2; ++i)
            {
                const double f = LogGrid::freq (i), p = prom[(size_t) i];
                if (! inRange (f, s) || p < 2.5 || r.mainDb[(size_t) i] < maxLvl - 50.0) continue;
                if (p < prom[(size_t) i - 1] || p < prom[(size_t) i + 1] || p < prom[(size_t) i - 2] || p < prom[(size_t) i + 2]) continue;
                double pers = 0; for (int j = i - 1; j <= i + 1; ++j) pers = std::max (pers, r.persistence[(size_t) j]);
                if (pers < 0.3) continue;
                const double w = widthOct (prom, i);
                if (w > 0.34) continue;   // formantes largos não são ressonâncias
                cands.push_back ({ i, p * pers, p });
            }
            // série harmónica (nota sustentada): penaliza membros que são múltiplos de outro candidato mais forte
            for (auto& c : cands)
            {
                const double f = LogGrid::freq (c.i);
                int related = 0;
                for (auto& o : cands)
                    if (&o != &c)
                        for (int h = 2; h <= 6; ++h)
                        {
                            const double fo = LogGrid::freq (o.i);
                            if (std::abs (f / fo - h) < 0.015 * h || std::abs (fo / f - h) < 0.015 * h) { ++related; break; }
                        }
                if (related >= 2) c.score *= 0.45;
            }
            std::sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.score > b.score; });
            std::vector<std::string> where;
            for (auto& c : cands)
            {
                if (out.bands.size() >= 8) break;
                const double f = LogGrid::freq (c.i);
                bool close = false;
                for (auto& b : out.bands) if (std::abs (std::log2 (b.freq / f)) < 1.0 / 6.0) close = true;
                if (close) continue;
                const double q = clampv (qFromWidth (widthOct (prom, c.i)), 3.0, 18.0);
                BandParams b = bell (f, 0.0, q);
                b.dynamic = true; b.direction = DynDirection::Above;
                // corte ≈ proeminência quando a ressonância está presente (nível mediano do detetor real)
                const double P = clampv (c.prom, 1.5, 12.0);
                b.rangeDb = -P * std::max (0.05, amount);
                b.ratio = 4.0; b.attackMs = 3.0; b.releaseMs = 60.0; b.kneeDb = 6.0; b.detMode = DetectorMode::RMS;
                const double med = LearnResult::percentile (r.detectorSeries (b, false), 0.5);
                b.thresholdDb = std::round ((med - 0.8 * P * 4.0 / 3.0) * 2.0) / 2.0;
                out.bands.push_back (b);
                if (where.size() < 4) where.push_back (hz (f));
            }
            if (out.bands.empty()) { out.summary = "Sem ressonâncias persistentes nesta zona."; return out; }
            std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
            out.summary = bandsWord (out.bands.size()) + " dinâmicas · ";
            for (size_t i = 0; i < where.size(); ++i) out.summary += (i ? ", " : "") + where[i];
            break;
        }

        case SmartOp::DeEss:
        {
            const int T = r.frames, B = TwelfthBands::kCount;
            std::vector<int> inBand;
            for (int j = 0; j < B; ++j) if (inRange (TwelfthBands::centre (j), s)) inBand.push_back (j);
            if (inBand.empty()) { out.summary = "Escolha uma zona de frequência válida."; return out; }
            std::vector<double> ratio ((size_t) T), level ((size_t) T);
            for (int t = 0; t < T; ++t)
            {
                double pb = 0, pt = 0;
                for (int j = 0; j < B; ++j) { const double p = std::pow (10.0, r.rowDb (r.mainRows, t, j) / 10.0); pt += p; }
                for (int j : inBand) pb += std::pow (10.0, r.rowDb (r.mainRows, t, j) / 10.0);
                level[(size_t) t] = 10.0 * std::log10 (pb + 1e-24);
                ratio[(size_t) t] = level[(size_t) t] - 10.0 * std::log10 (pt + 1e-24);
            }
            auto sortedR = ratio; std::sort (sortedR.begin(), sortedR.end());
            const double medR = sortedR[sortedR.size() / 2];
            double maxL = -300; for (double v : level) maxL = std::max (maxL, v);
            std::vector<bool> sib ((size_t) T);
            int ns = 0;
            for (int t = 0; t < T; ++t) { sib[(size_t) t] = ratio[(size_t) t] > medR + 6.0 && level[(size_t) t] > maxL - 30.0; ns += sib[(size_t) t]; }
            if (ns < std::max (2, T / 50)) { out.summary = "Pouca sibilância detetada nesta passagem."; return out; }
            // centro da sibilância: onde as tramas sibilantes têm mais energia, entre as bandas
            // em que claramente sobem face às restantes (+3 dB)
            std::vector<double> diff ((size_t) B, -100.0), sibAvg ((size_t) B, -300.0);
            for (int j : inBand)
            {
                double a = 0, b = 0;
                for (int t = 0; t < T; ++t) (sib[(size_t) t] ? a : b) += std::pow (10.0, r.rowDb (r.mainRows, t, j) / 10.0);
                sibAvg[(size_t) j] = 10.0 * std::log10 (a / ns + 1e-24);
                diff[(size_t) j] = sibAvg[(size_t) j] - 10.0 * std::log10 (b / std::max (1, T - ns) + 1e-24);
            }
            int bj = -1;
            for (int j : inBand) if (diff[(size_t) j] > 3.0 && (bj < 0 || sibAvg[(size_t) j] > sibAvg[(size_t) bj])) bj = j;
            if (bj < 0) { out.summary = "Pouca sibilância detetada nesta passagem."; return out; }
            int a = bj, b = bj;
            while (a > inBand.front() && sibAvg[(size_t) a - 1] > sibAvg[(size_t) bj] - 3.0) --a;
            while (b < inBand.back() && sibAvg[(size_t) b + 1] > sibAvg[(size_t) bj] - 3.0) ++b;
            const double wOct = (b - a + 1) / 12.0;
            const double fc = TwelfthBands::centre ((a + b) / 2);
            BandParams band;
            const bool broadTop = b == inBand.back() && fc > 9000.0;
            if (broadTop) { band = bell (fc, 0, 0.7); band.type = FilterType::HighShelf; band.freq = TwelfthBands::centre (a); }
            else band = bell (fc, 0, clampv (qFromWidth (wOct), 1.0, 4.0));
            band.dynamic = true; band.direction = DynDirection::Above;
            band.rangeDb = -std::max (1.5, 10.0 * amount);
            band.ratio = 4.0; band.attackMs = 1.0; band.releaseMs = 50.0; band.kneeDb = 4.0; band.detMode = DetectorMode::RMS;
            // limiar a partir do detetor real: acima do fundo, abaixo dos picos sibilantes
            {
                const auto ser = r.detectorSeries (band, false);
                // separação de Otsu entre "fundo" e "sibilantes" na distribuição (dB) do detetor
                band.thresholdDb = std::round (std::max (LearnResult::otsu (ser), LearnResult::percentile (ser, 0.5) + 2.0) * 2.0) / 2.0;
            }
            out.bands.push_back (band);
            out.summary = std::string ("1 banda dinâmica · sibilância em ") + hz (fc) + " · " + std::to_string ((100 * ns) / T) + "% das tramas";
            break;
        }

        case SmartOp::AutoCut:
        {
            const auto s3 = smooth (r.mainDb, 1.0 / 3.0);
            double maxLvl = -300;
            for (int i = 0; i < G; ++i) { const double f = LogGrid::freq (i); if (f >= 30 && f <= 16000) maxLvl = std::max (maxLvl, s3[(size_t) i]); }
            std::vector<std::string> parts;
            // corte de graves
            int iL = -1;
            for (int i = 0; i < G; ++i) if (s3[(size_t) i] >= maxLvl - 24.0) { iL = i; break; }
            if (iL >= 0)
            {
                const double fL = LogGrid::freq (iL);
                double below = -300;
                for (int i = 0; i < G; ++i) if (LogGrid::freq (i) < fL * 0.5) below = std::max (below, s3[(size_t) i]);
                if (fL > 35.0 && below < maxLvl - 30.0)
                {
                    const double fc = std::max (20.0, fL * (0.4 + 0.3 * amount));
                    if (inRange (fc, s))
                    {
                        BandParams b; b.active = true; b.type = FilterType::LowCut; b.freq = fc; b.q = 0.7071;
                        b.slope = below < maxLvl - 40.0 ? Slope::dB24 : Slope::dB12;
                        out.bands.push_back (b); parts.push_back ("graves abaixo de " + hz (fc));
                    }
                }
            }
            // corte de agudos
            int iH = -1;
            for (int i = G - 1; i >= 0; --i) if (s3[(size_t) i] >= maxLvl - 30.0) { iH = i; break; }
            if (iH >= 0)
            {
                const double fH = LogGrid::freq (iH);
                double above = -300;
                for (int i = 0; i < G; ++i) if (LogGrid::freq (i) > fH * 1.5) above = std::max (above, s3[(size_t) i]);
                if (fH < 17000.0 && above < maxLvl - 40.0)
                {
                    const double fc = std::min (20000.0, fH * (2.0 - 0.6 * amount));
                    if (inRange (fc, s))
                    {
                        BandParams b; b.active = true; b.type = FilterType::HighCut; b.freq = fc; b.q = 0.7071; b.slope = Slope::dB12;
                        out.bands.push_back (b); parts.push_back ("agudos acima de " + hz (fc));
                    }
                }
            }
            if (out.bands.empty()) { out.summary = "Sem cortes úteis: há conteúdo musical nas pontas (sub e ar protegidos)."; return out; }
            out.summary = "Cortar ";
            for (size_t i = 0; i < parts.size(); ++i) out.summary += (i ? " e " : "") + parts[i];
            break;
        }

        case SmartOp::MatchReference:
        case SmartOp::TonalBalance:
        {
            std::vector<double> target;
            if (s.op == SmartOp::MatchReference)
            {
                if (! s.referenceDb.empty() && (int) s.referenceDb.size() == G) target = s.referenceDb;
                else if (r.hasSidechain) target = r.scDb;
                else { out.summary = "Ligue a referência ao sidechain (ou escolha outra instância) e aprenda de novo."; return out; }
            }
            else
            {
                target.resize ((size_t) G);   // inclinação média de misturas equilibradas: −4,5 dB/oct (potência por bin)
                for (int i = 0; i < G; ++i) target[(size_t) i] = -4.5 * std::log2 (LogGrid::freq (i) / 1000.0);
            }
            const double w = s.op == SmartOp::MatchReference ? 1.0 / 3.0 : 1.0;
            const auto a = smooth (target, w), m = smooth (r.mainDb, w);
            std::vector<double> d ((size_t) G);
            double mean = 0; int c = 0;
            for (int i = 0; i < G; ++i)
            {
                d[(size_t) i] = a[(size_t) i] - m[(size_t) i];
                const double f = LogGrid::freq (i);
                if (f >= 200 && f <= 5000) { mean += d[(size_t) i]; ++c; }
            }
            mean /= std::max (1, c);
            const double lim = s.op == SmartOp::MatchReference ? 12.0 : 6.0;
            for (int i = 0; i < G; ++i)
            {
                const double f = LogGrid::freq (i);
                d[(size_t) i] = clampv (d[(size_t) i] - mean, -lim, lim) * amount;
                if (! inRange (f, s)) d[(size_t) i] = 0;
                if (s.op == SmartOp::MatchReference && r.mainDb[(size_t) i] < -110) d[(size_t) i] = 0;   // sem sinal, sem decisão
            }
            if (s.op == SmartOp::MatchReference) out.bands = fitResidual (d, s, 8, 9.0 * std::max (0.2, amount), 110.0, 8000.0, 0.4, 4.0);
            else                                  out.bands = fitResidual (d, s, 4, 4.0 * std::max (0.2, amount), 150.0, 6000.0, 0.5, 1.0);
            if (out.bands.empty()) { out.summary = "Já está muito próximo — nada a corrigir."; return out; }
            out.summary = bandsWord (out.bands.size()) + (s.op == SmartOp::MatchReference ? " para igualar a referência" : " para equilibrar o tom");
            break;
        }

        case SmartOp::Unmask:
        {
            if (! r.hasSidechain) { out.summary = "Ligue o sinal a destacar (ex.: voz) ao sidechain e aprenda de novo."; return out; }
            const auto m = smooth (r.mainDb, 1.0 / 6.0), sc = smooth (r.scDb, 1.0 / 6.0), sc1 = smooth (r.scDb, 1.0);
            double mMax = -300, sMax = -300;
            for (int i = 0; i < G; ++i) if (inRange (LogGrid::freq (i), s)) { mMax = std::max (mMax, m[(size_t) i]); sMax = std::max (sMax, sc[(size_t) i]); }
            std::vector<double> score ((size_t) G, 0.0);
            for (int i = 0; i < G; ++i)
            {
                if (! inRange (LogGrid::freq (i), s)) continue;
                const double wS = clampv ((sc[(size_t) i] - sMax + 18.0) / 18.0, 0.0, 1.0);
                const double wM = clampv ((m[(size_t) i] - mMax + 24.0) / 24.0, 0.0, 1.0);
                const double sal = 1.0 + std::max (0.0, sc[(size_t) i] - sc1[(size_t) i]) / 6.0;
                score[(size_t) i] = wS * wM * sal;
            }
            std::vector<std::pair<double, int>> peaks;
            for (int i = 3; i < G - 3; ++i)
                if (score[(size_t) i] >= 0.25 && score[(size_t) i] >= score[(size_t) i - 1] && score[(size_t) i] > score[(size_t) i + 1])
                    peaks.push_back ({ score[(size_t) i], i });
            std::sort (peaks.rbegin(), peaks.rend());
            std::vector<std::string> where;
            for (auto& [sv, i] : peaks)
            {
                if (out.bands.size() >= 4) break;
                const double f = LogGrid::freq (i);
                bool close = false;
                for (auto& b : out.bands) if (std::abs (std::log2 (b.freq / f)) < 0.6) close = true;
                if (close) continue;
                const double q = clampv (qFromWidth (widthOct (score, i, 0.7)), 0.8, 3.0);
                BandParams b = bell (f, 0, q);
                b.dynamic = true; b.direction = DynDirection::Above; b.source = DetectorSource::Sidechain;
                b.rangeDb = -std::max (1.5, 8.0 * amount * std::min (1.0, sv));
                b.ratio = 3.0; b.attackMs = 5.0; b.releaseMs = 120.0; b.kneeDb = 6.0; b.detMode = DetectorMode::RMS;
                {   // atua quando o sidechain está ativo nessa zona (ignora silêncios)
                    auto ser = r.detectorSeries (b, true);
                    const double floorDb = LearnResult::percentile (ser, 0.1);
                    std::vector<double> act; for (double v : ser) if (v > floorDb + 6.0 && v > -80.0) act.push_back (v);
                    b.thresholdDb = std::round (LearnResult::percentile (act.empty() ? ser : act, 0.3) * 2.0) / 2.0;
                }
                out.bands.push_back (b);
                where.push_back (hz (f));
            }
            if (out.bands.empty()) { out.summary = "Pouca sobreposição entre o sinal e o sidechain."; return out; }
            std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
            out.summary = bandsWord (out.bands.size()) + " dinâmicas pelo sidechain · ";
            for (size_t i = 0; i < where.size(); ++i) out.summary += (i ? ", " : "") + where[i];
            break;
        }
        default: break;
    }
    out.ok = ! out.bands.empty();
    return out;
}
} // namespace eqm
