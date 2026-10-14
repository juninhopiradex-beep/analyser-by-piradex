#include "StateManager.h"

using namespace eqm;

StateManager::StateManager (juce::AudioProcessorValueTreeState& s, PresetManager& pm)
    : apvts (s), presetManager (pm)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (r->getParameterID() != ids::bypass && r->getParameterID() != ids::delta)
                snapParams.push_back (r);
    for (int i = 0; i < kMaxBands; ++i)
        for (int b = 0; b < (int) ids::B::Count; ++b)
            bandParams[(size_t) i][(size_t) b] = apvts.getParameter (ids::band (i, (ids::B) b));
    lastCommitted = capture();
    startTimer (300);
}

StateManager::~StateManager() { stopTimer(); }

StateManager::Snapshot StateManager::capture() const
{
    Snapshot s;
    s.values.reserve (snapParams.size());
    for (auto* p : snapParams) s.values.push_back (p->getValue());
    s.tags = tags;
    return s;
}

void StateManager::apply (const Snapshot& s)
{
    if (s.values.size() != snapParams.size()) return;
    for (size_t i = 0; i < snapParams.size(); ++i)
    {
        auto* p = snapParams[i];
        if (std::abs (p->getValue() - s.values[i]) > 1e-7f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (s.values[i]);
            p->endChangeGesture();
        }
    }
    tags = s.tags;
}

bool StateManager::pendingChange() const { return uiTouched && capture() != lastCommitted; }

void StateManager::push (const Snapshot& before, const Snapshot& after)
{
    auto& h = hist[(size_t) slot];
    h.undo.emplace_back (before, after);
    if (h.undo.size() > 200) h.undo.erase (h.undo.begin());
    h.redo.clear();
}

void StateManager::commitPending()
{
    const auto now = capture();
    if (uiTouched && now != lastCommitted) push (lastCommitted, now);
    lastCommitted = now;
    uiTouched = false;
}

void StateManager::timerCallback()
{
    // fecha um passo de desfazer quando o utilizador larga o rato
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) return;
    if (uiTouched)
    {
        const bool had = canUndo();
        commitPending();
        if (onChange && had != canUndo()) onChange();
        if (onChange) onChange();
    }
    else lastCommitted = capture();   // mudanças vindas do DAW (automação) não entram no histórico
}

void StateManager::performAction (const std::function<void()>& fn)
{
    commitPending();
    const auto before = capture();
    fn();
    const auto after = capture();
    if (after != before) push (before, after);
    lastCommitted = after;
    if (onChange) onChange();
}

void StateManager::undo()
{
    commitPending();
    auto& h = hist[(size_t) slot];
    if (h.undo.empty()) return;
    auto e = h.undo.back(); h.undo.pop_back();
    apply (e.first);
    lastCommitted = capture();
    h.redo.push_back (e);
    if (onChange) onChange();
}

void StateManager::redo()
{
    commitPending();
    auto& h = hist[(size_t) slot];
    if (h.redo.empty()) return;
    auto e = h.redo.back(); h.redo.pop_back();
    apply (e.second);
    lastCommitted = capture();
    h.undo.push_back (e);
    if (onChange) onChange();
}

// ----------------------------------------------------------------------------- A/B
void StateManager::switchSlot (int newSlot)
{
    newSlot = juce::jlimit (0, 1, newSlot);
    if (newSlot == slot) return;
    commitPending();
    slots[(size_t) slot] = capture(); slotValid[(size_t) slot] = true;
    if (! slotValid[(size_t) newSlot]) { slots[(size_t) newSlot] = slots[(size_t) slot]; slotValid[(size_t) newSlot] = true; }
    slot = newSlot;
    apply (slots[(size_t) slot]);
    lastCommitted = capture();
    if (onChange) onChange();
}

void StateManager::copyActiveToOther()
{
    commitPending();
    const int other = 1 - slot;
    slots[(size_t) other] = capture();
    slotValid[(size_t) other] = true;
    hist[(size_t) other] = History();
    if (onChange) onChange();
}

