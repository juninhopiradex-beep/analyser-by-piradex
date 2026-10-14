// EQMotion — janelas flutuantes: navegador de presets, instâncias, definições
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

// ============================================================================= presets
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (EQMotionProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Procurar presets…"), pal::textFaint);
        search.setFont (fonts::ui (13.0f));
        search.onTextChange = [this] { rebuild(); };
        addAndMakeVisible (search);
        folder.addItem ("Todas as pastas", 1);
        folder.addItem ("Favoritos", 2);
        folder.addItem ("Os meus presets", 3);
        juce::StringArray seen;
        for (auto& f : presets::factory()) if (! seen.contains (f.folder)) seen.add (f.folder);
        for (int i = 0; i < seen.size(); ++i) folder.addItem (seen[i], 10 + i);
        folders = seen;
        folder.setSelectedId (1, juce::dontSendNotification);
        folder.onChange = [this] { rebuild(); };
        addAndMakeVisible (folder);
        list.setModel (this);
        list.setRowHeight (40);
        list.setColour (juce::ListBox::backgroundColourId, pal::bg1);
        addAndMakeVisible (list);
        save.onClick = [this] { saveAs(); };
        openFolder.getProperties().set ("flat", true);
        openFolder.onClick = [this] { proc.presetManager.userFolder().startAsProcess(); };
        addAndMakeVisible (save); addAndMakeVisible (openFolder);
        setSize (440, 480);
        rebuild();
    }

    std::function<void()> onLoaded;

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (28);
        folder.setBounds (top.removeFromRight (150)); top.removeFromRight (8);
        search.setBounds (top);
        r.removeFromTop (8);
        auto bottom = r.removeFromBottom (30);
        save.setBounds (bottom.removeFromLeft (140)); bottom.removeFromLeft (8);
        openFolder.setBounds (bottom.removeFromLeft (160));
        r.removeFromBottom (8);
        list.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (pal::bg1); }

private:
    struct Item { bool user; int factoryIndex; juce::File file; juce::String name, folder, desc, key; };

    void rebuild()
    {
        items.clear();
        const auto q = search.getText().trim().toLowerCase();
        const int fid = folder.getSelectedId();
        auto keep = [&] (const Item& it)
        {
            if (q.isNotEmpty() && ! (it.name.toLowerCase().contains (q) || it.folder.toLowerCase().contains (q) || it.desc.toLowerCase().contains (q))) return false;
            if (fid == 2) return proc.presetManager.isFavourite (it.key);
            if (fid == 3) return it.user;
            if (fid >= 10) return ! it.user && it.folder == folders[fid - 10];
            return true;
        };
        const auto& f = presets::factory();
        for (int i = 0; i < (int) f.size(); ++i)
        {
            Item it { false, i, {}, f[(size_t) i].name, f[(size_t) i].folder, f[(size_t) i].description, PresetManager::factoryKey (f[(size_t) i]) };
            if (keep (it)) items.push_back (it);
        }
        for (auto& file : proc.presetManager.userPresets())
        {
            Item it { true, -1, file, file.getFileNameWithoutExtension(), "Os meus presets", "", PresetManager::userKey (file) };
            if (keep (it)) items.push_back (it);
        }
        list.updateContent();
        list.repaint();
    }

    int getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selectedRow) override
    {
        if (row < 0 || row >= (int) items.size()) return;
        const auto& it = items[(size_t) row];
        const bool current = it.name == proc.state.presetName();
        g.setColour (selectedRow ? pal::bg3 : row % 2 ? pal::bg1 : pal::bg1.brighter (0.015f));
        g.fillRect (0, 0, w, h);
        auto star = icons::star (true);
        star.applyTransform (juce::AffineTransform::scale (16, 16).translated (10, (h - 16) / 2.0f));
        const bool fav = proc.presetManager.isFavourite (it.key);
        g.setColour (fav ? pal::accent : pal::textFaint.withAlpha (0.6f));
        if (fav) g.fillPath (star); else g.strokePath (star, juce::PathStrokeType (1.2f));
        g.setColour (current ? pal::accent : pal::text);
        g.setFont (fonts::ui (13.5f, 2));
        g.drawText (it.name, 36, 4, w - 150, 18, juce::Justification::centredLeft);
        g.setColour (pal::textFaint);
        g.setFont (fonts::ui (11.0f, 1));
        g.drawText (it.folder, w - 130, 4, 120, 18, juce::Justification::centredRight);
        g.setColour (pal::textDim);
        g.setFont (fonts::ui (11.5f));
        g.drawText (it.desc, 36, 21, w - 46, 16, juce::Justification::centredLeft);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= (int) items.size()) return;
        const auto& it = items[(size_t) row];
        if (e.x < 32) { proc.presetManager.setFavourite (it.key, ! proc.presetManager.isFavourite (it.key)); rebuild(); return; }
        if (it.user) proc.state.loadUser (it.file); else proc.state.loadFactory (presets::factory()[(size_t) it.factoryIndex]);
        list.repaint();
        if (onLoaded) onLoaded();
    }
    void saveAs()
    {
        auto* w = new juce::AlertWindow ("Guardar preset", juce::String::fromUTF8 ("Nome do preset (fica na pasta Documentos/Piradex/EQMotion/Presets):"), juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.state.presetName() == "Default" ? juce::String() : proc.state.presetName());
        w->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
        {
            if (r == 1 && safe != nullptr)
            {
                safe->proc.state.saveUser (w->getTextEditorContents ("name"));
                safe->rebuild();
            }
        }), true);
    }

    EQMotionProcessor& proc;
    juce::TextEditor search;
    juce::ComboBox folder;
    juce::StringArray folders;
    juce::ListBox list;
    juce::TextButton save { juce::String::fromUTF8 ("Guardar como…") }, openFolder { "Abrir pasta de presets" };
    std::vector<Item> items;
};

