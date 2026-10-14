// =============================================================================
//  Teste de integração do EQMotion com instâncias reais do processador e do editor
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <random>
#include <chrono>

using namespace eqm;
static int failures = 0, checks = 0;
static void check (bool ok, const char* what, double got = 0, double want = 0)
{
    ++checks;
    std::printf ("  %-58s %s (obtido %.4g, esperado %.4g)\n", what, ok ? "OK   " : "FALHA", got, want);
    if (! ok) ++failures;
}
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void setP (EQMotionProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
static void setBand (EQMotionProcessor& p, int i, const BandParams& b) { p.state.setBand (i, b); }
static BandParams mk (FilterType t, double f, double g, double q, Slope s = Slope::dB12)
{
    BandParams b; b.active = true; b.type = t; b.freq = f; b.gainDb = g; b.q = q; b.slope = s; return b;
}

// Processa um sinal (estéreo igual nos dois canais) em blocos; devolve canal esquerdo
template <typename T = float>
static std::vector<double> run (EQMotionProcessor& p, const std::vector<double>& in, int block = 512, std::vector<double>* sc = nullptr)
{
    const int n = (int) in.size();
    std::vector<double> out ((size_t) n);
    const int chans = p.getTotalNumInputChannels();
    juce::AudioBuffer<T> buf (juce::jmax (chans, 2), block);
    juce::MidiBuffer midi;
    for (int s = 0; s < n; s += block)
    {
        const int m = std::min (block, n - s);
        buf.setSize (buf.getNumChannels(), m, false, false, true);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < m; ++i) buf.setSample (c, i, (T) in[(size_t) (s + i)]);
        for (int c = 2; c < buf.getNumChannels(); ++c) for (int i = 0; i < m; ++i) buf.setSample (c, i, sc ? (T) (*sc)[(size_t) (s + i)] : (T) 0);
        p.processBlock (buf, midi);
        for (int i = 0; i < m; ++i) out[(size_t) (s + i)] = (double) buf.getSample (0, i);
    }
    return out;
}

static std::vector<double> noise (int n, unsigned seed = 1, double amp = 0.25)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<double> u (-amp, amp);
    std::vector<double> v ((size_t) n); for (auto& x : v) x = u (rng); return v;
}

static double magAt (const std::vector<double>& ir, double f, double fs)
{
    std::complex<double> acc;
    for (size_t n = 0; n < ir.size(); ++n) acc += ir[n] * std::polar (1.0, -kTwoPi * f * (double) n / fs);
    return 20 * std::log10 (std::abs (acc) + 1e-30);
}

static std::unique_ptr<EQMotionProcessor> make (double fs = 48000, int block = 512, bool sidechain = false)
{
    auto p = std::make_unique<EQMotionProcessor>();
    if (sidechain)
    {
        auto layout = p->getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        p->setBusesLayout (layout);
    }
    p->setRateAndBufferSizeDetails (fs, block);
    p->prepareToPlay (fs, block);
    return p;
}

static void waitConfig (EQMotionProcessor& p)
{
    pump (120);   // timer da latência (25 Hz)
    std::vector<double> z (4096, 0.0);
    run (p, z);   // o motor troca (com desvanecimento) no bloco seguinte
}