// ----------------------------------------------------------------------------- bandas
BandParams StateManager::band (int i) const
{
    params::BandPointers bp;
    for (int b = 0; b < (int) ids::B::Count; ++b)
        bp.p[(size_t) b] = apvts.getRawParameterValue (ids::band (i, (ids::B) b));
    return params::readBand (bp);
}

void StateManager::setBandParam (int i, ids::B which, float realValue)
{
    auto* p = bandParams[(size_t) i][(size_t) which];
    const float n = p->convertTo0to1 (realValue);
    if (std::abs (p->getValue() - n) < 1e-7f) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (n);
    p->endChangeGesture();
}

void StateManager::setBand (int i, const BandParams& b)
{
    using ids::B;
    setBandParam (i, B::Type, (float) b.type);
    setBandParam (i, B::Slope, (float) b.slope);
    setBandParam (i, B::Freq, (float) b.freq);
    setBandParam (i, B::Gain, (float) b.gainDb);
    setBandParam (i, B::Q, (float) b.q);
    setBandParam (i, B::Place, (float) b.placement);
    setBandParam (i, B::Dyn, b.dynamic ? 1.0f : 0.0f);
    setBandParam (i, B::Dir, (float) b.direction);
    setBandParam (i, B::Thr, (float) b.thresholdDb);
    setBandParam (i, B::Range, (float) b.rangeDb);
    setBandParam (i, B::Ratio, (float) b.ratio);
    setBandParam (i, B::Atk, (float) b.attackMs);
    setBandParam (i, B::Hold, (float) b.holdMs);
    setBandParam (i, B::Rel, (float) b.releaseMs);
    setBandParam (i, B::Knee, (float) b.kneeDb);
    setBandParam (i, B::Src, (float) b.source);
    setBandParam (i, B::Det, (float) b.detMode);
    setBandParam (i, B::En, b.enabled ? 1.0f : 0.0f);
    setBandParam (i, B::On, b.active ? 1.0f : 0.0f);
}

