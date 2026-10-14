// =============================================================================
//  EQMotion by Piradex — parâmetros (todos automatizáveis, versão 1)
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Types.h"

namespace ids
{
inline const juce::String bypass    { "bypass" },  output   { "output" },    autoGain  { "autoGain" },
                          procMode  { "procMode" }, phase   { "phase" },     lpQuality { "lpQuality" },
                          lookahead { "lookahead" }, character { "character" },
                          deharsh   { "deharsh" },  deharshAmt { "deharshAmt" }, deharshAtk { "deharshAtk" },
                          deharshRel { "deharshRel" }, delta { "delta" },    gainScale { "gainScale" };

// sufixos por banda
enum class B : int { On = 0, En, Type, Slope, Freq, Gain, Q, Place, Dyn, Dir, Thr, Range, Ratio, Atk, Hold, Rel, Knee, Src, Det, Count };
inline const char* bandSuffix (B b)
{
    static const char* s[] = { "on", "en", "type", "slope", "freq", "gain", "q", "place", "dyn", "dir", "thr", "range",
                                "ratio", "atk", "hold", "rel", "knee", "src", "det" };
    return s[(int) b];
}
inline juce::String band (int index, B b) { return "b" + juce::String (index + 1) + "_" + bandSuffix (b); }
}

namespace names
{
inline juce::StringArray types()
{
    return { juce::String::fromUTF8 ("Sino"), "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch",
             juce::String::fromUTF8 ("Passa-banda"), "All-pass", "Tilt" };
}
inline juce::StringArray slopes() { return { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall" }; }
inline juce::StringArray placements() { return { juce::String::fromUTF8 ("Estéreo"), "Esquerda", "Direita", "Mid", "Side" }; }
inline juce::StringArray placementShort() { return { "ST", "L", "R", "M", "S" }; }
inline juce::StringArray phases() { return { "Zero Latency", "Natural Phase", "Linear Phase" }; }
inline juce::StringArray lpQualities() { return { "Baixa", juce::String::fromUTF8 ("Média"), "Alta" }; }
inline juce::StringArray procModes() { return { juce::String::fromUTF8 ("Estéreo"), "Esq./Dir.", "Mid/Side" }; }
inline juce::StringArray characters() { return { "Clean", "Subtle", "Warm" }; }
}

namespace params
{
using namespace juce;

inline NormalisableRange<float> freqRange()
{
    NormalisableRange<float> r (10.0f, 30000.0f,
        [] (float s, float e, float t) { return s * std::pow (e / s, t); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
    r.interval = 0.0f;
    return r;
}
inline NormalisableRange<float> logRange (float lo, float hi)
{
    return NormalisableRange<float> (lo, hi,
        [] (float s, float e, float t) { return s * std::pow (e / s, t); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
}

inline String hzText (float v, int)
{
    if (v >= 1000.0f) return String (v / 1000.0f, v >= 10000.0f ? 1 : 2).replace (".", ",") + " kHz";
    return String (v, v < 100.0f ? 1 : 0).replace (".", ",") + " Hz";
}
inline float hzFromText (const String& t)
{
    auto s = t.trim().toLowerCase().replace (",", ".");
    const bool k = s.contains ("k");
    float v = s.retainCharacters ("0123456789.-").getFloatValue();
    return k ? v * 1000.0f : v;
}
inline String dbText (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1).replace (".", ",") + " dB"; }
inline String msText (float v, int) { return (v < 10.0f ? String (v, 1) : String (v, 0)).replace (".", ",") + " ms"; }
inline float numFromText (const String& t) { return t.replace (",", ".").retainCharacters ("0123456789.-").getFloatValue(); }

inline AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto pid = [] (const String& id) { return ParameterID { id, 1 }; };
    auto dbAttr = AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withValueFromStringFunction (numFromText).withLabel ("dB");
    auto msAttr = AudioParameterFloatAttributes().withStringFromValueFunction (msText).withValueFromStringFunction (numFromText).withLabel ("ms");
    auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; })
                                                   .withValueFromStringFunction (numFromText);

    // ---- globais
    layout.add (std::make_unique<AudioParameterBool> (pid (ids::bypass), "Bypass", false));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::output), String::fromUTF8 ("Saída"), NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterBool> (pid (ids::autoGain), "Auto gain", false));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::procMode), "Modo de canais", names::procModes(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::phase), "Fase", names::phases(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::lpQuality), "Qualidade fase linear", names::lpQualities(), 1));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::lookahead), "Lookahead", NormalisableRange<float> (0.0f, 20.0f, 0.1f), 0.0f, msAttr));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ids::character), "Character", names::characters(), 0));
    layout.add (std::make_unique<AudioParameterBool> (pid (ids::deharsh), "De-harsh", false));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::deharshAmt), "De-harsh amount", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctAttr));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::deharshAtk), "De-harsh attack", logRange (0.5f, 50.0f), 5.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::deharshRel), "De-harsh release", logRange (10.0f, 500.0f), 80.0f, msAttr));
    layout.add (std::make_unique<AudioParameterBool> (pid (ids::delta), "Delta", false));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ids::gainScale), "Escala de ganho", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pctAttr));

    // ---- 24 bandas
    for (int i = 0; i < eqm::kMaxBands; ++i)
    {
        const String n = "Banda " + String (i + 1) + " ";
        using ids::B;
        auto grp = std::make_unique<AudioProcessorParameterGroup> ("band" + String (i + 1), "Banda " + String (i + 1), "|");
        grp->addChild (std::make_unique<AudioParameterBool> (pid (ids::band (i, B::On)), n + "ativa", false));
        grp->addChild (std::make_unique<AudioParameterBool> (pid (ids::band (i, B::En)), n + "ligada", true));
        grp->addChild (std::make_unique<AudioParameterChoice> (pid (ids::band (i, B::Type)), n + "tipo", names::types(), 0));
        grp->addChild (std::make_unique<AudioParameterChoice> (pid (ids::band (i, B::Slope)), n + String::fromUTF8 ("inclinação"), names::slopes(), 1));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Freq)), n + String::fromUTF8 ("frequência"), freqRange(), 1000.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (hzText).withValueFromStringFunction (hzFromText).withLabel ("Hz")));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Gain)), n + "ganho", NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f, dbAttr));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Q)), n + "Q", logRange ((float) eqm::kMinQ, (float) eqm::kMaxQ), 0.7071f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10 ? 2 : 1).replace (".", ","); }).withValueFromStringFunction (numFromText)));
        grp->addChild (std::make_unique<AudioParameterChoice> (pid (ids::band (i, B::Place)), n + String::fromUTF8 ("colocação"), names::placements(), 0));
        grp->addChild (std::make_unique<AudioParameterBool> (pid (ids::band (i, B::Dyn)), n + String::fromUTF8 ("dinâmica"), false));
        grp->addChild (std::make_unique<AudioParameterChoice> (pid (ids::band (i, B::Dir)), n + String::fromUTF8 ("direção"), StringArray { "Acima", "Abaixo" }, 0));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Thr)), n + "limiar", NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -30.0f, dbAttr));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Range)), n + "range", NormalisableRange<float> (-30.0f, 30.0f, 0.01f), -6.0f, dbAttr));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Ratio)), n + "ratio", logRange (1.0f, 20.0f), 4.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1).replace (".", ",") + ":1"; }).withValueFromStringFunction (numFromText)));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Atk)), n + "ataque", logRange (0.1f, 200.0f), 5.0f, msAttr));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Hold)), n + "hold", NormalisableRange<float> (0.0f, 500.0f, 0.1f, 0.4f), 0.0f, msAttr));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Rel)), n + "release", logRange (5.0f, 2000.0f), 80.0f, msAttr));
        grp->addChild (std::make_unique<AudioParameterFloat> (pid (ids::band (i, B::Knee)), n + "joelho", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, dbAttr));
        grp->addChild (std::make_unique<AudioParameterChoice> (pid (ids::band (i, B::Src)), n + "detetor", StringArray { "Pista", "Sidechain" }, 0));
        grp->addChild (std::make_unique<AudioParameterChoice> (pid (ids::band (i, B::Det)), n + "modo detetor", StringArray { "Peak", "RMS" }, 0));
        layout.add (std::move (grp));
    }
    return layout;
}

