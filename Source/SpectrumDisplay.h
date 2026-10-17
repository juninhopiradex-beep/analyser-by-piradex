// Spectral Collision Display: Main (branco quente), Sidechain (ciano), redução (âmbar, a descer do topo),
// zona activa Low/High Cut (arrastável), masking meter e avisos de sidechain.
#pragma once
#include "PluginProcessor.h"
#include "MixspaceLookAndFeel.h"

class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay (MixspaceProcessor&);
    ~SpectrumDisplay() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForLevel (float db, float f) const;
    float yForReduction (float db) const;
    int edgeAt (juce::Point<float>) const;   // 0 = nenhum, 1 = low cut, 2 = high cut
    void paintGrid (juce::Graphics&, juce::Rectangle<float>);
    void paintZone (juce::Graphics&, juce::Rectangle<float>);
    void paintCurves (juce::Graphics&, juce::Rectangle<float>);
    void paintOverlay (juce::Graphics&, juce::Rectangle<float>);

    MixspaceProcessor& proc;
    juce::RangedAudioParameter* lowParam;
    juce::RangedAudioParameter* highParam;
    juce::RangedAudioParameter* rangeParam;

    static constexpr int kMax = SharedAnalysis::kMax;
    int nb = 0;
    std::array<float, kMax> freq {}, dMain {}, dSc {}, dRed {};
    float masking = 0.0f, avgRed = 0.0f;
    bool frozen = false;
    int hoverEdge = 0, dragEdge = 0;

    juce::TextButton freezeButton { "Freeze" };
    piradex::i18n::Binder lang;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
