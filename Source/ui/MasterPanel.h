// EQMotion — coluna direita: medidores, saída, auto gain, character, de-harsh, latência
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

class MasterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MasterPanel (EQMotionProcessor& p) : proc (p)
    {
        addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
        inMeter.onReset = [this] { proc.inMeter.clip.store (false); };
        outMeter.onReset = [this] { proc.outMeter.clip.store (false); };

        output.attach (proc.apvts.getParameter (ids::output));
        scale.attach (proc.apvts.getParameter (ids::gainScale));
        addAndMakeVisible (output); addAndMakeVisible (scale);
        scale.tipFromName = false;
        autoAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (ids::autoGain), autoGain);
        addAndMakeVisible (autoGain);

        character.attach (proc.apvts.getParameter (ids::character));
        addAndMakeVisible (character);

        deharshAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (ids::deharsh), deharsh);
        addAndMakeVisible (deharsh);
        dhAmount.attach (proc.apvts.getParameter (ids::deharshAmt));
        dhAtk.attach (proc.apvts.getParameter (ids::deharshAtk));
        dhRel.attach (proc.apvts.getParameter (ids::deharshRel));
        for (auto* k : { &dhAmount, &dhAtk, &dhRel }) addAndMakeVisible (k);

        lang.bind ([this]
        {
            output.setText (tr ("SAÍDA", "OUTPUT"));
            scale.setText (tr ("ESCALA", "SCALE"));
            scale.slider.setTooltip (tr ("Escala todos os ganhos das bandas (0–200 %)", "Scales all band gains (0–200 %)"));
            autoGain.setTooltip (tr ("Compara bypass, A/B e presets com a mesma loudness — ganha a melhor definição, não a mais alta",
                                     "Compares bypass, A/B and presets at the same loudness — the best setting wins, not the loudest"));
            character.setLabels ({ tr ("Limpo", "Clean"), tr ("Subtil", "Subtle"), tr ("Quente", "Warm") });
            character.setTooltips ({ tr ("Transparente: sem harmónicos adicionados", "Transparent: no added harmonics"),
                                     tr ("Cor ligeira: harmónicos de baixo nível", "Light colour: low-level harmonics"),
                                     tr ("Mais calor: harmónicos e ligeira inclinação tonal", "More warmth: harmonics and a slight tonal tilt") });
            deharsh.setTooltip (tr ("Doma a dureza 2–8 kHz só quando ela sobe acima do equilíbrio do próprio sinal",
                                    "Tames 2–8 kHz harshness only when it rises above the signal's own balance"));
            dhAmount.setText (tr ("QUANT.", "AMOUNT"));
            dhAtk.setText (tr ("ATQ", "ATK"));
            dhRel.setText (tr ("LIB", "REL"));
        });
        lang.onChange = [this] { repaint(); };
        startTimerHz (30);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 10);
        auto meters = r.removeFromTop (juce::jmax (150, r.getHeight() - 430));
        inMeter.setBounds (meters.removeFromLeft (meters.getWidth() / 2).reduced (3, 0));
        outMeter.setBounds (meters.reduced (3, 0));
        r.removeFromTop (8);
        auto row = r.removeFromTop (84);
        output.setBounds (row.removeFromLeft (row.getWidth() / 2));
        scale.setBounds (row);
        r.removeFromTop (4);
        autoGain.setBounds (r.removeFromTop (22));
        agArea = r.removeFromTop (14);
        r.removeFromTop (10);
        charTitle = r.removeFromTop (16);
        character.setBounds (r.removeFromTop (78));
        charStatus = r.removeFromTop (16);
        r.removeFromTop (8);
        deharsh.setBounds (r.removeFromTop (22));
        dhStatus = r.removeFromTop (14);
        auto k = r.removeFromTop (74);
        const int w3 = k.getWidth() / 3;
        dhAmount.setBounds (k.removeFromLeft (w3));
        dhAtk.setBounds (k.removeFromLeft (w3));
        dhRel.setBounds (k);
        latArea = getLocalBounds().removeFromBottom (36).reduced (8, 4);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (pal::bg1);
        g.fillRect (getLocalBounds());
        g.setColour (pal::line);
        g.drawVerticalLine (0, 0, (float) getHeight());
        g.setFont (fonts::ui (10.5f, 1, 0.05f));
        g.setColour (pal::textFaint.brighter (0.25f));
        g.drawText (tr ("CARÁCTER", "CHARACTER"), charTitle, juce::Justification::centredLeft);
        const int ch = juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue (ids::character)->load());
        g.setFont (fonts::ui (10.5f));
        g.setColour (ch == 0 ? pal::textDim : pal::accent);
        g.drawText (ch == 0 ? tr ("Sem harmónicos adicionados", "No added harmonics") : ch == 1 ? tr ("Harmónicos subtis", "Subtle harmonics") : tr ("Harmónicos + calor", "Harmonics + warmth"),
                    charStatus, juce::Justification::centredLeft);
        g.setColour (pal::textDim);
        g.setFont (fonts::mono (10.0f));
        if (proc.apvts.getRawParameterValue (ids::autoGain)->load() > 0.5f)
            g.drawText (tr ("comp. ", "comp. ") + juce::String (proc.autoGainDb(), 1) + " dB", agArea, juce::Justification::centredLeft);
        if (proc.apvts.getRawParameterValue (ids::deharsh)->load() > 0.5f)
            g.drawText (tr ("redução ", "reduction ") + juce::String (proc.deharshDb(), 1) + " dB", dhStatus, juce::Justification::centredLeft);

        const int lat = proc.currentLatency();
        g.setColour (pal::textFaint);
        g.setFont (fonts::mono (10.0f));
        const double ms = lat * 1000.0 / juce::jmax (1.0, proc.currentSampleRate());
        g.drawFittedText (tr ("Latência\n", "Latency\n") + juce::String (lat) + tr (" amostras · ", " samples · ") + juce::String (ms, 1) + " ms",
                          latArea, juce::Justification::centredLeft, 2);
    }

private:
    void timerCallback() override
    {
        auto take = [] (std::atomic<float>& a) { return a.exchange (0.0f); };
        inMeter.setLevels (take (proc.inMeter.peakL), take (proc.inMeter.peakR), proc.inMeter.rmsL.load(), proc.inMeter.rmsR.load(), proc.inMeter.clip.load());
        outMeter.setLevels (take (proc.outMeter.peakL), take (proc.outMeter.peakR), proc.outMeter.rmsL.load(), proc.outMeter.rmsR.load(), proc.outMeter.clip.load());
        if (++ticks % 6 == 0) repaint (0, agArea.getY() - 2, getWidth(), getHeight() - agArea.getY() + 2);
    }

    EQMotionProcessor& proc;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    Knob output { juce::String::fromUTF8 ("SAÍDA"), pal::accent, true }, scale { "ESCALA" };
    juce::ToggleButton autoGain { "Auto gain" }, deharsh { "De-harsh" };
    std::unique_ptr<juce::ButtonParameterAttachment> autoAtt, deharshAtt;
    Segmented character { names::characters(), true };
    Knob dhAmount { "AMOUNT" }, dhAtk { "ATK" }, dhRel { "REL" };
    juce::Rectangle<int> charTitle, charStatus, agArea, dhStatus, latArea;
    int ticks = 0;
    piradex::i18n::Binder lang;
};
