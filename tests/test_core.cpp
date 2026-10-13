// Testes numéricos do núcleo DSP do MIXSPACE.
//   g++ -O2 -std=c++17 -ISource tests/test_core.cpp -o test_core && ./test_core
#include "SpaceCore.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace mxs;

static int failures = 0;
static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("  %-56s %s  (obtido %.4f, esperado %s%.4f)\n", what, ok ? "OK   " : "FALHA", got, "", want);
    if (! ok) ++failures;
}

// Filtro passa-banda simples (biquad RBJ) para gerar "voz" de teste
struct Biquad
{
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    static Biquad bandpass (double fs, double f0, double q)
    {
        const double w = 2 * kPi * f0 / fs, al = std::sin (w) / (2 * q), a0 = 1 + al;
        return { al / a0, 0, -al / a0, -2 * std::cos (w) / a0, (1 - al) / a0 };
    }
    float process (float x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return (float) y; }
};

struct Signals
{
    std::vector<float> L, R, S;
};

static std::vector<float> noise (int n, unsigned seed, float amp)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, amp);
    std::vector<float> v ((size_t) n);
    for (auto& x : v) x = d (rng);
    return v;
}

// ruído passa-banda de 4.ª ordem (abas de ~12 dB/oitava, mais próximo de uma região formântica)
static std::vector<float> bandNoise (int n, unsigned seed, double fs, double f0, double q, float amp)
{
    auto v = noise (n, seed, amp);
    Biquad b1 = Biquad::bandpass (fs, f0, q), b2 = Biquad::bandpass (fs, f0, q);
    for (auto& x : v) x = b2.process (b1.process (x));
    return v;
}

static std::vector<float> sine (int n, double fs, double f, float amp)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2 * kPi * f * i / fs);
    return v;
}

struct Out
{
    std::vector<float> wetL, wetR, dryL, dryR, sc;
};

static Out run (SpaceEngine& e, const std::vector<float>& L, const std::vector<float>* R, const std::vector<float>* S, int block = 512)
{
    const int n = (int) L.size();
    Out o;
    for (auto* v : { &o.wetL, &o.wetR, &o.dryL, &o.dryR, &o.sc }) v->assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        e.process (L.data() + i, R ? R->data() + i : nullptr, S ? S->data() + i : nullptr,
                   o.wetL.data() + i, o.wetR.data() + i, o.dryL.data() + i, o.dryR.data() + i, o.sc.data() + i, m);
    }
    return o;
}

static float bandRedAt (const SpaceEngine& e, double f)
{
    int best = 0;
    for (int b = 0; b < e.numBands(); ++b)
        if (std::abs (std::log (e.bandFreq (b) / f)) < std::abs (std::log (e.bandFreq (best) / f))) best = b;
    return e.bandRedDb (best);
}

static double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from)
{
    double m = 0;
    for (size_t i = from; i < a.size(); ++i) m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}

// potência numa banda estreita à volta de f (DFT directa nas últimas amostras)
static double bandPowerDb (const std::vector<float>& x, size_t from, double fs, double f0, double f1)
{
    const size_t len = 16384;
    const size_t start = x.size() - len;
    (void) from;
    double acc = 0;
    int cnt = 0;
    for (double f = f0; f <= f1; f += fs / (double) len)
    {
        double re = 0, im = 0;
        for (size_t i = 0; i < len; ++i)
        {
            const double w = 0.5 * (1 - std::cos (2 * kPi * (double) i / (double) len));
            re += x[start + i] * w * std::cos (2 * kPi * f * (double) i / fs);
            im -= x[start + i] * w * std::sin (2 * kPi * f * (double) i / fs);
        }
        acc += re * re + im * im;
        ++cnt;
    }
    return 10 * std::log10 (acc / cnt + 1e-30);
}