// Ponteiros "raw" para leitura rápida (thread de áudio)
struct BandPointers
{
    std::array<std::atomic<float>*, (size_t) ids::B::Count> p {};
};

inline eqm::BandParams readBand (const BandPointers& bp)
{
    using ids::B;
    auto v = [&] (B b) { return bp.p[(size_t) b]->load (std::memory_order_relaxed); };
    eqm::BandParams r;
    r.active     = v (B::On) > 0.5f;
    r.enabled    = v (B::En) > 0.5f;
    r.type       = (eqm::FilterType) juce::jlimit (0, (int) eqm::FilterType::NumTypes - 1, (int) v (B::Type));
    r.slope      = (eqm::Slope) juce::jlimit (0, (int) eqm::Slope::NumSlopes - 1, (int) v (B::Slope));
    r.freq       = v (B::Freq);
    r.gainDb     = v (B::Gain);
    r.q          = v (B::Q);
    r.placement  = (eqm::Placement) juce::jlimit (0, 4, (int) v (B::Place));
    r.dynamic    = v (B::Dyn) > 0.5f;
    r.direction  = (eqm::DynDirection) juce::jlimit (0, 1, (int) v (B::Dir));
    r.thresholdDb = v (B::Thr);
    r.rangeDb    = v (B::Range);
    r.ratio      = v (B::Ratio);
    r.attackMs   = v (B::Atk);
    r.holdMs     = v (B::Hold);
    r.releaseMs  = v (B::Rel);
    r.kneeDb     = v (B::Knee);
    r.source     = (eqm::DetectorSource) juce::jlimit (0, 1, (int) v (B::Src));
    r.detMode    = (eqm::DetectorMode) juce::jlimit (0, 1, (int) v (B::Det));
    return r;
}
} // namespace params
