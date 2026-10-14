// =============================================================================
//  EQMotion — registo das instâncias abertas no projeto (no mesmo processo)
//  Cada instância publica o seu espetro de saída, a média de longo prazo e a
//  curva; as outras podem sobrepô-los no gráfico ou usá-los como referência.
//  Só é tocado nas threads de mensagens e de análise (nunca na de áudio).
// =============================================================================
#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <memory>
#include <vector>

struct InstanceShared
{
    juce::SpinLock lock;
    int id = 0;
    juce::String name;
    juce::Colour colour;
    std::vector<float> spectrumDb;    // pontos do ecrã (mesma grelha para todas)
    std::vector<double> avgGridDb;    // média de longo prazo na LogGrid (para "igualar referência")
    std::vector<float> curveDb;       // curva EQ (pontos do ecrã)
    double displayLo = 10, displayHi = 30000;
    bool bypassed = false;
    juce::uint32 lastUpdateMs = 0;
};

class InstanceRegistry
{
public:
    static InstanceRegistry& get() { static InstanceRegistry r; return r; }

    std::shared_ptr<InstanceShared> add()
    {
        const juce::ScopedLock sl (lock);
        auto s = std::make_shared<InstanceShared>();
        s->id = ++nextId;
        s->name = "EQMotion " + juce::String (s->id);
        list.push_back (s);
        return s;
    }
    void remove (const std::shared_ptr<InstanceShared>& s)
    {
        const juce::ScopedLock sl (lock);
        list.erase (std::remove (list.begin(), list.end(), s), list.end());
    }
    std::vector<std::shared_ptr<InstanceShared>> all() const
    {
        const juce::ScopedLock sl (lock);
        return list;
    }

    // Enquanto houver algum editor aberto, todas as instâncias publicam o espetro
    void editorOpened()  { openEditors.fetch_add (1); }
    void editorClosed()  { openEditors.fetch_sub (1); }
    bool anyEditorOpen() const { return openEditors.load() > 0; }

private:
    juce::CriticalSection lock;
    std::vector<std::shared_ptr<InstanceShared>> list;
    int nextId = 0;
    std::atomic<int> openEditors { 0 };
};
