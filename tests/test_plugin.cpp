// Teste de integração com instâncias reais do plugin:
//  login · Gerador -> EQ -> Analisador · rapidez da curva ao mexer no EQ · harmónicos e forma de onda
//  · varrimento THD vs nível · música por sidechain · modo Host com um VST3 real (TestEQ)
// Também grava capturas da interface de cada vista.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using View = AnalysisResults::View;
static int failures = 0;

static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("  %-46s %s (obtido %.3f, esperado %.3f)\n", what, ok ? "OK   " : "FALHA", got, want);
    if (! ok) ++failures;
}

static void setParam (CurveAnalyzerProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

struct Biquad
{
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    static Biquad peak (double fs, double f0, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * pca::kPi * f0 / fs, al = std::sin (w) / (2 * q);
        const double a0 = 1 + al / A;
        return { (1 + al * A) / a0, -2 * std::cos (w) / a0, (1 - al * A) / a0, -2 * std::cos (w) / a0, (1 - al / A) / a0 };
    }
    void setFrom (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
    float process (float x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return (float) y; }
    double magDb (double f, double fs) const
    {
        const auto z = std::polar (1.0, -2 * pca::kPi * f / fs);
        return pca::ampToDb (std::abs ((b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z)));
    }
};

static float curveAt (const pca::Curve& c, double f)
{
    size_t best = 0;
    for (size_t i = 0; i < c.freq.size(); ++i)
        if (std::abs (std::log (c.freq[i] / f)) < std::abs (std::log (c.freq[best] / f))) best = i;
    return c.magDb[best];
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void snapshot (CurveAnalyzerProcessor& p, const char* file, int view = 0)
{
    setParam (p, ids::view, (float) view);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1280, 740);
    pump (350);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (file));
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("  captura: %s\n", file);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int bs = 512;
    juce::MidiBuffer midi;

    // ---------------------------------------------------------------- Login
    std::printf ("Login\n");
    auto& L = lic::License::get();
    L.lock();
    check (! L.isUnlocked(), "bloqueado no arranque", L.isUnlocked(), 0);
    {
        CurveAnalyzerProcessor p;
        setParam (p, ids::role, 1);
        p.prepareToPlay (fs, bs);
        juce::AudioBuffer<float> b (2, bs);
        for (int n = 0; n < bs; ++n) { b.setSample (0, n, 0.25f); b.setSample (1, n, -0.5f); }
        p.processBlock (b, midi);
        check (b.getSample (0, 100) == 0.25f && b.getSample (1, 100) == -0.5f, "bloqueado = passa o áudio intacto", b.getSample (0, 100), 0.25);
        snapshot (p, "ui_login.png");
    }
    check (L.tryPassword ("senha-errada", false) == lic::License::Result::Wrong, "senha errada recusada", 0, 0);
    const auto master = juce::SystemStats::getEnvironmentVariable ("ANL_TEST_MASTER", {});
    const auto beta   = juce::SystemStats::getEnvironmentVariable ("ANL_TEST_BETA", {});
    if (master.isNotEmpty())
    {
        const bool ok = L.tryPassword (master, false) == lic::License::Result::Ok && L.kind() == lic::Kind::Master;
        check (ok, "senha master aceite", ok, 1);
        L.lock();
    }
    if (beta.isNotEmpty())
    {
        const bool ok = L.tryPassword (beta, false) == lic::License::Result::Ok && L.kind() == lic::Kind::Trial;
        check (ok, "senha beta aceite", ok, 1);
    }
    if (! L.isUnlocked()) L.unlockForTests();

    // ---------------------------------------------------------------- Resposta + rapidez
    std::printf ("Resposta: Gerador -> EQ (+6 dB @ 5 kHz) + 37 amostras -> Analisador\n");
    auto gen = std::make_unique<CurveAnalyzerProcessor>();
    auto ana = std::make_unique<CurveAnalyzerProcessor>();
    setParam (*gen, ids::role, 1); setParam (*gen, ids::group, 2); setParam (*gen, ids::signal, 1);
    setParam (*ana, ids::role, 0); setParam (*ana, ids::group, 2); setParam (*ana, ids::signal, 0);
    gen->prepareToPlay (fs, bs); ana->prepareToPlay (fs, bs);

    Biquad eqL = Biquad::peak (fs, 5000, 1.0, 6.0), eqR = eqL;
    const Biquad refUp = eqL, refDown = Biquad::peak (fs, 5000, 1.0, -6.0);
    std::vector<float> dl (37, 0.0f), dr (37, 0.0f); size_t dp = 0;
    juce::AudioBuffer<float> buf (2, bs);
    int64_t sampleClock = 0;
    int pacingMs = 2;
    auto runChain = [&] (int blocks, std::function<bool()> stopWhen = {})
    {
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            gen->processBlock (buf, midi);
            for (int n = 0; n < bs; ++n)
            {
                const float l = eqL.process (buf.getSample (0, n)), r = eqR.process (buf.getSample (1, n));
                buf.setSample (0, n, dl[dp]); buf.setSample (1, n, dr[dp]);
                dl[dp] = l; dr[dp] = r; dp = (dp + 1) % dl.size();
            }
            ana->processBlock (buf, midi);
            sampleClock += bs;
            juce::Thread::sleep (pacingMs);
            if (stopWhen && stopWhen()) return true;
        }
        return false;
    };
    runChain (300);
    juce::Thread::sleep (200);
    AnalysisResults r; ana->getResults (r);
    check (r.view == View::Response, "vista = resposta", (double) (int) r.view, (double) (int) View::Response);
    check (r.linked, "ligado ao Gerador do grupo", r.linked, 1);
    check (std::abs (r.delaySamples - 37) < 0.5, "latência detetada (amostras)", r.delaySamples, 37);
    for (double f : { 100.0, 1000.0, 5000.0, 10000.0 })
    {
        char t[64]; std::snprintf (t, sizeof t, "magnitude @ %.0f Hz (dB)", f);
        check (std::abs (curveAt (r.curve, f) - refUp.magDb (f, fs)) < 0.15, t, curveAt (r.curve, f), refUp.magDb (f, fs));
    }
    snapshot (*ana, "ui_resposta.png", 0);
    snapshot (*ana, "ui_resposta_onda.png", 1);
    check (! r.waveOut.empty(), "resposta impulsional disponível", (double) r.waveOut.size(), 1);

    // Rodar o EQ de +6 para -6 dB: quanto tempo até a curva mostrar o valor novo?
    pacingMs = 10;                   // ~tempo real (512 amostras = 10,7 ms), como num DAW
    eqL.setFrom (refDown); eqR.setFrom (refDown);
    const int64_t t0 = sampleClock;
    const bool reached = runChain (400, [&] { AnalysisResults q; ana->getResults (q);
                                              return ! q.curve.empty() && std::abs (curveAt (q.curve, 5000) - (-6.0)) < 0.3; });
    const double ms = 1000.0 * (double) (sampleClock - t0) / fs;
    check (reached && ms < 400.0, "curva acompanha o EQ em < 400 ms", ms, 400);

    // ---------------------------------------------------------------- Harmónicos + onda
    std::printf ("Harmónicos: seno 1 kHz -6 dBFS -> saturação tanh -> Analisador\n");
    auto sat = [] (double x) { return std::tanh (2.0 * x + 0.25) - std::tanh (0.25); };
    setParam (*gen, ids::signal, 3); setParam (*gen, ids::sineFreq, 1000); setParam (*gen, ids::level, -6);
    setParam (*gen, ids::fftSize, 2);
    gen->prepareToPlay (fs, bs);
    auto runSat = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            gen->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < bs; ++n)
                    buf.setSample (ch, n, (float) sat (buf.getSample (ch, n)));
            ana->processBlock (buf, midi);
            juce::Thread::sleep (1);
        }
    };
    runSat (500);
    juce::Thread::sleep (250);
    ana->getResults (r);
    check (r.view == View::Harmonics, "vista = harmónicos", (double) (int) r.view, (double) (int) View::Harmonics);
    check (r.harm.thdPct > 1.0 && r.harm.thdPct < 30.0, "THD plausível (%)", r.harm.thdPct, 10);
    // A forma de onda reconstruída tem de bater com a curva de saturação real
    double worst = 0.0, peak = 0.0;
    for (size_t i = 0; i < r.waveIn.size(); ++i)
    {
        const double want = sat (r.waveIn[i]);
        worst = std::max (worst, std::abs (want - r.waveOut[i]));
        peak = std::max (peak, std::abs (want));
    }
    check (! r.waveIn.empty() && worst < 0.01 * peak, "onda de saída = saturação real (erro máx.)", worst, 0.01 * peak);
    snapshot (*ana, "ui_harmonicos.png", 0);
    snapshot (*ana, "ui_onda.png", 1);

    // ---------------------------------------------------------------- Varrimento THD vs nível
    std::printf ("Varrimento: THD vs nível (15 passos) pedido pelo Analisador\n");
    setParam (*ana, ids::view, 3);
    setParam (*gen, ids::fftSize, 1);
    ana->requestSweep (pca::SweepKind::Level);
    runSat (1400);
    juce::Thread::sleep (250);
    ana->getResults (r);
    int validPts = 0;
    for (auto& s : r.sweep) validPts += s.valid ? 1 : 0;
    check (validPts >= 13, "pontos medidos no varrimento", validPts, 15);
    if (validPts >= 2)
    {
        const auto& a = r.sweep.front(); const auto& z = r.sweep.back();
        check (a.valid && z.valid && z.thdDb > a.thdDb + 20.0, "THD sobe com o nível (dB)", z.thdDb - a.thdDb, 20);
        check (z.gainDb < a.gainDb - 0.5, "compressão visível no ganho (dB)", z.gainDb - a.gainDb, -0.5);
    }
    check (r.sweepKind == 0, "varrimento terminou sozinho", r.sweepKind, 0);
    snapshot (*ana, "ui_varrimento.png", 3);

    // ---------------------------------------------------------------- Música (sidechain)
    std::printf ("Música: dry no sidechain, wet = EQ(dry) atrasado 700 amostras\n");
    auto mus = std::make_unique<CurveAnalyzerProcessor>();
    auto layout = mus->getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (mus->setBusesLayout (layout), "sidechain ativado", 1, 1);
    setParam (*mus, ids::role, 0); setParam (*mus, ids::source, 1); setParam (*mus, ids::group, 5);
    setParam (*mus, ids::averages, 6); setParam (*mus, ids::smoothing, 2);
    mus->prepareToPlay (fs, bs);
    Biquad m1 = Biquad::peak (fs, 2500, 0.7, -5.0);
    const Biquad mref = m1;
    juce::AudioBuffer<float> mb (mus->getTotalNumInputChannels(), bs);
    std::vector<float> line (700, 0.0f); size_t lp = 0;
    juce::Random rng (3);
    double p0 = 0, p1 = 0, p2 = 0;
    auto pinkSample = [&]
    {
        const double w = rng.nextDouble() * 2 - 1;
        p0 = 0.99765 * p0 + w * 0.0990460; p1 = 0.96300 * p1 + w * 0.2965164; p2 = 0.57000 * p2 + w * 1.0526913;
        return (float) (0.1 * (p0 + p1 + p2 + w * 0.1848));
    };
    for (int b = 0; b < 1500; ++b)
    {
        for (int n = 0; n < bs; ++n)
        {
            const float dry = pinkSample();
            const float wet = line[lp];
            line[lp] = m1.process (dry); lp = (lp + 1) % line.size();
            mb.setSample (0, n, wet); mb.setSample (1, n, wet);
            mb.setSample (2, n, dry); mb.setSample (3, n, dry);
        }
        mus->processBlock (mb, midi);
        if (b % 4 == 0) juce::Thread::sleep (2);
    }
    juce::Thread::sleep (300);
    mus->getResults (r);
    check (r.view == View::Music, "vista = música", (double) (int) r.view, (double) (int) View::Music);
    check (std::abs (r.delaySamples - 700) < 0.5, "latência alinhada (amostras)", r.delaySamples, 700);
    for (double f : { 200.0, 2500.0, 8000.0 })
    {
        char t[64]; std::snprintf (t, sizeof t, "magnitude @ %.0f Hz (dB)", f);
        check (std::abs (curveAt (r.curve, f) - mref.magDb (f, fs)) < 0.35, t, curveAt (r.curve, f), mref.magDb (f, fs));
    }
    const double specDiff = curveAt (r.specWet, 2500) - curveAt (r.specDry, 2500);
    check (std::abs (specDiff - mref.magDb (2500, fs)) < 1.0, "espetro depois-antes @ 2.5 kHz (dB)", specDiff, mref.magDb (2500, fs));
    snapshot (*mus, "ui_musica.png", 0);
    snapshot (*mus, "ui_espetro.png", 2);

    // ---------------------------------------------------------------- Modo Host com VST3 real
    const auto testEq = juce::SystemStats::getEnvironmentVariable ("ANL_TESTEQ", {});
    if (testEq.isNotEmpty())
    {
        std::printf ("Host: TestEQ.vst3 (+6 dB @ 5 kHz, 64 amostras de latência) carregado dentro do ANALYSER\n");
        auto host = std::make_unique<CurveAnalyzerProcessor>();
        setParam (*host, ids::role, 2); setParam (*host, ids::signal, 1); setParam (*host, ids::source, 0);
        host->prepareToPlay (fs, bs);
        juce::String err = "pendente";
        host->loadHostedPlugin ("VST3", testEq, [&err] (const juce::String& e) { err = e; });
        for (int i = 0; i < 50 && err == "pendente"; ++i) pump (100);
        check (host->hasHostedPlugin() && err.isEmpty(), "plugin carregado", host->hasHostedPlugin(), 1);
        if (err.isNotEmpty()) std::printf ("  erro: %s\n", err.toRawUTF8());

        juce::AudioBuffer<float> hb (2, bs);
        for (int b = 0; b < 300; ++b)
        {
            for (int n = 0; n < bs; ++n) { hb.setSample (0, n, 0.3f); hb.setSample (1, n, 0.3f); }
            host->processBlock (hb, midi);
            juce::Thread::sleep (2);
        }
        juce::Thread::sleep (250);
        host->getResults (r);
        check (r.view == View::Response && r.hostMode, "vista = resposta (host)", (double) (int) r.view, (double) (int) View::Response);
        check (std::abs (r.delaySamples - 64) < 0.5, "latência do plugin interno (amostras)", r.delaySamples, 64);
        check (std::abs (curveAt (r.curve, 5000) - 6.0) < 0.2, "curva do plugin interno @ 5 kHz (dB)", curveAt (r.curve, 5000), 6.0);
        check (hb.getSample (0, 10) == 0.3f, "faixa passa intacta no modo gerador", hb.getSample (0, 10), 0.3);
        if (juce::SystemStats::getEnvironmentVariable ("ANL_HAS_WM", {}).isNotEmpty()) { host->openHostedEditor(); pump (200); }
        snapshot (*host, "ui_host.png", 0);

        // Música através do plugin interno: ouve-se o plugin e mede-se dry vs wet
        setParam (*host, ids::source, 1);
        for (int b = 0; b < 900; ++b)
        {
            for (int n = 0; n < bs; ++n) { const float d = pinkSample(); hb.setSample (0, n, d); hb.setSample (1, n, d); }
            host->processBlock (hb, midi);
            if (b % 4 == 0) juce::Thread::sleep (2);
        }
        juce::Thread::sleep (300);
        host->getResults (r);
        check (r.view == View::Music && std::abs (r.delaySamples - 64) < 0.5, "música no host: latência (amostras)", r.delaySamples, 64);
        check (std::abs (curveAt (r.curve, 5000) - 6.0) < 0.4, "música no host: curva @ 5 kHz (dB)", curveAt (r.curve, 5000), 6.0);
        host->unloadHostedPlugin();
        check (! host->hasHostedPlugin(), "plugin removido", host->hasHostedPlugin(), 0);
    }
    else
        std::printf ("(defina ANL_TESTEQ com o caminho do TestEQ.vst3 para testar o modo Host)\n");

    std::printf ("\n%s (%d falhas)\n", failures ? "FALHOU" : "TUDO OK", failures);
    gen.reset(); ana.reset(); mus.reset();
    return failures ? 1 : 0;
}