// ============================================================================= instâncias
class InstancesPanel : public juce::Component
{
public:
    explicit InstancesPanel (EQMotionProcessor& p) : proc (p)
    {
        nameEditor.setFont (fonts::ui (13.0f));
        { const juce::SpinLock::ScopedLockType lk (proc.shared->lock); nameEditor.setText (proc.shared->name, false); }
        nameEditor.onTextChange = [this] { const juce::SpinLock::ScopedLockType lk (proc.shared->lock); proc.shared->name = nameEditor.getText(); };
        addAndMakeVisible (nameEditor);
        for (auto& inst : InstanceRegistry::get().all())
        {
            if (inst == proc.shared) continue;
            juce::String n; { const juce::SpinLock::ScopedLockType lk (inst->lock); n = inst->name + (inst->bypassed ? "  (bypass)" : ""); }
            auto* t = toggles.add (new juce::ToggleButton (n));
            const auto shown = juce::StringArray::fromTokens (proc.uiState.getProperty ("instancesShown").toString(), ",", "");
            t->setToggleState (shown.contains (juce::String (inst->id)), juce::dontSendNotification);
            const int id = inst->id;
            t->onClick = [this, id, t]
            {
                auto s = juce::StringArray::fromTokens (proc.uiState.getProperty ("instancesShown").toString(), ",", "");
                s.removeString (juce::String (id));
                if (t->getToggleState()) s.add (juce::String (id));
                proc.uiState.setProperty ("instancesShown", s.joinIntoString (","), nullptr);
            };
            addAndMakeVisible (t);
        }
        setSize (320, 96 + juce::jmax (1, toggles.size()) * 28);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        r.removeFromTop (18);
        nameEditor.setBounds (r.removeFromTop (26));
        r.removeFromTop (26);
        for (auto* t : toggles) t->setBounds (r.removeFromTop (28));
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (pal::bg1);
        g.setColour (pal::textFaint.brighter (0.2f));
        g.setFont (fonts::ui (10.5f, 1, 0.05f));
        g.drawText (juce::String::fromUTF8 ("NOME DESTA INSTÂNCIA"), 14, 10, 300, 16, juce::Justification::centredLeft);
        g.drawText (juce::String::fromUTF8 ("SOBREPOR NO GRÁFICO (CURVA + ESPETRO)"), 14, 64, 300, 16, juce::Justification::centredLeft);
        if (toggles.isEmpty())
        {
            g.setColour (pal::textDim);
            g.setFont (fonts::ui (12.0f));
            g.drawText (juce::String::fromUTF8 ("Não há outras instâncias do EQMotion neste projeto."), 14, 84, 300, 20, juce::Justification::centredLeft);
        }
    }
private:
    EQMotionProcessor& proc;
    juce::TextEditor nameEditor;
    juce::OwnedArray<juce::ToggleButton> toggles;
};

