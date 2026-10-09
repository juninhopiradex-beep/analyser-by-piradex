// Testes numéricos do núcleo DSP: g++ -O2 -std=c++17 -I../Source test_core.cpp -o test_core
#include "CurveCore.h"
#include <cstdio>
#include <cassert>

using namespace pca;

struct Biquad
{
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    static Biquad peak (double fs, double f0, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f0 / fs, al = std::sin (w) / (2 * q);
        const double a0 = 1 + al / A;
        return { (1 + al * A) / a0, -2 * std::cos (w) / a0, (1 - al * A) / a0, -2 * std::cos (w) / a0, (1 - al / A) / a0 };
    }
    static Biquad lowShelf (double fs, double f0, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f0 / fs, cs = std::cos (w);
        const double al = std::sin (w) / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * cs + sq;
        return { A * ((A + 1) - (A - 1) * cs + sq) / a0, 2 * A * ((A - 1) - (A + 1) * cs) / a0,
                 A * ((A + 1) - (A - 1) * cs - sq) / a0, -2 * ((A - 1) + (A + 1) * cs) / a0,
                 ((A + 1) + (A - 1) * cs - sq) / a0 };
    }
    double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    cpx response (double f, double fs) const
    {
        const cpx z = std::polar (1.0, -2 * kPi * f / fs);
        return (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z);
    }
};

static int failures = 0;
static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("  %-48s %s  (obtido %.3f, esperado %.3f)\n", what, ok ? "OK " : "FALHA", got, want);
    if (! ok) ++failures;
}

static float curveAt (const Curve& c, double f, bool phase = false)
{
    size_t best = 0;
    for (size_t i = 0; i < c.freq.size(); ++i)
        if (std::abs (std::log (c.freq[i] / f)) < std::abs (std::log (c.freq[best] / f))) best = i;
    return phase ? c.phaseDeg[best] : c.magDb[best];
}

static void testResponse (Signal sig, const char* name)
{
    std::printf ("Resposta — %s\n", name);
    const double fs = 48000; const int N = 8192, delay = 100;
    ExcitationSpec spec; spec.N = N; spec.fs = fs; spec.signal = sig; spec.levelDb = -18;
    const Excitation ex = makeExcitation (spec);

    Biquad b1 = Biquad::peak (fs, 5000, 1.0, 6.0), b2 = Biquad::lowShelf (fs, 200, 4.0);
    const Biquad r1 = b1, r2 = b2;
    const int total = N * 30;
    std::vector<float> y ((size_t) total + delay, 0.0f);
    for (int n = 0; n < total; ++n)
        y[(size_t) n + delay] = (float) b2.process (b1.process (ex.x[(size_t) (n % N)]));

    ResponseAnalyzer ra; ra.configure (ex); ra.setAverages (8);
    // analisador começa num ponto arbitrário e tem uma "falha" (paragem do host) a meio
    int pos = 5 * N + 777;
    for (int f = 0; f < 20; ++f) { if (f == 10) pos += 513; ra.addFrame (&y[(size_t) pos]); pos += N; }

    std::vector<cpx> H; std::vector<char> v;
    ra.transfer (H, v);
    const double d = ra.estimateDelay (H);
    Curve c; buildLogCurve (H, v, nullptr, N, fs, d, 1024, 20, 20000, 0.0, c);

    for (double f : { 50.0, 200.0, 1000.0, 5000.0, 12000.0 })
    {
        const cpx h = r1.response (f, fs) * r2.response (f, fs);
        char buf[96];
        std::snprintf (buf, sizeof buf, "magnitude @ %.0f Hz (dB)", f);
        check (std::abs (curveAt (c, f) - ampToDb (std::abs (h))) < 0.15, buf, curveAt (c, f), ampToDb (std::abs (h)));
        std::snprintf (buf, sizeof buf, "fase @ %.0f Hz (graus)", f);
        const double want = std::arg (h) * 180 / kPi;
        check (std::abs (curveAt (c, f, true) - want) < 3.0, buf, curveAt (c, f, true), want);
    }
}

