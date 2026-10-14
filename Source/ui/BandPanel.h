// EQMotion — barra de bandas e inspetor da banda selecionada
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

class BandStrip : public juce::Component, private juce::Timer
{
public:
    explicit BandStrip (EQMotionProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (int b) { selected = b; repaint(); }
    std::function<void (int)> onSelect;
    std::function<void (int)> onContextMenu;
    std::function<void()> onAdd;

private:
    void timerCallback() override;
    juce::Rectangle<int> chipBounds (int slotIndex) const;
    EQMotionProcessor& proc;
    juce::TextButton add { "+" }, clear { "Limpar bandas" };
    std::vector<int> visible;
    int selected = -1;
};

class BandInspector : public juce::Component, private juce::Timer
{
public:
    explicit BandInspector (EQMotionProcessor&);
    void setBand (int b);
    int band() const { return current; }
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onBandRemoved;

private:
    void timerCallback() override;
    void refreshEnablement();
    juce::RangedAudioParameter* bp (ids::B w) const { return proc.apvts.getParameter (ids::band (current, w)); }

    EQMotionProcessor& proc;
    int current = -1;
    eqm::BandParams shown;

    juce::OwnedArray<juce::Button> typeButtons;
    juce::ComboBox slope, place;
    Knob freq { juce::String::fromUTF8 ("FREQUÊNCIA") }, gain { "GANHO", pal::accent, true }, q { "Q" };
    juce::TextButton listen { "Listen" }, bypass { "Bypass" }, reset { "Repor" }, remove { "Remover" };
    juce::ToggleButton dynOn { juce::String::fromUTF8 ("Dinâmica") };
    Segmented dir { { "Acima", "Abaixo" } }, src { { "Pista", "Sidechain" } }, det { { "Peak", "RMS" } };
    Knob thr { "LIMIAR" }, range { "RANGE", pal::accent, true }, ratio { "RATIO" }, atk { "ATAQUE" }, hold { "HOLD" }, rel { "RELEASE" }, knee { "JOELHO" };
    std::unique_ptr<juce::ComboBoxParameterAttachment> slopeAtt, placeAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> dynAtt;
    juce::Rectangle<int> meterArea, headerArea, dynArea;
    float liveGr = 0, liveLevel = -100;
};
