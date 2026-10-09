// Teste de integração: duas instâncias reais do plugin (Gerador -> EQ -> Analisador),
// modo harmónicos e modo música por sidechain. Também grava capturas da interface.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using View = AnalysisResults::View;
static int failures = 0;

static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("  %-44s %s (obtido %.3f, esperado %.3f)\n", what, ok ? "OK   " : "FALHA", got, want);
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

static void snapshot (CurveAnalyzerProcessor& p, const char* file)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1240, 700);
    pump (300);
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

    // ---------------------------------------------------------------- Login
    std::printf ("Login\n");
    auto& L = lic::License::get();
    L.lock();
    check (! L.isUnlocked(), "bloqueado no arranque", L.isUnlocked(), 0);
    {
        // bloqueado: o plugin tem de ser transparente (mesmo em modo Gerador)
        CurveAnalyzerProcessor p;
        setParam (p, ids::role, 1);
        p.prepareToPlay (fs, bs);
        juce::AudioBuffer<float> b (2, bs);
        for (int n = 0; n < bs; ++n) { b.setSample (0, n, 0.25f); b.setSample (1, n, -0.5f); }
        juce::MidiBuffer m;
        p.processBlock (b, m);
        check (b.getSample (0, 100) == 0.25f && b.getSample (1, 100) == -0.5f, "bloqueado = passa o áudio intacto", b.getSample (0, 100), 0.25);
        snapshot (p, "ui_login.png");
    }
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    check (L.tryPassword ("senha-errada", false) == lic::License::Result::Wrong, "senha errada recusada", 0, 0);
    const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::printf ("  (verificação de senha: %.0f ms)\n", ms);
    // As senhas reais nunca ficam no repositório: passam por variáveis de ambiente
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
        std::printf ("  estado: %s\n", L.statusText().toRawUTF8());
    }
    if (! L.isUnlocked())
    {
        // sem senhas no ambiente: desbloqueia só para os testes de medição
        L.unlockForTests();
        std::printf ("  (defina ANL_TEST_MASTER / ANL_TEST_BETA para testar as senhas reais)\n");
    }
    const lic::KeyEntry k { "t", lic::Kind::Trial, "", "", 2026, 12, 31 };
    check (! lic::License::isExpired (k, juce::Time (2026, 11, 31, 22, 0)), "beta válida em 31/12/2026", 0, 0);
    check (lic::License::isExpired (k, juce::Time (2027, 0, 1, 0, 1)), "beta expirada em 01/01/2027", 1, 1);

    // ---------------------------------------------------------------- Resposta
    std::printf ("Resposta: Gerador -> EQ (+6 dB @ 5 kHz) + 37 amostras -> Analisador\n");
    auto gen = std::make_unique<CurveAnalyzerProcessor>();
    auto ana = std::make_unique<CurveAnalyzerProcessor>();
    setParam (*gen, ids::role, 1); setParam (*gen, ids::group, 2); setParam (*gen, ids::signal, 1);
    setParam (*ana, ids::role, 0); setParam (*ana, ids::group, 2);
    setParam (*ana, ids::signal, 0);          // propositadamente diferente: deve adotar o do Gerador
    gen->prepareToPlay (fs, bs); ana->prepareToPlay (fs, bs);

    Biquad eqL = Biquad::peak (fs, 5000, 1.0, 6.0), eqR = eqL;
    const Biquad ref = eqL;
    std::vector<float> dl (37, 0.0f), dr (37, 0.0f); size_t dp = 0;
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    for (int b = 0; b < 600; ++b)
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
        if (b % 8 == 0) juce::Thread::sleep (2);
    }
    juce::Thread::sleep (300);
    AnalysisResults r; ana->getResults (r);
    check (r.view == View::Response, "vista = resposta", (double) (int) r.view, (double) (int) View::Response);
    check (r.linked, "ligado ao Gerador do grupo", r.linked, 1);
    check (r.spec.signal == pca::Signal::LogSweep, "adotou sinal do Gerador (sweep)", (double) (int) r.spec.signal, 1);
    check (std::abs (r.delaySamples - 37) < 0.5, "latência detetada (amostras)", r.delaySamples, 37);
    for (double f : { 100.0, 1000.0, 5000.0, 10000.0 })
    {
        char t[64]; std::snprintf (t, sizeof t, "magnitude @ %.0f Hz (dB)", f);
        check (std::abs (curveAt (r.curve, f) - ref.magDb (f, fs)) < 0.15, t, curveAt (r.curve, f), ref.magDb (f, fs));
    }
    snapshot (*ana, "ui_resposta.png");
    snapshot (*gen, "ui_gerador.png");

    // ---------------------------------------------------------------- Harmónicos
    std::printf ("Harmónicos: seno 1 kHz -6 dBFS -> saturação tanh -> Analisador\n");
    setParam (*gen, ids::signal, 3); setParam (*gen, ids::sineFreq, 1000); setParam (*gen, ids::level, -6);
    setParam (*gen, ids::fftSize, 2);
    gen->prepareToPlay (fs, bs);
    for (int b = 0; b < 900; ++b)
    {
        buf.clear();
        gen->processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < bs; ++n)
            {
                const float x = buf.getSample (ch, n);
                buf.setSample (ch, n, (float) (std::tanh (2.0 * x + 0.25) - std::tanh (0.25)));
            }
        ana->processBlock (buf, midi);
        if (b % 8 == 0) juce::Thread::sleep (2);
    }
    juce::Thread::sleep (300);
    ana->getResults (r);
    check (r.view == View::Harmonics, "vista = harmónicos", (double) (int) r.view, (double) (int) View::Harmonics);
    check (r.harm.thdPct > 1.0 && r.harm.thdPct < 30.0, "THD plausível (%)", r.harm.thdPct, 10);
    check (r.harm.relDb[2] > -40 && r.harm.relDb[3] > -40, "H2 e H3 presentes (dBc)", r.harm.relDb[2], r.harm.relDb[3]);
    std::printf ("  H2 %.1f dBc · H3 %.1f dBc · THD %.2f %% · ganho %.2f dB\n", r.harm.relDb[2], r.harm.relDb[3], r.harm.thdPct, r.harm.gainDb);
    snapshot (*ana, "ui_harmonicos.png");

    // ---------------------------------------------------------------- Música (sidechain)
    std::printf ("Música: dry no sidechain, wet = EQ(dry) atrasado 700 amostras\n");
    auto mus = std::make_unique<CurveAnalyzerProcessor>();
    auto layout = mus->getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (mus->setBusesLayout (layout), "sidechain ativado", 1, 1);
    setParam (*mus, ids::role, 0); setParam (*mus, ids::source, 1); setParam (*mus, ids::group, 5);
    setParam (*mus, ids::averages, 5); setParam (*mus, ids::smoothing, 2);
    mus->prepareToPlay (fs, bs);
    Biquad m1 = Biquad::peak (fs, 2500, 0.7, -5.0);
    const Biquad mref = m1;
    juce::AudioBuffer<float> mb (mus->getTotalNumInputChannels(), bs);
    std::vector<float> line (700, 0.0f); size_t lp = 0;
    juce::Random rng (3);
    double p0 = 0, p1 = 0, p2 = 0;
    for (int b = 0; b < 1500; ++b)
    {
        for (int n = 0; n < bs; ++n)
        {
            const double w = rng.nextDouble() * 2 - 1;
            p0 = 0.99765 * p0 + w * 0.0990460; p1 = 0.96300 * p1 + w * 0.2965164; p2 = 0.57000 * p2 + w * 1.0526913;
            const float dry = (float) (0.1 * (p0 + p1 + p2 + w * 0.1848));
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
    snapshot (*mus, "ui_musica.png");

    std::printf ("\n%s (%d falhas)\n", failures ? "FALHOU" : "TUDO OK", failures);
    gen.reset(); ana.reset(); mus.reset();
    return failures ? 1 : 0;
}
