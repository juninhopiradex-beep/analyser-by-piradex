// EQMotion — nomes mostrados na interface no idioma actual
// (os nomes das escolhas dos parâmetros vistos pelo DAW ficam fixos, em Params.h)
#pragma once
#include "Theme.h"
#include "../Params.h"

namespace uinames
{
inline juce::StringArray types()
{
    return { tr ("Sino", "Bell"), "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch",
             tr ("Passa-banda", "Band Pass"), "All-pass", "Tilt" };
}
inline juce::StringArray slopes() { return names::slopes(); }
inline juce::StringArray placements() { return { tr ("Estéreo", "Stereo"), tr ("Esquerda", "Left"), tr ("Direita", "Right"), "Mid", "Side" }; }
inline juce::StringArray placementShort() { return { "ST", tr ("E", "L"), tr ("D", "R"), "M", "S" }; }
} // namespace uinames
