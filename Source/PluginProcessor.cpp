#include "PluginProcessor.h"
#include "PluginEditor.h"

MixspaceProcessor::MixspaceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIXSPACE", createParameterLayout())
{
    pSpace    = apvts.getRawParameterValue (ids::space);
    pRange    = apvts.getRawParameterValue (ids::range);
    pFocus    = apvts.getRawParameterValue (ids::focus);
    pLow      = apvts.getRawParameterValue (ids::lowCut);
    pHigh     = apvts.getRawParameterValue (ids::highCut);
    pAttack   = apvts.getRawParameterValue (ids::attack);
    pRelease  = apvts.getRawParameterValue (ids::release);
    pSmooth   = apvts.getRawParameterValue (ids::smooth);
    pStereo   = apvts.getRawParameterValue (ids::stereo);
    pQuality  = apvts.getRawParameterValue (ids::quality);
    pDelta    = apvts.getRawParameterValue (ids::delta);
    pScListen = apvts.getRawParameterValue (ids::scListen);
    pBypass   = apvts.getRawParameterValue (ids::bypass);
    bypassParam = apvts.getParameter (ids::bypass);
}

bool MixspaceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut) return false;
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo()) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void MixspaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int q = 0; q < 3; ++q)
        engines[(size_t) q].prepare (sampleRate, mxs::fftSizeFor (q, sampleRate));

    const int chunk = juce::jmax (samplesPerBlock, 512);
    wet.setSize (2, chunk, false, true, false);
    dry.setSize (2, chunk, false, true, false);
    scMono.setSize (1, chunk, false, true, false);
    scDelayed.setSize (1, chunk, false, true, false);

    for (auto* s : { &bypassMix, &deltaMix, &scListenMix }) s->reset (sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    deltaMix.setCurrentAndTargetValue (pDelta->load() > 0.5f ? 1.0f : 0.0f);
    scListenMix.setCurrentAndTargetValue (pScListen->load() > 0.5f ? 1.0f : 0.0f);

    selectEngine ((int) pQuality->load());
}

void MixspaceProcessor::selectEngine (int quality)
{
    activeEngine = juce::jlimit (0, 2, quality);
    auto& e = engines[(size_t) activeEngine];
    e.reset();
    analysis.numBands.store (e.numBands());
    for (int b = 0; b < e.numBands(); ++b)
        analysis.freq[(size_t) b].store (e.bandFreq (b));
    lastPublishedFrame = 0;
    currentLatency.store (e.getLatency());
    setLatencySamples (e.getLatency());
}

void MixspaceProcessor::publishAnalysis (const mxs::SpaceEngine& e)
{
    const int nb = e.numBands();
    analysis.numBands.store (nb);
    for (int b = 0; b < nb; ++b)
    {
        analysis.freq[(size_t) b].store (e.bandFreq (b), std::memory_order_relaxed);
        analysis.mainDb[(size_t) b].store (e.bandMainDb (b), std::memory_order_relaxed);
        analysis.scDb[(size_t) b].store (e.bandScDb (b), std::memory_order_relaxed);
        analysis.redDb[(size_t) b].store (e.bandRedDb (b), std::memory_order_relaxed);
    }
    analysis.masking.store (e.getMasking());
    analysis.avgRed.store (e.getAverageReductionDb());
    analysis.maxRed.store (e.getMaxReductionDb());
    analysis.frame.fetch_add (1);
}

void MixspaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    if ((int) pQuality->load() != activeEngine)
        selectEngine ((int) pQuality->load());

    auto& engine = engines[(size_t) activeEngine];

    mxs::Params p;
    p.space = pSpace->load();
    p.rangeDb = pRange->load();
    p.focus = pFocus->load();
    p.lowCutHz = pLow->load();
    p.highCutHz = pHigh->load();
    p.attackMs = pAttack->load();
    p.releaseMs = pRelease->load();
    p.smooth = pSmooth->load();
    p.stereo = (mxs::StereoMode) juce::jlimit (0, 4, (int) pStereo->load());
    engine.setParams (p);
    engine.setAnalysisWanted (editorOpen.load());

    bypassMix.setTargetValue (hostBypass || pBypass->load() > 0.5f ? 1.0f : 0.0f);
    deltaMix.setTargetValue (pDelta->load() > 0.5f ? 1.0f : 0.0f);
    scListenMix.setTargetValue (pScListen->load() > 0.5f ? 1.0f : 0.0f);

    auto mainBuf = getBusBuffer (buffer, true, 0);
    const int mainCh = juce::jmin (2, mainBuf.getNumChannels());
    if (mainCh == 0) return;

    juce::AudioBuffer<float> scBuf;
    bool scOk = false;
    if (getBusCount (true) > 1)
    {
        auto* scBus = getBus (true, 1);
        if (scBus != nullptr && scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
        {
            scBuf = getBusBuffer (buffer, true, 1);
            scOk = scBuf.getNumChannels() > 0;
        }
    }
    scConnected.store (scOk);

    // detecção de silêncio (para as mensagens da interface)
    {
        float mainPeak = 0.0f, scPeak = 0.0f;
        for (int ch = 0; ch < mainCh; ++ch) mainPeak = juce::jmax (mainPeak, mainBuf.getMagnitude (ch, 0, numSamples));
        if (scOk) for (int ch = 0; ch < scBuf.getNumChannels(); ++ch) scPeak = juce::jmax (scPeak, scBuf.getMagnitude (ch, 0, numSamples));
        mainSilentSamples.store (mainPeak < 1.0e-5f ? juce::jmin (1 << 30, mainSilentSamples.load() + numSamples) : 0);
        scSilentSamples.store (scPeak < 1.0e-5f ? juce::jmin (1 << 30, scSilentSamples.load() + numSamples) : 0);
    }

    const int chunk = wet.getNumSamples();
    for (int start = 0; start < numSamples; start += chunk)
    {
        const int m = juce::jmin (chunk, numSamples - start);

        const float* sc = nullptr;
        if (scOk)
        {
            const int nsc = scBuf.getNumChannels();
            auto* dst = scMono.getWritePointer (0);
            juce::FloatVectorOperations::copy (dst, scBuf.getReadPointer (0, start), m);
            for (int ch = 1; ch < nsc; ++ch)
                juce::FloatVectorOperations::add (dst, scBuf.getReadPointer (ch, start), m);
            if (nsc > 1) juce::FloatVectorOperations::multiply (dst, 1.0f / (float) nsc, m);
            sc = dst;
        }

        engine.process (mainBuf.getReadPointer (0, start),
                        mainCh > 1 ? mainBuf.getReadPointer (1, start) : nullptr,
                        sc,
                        wet.getWritePointer (0), wet.getWritePointer (1),
                        dry.getWritePointer (0), dry.getWritePointer (1),
                        scDelayed.getWritePointer (0), m);

        const float* scD = scDelayed.getReadPointer (0);
        for (int i = 0; i < m; ++i)
        {
            const float b = bypassMix.getNextValue();
            const float d = deltaMix.getNextValue();
            const float s = scListenMix.getNextValue();
            for (int ch = 0; ch < mainCh; ++ch)
            {
                const float w = wet.getSample (ch, i), dr = dry.getSample (ch, i);
                float out = w + d * ((dr - w) - w);      // DELTA: ouve só o que foi retirado
                out += s * (scD[i] - out);                 // SC LISTEN
                out += b * (dr - out);                     // BYPASS com a mesma latência (click-free)
                mainBuf.setSample (ch, start + i, out);
            }
        }
    }

    if (editorOpen.load() && engine.getFrameCount() != lastPublishedFrame)
    {
        lastPublishedFrame = engine.getFrameCount();
        publishAnalysis (engine);
    }
}

juce::AudioProcessorEditor* MixspaceProcessor::createEditor()
{
    return new MixspaceEditor (*this);
}

void MixspaceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MixspaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            uiScale.store ((float) tree.getProperty ("uiScale", 1.0f));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixspaceProcessor();
}