static void testHarmonics()
{
    std::printf ("Harmónicos — saturação assimétrica\n");
    const double fs = 48000; const int N = 16384;
    ExcitationSpec spec; spec.N = N; spec.fs = fs; spec.signal = Signal::Sine; spec.levelDb = -6; spec.sineHz = 1000;
    const Excitation ex = makeExcitation (spec);
    auto sat = [] (double x) { return std::tanh (2.0 * x + 0.3) - std::tanh (0.3); };

    std::vector<float> y ((size_t) N * 8);
    std::mt19937 rng (1); std::normal_distribution<double> nd (0.0, 1.0e-5);
    for (size_t n = 0; n < y.size(); ++n) y[n] = (float) (sat (ex.x[n % (size_t) N]) + nd (rng));

    HarmonicAnalyzer ha; ha.configure (ex); ha.setAverages (0);
    for (int f = 0; f < 6; ++f) ha.addFrame (&y[(size_t) (f * N + 333)]);
    const HarmonicResult r = ha.analyse();

    // Referência: série de Fourier direta da curva de saturação
    const int M = 1 << 16;
    double re[11] = {}, im[11] = {};
    for (int i = 0; i < M; ++i)
    {
        const double t = 2 * kPi * i / M, s = sat (dbToGain (-6) * std::sin (t));
        for (int h = 1; h <= 10; ++h) { re[h] += s * std::cos (h * t); im[h] += s * std::sin (h * t); }
    }
    double a[11];
    for (int h = 1; h <= 10; ++h) a[h] = 2.0 * std::hypot (re[h], im[h]) / M;
    double h2 = 0; for (int h = 2; h <= 10; ++h) h2 += a[h] * a[h];
    const double thd = 100 * std::sqrt (h2) / a[1];

    check (std::abs (r.f0 - 1000.0) < 3.0, "f0 (Hz, centrado no bin)", r.f0, 1000.0);
    check (std::abs (r.relDb[2] - ampToDb (a[2] / a[1])) < 0.1, "H2 (dBc)", r.relDb[2], ampToDb (a[2] / a[1]));
    check (std::abs (r.relDb[3] - ampToDb (a[3] / a[1])) < 0.1, "H3 (dBc)", r.relDb[3], ampToDb (a[3] / a[1]));
    check (std::abs (r.thdPct - thd) < 0.02 * thd, "THD (%)", r.thdPct, thd);
    check (std::abs (r.gainDb - ampToDb (a[1] / dbToGain (-6))) < 0.05, "ganho fundamental (dB)", r.gainDb, ampToDb (a[1] / dbToGain (-6)));
}

static void testMusic()
{
    std::printf ("Música — dry/wet com atraso desconhecido\n");
    const double fs = 48000; const int N = 8192, lat = 1234;
    Biquad b1 = Biquad::peak (fs, 2500, 0.7, -5.0), b2 = Biquad::lowShelf (fs, 120, 3.0);
    const Biquad r1 = b1, r2 = b2;

    // "Música": ruído rosa aproximado (filtro de Paul Kellet)
    const int total = N * 60;
    std::vector<float> dry ((size_t) total), wet ((size_t) total, 0.0f);
    std::mt19937 rng (7); std::uniform_real_distribution<double> ud (-1, 1);
    double p0 = 0, p1 = 0, p2 = 0;
    for (int n = 0; n < total; ++n)
    {
        const double w = ud (rng);
        p0 = 0.99765 * p0 + w * 0.0990460; p1 = 0.96300 * p1 + w * 0.2965164; p2 = 0.57000 * p2 + w * 1.0526913;
        dry[(size_t) n] = (float) (0.1 * (p0 + p1 + p2 + w * 0.1848));
    }
    for (int n = 0; n < total - lat; ++n)
        wet[(size_t) n + lat] = (float) b2.process (b1.process (dry[(size_t) n]));

    TransferAnalyzer ta; ta.configure (N); ta.setAverages (32);
    int dly = 0;   // atraso aplicado ao dry, aprendido automaticamente
    for (int e = 4 * N; e + N <= total; e += N / 2)
    {
        ta.addFrames (&dry[(size_t) (e - dly)], &wet[(size_t) e]);
        if (ta.frames() >= 4)
        {
            double prom; const int r = ta.residualLag (&prom);
            if (r != 0 && prom > 6.0) { dly += r; ta.reset(); }
        }
    }
    check (dly == lat, "latência detetada (amostras)", dly, lat);

    std::vector<cpx> H; std::vector<char> v; std::vector<double> coh;
    ta.transfer (H, v, coh);
    Curve c; buildLogCurve (H, v, &coh, N, fs, 0.0, 1024, 20, 20000, 1.0 / 12.0, c);
    for (double f : { 100.0, 1000.0, 2500.0, 8000.0 })
    {
        const cpx h = r1.response (f, fs) * r2.response (f, fs);
        char buf[96]; std::snprintf (buf, sizeof buf, "magnitude @ %.0f Hz (dB)", f);
        check (std::abs (curveAt (c, f) - ampToDb (std::abs (h))) < 0.3, buf, curveAt (c, f), ampToDb (std::abs (h)));
    }
    check (curveAt (c, 1000.0, false) != 0 && c.coh[512] > 0.95, "coerência média-alta", c.coh[512], 1.0);
}

int main()
{
    testResponse (Signal::LogSweep,  "Sweep log");
    testResponse (Signal::Impulse,   "Impulso");
    testResponse (Signal::PinkNoise, "Ruído rosa periódico");
    testHarmonics();
    testMusic();
    std::printf ("\n%s (%d falhas)\n", failures ? "FALHOU" : "TUDO OK", failures);
    return failures ? 1 : 0;
}
