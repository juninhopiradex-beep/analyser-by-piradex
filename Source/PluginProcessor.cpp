#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace eqm;

static constexpr int kDisplayPoints = 480;
static constexpr double kDisplayLo = 10.0, kDisplayHi = 30000.0;

// =============================================================================
//  Thread de análise
// =============================================================================
class EQMotionProcessor::AnalysisThread : public juce::Thread
{
public:
    explicit AnalysisThread (EQMotionProcessor& p) : juce::Thread ("EQMotion analysis"), owner (p)
    {
        chunk.setSize (6, 2048);
    }
    ~AnalysisThread() override { stopThread (2000); }

    void configure (double sampleRate)
    {
        const juce::ScopedLock sl (cfgLock);
        fs = sampleRate;
        needsConfig = true;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (12);
            if (needsConfig.exchange (false) || cfgFft != owner.analyzer.fftSize.load())
                reconfigure();

            const bool active = InstanceRegistry::get().anyEditorOpen() || owner.learning.load();
            bool updated = false;
            for (;;)
            {
                const int avail = owner.fifo.getNumReady();
                if (avail <= 0) break;
                const int n = juce::jmin (avail, chunk.getNumSamples());
                int s1, n1, s2, n2;
                owner.fifo.prepareToRead (n, s1, n1, s2, n2);
                for (int c = 0; c < 6; ++c)
                {
                    if (n1 > 0) chunk.copyFrom (c, 0, owner.fifoBuf, c, s1, n1);
                    if (n2 > 0) chunk.copyFrom (c, n1, owner.fifoBuf, c, s2, n2);
                }
                owner.fifo.finishedRead (n1 + n2);
                if (active) updated |= analyse (n1 + n2);
            }
            if (updated) publish();
        }
    }

