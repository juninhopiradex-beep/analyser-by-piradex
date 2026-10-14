#include "Presets.h"

namespace presets
{
static FactoryPreset P (const char* folder, const char* name, const char* desc, std::vector<BandParams> b,
                        std::map<juce::String, float> g = {})
{
    return { juce::String::fromUTF8 (folder), juce::String::fromUTF8 (name), juce::String::fromUTF8 (desc), std::move (b), std::move (g) };
}

const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> list = []
    {
        std::vector<FactoryPreset> v;
        // ---------------------------------------------------------------- Voz
        v.push_back (P ("Voz", "Voz clara", "Limpa graves, tira caixa a 300 Hz e abre o ar.",
            { lc (90, Slope::dB24), bell (300, -2.5, 1.4), bell (3200, 1.5, 1.0), hs (11000, 2.5, 0.6) }));
        v.push_back (P ("Voz", "Voz rap presente", "Corpo controlado, presença e sibilância dinâmica.",
            { lc (80, Slope::dB18), dyn (bell (220, 0, 1.2), -26, -4, 3, 10, 120), bell (2800, 2.0, 0.9),
              dyn (bell (6800, 0, 3.0), -30, -6, 4, 1, 50), hs (12000, 2.0, 0.6) }));
        v.push_back (P ("Voz", "Voz R&B suave", "Calor nos médios-graves, agudos sedosos.",
            { lc (70, Slope::dB12), bell (180, 1.5, 0.8), bell (450, -1.5, 1.5), dyn (bell (3500, 0, 1.6), -24, -3, 2.5, 5, 100),
              hs (14000, 2.0, 0.6) }, { { "character", 1.0f } }));
        v.push_back (P ("Voz", "Voz kizomba/semba", "Voz quente e à frente sem dureza.",
            { lc (85, Slope::dB24), bell (250, -1.5, 1.0), bell (1200, 1.0, 0.8), dyn (bell (4000, 0, 1.5), -25, -3.5, 3, 4, 90),
              hs (10000, 1.5, 0.7) }));
        v.push_back (P ("Voz", "Backing vocals", "Recua e abre espaço para a voz principal.",
            { lc (150, Slope::dB24), bell (400, -2.0, 0.9), bell (2500, -2.0, 0.8), hs (9000, 1.5, 0.7) }));

        // ---------------------------------------------------------------- Bateria
        v.push_back (P ("Bateria", "Bombo com punch", "Sub firme, menos caixa, clique definido.",
            { lc (28, Slope::dB24), bell (60, 2.5, 1.2), bell (350, -4.0, 1.4), bell (3500, 2.5, 1.6), hc (14000, Slope::dB12) }));
        v.push_back (P ("Bateria", "Tarola com corpo", "Corpo aos 200 Hz e estalo, toque de ar.",
            { lc (90, Slope::dB18), bell (200, 2.0, 1.2), notch (900, 8.0), bell (5000, 2.0, 1.0), hs (11000, 1.5, 0.7) }));
        v.push_back (P ("Bateria", "Tarola dinâmica", "Doma a ressonância da tarola só nas pancadas.",
            { lc (80, Slope::dB12), dyn (bell (420, 0, 4.0), -24, -6, 4, 2, 80, DetectorMode::Peak), bell (5500, 1.5, 1.2) }));
        v.push_back (P ("Bateria", "Overheads", "Pratos brilhantes sem graves a mais.",
            { lc (250, Slope::dB24), bell (800, -1.5, 1.0), dyn (bell (3500, 0, 2.0), -28, -3, 3, 3, 80), hs (12000, 2.0, 0.6) }));
        v.push_back (P ("Bateria", "Hi-hats suaves", "Retira a agressividade dos 5–8 kHz.",
            { lc (400, Slope::dB24), dyn (bell (6500, 0, 1.8), -30, -5, 3, 2, 60) }));

        // ---------------------------------------------------------------- Baixo
        v.push_back (P ("Baixo", "Baixo 808 controlado", "Sub estável, harmónicos audíveis em colunas pequenas.",
            { lc (25, Slope::dB36), dyn (ls (70, 0, 0.7), -18, -3, 3, 15, 150), bell (250, -2.0, 1.0), bell (900, 1.5, 1.2) },
            { { "character", 1.0f } }));
        v.push_back (P ("Baixo", "Baixo elétrico", "Corpo, definição e menos lama.",
            { lc (35, Slope::dB24), bell (100, 1.5, 1.0), bell (300, -2.5, 1.2), bell (1500, 2.0, 1.4), hc (8000, Slope::dB12) }));
        v.push_back (P ("Baixo", "Sub mono limpo", "Graves só no centro; lados abaixo de 120 Hz cortados.",
            { lc (25, Slope::dB24), at (lc (120, Slope::dB24), Placement::Side), bell (60, 1.0, 1.0) }, { { "procMode", 2.0f } }));
        v.push_back (P ("Baixo", "Synth bass", "Brilho no ataque, médio-grave limpo.",
            { lc (30, Slope::dB24), bell (180, -1.5, 1.0), bell (2200, 2.0, 1.0), hs (6000, 1.5, 0.7) }));

        // ---------------------------------------------------------------- Mix Bus
        v.push_back (P ("Mix Bus", "Brilho de bus", "Tilt suave e ar — cola a mistura.",
            { lc (22, Slope::dB12), tilt (1200, 1.5), hs (14000, 1.5, 0.6) }, { { "phase", 1.0f } }));
        v.push_back (P ("Mix Bus", "Limpar lama", "Tira os 200–400 Hz só quando acumulam.",
            { lc (25, Slope::dB12), dyn (bell (280, 0, 1.0), -22, -3, 2.5, 20, 200) }, { { "phase", 1.0f } }));
        v.push_back (P ("Mix Bus", "Cola analógica", "Character Warm com curva larga e gentil.",
            { ls (90, 1.0, 0.6), bell (500, -0.7, 0.6), hs (9000, 1.2, 0.6) }, { { "character", 2.0f }, { "phase", 1.0f } }));
        v.push_back (P ("Mix Bus", "Mid/Side largo", "Mais ar nos lados, centro firme.",
            { at (lc (110, Slope::dB12), Placement::Side), at (hs (8000, 2.0, 0.7), Placement::Side), at (bell (250, -1.0, 0.8), Placement::Mid) },
            { { "procMode", 2.0f }, { "phase", 1.0f } }));

        // ---------------------------------------------------------------- Mastering
        v.push_back (P ("Mastering", "Master transparente", "Fase linear, correções largas e discretas.",
            { lc (18, Slope::dB24), ls (60, 0.5, 0.6), bell (300, -0.6, 0.7), hs (12000, 0.8, 0.6) },
            { { "phase", 2.0f }, { "lpQuality", 1.0f } }));
        v.push_back (P ("Mastering", "Master quente", "Graves arredondados, agudos macios, Character Subtle.",
            { lc (20, Slope::dB12), ls (80, 1.0, 0.6), dyn (bell (3200, 0, 1.0), -20, -1.5, 2, 10, 150), hs (15000, -0.5, 0.6) },
            { { "character", 1.0f }, { "phase", 1.0f } }));
        v.push_back (P ("Mastering", "Master M/S", "Centro mais claro, lados mais largos, graves mono.",
            { at (lc (100, Slope::dB12), Placement::Side), at (bell (2500, 0.6, 0.8), Placement::Mid), at (hs (10000, 1.2, 0.6), Placement::Side) },
            { { "procMode", 2.0f }, { "phase", 2.0f } }));
        v.push_back (P ("Mastering", "Master de-harsh", "De-harsh ligado e agudos dinâmicos.",
            { lc (20, Slope::dB12), dyn (hs (8000, 0, 0.7), -26, -2, 2, 5, 120) },
            { { "deharsh", 1.0f }, { "deharshAmt", 35.0f }, { "phase", 1.0f } }));
        v.push_back (P ("Mastering", "Streaming brilhante", "Ar e definição sem aumentar a dureza.",
            { lc (22, Slope::dB24), bell (180, -0.5, 0.8), hs (11000, 1.5, 0.6) },
            { { "deharsh", 1.0f }, { "deharshAmt", 25.0f }, { "phase", 2.0f } }));

        // ---------------------------------------------------------------- De-Resonance
        v.push_back (P ("De-Resonance", "Ressonâncias de sala", "Notches dinâmicos típicos de salas pequenas.",
            { dyn (bell (125, 0, 6), -30, -6, 4, 5, 150), dyn (bell (250, 0, 6), -30, -5, 4, 5, 150), dyn (bell (500, 0, 8), -32, -4, 4, 5, 120) }));
        v.push_back (P ("De-Resonance", "Voz anasalada", "Doma os 1–2 kHz só quando aparecem.",
            { dyn (bell (1100, 0, 4), -28, -4, 3, 3, 80), dyn (bell (1800, 0, 5), -30, -4, 3, 3, 80) }));
        v.push_back (P ("De-Resonance", "Guitarra acústica", "Tira o 'boom' do corpo e picos de cordas.",
            { lc (70, Slope::dB18), dyn (bell (190, 0, 3), -24, -5, 3, 8, 150), dyn (bell (2600, 0, 6), -30, -3, 3, 2, 60) }));
        v.push_back (P ("De-Resonance", "Ponto de partida", "Sem bandas: use Smart Ops → Deresonate.", {}));

        // ---------------------------------------------------------------- De-Ess
        v.push_back (P ("De-Ess", "De-ess voz masculina", "Sibilância à volta de 5–6 kHz.",
            { dyn (bell (5500, 0, 2.5), -30, -7, 4, 1, 50, DetectorMode::Peak) }));
        v.push_back (P ("De-Ess", "De-ess voz feminina", "Sibilância à volta de 7–8 kHz.",
            { dyn (bell (7500, 0, 2.5), -30, -7, 4, 1, 50, DetectorMode::Peak) }));
        v.push_back (P ("De-Ess", "De-ess largo", "Shelf dinâmico acima de 6 kHz — suave.",
            { dyn (hs (6000, 0, 0.7), -28, -5, 3, 1, 60, DetectorMode::Peak) }));
        v.push_back (P ("De-Ess", "De-ess duplo", "Dois pontos: 'S' e 'T'.",
            { dyn (bell (4800, 0, 3.5), -30, -5, 4, 1, 50, DetectorMode::Peak), dyn (bell (8500, 0, 3), -32, -6, 4, 1, 50, DetectorMode::Peak) }));

        // ---------------------------------------------------------------- Unmask
        v.push_back (P ("Unmask", "Voz sobre beat", "No beat; voz no sidechain. Abre 1–4 kHz quando a voz entra.",
            { dyn (bell (1500, 0, 1.2), -36, -4, 3, 5, 120, DetectorMode::RMS, DetectorSource::Sidechain),
              dyn (bell (3200, 0, 1.2), -38, -4, 3, 5, 120, DetectorMode::RMS, DetectorSource::Sidechain) }));
        v.push_back (P ("Unmask", "Bombo sobre baixo", "No baixo; bombo no sidechain. Abre o sub nas pancadas.",
            { dyn (bell (60, 0, 1.4), -30, -6, 4, 1, 90, DetectorMode::Peak, DetectorSource::Sidechain) }));
        v.push_back (P ("Unmask", "Voz sobre pads", "Nos pads/teclas; voz no sidechain.",
            { dyn (bell (800, 0, 0.9), -38, -3, 2.5, 10, 200, DetectorMode::RMS, DetectorSource::Sidechain),
              dyn (bell (2500, 0, 0.9), -40, -4, 2.5, 10, 200, DetectorMode::RMS, DetectorSource::Sidechain) }));
        v.push_back (P ("Unmask", "Ducking de graves", "Tira graves do instrumental quando o sidechain toca.",
            { dyn (ls (150, 0, 0.7), -32, -5, 4, 3, 150, DetectorMode::RMS, DetectorSource::Sidechain) }));

        // ---------------------------------------------------------------- Criativo
        v.push_back (P ("Criativo", "Rádio antigo", "Banda estreita e média projetada.",
            { lc (450, Slope::dB48), hc (3500, Slope::dB48), bell (1500, 6.0, 1.2) }, { { "character", 2.0f } }));
        v.push_back (P ("Criativo", "Telefone", "Passa-banda clássico 300–3400 Hz.",
            { lc (300, Slope::dB72), hc (3400, Slope::dB72), bell (1800, 4.0, 1.0) }));
        v.push_back (P ("Criativo", "Lo-fi abafado", "Agudos fora, graves macios.",
            { hc (5000, Slope::dB24, 1.2), ls (120, 2.0, 0.6) }, { { "character", 2.0f } }));
        v.push_back (P ("Criativo", "Wah fixo", "Pico ressonante para efeito.",
            { lc (200, Slope::dB12), bell (1200, 12.0, 6.0), hc (6000, Slope::dB12) }));
        v.push_back (P ("Criativo", "Bomba de graves (expansão)", "Realça os graves só nas pancadas fortes.",
            { dyn (ls (90, 0, 0.7), -18, 5, 2, 2, 120, DetectorMode::Peak) }));

        // ---------------------------------------------------------------- Utilitário
        v.push_back (P ("Utilitário", "Limpo (vazio)", "Sem bandas.", {}));
        v.push_back (P ("Utilitário", "Corte de rumble", "Tira subgraves inaudíveis.", { lc (30, Slope::dB48) }));
        v.push_back (P ("Utilitário", "Brickwall 20 Hz – 20 kHz", "Limita a banda ao audível.",
            { lc (20, Slope::Brickwall), hc (20000, Slope::Brickwall) }, { { "phase", 2.0f } }));
        v.push_back (P ("Utilitário", "Graves mono (M/S)", "Lados limpos abaixo de 150 Hz.",
            { at (lc (150, Slope::dB24), Placement::Side) }, { { "procMode", 2.0f } }));
        v.push_back (P ("Utilitário", "Notch 50 Hz (zumbido)", "Remove zumbido elétrico e harmónicos.",
            { notch (50, 12), notch (100, 12), notch (150, 12), notch (200, 12) }));
        v.push_back (P ("Utilitário", "Notch 60 Hz (zumbido)", "Para equipamento a 60 Hz.",
            { notch (60, 12), notch (120, 12), notch (180, 12), notch (240, 12) }));
        return v;
    }();
    return list;
}
} // namespace presets

// -----------------------------------------------------------------------------
PresetManager::PresetManager()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "EQMotion by Piradex - preferencias";
    o.folderName          = "Piradex";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (o);
}

juce::File PresetManager::userFolder() const
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Piradex").getChildFile ("EQMotion").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

juce::Array<juce::File> PresetManager::userPresets() const
{
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.eqmpreset");
    files.sort();
    return files;
}

bool PresetManager::isFavourite (const juce::String& key) const
{
    return juce::StringArray::fromLines (props->getValue ("favourites")).contains (key);
}

void PresetManager::setFavourite (const juce::String& key, bool fav)
{
    auto list = juce::StringArray::fromLines (props->getValue ("favourites"));
    list.removeString (key);
    if (fav) list.add (key);
    list.removeEmptyStrings();
    props->setValue ("favourites", list.joinIntoString ("\n"));
    props->saveIfNeeded();
}
