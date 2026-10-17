// Teste de integração com uma instância real do plugin (buses, latência, sidechain, delta, bypass,
// estado, mudança de qualidade) + capturas da interface.
//   cmake -B build -DMXS_BUILD_TESTS=ON && cmake --build build --target mxs_plugin_test
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <random>

static int failures = 0;
static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("  %-52s %s (obtido %.4f, esperado %.4f)\n", what, ok ? "OK   " : "FALHA", got, want);
    if (! ok) ++failures;
}

static void setParam (MixspaceProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

struct Gen
{
    std::mt19937 rng { 7 };
    std::normal_distribution<float> nd { 0.0f, 1.0f };
    double b1z1 = 0, b1z2 = 0, b2z1 = 0, b2z2 = 0, b0, a1, a2;
    Gen (double fs, double f0, double q)
    {
        const double w = 2 * juce::MathConstants<double>::pi * f0 / fs, al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = al / a0; a1 = -2 * std::cos (w) / a0; a2 = (1 - al) / a0;
    }
    float bp (float x, double& z1, double& z2) { const double y = b0 * x + z1; z1 = -a1 * y + z2; z2 = -b0 * x - a2 * y; return (float) y; }
    float voice() { return 0.5f * bp (bp (nd (rng), b1z1, b1z2), b2z1, b2z2); }
    float beat() { return 0.1f * nd (rng); }
};

// processa 'blocks' blocos; devolve a entrada main (L) e a saída (L) concatenadas
static void runBlocks (MixspaceProcessor& p, Gen& gen, int blocks, int bs, bool withSc,
                       std::vector<float>* inL = nullptr, std::vector<float>* outL = nullptr)
{
    const int totalCh = p.getTotalNumInputChannels();
    juce::AudioBuffer<float> buf (juce::jmax (totalCh, p.getTotalNumOutputChannels()), bs);
    juce::MidiBuffer midi;
    for (int k = 0; k < blocks; ++k)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i)
        {
            const float l = gen.beat(), r = gen.beat(), v = gen.voice();
            buf.setSample (0, i, l);
            if (p.getMainBusNumInputChannels() > 1) buf.setSample (1, i, r);
            if (withSc && totalCh > p.getMainBusNumInputChannels())
                for (int ch = p.getMainBusNumInputChannels(); ch < totalCh; ++ch) buf.setSample (ch, i, v);
            if (inL) inL->push_back (l);
        }
        p.processBlock (buf, midi);
        if (outL) for (int i = 0; i < bs; ++i) outL->push_back (buf.getSample (0, i));
    }
}

static double rmsDb (const std::vector<float>& x, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return 10 * std::log10 (s / (double) (to - from) + 1e-30);
}

static void snapshot (juce::Component& ed, const char* file)
{
    namespace L = piradex::i18n;
    auto save = [&] (const juce::String& name)
    {
        auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (name));
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::printf ("  captura: %s\n", name.toRawUTF8());
    };
    L::set (L::Language::PT, false);
    pump (400);
    save (file);
    L::set (L::Language::EN, false);          // a mesma janela em inglês (troca em directo)
    pump (150);
    save (juce::String (file).replace (".png", "_en.png"));
    L::set (L::Language::PT, false);
    pump (60);
}

// Idioma PT/EN: tr(), aviso de troca, ficheiro Piradex, nomes de automação fixos
static void testLanguage()
{
    namespace L = piradex::i18n;
    std::printf ("[idioma PT/EN]\n");
    const auto before = L::current();
    L::set (L::Language::PT, false);
    check (L::tr ("Avançado", "Advanced") == juce::String::fromUTF8 ("Avançado"), "tr devolve PT", 1, 1);
    L::set (L::Language::EN, false);
    check (L::tr ("Avançado", "Advanced") == "Advanced", "tr devolve EN", 1, 1);
    struct Count : L::Listener { int n = 0; void languageChanged() override { ++n; } } count;
    L::addListener (&count);
    L::set (L::Language::PT, false);
    L::set (L::Language::PT, false);
    L::removeListener (&count);
    check (count.n == 1, "aviso de troca so quando muda", count.n, 1);
    L::set (L::Language::EN, true);
    L::set (L::Language::PT, false);
    L::reload();
    check (L::current() == L::Language::EN, "idioma guardado em Piradex.settings", (int) L::current(), 1);
    MixspaceProcessor p;
    const auto name = p.apvts.getParameter (ids::space)->getName (64);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        L::set (L::Language::PT, false); pump (40);
        L::set (L::Language::EN, false); pump (40);
    }
    check (p.apvts.getParameter (ids::space)->getName (64) == name, "nomes de automacao fixos", 1, 1);
    L::set (before, true);
    L::set (L::Language::PT, false);
}

