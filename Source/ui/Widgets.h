// EQMotion — pequenos componentes reutilizáveis
#pragma once
#include "Theme.h"
#include "../Params.h"

// Botão rotativo com nome em cima e valor editável em baixo
class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, juce::Colour c = pal::accent, bool bipolar = false)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.getProperties().set ("colour", (juce::int64) c.getARGB());
        slider.getProperties().set ("bipolar", bipolar);
        slider.setVelocityModeParameters (0.6, 1, 0.08, true, juce::ModifierKeys::commandModifier);   // Cmd/Ctrl: ajuste fino
        slider.setTitle (name);
        addAndMakeVisible (slider);
        label.setText (name, juce::dontSendNotification);
        label.setFont (fonts::ui (10.5f, 1, 0.04f));
        label.setColour (juce::Label::textColourId, pal::textFaint.brighter (0.25f));
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }
    void setColour (juce::Colour c) { slider.getProperties().set ("colour", (juce::int64) c.getARGB()); slider.repaint(); }
    void attach (juce::RangedAudioParameter* p)
    {
        att.reset();
        if (p != nullptr)
        {
            att = std::make_unique<juce::SliderParameterAttachment> (*p, slider, nullptr);
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            slider.setTooltip (p->getName (64));
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 16);
    }
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

// Controlo segmentado ligado a um parâmetro de escolha (ou bool com 2 opções)
class Segmented : public juce::Component
{
public:
    Segmented (juce::StringArray opts, bool vertical = false) : options (std::move (opts)), vert (vertical)
    {
        for (int i = 0; i < options.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (options[i]));
            b->setClickingTogglesState (false);
            b->getProperties().set ("radius", 5.0f);
            b->onClick = [this, i] { select (i); };
            addAndMakeVisible (b);
        }
    }
    ~Segmented() override { if (param != nullptr) att.reset(); }

    void attach (juce::RangedAudioParameter* p)
    {
        param = p;
        att.reset();
        if (p == nullptr) return;
        att = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { show ((int) std::round (v)); }, nullptr);
        att->sendInitialUpdate();
    }
    void select (int i)
    {
        if (onBeforeChange) onBeforeChange();
        if (att) att->setValueAsCompleteGesture ((float) i);
        else show (i);
        if (onChange) onChange (i);
    }
    void show (int i)
    {
        current = i;
        for (int k = 0; k < buttons.size(); ++k) buttons[k]->setToggleState (k == i, juce::dontSendNotification);
    }
    int selected() const { return current; }
    void setAccent (juce::Colour c) { for (auto* b : buttons) b->getProperties().set ("accent", (juce::int64) c.getARGB()); repaint(); }
    void setTooltips (const juce::StringArray& t) { for (int k = 0; k < buttons.size() && k < t.size(); ++k) buttons[k]->setTooltip (t[k]); }

    void resized() override
    {
        auto r = getLocalBounds();
        const int n = buttons.size();
        for (int k = 0; k < n; ++k)
        {
            if (vert) { const int h = r.getHeight() / (n - k); buttons[k]->setBounds (r.removeFromTop (h).reduced (0, 1)); }
            else      { const int w = r.getWidth() / (n - k);  buttons[k]->setBounds (r.removeFromLeft (w).reduced (1, 0)); }
        }
    }
    std::function<void (int)> onChange;
    std::function<void()> onBeforeChange;

private:
    juce::StringArray options;
    bool vert;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    int current = 0;
};

