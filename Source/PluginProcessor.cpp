#include "PluginProcessor.h"
#include "PluginEditor.h"

using View = AnalysisResults::View;

// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CurveAnalyzerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const String& id, const String& name, StringArray items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto toggle = [&] (const String& id, const String& name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    choice (ids::role,      "Papel",       { "Analisador", "Gerador" }, 0);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ids::group, 1 }, "Grupo", 1, pca::GroupRegistry::kNumGroups, 1));
    choice (ids::signal,    "Sinal",       { "Impulso", "Sweep log", juce::String::fromUTF8 ("Ruído rosa"), juce::String::fromUTF8 ("Seno (harmónicos)") }, 1);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::level, 1 }, juce::String::fromUTF8 ("Nível"),
                                                  NormalisableRange<float> (-60.0f, 0.0f, 0.5f), -18.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dBFS")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::sineFreq, 1 }, "Freq. seno",
                                                  NormalisableRange<float> (20.0f, 10000.0f, 1.0f, 0.3f), 1000.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")));
    choice (ids::fftSize,   "FFT",         { "4096", "8192", "16384", "32768", "65536" }, 1);
    choice (ids::source,    "Fonte",       { "Gerador", juce::String::fromUTF8 ("Sidechain (música)") }, 0);
    choice (ids::channel,   "Canal",       { "Esquerdo", "Direito", "Mid", "Side" }, 0);
    choice (ids::averages,  juce::String::fromUTF8 ("Média"), { "1", "2", "4", "8", "16", "32", juce::String::fromUTF8 ("∞") }, 3);
    choice (ids::smoothing, juce::String::fromUTF8 ("Suavização"), { "Nenhuma", "1/24 oit.", "1/12 oit.", "1/6 oit.", "1/3 oit." }, 0);
    choice (ids::range,     "Escala",      { juce::String::fromUTF8 ("±3 dB"), juce::String::fromUTF8 ("±6 dB"), juce::String::fromUTF8 ("±12 dB"),
                                             juce::String::fromUTF8 ("±24 dB"), juce::String::fromUTF8 ("±48 dB") }, 2);
    toggle (ids::showPhase, "Fase",        true);
    toggle (ids::autoSync,  "Auto Sync",   true);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ids::latency, 1 }, juce::String::fromUTF8 ("Latência extra"), -4096, 4096, 0));
    toggle (ids::freeze,    "Congelar",    false);
    toggle (ids::muteOut,   juce::String::fromUTF8 ("Silenciar saída"), false);
    return l;
}

// =============================================================================
CurveAnalyzerProcessor::CurveAnalyzerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Entrada",   juce::AudioChannelSet::stereo(), true)
                        .withOutput (juce::String::fromUTF8 ("Saída"), juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "AnalyserByPiradex", createLayout())
{
    lic::License::get();   // carrega o estado da licença na thread de mensagens
    ringWet.assign ((size_t) kRingSize, 0.0f);
    ringDry.assign ((size_t) kRingSize, 0.0f);
    worker.startThread (juce::Thread::Priority::low);
    startTimerHz (20);
}