static juce::TextButton* findButton (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text) return b;
        if (auto* f = findButton (*c, text)) return f;
    }
    return nullptr;
}

static void stereoWithSidechain (MixspaceProcessor& p)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (juce::AudioChannelSet::stereo()); l.inputBuses.add (juce::AudioChannelSet::stereo());
    l.outputBuses.add (juce::AudioChannelSet::stereo());
    p.setBusesLayout (l);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testLanguage();
    const double fs = 48000.0;
    const int bs = 512;

    // ---------------------------------------------------------------- Login
    std::printf ("Login\n");
    {
        auto& L = lic::License::get();
        L.lock();
        check (! L.isUnlocked(), "bloqueado no arranque", L.isUnlocked(), 0);

        // o PBKDF2 do plugin tem de dar o mesmo que o gerador de senhas (Python hashlib)
        const auto h = lic::License::pbkdf2 ("TESTE-MIXSPACE-0000", lic::License::fromHex ("00112233445566778899aabbccddeeff"),
                                             lic::License::kIterations, 32);
        const bool same = juce::String::toHexString (h.getData(), (int) h.getSize(), 0)
                          == "9d32cb46c1a6652cc5190fbf03823c03b9de041d632d37b11586114b854dee2b";
        check (same, "PBKDF2 igual ao do nova_senha_beta.py", same, 1);

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto r = L.tryPassword ("BETA-AAAA-BBBB-CCCC", false);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (r == lic::License::Result::Wrong && ! L.isUnlocked(), "senha errada recusada", (double) (int) r, 1);
        std::printf ("    verificação de uma senha (6 chaves): %.0f ms\n", ms);

        // bloqueado: mesmo com SPACE 100 e voz no sidechain, o beat passa intacto (com a latência)
        MixspaceProcessor p;
        stereoWithSidechain (p);
        p.prepareToPlay (fs, bs);
        setParam (p, ids::space, 100.0f);
        setParam (p, ids::range, 12.0f);
        Gen g (fs, 1000, 2);
        std::vector<float> in, out;
        runBlocks (p, g, 80, bs, true, &in, &out);
        double err = 0;
        for (size_t i = 4096; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - 2048]));
        check (err < 1e-6, "bloqueado: áudio passa sem processar", err, 0);

        {
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::stereo()); l.inputBuses.add (juce::AudioChannelSet::stereo());
            l.outputBuses.add (juce::AudioChannelSet::stereo());
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            snapshot (*ed, "ui_login.png");
        }

        L.unlockForTests();
        check (L.isUnlocked(), "desbloqueado (build de teste)", L.isUnlocked(), 1);
        std::vector<float> in2, out2;
        runBlocks (p, g, 120, bs, true, &in2, &out2);
        double diff = 0;
        for (size_t i = 30000; i < out2.size(); ++i) diff = std::max (diff, (double) std::abs (out2[i] - in2[i - 2048]));
        check (diff > 1e-3, "desbloqueado: o MIXSPACE volta a processar", diff, 0.001);
    }

    std::printf ("Buses e latência\n");
    {
        MixspaceProcessor p;
        check (p.getBusCount (true) == 2, "2 entradas (main + sidechain)", p.getBusCount (true), 2);
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (p.checkBusesLayoutSupported (mono), "layout mono suportado", 1, 1);
        juce::AudioProcessor::BusesLayout bad;
        bad.inputBuses.add (juce::AudioChannelSet::stereo()); bad.inputBuses.add (juce::AudioChannelSet::stereo());
        bad.outputBuses.add (juce::AudioChannelSet::mono());
        check (! p.checkBusesLayoutSupported (bad), "entrada ≠ saída rejeitado", 0, 0);

        stereoWithSidechain (p);
        p.prepareToPlay (fs, bs);
        check (p.getLatencySamples() == 2048, "latência reportada (MIX @ 48 kHz)", p.getLatencySamples(), 2048);
        setParam (p, ids::quality, 2);
        Gen g (fs, 1000, 2);
        runBlocks (p, g, 1, bs, true);
        check (p.getLatencySamples() == 4096, "MASTER muda a latência para 4096", p.getLatencySamples(), 4096);
    }

    std::printf ("Áudio\n");
    {
        MixspaceProcessor p;
        stereoWithSidechain (p);
        p.prepareToPlay (fs, bs);
        setParam (p, ids::space, 0.0f);
        Gen g (fs, 1000, 2);
        std::vector<float> in, out;
        runBlocks (p, g, 120, bs, true, &in, &out);
        double err = 0;
        for (size_t i = 4096; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - 2048]));
        check (err < 3e-5, "SPACE 0: saída = entrada atrasada", err, 0);
        check (p.isSidechainConnected(), "sidechain ligado detectado", 1, 1);

        setParam (p, ids::space, 60.0f);
        std::vector<float> in2, out2;
        runBlocks (p, g, 200, bs, true, &in2, &out2);
        const double drop = rmsDb (in2, 50000, in2.size() - 4096) - rmsDb (out2, 52048, out2.size() - 2048);
        check (drop > 0.05 && drop < 3.0, "SPACE 60: beat perde pouca energia no total (dB)", drop, 0.2);

        setParam (p, ids::delta, 1.0f);
        std::vector<float> in3, out3;
        runBlocks (p, g, 120, bs, true, &in3, &out3);
        const double deltaDb = rmsDb (out3, 20000, out3.size()) - rmsDb (in3, 20000, in3.size());
        check (deltaDb < -6.0, "DELTA: só o que foi retirado (bem abaixo do beat)", deltaDb, -6.0);
        setParam (p, ids::delta, 0.0f);

        setParam (p, ids::bypass, 1.0f);
        std::vector<float> in4, out4;
        runBlocks (p, g, 60, bs, true, &in4, &out4);
        double errB = 0;
        for (size_t i = 8192; i < out4.size(); ++i) errB = std::max (errB, (double) std::abs (out4[i] - in4[i - 2048]));
        check (errB < 1e-6, "BYPASS: entrada atrasada (mesma latência)", errB, 0);
    }

    std::printf ("Sidechain desligado\n");
    {
        MixspaceProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::stereo()); l.inputBuses.add (juce::AudioChannelSet::disabled());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.setBusesLayout (l), "layout com sidechain desactivado aceite", 1, 1);
        p.prepareToPlay (fs, bs);
        setParam (p, ids::space, 100.0f);
        Gen g (fs, 1000, 2);
        std::vector<float> in, out;
        runBlocks (p, g, 60, bs, false, &in, &out);
        double err = 0;
        for (size_t i = 4096; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - 2048]));
        check (! p.isSidechainConnected(), "estado: SIDECHAIN NOT CONNECTED", 0, 0);
        check (err < 3e-5, "sem sidechain o áudio passa intacto", err, 0);
    }

    std::printf ("Estado (guardar / repor)\n");
    {
        MixspaceProcessor a, b;
        setParam (a, ids::space, 33.0f);
        setParam (a, ids::stereo, 1.0f);
        a.setUiScale (1.25f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        const float sp = b.apvts.getRawParameterValue (ids::space)->load();
        check (std::abs (sp - 33.0f) < 0.11f, "SPACE reposto", sp, 33.0);
        check ((int) b.apvts.getRawParameterValue (ids::stereo)->load() == 1, "modo MID reposto", b.apvts.getRawParameterValue (ids::stereo)->load(), 1);
        check (std::abs (b.getUiScale() - 1.25f) < 1e-4f, "tamanho da interface reposto", b.getUiScale(), 1.25);
    }

    std::printf ("Interface\n");
    {
        MixspaceProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::stereo()); l.inputBuses.add (juce::AudioChannelSet::disabled());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        p.setBusesLayout (l);
        p.prepareToPlay (fs, bs);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        Gen g (fs, 1000, 2);
        runBlocks (p, g, 40, bs, false);
        snapshot (*ed, "ui_sem_sidechain.png");
    }
    {
        MixspaceProcessor p;
        stereoWithSidechain (p);
        p.prepareToPlay (fs, bs);
        setParam (p, ids::space, 35.0f);
        setParam (p, ids::lowCut, 120.0f);
        setParam (p, ids::highCut, 12000.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        Gen g (fs, 1500, 1.2);
        for (int k = 0; k < 30; ++k) { runBlocks (p, g, 8, bs, true); pump (30); }
        check (p.analysis.maxRed.load() > 0.5f, "display recebe a curva de redução", p.analysis.maxRed.load(), 1.0);
        snapshot (*ed, "ui_principal.png");

        if (auto* adv = findButton (*ed, piradex::i18n::tr ("Avançado", "Advanced")))
        {
            adv->triggerClick();
            pump (100);
            snapshot (*ed, "ui_advanced.png");
            adv->triggerClick();
        }

        ed->setSize (MainView::kW * 3 / 2, MainView::kH * 3 / 2);
        for (int k = 0; k < 10; ++k) { runBlocks (p, g, 8, bs, true); pump (30); }
        snapshot (*ed, "ui_150.png");
        check (std::abs (p.getUiScale() - 1.5f) < 0.01f, "redimensionar para 150 %", p.getUiScale(), 1.5);
    }

    std::printf ("\n%s (%d falha%s)\n", failures == 0 ? "TODOS OS TESTES PASSARAM" : "HÁ FALHAS", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