static void snapshot (EQMotionProcessor& p, const char* file, std::function<void (EQMotionEditor&)> prep = {})
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<EQMotionEditor*> (ed.get());
    ed->setSize (EQMotionEditor::kBaseW, EQMotionEditor::kBaseH);
    if (prep) prep (*e);
    // alimenta o analisador
    auto x = noise (48000, 11, 0.2);
    for (int k = 0; k < 6; ++k) { run (p, x); pump (60); }
    pump (300);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (file));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("  captura: %s\n", file);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    lic::License::get().unlockForTests();
    const double fs = 48000;

    std::printf ("Transparência e precisão (Zero Latency)\n");
    {
        auto p = make();
        auto x = noise (20000);
        auto y = run (*p, x);
        double err = 0; for (size_t i = 0; i < x.size(); ++i) err = std::max (err, std::abs (y[i] - (double) (float) x[i]));
        check (err == 0.0, "sem bandas: saída = entrada (bit a bit)", err, 0);
        check (p->getLatencySamples() == 0, "latência ZL", p->getLatencySamples(), 0);

        setBand (*p, 0, mk (FilterType::Bell, 1000, 6, 1.0));
        setBand (*p, 1, mk (FilterType::LowCut, 100, 0, 0.7071, Slope::dB24));
        pump (50);
        run (*p, std::vector<double> (9600, 0.0));
        std::vector<double> imp (16384, 0.0); imp[0] = 1.0;
        auto ir = run (*p, imp);
        const double m1k = magAt (ir, 1000, fs), m100 = magAt (ir, 100, fs), m25 = magAt (ir, 25, fs);
        check (std::abs (m1k - 6.0) < 0.05, "sino +6 dB @ 1 kHz", m1k, 6.0);
        check (std::abs (m100 + 3.01) < 0.1, "low cut 24 dB/oct: −3 dB em 100 Hz", m100, -3.01);
        check (std::abs (m25 + 48.2) < 1.5, "low cut: −48 dB duas oitavas abaixo", m25, -48.2);

        // precisão dupla
        auto yd = run<double> (*p, imp);
        double dd = 0; for (size_t i = 0; i < 4096; ++i) dd = std::max (dd, std::abs (yd[i] - ir[i]));
        check (dd < 1e-6, "processamento em 64 bits igual ao de 32 bits", dd, 0);
    }

    std::printf ("Natural Phase\n");
    {
        auto p = make (44100);
        setP (*p, ids::phase, 1);
        waitConfig (*p);
        const int lat = p->getLatencySamples();
        check (lat > 0 && lat < 16, "latência reportada (poucas amostras)", lat, 4);
        // sem bandas + Delta: silêncio (o caminho seco passa pelos mesmos filtros de sobreamostragem)
        setP (*p, ids::delta, 1);
        pump (30);
        auto x = noise (44100, 3);
        auto y = run (*p, x);
        double mx = 0; for (size_t i = 22050; i < y.size(); ++i) mx = std::max (mx, std::abs (y[i]));
        check (mx < 1e-6, "Delta sem bandas é nulo (alinhamento exato)", mx, 0);
        setP (*p, ids::delta, 0);
        // sino a 18 kHz: resposta igual à analógica (sem compressão perto de Nyquist)
        setBand (*p, 0, mk (FilterType::Bell, 18000, 9, 2.0));
        pump (50);
        run (*p, std::vector<double> (9600, 0.0));
        std::vector<double> imp (16384, 0.0); imp[0] = 1.0;
        auto ir = run (*p, imp);
        std::vector<double> irs (ir.begin() + lat, ir.end());
        double worst = 0;
        for (double f : { 5000.0, 10000.0, 14000.0, 16000.0, 18000.0, 19000.0 })
            worst = std::max (worst, std::abs (magAt (irs, f, 44100) - analogMagnitudeDb (designAnalog (mk (FilterType::Bell, 18000, 9, 2), 9), f, 18000)));
        check (worst < 0.6, "Natural: sino 18 kHz igual ao analógico", worst, 0);
    }

    std::printf ("Linear Phase\n");
    {
        auto p = make();
        setP (*p, ids::phase, 2);
        waitConfig (*p);
        const int lat = p->getLatencySamples();
        check (lat == linearLatency (LinearQuality::Medium), "latência LP (média)", lat, linearLatency (LinearQuality::Medium));
        setBand (*p, 0, mk (FilterType::Bell, 2000, -8, 1.5));
        pump (30);
        run (*p, std::vector<double> (20000, 0.0));
        std::vector<double> imp (lat * 2 + 8192, 0.0); imp[100] = 1.0;
        auto ir = run (*p, imp);
        int pk = 0; for (int i = 0; i < (int) ir.size(); ++i) if (std::abs (ir[(size_t) i]) > std::abs (ir[(size_t) pk])) pk = i;
        check (pk == 100 + lat, "pico do impulso exatamente na latência", pk, 100 + lat);
        double asym = 0; for (int k = 1; k < 1500; ++k) asym = std::max (asym, std::abs (ir[(size_t) (pk + k)] - ir[(size_t) (pk - k)]));
        check (asym < 1e-6, "resposta simétrica (fase linear)", asym, 0);
        // bypass alinhado: saída = entrada atrasada pela latência
        setP (*p, ids::bypass, 1);
        pump (30);
        auto x = noise (lat + 30000, 5);
        auto y = run (*p, x);
        double e = 0; for (size_t i = (size_t) lat + 4000; i < y.size(); ++i) e = std::max (e, std::abs (y[i] - (double) (float) x[i - (size_t) lat]));
        check (e < 1e-6, "bypass alinhado em latência (troca sem saltos)", e, 0);
        setP (*p, ids::bypass, 0);
        // Delta LP: saída − original alinhado = só o que o EQ muda
        setP (*p, ids::delta, 1);
        pump (30);
        auto s = std::vector<double> ((size_t) (lat + 48000));
        for (size_t i = 0; i < s.size(); ++i) s[i] = 0.2 * std::sin (kTwoPi * 300 * (double) i / fs);   // longe do sino
        auto d = run (*p, s);
        double mx = 0; for (size_t i = (size_t) lat + 12000; i < d.size(); ++i) mx = std::max (mx, std::abs (d[i]));
        check (mx < 0.2 * 0.03, "Delta LP: a 300 Hz o EQ quase não mexe", mx, 0);
        setP (*p, ids::delta, 0);
    }

    std::printf ("Mid/Side\n");
    {
        auto p = make();
        auto b = mk (FilterType::LowCut, 200, 0, 0.7071, Slope::dB48); b.placement = Placement::Side;
        setBand (*p, 0, b);
        pump (30);
        // sinal mono de 60 Hz (só Mid): o corte no Side não lhe toca
        std::vector<double> s (48000);
        for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3 * std::sin (kTwoPi * 60 * (double) i / fs);
        auto y = run (*p, s);
        double e = 0; for (size_t i = 24000; i < y.size(); ++i) e = std::max (e, std::abs (y[i] - s[i]));
        check (e < 1e-5, "corte só no Side deixa o Mid intacto", e, 0);
    }

    std::printf ("Bandas dinâmicas e sidechain\n");
    {
        auto p = make (fs, 512, true);
        auto b = mk (FilterType::Bell, 1000, 0, 2);
        b.dynamic = true; b.thresholdDb = -40; b.ratio = 20; b.rangeDb = -10; b.kneeDb = 0; b.source = DetectorSource::Sidechain; b.attackMs = 1; b.releaseMs = 30;
        setBand (*p, 0, b);
        pump (30);
        std::vector<double> s (48000), sc (48000, 0.0);
        for (size_t i = 0; i < s.size(); ++i) s[i] = 0.25 * std::sin (kTwoPi * 1000 * (double) i / fs);
        auto y0 = run (*p, s, 512, &sc);
        double a0 = 0; for (size_t i = 24000; i < y0.size(); ++i) a0 = std::max (a0, std::abs (y0[i]));
        check (std::abs (gainToDb (a0 / 0.25)) < 0.1, "sidechain em silêncio: sem redução", gainToDb (a0 / 0.25), 0);
        for (size_t i = 0; i < sc.size(); ++i) sc[i] = 0.3 * std::sin (kTwoPi * 1000 * (double) i / fs);
        auto y1 = run (*p, s, 512, &sc);
        double a1 = 0; for (size_t i = 24000; i < y1.size(); ++i) a1 = std::max (a1, std::abs (y1[i]));
        check (std::abs (gainToDb (a1 / 0.25) + 10) < 0.5, "sidechain ativo: redução até ao range (−10 dB)", gainToDb (a1 / 0.25), -10);
        check (p->bandDynamicDb (0) < -9, "valor dinâmico publicado para a UI", p->bandDynamicDb (0), -10);
    }

    std::printf ("Auto gain, Character, Listen\n");
    {
        auto p = make();
        setBand (*p, 0, mk (FilterType::Bell, 1500, 9, 0.5));
        setP (*p, ids::autoGain, 1);
        pump (30);
        auto x = noise (48000 * 6, 9, 0.2);
        auto y = run (*p, x);
        double ex = 0, ey = 0; for (size_t i = 48000 * 4; i < x.size(); ++i) { ex += x[i] * x[i]; ey += y[i] * y[i]; }
        const double diff = 10 * std::log10 (ey / ex);
        check (std::abs (diff) < 1.5, "auto gain: comparação com a mesma loudness", diff, 0);
        setP (*p, ids::autoGain, 0);
        p->state.clearBands();
        setP (*p, ids::character, 0);
        pump (30);
        run (*p, noise (48000 * 2, 77));   // deixa o auto gain e as rampas assentarem
        auto c0 = run (*p, x);
        double e0 = 0; size_t ei = 0; for (size_t i = 0; i < x.size(); ++i) if (std::abs (c0[i] - (double) (float) x[i]) > e0) { e0 = std::abs (c0[i] - (double) (float) x[i]); ei = i; }
        if (e0 > 0) std::printf ("    (erro máximo na amostra %zu: %.6f vs %.6f, ganho auto %.5f dB)\n", ei, c0[ei], x[ei], p->autoGainDb());
        check (e0 == 0.0, "Character Clean: sem harmónicos (nulo exato)", e0, 0);
        setP (*p, ids::character, 2);
        pump (30);
        auto c2 = run (*p, x);
        double e2 = 0; for (size_t i = 48000; i < x.size(); ++i) e2 = std::max (e2, std::abs (c2[i] - (double) (float) x[i]));
        check (e2 > 1e-5 && e2 < 0.05, "Character Warm: cor ligeira", e2, 0.005);
        setP (*p, ids::character, 0);
        // Listen da banda: só a zona da banda passa
        setBand (*p, 0, mk (FilterType::Bell, 4000, 3, 4));
        p->setListenBand (0);
        pump (30);
        std::vector<double> lo (48000);
        for (size_t i = 0; i < lo.size(); ++i) lo[i] = 0.3 * std::sin (kTwoPi * 100 * (double) i / fs);
        auto l = run (*p, lo);
        double al = 0; for (size_t i = 24000; i < l.size(); ++i) al = std::max (al, std::abs (l[i]));
        check (gainToDb (al / 0.3) < -25, "Listen numa banda a 4 kHz quase cala os 100 Hz", gainToDb (al / 0.3), -30);
        p->setListenBand (-1);
    }

    std::printf ("Login\n");
    {
        auto p = make();
        setBand (*p, 0, mk (FilterType::Bell, 1000, 12, 1));
        pump (30);
        lic::License::get().lock();
        auto x = noise (24000, 4);
        auto y = run (*p, x);
        double e = 0; for (size_t i = 4000; i < x.size(); ++i) e = std::max (e, std::abs (y[i] - (double) (float) x[i]));
        check (e < 1e-6, "sem sessão: áudio passa sem processar", e, 0);
        bool expired = false;
        check (lic::License::matchPassword ("senha-errada", expired) < 0, "senha errada rejeitada", 0, 0);
        lic::License::get().unlockForTests();
    }

    std::printf ("Estado, desfazer, A/B, copiar/colar, presets\n");
    {
        auto p = make();
        auto& st = p->state;
        st.performAction ([&] { st.addBand (mk (FilterType::Bell, 500, -3, 2)); });
        st.performAction ([&] { st.addBand (mk (FilterType::HighShelf, 9000, 2, 0.7)); });
        check (st.activeBandCount() == 2, "duas bandas criadas", st.activeBandCount(), 2);
        st.undo();
        check (st.activeBandCount() == 1, "desfazer remove a última", st.activeBandCount(), 1);
        st.redo();
        check (st.activeBandCount() == 2, "refazer repõe", st.activeBandCount(), 2);
        // A/B
        st.switchSlot (1);
        st.performAction ([&] { st.clearBands(); });
        check (st.activeBandCount() == 0, "B limpo", st.activeBandCount(), 0);
        st.switchSlot (0);
        check (st.activeBandCount() == 2, "A mantém as bandas", st.activeBandCount(), 2);
        // estado guardado/restaurado
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        auto q = make();
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        pump (100);
        check (q->state.activeBandCount() == 2, "estado restaurado: bandas", q->state.activeBandCount(), 2);
        check (std::abs (q->state.band (1).freq - 9000) < 1, "estado restaurado: frequência", q->state.band (1).freq, 9000);
        q->state.switchSlot (1);
        check (q->state.activeBandCount() == 0, "estado restaurado: slot B", q->state.activeBandCount(), 0);
        // copiar / colar entre instâncias
        st.copyBands ({ 0, 1 });
        auto pasted = q->state.pasteBands();
        check (pasted.size() == 2 && q->state.band (pasted[1]).type == FilterType::HighShelf, "copiar/colar bandas entre instâncias", pasted.size(), 2);

        // presets de fábrica: todos carregam e soam com valores finitos
        int ok = 0;
        const auto x = noise (24000, 21, 0.3);
        for (auto& pr : presets::factory())
        {
            p->state.loadFactory (pr);
            pump (40);
            run (*p, std::vector<double> (2048, 0.0));
            auto y = run (*p, x);
            bool fine = true; double pk = 0;
            for (double v : y) { fine &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
            if (fine && pk < 8.0 && p->state.activeBandCount() == (int) pr.bands.size()) ++ok;
            else std::printf ("    preset com problema: %s\n", pr.name.toRawUTF8());
        }
        check (ok == (int) presets::factory().size(), "presets de fábrica carregam e processam", ok, (double) presets::factory().size());
        // preset do utilizador
        p->state.loadFactory (presets::factory()[1]);
        check (p->state.saveUser ("Teste EQMotion"), "guardar preset do utilizador", 1, 1);
        p->state.performAction ([&] { p->state.clearBands(); });
        auto f = p->presetManager.userFolder().getChildFile ("Teste EQMotion.eqmpreset");
        const bool loaded = p->state.loadUser (f);
        if (! loaded || p->state.activeBandCount() == 0) std::printf ("%s\n", f.loadFileAsString().substring (0, 1500).toRawUTF8());
        check (loaded && p->state.activeBandCount() == (int) presets::factory()[1].bands.size(), "abrir preset do utilizador", p->state.activeBandCount(), (double) presets::factory()[1].bands.size());
        f.deleteFile();
    }

    std::printf ("Smart Ops ponta a ponta\n");
    {
        auto p = make();
        p->state.performAction ([&] { p->state.addBand (mk (FilterType::LowShelf, 120, 2, 0.7)); });   // edição manual
        InstanceRegistry::get().editorOpened();
        p->smartSetOp (SmartOp::Deresonate);
        p->smartStartLearning();
        std::vector<double> x ((size_t) (fs * 5));
        std::mt19937 rng (2); std::normal_distribution<double> nd (0, 0.05);
        for (size_t i = 0; i < x.size(); ++i) x[i] = nd (rng) + 0.03 * std::sin (kTwoPi * 1850 * (double) i / fs);
        for (size_t s = 0; s < x.size(); s += 24000)
        {
            std::vector<double> part (x.begin() + (long) s, x.begin() + (long) std::min (x.size(), s + 24000));
            run (*p, part);
            pump (30);
        }
        pump (300);
        check (p->smartLearnSeconds() > 4.5, "aprendeu durante a reprodução", p->smartLearnSeconds(), 5);
        p->smartStopLearning();
        const auto& sug = p->smart.suggestion;
        bool found = false; for (auto& b : sug.bands) found |= std::abs (std::log2 (b.freq / 1850)) < 0.06;
        check (sug.ok && found, "deresonate encontrou a ressonância de 1850 Hz", sug.bands.size(), 1);
        check (p->state.activeBandCount() == 1, "nada muda antes de Aplicar (só pré-visualização)", p->state.activeBandCount(), 1);
        const int n = (int) sug.bands.size();
        p->smartApply();
        check (p->state.activeBandCount() == 1 + n, "Aplicar cria as bandas sugeridas", p->state.activeBandCount(), 1 + n);
        // edição manual depois de aplicar
        p->state.performAction ([&] { p->state.addBand (mk (FilterType::Bell, 6000, 1, 1)); });
        p->smartRevert();
        check (p->state.activeBandCount() == 2, "Reverter remove só as bandas da operação", p->state.activeBandCount(), 2);
        check (p->state.band (0).type == FilterType::LowShelf, "edição manual anterior intacta", 1, 1);
        p->state.undo();   // desfaz o Reverter (um passo)
        check (p->state.activeBandCount() == 2 + n, "Reverter é um passo de desfazer", p->state.activeBandCount(), 2 + n);
        InstanceRegistry::get().editorClosed();
    }

    std::printf ("Robustez: taxas de amostragem, blocos irregulares, mudanças rápidas\n");
    {
        bool allFinite = true;
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
            for (int mode = 0; mode < 3; ++mode)
            {
                auto p = make (sr, 256);
                setP (*p, ids::phase, (float) mode);
                for (int b = 0; b < 24; ++b)
                {
                    auto band = mk ((FilterType) (b % 9), 30.0 * std::pow (1.32, b), (b % 5) * 3 - 6, 0.5 + b * 0.3, (Slope) (b % 9));
                    band.dynamic = b % 3 == 0; band.thresholdDb = -30; band.placement = (Placement) (b % 5);
                    setBand (*p, b, band);
                }
                waitConfig (*p);
                std::mt19937 rng (1);
                auto x = noise ((int) sr, 3, 0.5);
                std::vector<double> out;
                juce::AudioBuffer<float> buf (2, 4096);
                juce::MidiBuffer midi;
                for (size_t s = 0; s < x.size();)
                {
                    const int m = std::min<int> ((int) (rng() % 3000) + 1, (int) (x.size() - s));
                    buf.setSize (2, m, false, false, true);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < m; ++i) buf.setSample (c, i, (float) x[s + (size_t) i]);
                    // automação brusca
                    setP (*p, ids::band (rng() % 24, ids::B::Freq), 20.0f + (float) (rng() % 20000));
                    setP (*p, ids::band (rng() % 24, ids::B::Type), (float) (rng() % 9));
                    p->processBlock (buf, midi);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < m; ++i) allFinite &= std::isfinite (buf.getSample (c, i)) && std::abs (buf.getSample (c, i)) < 1000.0f;
                    s += (size_t) m;
                }
            }
        check (allFinite, "24 bandas, todas as taxas e modos, blocos de 1–3000 amostras: sem NaN/Inf", allFinite, 1);

        // reabrir a sessão com outra taxa/bloco
        auto p = make (44100, 64);
        setBand (*p, 0, mk (FilterType::Bell, 1000, 6, 1));
        p->prepareToPlay (96000, 1024);
        auto y = run (*p, noise (20000));
        bool fin = true; for (double v : y) fin &= std::isfinite (v);
        check (fin, "mudança de taxa/bloco (suspender/retomar)", fin, 1);
    }

    std::printf ("CPU (1 s de áudio estéreo a 48 kHz)\n");
    {
        auto bench = [&] (int mode, int nb, bool dyn) -> double
        {
            auto p = make (48000, 512);
            setP (*p, ids::phase, (float) mode);
            for (int b = 0; b < nb; ++b)
            {
                auto band = mk (b % 4 == 0 ? FilterType::LowCut : FilterType::Bell, 40.0 * std::pow (1.35, b), 2, 1.2, Slope::dB24);
                band.dynamic = dyn && band.type == FilterType::Bell;
                setBand (*p, b, band);
            }
            waitConfig (*p);
            auto x = noise (48000);
            const auto t0 = std::chrono::high_resolution_clock::now();
            run (*p, x);
            const auto t1 = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<double, std::milli> (t1 - t0).count();
        };
        const double z8 = bench (0, 8, false), z24 = bench (0, 24, true), n8 = bench (1, 8, false), n24 = bench (1, 24, true), l8 = bench (2, 8, false), l24 = bench (2, 24, true);
        std::printf ("    Zero Latency: 8 bandas %.1f ms · 24 dinâmicas %.1f ms\n", z8, z24);
        std::printf ("    Natural:      8 bandas %.1f ms · 24 dinâmicas %.1f ms\n", n8, n24);
        std::printf ("    Linear:       8 bandas %.1f ms · 24 dinâmicas %.1f ms\n", l8, l24);
        check (z8 < 30 && l24 < 250 && n24 < 400, "carga de CPU dentro do esperado (ms por segundo de áudio)", z8, 30);
    }

    std::printf ("Interface\n");
    {
        auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("docs");
        out.createDirectory();
        auto p = make();
        p->state.loadFactory (presets::factory()[1]);   // "Voz rap presente"
        snapshot (*p, "docs/ui_principal.png", [] (EQMotionEditor& e) { e.getGraph().selectBand (3); });

        auto q = make (fs, 512, true);
        auto b = mk (FilterType::Bell, 2400, 0, 4); b.dynamic = true; b.thresholdDb = -60; b.rangeDb = -9;
        q->state.setBand (0, mk (FilterType::LowCut, 35, 0, 0.7071, Slope::dB24));
        q->state.setBand (1, b);
        q->state.setBand (2, mk (FilterType::HighShelf, 11000, 2.5, 0.6));
        q->smartSetOp (SmartOp::Deresonate);
        InstanceRegistry::get().editorOpened();
        q->smartStartLearning();
        std::vector<double> x ((size_t) (fs * 3));
        std::mt19937 rng (5); std::normal_distribution<double> nd (0, 0.06);
        for (size_t i = 0; i < x.size(); ++i) x[i] = nd (rng) + 0.04 * std::sin (kTwoPi * 820 * (double) i / fs) + 0.03 * std::sin (kTwoPi * 3300 * (double) i / fs);
        for (size_t s0 = 0; s0 < x.size(); s0 += 12000)
        {
            std::vector<double> part (x.begin() + (long) s0, x.begin() + (long) std::min (x.size(), s0 + 12000));
            run (*q, part); pump (40);
        }
        pump (200);
        q->smartStopLearning();
        InstanceRegistry::get().editorClosed();
        snapshot (*q, "docs/ui_smart_ops.png", [] (EQMotionEditor& e) { e.openSmartOps (true); e.getGraph().selectBand (1); });

        auto r = make();
        auto m = mk (FilterType::LowCut, 120, 0, 0.7071, Slope::dB24); m.placement = Placement::Side;
        auto mm = mk (FilterType::Bell, 2500, 2, 0.8); mm.placement = Placement::Mid;
        auto ms = mk (FilterType::HighShelf, 9000, 3, 0.7); ms.placement = Placement::Side;
        r->state.setBand (0, m); r->state.setBand (1, mm); r->state.setBand (2, ms);
        setP (*r, ids::procMode, 2);
        setP (*r, ids::phase, 2);
        snapshot (*r, "docs/ui_mid_side_linear.png", [] (EQMotionEditor& e) { e.getGraph().selectBand (1); });

        // tamanho mínimo e janelas flutuantes
        {
            auto sm = make();
            sm->state.loadFactory (presets::factory()[17]);
            std::unique_ptr<juce::AudioProcessorEditor> ed (sm->createEditor());
            ed->setSize (1100, 680);
            ed->getChildComponent (0);
            if (auto* e = dynamic_cast<EQMotionEditor*> (ed.get())) e->getGraph().selectBand (1);
            pump (300);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile ("docs/ui_minimo.png"));
            f.deleteFile(); { juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os); }
            auto shot = [] (juce::Component& c, const char* file)
            {
                pump (100);
                auto im = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
                juce::File ff (juce::File::getCurrentWorkingDirectory().getChildFile (file));
                ff.deleteFile(); juce::FileOutputStream os (ff); juce::PNGImageFormat().writeImageToStream (im, os);
                std::printf ("  captura: %s\n", file);
            };
            EQMLookAndFeel lf;
            PresetBrowser pb (*sm); pb.setLookAndFeel (&lf); shot (pb, "docs/ui_presets.png"); pb.setLookAndFeel (nullptr);
            SettingsPanel sp (*sm, [] (float) {}, [] (float) {}, 12.0f); sp.setLookAndFeel (&lf); shot (sp, "docs/ui_definicoes.png"); sp.setLookAndFeel (nullptr);
        }

        lic::License::get().lock();
        auto l = make();
        snapshot (*l, "docs/ui_login.png");
        lic::License::get().unlockForTests();
    }

    std::printf ("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