int main()
{
    const double fs = 48000.0;

    std::printf ("FFT\n");
    {
        Fft f; f.prepare (2048);
        auto re = noise (2048, 1, 0.3f), im = noise (2048, 2, 0.3f);
        const auto r0 = re, i0 = im;
        f.forward (re.data(), im.data());
        f.inverse (re.data(), im.data());
        check (maxAbsDiff (re, r0, 0) < 1e-5 && maxAbsDiff (im, i0, 0) < 1e-5, "ida e volta sem erro", maxAbsDiff (re, r0, 0), 0);
    }

    std::printf ("Tamanhos de FFT por modo e sample rate\n");
    check (fftSizeFor (1, 44100) == 2048, "MIX @ 44.1 kHz = 2048", fftSizeFor (1, 44100), 2048);
    check (fftSizeFor (1, 96000) == 4096, "MIX @ 96 kHz = 4096", fftSizeFor (1, 96000), 4096);
    check (fftSizeFor (0, 48000) == 1024, "LIVE @ 48 kHz = 1024", fftSizeFor (0, 48000), 1024);
    check (fftSizeFor (2, 192000) == 16384, "MASTER @ 192 kHz = 16384", fftSizeFor (2, 192000), 16384);

    const int n = (int) fs * 6;
    auto beatL = noise (n, 10, 0.1f), beatR = noise (n, 11, 0.1f);
    auto voice = bandNoise (n, 20, fs, 1000, 2.0, 0.5f);

    std::printf ("Bandas e latência\n");
    {
        SpaceEngine e; e.prepare (fs, 2048);
        check (e.numBands() >= 64 && e.numBands() <= 128, "64–128 regiões espectrais", e.numBands(), 96);
        check (e.getLatency() == 2048, "latência = tamanho da FFT", e.getLatency(), 2048);
    }

    std::printf ("Transparência\n");
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 0; e.setParams (p);
        auto o = run (e, beatL, &beatR, &voice);
        double err = 0;
        for (int i = 2048; i < n; ++i)
            err = std::max ({ err, (double) std::abs (o.wetL[(size_t) i] - beatL[(size_t) i - 2048]),
                                   (double) std::abs (o.wetR[(size_t) i] - beatR[(size_t) i - 2048]) });
        check (err < 2e-5, "SPACE 0 = nulo com a entrada atrasada", err, 0);
        check (maxAbsDiff (o.wetL, o.dryL, 4096) < 2e-5, "seco atrasado alinhado com o processado", maxAbsDiff (o.wetL, o.dryL, 4096), 0);
    }
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 100; e.setParams (p);
        e.setAnalysisWanted (false);
        auto o = run (e, beatL, &beatR, nullptr);
        check (maxAbsDiff (o.wetL, o.dryL, 4096) < 2e-5, "sem sidechain: passa intacto (caminho rápido)", maxAbsDiff (o.wetL, o.dryL, 4096), 0);
        e.reset(); e.setAnalysisWanted (true);
        auto o2 = run (e, beatL, &beatR, nullptr);
        check (maxAbsDiff (o2.wetR, o2.dryR, 4096) < 2e-5, "sem sidechain: passa intacto (com display)", maxAbsDiff (o2.wetR, o2.dryR, 4096), 0);
    }

    std::printf ("Colisão espectral (voz ~1 kHz no sidechain, beat de ruído largo)\n");
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 60; p.rangeDb = 6; p.focus = 40; e.setParams (p);
        auto o = run (e, beatL, &beatR, &voice);
        const float r1k = bandRedAt (e, 1000), r100 = bandRedAt (e, 100), r8k = bandRedAt (e, 8000);
        check (r1k > 2.5f, "reduz na região da voz (1 kHz)", r1k, 2.5);
        check (r100 < 0.5f, "não mexe nos graves (100 Hz)", r100, 0.5);
        check (r8k < 0.5f, "não mexe nos agudos (8 kHz)", r8k, 0.5);
        check (e.getMaxReductionDb() <= 6.0f + 1e-4f, "nunca passa o RANGE", e.getMaxReductionDb(), 6.0);
        check (e.getMasking() > 0.3f, "masking meter detecta sobreposição", e.getMasking(), 0.3);

        const double inDb = bandPowerDb (o.dryL, 0, fs, 950, 1050);
        const double outDb = bandPowerDb (o.wetL, 0, fs, 950, 1050);
        check (inDb - outDb > 2.0, "atenuação real medida na saída a 1 kHz (dB)", inDb - outDb, 2.0);
        const double in100 = bandPowerDb (o.dryL, 0, fs, 90, 110), out100 = bandPowerDb (o.wetL, 0, fs, 90, 110);
        check (std::abs (in100 - out100) < 0.5, "100 Hz intacto na saída (dB)", in100 - out100, 0);
    }
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 100; p.rangeDb = 12; e.setParams (p);
        run (e, beatL, &beatR, &voice);
        check (e.getMaxReductionDb() <= 12.0f + 1e-4f, "SPACE 100 / RANGE 12: teto respeitado", e.getMaxReductionDb(), 12.0);
        Params p2 = p; p2.rangeDb = 3; e.setParams (p2);
        run (e, beatL, &beatR, &voice);
        check (e.getMaxReductionDb() <= 3.0f + 1e-4f, "RANGE 3 dB: teto respeitado", e.getMaxReductionDb(), 3.0);
    }
    {
        // SPACE mais alto → mais redução (monotonia)
        float prev = -1; bool mono = true;
        for (float s : { 10.0f, 25.0f, 50.0f, 100.0f })
        {
            SpaceEngine e; e.prepare (fs, 2048);
            Params p; p.space = s; e.setParams (p);
            run (e, beatL, &beatR, &voice);
            const float r = bandRedAt (e, 1000);
            std::printf ("    SPACE %3.0f%% → %.2f dB a 1 kHz (média na zona %.2f dB)\n", s, r, e.getAverageReductionDb());
            if (r < prev) mono = false;
            prev = r;
        }
        check (mono, "redução cresce com SPACE", prev, 0);
    }
    {
        // FOCUS alto → menos regiões afectadas
        auto widthAt = [&] (float focus)
        {
            SpaceEngine e; e.prepare (fs, 2048);
            Params p; p.space = 60; p.focus = focus; p.smooth = 0; e.setParams (p);
            run (e, beatL, &beatR, &voice);
            int c = 0;
            for (int b = 0; b < e.numBands(); ++b) if (e.bandRedDb (b) > 1.0f) ++c;
            return c;
        };
        const int wBroad = widthAt (0), wFocus = widthAt (100);
        std::printf ("    bandas com >1 dB: FOCUS 0%% = %d, FOCUS 100%% = %d\n", wBroad, wFocus);
        check (wFocus < wBroad, "FOCUS 100 é mais selectivo que FOCUS 0", wFocus, wBroad);
    }

    std::printf ("Voz harmónica (f0 220 Hz + formantes) sobre beat com sub a 50 Hz\n");
    {
        std::vector<float> v ((size_t) n, 0.0f);
        for (int h = 1; 220.0 * h < 8000.0; ++h)
        {
            const double ph = 0.7 * h;
            for (int i = 0; i < n; ++i) v[(size_t) i] += (float) (0.3 / h * std::sin (2 * kPi * 220.0 * h * i / fs + ph));
        }
        std::vector<float> voc ((size_t) n, 0.0f);
        for (double f : { 700.0, 1200.0, 2600.0 })
        {
            Biquad b = Biquad::bandpass (fs, f, 3.0);
            for (int i = 0; i < n; ++i) voc[(size_t) i] += b.process (v[(size_t) i]);
        }
        auto beat = noise (n, 40, 0.25f);   // beat denso (~ −12 dBFS RMS), voz a ~ −17 dBFS RMS
        const auto sub = sine (n, fs, 50, 0.4f);
        for (int i = 0; i < n; ++i) beat[(size_t) i] += sub[(size_t) i];

        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 30; e.setParams (p);
        auto o = run (e, beat, &beat, &voc);
        float mx = 0, fAt = 0;
        for (int b = 0; b < e.numBands(); ++b) if (e.bandRedDb (b) > mx) { mx = e.bandRedDb (b); fAt = e.bandFreq (b); }
        std::printf ("    SPACE 30%%: redução máxima %.2f dB a %.0f Hz, média %.2f dB\n", mx, fAt, e.getAverageReductionDb());
        check (bandRedAt (e, 50) < 0.1f, "sub de 50 Hz intacto (fora do Low Cut 80 Hz)", bandRedAt (e, 50), 0);
        check (mx > 1.0f && fAt > 500 && fAt < 3500, "abre espaço na zona dos formantes", fAt, 1200);
        const double in50 = bandPowerDb (o.dryL, 0, fs, 45, 55), out50 = bandPowerDb (o.wetL, 0, fs, 45, 55);
        check (std::abs (in50 - out50) < 0.1, "nível do sub na saída igual (dB)", in50 - out50, 0);
    }

    std::printf ("Só onde há colisão real\n");
    {
        // beat só com 100 Hz; voz a 3 kHz → nada a retirar
        auto beatLow = sine (n, fs, 100, 0.3f);
        auto voice3k = bandNoise (n, 30, fs, 3000, 2.0, 0.5f);
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 100; p.rangeDb = 12; e.setParams (p);
        auto o = run (e, beatLow, &beatLow, &voice3k);
        check (bandRedAt (e, 3000) < 0.3f, "Main sem conteúdo a 3 kHz → redução ≈ 0", bandRedAt (e, 3000), 0);
        check (maxAbsDiff (o.wetL, o.dryL, 8192) < 0.003, "saída praticamente igual à entrada", maxAbsDiff (o.wetL, o.dryL, 8192), 0);
    }
    {
        // Low/High Cut: voz a 1 kHz mas zona 2–10 kHz
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 80; p.lowCutHz = 2000; p.highCutHz = 10000; e.setParams (p);
        run (e, beatL, &beatR, &voice);
        check (bandRedAt (e, 1000) < 0.2f, "fora da zona (Low Cut 2 kHz) não actua", bandRedAt (e, 1000), 0);
    }
    {
        // sidechain muito baixo (−70 dBFS) → sem redução
        auto quiet = bandNoise (n, 21, fs, 1000, 2.0, 0.0005f);
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 100; e.setParams (p);
        run (e, beatL, &beatR, &quiet);
        check (e.getMaxReductionDb() < 0.2f, "sidechain quase silencioso não provoca redução", e.getMaxReductionDb(), 0);
    }

    std::printf ("Estéreo\n");
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 80; p.stereo = StereoMode::Mid; e.setParams (p);
        auto o = run (e, beatL, &beatR, &voice);
        double errS = 0, dM = 0;
        for (int i = 8192; i < n; ++i)
        {
            const double sOut = 0.5 * (o.wetL[(size_t) i] - o.wetR[(size_t) i]), sIn = 0.5 * (o.dryL[(size_t) i] - o.dryR[(size_t) i]);
            errS = std::max (errS, std::abs (sOut - sIn));
            dM = std::max (dM, std::abs (0.5 * (o.wetL[(size_t) i] + o.wetR[(size_t) i]) - 0.5 * (o.dryL[(size_t) i] + o.dryR[(size_t) i])));
        }
        check (errS < 1e-4, "modo MID: Side intacto", errS, 0);
        check (dM > 0.01, "modo MID: Mid processado", dM, 0.01);
    }
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 80; p.stereo = StereoMode::Left; e.setParams (p);
        auto o = run (e, beatL, &beatR, &voice);
        check (maxAbsDiff (o.wetR, o.dryR, 4096) < 2e-5, "modo LEFT: canal direito intacto", maxAbsDiff (o.wetR, o.dryR, 4096), 0);
    }
    {
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 60; e.setParams (p);
        auto o = run (e, beatL, nullptr, &voice);
        check (bandRedAt (e, 1000) > 2.0f, "Main mono funciona", bandRedAt (e, 1000), 2.0);
    }

    std::printf ("Tempo (attack / release)\n");
    {
        // voz durante 2 s e depois silêncio: a redução tem de recuperar
        auto v = voice;
        for (int i = (int) fs * 2; i < n; ++i) v[(size_t) i] = 0.0f;
        SpaceEngine e; e.prepare (fs, 2048);
        Params p; p.space = 60; p.releaseMs = 150; e.setParams (p);
        std::vector<float> part1 (beatL.begin(), beatL.begin() + (long) fs * 2);
        std::vector<float> v1 (v.begin(), v.begin() + (long) fs * 2);
        run (e, part1, nullptr, &v1);
        const float during = e.getMaxReductionDb();
        std::vector<float> part2 (beatL.begin(), beatL.begin() + (long) fs);
        std::vector<float> v2 ((size_t) fs, 0.0f);
        run (e, part2, nullptr, &v2);
        check (during > 2.0f && e.getMaxReductionDb() < 0.05f, "recupera depois de a voz parar", e.getMaxReductionDb(), 0);
    }

    std::printf ("CPU (60 s estéreo @ 48 kHz)\n");
    for (int mode = 0; mode < 3; ++mode)
    {
        const int N = fftSizeFor (mode, fs);
        SpaceEngine e; e.prepare (fs, N);
        Params p; p.space = 40; e.setParams (p);
        const int secs = 60, len = (int) fs * secs;
        std::vector<float> L ((size_t) len), R ((size_t) len), S ((size_t) len);
        for (int i = 0; i < len; ++i) { L[(size_t) i] = beatL[(size_t) (i % n)]; R[(size_t) i] = beatR[(size_t) (i % n)]; S[(size_t) i] = voice[(size_t) (i % n)]; }
        const auto t0 = std::chrono::steady_clock::now();
        run (e, L, &R, &S);
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("    %-6s FFT %5d  latência %5.1f ms  →  %.2f %% de um núcleo\n",
                     mode == 0 ? "LIVE" : mode == 1 ? "MIX" : "MASTER", N, 1000.0 * N / fs, 100.0 * el / secs);
    }

    std::printf ("\n%s (%d falha%s)\n", failures == 0 ? "TODOS OS TESTES PASSARAM" : "HÁ FALHAS", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
