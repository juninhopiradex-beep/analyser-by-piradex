#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Params.h"
#include "SpaceCore.h"

// Dados de análise partilhados entre o áudio e a interface (sem locks)
struct SharedAnalysis
{
    static constexpr int kMax = mxs::SpaceEngine::kMaxBands;
    std::atomic<int> numBands { 0 };
    std::array<std::atomic<float>, kMax> freq, mainDb, scDb, redDb;
    std::atomic<float> masking { 0.0f }, avgRed { 0.0f }, maxRed { 0.0f };
    std::atomic<unsigned> frame { 0 };

    SharedAnalysis()
    {
        for (int i = 0; i < kMax; ++i) { freq[(size_t) i] = 1000.0f; mainDb[(size_t) i] = -200.0f; scDb[(size_t) i] = -200.0f; redDb[(size_t) i] = 0.0f; }
    }
};

class MixspaceProcessor : public juce::AudioProcessor
{
public:
    MixspaceProcessor();
    ~MixspaceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // Bypass do host: mesmo caminho (crossfade + mesma latência), para não haver saltos de alinhamento
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override
    {
        hostBypass = true;
        processBlock (b, m);
        hostBypass = false;
    }
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // ---- estado para a interface
    bool isSidechainConnected() const { return scConnected.load(); }
    bool isSidechainSilent() const    { return scSilentSamples.load() > (int) (getSampleRate() * 1.5); }
    bool isMainSilent() const         { return mainSilentSamples.load() > (int) (getSampleRate() * 1.5); }
    int  getCurrentLatency() const    { return currentLatency.load(); }
    void setEditorOpen (bool open)    { editorOpen.store (open); }

    float getUiScale() const          { return uiScale.load(); }
    void  setUiScale (float s)        { uiScale.store (s); }

    juce::AudioProcessorValueTreeState apvts;
    SharedAnalysis analysis;

private:
    void selectEngine (int quality);
    void publishAnalysis (const mxs::SpaceEngine& e);

    std::array<mxs::SpaceEngine, 3> engines;     // LIVE / MIX / MASTER, todos preparados
    int activeEngine = 1;

    juce::AudioBuffer<float> wet, dry, scMono, scDelayed;
    juce::SmoothedValue<float> bypassMix, deltaMix, scListenMix;
    unsigned lastPublishedFrame = 0;
    bool hostBypass = false;

    std::atomic<float>* pSpace = nullptr;  std::atomic<float>* pRange = nullptr;  std::atomic<float>* pFocus = nullptr;
    std::atomic<float>* pLow = nullptr;    std::atomic<float>* pHigh = nullptr;
    std::atomic<float>* pAttack = nullptr; std::atomic<float>* pRelease = nullptr; std::atomic<float>* pSmooth = nullptr;
    std::atomic<float>* pStereo = nullptr; std::atomic<float>* pQuality = nullptr;
    std::atomic<float>* pDelta = nullptr;  std::atomic<float>* pScListen = nullptr; std::atomic<float>* pBypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    std::atomic<bool> scConnected { false }, editorOpen { false };
    std::atomic<int> scSilentSamples { 0 }, mainSilentSamples { 0 }, currentLatency { 0 };
    std::atomic<float> uiScale { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixspaceProcessor)
};
