// =============================================================================
//  EQMotion — presets de fábrica (10 pastas) e presets do utilizador
// =============================================================================
#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "dsp/Types.h"
#include <map>
#include <vector>

struct PresetBand
{
    eqm::BandParams p;
};

struct FactoryPreset
{
    juce::String folder, name, description;
    std::vector<eqm::BandParams> bands;
    std::map<juce::String, float> globals;   // id → valor real (ex.: "character" → 1)
};

namespace presets
{
using namespace eqm;
inline BandParams bell (double f, double g, double q) { BandParams b; b.active = true; b.type = FilterType::Bell; b.freq = f; b.gainDb = g; b.q = q; return b; }
inline BandParams ls (double f, double g, double q = 0.7) { auto b = bell (f, g, q); b.type = FilterType::LowShelf; return b; }
inline BandParams hs (double f, double g, double q = 0.7) { auto b = bell (f, g, q); b.type = FilterType::HighShelf; return b; }
inline BandParams lc (double f, Slope s = Slope::dB24, double q = 0.7071) { BandParams b; b.active = true; b.type = FilterType::LowCut; b.freq = f; b.slope = s; b.q = q; return b; }
inline BandParams hc (double f, Slope s = Slope::dB12, double q = 0.7071) { auto b = lc (f, s, q); b.type = FilterType::HighCut; return b; }
inline BandParams notch (double f, double q) { BandParams b; b.active = true; b.type = FilterType::Notch; b.freq = f; b.q = q; return b; }
inline BandParams tilt (double f, double g) { auto b = bell (f, g, 0.7); b.type = FilterType::Tilt; return b; }
inline BandParams dyn (BandParams b, double thr, double range, double ratio = 3, double atk = 5, double rel = 80,
                       DetectorMode m = DetectorMode::RMS, DetectorSource src = DetectorSource::Internal)
{
    b.dynamic = true; b.thresholdDb = thr; b.rangeDb = range; b.ratio = ratio; b.attackMs = atk; b.releaseMs = rel; b.detMode = m; b.source = src;
    return b;
}
inline BandParams below (BandParams b) { b.direction = DynDirection::Below; return b; }
inline BandParams at (BandParams b, Placement p) { b.placement = p; return b; }

const std::vector<FactoryPreset>& factory();

// Texto mostrado no idioma da interface (o nome guardado não muda)
juce::String displayName (const juce::String& storedName);
juce::String displayDescription (const FactoryPreset&);
juce::String displayFolder (const juce::String& folder);
} // namespace presets

class PresetManager
{
public:
    PresetManager();

    juce::File userFolder() const;
    juce::Array<juce::File> userPresets() const;

    bool isFavourite (const juce::String& key) const;
    void setFavourite (const juce::String& key, bool fav);

    juce::PropertiesFile& settings() { return *props; }

    static juce::String factoryKey (const FactoryPreset& p) { return "factory:" + p.folder + "/" + p.name; }
    static juce::String userKey (const juce::File& f) { return "user:" + f.getFileNameWithoutExtension(); }

private:
    std::unique_ptr<juce::PropertiesFile> props;
};
