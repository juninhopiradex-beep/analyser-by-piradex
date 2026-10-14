// Testes das Smart Ops com sinais sintéticos conhecidos
//   g++ -O2 -std=c++17 -ISource tests/test_smartops.cpp -o test_smartops && ./test_smartops
#include "dsp/SmartOps.h"
#include "dsp/IIREngine.h"
#include <cstdio>
#include <random>

using namespace eqm;
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FALHOU: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static const double fs = 48000;

// ruído "rosa-ish" (filtro de 1.ª ordem sobre ruído branco) + fonte opcional
struct Gen
{
    std::mt19937 rng { 7 };
    std::normal_distribution<double> nd { 0.0, 1.0 };
    double b0 = 0, b1 = 0, b2 = 0;
    double pinkish()
    {   // Paul Kellet (versão económica)
        const double w = nd (rng) * 0.05;
        b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        return (b0 + b1 + b2 + w * 0.1848) * 0.5;
    }
    double white() { return nd (rng) * 0.05; }
};

static LearnResult learn (std::vector<float>& main, std::vector<float>* sc)
{
    SmartLearner l;
    l.start (fs);
    const int n = (int) main.size();
    for (int s = 0; s < n; s += 512)
        l.feed (main.data() + s, sc ? sc->data() + s : nullptr, std::min (512, n - s), sc != nullptr);
    l.stop();
    return l.result();
}

static std::vector<float> filtered (std::vector<double> x, const BandArray& bands)
{
    IIREngine e; e.prepare (fs, 16, 0);
    std::vector<double> r = x;
    GlobalDynSettings g;
    for (size_t s = 0; s < x.size(); s += 512) e.process (x.data() + s, r.data() + s, nullptr, nullptr, (int) std::min<size_t> (512, x.size() - s), bands, g);
    return std::vector<float> (x.begin(), x.end());
}

static BandParams mk (FilterType t, double f, double g, double q, Slope s = Slope::dB24)
{
    BandParams b; b.active = true; b.type = t; b.freq = f; b.gainDb = g; b.q = q; b.slope = s; return b;
}

static void print (const char* name, const SmartSuggestion& s)
{
    std::printf ("  %s → %s\n", name, s.summary.c_str());
    for (auto& b : s.bands)
        std::printf ("      tipo %d  %.0f Hz  ganho %.1f  Q %.2f  dyn %d  range %.1f  limiar %.1f  sc %d\n",
                     (int) b.type, b.freq, b.gainDb, b.q, (int) b.dynamic, b.rangeDb, b.thresholdDb, (int) b.source);
}

