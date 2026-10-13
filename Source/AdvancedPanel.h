// Painel ADVANCED: Low/High Cut, Attack/Release, Smooth, modo estéreo, SC Listen e qualidade.
// Abre por cima do display — a página principal nunca ganha mais knobs.
#pragma once
#include "PluginProcessor.h"
#include "MixspaceLookAndFeel.h"

// Botões segmentados ligados a um parâmetro de escolha (Stereo/Mid/Side/L/R, Live/Mix/Master)
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::RangedAudioParameter& p, const juce::StringArray& labels, juce::Colour accent)
        : attachment (p, [this] (float v) { update (juce::roundToInt (v)); }, nullptr)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->getProperties().set ("segment", true);
            b->getProperties().set ("accent", (juce::int64) accent.getARGB());
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        const int n = buttons.size(), gap = 4;
        const float w = (float) (getWidth() - gap * (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds (juce::roundToInt ((float) i * (w + (float) gap)), 0, juce::roundToInt (w), getHeight());
    }

private:
    void update (int idx)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    }

    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

class AdvancedPanel : public juce::Component, private juce::Timer
{
public:
    AdvancedPanel (MixspaceProcessor&, std::function<void()> onClose);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint (latencyArea); }

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<SA> att;
        juce::String label;
    };

    MixspaceProcessor& proc;
    Knob lowCut, highCut, attack, release, smooth;
    ChoiceButtons stereo, quality;
    juce::TextButton scListen { "SC Listen" }, closeButton { juce::String::fromUTF8 ("\xc3\x97") };
    std::unique_ptr<BA> scListenAtt;
    juce::Rectangle<int> latencyArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
};
