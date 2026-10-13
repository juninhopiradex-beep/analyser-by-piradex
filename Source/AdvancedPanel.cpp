#include "AdvancedPanel.h"

namespace
{
    constexpr int kKnob = 84;
    constexpr int kKnobY = 62;

    struct Group { const char* title; int x, w; };
    const Group groups[] = {
        { "FREQUENCY", 24, 2 * kKnob + 10 },
        { "TIMING",    236, 2 * kKnob + 10 },
        { "SHAPE",     448, kKnob },
        { "MONITOR",   580, 176 },
    };
}

AdvancedPanel::AdvancedPanel (MixspaceProcessor& p, std::function<void()> onClose)
    : proc (p),
      stereo (*p.apvts.getParameter (ids::stereo), stereoModeNames, pal::sc),
      quality (*p.apvts.getParameter (ids::quality), qualityNames, pal::red)
{
    auto setup = [this] (Knob& k, const juce::String& id, const juce::String& label)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        k.att = std::make_unique<SA> (proc.apvts, id, k.slider);
        if (auto* prm = proc.apvts.getParameter (id))
            k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k.label = label;
        addAndMakeVisible (k.slider);
    };
    setup (lowCut, ids::lowCut, "LOW CUT");
    setup (highCut, ids::highCut, "HIGH CUT");
    setup (attack, ids::attack, "ATTACK");
    setup (release, ids::release, "RELEASE");
    setup (smooth, ids::smooth, "SMOOTH");

    addAndMakeVisible (stereo);
    addAndMakeVisible (quality);

    scListen.getProperties().set ("accent", (juce::int64) pal::sc.getARGB());
    scListen.setClickingTogglesState (true);
    scListenAtt = std::make_unique<BA> (proc.apvts, ids::scListen, scListen);
    addAndMakeVisible (scListen);

    closeButton.getProperties().set ("led", false);
    closeButton.setTooltip ("Fechar o painel");
    closeButton.onClick = std::move (onClose);
    addAndMakeVisible (closeButton);

    startTimerHz (4);
}

void AdvancedPanel::resized()
{
    closeButton.setBounds (getWidth() - 40, 8, 28, 22);

    lowCut.slider.setBounds (groups[0].x, kKnobY, kKnob, kKnob);
    highCut.slider.setBounds (groups[0].x + kKnob + 10, kKnobY, kKnob, kKnob);
    attack.slider.setBounds (groups[1].x, kKnobY, kKnob, kKnob);
    release.slider.setBounds (groups[1].x + kKnob + 10, kKnobY, kKnob, kKnob);
    smooth.slider.setBounds (groups[2].x, kKnobY, kKnob, kKnob);

    scListen.setBounds (groups[3].x + 6, kKnobY + 22, 150, 28);

    stereo.setBounds (24, 214, 388, 28);
    quality.setBounds (448, 214, 210, 28);
    latencyArea = { 668, 214, 92, 28 };
}

void AdvancedPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff12161a), 0.0f, 0.0f, juce::Colour (0xff0b0d10), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (pal::sc.withAlpha (0.25f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (pal::text);
    g.setFont (uiFont (13.0f, true, 0.3f));
    g.drawText ("ADVANCED", juce::Rectangle<float> (24.0f, 10.0f, 200.0f, 18.0f), juce::Justification::centredLeft);
    g.setColour (pal::grid.brighter (0.2f));
    g.drawHorizontalLine (36, 24.0f, b.getRight() - 24.0f);

    g.setFont (uiFont (10.0f, true, 0.2f));
    for (const auto& grp : groups)
    {
        g.setColour (pal::textFaint.brighter (0.3f));
        g.drawText (grp.title, juce::Rectangle<float> ((float) grp.x, 42.0f, (float) grp.w, 14.0f), juce::Justification::centredLeft);
    }

    g.setFont (uiFont (10.5f, true, 0.14f));
    g.setColour (pal::textDim);
    for (auto* k : { &lowCut, &highCut, &attack, &release, &smooth })
        g.drawText (k->label, k->slider.getBounds().toFloat().withY ((float) k->slider.getBottom() + 2.0f).withHeight (14.0f), juce::Justification::centred);

    g.setFont (uiFont (11.0f));
    g.setColour (pal::textFaint.brighter (0.2f));
    g.drawFittedText (juce::String::fromUTF8 ("Ouve só o sinal do sidechain\n(ex.: a voz) para conferir o envio."),
                      juce::Rectangle<int> (groups[3].x + 6, kKnobY + 58, 170, 32), juce::Justification::topLeft, 2);

    g.setFont (uiFont (10.0f, true, 0.2f));
    g.setColour (pal::textFaint.brighter (0.3f));
    g.drawText ("STEREO", juce::Rectangle<float> (24.0f, 196.0f, 200.0f, 14.0f), juce::Justification::centredLeft);
    g.drawText ("QUALITY", juce::Rectangle<float> (448.0f, 196.0f, 200.0f, 14.0f), juce::Justification::centredLeft);

    const int lat = proc.getCurrentLatency();
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    g.setColour (pal::textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText (juce::String::fromUTF8 ("Latência ") + juce::String (1000.0 * lat / sr, 1) + " ms", latencyArea.toFloat(), juce::Justification::centredRight);

    g.setFont (uiFont (11.0f));
    g.setColour (pal::textFaint);
    g.drawText (juce::String::fromUTF8 ("Dica: arraste os limites da zona activa no display · duplo clique repõe o valor"),
                juce::Rectangle<float> (24.0f, b.getBottom() - 26.0f, b.getWidth() - 48.0f, 16.0f), juce::Justification::centredLeft);
}