private:
    void reconfigure()
    {
        double rate;
        { const juce::ScopedLock sl (cfgLock); rate = fs; }
        cfgFft = juce::jlimit (1024, 16384, owner.analyzer.fftSize.load());
        const double hi = juce::jmin (kDisplayHi, rate * 0.5);
        for (auto* a : { &pre, &post, &postA, &postB, &sc })
            a->configure (rate, cfgFft, kDisplayPoints, kDisplayLo, hi);
        mono.resize (2048); monoB.resize (2048); scMono.resize (2048);
        freqs.resize (kDisplayPoints);
        for (int i = 0; i < kDisplayPoints; ++i) freqs[(size_t) i] = pre.freqAt (i);
        sampleRate = rate;
    }

    bool analyse (int n)
    {
        const auto speed = (SpectrumAnalyzer::Speed) juce::jlimit (0, 2, owner.analyzer.speed.load());
        const float slope = owner.analyzer.slope.load();
        for (auto* a : { &pre, &post, &postA, &postB, &sc }) { a->setSpeed (speed); a->setSlope (slope); }

        auto* pl = chunk.getReadPointer (0); auto* pr = chunk.getReadPointer (1);
        auto* ol = chunk.getReadPointer (2); auto* orr = chunk.getReadPointer (3);
        auto* sl = chunk.getReadPointer (4); auto* sr = chunk.getReadPointer (5);
        const int split = owner.procModeForAnalysis.load();
        currentSplit = split;

        bool scSignal = false;
        for (int i = 0; i < n; ++i)
        {
            mono[(size_t) i] = 0.5f * (pl[i] + pr[i]);
            scMono[(size_t) i] = 0.5f * (sl[i] + sr[i]);
            scSignal |= std::abs (scMono[(size_t) i]) > 1.0e-5f;
        }
        if (scSignal) scSeenMs = juce::Time::getMillisecondCounter();

        // aprendizagem das Smart Ops: entrada (antes do EQ) e sidechain
        if (owner.learning.load())
        {
            const juce::ScopedLock lk (owner.learnerLock);
            owner.learner.feed (mono.data(), scMono.data(), n, owner.scAvailableForLearning.load());
        }

        if (owner.analyzer.freeze.load()) return false;
        bool upd = pre.push (mono.data(), n);
        sc.push (scMono.data(), n);
        for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (ol[i] + orr[i]);
        upd |= post.push (mono.data(), n);
        if (split == 1) { postA.push (ol, n); postB.push (orr, n); }
        else if (split == 2)
        {
            for (int i = 0; i < n; ++i) { mono[(size_t) i] = 0.5f * (ol[i] + orr[i]); monoB[(size_t) i] = 0.5f * (ol[i] - orr[i]); }
            postA.push (mono.data(), n); postB.push (monoB.data(), n);
        }
        return upd;
    }

    void publish()
    {
        {
            const juce::SpinLock::ScopedLockType lk (owner.spectrumLock);
            auto& f = owner.spectrum;
            f.pre = pre.displayDb(); f.post = post.displayDb(); f.peak = post.peakDb(); f.sc = sc.displayDb();
            f.split = currentSplit;
            if (currentSplit != 0) { f.postA = postA.displayDb(); f.postB = postB.displayDb(); }
            else { f.postA.clear(); f.postB.clear(); }
            f.freqs = freqs;
            f.hasSc = juce::Time::getMillisecondCounter() - scSeenMs < 1500;
            f.stamp = ++owner.spectrumStamp;
        }
        // publica para as outras instâncias (espetro + média longa na LogGrid)
        if (auto s = owner.shared)
        {
            const auto& avg = post.averagedPower();
            binsToGrid (avg.data(), (int) avg.size(), sampleRate, post.fftSize(), gridTmp);
            std::vector<float> disp (kDisplayPoints);
            // reamostra para a grelha comum 10 Hz – 30 kHz (a do ecrã pode acabar em fs/2)
            const auto& d = post.displayDb();
            for (int i = 0; i < kDisplayPoints; ++i)
            {
                const double f = kDisplayLo * std::pow (kDisplayHi / kDisplayLo, (double) i / (kDisplayPoints - 1));
                const double pos = std::log (f / kDisplayLo) / std::log (freqs.back() / kDisplayLo) * (kDisplayPoints - 1);
                const int a = juce::jlimit (0, kDisplayPoints - 1, (int) pos);
                disp[(size_t) i] = f > freqs.back() ? -160.0f : d[(size_t) a];
            }
            const juce::SpinLock::ScopedLockType lk (s->lock);
            s->spectrumDb = std::move (disp);
            s->avgGridDb = gridTmp;
            s->lastUpdateMs = juce::Time::getMillisecondCounter();
        }
    }

    EQMotionProcessor& owner;
    juce::CriticalSection cfgLock;
    double fs = 48000, sampleRate = 48000;
    std::atomic<bool> needsConfig { true };
    int cfgFft = 0, currentSplit = 0;
    SpectrumAnalyzer pre, post, postA, postB, sc;
    juce::AudioBuffer<float> chunk;
    std::vector<float> mono, monoB, scMono;
    std::vector<double> freqs, gridTmp;
    juce::uint32 scSeenMs = 0;
};

// =============================================================================
EQMotionProcessor::EQMotionProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "EQMOTION", params::createLayout())
{
    for (int i = 0; i < kMaxBands; ++i)
        for (int b = 0; b < (int) ids::B::Count; ++b)
            bandPtrs[(size_t) i].p[(size_t) b] = apvts.getRawParameterValue (ids::band (i, (ids::B) b));
    pBypass = apvts.getRawParameterValue (ids::bypass);   pOutput = apvts.getRawParameterValue (ids::output);
    pAutoGain = apvts.getRawParameterValue (ids::autoGain); pProcMode = apvts.getRawParameterValue (ids::procMode);
    pPhase = apvts.getRawParameterValue (ids::phase);     pLpQ = apvts.getRawParameterValue (ids::lpQuality);
    pLook = apvts.getRawParameterValue (ids::lookahead);  pChar = apvts.getRawParameterValue (ids::character);
    pDeh = apvts.getRawParameterValue (ids::deharsh);     pDehAmt = apvts.getRawParameterValue (ids::deharshAmt);
    pDehAtk = apvts.getRawParameterValue (ids::deharshAtk); pDehRel = apvts.getRawParameterValue (ids::deharshRel);
    pDelta = apvts.getRawParameterValue (ids::delta);     pGainScale = apvts.getRawParameterValue (ids::gainScale);

    for (auto& d : dynDelta) d.store (0.0);
    for (auto& d : dynLevel) d.store (-150.0);

    lic::License::get();   // carrega a licença na thread de mensagens (nunca na de áudio)
    shared = InstanceRegistry::get().add();
    analysis = std::make_unique<AnalysisThread> (*this);
    analysis->startThread (juce::Thread::Priority::low);
    startTimerHz (25);
}