// Barra de nível vertical (dois canais) com pico e indicador de clip
class LevelMeter : public juce::Component
{
public:
    LevelMeter (const juce::String& title) : name (title) {}
    void setLevels (float pl, float pr, float rl, float rr, bool clipNow)
    {
        auto upd = [] (float& v, float t, float fall) { v = t > v ? t : juce::jmax (t, v - fall); };
        const float pkL = juce::Decibels::gainToDecibels (pl, -90.0f), pkR = juce::Decibels::gainToDecibels (pr, -90.0f);
        upd (peakL, pkL, 1.2f); upd (peakR, pkR, 1.2f);
        upd (rmsL, juce::Decibels::gainToDecibels (rl, -90.0f), 0.8f); upd (rmsR, juce::Decibels::gainToDecibels (rr, -90.0f), 0.8f);
        if (pkL > holdL) { holdL = pkL; holdTicksL = 45; } else if (--holdTicksL < 0) holdL = juce::jmax (pkL, holdL - 0.6f);
        if (pkR > holdR) { holdR = pkR; holdTicksR = 45; } else if (--holdTicksR < 0) holdR = juce::jmax (pkR, holdR - 0.6f);
        clip = clip || clipNow;
        repaint();
    }
    void mouseDown (const juce::MouseEvent&) override { clip = false; holdL = holdR = -90; if (onReset) onReset(); repaint(); }
    std::function<void()> onReset;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (pal::textFaint.brighter (0.2f));
        g.setFont (fonts::ui (10.0f, 1, 0.05f));
        g.drawText (name, r.removeFromTop (14), juce::Justification::centred);
        auto led = r.removeFromTop (8).withSizeKeepingCentre (22, 5);
        g.setColour (clip ? pal::accent2 : pal::bg3);
        g.fillRoundedRectangle (led, 2.0f);
        r.removeFromTop (3);
        auto valueR = r.removeFromBottom (14);
        const float bw = (r.getWidth() - 3) * 0.5f;
        auto bar = [&] (juce::Rectangle<float> b, float pk, float rms, float hold)
        {
            g.setColour (pal::bg0); g.fillRoundedRectangle (b, 2.0f);
            auto yFor = [&] (float db) { return b.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f) * b.getHeight(); };
            const float y = yFor (pk), yr = yFor (rms);
            juce::ColourGradient cg (pal::ok.withAlpha (0.85f), 0, b.getBottom(), pal::accent2, 0, b.getY(), false);
            cg.addColour (0.82, pal::accent);
            g.setGradientFill (cg);
            g.fillRect (b.withTop (y));
            g.setColour (pal::text.withAlpha (0.35f));
            g.fillRect (b.withTop (yr).withHeight (1.5f));
            g.setColour (pal::text.withAlpha (0.8f));
            g.fillRect (b.withTop (yFor (hold)).withHeight (1.0f));
        };
        bar (r.withWidth (bw), peakL, rmsL, holdL);
        bar (r.withTrimmedLeft (bw + 3), peakR, rmsR, holdR);
        g.setColour (pal::textFaint.withAlpha (0.6f));
        for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
        {
            const float y = r.getBottom() - (db + 60.0f) / 66.0f * r.getHeight();
            g.drawHorizontalLine ((int) y, r.getX() - 2, r.getX());
        }
        g.setColour (clip ? pal::accent2 : pal::textDim);
        g.setFont (fonts::mono (10.0f));
        const float hv = juce::jmax (holdL, holdR);
        g.drawText (hv <= -89.0f ? "-inf" : juce::String (hv, 1), valueR, juce::Justification::centred);
    }

private:
    juce::String name;
    float peakL = -90, peakR = -90, rmsL = -90, rmsR = -90, holdL = -90, holdR = -90;
    int holdTicksL = 0, holdTicksR = 0;
    bool clip = false;
};

// Barra de valor horizontal com nome à esquerda e valor à direita (desenhados por cima)
class ValueBar : public juce::Slider
{
public:
    ValueBar (const juce::String& n, std::function<juce::String (double)> f) : label (n), fmt (std::move (f))
    {
        setSliderStyle (juce::Slider::LinearBar);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setTitle (n);
    }
    void paint (juce::Graphics& g) override
    {
        juce::Slider::paint (g);
        auto b = getLocalBounds().reduced (10, 0);
        g.setFont (fonts::ui (11.5f, 1)); g.setColour (isEnabled() ? pal::textDim : pal::textFaint);
        g.drawText (label, b, juce::Justification::centredLeft);
        g.setFont (fonts::mono (11.5f)); g.setColour (isEnabled() ? pal::text : pal::textFaint);
        g.drawText (fmt (getValue()), b, juce::Justification::centredRight);
    }
private:
    juce::String label;
    std::function<juce::String (double)> fmt;
};

// Título de secção com número (estilo "passo")
inline void drawStep (juce::Graphics& g, juce::Rectangle<int> r, int n, const juce::String& title, bool active)
{
    auto c = r.removeFromLeft (18).withSizeKeepingCentre (18, 18).toFloat();
    g.setColour (active ? pal::accent : pal::bg3);
    g.fillRoundedRectangle (c, 5.0f);
    g.setColour (active ? pal::bg0 : pal::textDim);
    g.setFont (fonts::ui (11.0f, 2));
    g.drawText (juce::String (n), c, juce::Justification::centred);
    g.setColour (active ? pal::text : pal::textDim);
    g.setFont (fonts::ui (13.0f, 2));
    g.drawText (title, r.withTrimmedLeft (8), juce::Justification::centredLeft);
}
