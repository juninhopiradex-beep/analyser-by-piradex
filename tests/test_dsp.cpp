// Testes numéricos do núcleo DSP do EQMotion (sem JUCE)
//   g++ -O2 -std=c++17 -ISource tests/test_dsp.cpp -o test_dsp && ./test_dsp
#include "dsp/LinearPhaseEngine.h"
#include "dsp/Character.h"
#include <cstdio>
#include <vector>
#include <random>

using namespace eqm;
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FALHOU: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static BandParams band (FilterType t, double f, double g, double q, Slope s = Slope::dB12)
{
    BandParams b; b.active = true; b.type = t; b.freq = f; b.gainDb = g; b.q = q; b.slope = s; return b;
}

// resposta medida de um sistema a um impulso (canal L), magnitude em dB num bin
struct Measured
{
    std::vector<std::complex<double>> spec; int N; double fs;
    double db (double f) const
    {
        const double k = f * N / fs;
        const int k0 = (int) std::floor (k);
        const double t = k - k0;
        const double a = std::abs (spec[(size_t) k0]), b = std::abs (spec[(size_t) std::min (k0 + 1, N / 2)]);
        return 20 * std::log10 (std::max (1e-30, a * (1 - t) + b * t));
    }
};
static Measured measure (const std::vector<double>& ir, double fs)
{
    Measured m; m.N = (int) ir.size(); m.fs = fs;
    RealFFT fft (m.N);
    m.spec.resize ((size_t) m.N / 2 + 1);
    fft.forward (ir.data(), m.spec.data());
    return m;
}

static void testFFT()
{
    std::printf ("FFT\n");
    const int N = 256;
    RealFFT fft (N);
    std::mt19937 rng (1);
    std::uniform_real_distribution<double> u (-1, 1);
    std::vector<double> x (N), y (N);
    for (auto& v : x) v = u (rng);
    std::vector<std::complex<double>> X (N / 2 + 1);
    fft.forward (x.data(), X.data());
    double maxErr = 0;
    for (int k = 0; k <= N / 2; ++k)
    {
        std::complex<double> ref;
        for (int n = 0; n < N; ++n) ref += x[(size_t) n] * std::polar (1.0, -kTwoPi * k * n / N);
        maxErr = std::max (maxErr, std::abs (ref - X[(size_t) k]));
    }
    CHECK (maxErr < 1e-9, "FFT vs DFT erro %g", maxErr);
    fft.inverse (X.data(), y.data());
    double rt = 0; for (int n = 0; n < N; ++n) rt = std::max (rt, std::abs (x[(size_t) n] - y[(size_t) n]));
    CHECK (rt < 1e-12, "FFT ida-e-volta erro %g", rt);
}

