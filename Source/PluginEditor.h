#pragma once

#include "PluginProcessor.h"
#include "MixspaceLookAndFeel.h"
#include "SpectrumDisplay.h"
#include "AdvancedPanel.h"

// Página principal, desenhada a 820 × 560 e escalada como um todo (80–200 %, Retina/HiDPI).
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 820, kH = 560;

    MainView (MixspaceProcessor&, std::function<void (float)> setScale);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint (headerRight); }
    void showAdvanced (bool show);
    void showMenu();

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    MixspaceProcessor& proc;
    std::function<void (float)> setScale;

    SpectrumDisplay display;
    AdvancedPanel advanced;

    juce::Slider space, range, focus;
    std::unique_ptr<SA> spaceAtt, rangeAtt, focusAtt;
    juce::TextButton deltaButton { "Delta" }, bypassButton { "Bypass" }, advancedButton { "Advanced" };
    std::unique_ptr<BA> deltaAtt, bypassAtt;

    juce::Rectangle<int> logoArea { 18, 8, 260, 36 }, headerRight { 560, 8, 242, 36 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

class MixspaceEditor : public juce::AudioProcessorEditor
{
public:
    explicit MixspaceEditor (MixspaceProcessor&);
    ~MixspaceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MixspaceProcessor& proc;
    MixspaceLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixspaceEditor)
};
