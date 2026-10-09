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
    static const juce::String view      = "view";
}

// Papel de cada instância
enum class Role : int { Analyser = 0, Generator = 1, Host = 2 };
// Separadores de vista
enum class ViewTab : int { Curve = 0, Wave = 1, Spectrum = 2, Sweep = 3 };

// Resultados publicados pela thread de análise e lidos pelo editor
struct AnalysisResults
{
    enum class View { None, Generator, Response, Harmonics, Music };
    View view = View::None;

    pca::Curve           curve;        // resposta (dB/fase), espetro (harmónicos) ou diferença (música)
    pca::HarmonicResult  harm;
    pca::ExcitationSpec  spec;
    double delaySamples   = 0.0;       // latência detetada/aplicada
    bool   latencyReliable = false;    // relógio comum (Host, música, ou DAW a tocar)
    double levelDb        = -200.0;    // nível do sinal analisado
    double dryLevelDb     = -200.0;    // modo música: nível do dry
    int    frames         = 0;
    bool   hasSignal      = false;
    bool   linked         = false;     // definições vindas de um Gerador do grupo
    bool   sidechainMissing = false;
    bool   hostMode       = false;
    bool   hostLoaded     = false;
    juce::String hostName;

    // Vista "Onda"
    std::vector<float> waveIn, waveOut;   // harmónicos: entrada/saída · resposta: só waveOut (IR) · música: dry/wet
    double waveStartMs = 0.0, waveSpanMs = 0.0;

    // Vista "Espetro" (música)
    pca::Curve specDry, specWet;

    // Vista "Varrimento"
    int sweepKind = 0, sweepStep = -1, sweepTotal = 0;
    std::vector<pca::SweepPoint> sweep;
    int sweepShownKind = 0;               // tipo do último varrimento guardado

    uint64_t serial = 0;
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

    // Varrimentos: pedidos pelo Analisador (ou pelo próprio Host), executados pelo Gerador
    void requestSweep (pca::SweepKind kind);
    void stopSweep();

    // ---- Modo Host: um plugin carregado dentro do ANALYSER ---------------
    struct InstalledPlugin { juce::String name, format, fileOrId; };
    std::vector<InstalledPlugin> listInstalledPlugins();          // não carrega nada (rápido)
    void loadHostedPlugin (const juce::String& formatName, const juce::String& fileOrId,
                           std::function<void (const juce::String& error)> done = {});
    void unloadHostedPlugin();
    void openHostedEditor();
    bool hasHostedPlugin() const       { return hostedLoaded.load(); }
    juce::String getHostedName() const { return hostedNameStr; }

    // Curvas de referência (sobreposição) — só tocadas na thread de mensagens
    std::vector<pca::Curve> references;

    // Helpers de parâmetros
    int    paramIndex (const juce::String& id) const { return (int) std::lround (apvts.getRawParameterValue (id)->load()); }
    float  paramValue (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    Role   role() const { return (Role) juce::jlimit (0, 2, paramIndex (ids::role)); }
    static int    fftSizeForIndex (int idx)  { return 4096 << juce::jlimit (0, 4, idx); }
    static int    averagesForIndex (int idx) { static const int v[] { 8, 1, 2, 4, 8, 16, 32, 0 }; return v[juce::jlimit (0, 7, idx)]; }
    static double smoothingForIndex (int idx){ static const double v[] { 0.0, 1.0 / 24, 1.0 / 12, 1.0 / 6, 1.0 / 3 }; return v[juce::jlimit (0, 4, idx)]; }
    static double rangeForIndex (int idx)    { static const double v[] { 3, 6, 12, 24, 48 }; return v[juce::jlimit (0, 4, idx)]; }

private:
    void timerCallback() override;          // reconstrói o sinal do Gerador
    void runAnalysis();                     // corre na thread de análise
    pca::ExcitationSpec ownSpec() const;
    pca::GroupSlot& slotForRole (Role r);
    void renderGenerator (float* dst, int numSamples, pca::GroupSlot& slot, int64_t clock);   // thread de áudio
    int64_t blockClock (int64_t ownCounter, bool& fromTimeline);
    void writeRing (const float* wl, const float* wr, const float* dl, const float* dr, int64_t at, int n);
    void prepareHostBuffers();
    void applyPendingHostState();

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
    std::atomic<int>     maxBlock { 512 };
    std::atomic<bool>    sidechainActive { false };
    std::atomic<bool>    resetRequested { false };
    std::atomic<int64_t> clockDelta { 0 };          // relógio do Gerador - índice do anel
    std::atomic<bool>    clockFromTimeline { false };
    int64_t              genClock = 0;              // contador próprio do Gerador (DAW parado)

    // --- Gerador (tabela periódica + seno sintetizado + varrimentos) ---
    juce::CriticalSection genLock;
    std::vector<float>    genSignal;
    pca::ExcitationSpec   genSpec;
    bool                  genValid = false;
    size_t                genPos = 0;
    int                   sweepKindRun = 0, sweepStepRun = -1;
    int64_t               sweepStepSamples = 0;
    std::vector<double>   sweepLevels, sweepFreqs;     // preenchidos no construtor
    pca::GroupSlot        hostSlot;                    // "grupo" privado do modo Host

    // --- Modo Host ---
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> hosted;
    juce::CriticalSection hostLock;
    juce::AudioBuffer<float> hostBuf;
    std::vector<float> genScratch;
    juce::MidiBuffer hostMidi;
    std::unique_ptr<juce::DocumentWindow> hostWindow;
    juce::String hostedNameStr, hostedFormat, hostedFileOrId;
    std::atomic<bool> hostedLoaded { false };
    std::unique_ptr<juce::MemoryBlock> pendingHostState;
    std::shared_ptr<int> aliveToken = std::make_shared<int> (0);

    // --- Thread de análise (estado privado dela) ---
    struct AnalysisKey
    {
        pca::ExcitationSpec spec; int source = 0; int channel = 0; int role = 0;
        bool operator!= (const AnalysisKey& o) const
        { return spec != o.spec || source != o.source || channel != o.channel || role != o.role; }
    };
    AnalysisKey                       curKey;
    bool                              haveKey = false;
    std::unique_ptr<pca::Excitation>  excitation;
    pca::ResponseAnalyzer             respAn;
    pca::HarmonicAnalyzer             harmAn;
    pca::TransferAnalyzer             xferAn;
    int64_t                           nextFrame = 0;
    int64_t                           skipUntil = 0;
    double                            heldDelay = 0.0;
    int                               musicDelay = 0;
    uint32_t                          lastPublishMs = 0;
    std::vector<float>                frameWet, frameDry;
    std::vector<pca::cpx>             tmpH, tmpD, tmpW;
    std::vector<char>                 tmpValid;
    std::vector<double>               tmpCoh, tmpAmp;
    std::vector<float>                tmpIr;
    std::vector<pca::SweepPoint>      sweepPts;
    int                               sweepRunSeen = -1, sweepPtsKind = 0;

    mutable juce::CriticalSection resultsLock;
    AnalysisResults       results;

    Worker worker { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveAnalyzerProcessor)
};
