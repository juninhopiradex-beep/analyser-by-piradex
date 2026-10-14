// =============================================================================
//  EQMotion by Piradex — processador
//
//  Threads:
//   • áudio: só DSP com memória pré-alocada (sem locks, ficheiros ou UI)
//   • análise: espetros, aprendizagem das Smart Ops, publicação para outras instâncias
//   • mensagens: latência, estado, desfazer, presets, licença
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "CurveMath.h"
#include "dsp/LinearPhaseEngine.h"
#include "dsp/Character.h"
#include "dsp/SmartOps.h"
#include "InstanceRegistry.h"
#include "StateManager.h"
#include "License.h"

// Espetros prontos para o ecrã (copiados sob lock pela UI)
struct SpectrumFrame
{
    std::vector<float> pre, post, postA, postB, sc, peak;
    std::vector<double> freqs;
    bool hasSc = false;
    int split = 0;          // 0 = soma, 1 = L/R, 2 = M/S
    juce::uint32 stamp = 0;
};

struct AnalyzerSettings
{
    std::atomic<int> fftSize { 8192 };
    std::atomic<int> speed { 1 };            // 0 lento, 1 médio, 2 rápido
    std::atomic<float> slope { 4.5f };
    std::atomic<bool> freeze { false };
    std::atomic<bool> showPre { true }, showPost { true }, showSc { true };
};

class EQMotionProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    EQMotionProcessor();
    ~EQMotionProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EQMotion by Piradex"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ids::bypass); }
    void updateTrackProperties (const TrackProperties&) override;

    // ---------------------------------------------------------------- acesso da UI
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;
    StateManager state { apvts, presetManager };
    AnalyzerSettings analyzer;
    juce::ValueTree uiState { "EQM_UI" };

    eqm::BandArray currentBands() const;
    double currentSampleRate() const { return sampleRateHz; }
    eqm::PhaseMode activePhaseMode() const { return (eqm::PhaseMode) activeMode.load(); }
    int  currentLatency() const { return getLatencySamples(); }
    double bandDynamicDb (int b) const { return dynDelta[(size_t) b].load(); }
    double bandDetectorDb (int b) const { return dynLevel[(size_t) b].load(); }
    double deharshDb() const { return deharshDelta.load(); }
    double autoGainDb() const { return autoGainNow.load(); }
    bool  sidechainActive() const { return scPresent.load(); }

    void setListenBand (int b) { listenBand.store (b); }
    int  listenBandIndex() const { return listenBand.load(); }

    struct Meter { std::atomic<float> peakL { 0 }, peakR { 0 }, rmsL { 0 }, rmsR { 0 }; std::atomic<bool> clip { false }; };
    Meter inMeter, outMeter;

    bool copySpectrum (SpectrumFrame& out);   // false se não houver nada novo

    std::shared_ptr<InstanceShared> shared;  // esta instância no registo

    // ---------------------------------------------------------------- Smart Ops
    struct SmartState
    {
        eqm::SmartOp op = eqm::SmartOp::Deresonate;
        double fromHz = 20, toHz = 20000, amount = 1.0;
        bool rangeTouched = false;
        int referenceInstance = 0;          // 0 = sidechain
        bool learning = false;
        bool hasResult = false;
        eqm::SmartSuggestion suggestion;
        int lastAppliedOp = 0;
        juce::String message;
        eqm::LearnResult result;
    } smart;
    void smartSetOp (eqm::SmartOp op);
    void smartStartLearning();
    void smartStopLearning();
    void smartRecompute();
    void smartApply();
    void smartRevert();
    double smartLearnSeconds() const;
    std::function<void()> onSmartChanged;

private:
    template <typename T> void processImpl (juce::AudioBuffer<T>& buffer, bool hostBypassed);
    void processChunk (int n);
    void timerCallback() override;
    void readBandParams();
    void publishCurve();
    void pushAnalysis (int n);

    // parâmetros (ponteiros raw)
    std::array<params::BandPointers, eqm::kMaxBands> bandPtrs;
    std::atomic<float>* pBypass = nullptr; std::atomic<float>* pOutput = nullptr; std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pProcMode = nullptr; std::atomic<float>* pPhase = nullptr; std::atomic<float>* pLpQ = nullptr;
    std::atomic<float>* pLook = nullptr; std::atomic<float>* pChar = nullptr; std::atomic<float>* pDeh = nullptr;
    std::atomic<float>* pDehAmt = nullptr; std::atomic<float>* pDehAtk = nullptr; std::atomic<float>* pDehRel = nullptr;
    std::atomic<float>* pDelta = nullptr; std::atomic<float>* pGainScale = nullptr;

    // configuração do motor (mudada na thread de mensagens depois de reportar a latência)
    static int encodeConfig (int mode, int quality, int look) { return mode | (quality << 2) | (look << 4); }
    std::atomic<int> requestedConfig { 0 };
    int activeConfig = -1;
    std::atomic<int> activeMode { 0 };
    int fadeState = 0;                      // 0 normal, 1 a desvanecer antes de mudar de motor
    int pendingConfig = 0;

    double sampleRateHz = 48000;
    int maxChunk = 512;
    int osFactor = 4;
    eqm::IIREngine iirZL, iirNP;
    std::array<eqm::LinearPhaseEngine, 3> lp;
    std::unique_ptr<juce::dsp::Oversampling<double>> osMain, osDry, osSc;
    eqm::DelayLine dryL, dryR;
    eqm::CharacterStage character;
    eqm::LoudnessMatch loudness;
    eqm::Biquad listenL[eqm::kMaxSections], listenR[eqm::kMaxSections];
    eqm::DigitalBand listenDesign;
    int listenActive = -1;
    eqm::BandParams listenParams;
    std::atomic<int> listenBand { -1 };
    eqm::Ramp bypassMix, deltaMix, switchGain, listenMix;
    eqm::Smoothed outGain;

    juce::AudioBuffer<double> work, dry, scBuf, inCopy;
    eqm::BandArray bands {};
    eqm::GlobalDynSettings dynSettings;
    bool scAvailableThisBlock = false, bypassTarget = false;
    std::atomic<bool> scAvailableForLearning { false };

    std::array<std::atomic<double>, eqm::kMaxBands> dynDelta {}, dynLevel {};
    std::atomic<double> deharshDelta { 0 }, autoGainNow { 0 };
    std::atomic<bool> scPresent { false };

    // ---------------------------------------------------------------- análise
    class AnalysisThread;
    std::unique_ptr<AnalysisThread> analysis;
    juce::AbstractFifo fifo { 1 << 15 };
    juce::AudioBuffer<float> fifoBuf { 6, 1 << 15 };
    std::atomic<int> procModeForAnalysis { 0 };

    juce::SpinLock spectrumLock;
    SpectrumFrame spectrum;
    juce::uint32 spectrumStamp = 0, lastCopiedStamp = 0;

    juce::CriticalSection learnerLock;
    eqm::SmartLearner learner;
    std::atomic<bool> learning { false };

    int timerTicks = 0;
    juce::CriticalSection pendingStateLock;
    juce::ValueTree pendingStateTree;
    int lastPublishedHash = 0;
    friend class AnalysisThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQMotionProcessor)
};
