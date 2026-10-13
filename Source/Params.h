// Identificadores e layout dos parâmetros do MIXSPACE (todos automatizáveis na DAW)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    // página principal
    inline const juce::String space   { "space" };
    inline const juce::String range   { "range" };
    inline const juce::String focus   { "focus" };
    inline const juce::String delta   { "delta" };
    inline const juce::String bypass  { "bypass" };
    // painel ADVANCED
    inline const juce::String lowCut  { "lowcut" };
    inline const juce::String highCut { "highcut" };
    inline const juce::String attack  { "attack" };
    inline const juce::String release { "release" };
    inline const juce::String smooth  { "smooth" };
    inline const juce::String stereo  { "stereo" };
    inline const juce::String quality { "quality" };
    inline const juce::String scListen{ "sclisten" };
}

inline const juce::StringArray stereoModeNames { "Stereo", "Mid", "Side", "Left", "Right" };
inline const juce::StringArray qualityNames    { "Live", "Mix", "Master" };

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using Float = AudioParameterFloat;
    using Attr  = AudioParameterFloatAttributes;

    auto pct = [] (float v, int) { return String (v, 0) + " %"; };
    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (v, 0) + " Hz"; };
    auto ms  = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; };

    auto freqRange = [] (float lo, float hi)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (std::sqrt (lo * hi));   // escala logarítmica
        return r;
    };

    AudioProcessorValueTreeState::ParameterLayout layout;

    // SPACE: mais resolução na zona musical (10–40 %)
    NormalisableRange<float> spaceRange (0.0f, 100.0f, 0.1f);
    spaceRange.setSkewForCentre (35.0f);
    layout.add (std::make_unique<Float> (ParameterID { ids::space, 1 }, "Space", spaceRange, 25.0f,
                                         Attr().withLabel ("%").withStringFromValueFunction (pct)));
    layout.add (std::make_unique<Float> (ParameterID { ids::range, 1 }, "Range", NormalisableRange<float> (0.0f, 12.0f, 0.1f), 6.0f,
                                         Attr().withLabel ("dB").withStringFromValueFunction (db)));
    layout.add (std::make_unique<Float> (ParameterID { ids::focus, 1 }, "Focus", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f,
                                         Attr().withLabel ("%").withStringFromValueFunction (pct)));

    layout.add (std::make_unique<Float> (ParameterID { ids::lowCut, 1 }, "Low Cut", freqRange (20.0f, 2000.0f), 80.0f,
                                         Attr().withLabel ("Hz").withStringFromValueFunction (hz)));
    layout.add (std::make_unique<Float> (ParameterID { ids::highCut, 1 }, "High Cut", freqRange (1000.0f, 20000.0f), 16000.0f,
                                         Attr().withLabel ("Hz").withStringFromValueFunction (hz)));

    NormalisableRange<float> att (1.0f, 200.0f); att.setSkewForCentre (20.0f);
    NormalisableRange<float> rel (10.0f, 1000.0f); rel.setSkewForCentre (150.0f);
    layout.add (std::make_unique<Float> (ParameterID { ids::attack, 1 }, "Attack", att, 15.0f,
                                         Attr().withLabel ("ms").withStringFromValueFunction (ms)));
    layout.add (std::make_unique<Float> (ParameterID { ids::release, 1 }, "Release", rel, 180.0f,
                                         Attr().withLabel ("ms").withStringFromValueFunction (ms)));
    layout.add (std::make_unique<Float> (ParameterID { ids::smooth, 1 }, "Smooth", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 65.0f,
                                         Attr().withLabel ("%").withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::stereo, 1 }, "Stereo Mode", stereoModeNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::quality, 1 }, "Quality", qualityNames, 1,
                                                        AudioParameterChoiceAttributes().withAutomatable (false)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::delta, 1 }, "Delta", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::scListen, 1 }, "SC Listen", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));

    return layout;
}