static void testDesign()
{
    std::printf ("Desenho dos filtros (bilinear)\n");
    const double fs = 48000;
    for (double f : { 50.0, 1000.0, 10000.0, 18000.0 })
        for (double g : { -12.0, 6.0, 18.0 })
        {
            auto d = designDigital (band (FilterType::Bell, f, g, 1.0), g, fs);
            CHECK (std::abs (digitalMagnitudeDb (d, f, fs) - g) < 0.01, "bell %g Hz %g dB → %g", f, g, digitalMagnitudeDb (d, f, fs));
        }
    auto ls = designDigital (band (FilterType::LowShelf, 200, 6, 0.707), 6, fs);
    CHECK (std::abs (digitalMagnitudeDb (ls, 10, fs) - 6) < 0.05, "low shelf DC");
    CHECK (std::abs (digitalMagnitudeDb (ls, 15000, fs)) < 0.05, "low shelf agudos");
    auto hs = designDigital (band (FilterType::HighShelf, 4000, -9, 0.707), -9, fs);
    CHECK (std::abs (digitalMagnitudeDb (hs, 22000, fs) + 9) < 0.2, "high shelf agudos %g", digitalMagnitudeDb (hs, 22000, fs));
    CHECK (std::abs (digitalMagnitudeDb (hs, 20, fs)) < 0.05, "high shelf graves");

    // inclinações dos cortes: queda por oitava longe da frequência de corte
    const Slope slopes[] = { Slope::dB6, Slope::dB12, Slope::dB18, Slope::dB24, Slope::dB36, Slope::dB48, Slope::dB72, Slope::dB96 };
    for (auto s : slopes)
    {
        auto d = designDigital (band (FilterType::LowCut, 2000, 0, 0.7071, s), 0, fs);
        const double drop = (digitalMagnitudeDb (d, 2000.0 / 3, fs) - digitalMagnitudeDb (d, 1000.0 / 3, fs));
        CHECK (std::abs (drop - slopeDbPerOct (s)) < 0.6, "low cut %d dB/oct mede %.2f", slopeDbPerOct (s), drop);
        CHECK (std::abs (digitalMagnitudeDb (d, 2000, fs) + 3.01) < 0.05, "low cut %d dB/oct: −3 dB em fc (%g)", slopeDbPerOct (s), digitalMagnitudeDb (d, 2000, fs));
        auto h = designDigital (band (FilterType::HighCut, 150, 0, 0.7071, s), 0, fs);
        const double dropH = digitalMagnitudeDb (h, 450, fs) - digitalMagnitudeDb (h, 900, fs);
        CHECK (std::abs (dropH - slopeDbPerOct (s)) < 0.8, "high cut %d dB/oct mede %.2f", slopeDbPerOct (s), dropH);
    }
    auto bw = designDigital (band (FilterType::HighCut, 1000, 0, 0.7071, Slope::Brickwall), 0, fs);
    CHECK (std::abs (digitalMagnitudeDb (bw, 1000, fs) + 3) < 0.3, "brickwall −3 dB em fc (%g)", digitalMagnitudeDb (bw, 1000, fs));
    CHECK (digitalMagnitudeDb (bw, 1400, fs) < -85, "brickwall rejeição a 1,4·fc (%g)", digitalMagnitudeDb (bw, 1400, fs));
    CHECK (digitalMagnitudeDb (bw, 700, fs) > -0.5, "brickwall banda passante (%g)", digitalMagnitudeDb (bw, 700, fs));
    auto bwl = designDigital (band (FilterType::LowCut, 1000, 0, 0.7071, Slope::Brickwall), 0, fs);
    CHECK (digitalMagnitudeDb (bwl, 700, fs) < -85, "brickwall low cut rejeição (%g)", digitalMagnitudeDb (bwl, 700, fs));
    // Q do corte de 12 dB = Q pedido (ressonância)
    auto res = designDigital (band (FilterType::LowCut, 1000, 0, 4.0), 0, fs);
    CHECK (std::abs (digitalMagnitudeDb (res, 1000, fs) - 20 * std::log10 (4.0)) < 0.1, "Q do corte");
    // notch, all-pass, tilt
    auto n = designDigital (band (FilterType::Notch, 3000, 0, 4), 0, fs);
    CHECK (digitalMagnitudeDb (n, 3000, fs) < -100, "notch");
    auto ap = designDigital (band (FilterType::AllPass, 3000, 0, 1), 0, fs);
    CHECK (std::abs (digitalMagnitudeDb (ap, 500, fs)) < 1e-6 && std::abs (digitalMagnitudeDb (ap, 9000, fs)) < 1e-6, "all-pass");
    auto t = designDigital (band (FilterType::Tilt, 1000, 6, 1), 6, fs);
    CHECK (std::abs (digitalMagnitudeDb (t, 20, fs) + 3) < 0.15 && std::abs (digitalMagnitudeDb (t, 20000, fs) - 3) < 0.3, "tilt ±3 dB");
}

static void testNatural()
{
    std::printf ("Modo Natural (4× sobreamostragem) vs analógico\n");
    const double fs = 44100, fsx = fs * 4;
    double worst = 0, worstGentle = 0;
    for (double f : { 8000.0, 15000.0, 18000.0, 20000.0, 25000.0 })
        for (double q : { 0.7, 2.0, 6.0 })
            for (double g : { -12.0, 9.0 })
                for (auto type : { FilterType::Bell, FilterType::HighShelf })
                {
                    auto b = band (type, f, g, q);
                    auto d = designDigital (b, g, fsx);
                    const auto a = designAnalog (b, g);
                    for (double x = 20; x <= 20000; x *= 1.02)
                    {
                        const double e = std::abs (digitalMagnitudeDb (d, x, fsx) - analogMagnitudeDb (a, x, f));
                        worst = std::max (worst, e);
                        if (type == FilterType::Bell || q <= 2.0) worstGentle = std::max (worstGentle, e);
                    }
                }
    std::printf ("  pior erro até 20 kHz: %.3f dB (sinos e shelves com Q≤2: %.3f dB)\n", worst, worstGentle);
    CHECK (worstGentle < 0.75, "Natural: erro face ao analógico %.3f dB", worstGentle);
    CHECK (worst < 2.0, "Natural: erro máximo (shelf ressonante) %.3f dB", worst);
}