EQMotionProcessor::~EQMotionProcessor()
{
    stopTimer();
    analysis.reset();
    InstanceRegistry::get().remove (shared);
}

bool EQMotionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
void EQMotionProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRateHz = sr;
    maxChunk = juce::jlimit (64, 4096, juce::jmax (samplesPerBlock, 256));
    osFactor = curve::naturalOversampling (sr);
    const int stages = osFactor == 4 ? 2 : osFactor == 2 ? 1 : 0;

    iirZL.prepare (sr, 16, 0);
    iirNP.prepare (sr * osFactor, juce::jmin (64, 16 * osFactor), (int) std::ceil (0.021 * sr * osFactor) + 8);
    for (int q = 0; q < 3; ++q) lp[(size_t) q].prepare (sr, (LinearQuality) q);

    using OS = juce::dsp::Oversampling<double>;
    osMain = std::make_unique<OS> (2, stages, OS::filterHalfBandPolyphaseIIR, true, false);
    osDry  = std::make_unique<OS> (2, stages, OS::filterHalfBandPolyphaseIIR, true, false);
    osSc   = std::make_unique<OS> (2, stages, OS::filterHalfBandPolyphaseIIR, true, false);
    for (auto* o : { osMain.get(), osDry.get(), osSc.get() }) { o->initProcessing ((size_t) maxChunk); o->reset(); }

    const int maxDelay = lp[2].latency() + (int) std::ceil (0.021 * sr) + 64;
    dryL.prepare (maxDelay); dryR.prepare (maxDelay);

    character.prepare (sr);
    loudness.prepare (sr);
    for (auto* s : { &bypassMix, &deltaMix, &switchGain, &listenMix }) s->setTime (0.02, sr);
    outGain.setTime (0.03, sr);
    bypassMix.reset (pBypass->load() > 0.5f || ! lic::License::get().isUnlocked() ? 1.0 : 0.0);
    deltaMix.reset (pDelta->load() > 0.5f ? 1.0 : 0.0);
    outGain.reset (dbToGain (pOutput->load()));
    switchGain.reset (1.0);
    listenMix.reset (0.0);
    listenActive = -1;

    work.setSize (2, maxChunk); dry.setSize (2, maxChunk); scBuf.setSize (2, maxChunk); inCopy.setSize (2, maxChunk);

    // configuração inicial: reportada já, sem desvanecimento
    const int mode = juce::jlimit (0, 2, (int) pPhase->load()), q = juce::jlimit (0, 2, (int) pLpQ->load());
    const int la = mode == 1 ? juce::roundToInt (pLook->load() * 0.001 * sr) : 0;
    const int cfg = encodeConfig (mode, q, la);
    requestedConfig.store (cfg);
    activeConfig = -1;
    fadeState = 0;
    const int lat = mode == 0 ? 0 : mode == 1 ? juce::roundToInt (osMain->getLatencyInSamples()) + la : lp[(size_t) q].latency();
    setLatencySamples (lat);

    fifo.reset();
    analysis->configure (sr);
}

void EQMotionProcessor::releaseResources() {}

eqm::BandArray EQMotionProcessor::currentBands() const
{
    BandArray b {};
    for (int i = 0; i < kMaxBands; ++i) b[(size_t) i] = params::readBand (bandPtrs[(size_t) i]);
    return b;
}

void EQMotionProcessor::readBandParams()
{
    for (int i = 0; i < kMaxBands; ++i) bands[(size_t) i] = params::readBand (bandPtrs[(size_t) i]);
    dynSettings.gainScale = pGainScale->load() * 0.01;
    dynSettings.deharshOn = pDeh->load() > 0.5f;
    dynSettings.deharshAmount = pDehAmt->load() * 0.01;
    dynSettings.deharshAttackMs = pDehAtk->load();
    dynSettings.deharshReleaseMs = pDehRel->load();
}

// -----------------------------------------------------------------------------
void EQMotionProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)          { processImpl (b, false); }
void EQMotionProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&)         { processImpl (b, false); }
void EQMotionProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { processImpl (b, true); }
void EQMotionProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processImpl (b, true); }

