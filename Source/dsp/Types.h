// =============================================================================
//  EQMotion by Piradex — tipos partilhados do núcleo DSP
//  (C++17 puro: sem JUCE, para poder ser testado isoladamente)
// =============================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>

namespace eqm
{
constexpr int    kMaxBands      = 24;
constexpr double kPi            = 3.14159265358979323846;
constexpr double kTwoPi         = 2.0 * kPi;
constexpr double kMinFreq       = 10.0;
constexpr double kMaxFreq       = 30000.0;
constexpr double kMinQ          = 0.025;
constexpr double kMaxQ          = 40.0;
constexpr double kMaxGainDb     = 30.0;

enum class FilterType : int
{
    Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, AllPass, Tilt,
    NumTypes
};

// Inclinações dos cortes: 6 → 96 dB/oct + "parede" (brickwall)
enum class Slope : int { dB6 = 0, dB12, dB18, dB24, dB36, dB48, dB72, dB96, Brickwall, NumSlopes };

inline int slopeOrder (Slope s)
{
    static constexpr int orders[] = { 1, 2, 3, 4, 6, 8, 12, 16, 16 };
    return orders[(int) s];
}
inline int slopeDbPerOct (Slope s) { return s == Slope::Brickwall ? 0 : slopeOrder (s) * 6; }

// Onde a banda atua
enum class Placement : int { Stereo = 0, Left, Right, Mid, Side, NumPlacements };

enum class DynDirection : int { Above = 0, Below };   // atua quando o detetor está acima/abaixo do limiar
enum class DetectorSource : int { Internal = 0, Sidechain };
enum class DetectorMode : int { Peak = 0, RMS };

enum class PhaseMode : int { ZeroLatency = 0, Natural, Linear };
enum class LinearQuality : int { Low = 0, Medium, High };
enum class ProcessMode : int { Stereo = 0, LeftRight, MidSide };

inline bool typeHasGain (FilterType t)
{
    return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf || t == FilterType::Tilt;
}
inline bool typeIsCut (FilterType t) { return t == FilterType::LowCut || t == FilterType::HighCut; }
inline bool typeSupportsDynamics (FilterType t) { return typeHasGain (t); }

struct BandParams
{
    bool active = false;          // a banda existe
    bool enabled = true;          // false = banda em bypass
    FilterType type = FilterType::Bell;
    Slope slope = Slope::dB12;
    double freq = 1000.0;
    double gainDb = 0.0;
    double q = 0.7071;
    Placement placement = Placement::Stereo;

    bool dynamic = false;
    DynDirection direction = DynDirection::Above;
    double thresholdDb = -30.0;
    double rangeDb = -6.0;        // com sinal: negativo corta, positivo realça quando ativo
    double ratio = 4.0;
    double attackMs = 5.0;
    double holdMs = 0.0;
    double releaseMs = 80.0;
    double kneeDb = 6.0;
    DetectorSource source = DetectorSource::Internal;
    DetectorMode detMode = DetectorMode::Peak;

    bool isProcessing() const { return active && enabled; }
    bool isDynamic() const    { return dynamic && typeSupportsDynamics (type); }

    // Igualdade das partes que mudam a forma estática do filtro
    bool sameShape (const BandParams& o) const
    {
        return active == o.active && enabled == o.enabled && type == o.type && slope == o.slope
            && placement == o.placement;
    }
};

using BandArray = std::array<BandParams, kMaxBands>;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g, double floorDb = -200.0)
{
    return g > 0.0 ? std::max (floorDb, 20.0 * std::log10 (g)) : floorDb;
}
template <typename T> inline T clampv (T v, T lo, T hi) { return std::min (hi, std::max (lo, v)); }
} // namespace eqm