int main()
{
    const int n = (int) (fs * 6);
    Gen g;

    std::printf ("Deresonate\n");
    {
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] = (float) (g.pinkish() + 0.02 * std::sin (kTwoPi * 2500 * i / fs) + 0.03 * std::sin (kTwoPi * 630 * i / fs));
        auto r = learn (x, nullptr);
        CHECK (r.seconds > 5.9, "duração aprendida");
        SmartSettings s; s.op = SmartOp::Deresonate; s.amount = 1.0;
        auto sug = suggest (r, s);
        print ("ressonâncias 630 Hz e 2,5 kHz", sug);
        bool f630 = false, f2500 = false;
        for (auto& b : sug.bands)
        {
            if (std::abs (std::log2 (b.freq / 630)) < 0.05) f630 = true;
            if (std::abs (std::log2 (b.freq / 2500)) < 0.05) f2500 = true;
            CHECK (b.dynamic && b.rangeDb < 0 && b.rangeDb >= -12 && b.q >= 3 && b.q <= 18, "banda deresonate fora dos limites");
        }
        CHECK (f630 && f2500, "não encontrou as duas ressonâncias");
        CHECK (sug.bands.size() <= 4, "demasiadas bandas (%zu) para 2 ressonâncias", sug.bands.size());
        {
            IIREngine e; e.prepare (fs, 16, 0);
            BandArray bb {}; for (size_t i = 0; i < sug.bands.size(); ++i) bb[i] = sug.bands[i];
            std::vector<double> L (x.begin(), x.end()), R = L;
            GlobalDynSettings gg;
            for (int s0 = 0; s0 < n; s0 += 512) e.process (L.data() + s0, R.data() + s0, nullptr, nullptr, std::min (512, n - s0), bb, gg);
            double worst = 0; for (size_t i = 0; i < sug.bands.size(); ++i) worst = std::min (worst, e.bandDeltaDb ((int) i));
            std::printf ("  redução dinâmica nas ressonâncias: até %.1f dB\n", worst);
            CHECK (worst < -2.0, "deresonate não atua com o limiar aprendido");
        }
        // fora do intervalo escolhido não sugere nada
        s.fromHz = 3000; s.toHz = 8000;
        auto sub = suggest (r, s);
        for (auto& b : sub.bands) CHECK (b.freq >= 3000 && b.freq <= 8000, "banda fora do intervalo");
        // ruído puro: nada (ou quase nada)
        std::vector<float> y ((size_t) n);
        for (auto& v : y) v = (float) g.pinkish();
        auto r2 = learn (y, nullptr);
        s.fromHz = 20; s.toHz = 20000;
        auto none = suggest (r2, s);
        print ("ruído sem ressonâncias", none);
        CHECK (none.bands.size() <= 1, "inventou ressonâncias em ruído (%zu)", none.bands.size());
        // menos de 2 s: não sugere
        std::vector<float> shortSig (x.begin(), x.begin() + (long) fs);
        auto r3 = learn (shortSig, nullptr);
        CHECK (! suggest (r3, s).ok, "sugeriu com menos de 2 s");
    }

    std::printf ("Auto cortes\n");
    {
        std::vector<double> x ((size_t) n);
        for (auto& v : x) v = g.white() * 4;
        BandArray bands {};
        bands[0] = mk (FilterType::LowCut, 150, 0, 0.7071, Slope::dB48);
        bands[1] = mk (FilterType::HighCut, 6000, 0, 0.7071, Slope::dB48);
        auto y = filtered (x, bands);
        auto r = learn (y, nullptr);
        SmartSettings s; s.op = SmartOp::AutoCut; s.amount = 1.0;
        auto sug = suggest (r, s);
        print ("conteúdo 150 Hz – 6 kHz", sug);
        bool lc = false, hc = false;
        for (auto& b : sug.bands)
        {
            if (b.type == FilterType::LowCut)  { lc = true; CHECK (b.freq < 150 && b.freq > 40, "low cut em %.0f", b.freq); }
            if (b.type == FilterType::HighCut) { hc = true; CHECK (b.freq > 6000 && b.freq < 16000, "high cut em %.0f", b.freq); }
        }
        CHECK (lc && hc, "faltam cortes");
        // conteúdo de sub real (40 Hz): protege
        std::vector<float> z ((size_t) n);
        for (int i = 0; i < n; ++i) z[(size_t) i] = (float) (g.pinkish() + 0.3 * std::sin (kTwoPi * 40 * i / fs));
        auto rz = learn (z, nullptr);
        auto sz = suggest (rz, s);
        print ("com sub a 40 Hz", sz);
        for (auto& b : sz.bands) CHECK (! (b.type == FilterType::LowCut && b.freq > 30), "cortou o sub (%.0f Hz)", b.freq);
    }

    std::printf ("Igualar referência\n");
    {
        std::vector<double> x ((size_t) n), ref ((size_t) n);
        for (int i = 0; i < n; ++i) { x[(size_t) i] = g.pinkish(); ref[(size_t) i] = g.pinkish(); }
        BandArray bands {};
        bands[0] = mk (FilterType::Bell, 2000, 6, 1.0);
        bands[1] = mk (FilterType::LowShelf, 100, -4, 0.7);
        auto refF = filtered (ref, bands);
        std::vector<float> xf (x.begin(), x.end());
        auto r = learn (xf, &refF);
        CHECK (r.hasSidechain, "sidechain não detetado");
        SmartSettings s; s.op = SmartOp::MatchReference; s.amount = 1.0;
        auto sug = suggest (r, s);
        print ("referência = +6 dB @ 2 kHz, −4 dB shelf @ 100 Hz", sug);
        // a curva sugerida deve aproximar a diferença real
        double err = 0;
        for (double f = 60; f < 12000; f *= 1.1)
        {
            double want = analogMagnitudeDb (designAnalog (bands[0], 6), f, 2000) + analogMagnitudeDb (designAnalog (bands[1], -4), f, 100);
            double got = 0;
            for (auto& b : sug.bands) got += analogMagnitudeDb (designAnalog (b, b.gainDb), f, b.freq);
            err = std::max (err, std::abs ((want - 0) - got));
        }
        // a normalização retira o nível médio 200–5 kHz; aceitamos esse desvio constante
        std::printf ("  desvio máximo face à diferença real: %.2f dB\n", err);
        CHECK (err < 3.5, "igualação longe da referência (%.2f dB)", err);
        CHECK (sug.bands.size() <= 8, "demasiadas bandas");
        // Amount 50 % → metade do ganho
        s.amount = 0.5;
        auto half = suggest (r, s);
        double maxG = 0; for (auto& b : half.bands) maxG = std::max (maxG, std::abs (b.gainDb));
        CHECK (maxG < 4.5, "Amount não escalou (%.1f dB)", maxG);
    }

    std::printf ("Unmask\n");
    {
        std::vector<double> voc ((size_t) n);
        for (auto& v : voc) v = g.white() * 3;
        BandArray bands {};
        bands[0] = mk (FilterType::LowCut, 1500, 0, 0.7071, Slope::dB48);
        bands[1] = mk (FilterType::HighCut, 3500, 0, 0.7071, Slope::dB48);
        auto sc = filtered (voc, bands);
        std::vector<float> beat ((size_t) n);
        for (auto& v : beat) v = (float) g.pinkish();
        auto r = learn (beat, &sc);
        SmartSettings s; s.op = SmartOp::Unmask; s.amount = 0.8; smartOpDefaultRange (SmartOp::Unmask, s.fromHz, s.toHz);
        auto sug = suggest (r, s);
        print ("voz 1,5–3,5 kHz sobre beat", sug);
        CHECK (sug.ok, "unmask sem sugestão");
        for (auto& b : sug.bands)
        {
            CHECK (b.source == DetectorSource::Sidechain && b.dynamic && b.rangeDb < 0, "unmask deve usar o sidechain");
            CHECK (b.freq > 1100 && b.freq < 4500, "unmask fora da zona da voz (%.0f Hz)", b.freq);
        }
        // sem sidechain → explica
        auto r2 = learn (beat, nullptr);
        CHECK (! suggest (r2, s).ok, "unmask sem sidechain devia recusar");
    }

    std::printf ("De-ess\n");
    {
        std::vector<double> x ((size_t) n), sib ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            x[(size_t) i] = g.pinkish();
            const bool burst = (i / (int) (fs * 0.05)) % 6 == 0;   // rajadas de 50 ms
            sib[(size_t) i] = burst ? g.white() * 6 : 0.0;
        }
        BandArray bands {};
        bands[0] = mk (FilterType::Bell, 6500, 0, 1.0); bands[0].type = FilterType::BandPass; bands[0].q = 2.0;
        auto s6 = filtered (sib, bands);
        std::vector<float> mix ((size_t) n);
        for (int i = 0; i < n; ++i) mix[(size_t) i] = (float) x[(size_t) i] + s6[(size_t) i];
        auto r = learn (mix, nullptr);
        SmartSettings s; s.op = SmartOp::DeEss; s.amount = 0.7; smartOpDefaultRange (SmartOp::DeEss, s.fromHz, s.toHz);
        auto sug = suggest (r, s);
        print ("sibilância a 6,5 kHz", sug);
        CHECK (sug.ok && sug.bands.size() <= 2, "de-ess");
        if (sug.ok) CHECK (std::abs (std::log2 (sug.bands[0].freq / 6500)) < 0.35, "de-ess centrado em %.0f Hz", sug.bands[0].freq);
        if (sug.ok)
        {
            // o limiar aprendido tem de atuar nas rajadas e deixar o resto quase intacto
            IIREngine e; e.prepare (fs, 16, 0);
            BandArray bb {}; bb[0] = sug.bands[0];
            std::vector<double> L (mix.begin(), mix.end()), R = L;
            GlobalDynSettings gg;
            double maxCut = 0, restCut = 0;
            for (int s0 = 0; s0 < n; s0 += 512)
            {
                const int m = std::min (512, n - s0);
                e.process (L.data() + s0, R.data() + s0, nullptr, nullptr, m, bb, gg);
                if (s0 > fs) { const bool burst = (s0 / (int) (fs * 0.05)) % 6 == 0;
                    if (burst) maxCut = std::min (maxCut, e.bandDeltaDb (0)); else if ((s0 / (int) (fs * 0.05)) % 6 >= 3 && ((s0 + m) / (int) (fs * 0.05)) % 6 >= 3) restCut = std::min (restCut, e.bandDeltaDb (0)); }
            }
            std::printf ("  redução nas rajadas %.1f dB, fora delas %.1f dB\n", maxCut, restCut);
            CHECK (maxCut < -3.0 && restCut > -1.5, "limiar do de-ess mal calibrado");
        }
    }

    std::printf ("Equilíbrio tonal\n");
    {
        std::vector<float> x ((size_t) n);
        for (auto& v : x) v = (float) g.white();   // branco: demasiado brilhante face ao alvo
        auto r = learn (x, nullptr);
        SmartSettings s; s.op = SmartOp::TonalBalance; s.amount = 1.0;
        auto sug = suggest (r, s);
        print ("ruído branco", sug);
        double hf = 0; for (auto& b : sug.bands) hf += analogMagnitudeDb (designAnalog (b, b.gainDb), 12000, b.freq);
        CHECK (sug.ok && hf < -1.0, "devia baixar os agudos (%.1f dB)", hf);
        for (auto& b : sug.bands) CHECK (std::abs (b.gainDb) <= 4.0 + 1e-9, "tonal fora de ±4 dB");
    }

    std::printf ("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