static std::vector<double> runIIR (IIREngine& e, const BandArray& bands, double fs, int irLen, int settle, std::vector<double>* right = nullptr)
{
    GlobalDynSettings g;
    std::vector<double> L ((size_t) (settle + irLen), 0.0), R = L;
    L[(size_t) settle] = 1.0; R[(size_t) settle] = 1.0;
    for (int s = 0; s < (int) L.size(); s += 256)
    {
        const int n = std::min (256, (int) L.size() - s);
        e.process (L.data() + s, R.data() + s, nullptr, nullptr, n, bands, g);
    }
    (void) fs;
    if (right) *right = std::vector<double> (R.begin() + settle, R.end());
    return std::vector<double> (L.begin() + settle, L.end());
}

static void testIIREngine()
{
    std::printf ("Motor IIR\n");
    const double fs = 48000;
    IIREngine e; e.prepare (fs, 16, 2048);
    BandArray bands {};
    // sem bandas: transparente (bit a bit)
    {
        std::vector<double> L (1000), R (1000), L0, R0;
        std::mt19937 rng (3); std::uniform_real_distribution<double> u (-1, 1);
        for (auto& v : L) v = u (rng);
        for (auto& v : R) v = u (rng);
        L0 = L; R0 = R;
        GlobalDynSettings g;
        e.process (L.data(), R.data(), nullptr, nullptr, 1000, bands, g);
        CHECK (L == L0 && R == R0, "sem bandas o motor não é transparente");
    }
    bands[0] = band (FilterType::Bell, 1000, 6, 1.5);
    bands[1] = band (FilterType::LowCut, 80, 0, 0.7071, Slope::dB24);
    bands[2] = band (FilterType::HighShelf, 8000, -4, 0.8);
    e.reset();
    auto ir = runIIR (e, bands, fs, 65536, 4800);
    auto m = measure (ir, fs);
    double worst = 0;
    for (double f = 30; f < 20000; f *= 1.05)
    {
        double ref = 0;
        for (int b = 0; b < 3; ++b) ref += digitalMagnitudeDb (designDigital (bands[(size_t) b], bands[(size_t) b].gainDb, fs), f, fs);
        worst = std::max (worst, std::abs (m.db (f) - ref));
    }
    CHECK (worst < 0.05, "IIR medido vs desenho: %.4f dB", worst);

    // colocação Mid: só o Mid é processado → com L=R (mono) o resultado é igual nos dois canais,
    // com sinal só no Side a banda não atua
    BandArray mid {};
    mid[0] = band (FilterType::Bell, 1000, 12, 1); mid[0].placement = Placement::Mid;
    IIREngine e2; e2.prepare (fs, 16, 0);
    GlobalDynSettings g;
    std::vector<double> L (24000), R (24000);
    for (int i = 0; i < 24000; ++i) { const double v = std::sin (kTwoPi * 1000 * i / fs) * 0.1; L[(size_t) i] = v; R[(size_t) i] = -v; }
    for (int s = 0; s < 24000; s += 512) e2.process (L.data() + s, R.data() + s, nullptr, nullptr, std::min (512, 24000 - s), mid, g);
    double pk = 0; for (int i = 12000; i < 24000; ++i) pk = std::max (pk, std::abs (L[(size_t) i]));
    CHECK (std::abs (pk - 0.1) < 1e-3, "banda Mid atuou em sinal só Side (pico %g)", pk);
}