template <typename T>
void EQMotionProcessor::processImpl (juce::AudioBuffer<T>& buffer, bool hostBypassed)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int chans = juce::jmin (2, mainIn.getNumChannels());
    const int total = buffer.getNumSamples();
    if (chans == 0 || total == 0) return;

    juce::AudioBuffer<T> scIn;
    bool scOk = false;
    if (getBusCount (true) > 1)
    {
        auto* bus = getBus (true, 1);
        if (bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            scIn = getBusBuffer (buffer, true, 1);
            scOk = scIn.getNumChannels() > 0;
        }
    }
    scAvailableThisBlock = scOk;
    scAvailableForLearning.store (scOk);

    const bool lockedOrBypassed = hostBypassed || pBypass->load() > 0.5f || ! lic::License::get().isUnlocked();

    for (int start = 0; start < total; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, total - start);
        for (int c = 0; c < 2; ++c)
        {
            const T* src = mainIn.getReadPointer (juce::jmin (c, chans - 1), start);
            double* w = work.getWritePointer (c);
            double* d = dry.getWritePointer (c);
            double* ic = inCopy.getWritePointer (c);
            for (int i = 0; i < n; ++i) { w[i] = (double) src[i]; d[i] = w[i]; ic[i] = w[i]; }
            double* s = scBuf.getWritePointer (c);
            if (scOk)
            {
                const T* ssrc = scIn.getReadPointer (juce::jmin (c, scIn.getNumChannels() - 1), start);
                for (int i = 0; i < n; ++i) s[i] = (double) ssrc[i];
            }
            else juce::FloatVectorOperations::clear (s, n);
        }
        bypassTarget = lockedOrBypassed;
        processChunk (n);
        for (int c = 0; c < mainOut.getNumChannels(); ++c)
        {
            T* dst = mainOut.getWritePointer (c, start);
            const double* w = work.getReadPointer (juce::jmin (c, 1));
            for (int i = 0; i < n; ++i) dst[i] = (T) w[i];
        }
    }
    for (int c = mainOut.getNumChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, total);
}

template void EQMotionProcessor::processImpl<float> (juce::AudioBuffer<float>&, bool);
template void EQMotionProcessor::processImpl<double> (juce::AudioBuffer<double>&, bool);

