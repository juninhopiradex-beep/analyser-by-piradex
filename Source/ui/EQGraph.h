// =============================================================================
//  EQMotion — gráfico principal
// =============================================================================
#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

class EQGraph : public juce::Component, private juce::Timer
{
public:
    explicit EQGraph (EQMotionProcessor&);
    ~EQGraph() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // seleção
    int  selectedBand() const { return primary; }
    void selectBand (int b, bool notify = true);
    juce::Array<int> selection() const { return selected; }
    std::function<void (int)> onSelectionChanged;

    void setSmartPanelOpen (bool open) { smartOpen = open; repaint(); }
    void setDbRange (float r);
    float dbRange() const { return dbRangeVal; }
    void resetZoom();
    bool isZoomed() const { return std::abs (std::log (viewLo / 10.0)) > 0.01 || std::abs (std::log (viewHi / 30000.0)) > 0.01; }

    // menu de contexto de uma banda (também usado pela barra de bandas)
    void showBandMenu (int band);
    void addBandAt (double freq, double gainDb);

private:
    void timerCallback() override;
    void rebuildCurves (bool force);
    void rebuildFreqs();

    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    float yForSpec (double db) const;
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> nodePos (int b) const;
    int nodeAt (juce::Point<float>) const;

    void drawGrid (juce::Graphics&, juce::Rectangle<float>);
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>);
    void drawInstances (juce::Graphics&, juce::Rectangle<float>);
    void drawSmart (juce::Graphics&, juce::Rectangle<float>);
    void drawBands (juce::Graphics&, juce::Rectangle<float>);
    void drawNodes (juce::Graphics&);
    void drawReadouts (juce::Graphics&, juce::Rectangle<float>);
    juce::Path curvePath (const std::vector<float>& db, juce::Rectangle<float>) const;
    juce::Path specPath (const std::vector<float>& db, const std::vector<double>& f, juce::Rectangle<float>, bool closed) const;

    void beginDrag (const juce::MouseEvent&);
    void endGestures();
    juce::RangedAudioParameter* bp (int b, ids::B which) const;
    void setBp (int b, ids::B which, float v);
    void notifySelection();

    EQMotionProcessor& proc;
    StateManager& st;

    // vista
    double viewLo = 10.0, viewHi = 30000.0;
    float dbRangeVal = 12.0f;
    std::vector<double> freqs;
    bool smartOpen = false;

    // dados (recalculados quando mudam)
    eqm::BandArray bands {};
    std::array<double, eqm::kMaxBands> live {};          // desvio dinâmico atual (dB)
    std::array<std::vector<float>, eqm::kMaxBands> bandStatic, bandLive, bandRange;
    std::vector<float> totalStereo, totalL, totalR, totalM, totalS, previewCurve, tmp;
    bool hasLR = false, hasMS = false;
    std::vector<float> previewTotal;
    juce::int64 lastHash = 0;
    SpectrumFrame spec;
    std::array<float, eqm::kMaxBands> halo {};

    // interação
    juce::Array<int> selected;
    int primary = -1, hover = -1;
    juce::Point<float> mouse { -1, -1 };
    bool mouseInside = false;
    enum class Drag { None, Node, Pan, RangeFrom, RangeTo } drag = Drag::None;
    juce::Point<float> dragStart;
    struct Start { int band; double freq, gain, q; };
    std::vector<Start> starts;
    std::vector<juce::RangedAudioParameter*> gestureParams;
    double panLo = 0, panHi = 0;
    piradex::i18n::Binder lang;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQGraph)
};
