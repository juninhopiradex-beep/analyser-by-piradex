#include "PluginProcessor.h"
#include "PluginEditor.h"

using View = AnalysisResults::View;
static juce::String U8 (const char* s) { return juce::String::fromUTF8 (s); }

// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CurveAnalyzerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const String& id, const String& name, StringArray items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto toggle = [&] (const String& id, const String& name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    choice (ids::role,      "Papel",       { "Analisador", "Gerador", "Host" }, 0);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ids::group, 1 }, "Grupo", 1, pca::GroupRegistry::kNumGroups, 1));
    choice (ids::signal,    "Sinal",       { "Impulso", "Sweep log", U8 ("Ruído rosa"), U8 ("Seno (harmónicos)") }, 1);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::level, 1 }, U8 ("Nível"),
                                                  NormalisableRange<float> (-60.0f, 0.0f, 0.5f), -18.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dBFS")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::sineFreq, 1 }, "Freq. seno",
                                                  NormalisableRange<float> (20.0f, 10000.0f, 1.0f, 0.3f), 1000.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")));
    choice (ids::fftSize,   "FFT",         { "4096", "8192", "16384", "32768", "65536" }, 1);
    choice (ids::source,    "Fonte",       { "Gerador", U8 ("Música") }, 0);
    choice (ids::channel,   "Canal",       { "Esquerdo", "Direito", "Mid", "Side" }, 0);
    choice (ids::averages,  U8 ("Média"),  { "Auto", "1", "2", "4", "8", "16", "32", U8 ("∞") }, 0);
    choice (ids::smoothing, U8 ("Suavização"), { "Nenhuma", "1/24 oit.", "1/12 oit.", "1/6 oit.", "1/3 oit." }, 0);
    choice (ids::range,     "Escala",      { U8 ("±3 dB"), U8 ("±6 dB"), U8 ("±12 dB"), U8 ("±24 dB"), U8 ("±48 dB") }, 2);
    toggle (ids::showPhase, "Fase",        true);
    toggle (ids::autoSync,  "Auto Sync",   true);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ids::latency, 1 }, U8 ("Latência extra"), -4096, 4096, 0));
    toggle (ids::freeze,    "Congelar",    false);
    toggle (ids::muteOut,   U8 ("Silenciar saída"), false);
    choice (ids::view,      "Vista",       { "Curva", "Onda", "Espetro", "Varrimento" }, 0);
    return l;
}

// =============================================================================
CurveAnalyzerProcessor::CurveAnalyzerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Entrada",   juce::AudioChannelSet::stereo(), true)
                        .withOutput (U8 ("Saída"), juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "AnalyserByPiradex", createLayout())
{
    lic::License::get();   // carrega o estado da licença na thread de mensagens
    juce::addDefaultFormatsToManager (formatManager);
    ringWet.assign ((size_t) kRingSize, 0.0f);
    ringDry.assign ((size_t) kRingSize, 0.0f);
    sweepLevels = pca::sweepValues (pca::SweepKind::Level);
    sweepFreqs  = pca::sweepValues (pca::SweepKind::Frequency);
    hostBuf.setSize (2, 512);
    genScratch.assign (512, 0.0f);
    worker.startThread (juce::Thread::Priority::low);
    startTimerHz (20);
}

CurveAnalyzerProcessor::~CurveAnalyzerProcessor()
{
    stopTimer();
    worker.stopThread (3000);
    hostWindow.reset();
    {
        const juce::ScopedLock sl (hostLock);
        hosted.reset();
    }
}

bool CurveAnalyzerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

pca::ExcitationSpec CurveAnalyzerProcessor::ownSpec() const
{
    pca::ExcitationSpec s;
    s.N       = fftSizeForIndex (paramIndex (ids::fftSize));
    s.fs      = sampleRate.load();
    s.signal  = (pca::Signal) juce::jlimit (0, 3, paramIndex (ids::signal));
    s.levelDb = paramValue (ids::level);
    s.sineHz  = paramValue (ids::sineFreq);
    return s;
}

pca::GroupSlot& CurveAnalyzerProcessor::slotForRole (Role r)
{
    return r == Role::Host ? hostSlot : pca::GroupRegistry::get (paramIndex (ids::group));
}