void EQMotionProcessor::processChunk (int n)
{
    readBandParams();
    double* L = work.getWritePointer (0);
    double* R = work.getWritePointer (1);
    double* dL = dry.getWritePointer (0);
    double* dR = dry.getWritePointer (1);
    const double* sL = scBuf.getReadPointer (0);
    const double* sR = scBuf.getReadPointer (1);
    const bool sc = scAvailableThisBlock;

    // medidor de entrada
    {
        float pl = 0, pr = 0; double el = 0, er = 0;
        for (int i = 0; i < n; ++i) { pl = std::max (pl, (float) std::abs (L[i])); pr = std::max (pr, (float) std::abs (R[i])); el += L[i] * L[i]; er += R[i] * R[i]; }
        inMeter.peakL.store (std::max (inMeter.peakL.load(), pl)); inMeter.peakR.store (std::max (inMeter.peakR.load(), pr));
        inMeter.rmsL.store ((float) std::sqrt (el / n)); inMeter.rmsR.store ((float) std::sqrt (er / n));
        if (pl > 1.0f || pr > 1.0f) inMeter.clip.store (true);
    }

    // ---- mudança de motor / latência (com desvanecimento)
    const int req = requestedConfig.load();
    bool switchNow = false;
    if (req != activeConfig)
    {
        if (activeConfig < 0) switchNow = true;
        else if (fadeState == 0) { fadeState = 1; pendingConfig = req; }
    }
    if (switchNow || (fadeState == 2))
    {
        activeConfig = switchNow ? req : pendingConfig;
        const int mode = activeConfig & 3, q = (activeConfig >> 2) & 3, la = activeConfig >> 4;
        activeMode.store (mode);
        iirZL.reset(); iirNP.reset(); for (auto& e : lp) e.reset();
        osMain->reset(); osDry->reset(); osSc->reset();
        dryL.reset(); dryR.reset();
        iirNP.setLookahead (la * osFactor);
        const int dryDelay = mode == 2 ? lp[(size_t) q].latency() : mode == 1 ? la : 0;
        dryL.setDelay (dryDelay); dryR.setDelay (dryDelay);
        character.reset(); loudness.reset();
        if (! switchNow) switchGain.reset (0.0);
        switchGain.target = 1.0;
        fadeState = 0;
    }
    const int mode = activeConfig & 3, q = (activeConfig >> 2) & 3;

    // ---- motor
    if (mode == 0)
        iirZL.process (L, R, sc ? sL : nullptr, sc ? sR : nullptr, n, bands, dynSettings);
    else if (mode == 1)
    {
        juce::dsp::AudioBlock<double> blk (work.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = osMain->processSamplesUp (blk);
        const int un = (int) up.getNumSamples();
        const double* uScL = nullptr; const double* uScR = nullptr;
        if (sc)
        {
            juce::dsp::AudioBlock<double> sblk (scBuf.getArrayOfWritePointers(), 2, (size_t) n);
            auto su = osSc->processSamplesUp (sblk);
            uScL = su.getChannelPointer (0); uScR = su.getChannelPointer (1);
        }
        iirNP.process (up.getChannelPointer (0), up.getChannelPointer (1), uScL, uScR, un, bands, dynSettings);
        osMain->processSamplesDown (blk);
        juce::dsp::AudioBlock<double> dblk (dry.getArrayOfWritePointers(), 2, (size_t) n);
        osDry->processSamplesUp (dblk);
        osDry->processSamplesDown (dblk);
    }
    else
        lp[(size_t) q].process (L, R, sc ? sL : nullptr, sc ? sR : nullptr, n, bands, dynSettings);

    for (int i = 0; i < n; ++i) { dL[i] = dryL.process (dL[i]); dR[i] = dryR.process (dR[i]); }

    // proteção: NaN/Inf nunca chegam à saída
    bool finite = true;
    for (int i = 0; i < n && finite; ++i) finite = std::isfinite (L[i]) && std::isfinite (R[i]);
    if (! finite)
    {
        iirZL.reset(); iirNP.reset(); for (auto& e : lp) e.reset(); character.reset();
        for (int i = 0; i < n; ++i) { L[i] = dL[i]; R[i] = dR[i]; }
    }

    // valores para a UI
    for (int b = 0; b < kMaxBands; ++b)
    {
        const double d = mode == 0 ? iirZL.bandDeltaDb (b) : mode == 1 ? iirNP.bandDeltaDb (b) : lp[(size_t) q].bandDeltaDb (b);
        const double lv = mode == 0 ? iirZL.bandLevelDb (b) : mode == 1 ? iirNP.bandLevelDb (b) : lp[(size_t) q].bandLevelDb (b);
        dynDelta[(size_t) b].store (d, std::memory_order_relaxed);
        dynLevel[(size_t) b].store (lv, std::memory_order_relaxed);
    }
    deharshDelta.store (mode == 0 ? iirZL.deharshDeltaDb() : mode == 1 ? iirNP.deharshDeltaDb() : lp[(size_t) q].deharshDeltaDb());

    // ---- character
    character.process (L, R, n, (Character) juce::jlimit (0, 2, (int) pChar->load()));

    // ---- delta (o que o EQ muda) e auto gain
    deltaMix.target = pDelta->load() > 0.5f ? 1.0 : 0.0;
    if (deltaMix.value > 1e-6 || deltaMix.target > 0)
        for (int i = 0; i < n; ++i) { const double k = deltaMix.next(); L[i] -= k * dL[i]; R[i] -= k * dR[i]; }
    loudness.process (dL, dR, L, R, n, pAutoGain->load() > 0.5f && deltaMix.target < 0.5);
    autoGainNow.store (loudness.currentGainDb());

    // ---- saída
    outGain.target = dbToGain (pOutput->load());
    if (outGain.value != outGain.target)
    {
        for (int i = 0; i < n; ++i) { const double g = outGain.next(); L[i] *= g; R[i] *= g; }
        if (std::abs (outGain.value - outGain.target) < 1e-6) outGain.value = outGain.target;
    }
    else if (outGain.value != 1.0)
        for (int i = 0; i < n; ++i) { L[i] *= outGain.value; R[i] *= outGain.value; }

    // ---- listen de uma banda (zona da banda, a partir do sinal original alinhado)
    const int lb = listenBand.load();
    if (lb >= 0 && lb < kMaxBands && bands[(size_t) lb].active)
    {
        const auto& bp = bands[(size_t) lb];
        if (lb != listenActive || bp.type != listenParams.type || bp.freq != listenParams.freq || bp.q != listenParams.q)
        {
            listenDesign = listenFilter (bp, sampleRateHz);
            for (int k = 0; k < listenDesign.count; ++k) { listenL[k].c = listenDesign.c[(size_t) k]; listenR[k].c = listenDesign.c[(size_t) k]; }
            if (lb != listenActive) for (int k = 0; k < kMaxSections; ++k) { listenL[k].reset(); listenR[k].reset(); }
            listenActive = lb; listenParams = bp;
        }
        listenMix.target = 1.0;
    }
    else listenMix.target = 0.0;
    if (listenMix.value > 1e-6 || listenMix.target > 0)
        for (int i = 0; i < n; ++i)
        {
            double a = dL[i], b = dR[i];
            for (int k = 0; k < listenDesign.count; ++k) { a = listenL[k].process (a); b = listenR[k].process (b); }
            const double m = listenMix.next();
            L[i] += m * (a - L[i]); R[i] += m * (b - R[i]);
        }
    else listenActive = -1;

    // ---- bypass (ou sem sessão iniciada): original alinhado, mesma latência
    bypassMix.target = bypassTarget ? 1.0 : 0.0;
    if (bypassMix.value > 1e-6 || bypassMix.target > 0)
        for (int i = 0; i < n; ++i) { const double k = bypassMix.next(); L[i] += k * (dL[i] - L[i]); R[i] += k * (dR[i] - R[i]); }

    // ---- desvanecimento na troca de motor
    if (fadeState == 1)
    {
        for (int i = 0; i < n; ++i) { const double g = 1.0 - (double) (i + 1) / n; L[i] *= g; R[i] *= g; }
        fadeState = 2;   // troca no próximo bloco
        switchGain.reset (0.0);
    }
    else if (switchGain.value < 1.0 - 1e-9)
        for (int i = 0; i < n; ++i) { const double g = switchGain.next(); L[i] *= g; R[i] *= g; }

    // medidor de saída
    {
        float pl = 0, pr = 0; double el = 0, er = 0;
        for (int i = 0; i < n; ++i) { pl = std::max (pl, (float) std::abs (L[i])); pr = std::max (pr, (float) std::abs (R[i])); el += L[i] * L[i]; er += R[i] * R[i]; }
        outMeter.peakL.store (std::max (outMeter.peakL.load(), pl)); outMeter.peakR.store (std::max (outMeter.peakR.load(), pr));
        outMeter.rmsL.store ((float) std::sqrt (el / n)); outMeter.rmsR.store ((float) std::sqrt (er / n));
        if (pl > 1.0f || pr > 1.0f) outMeter.clip.store (true);
    }

    scPresent.store (sc);
    procModeForAnalysis.store (juce::jlimit (0, 2, (int) pProcMode->load()));
    pushAnalysis (n);
}

void EQMotionProcessor::pushAnalysis (int n)
{
    if (fifo.getFreeSpace() < n) return;   // a thread de análise está atrasada: perde este bloco
    int s1, n1, s2, n2;
    fifo.prepareToWrite (n, s1, n1, s2, n2);
    const double* srcs[6] = { inCopy.getReadPointer (0), inCopy.getReadPointer (1), work.getReadPointer (0),
                              work.getReadPointer (1), scBuf.getReadPointer (0), scBuf.getReadPointer (1) };
    for (int c = 0; c < 6; ++c)
    {
        float* d = fifoBuf.getWritePointer (c);
        for (int i = 0; i < n1; ++i) d[s1 + i] = (float) srcs[c][i];
        for (int i = 0; i < n2; ++i) d[s2 + i] = (float) srcs[c][n1 + i];
    }
    fifo.finishedWrite (n1 + n2);
}

bool EQMotionProcessor::copySpectrum (SpectrumFrame& out)
{
    const juce::SpinLock::ScopedLockType lk (spectrumLock);
    if (spectrum.stamp == out.stamp) return false;
    out = spectrum;
    return true;
}

// -----------------------------------------------------------------------------
//  Thread de mensagens
// -----------------------------------------------------------------------------
void EQMotionProcessor::timerCallback()
{
    ++timerTicks;
    {
        juce::ValueTree pending;
        { const juce::ScopedLock sl (pendingStateLock); std::swap (pending, pendingStateTree); }
        if (pending.isValid()) state.fromValueTree (pending);
    }

    // latência e motor
    const double sr = sampleRateHz;
    const int mode = juce::jlimit (0, 2, (int) pPhase->load()), q = juce::jlimit (0, 2, (int) pLpQ->load());
    const int la = mode == 1 ? juce::roundToInt (pLook->load() * 0.001 * sr) : 0;
    const int cfg = encodeConfig (mode, q, la);
    if (cfg != requestedConfig.load() && osMain != nullptr)
    {
        const int lat = mode == 0 ? 0 : mode == 1 ? juce::roundToInt (osMain->getLatencyInSamples()) + la : lp[(size_t) q].latency();
        setLatencySamples (lat);
        requestedConfig.store (cfg);
    }

    if (timerTicks % 750 == 0) lic::License::get().checkExpiry();   // a cada 30 s
    if (timerTicks % 3 == 0) publishCurve();

    if (learning.load() && onSmartChanged && timerTicks % 5 == 0) onSmartChanged();
}

void EQMotionProcessor::publishCurve()
{
    if (shared == nullptr) return;
    const auto b = currentBands();
    int h = 17;
    for (auto& p : b)
        h = h * 31 + (int) (p.active * 7 + p.enabled * 3 + (int) p.type * 11 + (int) p.slope * 13 + (int) (p.freq * 10)
                            + (int) (p.gainDb * 100) + (int) (p.q * 1000) + (int) p.placement * 5);
    h = h * 31 + (int) pPhase->load() + (int) (pGainScale->load() * 10) + (pBypass->load() > 0.5f ? 1 : 0);
    if (h == lastPublishedHash) return;
    lastPublishedHash = h;
    static const auto freqs = curve::logFreqs (kDisplayPoints, kDisplayLo, kDisplayHi);
    std::array<double, kMaxBands> gains {};
    for (int i = 0; i < kMaxBands; ++i) gains[(size_t) i] = effectiveGain (b[(size_t) i], pGainScale->load() * 0.01, 0.0);
    std::vector<float> out, tmp;
    curve::totalCurve (b, gains, (PhaseMode) (int) pPhase->load(), sampleRateHz, freqs, Placement::Stereo, out, tmp);
    const juce::SpinLock::ScopedLockType lk (shared->lock);
    shared->curveDb = std::move (out);
    shared->bypassed = pBypass->load() > 0.5f;
}

void EQMotionProcessor::updateTrackProperties (const TrackProperties& p)
{
    if (shared == nullptr) return;
    const juce::SpinLock::ScopedLockType lk (shared->lock);
    if (p.name.has_value() && p.name->isNotEmpty()) shared->name = *p.name;
    if (p.colourARGB.has_value()) shared->colour = juce::Colour ((juce::uint32) *p.colourARGB);
}

// -----------------------------------------------------------------------------
//  Smart Ops
// -----------------------------------------------------------------------------
void EQMotionProcessor::smartSetOp (SmartOp op)
{
    smart.op = op;
    if (! smart.rangeTouched) smartOpDefaultRange (op, smart.fromHz, smart.toHz);
    smartRecompute();
}

void EQMotionProcessor::smartStartLearning()
{
    {
        const juce::ScopedLock lk (learnerLock);
        learner.start (sampleRateHz);
    }
    learning.store (true);
    smart.learning = true;
    smart.hasResult = false;
    smart.suggestion = {};
    smart.message = juce::String::fromUTF8 ("A aprender… deixe tocar a passagem e carregue em Parar.");
    if (onSmartChanged) onSmartChanged();
}

double EQMotionProcessor::smartLearnSeconds() const
{
    const juce::ScopedLock lk (learnerLock);
    return learner.seconds();
}

void EQMotionProcessor::smartStopLearning()
{
    learning.store (false);
    {
        const juce::ScopedLock lk (learnerLock);
        learner.stop();
        smart.result = learner.result();
    }
    smart.learning = false;
    smart.hasResult = true;
    smartRecompute();
}

void EQMotionProcessor::smartRecompute()
{
    if (smart.hasResult)
    {
        SmartSettings s;
        s.op = smart.op; s.fromHz = smart.fromHz; s.toHz = smart.toHz; s.amount = smart.amount;
        if (smart.op == SmartOp::MatchReference && smart.referenceInstance > 0)
            for (auto& inst : InstanceRegistry::get().all())
                if (inst->id == smart.referenceInstance && inst != shared)
                {
                    const juce::SpinLock::ScopedLockType lk (inst->lock);
                    s.referenceDb = inst->avgGridDb;
                }
        if (smartOpNeedsSidechain (smart.op) && ! smart.result.hasSidechain)
        {
            smart.suggestion = {};
            smart.message = juce::String::fromUTF8 ("Sem sinal no sidechain: ligue a fonte a destacar ao sidechain e aprenda de novo.");
        }
        else
        {
            smart.suggestion = suggest (smart.result, s);
            smart.message = juce::String::fromUTF8 (smart.suggestion.summary.c_str());
        }
    }
    if (onSmartChanged) onSmartChanged();
}

void EQMotionProcessor::smartApply()
{
    if (! smart.suggestion.ok) return;
    const int free = kMaxBands - state.activeBandCount();
    int applied = 0;
    const int opId = state.applySmart (smart.suggestion.bands, applied);
    smart.lastAppliedOp = opId;
    const int wanted = (int) smart.suggestion.bands.size();
    smart.message = applied == wanted ? juce::String::fromUTF8 ("Aplicado: ") + juce::String (applied) + (applied == 1 ? " banda." : " bandas.")
                                      : juce::String::fromUTF8 ("Só havia espaço para ") + juce::String (juce::jmin (free, applied)) + " de " + juce::String (wanted) + " bandas.";
    smart.suggestion = {};
    smart.hasResult = false;
    if (onSmartChanged) onSmartChanged();
}

void EQMotionProcessor::smartRevert()
{
    if (! state.hasSmartBands (smart.lastAppliedOp)) return;
    state.revertSmart (smart.lastAppliedOp);
    smart.message = juce::String::fromUTF8 ("Operação revertida (as suas outras edições ficaram intactas).");
    if (onSmartChanged) onSmartChanged();
}

// -----------------------------------------------------------------------------
//  Estado
// -----------------------------------------------------------------------------
void EQMotionProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto root = apvts.copyState();
    root.removeChild (root.getChildWithName ("EQM_STATE"), nullptr);
    root.removeChild (root.getChildWithName ("EQM_UI"), nullptr);
    root.removeChild (root.getChildWithName ("EQM_SMART"), nullptr);
    root.appendChild (state.toValueTree(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    juce::ValueTree sm ("EQM_SMART");
    sm.setProperty ("op", (int) smart.op, nullptr);
    sm.setProperty ("from", smart.fromHz, nullptr);
    sm.setProperty ("to", smart.toHz, nullptr);
    sm.setProperty ("amount", smart.amount, nullptr);
    sm.setProperty ("rangeTouched", smart.rangeTouched, nullptr);
    sm.setProperty ("lastOp", smart.lastAppliedOp, nullptr);
    root.appendChild (sm, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void EQMotionProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    auto st = tree.getChildWithName ("EQM_STATE").createCopy();
    auto ui = tree.getChildWithName ("EQM_UI").createCopy();
    auto sm = tree.getChildWithName ("EQM_SMART").createCopy();
    tree.removeChild (tree.getChildWithName ("EQM_STATE"), nullptr);
    tree.removeChild (tree.getChildWithName ("EQM_UI"), nullptr);
    tree.removeChild (tree.getChildWithName ("EQM_SMART"), nullptr);
    apvts.replaceState (tree);
    if (ui.isValid()) uiState.copyPropertiesAndChildrenFrom (ui, nullptr);
    if (sm.isValid())
    {
        smart.op = (SmartOp) juce::jlimit (0, (int) SmartOp::NumOps - 1, (int) sm.getProperty ("op", 0));
        smart.fromHz = sm.getProperty ("from", 20.0); smart.toHz = sm.getProperty ("to", 20000.0);
        smart.amount = sm.getProperty ("amount", 1.0); smart.rangeTouched = sm.getProperty ("rangeTouched", false);
        smart.lastAppliedOp = sm.getProperty ("lastOp", 0);
    }
    if (juce::MessageManager::existsAndIsCurrentThread()) state.fromValueTree (st);
    else { const juce::ScopedLock sl (pendingStateLock); pendingStateTree = st; }
}

juce::AudioProcessorEditor* EQMotionProcessor::createEditor() { return new EQMotionEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EQMotionProcessor(); }
