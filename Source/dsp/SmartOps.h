// =============================================================================
//  EQMotion — Smart Ops: análise e sugestões
//
//  Princípios: nunca toca no áudio; aprende enquanto a música toca (≥2 s);
//  devolve um conjunto pequeno de bandas explicáveis e editáveis; o Amount
//  escala a força depois da análise; cortes e Q são limitados para nunca criar
//  dezenas de notches extremos.
// =============================================================================
#pragma once
#include <atomic>
#include "Spectrum.h"
#include "Dynamics.h"
#include <cstdio>
#include <string>
#include <vector>

namespace eqm
{
// A ordem do enum é guardada nas sessões: novas operações só no fim
enum class SmartOp : int { Deresonate = 0, DeEss, AutoCut, MatchReference, Unmask, TonalBalance,
                           Mud, Boxy, Harsh, LowEnd, Presence, Air, Warmth, Hum, AutoDynamic, NumOps };

// Ordem e grupos no menu (−1 = início de grupo seguinte)
struct SmartOpGroup { const char* pt; const char* en; std::vector<SmartOp> ops; };
inline const std::vector<SmartOpGroup>& smartOpGroups()
{
    static const std::vector<SmartOpGroup> g {
        { "Corrigir", "Fix", { SmartOp::Deresonate, SmartOp::DeEss, SmartOp::Mud, SmartOp::Boxy, SmartOp::Harsh, SmartOp::Hum, SmartOp::AutoCut } },
        { "Dinâmica", "Dynamics", { SmartOp::LowEnd, SmartOp::AutoDynamic, SmartOp::Unmask } },
        { "Realçar e equilibrar", "Enhance and balance", { SmartOp::Presence, SmartOp::Air, SmartOp::Warmth, SmartOp::TonalBalance, SmartOp::MatchReference } } };
    return g;
}

// Idioma dos textos gerados (o editor/processador acerta-o com o idioma da interface)
inline std::atomic<bool>& smartEnglish() { static std::atomic<bool> e { false }; return e; }
inline const char* SL (const char* pt, const char* en) { return smartEnglish().load() ? en : pt; }

inline const char* smartOpName (SmartOp op)
{
    switch (op)
    {
        case SmartOp::Deresonate:     return "Deresonate";
        case SmartOp::DeEss:          return "De-ess";
        case SmartOp::AutoCut:        return SL ("Auto cortes (graves/agudos)", "Auto cuts (low/high)");
        case SmartOp::MatchReference: return SL ("Igualar referência", "Match reference");
        case SmartOp::Unmask:         return SL ("Unmask (abrir espaço)", "Unmask (make room)");
        case SmartOp::TonalBalance:   return SL ("Equilíbrio tonal", "Tonal balance");
        case SmartOp::Mud:            return SL ("Limpar lama (graves-médios)", "Clean mud (low mids)");
        case SmartOp::Boxy:           return SL ("Caixa e nasal (médios)", "Boxy and nasal (mids)");
        case SmartOp::Harsh:          return SL ("Domar dureza (médios-agudos)", "Tame harshness (upper mids)");
        case SmartOp::LowEnd:         return SL ("Controlar graves (boom)", "Control low end (boom)");
        case SmartOp::Presence:       return SL ("Realçar presença", "Add presence");
        case SmartOp::Air:            return SL ("Realçar ar", "Add air");
        case SmartOp::Warmth:         return SL ("Realçar calor", "Add warmth");
        case SmartOp::Hum:            return SL ("Remover zumbido (50/60 Hz)", "Remove hum (50/60 Hz)");
        case SmartOp::AutoDynamic:    return SL ("Auto dinâmico (domar picos)", "Auto dynamic (tame peaks)");
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
        case SmartOp::Mud:      from = 120;  to = 600;   break;
        case SmartOp::Boxy:     from = 400;  to = 2000;  break;
        case SmartOp::Harsh:    from = 2000; to = 8000;  break;
        case SmartOp::LowEnd:   from = 20;   to = 250;   break;
        case SmartOp::Presence: from = 1200; to = 6000;  break;
        case SmartOp::Air:      from = 7000; to = 20000; break;
        case SmartOp::Warmth:   from = 80;   to = 450;   break;
        case SmartOp::Hum:      from = 20;   to = 1000;  break;
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
inline std::string dynBandsWord (size_t n)
{
    if (smartEnglish().load()) return std::to_string (n) + (n == 1 ? " dynamic band" : " dynamic bands");
    return std::to_string (n) + (n == 1 ? " banda dinâmica" : " bandas dinâmicas");
}
inline std::string bandsWord (size_t n) { return std::to_string (n) + (n == 1 ? SL (" banda", " band") : SL (" bandas", " bands")); }

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

inline double tonalTarget (double f) { return -4.5 * std::log2 (f / 1000.0); }   // potência por bin em misturas equilibradas

// mediana de v nos índices da grelha entre f0 e f1
inline double medianBetween (const std::vector<double>& v, double f0, double f1)
{
    std::vector<double> w;
    for (int i = 0; i < (int) v.size(); ++i) { const double f = LogGrid::freq (i); if (f >= f0 && f <= f1) w.push_back (v[(size_t) i]); }
    if (w.empty()) return 0.0;
    std::nth_element (w.begin(), w.begin() + (long) w.size() / 2, w.end());
    return w[w.size() / 2];
}

// valor (dB) de v interpolado na frequência f
inline double atFreq (const std::vector<double>& v, double f)
{
    const double x = clampv (LogGrid::index (f), 0.0, (double) v.size() - 1.001);
    const int i = (int) x; const double a = x - i;
    return v[(size_t) i] * (1 - a) + v[(size_t) i + 1] * a;
}

// nível (dB) por trama de uma zona ±halfOct à volta de f, a partir das bandas de 1/12 oct
inline std::vector<double> zoneSeries (const LearnResult& r, double f, double halfOct)
{
    std::vector<double> out;
    if (r.frames == 0 || r.mainRows.empty()) return out;
    const double jc = std::log2 (f / 20.0) * 12.0;
    const int j0 = clampv ((int) std::floor (jc - halfOct * 12.0 + 0.5), 0, TwelfthBands::kCount - 1);
    const int j1 = clampv ((int) std::floor (jc + halfOct * 12.0 + 0.5), j0, TwelfthBands::kCount - 1);
    out.reserve ((size_t) r.frames);
    for (int t = 0; t < r.frames; ++t)
    {
        double p = 0;
        for (int j = j0; j <= j1; ++j) p += std::pow (10.0, r.rowDb (r.mainRows, t, j) / 10.0);
        out.push_back (10.0 * std::log10 (p + 1e-24));
    }
    return out;
}

inline BandParams dynCut (BandParams b, double rangeDb, double ratio, double atk, double rel)
{
    b.dynamic = true; b.direction = DynDirection::Above; b.rangeDb = rangeDb; b.ratio = ratio;
    b.attackMs = atk; b.releaseMs = rel; b.kneeDb = 6.0; b.detMode = DetectorMode::RMS; b.source = DetectorSource::Internal;
    return b;
}

// limiar a partir da distribuição real do detetor: atua só na fração (1 − pct) mais alta do tempo
inline void calibrate (BandParams& b, const LearnResult& r, double pct)
{
    const auto ser = r.detectorSeries (b, false);
    b.thresholdDb = std::round (LearnResult::percentile (ser, pct) * 2.0) / 2.0;
}

// Excesso local (saliência face à mediana de ±1 oitava) numa zona → 1–2 sinos dinâmicos
struct ZoneSpec { double lo, hi, minQ, maxQ, maxCut, attack, release, pct; int maxBands; const char* wherePt; const char* whereEn; const char* nonePt; const char* noneEn; };
inline void zoneExcess (const LearnResult& r, const SmartSettings& s, const ZoneSpec& z, SmartSuggestion& out)
{
    const int G = (int) r.mainDb.size();
    const auto s6 = smooth (r.mainDb, 1.0 / 6.0);
    const auto base = movingMedian (s6, LogGrid::kPerOct);   // ±1 oitava
    double maxLvl = -300; for (double v : s6) maxLvl = std::max (maxLvl, v);
    const double lo = std::max (z.lo, s.fromHz), hi = std::min (z.hi, s.toHz);
    std::vector<double> ex ((size_t) G, 0.0);
    for (int i = 0; i < G; ++i) ex[(size_t) i] = std::max (0.0, s6[(size_t) i] - base[(size_t) i]);
    std::vector<std::pair<double, int>> peaks;
    for (int i = 2; i < G - 2; ++i)
    {
        const double f = LogGrid::freq (i);
        if (f < lo || f > hi || s6[(size_t) i] < maxLvl - 45.0) continue;
        if (ex[(size_t) i] >= 1.5 && ex[(size_t) i] >= ex[(size_t) i - 1] && ex[(size_t) i] > ex[(size_t) i + 1]) peaks.push_back ({ ex[(size_t) i], i });
    }
    std::sort (peaks.rbegin(), peaks.rend());
    const double amt = std::max (0.05, clampv (s.amount, 0.0, 1.0));
    std::vector<std::string> where;
    for (auto& [e, i] : peaks)
    {
        if ((int) out.bands.size() >= z.maxBands) break;
        const double f = LogGrid::freq (i);
        bool close = false;
        for (auto& b : out.bands) if (std::abs (std::log2 (b.freq / f)) < 2.0 / 3.0) close = true;
        if (close || (! out.bands.empty() && e < 2.0)) continue;
        const double q = clampv (qFromWidth (widthOct (ex, i)), z.minQ, z.maxQ);
        auto b = dynCut (bell (f, 0, q), -clampv (e * 1.2, 1.5, z.maxCut) * amt, 3.0, z.attack, z.release);
        calibrate (b, r, z.pct);
        out.bands.push_back (b);
        where.push_back (hz (f));
    }
    if (out.bands.empty()) { out.summary = SL (z.nonePt, z.noneEn); return; }
    std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
    out.summary = dynBandsWord (out.bands.size()) + " · " + SL (z.wherePt, z.whereEn);
    for (size_t k = 0; k < where.size(); ++k) out.summary += (k ? ", " : " ") + where[k];
}

// Falta de energia face ao equilíbrio médio de misturas numa zona → realce estático suave
struct BoostSpec { double lo, hi, cLo, cHi, maxGain, q; FilterType type; const char* whatPt; const char* whatEn; const char* nonePt; const char* noneEn; };
inline void zoneDeficit (const LearnResult& r, const SmartSettings& s, const BoostSpec& z, SmartSuggestion& out)
{
    const int G = (int) r.mainDb.size();
    const auto m = smooth (r.mainDb, 1.0 / 3.0);
    std::vector<double> d ((size_t) G);
    for (int i = 0; i < G; ++i) d[(size_t) i] = tonalTarget (LogGrid::freq (i)) - m[(size_t) i];
    const double ref = medianBetween (d, 250.0, 4000.0);
    for (auto& v : d) v -= ref;
    const auto d1 = smooth (d, 1.0);
    double maxLvl = -300; for (double v : m) maxLvl = std::max (maxLvl, v);
    const double lo = std::max (z.lo, s.fromHz), hi = std::min (z.hi, s.toHz);
    int best = -1; double sum = 0; int c = 0;
    for (int i = 0; i < G; ++i)
    {
        const double f = LogGrid::freq (i);
        if (f < lo || f > hi) continue;
        if (m[(size_t) i] < maxLvl - 60.0) continue;             // sem conteúdo: não realça ruído
        sum += d[(size_t) i]; ++c;
        if (f >= z.cLo && f <= z.cHi && (best < 0 || d1[(size_t) i] > d1[(size_t) best])) best = i;
    }
    const double avg = c ? sum / c : 0.0;
    if (best < 0 || avg < 1.0 || c < 8) { out.summary = SL (z.nonePt, z.noneEn); return; }
    const double g = clampv (avg, 0.0, z.maxGain) * clampv (s.amount, 0.0, 1.0);
    if (g < 0.3) { out.summary = SL (z.nonePt, z.noneEn); return; }
    BandParams b = bell (z.type == FilterType::HighShelf ? clampv (LogGrid::freq (best) / 1.4, std::max (lo, z.cLo), std::min (hi, z.cHi)) : LogGrid::freq (best), g, z.q);
    b.type = z.type;
    out.bands.push_back (b);
    char buf[64]; std::snprintf (buf, sizeof buf, "+%.1f dB", g);
    std::string gs (buf); for (auto& ch : gs) if (ch == '.' && ! smartEnglish().load()) ch = ',';
    out.summary = std::string (SL (z.whatPt, z.whatEn)) + " " + gs + SL (" em ", " at ") + hz (b.freq)
                + SL (" · falta média de ", " · average shortfall ") + std::to_string ((int) std::round (avg)) + " dB";
}
} // namespace smartdetail

inline SmartSuggestion suggest (const LearnResult& r, const SmartSettings& s)
{
    using namespace smartdetail;
    SmartSuggestion out;
    const double amount = clampv (s.amount, 0.0, 1.0);
    if (r.frames < 8 || r.seconds < 2.0) { out.summary = SL ("Ainda a aprender — toque pelo menos 2 segundos.", "Still learning — play at least 2 seconds."); return out; }
    if (r.meanLevelDb < -75.0) { out.summary = SL ("Sinal demasiado baixo para analisar.", "Signal too low to analyse."); return out; }
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
            if (out.bands.empty()) { out.summary = SL ("Sem ressonâncias persistentes nesta zona.", "No persistent resonances in this range."); return out; }
            std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
            out.summary = dynBandsWord (out.bands.size()) + " · ";
            for (size_t i = 0; i < where.size(); ++i) out.summary += (i ? ", " : "") + where[i];
            break;
        }

        case SmartOp::DeEss:
        {
            const int T = r.frames, B = TwelfthBands::kCount;
            std::vector<int> inBand;
            for (int j = 0; j < B; ++j) if (inRange (TwelfthBands::centre (j), s)) inBand.push_back (j);
            if (inBand.empty()) { out.summary = SL ("Escolha uma zona de frequência válida.", "Choose a valid frequency range."); return out; }
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
            if (ns < std::max (2, T / 50)) { out.summary = SL ("Pouca sibilância detetada nesta passagem.", "Little sibilance detected in this passage."); return out; }
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
            if (bj < 0) { out.summary = SL ("Pouca sibilância detetada nesta passagem.", "Little sibilance detected in this passage."); return out; }
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
            out.summary = std::string (SL ("1 banda dinâmica · sibilância em ", "1 dynamic band · sibilance at ")) + hz (fc) + " · " + std::to_string ((100 * ns) / T) + SL ("% das tramas", "% of frames");
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
                        out.bands.push_back (b); parts.push_back (SL ("graves abaixo de ", "lows below ") + hz (fc));
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
                        out.bands.push_back (b); parts.push_back (SL ("agudos acima de ", "highs above ") + hz (fc));
                    }
                }
            }
            if (out.bands.empty()) { out.summary = SL ("Sem cortes úteis: há conteúdo musical nas pontas (sub e ar protegidos).", "No useful cuts: there is musical content at the extremes (sub and air protected)."); return out; }
            out.summary = SL ("Cortar ", "Cut ");
            for (size_t i = 0; i < parts.size(); ++i) out.summary += (i ? SL (" e ", " and ") : "") + parts[i];
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
                else { out.summary = SL ("Ligue a referência ao sidechain (ou escolha outra instância) e aprenda de novo.", "Route the reference to the sidechain (or choose another instance) and learn again."); return out; }
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
            if (out.bands.empty()) { out.summary = SL ("Já está muito próximo — nada a corrigir.", "Already very close — nothing to correct."); return out; }
            out.summary = bandsWord (out.bands.size()) + (s.op == SmartOp::MatchReference ? SL (" para igualar a referência", " to match the reference") : SL (" para equilibrar o tom", " to balance the tone"));
            break;
        }

        case SmartOp::Unmask:
        {
            if (! r.hasSidechain) { out.summary = SL ("Ligue o sinal a destacar (ex.: voz) ao sidechain e aprenda de novo.", "Route the signal to feature (e.g. vocal) to the sidechain and learn again."); return out; }
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
            if (out.bands.empty()) { out.summary = SL ("Pouca sobreposição entre o sinal e o sidechain.", "Little overlap between the signal and the sidechain."); return out; }
            std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
            out.summary = dynBandsWord (out.bands.size()) + SL (" pelo sidechain · ", " keyed by the sidechain · ");
            for (size_t i = 0; i < where.size(); ++i) out.summary += (i ? ", " : "") + where[i];
            break;
        }

        case SmartOp::Mud:
            zoneExcess (r, s, { 100, 700, 0.7, 2.5, 6.0, 15.0, 160.0, 0.55, 2, "lama em", "mud at",
                                "Sem acumulação nos graves-médios.", "No build-up in the low mids." }, out);
            break;
        case SmartOp::Boxy:
            zoneExcess (r, s, { 350, 2500, 1.0, 4.0, 6.0, 8.0, 100.0, 0.55, 2, "caixa/nasal em", "boxiness/nasal at",
                                "Sem caixa nem nasalidade salientes.", "No prominent boxiness or nasal tone." }, out);
            break;
        case SmartOp::Harsh:
            zoneExcess (r, s, { 1800, 9000, 1.2, 4.0, 6.0, 3.0, 70.0, 0.6, 2, "dureza em", "harshness at",
                                "Sem dureza saliente nos médios-agudos.", "No prominent harshness in the upper mids." }, out);
            break;

        case SmartOp::Presence:
            zoneDeficit (r, s, { 1200, 6000, 2000, 5000, 4.0, 0.9, FilterType::Bell, "Presença", "Presence",
                                 "Já tem presença suficiente face ao equilíbrio médio.", "Already has enough presence compared with the average balance." }, out);
            break;
        case SmartOp::Air:
            zoneDeficit (r, s, { 7000, 20000, 8000, 12000, 5.0, 0.7, FilterType::HighShelf, "Ar", "Air",
                                 "Já tem ar suficiente (ou não há conteúdo nos agudos para realçar).", "Already has enough air (or there is no top-end content to lift)." }, out);
            break;
        case SmartOp::Warmth:
            zoneDeficit (r, s, { 80, 450, 150, 300, 3.0, 0.8, FilterType::Bell, "Calor", "Warmth",
                                 "Já tem calor suficiente — se soa embolado, experimente Limpar lama.", "Already warm enough — if it sounds muddy, try Clean mud." }, out);
            break;

        case SmartOp::LowEnd:
        {
            const double lo = std::max (30.0, s.fromHz), hi = std::min (250.0, s.toHz);
            if (hi <= lo * 1.2) { out.summary = SL ("Escolha uma zona de graves (abaixo de 250 Hz).", "Choose a low-frequency range (below 250 Hz)."); return out; }
            const auto s6 = smooth (r.mainDb, 1.0 / 6.0);
            const auto base = movingMedian (s6, LogGrid::kPerOct);
            int bi = -1;
            for (int i = 0; i < G; ++i) { const double f = LogGrid::freq (i); if (f >= lo && f <= hi && (bi < 0 || s6[(size_t) i] > s6[(size_t) bi])) bi = i; }
            if (bi < 0) return out;
            const double f = LogGrid::freq (bi), bump = std::max (0.0, s6[(size_t) bi] - base[(size_t) bi]);
            std::vector<double> ex ((size_t) G); for (int i = 0; i < G; ++i) ex[(size_t) i] = std::max (0.0, s6[(size_t) i] - base[(size_t) i]);
            const double q = bump > 1.0 ? clampv (qFromWidth (widthOct (ex, bi)), 0.8, 2.5) : 1.0;
            auto b = dynCut (bell (f, 0, q), -2.0, 3.0, 10.0, 140.0);
            const auto ser = r.detectorSeries (b, false);
            const double p50 = LearnResult::percentile (ser, 0.5), p90 = LearnResult::percentile (ser, 0.9);
            const double swing = p90 - p50;
            if (swing < 3.0 && bump < 2.0) { out.summary = SL ("Graves estáveis — nada a controlar.", "Stable low end — nothing to control."); return out; }
            const double amt = std::max (0.05, clampv (s.amount, 0.0, 1.0));
            b.rangeDb = -clampv (0.6 * swing + 0.5 * bump, 2.0, 8.0) * amt;
            b.thresholdDb = std::round (LearnResult::percentile (ser, 0.6) * 2.0) / 2.0;
            out.bands.push_back (b);
            out.summary = dynBandsWord (1) + SL (" · boom em ", " · boom at ") + hz (f) + SL (" · varia ", " · swings ") + std::to_string ((int) std::round (swing)) + " dB";
            break;
        }

        case SmartOp::Hum:
        {
            const auto base = r.mainDb;
            int bestBase = 0; double bestScore = 0; std::vector<std::pair<double, double>> bestList;
            for (int f0 : { 50, 60 })
            {
                std::vector<std::pair<double, double>> list; double score = 0; int strong = 0;
                for (int k = 1; k <= 12; ++k)
                {
                    const double f = f0 * k;
                    if (f > std::min (1000.0, s.toHz) || f < s.fromHz) continue;
                    double pk = -300; for (double g2 = f * 0.985; g2 <= f * 1.015; g2 *= 1.002) pk = std::max (pk, atFreq (base, g2));
                    const double floorDb = 0.5 * (atFreq (base, f - f0 * 0.5) + atFreq (base, f + f0 * 0.5));
                    const double prom = pk - floorDb;
                    if (prom >= 5.0) { list.push_back ({ f, prom }); score += prom; if (prom >= 8.0) ++strong; }
                }
                if (strong >= 1 && list.size() >= 2 && score > bestScore) { bestScore = score; bestBase = f0; bestList = list; }
            }
            if (bestBase == 0) { out.summary = SL ("Sem zumbido de 50/60 Hz detetado.", "No 50/60 Hz hum detected."); return out; }
            // o zumbido é estacionário; uma nota de baixo sustentada varia ao longo do tempo
            {
                auto probe = bell (bestList.front().first, 0, 20.0); probe.detMode = DetectorMode::RMS;
                const auto ser = r.detectorSeries (probe, false);
                if (LearnResult::percentile (ser, 0.9) - LearnResult::percentile (ser, 0.1) > 8.0)
                { out.summary = SL ("Há um pico em 50/60 Hz mas varia como uma nota musical — não é zumbido.", "There is a 50/60 Hz peak but it moves like a musical note — not hum."); return out; }
            }
            const double amt = std::max (0.05, clampv (s.amount, 0.0, 1.0));
            std::sort (bestList.begin(), bestList.end(), [] (auto& a, auto& b) { return a.second > b.second; });
            for (auto& [f, prom] : bestList)
            {
                if (out.bands.size() >= 8) break;
                out.bands.push_back (bell (f, -clampv (prom + 3.0, 4.0, 30.0) * amt, clampv (f / 3.0, 12.0, 40.0)));
            }
            std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
            out.summary = bandsWord (out.bands.size()) + SL (" · zumbido de ", " · hum at ") + std::to_string (bestBase) + SL (" Hz e harmónicos", " Hz and harmonics");
            break;
        }

        case SmartOp::AutoDynamic:
        {
            // variação de cada zona (1/3 oct) face à variação do sinal todo: picos tonais que vêm e vão
            std::vector<double> total ((size_t) r.frames);
            for (int t = 0; t < r.frames; ++t)
            {
                double p = 0; for (int j = 0; j < TwelfthBands::kCount; ++j) p += std::pow (10.0, r.rowDb (r.mainRows, t, j) / 10.0);
                total[(size_t) t] = 10.0 * std::log10 (p + 1e-24);
            }
            const double vTot = LearnResult::percentile (total, 0.95) - LearnResult::percentile (total, 0.5);
            struct Z { double f, rel, top; };
            std::vector<Z> zs; double topMax = -300;
            for (double f = 40.0; f <= 16000.0; f *= std::pow (2.0, 1.0 / 3.0))
            {
                if (! inRange (f, s)) continue;
                auto raw = zoneSeries (r, f, 1.0 / 6.0);
                // média de potência em 3 tramas (~130 ms) para não confundir a flutuação estatística com picos
                std::vector<double> ser (raw.size());
                for (size_t t = 0; t < raw.size(); ++t)
                {
                    double p = 0; int c = 0;
                    for (size_t u = t > 0 ? t - 1 : 0; u <= std::min (raw.size() - 1, t + 1); ++u) { p += std::pow (10.0, raw[u] / 10.0); ++c; }
                    ser[t] = 10.0 * std::log10 (p / c + 1e-24);
                }
                // flutuação esperada de ruído estacionário com tão poucos bins de FFT nesta zona
                const double bins = std::max (1.0, f * (std::pow (2.0, 1.0 / 6.0) - std::pow (2.0, -1.0 / 6.0)) / (r.fs / LearnResult::kN));
                const double noiseSpread = 10.0 * std::log10 (1.0 + 1.645 / std::sqrt (bins * 1.5));
                const double top = LearnResult::percentile (ser, 0.95);
                zs.push_back ({ f, top - LearnResult::percentile (ser, 0.5) - vTot - noiseSpread, top });
                topMax = std::max (topMax, top);
            }
            std::sort (zs.begin(), zs.end(), [] (const Z& a, const Z& b) { return a.rel > b.rel; });
            const double amt = std::max (0.05, clampv (s.amount, 0.0, 1.0));
            std::vector<std::string> where;
            for (auto& z : zs)
            {
                if (out.bands.size() >= 4) break;
                if (z.rel < 3.0) break;
                if (z.top < topMax - 40.0) continue;
                bool close = false;
                for (auto& b : out.bands) if (std::abs (std::log2 (b.freq / z.f)) < 1.0) close = true;
                if (close) continue;
                auto b = dynCut (bell (z.f, 0, 1.4), -clampv (z.rel * 0.8, 2.0, 6.0) * amt, 3.0, 8.0, 120.0);
                b.kneeDb = 4.0;
                calibrate (b, r, 0.7);
                out.bands.push_back (b);
                where.push_back (hz (z.f));
            }
            if (out.bands.empty()) { out.summary = SL ("Nenhuma zona varia muito mais do que o resto do sinal.", "No range swings much more than the rest of the signal."); return out; }
            std::sort (out.bands.begin(), out.bands.end(), [] (const BandParams& a, const BandParams& b) { return a.freq < b.freq; });
            out.summary = dynBandsWord (out.bands.size()) + SL (" · picos em ", " · peaks at ");
            for (size_t k = 0; k < where.size(); ++k) out.summary += (k ? ", " : "") + where[k];
            break;
        }
        default: break;
    }
    out.ok = ! out.bands.empty();
    return out;
}
} // namespace eqm