void CurveAnalyzerProcessor::prepareToPlay (double sr, int block)
{
    sampleRate.store (sr);
    maxBlock.store (juce::jmax (64, block));
    prepareHostBuffers();
    timerCallback();   // gera já o sinal de teste com a nova taxa de amostragem
}

void CurveAnalyzerProcessor::prepareHostBuffers()
{
    const double sr = sampleRate.load();
    const int bs = maxBlock.load();
    const juce::ScopedLock sl (hostLock);
    int nch = 2;
    if (hosted != nullptr)
    {
        nch = juce::jmax (2, hosted->getTotalNumInputChannels(), hosted->getTotalNumOutputChannels());
        if (sr > 0.0)
        {
            hosted->setRateAndBufferSizeDetails (sr, bs);
            hosted->prepareToPlay (sr, bs);
        }
    }
    hostBuf.setSize (nch, bs, false, true, false);
    genScratch.assign ((size_t) bs, 0.0f);
}

// Reconstrói o período do sinal de teste quando as definições mudam (fora da thread de áudio)
void CurveAnalyzerProcessor::timerCallback()
{
    lic::License::get().checkExpiry();
    if (role() == Role::Analyser || sampleRate.load() <= 0.0) return;
    const auto spec = ownSpec();
    if (genValid && spec == genSpec) return;

    auto ex = pca::makeExcitation (spec);
    {
        const juce::ScopedLock sl (genLock);
        genSignal.swap (ex.x);
        genSpec = spec;
        genValid = true;
        genPos = 0;
    }
}

// =============================================================================
//  Varrimentos
// =============================================================================
void CurveAnalyzerProcessor::requestSweep (pca::SweepKind kind)
{
    slotForRole (role()).sweepRequest.store ((int) kind);
}

void CurveAnalyzerProcessor::stopSweep()
{
    slotForRole (role()).sweepRequest.store (-1);
}

// Relógio partilhado: com o DAW a tocar usa a posição da timeline (igual para todas as
// instâncias da faixa), senão um contador próprio.
int64_t CurveAnalyzerProcessor::blockClock (int64_t ownCounter, bool& fromTimeline)
{
    fromTimeline = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying())
                if (auto t = pos->getTimeInSamples())
                {
                    fromTimeline = true;
                    return *t;
                }
    return ownCounter;
}

// Gera numSamples do sinal de teste (thread de áudio). Publica no 'slot' o que está a emitir.
// O sinal é função do relógio: amostra (clock + n) do período -> o Analisador sabe o alinhamento.
void CurveAnalyzerProcessor::renderGenerator (float* dst, int numSamples, pca::GroupSlot& slot, int64_t clock)
{
    const double fs = sampleRate.load();
    const int    fftIdx = paramIndex (ids::fftSize);
    const int    N = fftSizeForIndex (fftIdx);
    int          sig = paramIndex (ids::signal);
    double       level = paramValue (ids::level);
    double       hz = paramValue (ids::sineFreq);

    // ---- máquina de estados do varrimento ----
    const int req = slot.sweepRequest.exchange (0);
    if (req > 0)
    {
        sweepKindRun = req; sweepStepRun = 0; sweepStepSamples = 0;
        slot.sweepRunId.fetch_add (1);
    }
    else if (req < 0)
    {
        sweepKindRun = 0; sweepStepRun = -1;
    }
    if (sweepKindRun != 0)
    {
        const auto& vals = sweepKindRun == 1 ? sweepLevels : sweepFreqs;
        const int64_t hold = 4 * (int64_t) N + (int64_t) (0.25 * fs);
        sweepStepSamples += numSamples;
        if (sweepStepSamples >= hold) { ++sweepStepRun; sweepStepSamples = 0; }
        if (sweepStepRun >= (int) vals.size()) { sweepKindRun = 0; sweepStepRun = -1; }
        else
        {
            sig = 3;
            if (sweepKindRun == 1) level = vals[(size_t) sweepStepRun];
            else                   hz    = vals[(size_t) sweepStepRun];
        }
    }

    slot.signal.store (sig);
    slot.fftIndex.store (fftIdx);
    slot.levelDb.store ((float) level);
    slot.sineHz.store ((float) hz);
    slot.sampleRate.store (fs);
    slot.sweepKind.store (sweepKindRun);
    slot.sweepStep.store (sweepStepRun);
    slot.lastSeenMs.store (juce::Time::getMillisecondCounter());

    if (sig == 3)
    {
        // Seno sintetizado, exatamente no centro de um bin (periódico em N)
        const int k = pca::sineBinFor (hz, N, fs > 0 ? fs : 48000.0);
        const double A = pca::dbToGain (level);
        for (int n = 0; n < numSamples; ++n)
        {
            const int64_t idx = (((clock + n) % N) + N) % N;
            dst[n] = (float) (A * std::sin (2.0 * pca::kPi * (double) ((k * idx) % N) / N));
        }
        return;
    }

    const juce::ScopedTryLock lock (genLock);
    if (lock.isLocked() && genValid && ! genSignal.empty())
    {
        const int64_t len = (int64_t) genSignal.size();
        for (int n = 0; n < numSamples; ++n)
            dst[n] = genSignal[(size_t) ((((clock + n) % len) + len) % len)];
    }
    else
    {
        std::fill (dst, dst + numSamples, 0.0f);
    }
}