CurveAnalyzerProcessor::~CurveAnalyzerProcessor()
{
    stopTimer();
    worker.stopThread (3000);
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

void CurveAnalyzerProcessor::prepareToPlay (double sr, int)
{
    sampleRate.store (sr);
    timerCallback();   // gera já o sinal de teste com a nova taxa de amostragem
}

// Reconstrói o período do sinal de teste quando as definições mudam (fora da thread de áudio)
void CurveAnalyzerProcessor::timerCallback()
{
    lic::License::get().checkExpiry();
    if (paramIndex (ids::role) != 1 || sampleRate.load() <= 0.0) return;
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
void CurveAnalyzerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const bool isGenerator = paramIndex (ids::role) == 1;

    // Sem sessão iniciada: o plugin é transparente (não estraga a sessão do DAW)
    if (! lic::License::get().isUnlocked())
    {
        for (int ch = mainIn.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
        return;
    }

    if (isGenerator)
    {
        // Publica as definições no grupo para o Analisador as adotar
        auto& slot = pca::GroupRegistry::get (paramIndex (ids::group));
        slot.signal.store (paramIndex (ids::signal));
        slot.fftIndex.store (paramIndex (ids::fftSize));
        slot.levelDb.store (paramValue (ids::level));
        slot.sineHz.store (paramValue (ids::sineFreq));
        slot.sampleRate.store (sampleRate.load());
        slot.lastSeenMs.store (juce::Time::getMillisecondCounter());

        const juce::ScopedTryLock lock (genLock);
        if (lock.isLocked() && genValid && ! genSignal.empty())
        {
            const size_t len = genSignal.size();
            size_t pos = genPos % len;
            for (int n = 0; n < numSamples; ++n)
            {
                const float v = genSignal[pos];
                if (++pos >= len) pos = 0;
                for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
                    mainOut.setSample (ch, n, v);
            }
            genPos = pos;
        }
        else
        {
            mainOut.clear();
        }
        for (int ch = mainOut.getNumChannels(); ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
        return;
    }

    // ---------------- Analisador: copia o sinal para o buffer circular -------------
    // Ponteiros diretos (sem cópias nem alocações na thread de áudio)
    const int chanMode = paramIndex (ids::channel);
    auto pick = [chanMode] (const float* l, const float* r, int n) -> float
    {
        if (l == nullptr) return 0.0f;
        const float a = l[n], b = r != nullptr ? r[n] : a;
        switch (chanMode)
        {
            case 1:  return b;
            case 2:  return 0.5f * (a + b);
            case 3:  return 0.5f * (a - b);
            default: return a;
        }
    };

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
    for (int n = 0; n < numSamples; ++n)
    {
        const size_t idx = (size_t) ((base + n) & kRingMask);
        ringWet[idx] = pick (inL, inR, n);
        ringDry[idx] = pick (scL, scR, n);
    }
    writeCount.store (base + numSamples, std::memory_order_release);

    // Passagem direta (in == out); opcionalmente silencia para não ouvir o sinal de teste
    if (paramIndex (ids::muteOut) == 1)
        mainOut.clear();
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

    AnalysisKey key;
    key.generator = paramIndex (ids::role) == 1;
    key.source    = paramIndex (ids::source);
    key.channel   = paramIndex (ids::channel);
    key.spec      = ownSpec();

    bool linked = false;
    if (! key.generator && key.source == 0)
    {
        auto& slot = pca::GroupRegistry::get (paramIndex (ids::group));
        const uint32_t now = juce::Time::getMillisecondCounter();
        const uint32_t seen = slot.lastSeenMs.load();
        if (seen != 0 && now - seen < 1500 && std::abs (slot.sampleRate.load() - fs) < 1.0)
        {
            linked = true;
            key.spec.signal  = (pca::Signal) juce::jlimit (0, 3, slot.signal.load());
            key.spec.N       = fftSizeForIndex (slot.fftIndex.load());
            key.spec.levelDb = slot.levelDb.load();
            key.spec.sineHz  = slot.sineHz.load();
        }
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
    const int hop = music ? N / 2 : N;

    // ---- (Re)configuração --------------------------------------------------
    if (! haveKey || key != curKey)
    {
        curKey = key;
        haveKey = true;
        frameWet.assign ((size_t) N, 0.0f);
        frameDry.assign ((size_t) N, 0.0f);
        if (! key.generator)
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
        musicDelay = 0;
        heldDelay = 0.0;
        nextFrame = writeCount.load (std::memory_order_acquire) / hop + 1;
    }

    if (key.generator)
    {
        const juce::ScopedLock sl (resultsLock);
        if (results.view != View::Generator)
        {
            results = AnalysisResults();
            results.view = View::Generator;
            results.spec = key.spec;
            ++results.serial;
        }
        return;
    }

    if (resetRequested.exchange (false))
    {
        respAn.reset(); harmAn.reset(); xferAn.reset();
        musicDelay = 0;
    }

    const int avg = averagesForIndex (paramIndex (ids::averages));
    respAn.setAverages (avg);
    harmAn.setAverages (avg);
    xferAn.setAverages (avg == 0 ? 0 : std::max (avg, 4));   // Welch precisa de algumas médias

    const bool frozen  = paramIndex (ids::freeze) == 1;
    const bool autoSync = paramIndex (ids::autoSync) == 1;
    const int  latencyExtra = paramIndex (ids::latency);
    const bool scMissing = music && ! sidechainActive.load();

    // ---- Consumir blocos disponíveis --------------------------------------------
    const int64_t S = writeCount.load (std::memory_order_acquire);
    const int64_t safety = 32768;
    int processed = 0;
    while (! worker.threadShouldExit())
    {
        const int64_t e = nextFrame * hop;
        if (e > S) break;
        const int64_t oldest = e - N - (music ? musicDelay : 0);
        if (oldest < 0) { ++nextFrame; continue; }
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
        else                respAn.addFrame (frameWet.data());

        ++nextFrame;
        if (++processed > 64) break;
    }

    // ---- Publicar resultados (~30 Hz) -------------------------------------------------
    const uint32_t now = juce::Time::getMillisecondCounter();
    if (now - lastPublishMs < 33) return;
    lastPublishMs = now;

    AnalysisResults r;
    r.spec   = key.spec;
    r.linked = linked;
    r.sidechainMissing = scMissing;
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
        // o dry já está alinhado por musicDelay; a latência extra entra só na fase
        pca::buildLogCurve (tmpH, tmpValid, &tmpCoh, N, fs, (double) latencyExtra, 1024, 20.0, 20000.0, smooth, r.curve);
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
        pca::buildLogCurve (tmpH, tmpValid, nullptr, N, fs, r.delaySamples, 1024, 20.0, 20000.0, smooth, r.curve);
    }

    {
        const juce::ScopedLock sl (resultsLock);
        r.serial = results.serial + 1;
        results = std::move (r);
    }
}

// =============================================================================
void CurveAnalyzerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CurveAnalyzerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CurveAnalyzerProcessor::createEditor()
{
    return new CurveAnalyzerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CurveAnalyzerProcessor();
}
