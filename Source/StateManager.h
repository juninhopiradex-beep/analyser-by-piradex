// =============================================================================
//  EQMotion — estado de alto nível (thread de mensagens)
//    • desfazer/refazer por "fotografias" de todos os parâmetros (cada slot A/B tem o seu histórico)
//    • slots A/B com cópia
//    • etiquetas das bandas criadas pelas Smart Ops (Revert só remove essas)
//    • operações de bandas: adicionar, remover, limpar, copiar/colar, presets
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"

class StateManager : private juce::Timer
{
public:
    struct Snapshot
    {
        std::vector<float> values;
        std::array<int, eqm::kMaxBands> tags {};
        bool operator== (const Snapshot& o) const { return values == o.values && tags == o.tags; }
        bool operator!= (const Snapshot& o) const { return ! (*this == o); }
    };

    StateManager (juce::AudioProcessorValueTreeState& state, PresetManager& presets);
    ~StateManager() override;

    // ---- histórico
    void markUiTouched() { uiTouched = true; }
    void performAction (const std::function<void()>& fn);   // uma operação = um passo de desfazer
    void undo();
    void redo();
    bool canUndo() const { return ! hist[(size_t) slot].undo.empty() || pendingChange(); }
    bool canRedo() const { return ! hist[(size_t) slot].redo.empty(); }
    void commitPending();

    // ---- A/B
    int activeSlot() const { return slot; }
    void switchSlot (int newSlot);
    void copyActiveToOther();

    // ---- bandas
    eqm::BandParams band (int i) const;
    void setBand (int i, const eqm::BandParams& b);               // sem passo de desfazer (usar dentro de performAction)
    void setBandParam (int i, ids::B which, float realValue);
    int  addBand (eqm::BandParams b);                               // -1 se as 24 estiverem ocupadas
    void removeBand (int i);
    void clearBands();
    int  activeBandCount() const;
    int  firstFreeBand() const;

    // ---- etiquetas Smart Ops
    int  bandTag (int i) const { return tags[(size_t) i]; }
    int  applySmart (const std::vector<eqm::BandParams>& bands, int& appliedCount);   // devolve o id da operação
    void revertSmart (int opId);
    bool hasSmartBands (int opId) const;
    int  lastSmartOp() const { return lastOpId; }

    // ---- copiar / colar (área de transferência do sistema, também entre instâncias)
    void copyBands (const juce::Array<int>& indices);
    juce::Array<int> pasteBands();
    static bool clipboardHasBands();

    // ---- presets
    void loadFactory (const FactoryPreset& p);
    bool loadUser (const juce::File& f);
    bool saveUser (const juce::String& name);
    juce::String presetName() const { return currentPreset; }
    void setPresetName (const juce::String& n) { currentPreset = n; if (onChange) onChange(); }
    bool isModifiedSincePreset() const;

    // ---- persistência (dentro do estado do plugin)
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& t);

    std::function<void()> onChange;   // UI atualiza botões de desfazer/A-B/preset

    Snapshot capture() const;
    void apply (const Snapshot& s);

private:
    struct History { std::vector<std::pair<Snapshot, Snapshot>> undo, redo; };
    void timerCallback() override;
    void push (const Snapshot& before, const Snapshot& after);
    bool pendingChange() const;
    void resetBandToDefault (int i);
    juce::XmlElement bandToXml (int i) const;
    void bandFromXml (int i, const juce::XmlElement& x);
    std::map<juce::String, float> globalsMap() const;

    juce::AudioProcessorValueTreeState& apvts;
    PresetManager& presetManager;
    std::vector<juce::RangedAudioParameter*> snapParams;
    std::array<std::array<juce::RangedAudioParameter*, (size_t) ids::B::Count>, eqm::kMaxBands> bandParams {};
    std::array<int, eqm::kMaxBands> tags {};
    int lastOpId = 0, nextOpId = 1;
    std::array<History, 2> hist;
    std::array<Snapshot, 2> slots;
    std::array<bool, 2> slotValid { false, false };
    int slot = 0;
    Snapshot lastCommitted, presetSnapshot;
    bool uiTouched = false, presetSnapshotValid = false;
    juce::String currentPreset { "Default" };
};