// Escreve no buffer circular o canal escolhido (wet = saída do dispositivo, dry = referência)
void CurveAnalyzerProcessor::writeRing (const float* wl, const float* wr, const float* dl, const float* dr, int64_t at, int n)
{
    const int chanMode = paramIndex (ids::channel);
    auto pick = [chanMode] (const float* l, const float* r, int i) -> float
    {
        if (l == nullptr) return 0.0f;
        const float a = l[i], b = r != nullptr ? r[i] : a;
        switch (chanMode)
        {
            case 1:  return b;
            case 2:  return 0.5f * (a + b);
            case 3:  return 0.5f * (a - b);
            default: return a;
        }
    };
    for (int i = 0; i < n; ++i)
    {
        const size_t idx = (size_t) ((at + i) & kRingMask);
        ringWet[idx] = pick (wl, wr, i);
        ringDry[idx] = pick (dl, dr, i);
    }
}

// =============================================================================
void CurveAnalyzerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const Role r = role();

    // Sem sessão iniciada: o plugin é transparente (não estraga a sessão do DAW)
    if (! lic::License::get().isUnlocked())
    {
        for (int ch = mainIn.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
        return;
    }

    // ---------------- Gerador ----------------
    if (r == Role::Generator)
    {
        if (mainOut.getNumChannels() > 0)
        {
            float* d = mainOut.getWritePointer (0);
            bool tl = false;
            const int64_t clock = blockClock (genClock, tl);
            renderGenerator (d, numSamples, pca::GroupRegistry::get (paramIndex (ids::group)), clock);
            for (int ch = 1; ch < mainOut.getNumChannels(); ++ch)
                mainOut.copyFrom (ch, 0, d, numSamples);
        }
        genClock += numSamples;
        for (int ch = mainOut.getNumChannels(); ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
        return;
    }

    // ---------------- Host: plugin carregado dentro do ANALYSER ----------------
    if (r == Role::Host)
    {
        const bool music = paramIndex (ids::source) == 1;
        const juce::ScopedTryLock hl (hostLock);
        const int cap = juce::jmin (hostBuf.getNumSamples(), (int) genScratch.size());
        int64_t at = writeCount.load (std::memory_order_relaxed);

        for (int off = 0; off < numSamples && cap > 0; off += cap)
        {
            const int n = juce::jmin (cap, numSamples - off);
            hostBuf.clear();
            // entrada do plugin interno: sinal de teste ou a música da faixa
            if (music)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    const int src = juce::jmin (ch, mainIn.getNumChannels() - 1);
                    if (src >= 0) hostBuf.copyFrom (ch, 0, mainIn, src, off, n);
                }
            }
            else
            {
                renderGenerator (genScratch.data(), n, hostSlot, at);   // relógio = índice do anel
                hostBuf.copyFrom (0, 0, genScratch.data(), n);
                hostBuf.copyFrom (1, 0, genScratch.data(), n);
            }

            // guarda o dry no anel antes de processar (o buffer é processado no lugar)
            writeRing (nullptr, nullptr, hostBuf.getReadPointer (0), hostBuf.getReadPointer (1), at, n);

            if (hl.isLocked() && hosted != nullptr)
            {
                juce::AudioBuffer<float> view (hostBuf.getArrayOfWritePointers(), hostBuf.getNumChannels(), n);
                hostMidi.clear();
                hosted->processBlock (view, hostMidi);
            }

            // wet = saída do plugin interno (os valores dry já estão no anel: reescreve só o wet)
            const int chanMode = paramIndex (ids::channel);
            const float* wl = hostBuf.getReadPointer (0);
            const float* wr = hostBuf.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float a = wl[i], b = wr[i];
                ringWet[(size_t) ((at + i) & kRingMask)] = chanMode == 1 ? b : chanMode == 2 ? 0.5f * (a + b)
                                                         : chanMode == 3 ? 0.5f * (a - b) : a;
            }

            // na música ouve-se o plugin; com o gerador a faixa passa intacta
            if (music)
                for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
                    mainOut.copyFrom (ch, off, hostBuf, juce::jmin (ch, hostBuf.getNumChannels() - 1), 0, n);

            at += n;
        }
        clockDelta.store (0);
        clockFromTimeline.store (true);
        writeCount.store (at, std::memory_order_release);
        sidechainActive.store (true);

        if (paramIndex (ids::muteOut) == 1) mainOut.clear();
        for (int ch = mainIn.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
        return;
    }

    // ---------------- Analisador ----------------
    const float* inL = mainIn.getNumChannels() > 0 ? mainIn.getReadPointer (0) : nullptr;
    const float* inR = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : nullptr;
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
    {
        const int first = scBus->getChannelIndexInProcessBlockBuffer (0);
        const int nch   = scBus->getNumberOfChannels();
        if (first >= 0 && first < buffer.getNumChannels())
        {
            scL = buffer.getReadPointer (first);
            if (nch > 1 && first + 1 < buffer.getNumChannels()) scR = buffer.getReadPointer (first + 1);
        }
    }
    sidechainActive.store (scL != nullptr);

    const int64_t base = writeCount.load (std::memory_order_relaxed);
    bool tl = false;
    const int64_t clock = blockClock (base, tl);
    clockDelta.store (clock - base);
    clockFromTimeline.store (tl);
    writeRing (inL, inR, scL, scR, base, numSamples);
    writeCount.store (base + numSamples, std::memory_order_release);

    if (paramIndex (ids::muteOut) == 1) mainOut.clear();
    for (int ch = mainIn.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

// =============================================================================
//  Thread de análise
// =============================================================================
void CurveAnalyzerProcessor::runAnalysis()
{
    const double fs = sampleRate.load();
    if (fs <= 0.0 || ! lic::License::get().isUnlocked()) return;

    const Role rl = role();
    AnalysisKey key;
    key.role    = (int) rl;
    key.source  = paramIndex (ids::source);
    key.channel = paramIndex (ids::channel);
    key.spec    = ownSpec();

    // De onde vêm as definições do sinal de teste
    bool linked = false;
    pca::GroupSlot* slot = nullptr;
    if (rl == Role::Analyser && key.source == 0)
    {
        auto& s = pca::GroupRegistry::get (paramIndex (ids::group));
        const uint32_t now = juce::Time::getMillisecondCounter();
        const uint32_t seen = s.lastSeenMs.load();
        if (seen != 0 && now - seen < 1500 && std::abs (s.sampleRate.load() - fs) < 1.0)
        {
            linked = true;
            slot = &s;
        }
    }
    else if (rl == Role::Host && key.source == 0 && hostSlot.lastSeenMs.load() != 0)
    {
        slot = &hostSlot;     // o gerador interno publica aqui (inclui varrimentos)
    }
    if (slot != nullptr)
    {
        key.spec.signal  = (pca::Signal) juce::jlimit (0, 3, slot->signal.load());
        key.spec.N       = fftSizeForIndex (slot->fftIndex.load());
        key.spec.levelDb = slot->levelDb.load();
        key.spec.sineHz  = slot->sineHz.load();
    }
    if (key.source == 1)   // música: sinal irrelevante, só conta o N
    {
        key.spec.signal = pca::Signal::Impulse;
        key.spec.levelDb = 0.0;
        key.spec.sineHz = 1000.0;
    }

    const int N = key.spec.N;
    const bool music = key.source == 1;
    const bool harmonics = ! music && key.spec.signal == pca::Signal::Sine;
    // Sinais periódicos: qualquer janela de N amostras é um período completo,
    // por isso os blocos podem sobrepor-se (N/4) -> curva 4x mais rápida.
    const int hop = music ? N / 2 : N / 4;

    // ---- (Re)configuração --------------------------------------------------
    if (! haveKey || key != curKey)
    {
        const bool sameShape = haveKey && key.role == curKey.role && key.source == curKey.source && key.spec.N == curKey.spec.N;
        curKey = key;
        haveKey = true;
        frameWet.assign ((size_t) N, 0.0f);
        frameDry.assign ((size_t) N, 0.0f);
        if (rl != Role::Generator)
        {
            if (music)
                xferAn.configure (N);
            else
            {
                excitation = std::make_unique<pca::Excitation> (pca::makeExcitation (key.spec));
                if (harmonics) harmAn.configure (*excitation);
                else           respAn.configure (*excitation);
            }
        }
        if (! sameShape) { musicDelay = 0; heldDelay = 0.0; }
        // Ignora blocos que ainda contêm o sinal antigo (inclui folga para latência)
        const int64_t S = writeCount.load (std::memory_order_acquire);
        skipUntil = S + (music ? 0 : N + N / 2);
        nextFrame = (skipUntil + N) / hop + 1;
    }

    if (rl == Role::Generator)
    {
        auto& gs = pca::GroupRegistry::get (paramIndex (ids::group));
        const int sk = gs.sweepKind.load(), st = gs.sweepStep.load();
        const juce::ScopedLock sl (resultsLock);
        if (results.view != View::Generator || ! (results.spec == key.spec)
            || results.sweepKind != sk || results.sweepStep != st)
        {
            const auto serial = results.serial;
            results = AnalysisResults();
            results.view = View::Generator;
            results.spec = key.spec;
            results.sweepKind = sk;
            results.sweepStep = st;
            results.sweepTotal = (int) pca::sweepValues ((pca::SweepKind) sk).size();
            results.serial = serial + 1;
        }
        return;
    }

    if (resetRequested.exchange (false))
    {
        respAn.reset(); harmAn.reset(); xferAn.reset();
        musicDelay = 0;
        sweepPts.clear(); sweepPtsKind = 0;
    }

    const int avgIdx = paramIndex (ids::averages);
    const int avg = averagesForIndex (avgIdx);
    respAn.setAverages (avg);
    respAn.setAdaptive (avgIdx == 0);
    harmAn.setAverages (avg);
    harmAn.setAdaptive (avgIdx == 0);
    xferAn.setAverages (avg == 0 ? 0 : std::max (avg, avgIdx == 0 ? 16 : 4));   // Welch precisa de várias médias

    const bool frozen  = paramIndex (ids::freeze) == 1;
    const bool autoSync = paramIndex (ids::autoSync) == 1;
    const int  latencyExtra = paramIndex (ids::latency);
    const bool scMissing = music && rl == Role::Analyser && ! sidechainActive.load();

    // ---- Consumir blocos disponíveis --------------------------------------------
    const int64_t S = writeCount.load (std::memory_order_acquire);
    const int64_t safety = 32768;
    int processed = 0;
    while (! worker.threadShouldExit())
    {
        const int64_t e = nextFrame * hop;
        if (e > S) break;
        const int64_t oldest = e - N - (music ? musicDelay : 0);
        if (oldest < 0 || e - N < skipUntil) { ++nextFrame; continue; }
        if (S - oldest > kRingSize - safety || frozen)
        {
            nextFrame = S / hop + 1;   // atrasou-se demasiado (ou congelado): salta para o presente
            break;
        }
        for (int i = 0; i < N; ++i)
            frameWet[(size_t) i] = ringWet[(size_t) ((e - N + i) & kRingMask)];

        if (music)
        {
            for (int i = 0; i < N; ++i)
                frameDry[(size_t) i] = ringDry[(size_t) ((e - N - musicDelay + i) & kRingMask)];
            if (xferAn.addFrames (frameDry.data(), frameWet.data()) && autoSync && xferAn.frames() >= 4)
            {
                double prom = 0.0;
                const int r = xferAn.residualLag (&prom);
                if (r != 0 && prom > 6.0)
                {
                    const int nd = juce::jlimit (0, kRingSize / 2, musicDelay + r);
                    if (nd != musicDelay) { musicDelay = nd; xferAn.reset(); }
                }
            }
        }
        else if (harmonics) harmAn.addFrame (frameWet.data());
        else                respAn.addFrame (frameWet.data(), (e - N) + clockDelta.load());

        ++nextFrame;
        if (++processed > 64) break;
    }

    // ---- Varrimento: guarda o resultado do passo em curso -------------------------
    pca::GroupSlot* sweepSlot = (rl == Role::Host) ? &hostSlot : (linked ? slot : nullptr);
    int sweepKindNow = 0, sweepStepNow = -1;
    if (sweepSlot != nullptr)
    {
        sweepKindNow = sweepSlot->sweepKind.load();
        sweepStepNow = sweepSlot->sweepStep.load();
        const int run = sweepSlot->sweepRunId.load();
        if (sweepKindNow != 0 && run != sweepRunSeen)
        {
            sweepRunSeen = run;
            sweepPtsKind = sweepKindNow;
            const auto vals = pca::sweepValues ((pca::SweepKind) sweepKindNow);
            sweepPts.assign (vals.size(), {});
            for (size_t i = 0; i < vals.size(); ++i) sweepPts[i].x = vals[i];
        }
        if (sweepKindNow != 0 && harmonics && sweepStepNow >= 0 && sweepStepNow < (int) sweepPts.size()
            && harmAn.frames() >= 3)
        {
            const auto h = harmAn.analyse();
            auto& p = sweepPts[(size_t) sweepStepNow];
            p.thdDb  = pca::ampToDb (h.thdPct / 100.0);
            p.h2Db   = h.present[2] ? h.relDb[2] : -200.0;
            p.h3Db   = h.present[3] ? h.relDb[3] : -200.0;
            p.gainDb = h.gainDb;
            p.valid  = true;
        }
    }

    // ---- Publicar resultados (~30 Hz) -------------------------------------------------
    const uint32_t now = juce::Time::getMillisecondCounter();
    if (now - lastPublishMs < 33) return;
    lastPublishMs = now;

    AnalysisResults r;
    r.spec   = key.spec;
    r.linked = linked;
    r.sidechainMissing = scMissing;
    r.hostMode = rl == Role::Host;
    r.hostLoaded = hostedLoaded.load();
    r.sweepKind = sweepKindNow;
    r.sweepStep = sweepStepNow;
    r.sweep = sweepPts;
    r.sweepShownKind = sweepPtsKind;
    r.sweepTotal = (int) sweepPts.size();
    const double smooth = smoothingForIndex (paramIndex (ids::smoothing));

    if (music)
    {
        r.view = View::Music;
        r.frames = xferAn.frames();
        r.levelDb = xferAn.wetLevelDb();
        r.dryLevelDb = xferAn.dryLevelDb();
        r.hasSignal = r.frames > 0;
        xferAn.transfer (tmpH, tmpValid, tmpCoh);
        r.delaySamples = musicDelay + latencyExtra;
        r.latencyReliable = true;
        // o dry já está alinhado por musicDelay; a latência extra entra só na fase
        pca::buildLogCurve (tmpH, tmpValid, &tmpCoh, N, fs, (double) latencyExtra, 1024, 20.0, 20000.0, smooth, r.curve);

        // Espetro antes/depois
        xferAn.spectra (tmpD, tmpW, tmpValid);
        const double sp = std::max (smooth, 1.0 / 12.0);
        pca::buildLogCurve (tmpD, tmpValid, nullptr, N, fs, 0.0, 512, 20.0, 20000.0, sp, r.specDry);
        pca::buildLogCurve (tmpW, tmpValid, nullptr, N, fs, 0.0, 512, 20.0, 20000.0, sp, r.specWet);

        // Onda: últimos ~40 ms do dry (alinhado) e do wet
        const int len = juce::jmin (N, (int) (0.04 * fs));
        r.waveIn.assign (frameDry.end() - len, frameDry.end());
        r.waveOut.assign (frameWet.end() - len, frameWet.end());
        r.waveStartMs = 0.0;
        r.waveSpanMs = 1000.0 * len / fs;
    }
    else if (harmonics)
    {
        r.view = View::Harmonics;
        r.frames = harmAn.frames();
        r.levelDb = harmAn.lastLevelDb();
        r.hasSignal = r.frames > 0 && r.levelDb > -110.0;
        harmAn.amplitudeSpectrum (tmpAmp);
        pca::buildPeakSpectrum (tmpAmp, N, fs, 1024, 20.0, 20000.0, r.curve);
        r.harm = harmAn.analyse();
        harmAn.waveform (1024, 2, r.waveIn, r.waveOut);
        r.waveStartMs = 0.0;
        r.waveSpanMs = r.harm.f0 > 0 ? 2000.0 / r.harm.f0 : 0.0;
    }
    else
    {
        r.view = View::Response;
        r.frames = respAn.frames();
        r.levelDb = respAn.lastLevelDb();
        r.hasSignal = r.frames > 0 && r.levelDb > -110.0;
        respAn.transfer (tmpH, tmpValid);
        if (autoSync && r.frames > 0 && ! frozen)
            heldDelay = respAn.estimateDelay (tmpH);
        r.delaySamples = heldDelay + latencyExtra;
        r.latencyReliable = rl == Role::Host || clockFromTimeline.load();
        pca::buildLogCurve (tmpH, tmpValid, nullptr, N, fs, r.delaySamples, 1024, 20.0, 20000.0, smooth, r.curve);

        // Onda: resposta impulsional de -2 ms a +20 ms em torno do pico
        respAn.impulseResponse (tmpIr);
        const int pre = (int) (0.002 * fs), post = (int) (0.020 * fs);
        const int start = (int) std::lround (r.delaySamples) - pre;
        const int total = pre + post;
        const int stride = juce::jmax (1, total / 2048);
        r.waveOut.clear();
        for (int i = 0; i < total; i += stride)
            r.waveOut.push_back (tmpIr[(size_t) (((start + i) % N + N) % N)]);
        r.waveStartMs = -2.0;
        r.waveSpanMs = 22.0;
    }

    if (rl == Role::Host)
        r.hostName = hostedNameStr;

    {
        const juce::ScopedLock sl (resultsLock);
        r.serial = results.serial + 1;
        results = std::move (r);
    }
}

// =============================================================================
//  Modo Host
// =============================================================================
std::vector<CurveAnalyzerProcessor::InstalledPlugin> CurveAnalyzerProcessor::listInstalledPlugins()
{
    std::vector<InstalledPlugin> out;
    for (auto* fmt : formatManager.getFormats())
    {
        const auto fname = fmt->getName();
        if (fname != "VST3" && fname != "AudioUnit") continue;
        const auto ids = fmt->searchPathsForPlugins (fmt->getDefaultLocationsToSearch(), true, true);
        for (auto& id : ids)
        {
            auto name = fmt->getNameOfPluginFromIdentifier (id);
            if (name.isEmpty()) name = juce::File (id).getFileNameWithoutExtension();
            if (name.containsIgnoreCase ("ANALYSER by Piradex")) continue;   // não nos carregamos a nós próprios
            out.push_back ({ name, fname == "AudioUnit" ? juce::String ("AU") : fname, id });
        }
    }
    std::sort (out.begin(), out.end(), [] (const InstalledPlugin& a, const InstalledPlugin& b)
               { return a.name.compareNatural (b.name) < 0; });
    return out;
}

namespace
{
struct HostWindow : public juce::DocumentWindow
{
    HostWindow (const juce::String& title, juce::AudioProcessorEditor* ed, std::function<void()> onCloseFn)
        : DocumentWindow (title, juce::Colours::black, DocumentWindow::closeButton), onClose (std::move (onCloseFn))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
        setAlwaysOnTop (true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { if (onClose) onClose(); }
    std::function<void()> onClose;
};
}

void CurveAnalyzerProcessor::loadHostedPlugin (const juce::String& formatName, const juce::String& fileOrId,
                                               std::function<void (const juce::String&)> done)
{
    juce::AudioPluginFormat* fmt = nullptr;
    for (auto* f : formatManager.getFormats())
        if (f->getName() == formatName || (formatName == "AU" && f->getName() == "AudioUnit"))
            fmt = f;
    if (fmt == nullptr) { if (done) done (U8 ("Formato não suportado: ") + formatName); return; }

    juce::OwnedArray<juce::PluginDescription> types;
    fmt->findAllTypesForFile (types, fileOrId);
    if (types.isEmpty()) { if (done) done (U8 ("Não foi possível ler este plugin.")); return; }

    const auto desc = *types[0];
    const double sr = sampleRate.load() > 0.0 ? sampleRate.load() : 48000.0;
    const int bs = maxBlock.load();
    std::weak_ptr<int> alive = aliveToken;

    formatManager.createPluginInstanceAsync (desc, sr, bs,
        [this, alive, desc, formatName, fileOrId, sr, bs, done] (std::unique_ptr<juce::AudioPluginInstance> inst, const juce::String& err)
    {
        if (alive.expired()) return;
        if (inst == nullptr) { if (done) done (err.isNotEmpty() ? err : U8 ("O plugin não abriu.")); return; }

        // Estéreo dentro/fora sempre que o plugin aceitar
        auto layout = inst->getBusesLayout();
        if (layout.inputBuses.size() > 0)  layout.inputBuses.getReference (0)  = juce::AudioChannelSet::stereo();
        if (layout.outputBuses.size() > 0) layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        for (int i = 1; i < layout.inputBuses.size(); ++i) layout.inputBuses.getReference (i) = juce::AudioChannelSet::disabled();
        inst->setBusesLayout (layout);
        inst->setRateAndBufferSizeDetails (sr, bs);
        inst->prepareToPlay (sr, bs);
        inst->enableAllBuses();

        hostWindow.reset();
        std::unique_ptr<juce::AudioPluginInstance> old;
        {
            const juce::ScopedLock sl (hostLock);
            old = std::move (hosted);
            hosted = std::move (inst);
        }
        old.reset();
        hostedNameStr = desc.name;
        hostedFormat = formatName;
        hostedFileOrId = fileOrId;
        hostedLoaded.store (true);
        prepareHostBuffers();
        applyPendingHostState();
        requestReset();
        if (done) done ({});
    });
}

void CurveAnalyzerProcessor::applyPendingHostState()
{
    if (pendingHostState == nullptr || hosted == nullptr) return;
    const juce::ScopedLock sl (hostLock);
    hosted->setStateInformation (pendingHostState->getData(), (int) pendingHostState->getSize());
    pendingHostState.reset();
}

void CurveAnalyzerProcessor::unloadHostedPlugin()
{
    hostWindow.reset();
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (hostLock);
        old = std::move (hosted);
    }
    old.reset();
    hostedLoaded.store (false);
    hostedNameStr.clear(); hostedFormat.clear(); hostedFileOrId.clear();
    prepareHostBuffers();
    requestReset();
}

void CurveAnalyzerProcessor::openHostedEditor()
{
    if (hosted == nullptr) return;
    if (hostWindow != nullptr) { hostWindow->toFront (true); return; }
    juce::AudioProcessorEditor* ed = hosted->createEditorIfNeeded();
    if (ed == nullptr) ed = new juce::GenericAudioProcessorEditor (*hosted);
    std::weak_ptr<int> alive = aliveToken;
    hostWindow = std::make_unique<HostWindow> (hostedNameStr + U8 (" — dentro do ANALYSER"), ed, [this, alive]
    {
        juce::MessageManager::callAsync ([this, alive] { if (! alive.expired()) hostWindow.reset(); });
    });
}

// =============================================================================
void CurveAnalyzerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return;
    if (hostedLoaded.load() && hosted != nullptr)
    {
        juce::MemoryBlock st;
        {
            const juce::ScopedLock sl (hostLock);
            hosted->getStateInformation (st);
        }
        auto* h = xml->createNewChildElement ("HOSTED");
        h->setAttribute ("format", hostedFormat);
        h->setAttribute ("id", hostedFileOrId);
        h->setAttribute ("name", hostedNameStr);
        h->addTextElement (st.toBase64Encoding());
    }
    copyXmlToBinary (*xml, destData);
}

void CurveAnalyzerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    juce::String fmt, id;
    if (auto* h = xml->getChildByName ("HOSTED"))
    {
        fmt = h->getStringAttribute ("format");
        id  = h->getStringAttribute ("id");
        auto mb = std::make_unique<juce::MemoryBlock>();
        if (mb->fromBase64Encoding (h->getAllSubText().trim()))
            pendingHostState = std::move (mb);
        xml->removeChildElement (h, true);
    }
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (fmt.isNotEmpty() && id.isNotEmpty())
    {
        std::weak_ptr<int> alive = aliveToken;
        juce::MessageManager::callAsync ([this, alive, fmt, id]
        {
            if (! alive.expired()) loadHostedPlugin (fmt, id);
        });
    }
}

juce::AudioProcessorEditor* CurveAnalyzerProcessor::createEditor()
{
    return new CurveAnalyzerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CurveAnalyzerProcessor();
}