void StateManager::resetBandToDefault (int i)
{
    for (auto* p : bandParams[(size_t) i])
    {
        if (std::abs (p->getValue() - p->getDefaultValue()) < 1e-7f) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
    tags[(size_t) i] = 0;
}

int StateManager::firstFreeBand() const
{
    for (int i = 0; i < kMaxBands; ++i) if (bandParams[(size_t) i][(size_t) ids::B::On]->getValue() < 0.5f) return i;
    return -1;
}

int StateManager::activeBandCount() const
{
    int n = 0;
    for (int i = 0; i < kMaxBands; ++i) if (bandParams[(size_t) i][(size_t) ids::B::On]->getValue() >= 0.5f) ++n;
    return n;
}

int StateManager::addBand (BandParams b)
{
    const int i = firstFreeBand();
    if (i < 0) return -1;
    b.active = true;
    setBand (i, b);
    tags[(size_t) i] = 0;
    return i;
}

void StateManager::removeBand (int i)
{
    if (i < 0 || i >= kMaxBands) return;
    resetBandToDefault (i);
}

void StateManager::clearBands()
{
    for (int i = 0; i < kMaxBands; ++i) resetBandToDefault (i);
}

// ----------------------------------------------------------------------------- Smart Ops
int StateManager::applySmart (const std::vector<BandParams>& bands, int& appliedCount)
{
    const int opId = nextOpId++;
    appliedCount = 0;
    performAction ([&]
    {
        for (auto b : bands)
        {
            const int i = addBand (b);
            if (i < 0) break;
            tags[(size_t) i] = opId;
            ++appliedCount;
        }
    });
    lastOpId = opId;
    return opId;
}

void StateManager::revertSmart (int opId)
{
    performAction ([&]
    {
        for (int i = 0; i < kMaxBands; ++i)
            if (tags[(size_t) i] == opId) resetBandToDefault (i);
    });
}

bool StateManager::hasSmartBands (int opId) const
{
    if (opId <= 0) return false;
    for (int i = 0; i < kMaxBands; ++i)
        if (tags[(size_t) i] == opId && bandParams[(size_t) i][(size_t) ids::B::On]->getValue() >= 0.5f) return true;
    return false;
}

// ----------------------------------------------------------------------------- copiar / colar
juce::XmlElement StateManager::bandToXml (int i) const
{
    juce::XmlElement x ("BAND");
    for (int b = 0; b < (int) ids::B::Count; ++b)
    {
        auto* p = bandParams[(size_t) i][(size_t) b];
        x.setAttribute (ids::bandSuffix ((ids::B) b), p->convertFrom0to1 (p->getValue()));
    }
    return x;
}

void StateManager::bandFromXml (int i, const juce::XmlElement& x)
{
    for (int b = 0; b < (int) ids::B::Count; ++b)
    {
        const auto* key = ids::bandSuffix ((ids::B) b);
        if (x.hasAttribute (key)) setBandParam (i, (ids::B) b, (float) x.getDoubleAttribute (key));
    }
}

void StateManager::copyBands (const juce::Array<int>& indices)
{
    juce::XmlElement root ("EQMOTION_BANDS");
    for (int i : indices) root.addChildElement (new juce::XmlElement (bandToXml (i)));
    juce::SystemClipboard::copyTextToClipboard (root.toString());
}

bool StateManager::clipboardHasBands()
{
    return juce::SystemClipboard::getTextFromClipboard().contains ("EQMOTION_BANDS");
}

juce::Array<int> StateManager::pasteBands()
{
    juce::Array<int> added;
    auto xml = juce::parseXML (juce::SystemClipboard::getTextFromClipboard());
    if (xml == nullptr || ! xml->hasTagName ("EQMOTION_BANDS")) return added;
    performAction ([&]
    {
        for (auto* b : xml->getChildIterator())
        {
            const int i = firstFreeBand();
            if (i < 0) break;
            bandFromXml (i, *b);
            setBandParam (i, ids::B::On, 1.0f);
            tags[(size_t) i] = 0;
            added.add (i);
        }
    });
    return added;
}

// ----------------------------------------------------------------------------- presets
void StateManager::loadFactory (const FactoryPreset& p)
{
    performAction ([&]
    {
        clearBands();
        // globais que o preset não define voltam ao valor por omissão (exceto saída, auto gain e bypass)
        for (auto id : { ids::procMode, ids::phase, ids::lpQuality, ids::character, ids::deharsh, ids::deharshAmt,
                         ids::deharshAtk, ids::deharshRel, ids::gainScale, ids::lookahead })
        {
            auto* par = apvts.getParameter (id);
            float target = par->getDefaultValue();
            auto it = p.globals.find (id);
            if (it != p.globals.end()) target = par->convertTo0to1 (it->second);
            if (std::abs (par->getValue() - target) > 1e-7f) { par->beginChangeGesture(); par->setValueNotifyingHost (target); par->endChangeGesture(); }
        }
        for (auto b : p.bands) addBand (b);
    });
    currentPreset = p.name;
    presetSnapshot = capture(); presetSnapshotValid = true;
    if (onChange) onChange();
}

std::map<juce::String, float> StateManager::globalsMap() const
{
    std::map<juce::String, float> g;
    for (auto id : { ids::procMode, ids::phase, ids::lpQuality, ids::character, ids::deharsh, ids::deharshAmt,
                     ids::deharshAtk, ids::deharshRel, ids::gainScale, ids::lookahead, ids::output, ids::autoGain })
    {
        auto* par = apvts.getParameter (id);
        g[id] = par->convertFrom0to1 (par->getValue());
    }
    return g;
}

bool StateManager::saveUser (const juce::String& name)
{
    auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return false;
    juce::XmlElement root ("EQMOTION_PRESET");
    root.setAttribute ("name", name.trim());
    root.setAttribute ("version", 1);
    auto* g = root.createNewChildElement ("GLOBALS");
    for (auto& [k, v] : globalsMap()) g->setAttribute (k, v);
    for (int i = 0; i < kMaxBands; ++i)
        if (bandParams[(size_t) i][(size_t) ids::B::On]->getValue() >= 0.5f)
            root.addChildElement (new juce::XmlElement (bandToXml (i)));
    const auto file = presetManager.userFolder().getChildFile (safe + ".eqmpreset");
    const bool ok = root.writeTo (file);
    if (ok) { currentPreset = name.trim(); presetSnapshot = capture(); presetSnapshotValid = true; if (onChange) onChange(); }
    return ok;
}

bool StateManager::loadUser (const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr || ! xml->hasTagName ("EQMOTION_PRESET")) return false;
    performAction ([&]
    {
        clearBands();
        if (auto* g = xml->getChildByName ("GLOBALS"))
            for (int a = 0; a < g->getNumAttributes(); ++a)
                if (auto* par = apvts.getParameter (g->getAttributeName (a)))
                {
                    const float n = par->convertTo0to1 ((float) g->getAttributeValue (a).getDoubleValue());
                    par->beginChangeGesture(); par->setValueNotifyingHost (n); par->endChangeGesture();
                }
        int i = 0;
        for (auto* b : xml->getChildWithTagNameIterator ("BAND"))
        {
            if (i >= kMaxBands) break;
            bandFromXml (i, *b);
            setBandParam (i, ids::B::On, 1.0f);
            ++i;
        }
    });
    currentPreset = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
    presetSnapshot = capture(); presetSnapshotValid = true;
    if (onChange) onChange();
    return true;
}

