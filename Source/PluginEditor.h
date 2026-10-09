#pragma once

#include "PluginProcessor.h"

namespace pxcol
{
    const juce::Colour bg        { 0xff0a0a0a };
    const juce::Colour panel     { 0xff161616 };
    const juce::Colour outline   { 0xff4a4a4a };
    const juce::Colour grid      { 0xff262626 };
    const juce::Colour gridMajor { 0xff3a3a3a };
    const juce::Colour text      { 0xffd6d6d6 };
    const juce::Colour dim       { 0xff8a8a8a };
    const juce::Colour green     { 0xff86ef8f };
    const juce::Colour purple    { 0xffb26cc9 };
    const juce::Colour even      { 0xfff2a65a };
    const juce::Colour odd       { 0xff5ac8f2 };
}

class PxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PxLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// Área do gráfico (todas as vistas)
class GraphView : public juce::Component
{
public:
    explicit GraphView (CurveAnalyzerProcessor& p) : proc (p) {}
    void setResults (const AnalysisResults& r);
    void setTab (ViewTab t) { if (t != tab) { tab = t; repaint(); } }
    void tick();                                 // anima a curva até ao valor novo (30 Hz)
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override  { hover = e.position; hovering = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { hovering = false; repaint(); }

private:
    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    void drawFreqGrid (juce::Graphics&);
    void drawResponse (juce::Graphics&);
    void drawHarmonics (juce::Graphics&);
    void drawGenerator (juce::Graphics&);
    void drawWave (juce::Graphics&);
    void drawSpectrum (juce::Graphics&);
    void drawSweep (juce::Graphics&);
    void drawMessage (juce::Graphics&, const juce::String& title, const juce::String& sub);
    void drawReadout (juce::Graphics&, const juce::String& text);
    bool drawWaitingIfNeeded (juce::Graphics&);
    juce::Path curvePath (const pca::Curve& c, std::function<float (float)> yOf, bool phase) const;

    CurveAnalyzerProcessor& proc;
    AnalysisResults res;
    pca::Curve shown;          // curva no ecrã (anima até res.curve)
    ViewTab tab = ViewTab::Curve;
    juce::Point<float> hover;
    bool hovering = false;
};

// Ecrã de login (tapa o plugin até a senha ser aceite)
class LoginOverlay : public juce::Component
{
public:
    LoginOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    std::function<void()> onUnlocked;

private:
    void submit();
    juce::Rectangle<int> card() const;
    juce::TextEditor password;
    juce::ToggleButton remember;
    juce::TextButton enter { "ENTRAR" };
    juce::Label message;
};

class CurveAnalyzerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CurveAnalyzerEditor (CurveAnalyzerProcessor&);
    ~CurveAnalyzerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void exportCsv();
    void updateEnablement();
    void showPluginMenu();
    ViewTab effectiveTab() const;
    bool tabAvailable (ViewTab t) const;

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabeledCombo
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<ComboAtt> att;
    };

    void setupCombo (LabeledCombo&, const juce::String& text, const juce::String& paramId);
    void setupToggle (juce::TextButton&, std::unique_ptr<ButtonAtt>&, const juce::String& text, const juce::String& paramId);
    void setupBarSlider (juce::Slider&, juce::Label&, std::unique_ptr<SliderAtt>&, const juce::String& text,
                         const juce::String& paramId, const juce::String& suffix);
    void setParam (const juce::String& id, float plain);

    CurveAnalyzerProcessor& proc;
    PxLookAndFeel lnf;

    // Papel (botões grandes) e separadores de vista
    juce::TextButton roleGen { "GERADOR" }, roleAna { "ANALISADOR" }, roleHost { "HOST" };
    juce::TextButton tabCurve { "CURVA" }, tabWave { "ONDA" }, tabSpec { "ESPETRO" }, tabSweep { "VARRIMENTO" };

    LabeledCombo group, signal, fft, source, channel, averages, smoothing, range;
    juce::Slider level, sine, latency;
    juce::Label  levelLabel, sineLabel, latencyLabel;
    std::unique_ptr<SliderAtt> levelAtt, sineAtt, latencyAtt;
    juce::TextButton phaseBtn, syncBtn, freezeBtn, muteBtn;
    std::unique_ptr<ButtonAtt> phaseAtt, syncAtt, freezeAtt, muteAtt;
    juce::TextButton resetBtn { "RESET" }, refBtn { "+ REF" }, clearRefBtn { "LIMPAR REFS" }, exportBtn { "EXPORTAR" };

    // Contexto (barra das vistas)
    juce::TextButton loadBtn { "CARREGAR PLUGIN" }, openBtn { "ABRIR PLUGIN" }, removeBtn { "REMOVER" };
    juce::TextButton sweepLevelBtn { U8label ("VARRER NÍVEL") }, sweepFreqBtn { U8label ("VARRER FREQUÊNCIA") }, sweepStopBtn { "PARAR" };
    juce::String chipText;
    juce::Colour chipColour;

    GraphView graph;
    LoginOverlay login;
    juce::String status;
    AnalysisResults last;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<CurveAnalyzerProcessor::InstalledPlugin> pluginList;

    static juce::String U8label (const char* s) { return juce::String::fromUTF8 (s); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveAnalyzerEditor)
};