// ============================================================================= definições
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel (EQMotionProcessor& p, std::function<void (float)> setScale, std::function<void (float)> setDbRange, float currentDbRange)
        : proc (p), applyScale (std::move (setScale)), applyDb (std::move (setDbRange))
    {
        auto& az = proc.analyzer;
        precision.addItemList ({ "Baixa (2048)", juce::String::fromUTF8 ("Média (4096)"), "Alta (8192)", juce::String::fromUTF8 ("Máxima (16384)") }, 1);
        const int fft = az.fftSize.load();
        precision.setSelectedId (fft <= 2048 ? 1 : fft <= 4096 ? 2 : fft <= 8192 ? 3 : 4, juce::dontSendNotification);
        precision.onChange = [this] { static const int s[] = { 2048, 4096, 8192, 16384 }; proc.analyzer.fftSize.store (s[precision.getSelectedId() - 1]); proc.uiState.setProperty ("fft", proc.analyzer.fftSize.load(), nullptr); };
        speed.addItemList ({ "Lenta", juce::String::fromUTF8 ("Média"), juce::String::fromUTF8 ("Rápida") }, 1);
        speed.setSelectedId (az.speed.load() + 1, juce::dontSendNotification);
        speed.onChange = [this] { proc.analyzer.speed.store (speed.getSelectedId() - 1); proc.uiState.setProperty ("speed", speed.getSelectedId() - 1, nullptr); };
        slope.addItemList ({ "0 dB/oct", "3 dB/oct", "4,5 dB/oct", "6 dB/oct" }, 1);
        const float sl = az.slope.load();
        slope.setSelectedId (sl < 1 ? 1 : sl < 4 ? 2 : sl < 5 ? 3 : 4, juce::dontSendNotification);
        slope.onChange = [this] { static const float s[] = { 0.0f, 3.0f, 4.5f, 6.0f }; proc.analyzer.slope.store (s[slope.getSelectedId() - 1]); proc.uiState.setProperty ("slope", proc.analyzer.slope.load(), nullptr); };
        dbRange.addItemList ({ juce::String::fromUTF8 ("±3 dB"), juce::String::fromUTF8 ("±6 dB"), juce::String::fromUTF8 ("±12 dB"), juce::String::fromUTF8 ("±24 dB"), juce::String::fromUTF8 ("±30 dB") }, 1);
        static const float ranges[] = { 3, 6, 12, 24, 30 };
        for (int i = 0; i < 5; ++i) if (std::abs (ranges[i] - currentDbRange) < 0.1f) dbRange.setSelectedId (i + 1, juce::dontSendNotification);
        dbRange.onChange = [this] { applyDb (ranges[dbRange.getSelectedId() - 1]); };
        size.addItemList ({ "100 %", "125 %", "150 %", "175 %", "200 %" }, 1);
        const float sc = (float) (double) proc.uiState.getProperty ("scale", 1.0);
        size.setSelectedId (juce::jlimit (1, 5, juce::roundToInt ((sc - 1.0f) / 0.25f) + 1), juce::dontSendNotification);
        size.onChange = [this] { applyScale (1.0f + 0.25f * (size.getSelectedId() - 1)); };
        lpQ.addItemList (names::lpQualities(), 1);
        lpQAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.apvts.getParameter (ids::lpQuality), lpQ);
        lookAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (ids::lookahead), look, nullptr);
        logout.onClick = [this] { lic::License::get().lock(); if (onLogout) onLogout(); };
        logout.setEnabled (lic::License::get().isUnlocked());
        for (auto* c : std::initializer_list<juce::Component*> { &precision, &speed, &slope, &dbRange, &size, &lpQ, &look, &logout }) addAndMakeVisible (c);
        setSize (380, 400);
    }
    std::function<void()> onLogout;

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto row = [&] (juce::Component& c, int h = 26) { auto rr = r.removeFromTop (h); c.setBounds (rr.withTrimmedLeft (150)); labels.push_back (rr.withWidth (150)); r.removeFromTop (6); };
        labels.clear();
        r.removeFromTop (20);
        row (precision); row (speed); row (slope);
        r.removeFromTop (16);
        row (dbRange); row (size);
        r.removeFromTop (16);
        row (lpQ); row (look);
        r.removeFromTop (16);
        auto lr = r.removeFromTop (28);
        logout.setBounds (lr.removeFromRight (150));
        licArea = lr;
        aboutArea = r;
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (pal::bg1);
        static const char* t[] = { "Precisão", "Velocidade", "Inclinação", "Escala do gráfico", "Tamanho da janela", "Qualidade fase linear", "Lookahead" };
        g.setFont (fonts::ui (12.5f, 1));
        for (size_t i = 0; i < labels.size() && i < 7; ++i)
        {
            g.setColour (pal::textDim);
            g.drawText (juce::String::fromUTF8 (t[i]), labels[i], juce::Justification::centredLeft);
        }
        g.setColour (pal::textFaint.brighter (0.2f));
        g.setFont (fonts::ui (10.5f, 1, 0.05f));
        g.drawText ("ANALISADOR", 16, 12, 200, 16, juce::Justification::centredLeft);
        if (labels.size() >= 7)
        {
            g.drawText (juce::String::fromUTF8 ("GRÁFICO E JANELA"), 16, labels[3].getY() - 18, 200, 16, juce::Justification::centredLeft);
            g.drawText ("PROCESSAMENTO", 16, labels[5].getY() - 18, 200, 16, juce::Justification::centredLeft);
        }
        g.setFont (fonts::ui (12.0f, 1));
        g.setColour (lic::License::get().isUnlocked() ? pal::ok : pal::textDim);
        g.drawText (lic::License::get().isUnlocked() ? lic::License::get().statusText() : juce::String::fromUTF8 ("Sem sessão"), licArea, juce::Justification::centredLeft);
        g.setColour (pal::textFaint);
        g.setFont (fonts::ui (11.5f));
        g.drawFittedText (juce::String::fromUTF8 ("EQMotion by Piradex  v") + JucePlugin_VersionString + "\nwww.beatfreakstudio.com  ·  juninhopiradex@hotmail.com",
                          aboutArea.withTrimmedTop (8), juce::Justification::topLeft, 3);
    }

private:
    EQMotionProcessor& proc;
    std::function<void (float)> applyScale, applyDb;
    juce::ComboBox precision, speed, slope, dbRange, size, lpQ;
    ValueBar look { juce::String::fromUTF8 ("só Natural"), [] (double v) { return juce::String (v, 1).replace (".", ",") + " ms"; } };
    std::unique_ptr<juce::ComboBoxParameterAttachment> lpQAtt;
    std::unique_ptr<juce::SliderParameterAttachment> lookAtt;
    juce::TextButton logout { juce::String::fromUTF8 ("Terminar sessão") };
    std::vector<juce::Rectangle<int>> labels;
    juce::Rectangle<int> licArea, aboutArea;
};