bool StateManager::isModifiedSincePreset() const
{
    if (! presetSnapshotValid) return activeBandCount() > 0 && currentPreset == "Default";
    return capture().values != presetSnapshot.values;
}

// ----------------------------------------------------------------------------- persistência
static juce::String snapToString (const StateManager::Snapshot& s)
{
    juce::StringArray a;
    for (float v : s.values) a.add (juce::String (v, 7));
    juce::StringArray t;
    for (int v : s.tags) t.add (juce::String (v));
    return a.joinIntoString (",") + "|" + t.joinIntoString (",");
}

static StateManager::Snapshot snapFromString (const juce::String& str)
{
    StateManager::Snapshot s;
    const auto parts = juce::StringArray::fromTokens (str, "|", "");
    if (parts.size() < 2) return s;
    for (auto& v : juce::StringArray::fromTokens (parts[0], ",", "")) s.values.push_back (v.getFloatValue());
    const auto t = juce::StringArray::fromTokens (parts[1], ",", "");
    for (int i = 0; i < juce::jmin (t.size(), eqm::kMaxBands); ++i) s.tags[(size_t) i] = t[i].getIntValue();
    return s;
}

juce::ValueTree StateManager::toValueTree() const
{
    juce::ValueTree t ("EQM_STATE");
    t.setProperty ("slot", slot, nullptr);
    t.setProperty ("preset", currentPreset, nullptr);
    t.setProperty ("nextOp", nextOpId, nullptr);
    t.setProperty ("lastOp", lastOpId, nullptr);
    juce::StringArray tg; for (int v : tags) tg.add (juce::String (v));
    t.setProperty ("tags", tg.joinIntoString (","), nullptr);
    for (int i = 0; i < 2; ++i)
    {
        Snapshot s = (i == slot) ? capture() : slots[(size_t) i];
        if (i == slot || slotValid[(size_t) i]) t.setProperty ("slot" + juce::String (i), snapToString (s), nullptr);
    }
    return t;
}

void StateManager::fromValueTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    slot = juce::jlimit (0, 1, (int) t.getProperty ("slot", 0));
    currentPreset = t.getProperty ("preset", "Default").toString();
    nextOpId = juce::jmax (1, (int) t.getProperty ("nextOp", 1));
    lastOpId = (int) t.getProperty ("lastOp", 0);
    const auto tg = juce::StringArray::fromTokens (t.getProperty ("tags").toString(), ",", "");
    for (int i = 0; i < juce::jmin (tg.size(), kMaxBands); ++i) tags[(size_t) i] = tg[i].getIntValue();
    for (int i = 0; i < 2; ++i)
    {
        const auto key = "slot" + juce::String (i);
        if (t.hasProperty (key))
        {
            slots[(size_t) i] = snapFromString (t.getProperty (key).toString());
            slotValid[(size_t) i] = slots[(size_t) i].values.size() == snapParams.size();
        }
    }
    hist = {};
    lastCommitted = capture();
    presetSnapshotValid = false;
    if (onChange) onChange();
}
