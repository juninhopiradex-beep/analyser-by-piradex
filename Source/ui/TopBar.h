// EQMotion — barra superior e barra do analisador
#pragma once
#include "Popovers.h"

class TopBar : public juce::Component, private juce::Timer
{
public:
    explicit TopBar (EQMotionProcessor& p) : proc (p)
    {
        auto& st = proc.state;
        presetName.getProperties().set ("radius", 6.0f);
        presetName.onClick = [this] { if (onPresetBrowser) onPresetBrowser (presetName); };
        prev.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        undo.onClick = [&st] { st.undo(); };
        redo.onClick = [&st] { st.redo(); };
        slotA.onClick = [&st] { st.switchSlot (0); };
        slotB.onClick = [&st] { st.switchSlot (1); };
        copyAB.onClick = [&st] { st.copyActiveToOther(); };
        for (auto* b : { &slotA, &slotB }) { b->getProperties().set ("radius", 5.0f); }

        procMode.addItemList (names::procModes(), 1);
        procModeAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.apvts.getParameter (ids::procMode), procMode);

        phase.attach (proc.apvts.getParameter (ids::phase));

        smart.setClickingTogglesState (false);
        smart.onClick = [this] { if (onSmartToggle) onSmartToggle(); };
        instances.onClick = [this] { if (onInstances) onInstances (instances); };
        settings.onClick = [this] { if (onSettings) onSettings (settings); };
        power.onColour = pal::accent;
        power.onClick = [this]
        {
            auto* b = proc.apvts.getParameter (ids::bypass);
            b->beginChangeGesture(); b->setValueNotifyingHost (b->getValue() > 0.5f ? 0.0f : 1.0f); b->endChangeGesture();
        };
        for (auto* c : std::initializer_list<juce::Component*> { &prev, &presetName, &next, &undo, &redo, &slotA, &slotB, &copyAB, &procMode, &phase, &smart, &instances, &settings, &power })
            addAndMakeVisible (c);
        lang.bind ([this]
        {
            presetName.setTooltip (tr ("Navegador de presets", "Preset browser"));
            prev.setTooltip (tr ("Preset anterior", "Previous preset")); next.setTooltip (tr ("Preset seguinte", "Next preset"));
            undo.setTooltip (tr ("Desfazer (Cmd/Ctrl+Z)", "Undo (Cmd/Ctrl+Z)")); redo.setTooltip (tr ("Refazer (Cmd/Ctrl+Shift+Z)", "Redo (Cmd/Ctrl+Shift+Z)"));
            slotA.setTooltip (tr ("Definição A", "Setting A")); slotB.setTooltip (tr ("Definição B", "Setting B"));
            copyAB.setTooltip (tr ("Copiar a definição atual para a outra (A → B ou B → A)", "Copy the current setting to the other (A → B or B → A)"));
            const int sel = procMode.getSelectedId();
            procMode.changeItemText (1, tr ("Estéreo", "Stereo"));
            procMode.changeItemText (2, tr ("Esq./Dir.", "Left/Right"));
            procMode.changeItemText (3, "Mid/Side");
            if (sel > 0) procMode.setSelectedId (sel, juce::dontSendNotification);
            procMode.setTooltip (tr ("Modo de canais: o analisador divide-se para condizer; novas bandas nascem em Mid (M/S) ou Esquerda (L/R)",
                                     "Channel mode: the analyzer splits to match; new bands start on Mid (M/S) or Left (L/R)"));
            phase.setTooltips ({ tr ("Zero Latency: fase mínima, 0 amostras — ideal para gravar e tocar ao vivo",
                                     "Zero Latency: minimum phase, 0 samples — ideal for tracking and live playing"),
                                 tr ("Natural Phase: resposta igual à analógica até 20 kHz (sem compressão perto de Nyquist), poucas amostras de latência",
                                     "Natural Phase: analog-matched response up to 20 kHz (no cramping near Nyquist), a few samples of latency"),
                                 tr ("Linear Phase: sem distorção de fase; tem pré-ringing e latência (qualidade nas definições)",
                                     "Linear Phase: no phase distortion; has pre-ringing and latency (quality in settings)") });
            smart.setTooltip (tr ("Smart Ops: análise que propõe bandas, pré-visualizadas antes de aplicar", "Smart Ops: analysis that proposes bands, previewed before applying"));
            settings.setTooltip (tr ("Definições (idioma, analisador, janela)", "Settings (language, analyzer, window)"));
            power.setTooltip (tr ("Ligar / bypass (comparação alinhada em latência)", "On / bypass (latency-aligned comparison)"));
        });
        startTimerHz (8);
        timerCallback();
    }

    std::function<void (juce::Component&)> onPresetBrowser, onInstances, onSettings;
    std::function<void()> onSmartToggle;
    void setSmartOpen (bool o) { smart.setToggleState (o, juce::dontSendNotification); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 9);
        compact = getWidth() < 1180;
        logoArea = r.removeFromLeft (compact ? 124 : 176);
        power.setBounds (r.removeFromRight (32)); r.removeFromRight (6);
        settings.setBounds (r.removeFromRight (32)); r.removeFromRight (4);
        instances.setBounds (r.removeFromRight (32)); r.removeFromRight (10);
        smart.setBounds (r.removeFromRight (compact ? 96 : 112)); r.removeFromRight (compact ? 8 : 12);
        phase.setBounds (r.removeFromRight (132)); r.removeFromRight (8);
        procMode.setBounds (r.removeFromRight (106)); r.removeFromRight (14);
        copyAB.setBounds (r.removeFromRight (30)); r.removeFromRight (2);
        slotB.setBounds (r.removeFromRight (30)); r.removeFromRight (2);
        slotA.setBounds (r.removeFromRight (30)); r.removeFromRight (10);
        redo.setBounds (r.removeFromRight (30)); r.removeFromRight (2);
        undo.setBounds (r.removeFromRight (30)); r.removeFromRight (12);
        prev.setBounds (r.removeFromLeft (26)); next.setBounds (r.removeFromRight (26));
        presetName.setBounds (r.reduced (4, 0));
    }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (pal::bg2, 0, 0, pal::bg1, 0, (float) getHeight(), false));
        g.fillRect (getLocalBounds());
        g.setColour (pal::line);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());

        // marca: onda em movimento dentro de um anel âmbar
        auto logo = logoArea;
        auto m = logo.removeFromLeft (30).withSizeKeepingCentre (28, 28).toFloat();
        g.setColour (pal::accent.withAlpha (0.16f)); g.fillEllipse (m);
        g.setColour (pal::accent); g.drawEllipse (m.reduced (1), 1.6f);
        juce::Path w;
        for (int i = 0; i <= 24; ++i)
        {
            const float t = i / 24.0f;
            const float x = m.getX() + 6 + t * (m.getWidth() - 12);
            const float y = m.getCentreY() - std::sin (t * juce::MathConstants<float>::twoPi * 1.2f) * 5.0f * std::sin (t * juce::MathConstants<float>::pi);
            if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
        }
        g.strokePath (w, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        auto t = logo.withTrimmedLeft (8);
        g.setFont (fonts::ui (19.0f, 2, -0.01f));
        const int wEQ = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "EQ")) + 1;
        const int wM = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "Motion")) + 6;
        g.setColour (pal::text);
        g.drawText ("EQ", t.removeFromLeft (wEQ), juce::Justification::centredLeft);
        g.setColour (pal::accent);
        g.drawText ("Motion", t.removeFromLeft (wM), juce::Justification::centredLeft);
        if (! compact)
        {
            g.setColour (pal::textFaint);
            g.setFont (fonts::ui (10.5f, 1));
            g.drawText ("by Piradex", t.withTrimmedTop (6), juce::Justification::centredLeft);
        }
    }

