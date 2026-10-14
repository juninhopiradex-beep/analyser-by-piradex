// EQMotion by Piradex — editor
#pragma once
#include "PluginProcessor.h"
#include "ui/EQGraph.h"
#include "ui/BandPanel.h"
#include "ui/MasterPanel.h"
#include "ui/SmartOpsPanel.h"
#include "ui/TopBar.h"
#include "ui/LoginOverlay.h"

class EQMotionEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EQMotionEditor (EQMotionProcessor&);
    ~EQMotionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int kBaseW = 1220, kBaseH = 760;

    // para testes / capturas
    EQGraph& getGraph() { return graph; }
    void openSmartOps (bool open);

private:
    void timerCallback() override;
    void select (int band);
    void updateLock();
    void setUiScale (float s);
    void showCallout (std::unique_ptr<juce::Component> c, juce::Component& anchor);

    EQMotionProcessor& proc;
    EQMLookAndFeel lnf;
    TopBar top;
    AnalyzerBar abar;
    EQGraph graph;
    BandStrip strip;
    BandInspector inspector;
    MasterPanel master;
    SmartOpsPanel smartPanel;
    LoginOverlay login;
    juce::TooltipWindow tooltips { this, 600 };
    bool smartOpen = false;

    struct Touch : juce::MouseListener
    {
        explicit Touch (StateManager& s) : st (s) {}
        void mouseDown (const juce::MouseEvent&) override { st.markUiTouched(); }
        void mouseUp (const juce::MouseEvent&) override { st.markUiTouched(); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override { st.markUiTouched(); }
        StateManager& st;
    } touch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQMotionEditor)
};