static void testLinear()
{
    std::printf ("Motor de fase linear\n");
    const double fs = 48000;
    for (auto qual : { LinearQuality::Low, LinearQuality::Medium, LinearQuality::High })
    {
        LinearPhaseEngine e; e.prepare (fs, qual);
        BandArray bands {};
        GlobalDynSettings g;
        const int lat = e.latency();
        CHECK (lat == linearLatency (qual), "latência");
        // sem bandas: atraso puro
        {
            const int n = lat + 4096;
            std::vector<double> L ((size_t) n, 0.0), R ((size_t) n, 0.0);
            std::mt19937 rng (5); std::uniform_real_distribution<double> u (-1, 1);
            for (auto& v : L) v = u (rng);
            const auto L0 = L;
            for (int s = 0; s < n; s += 300) e.process (L.data() + s, R.data() + s, nullptr, nullptr, std::min (300, n - s), bands, g);
            double err = 0;
            for (int i = lat; i < n; ++i) err = std::max (err, std::abs (L[(size_t) i] - L0[(size_t) (i - lat)]));
            CHECK (err < 1e-9, "LP sem bandas não nula (erro %g)", err);
        }
        e.reset();
        bands[0] = band (FilterType::Bell, 1000, 6, 1.0);
        bands[1] = band (FilterType::HighShelf, 6000, -6, 0.7);
        const int n = lat * 2 + 32768;
        std::vector<double> L ((size_t) n, 0.0), R ((size_t) n, 0.0);
        const int at = 4096;
        L[(size_t) at] = 1.0;
        for (int s = 0; s < n; s += 512) e.process (L.data() + s, R.data() + s, nullptr, nullptr, std::min (512, n - s), bands, g);
        // pico do IR no sítio da latência e simetria (fase linear)
        int pkI = 0; double pk = 0;
        for (int i = 0; i < n; ++i) if (std::abs (L[(size_t) i]) > pk) { pk = std::abs (L[(size_t) i]); pkI = i; }
        CHECK (pkI == at + lat, "pico do IR em %d (esperado %d)", pkI, at + lat);
        double asym = 0;
        const auto spec = linearSpec (qual);
        for (int k = 1; k < spec.kernel / 2; ++k) asym = std::max (asym, std::abs (L[(size_t) (pkI + k)] - L[(size_t) (pkI - k)]));
        CHECK (asym < 1e-9, "IR não simétrico (%g)", asym);
        std::vector<double> ir (32768, 0.0);
        for (int i = 0; i < 32768; ++i) { const int src = pkI - spec.kernel / 2 + i; if (src < n) ir[(size_t) i] = L[(size_t) src]; }
        auto m = measure (ir, fs);
        double worst = 0;
        const double fmin = qual == LinearQuality::Low ? 300 : 120;
        for (double f = fmin; f < 20000; f *= 1.05)
        {
            const double ref = analogMagnitudeDb (designAnalog (bands[0], 6), f, 1000) + analogMagnitudeDb (designAnalog (bands[1], -6), f, 6000);
            worst = std::max (worst, std::abs (m.db (f) - ref));
        }
        std::printf ("  qualidade %d: latência %d amostras, erro de magnitude %.3f dB\n", (int) qual, lat, worst);
        CHECK (worst < 0.15, "LP magnitude erro %.3f dB", worst);
    }

    // M/S em fase linear: banda só no Side, sinal só no Mid → intacto (atrasado)
    LinearPhaseEngine e; e.prepare (fs, LinearQuality::Low);
    BandArray bands {};
    bands[0] = band (FilterType::Bell, 1000, 12, 1); bands[0].placement = Placement::Side;
    GlobalDynSettings g;
    const int lat = e.latency(), n = 30000;
    std::vector<double> L ((size_t) n), R ((size_t) n), L0;
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = 0.2 * std::sin (kTwoPi * 997 * i / fs);
    L0 = L;
    for (int s = 0; s < n; s += 512) e.process (L.data() + s, R.data() + s, nullptr, nullptr, std::min (512, n - s), bands, g);
    double err = 0;
    for (int i = lat + 5000; i < n; ++i) err = std::max (err, std::abs (L[(size_t) i] - L0[(size_t) (i - lat)]) + std::abs (R[(size_t) i] - L0[(size_t) (i - lat)]));
    CHECK (err < 1e-6, "LP: banda Side atuou no Mid (%g)", err);
}