private:
    void timerCallback() override
    {
        auto& st = proc.state;
        auto name = presets::displayName (st.presetName());
        if (st.isModifiedSincePreset() && st.presetName() != "Default") name << " *";
        if (presetName.getButtonText() != name) presetName.setButtonText (name);
        undo.setEnabled (st.canUndo()); redo.setEnabled (st.canRedo());
        slotA.setToggleState (st.activeSlot() == 0, juce::dontSendNotification);
        slotB.setToggleState (st.activeSlot() == 1, juce::dontSendNotification);
        power.setToggleState (proc.apvts.getRawParameterValue (ids::bypass)->load() < 0.5f, juce::dontSendNotification);
        int others = 0;
        for (auto& i : InstanceRegistry::get().all()) if (i != proc.shared) ++others;
        instances.setEnabled (true);
        instances.setTooltip (tr ("Instâncias do EQMotion neste projeto (", "EQMotion instances in this project (") + juce::String (others) + tr (" outras)", " others)"));
    }

    void step (int dir)
    {
        // percorre fábrica + utilizador
        juce::StringArray namesAll;
        const auto& f = presets::factory();
        for (auto& p : f) namesAll.add (p.name);
        auto users = proc.presetManager.userPresets();
        for (auto& u : users) namesAll.add (u.getFileNameWithoutExtension());
        int idx = namesAll.indexOf (proc.state.presetName());
        idx = (idx + dir + namesAll.size()) % namesAll.size();
        if (idx < (int) f.size()) proc.state.loadFactory (f[(size_t) idx]);
        else proc.state.loadUser (users[idx - (int) f.size()]);
    }

    EQMotionProcessor& proc;
    juce::Rectangle<int> logoArea;
    bool compact = false;
    IconButton prev { "Anterior", [] { juce::Path p; p.startNewSubPath (0.62f, 0.25f); p.lineTo (0.38f, 0.5f); p.lineTo (0.62f, 0.75f); return p; }() };
    IconButton next { "Seguinte", [] { juce::Path p; p.startNewSubPath (0.38f, 0.25f); p.lineTo (0.62f, 0.5f); p.lineTo (0.38f, 0.75f); return p; }() };
    juce::TextButton presetName { "Default" };
    IconButton undo { "Desfazer", icons::undo() }, redo { "Refazer", icons::redo() }, copyAB { "Copiar A/B", icons::copy() };
    juce::TextButton slotA { "A" }, slotB { "B" };
    juce::ComboBox procMode;
    std::unique_ptr<juce::ComboBoxParameterAttachment> procModeAtt;
    Segmented phase { { "ZL", "NP", "LP" } };
    juce::TextButton smart { "Smart Ops" };
    IconButton instances { juce::String::fromUTF8 ("Instâncias"), icons::layers() }, settings { juce::String::fromUTF8 ("Definições"), icons::gear() }, power { "Bypass", icons::power() };
    piradex::i18n::Binder lang;
};

