#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "CurveCore.h"
#include "GroupRegistry.h"
#include "License.h"

namespace ids
{
    static const juce::String role      = "role";
    static const juce::String group     = "group";
    static const juce::String signal    = "signal";
    static const juce::String level     = "level";
    static const juce::String sineFreq  = "sineFreq";
    static const juce::String fftSize   = "fftSize";
    static const juce::String source    = "source";
    static const juce::String channel   = "channel";
    static const juce::String averages  = "averages";
    static const juce::String smoothing = "smoothing";
    static const juce::String range     = "range";
    static const juce::String showPhase = "showPhase";
    static const juce::String autoSync  = "autoSync";
    static const juce::String latency   = "latency";
    static const juce::String freeze    = "freeze";
    static const juce::String muteOut   = "muteOut";
}

// Resultados publicados pela thread de análise e lidos pelo editor
struct AnalysisResults
{
    enum class View { None, Generator, Response, Harmonics, Music };
    View view = View::None;

    pca::Curve           curve;        // resposta (dB/fase) ou espetro (dBFS)
    pca::HarmonicResult  harm;
    pca::ExcitationSpec  spec;
    double delaySamples   = 0.0;       // latência detetada/aplicada
    double levelDb        = -200.0;    // nível do sinal analisado
    double dryLevelDb     = -200.0;    // modo música: nível do sidechain
    int    frames         = 0;
    bool   hasSignal      = false;
    bool   linked         = false;     // definições vindas de um Gerador do grupo
    bool   sidechainMissing = false;
    uint64_t serial       = 0;
};

class CurveAnalyzerProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    CurveAnalyzerProcessor();
    ~CurveAnalyzerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
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

    // ---- API para o editor -------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void getResults (AnalysisResults& out) const
    {
        const juce::ScopedLock sl (resultsLock);
        out = results;
    }
    void requestReset() { resetRequested.store (true); }

    // Curvas de referência (sobreposição) — só tocadas na thread de mensagens
    std::vector<pca::Curve> references;

    // Helpers de parâmetros
    int    paramIndex (const juce::String& id) const { return (int) std::lround (apvts.getRawParameterValue (id)->load()); }
    float  paramValue (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    static int    fftSizeForIndex (int idx)  { return 4096 << juce::jlimit (0, 4, idx); }
    static int    averagesForIndex (int idx) { static const int v[] { 1, 2, 4, 8, 16, 32, 0 }; return v[juce::jlimit (0, 6, idx)]; }
    static double smoothingForIndex (int idx){ static const double v[] { 0.0, 1.0 / 24, 1.0 / 12, 1.0 / 6, 1.0 / 3 }; return v[juce::jlimit (0, 4, idx)]; }
    static double rangeForIndex (int idx)    { static const double v[] { 3, 6, 12, 24, 48 }; return v[juce::jlimit (0, 4, idx)]; }

private:
    void timerCallback() override;          // reconstrói o sinal do Gerador
    void runAnalysis();                     // corre na thread de análise
    pca::ExcitationSpec ownSpec() const;

    struct Worker : juce::Thread
    {
        explicit Worker (CurveAnalyzerProcessor& p) : juce::Thread ("Piradex Analysis"), owner (p) {}
        void run() override { while (! threadShouldExit()) { owner.runAnalysis(); wait (8); } }
        CurveAnalyzerProcessor& owner;
    };

    // --- Estado partilhado áudio <-> análise ---
    static constexpr int kRingSize = 1 << 19;       // 524288 amostras por canal
    static constexpr int kRingMask = kRingSize - 1;
    std::vector<float> ringWet, ringDry;
    std::atomic<int64_t> writeCount { 0 };
    std::atomic<double>  sampleRate { 0.0 };
    std::atomic<bool>    sidechainActive { false };
    std::atomic<bool>    resetRequested { false };

    // --- Gerador ---
    juce::CriticalSection genLock;
    std::vector<float>    genSignal;
    pca::ExcitationSpec   genSpec;
    bool                  genValid = false;
    size_t                genPos = 0;

    // --- Thread de análise (estado privado dela) ---
    struct AnalysisKey
    {
        pca::ExcitationSpec spec; int source = 0; int channel = 0; bool generator = false;
        bool operator!= (const AnalysisKey& o) const
        { return spec != o.spec || source != o.source || channel != o.channel || generator != o.generator; }
    };
    AnalysisKey                       curKey;
    bool                              haveKey = false;
    std::unique_ptr<pca::Excitation>  excitation;
    pca::ResponseAnalyzer             respAn;
    pca::HarmonicAnalyzer             harmAn;
    pca::TransferAnalyzer             xferAn;
    int64_t                           nextFrame = 0;
    double                            heldDelay = 0.0;
    int                               musicDelay = 0;
    uint32_t                          lastPublishMs = 0;
    std::vector<float>                frameWet, frameDry;
    std::vector<pca::cpx>             tmpH;
    std::vector<char>                 tmpValid;
    std::vector<double>               tmpCoh, tmpAmp;

    mutable juce::CriticalSection resultsLock;
    AnalysisResults       results;

    Worker worker { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveAnalyzerProcessor)
};