static void testDynamics()
{
    std::printf ("Dinâmica\n");
    CHECK (dynamicGainDb (-40, -30, 4, 0, -6, DynDirection::Above) == 0.0, "abaixo do limiar");
    CHECK (std::abs (dynamicGainDb (-18, -30, 4, 0, -12, DynDirection::Above) + 9.0) < 1e-9, "ratio 4:1, 12 dB acima → −9");
    CHECK (std::abs (dynamicGainDb (-10, -30, 4, 0, -6, DynDirection::Above) + 6.0) < 1e-9, "limitado pelo range");
    CHECK (std::abs (dynamicGainDb (-50, -30, 2, 0, 8, DynDirection::Below) - 8.0) < 1e-9, "expansão para cima abaixo do limiar");
    CHECK (dynamicGainDb (-30, -30, 4, 6, -6, DynDirection::Above) < 0 && dynamicGainDb (-33.1, -30, 4, 6, -6, DynDirection::Above) == 0.0, "joelho suave");

    // banda dinâmica real: seno de 1 kHz a −10 dBFS, limiar −30, ratio 4, range −8 → −8 dB
    const double fs = 48000;
    IIREngine e; e.prepare (fs, 16, 0);
    BandArray bands {};
    bands[0] = band (FilterType::Bell, 1000, 0, 2);
    bands[0].dynamic = true; bands[0].thresholdDb = -30; bands[0].ratio = 4; bands[0].rangeDb = -8; bands[0].kneeDb = 0;
    bands[0].attackMs = 2; bands[0].releaseMs = 50;
    GlobalDynSettings g;
    const int n = 48000;
    std::vector<double> L ((size_t) n), R ((size_t) n);
    const double amp = dbToGain (-10);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = amp * std::sin (kTwoPi * 1000 * i / fs);
    for (int s = 0; s < n; s += 256) e.process (L.data() + s, R.data() + s, nullptr, nullptr, std::min (256, n - s), bands, g);
    double pk = 0; for (int i = n - 4800; i < n; ++i) pk = std::max (pk, std::abs (L[(size_t) i]));
    std::printf ("  redução medida: %.2f dB (esperado −8)\n", gainToDb (pk / amp));
    CHECK (std::abs (gainToDb (pk / amp) + 8.0) < 0.3, "banda dinâmica %.2f dB", gainToDb (pk / amp));

    // o mesmo no motor de fase linear
    LinearPhaseEngine lp; lp.prepare (fs, LinearQuality::Low);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = amp * std::sin (kTwoPi * 1000 * i / fs);
    for (int s = 0; s < n; s += 256) lp.process (L.data() + s, R.data() + s, nullptr, nullptr, std::min (256, n - s), bands, g);
    pk = 0; for (int i = n - 4800; i < n; ++i) pk = std::max (pk, std::abs (L[(size_t) i]));
    CHECK (std::abs (gainToDb (pk / amp) + 8.0) < 0.4, "banda dinâmica LP %.2f dB", gainToDb (pk / amp));

    // sidechain: detetor no sidechain silencioso → nenhuma redução
    IIREngine e2; e2.prepare (fs, 16, 0);
    bands[0].source = DetectorSource::Sidechain;
    std::vector<double> sc ((size_t) n, 0.0);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = amp * std::sin (kTwoPi * 1000 * i / fs);
    for (int s = 0; s < n; s += 256) e2.process (L.data() + s, R.data() + s, sc.data() + s, sc.data() + s, std::min (256, n - s), bands, g);
    pk = 0; for (int i = n - 4800; i < n; ++i) pk = std::max (pk, std::abs (L[(size_t) i]));
    CHECK (std::abs (gainToDb (pk / amp)) < 0.05, "sidechain silencioso devia deixar passar (%.2f dB)", gainToDb (pk / amp));
}

static double thd (Character c, double levelDb, double fs = 48000)
{
    CharacterStage cs; cs.prepare (fs);
    const int n = 65536;
    std::vector<double> L ((size_t) n), R ((size_t) n);
    const double f0 = fs / n * 1024;   // bin exato
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = dbToGain (levelDb) * std::sin (kTwoPi * f0 * i / fs);
    cs.process (L.data(), R.data(), n, c);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = dbToGain (levelDb) * std::sin (kTwoPi * f0 * (i + n) / fs);
    cs.process (L.data(), R.data(), n, c);   // 2.ª passagem: estado estável
    RealFFT fft (n); std::vector<std::complex<double>> X ((size_t) n / 2 + 1);
    std::vector<double> w ((size_t) n);
    for (int i = 0; i < n; ++i) w[(size_t) i] = L[(size_t) i] * (0.5 - 0.5 * std::cos (kTwoPi * i / n));
    fft.forward (w.data(), X.data());
    auto binPow = [&] (int k) { double p = 0; for (int j = k - 3; j <= k + 3; ++j) p += std::norm (X[(size_t) j]); return p; };
    const double fund = binPow (1024);
    double harm = 0; for (int h = 2; h <= 9; ++h) harm += binPow (1024 * h);
    return 10 * std::log10 (harm / fund + 1e-30);
}

static void testCharacter()
{
    std::printf ("Character\n");
    CharacterStage cs; cs.prepare (48000);
    std::vector<double> L (4096), R (4096);
    std::mt19937 rng (9); std::uniform_real_distribution<double> u (-1, 1);
    for (auto& v : L) v = u (rng);
    for (auto& v : R) v = u (rng);
    const auto L0 = L;
    cs.process (L.data(), R.data(), 4096, Character::Clean);
    CHECK (L == L0, "Clean devia ser bit a bit transparente");
    const double s = thd (Character::Subtle, -6), w = thd (Character::Warm, -6), wq = thd (Character::Warm, -24);
    std::printf ("  THD a −6 dBFS: Subtle %.1f dB, Warm %.1f dB; Warm a −24 dBFS %.1f dB\n", s, w, wq);
    CHECK (s < -45 && s > -80, "THD Subtle fora do esperado (%.1f)", s);
    CHECK (w > s + 6 && w < -25, "THD Warm fora do esperado (%.1f)", w);
}

int main()
{
    testFFT();
    testDesign();
    testNatural();
    testIIREngine();
    testLinear();
    testDynamics();
    testCharacter();
    std::printf ("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