// Barra fina por cima do gráfico: o que o analisador mostra, Congelar, Delta
class AnalyzerBar : public juce::Component
{
public:
    explicit AnalyzerBar (EQMotionProcessor& p) : proc (p)
    {
        auto& az = proc.analyzer;
        setup (pre, "Entrada", az.showPre, "Espetro antes do EQ");
        setup (post, juce::String::fromUTF8 ("Saída"), az.showPost, juce::String::fromUTF8 ("Espetro depois do EQ"));
        setup (sc, "Sidechain", az.showSc, "Espetro da entrada sidechain");
        setup (freeze, "Congelar", az.freeze, "Congela o espetro para comparar");
        deltaAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (ids::delta), delta);
        delta.setClickingTogglesState (true);
        delta.getProperties().set ("accent", (juce::int64) pal::accent2.getARGB());
        addAndMakeVisible (delta);
        zoom.getProperties().set ("flat", true);
        zoom.onClick = [this] { if (onResetZoom) onResetZoom(); };
        addAndMakeVisible (zoom);
        // o nome interno (az_Entrada…) guarda o estado na sessão: só o texto visível muda com o idioma
        lang.bind ([this]
        {
            pre.setButtonText (tr ("Entrada", "Input"));       pre.setTooltip (tr ("Espetro antes do EQ", "Spectrum before the EQ"));
            post.setButtonText (tr ("Saída", "Output"));       post.setTooltip (tr ("Espetro depois do EQ", "Spectrum after the EQ"));
            sc.setTooltip (tr ("Espetro da entrada sidechain", "Sidechain input spectrum"));
            freeze.setButtonText (tr ("Congelar", "Freeze"));  freeze.setTooltip (tr ("Congela o espetro para comparar", "Freezes the spectrum to compare"));
            delta.setTooltip (tr ("Delta: ouvir só o que o EQ muda (saída menos o original alinhado em latência)",
                                  "Delta: hear only what the EQ changes (output minus the latency-aligned original)"));
            zoom.setButtonText (tr ("Repor zoom", "Reset zoom"));
            zoom.setTooltip (tr ("Repor zoom (Cmd/Ctrl + roda para aproximar, Alt + arrastar para deslocar)",
                                 "Reset zoom (Cmd/Ctrl + wheel to zoom in, Alt + drag to pan)"));
        });
    }
    std::function<void()> onResetZoom;
    void setZoomed (bool z) { zoom.setVisible (z); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4);
        for (auto* b : { &pre, &post, &sc }) { b->setBounds (r.removeFromLeft (84)); r.removeFromLeft (4); }
        r.removeFromLeft (10);
        freeze.setBounds (r.removeFromLeft (84)); r.removeFromLeft (4);
        delta.setBounds (r.removeFromLeft (70));
        zoom.setBounds (r.removeFromRight (110));
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (pal::bg1);
        g.fillRect (getLocalBounds());
        g.setColour (pal::lineSoft);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    }

private:
    void setup (juce::TextButton& b, const juce::String& text, std::atomic<bool>& flag, const juce::String& tip)
    {
        b.setButtonText (text);
        b.setClickingTogglesState (true);
        b.setToggleState (flag.load(), juce::dontSendNotification);
        b.setTooltip (tip);
        b.onClick = [&b, &flag, this] { flag.store (b.getToggleState()); proc.uiState.setProperty (b.getName(), b.getToggleState(), nullptr); };
        b.setName ("az_" + text);
        addAndMakeVisible (b);
    }
    EQMotionProcessor& proc;
    juce::TextButton pre, post, sc, freeze, delta { "Delta" }, zoom { "Repor zoom" };
    std::unique_ptr<juce::ButtonParameterAttachment> deltaAtt;
    piradex::i18n::Binder lang;
};
