// =============================================================================
//  EQMotion 2.1 — biblioteca de 200 presets em 10 categorias
//  Valores desenhados de raiz para o EQMotion (não são cópias de presets de outros EQs).
//  Nome e pasta guardados em português; o inglês é o nome mostrado com a interface em EN.
// =============================================================================
#include "Presets.h"

namespace presets
{
namespace
{
constexpr Slope S12 = Slope::dB12, S18 = Slope::dB18, S24 = Slope::dB24, S36 = Slope::dB36, S48 = Slope::dB48, S72 = Slope::dB72;
constexpr DetectorMode PK = DetectorMode::Peak, RM = DetectorMode::RMS;
constexpr DetectorSource SC = DetectorSource::Sidechain;
constexpr Placement MID = Placement::Mid, SIDE = Placement::Side, LEFT = Placement::Left, RIGHT = Placement::Right;

using G = std::map<juce::String, float>;
const G MS { { "procMode", 2.0f } }, LR { { "procMode", 1.0f } };
G with (G a, const G& b) { for (auto& [k, v] : b) a[k] = v; return a; }
const G NAT { { "phase", 1.0f } }, LIN { { "phase", 2.0f } }, SUBTLE { { "character", 1.0f } }, WARM { { "character", 2.0f } };

struct Lib
{
    std::vector<FactoryPreset>& v;
    const char* folder = "";
    void operator() (const char* pt, const char* en, const char* dpt, const char* den, std::vector<BandParams> b, G g = {})
    {
        FactoryPreset p;
        p.folder = juce::String::fromUTF8 (folder);
        p.name = juce::String::fromUTF8 (pt); p.nameEn = juce::String::fromUTF8 (en);
        p.description = juce::String::fromUTF8 (dpt); p.descriptionEn = juce::String::fromUTF8 (den);
        p.bands = std::move (b); p.globals = std::move (g);
        v.push_back (std::move (p));
    }
};
} // namespace

void addLibrary (std::vector<FactoryPreset>& v)
{
    Lib x { v };

    // ======================================================================= 1. Voz (1–20)
    x.folder = "Voz";
    x ("Clareza vocal", "Vocal Clarity", "Limpa os graves, tira lama aos 250 Hz e abre os 3–4 kHz.", "Cleans the lows, takes mud out at 250 Hz and opens up 3–4 kHz.",
       { lc (90, S24), bell (250, -2.5, 1.2), bell (3500, 2.0, 0.9), hs (11000, 2.0, 0.7) });
    x ("Presença vocal", "Vocal Presence Boost", "Realce largo aos 3 kHz para a voz vir à frente.", "Broad lift at 3 kHz to bring the vocal forward.",
       { lc (80, S18), bell (3000, 3.0, 0.8), bell (5000, 1.0, 1.2) });
    x ("Ar na voz", "Vocal Air Enhancement", "Shelf de ar aos 12 kHz com a sibilância controlada.", "Air shelf at 12 kHz with the sibilance kept in check.",
       { lc (80, S12), dyn (bell (6500, 0, 3.0), -30, -4, 4, 1, 50, PK), hs (12000, 3.5, 0.6) });
    x ("Calor na voz", "Vocal Warmth", "Mais corpo aos 200 Hz e agudos ligeiramente macios.", "More body at 200 Hz and slightly softer highs.",
       { lc (70, S12), bell (200, 2.0, 0.8), hs (10000, -1.0, 0.7) }, SUBTLE);
    x ("Corpo na voz", "Vocal Body Enhancement", "Shelf de corpo aos 180 Hz sem deixar embolar os 450 Hz.", "Body shelf at 180 Hz without letting 450 Hz go muddy.",
       { lc (75, S18), ls (180, 2.5, 0.7), bell (450, -1.0, 1.2) });
    x ("Voz sem lama", "Vocal Mud Removal", "Corta os 250–350 Hz só quando acumulam.", "Cuts 250–350 Hz only when it builds up.",
       { lc (90, S24), dyn (bell (300, 0, 1.3), -26, -5, 3, 10, 120) });
    x ("Voz menos anasalada", "Vocal Nasal Reduction", "Doma 1–1,6 kHz quando a voz fica nasal.", "Tames 1–1.6 kHz when the vocal turns nasal.",
       { dyn (bell (1000, 0, 3.0), -28, -4, 3, 5, 90), dyn (bell (1600, 0, 3.5), -30, -3, 3, 5, 90) });
    x ("Dureza na voz", "Vocal Harshness Control", "Suaviza os 2,8 e 4,5 kHz nas notas fortes.", "Smooths 2.8 and 4.5 kHz on the loud notes.",
       { dyn (bell (2800, 0, 2.5), -28, -4, 3, 3, 70), dyn (bell (4500, 0, 3.0), -30, -4, 3, 2, 60) });
    x ("Redução de sibilância", "Vocal Sibilance Reduction", "De-ess aos 6,5 kHz com shelf dinâmico acima dos 10 kHz.", "De-ess at 6.5 kHz with a dynamic shelf above 10 kHz.",
       { dyn (bell (6500, 0, 2.5), -30, -7, 4, 1, 50, PK), dyn (hs (10000, 0, 0.7), -32, -3, 3, 1, 50, PK) });
    x ("Limpeza de graves na voz", "Vocal Low-Cut Cleanup", "Corte firme aos 100 Hz contra rumble, pops e proximidade.", "Firm 100 Hz cut against rumble, pops and proximity.",
       { lc (100, S36), bell (160, -1.5, 1.0) });
    x ("Brilho na voz", "Vocal High-Frequency Shine", "Brilho aos 8 kHz e ar, com De-harsh leve.", "Sheen at 8 kHz plus air, with light De-harsh.",
       { lc (80, S12), bell (8000, 2.0, 0.9), hs (14000, 2.0, 0.6) }, { { "deharsh", 1.0f }, { "deharshAmt", 20.0f } });
    x ("Inteligibilidade da voz", "Vocal Intelligibility", "Menos caixa, mais consoantes entre 2 e 4 kHz.", "Less boxiness, more consonants between 2 and 4 kHz.",
       { lc (100, S24), bell (300, -2.0, 1.0), bell (2000, 2.0, 1.0), bell (4000, 1.5, 1.2) });
    x ("Correção de proximidade", "Vocal Proximity Correction", "Shelf dinâmico aos 200 Hz para o efeito de proximidade do microfone.", "Dynamic 200 Hz shelf for microphone proximity effect.",
       { lc (70, S12), dyn (ls (200, 0, 0.7), -24, -6, 3, 10, 150) });
    x ("Ressonâncias da voz", "Vocal Resonance Tamer", "Notches dinâmicos em pontos típicos — afine com Smart Ops → Deresonate.", "Dynamic notches at typical spots — fine-tune with Smart Ops → Deresonate.",
       { dyn (bell (800, 0, 6), -30, -4, 4, 3, 80), dyn (bell (1250, 0, 6), -30, -4, 4, 3, 80), dyn (bell (2500, 0, 6), -32, -3, 4, 3, 70), dyn (bell (3150, 0, 6), -32, -3, 4, 3, 70) });
    x ("Voz sem caixa", "Vocal Boxiness Removal", "Retira o som de caixa entre 400 e 650 Hz.", "Removes the boxy sound between 400 and 650 Hz.",
       { lc (80, S18), bell (450, -3.0, 1.6), bell (650, -1.5, 2.0) });
    x ("Médios da voz", "Vocal Midrange Sculpting", "Esculpe os médios: menos 500 Hz, foco em 1,2 e 3,5 kHz.", "Shapes the mids: less 500 Hz, focus at 1.2 and 3.5 kHz.",
       { lc (80, S18), bell (500, -1.5, 1.0), bell (1200, 1.0, 1.2), bell (2200, -1.0, 2.0), bell (3500, 1.5, 1.0) });
    x ("Presença dinâmica na voz", "Vocal Dynamic Presence", "Realça os 3 kHz só quando a voz perde presença.", "Lifts 3 kHz only when the vocal loses presence.",
       { lc (80, S18), below (dyn (bell (3000, 0, 1.0), -30, 3, 2, 10, 150)) });
    x ("Realce da respiração", "Vocal Breath Enhancement", "Realça o ar nos momentos baixos — respirações mais presentes.", "Lifts the air in quiet moments — breaths come forward.",
       { lc (80, S12), below (dyn (hs (9000, 0, 0.7), -40, 4, 2, 5, 120)) });
    x ("Voz ao telefone", "Vocal Telephone Effect", "Banda estreita 400 Hz–3,2 kHz com médios projetados.", "Narrow 400 Hz–3.2 kHz band with projected mids.",
       { lc (400, S48), hc (3200, S48), bell (1500, 5.0, 1.0) }, WARM);
    x ("Voz rádio vintage", "Vocal Vintage Radio", "Tom de rádio antigo com a cor do Character Warm.", "Old radio tone with Character Warm colour.",
       { lc (250, S24), hc (5000, S24), bell (1200, 3.0, 0.9), bell (2500, 2.0, 1.4) }, WARM);

    // ======================================================================= 2. Bateria (21–40)
    x.folder = "Bateria";
    x ("Sub do bombo", "Kick Sub Enhancement", "Mais sub aos 55 Hz, com corte de rumble.", "More sub at 55 Hz, with a rumble cut.",
       { lc (25, S24), ls (55, 3.0, 0.8), bell (300, -2.0, 1.2) });
    x ("Punch do bombo", "Kick Punch Boost", "Punch aos 80 Hz, menos caixa, ataque aos 2,5 kHz.", "Punch at 80 Hz, less box, attack at 2.5 kHz.",
       { lc (30, S24), bell (80, 3.0, 1.2), bell (380, -3.0, 1.5), bell (2500, 2.0, 1.4) });
    x ("Clique do bombo", "Kick Click Definition", "Clique definido aos 4 kHz sem chiar.", "Defined click at 4 kHz without fizz.",
       { lc (30, S24), bell (4000, 4.0, 1.8), hc (12000, S12) });
    x ("Bombo sem caixa", "Kick Low-Mid Cleanup", "Tira os 300–400 Hz cartonados nas pancadas.", "Takes out the papery 300–400 Hz on the hits.",
       { lc (30, S24), dyn (bell (350, 0, 1.6), -26, -6, 4, 2, 80, PK) });
    x ("Ressonância do bombo", "Kick Resonance Control", "Encurta a cauda ressonante dos 100–120 Hz.", "Shortens the ringing 100–120 Hz tail.",
       { lc (30, S24), dyn (bell (110, 0, 4.0), -24, -5, 4, 10, 180) });
    x ("Corpo da tarola", "Snare Body Boost", "Corpo aos 200 Hz sem caixa.", "Body at 200 Hz without box.",
       { lc (80, S18), bell (200, 3.0, 1.2), bell (500, -1.0, 1.5) });
    x ("Estalo da tarola", "Snare Crack Enhancement", "Estalo aos 2 e 5 kHz.", "Crack at 2 and 5 kHz.",
       { lc (90, S18), bell (2000, 2.5, 1.4), bell (5000, 3.0, 1.2) });
    x ("Tarola sem ring", "Snare Ring Reduction", "Doma o ring entre 800 Hz e 1,6 kHz depois da pancada.", "Tames the ring between 800 Hz and 1.6 kHz after the hit.",
       { lc (80, S12), dyn (bell (850, 0, 8.0), -28, -6, 4, 2, 120, PK), dyn (bell (1600, 0, 8.0), -30, -4, 4, 2, 120, PK) });
    x ("Brilho da tarola", "Snare Brightness", "Brilho e ar entre 6 e 10 kHz.", "Brightness and air at 6–10 kHz.",
       { lc (90, S18), bell (6000, 2.0, 1.0), hs (10000, 2.5, 0.7) });
    x ("Tarola sem caixa", "Snare Boxiness Removal", "Corta a caixa dos 450 Hz.", "Cuts the 450 Hz box.",
       { lc (80, S18), bell (450, -3.5, 1.8) });
    x ("Hi-hat menos duro", "Hi-Hat Harshness Reduction", "Suaviza os 4,5 e 8 kHz agressivos.", "Smooths the aggressive 4.5 and 8 kHz.",
       { lc (300, S24), dyn (bell (4500, 0, 2.0), -30, -4, 3, 2, 60), dyn (bell (8000, 0, 2.5), -32, -4, 3, 1, 50) });
    x ("Ar no hi-hat", "Hi-Hat Air Boost", "Ar acima dos 12 kHz, graves fora.", "Air above 12 kHz, lows out.",
       { lc (400, S24), hs (12000, 3.0, 0.6) });
    x ("Corpo dos tons", "Tom Body Enhancement", "Corpo aos 120 Hz e menos caixa.", "Body at 120 Hz and less box.",
       { lc (60, S18), bell (120, 2.5, 1.2), bell (400, -2.0, 1.4) });
    x ("Ataque dos tons", "Tom Attack Enhancement", "Ataque aos 3,5 kHz e médios limpos.", "Attack at 3.5 kHz and clean mids.",
       { lc (60, S18), bell (600, -1.5, 1.2), bell (3500, 3.0, 1.4) });
    x ("Tons sem ressonância", "Tom Resonance Removal", "Doma o sustain ressonante dos tons aos 250 e 700 Hz.", "Tames the ringing tom sustain at 250 and 700 Hz.",
       { dyn (bell (250, 0, 5), -26, -4, 4, 5, 150), dyn (bell (700, 0, 6), -28, -4, 4, 5, 150) });
    x ("Pratos suaves", "Cymbal Smoothness", "Pratos sem picos duros nos 3,5 kHz e acima dos 8 kHz.", "Cymbals without harsh spikes at 3.5 kHz and above 8 kHz.",
       { lc (300, S24), dyn (bell (3500, 0, 1.5), -30, -3, 3, 3, 80), dyn (hs (8000, 0, 0.7), -30, -3, 3, 2, 80) });
    x ("Ar nos pratos", "Cymbal Air Enhancement", "Ar acima dos 14 kHz para pratos abertos.", "Air above 14 kHz for open cymbals.",
       { lc (250, S24), hs (14000, 3.0, 0.6) });
    x ("Overheads com corte", "Overhead Low-Cut", "Corte aos 180 Hz e menos caixa nos overheads.", "180 Hz cut and less box on the overheads.",
       { lc (180, S24), bell (400, -1.5, 1.0) });
    x ("Punch no bus de bateria", "Drum Bus Punch", "Graves firmes, menos caixa e ataque aos 4 kHz.", "Firm lows, less box and attack at 4 kHz.",
       { ls (70, 1.5, 0.7), bell (400, -1.5, 0.9), bell (4000, 1.5, 1.0) }, with (SUBTLE, NAT));
    x ("Equilíbrio do bus de bateria", "Drum Bus Tonal Balance", "Tilt suave, lama controlada e ar discreto.", "Gentle tilt, controlled mud and discreet air.",
       { lc (25, S12), tilt (1000, 1.0), dyn (bell (300, 0, 1.0), -24, -2, 2, 10, 150), hs (12000, 1.0, 0.6) }, NAT);

    // ======================================================================= 3. Baixo (41–60)
    x.folder = "Baixo";
    x ("Realce de subgrave", "Sub Bass Enhancement", "Shelf aos 50 Hz com corte de rumble abaixo.", "50 Hz shelf with a rumble cut below.",
       { lc (25, S36), ls (50, 3.0, 0.8) });
    x ("Extensão de graves", "Deep Bass Extension", "Prolonga os graves até aos 40 Hz.", "Extends the lows down to 40 Hz.",
       { lc (20, S24), bell (40, 2.5, 1.0), bell (250, -1.0, 1.0) });
    x ("Fundamental do baixo", "Bass Fundamental Boost", "Realce da fundamental aos 80 Hz.", "Lifts the fundamental at 80 Hz.",
       { lc (30, S24), bell (80, 3.0, 1.0) });
    x ("Punch do baixo", "Bass Punch Enhancement", "Corpo aos 120 Hz e realce do ataque aos 800 Hz nas notas.", "Body at 120 Hz and attack lift at 800 Hz on the notes.",
       { lc (30, S24), bell (120, 2.0, 1.2), dyn (bell (800, 0, 1.4), -26, 3, 2, 2, 60, PK) });
    x ("Calor do baixo", "Bass Warmth", "Calor aos 150 Hz, agudos arredondados.", "Warmth at 150 Hz, rounded highs.",
       { lc (30, S24), bell (150, 2.0, 0.8), hs (5000, -2.0, 0.7) }, WARM);
    x ("Definição de médios no baixo", "Bass Midrange Definition", "Médios aos 700 Hz e 1,4 kHz para o baixo se ouvir.", "Mids at 700 Hz and 1.4 kHz so the bass cuts through.",
       { lc (35, S24), bell (700, 2.5, 1.2), bell (1400, 1.5, 1.4) });
    x ("Harmónicos do baixo", "Bass Harmonic Presence", "Harmónicos aos 1–2 kHz para colunas pequenas.", "Harmonics at 1–2 kHz for small speakers.",
       { lc (30, S24), bell (1000, 2.5, 1.0), bell (2000, 1.5, 1.2) }, SUBTLE);
    x ("Baixo sem lama", "Bass Mud Removal", "Corta os 250 Hz só quando embolam.", "Cuts 250 Hz only when it gets muddy.",
       { lc (35, S24), dyn (bell (250, 0, 1.4), -24, -5, 3, 10, 150) });
    x ("Controlo de boom", "Bass Boom Control", "Segura o boom dos 90 Hz nas notas fortes.", "Holds the 90 Hz boom on the loud notes.",
       { lc (30, S24), dyn (bell (90, 0, 1.5), -20, -5, 3, 10, 160) });
    x ("Ressonâncias do baixo", "Bass Resonance Suppression", "Doma notas que saltam aos 100 e 200 Hz.", "Tames notes that jump out at 100 and 200 Hz.",
       { dyn (bell (100, 0, 6), -24, -4, 4, 8, 150), dyn (bell (200, 0, 6), -26, -4, 4, 8, 150) });
    x ("Graves apertados", "Bass Low-End Tightening", "Corte firme e shelf dinâmico para graves mais controlados.", "Firm cut and dynamic shelf for tighter lows.",
       { lc (35, S36), dyn (ls (80, 0, 0.7), -20, -3, 3, 10, 120) });
    x ("Rugido do baixo", "Bass Guitar Growl", "Rugido aos 900 Hz com saturação Warm.", "Growl at 900 Hz with Warm saturation.",
       { lc (40, S24), bell (900, 3.0, 1.0), bell (1800, 1.5, 1.4) }, WARM);
    x ("Ataque de palheta", "Bass Pick Attack", "Realça a palheta aos 2,5 kHz.", "Brings out the pick at 2.5 kHz.",
       { lc (40, S24), bell (400, -1.5, 1.2), bell (2500, 3.0, 1.4) });
    x ("Definição do slap", "Bass Slap Definition", "Menos caixa, slap aos 3,5 kHz e estalos controlados.", "Less box, slap at 3.5 kHz and controlled pops.",
       { lc (40, S24), bell (250, -2.0, 1.2), bell (3500, 3.0, 1.2), dyn (hs (6000, 0, 0.7), -28, -3, 3, 1, 60, PK) });
    x ("Menos ruído de dedos", "Bass Finger Noise Reduction", "Doma o ruído de dedos e cordas aos 2,8 kHz.", "Tames finger and string noise at 2.8 kHz.",
       { dyn (bell (2800, 0, 2.5), -30, -5, 4, 1, 60, PK), hc (8000, S12) });
    x ("Agudos limpos no baixo", "Bass High-Frequency Cleanup", "Corta tudo acima dos 5 kHz.", "Cuts everything above 5 kHz.",
       { lc (30, S24), hc (5000, S24) });
    x ("Graves dinâmicos", "Bass Dynamic Low-End", "Baixa os graves quando sobem e dá sub quando faltam.", "Lowers the lows when they rise and adds sub when it is lacking.",
       { lc (28, S24), dyn (ls (100, 0, 0.7), -22, -4, 3, 10, 150), below (dyn (bell (60, 0, 1.0), -30, 2.5, 2, 20, 200)) });
    x ("Baixo separado do bombo", "Bass Kick Separation", "No baixo, com o bombo no sidechain: abre os 60 Hz nas pancadas.", "On the bass, kick on the sidechain: opens 60 Hz on the hits.",
       { lc (30, S24), dyn (bell (60, 0, 1.4), -30, -6, 4, 1, 90, PK, SC) });
    x ("Baixo mono", "Bass Mono Focus", "Lados limpos abaixo dos 150 Hz.", "Clean sides below 150 Hz.",
       { at (lc (150, S24), SIDE) }, MS);
    x ("Baixo vintage", "Bass Vintage Tone", "Graves redondos, agudos fechados aos 4 kHz.", "Round lows, highs closed at 4 kHz.",
       { lc (40, S18), bell (100, 2.0, 1.0), hc (4000, S12) }, WARM);

    // ======================================================================= 4. Guitarras (61–80)
    x.folder = "Guitarras";
    x ("Calor na acústica", "Acoustic Guitar Warmth", "Corpo aos 200 Hz e agudos ligeiramente macios.", "Body at 200 Hz and slightly softer highs.",
       { lc (70, S18), bell (200, 1.5, 0.9), hs (9000, -1.0, 0.7) }, SUBTLE);
    x ("Clareza na acústica", "Acoustic Guitar Clarity", "Menos 300 Hz, mais definição aos 3 kHz.", "Less 300 Hz, more definition at 3 kHz.",
       { lc (90, S24), bell (300, -2.0, 1.0), bell (3000, 2.0, 1.0) });
    x ("Brilho na acústica", "Acoustic Guitar Sparkle", "Brilho das cordas acima dos 10 kHz.", "String sparkle above 10 kHz.",
       { lc (90, S18), bell (6000, 1.0, 1.2), hs (10000, 3.0, 0.6) });
    x ("Corpo da acústica", "Acoustic Guitar Body", "Corpo aos 150 Hz sem caixa aos 500 Hz.", "Body at 150 Hz without box at 500 Hz.",
       { lc (70, S18), bell (150, 2.5, 1.0), bell (500, -1.0, 1.4) });
    x ("Acústica sem boom", "Acoustic Guitar Boom Removal", "Tira o boom do corpo aos 180 Hz quando ressoa.", "Removes the body boom at 180 Hz when it rings.",
       { lc (80, S24), dyn (bell (180, 0, 2.5), -24, -6, 3, 8, 150) });
    x ("Palhetada na acústica", "Acoustic Guitar Pick Definition", "Definição da palheta aos 4 kHz com picos controlados.", "Pick definition at 4 kHz with controlled spikes.",
       { lc (90, S18), dyn (bell (2500, 0, 3), -30, -3, 3, 2, 60), bell (4000, 2.5, 1.4) });
    x ("Presença na elétrica", "Electric Guitar Presence", "Presença larga aos 2,5 kHz.", "Broad presence at 2.5 kHz.",
       { lc (90, S24), bell (2500, 2.5, 0.9), hc (12000, S12) });
    x ("Mordida da elétrica", "Electric Guitar Bite", "Mordida aos 1,8 kHz e 4,5 kHz.", "Bite at 1.8 kHz and 4.5 kHz.",
       { lc (100, S24), bell (1800, 3.0, 1.4), bell (4500, 1.5, 1.4) });
    x ("Calor na elétrica", "Electric Guitar Warmth", "Corpo aos 250 Hz e dureza controlada.", "Body at 250 Hz and controlled harshness.",
       { lc (80, S18), bell (250, 2.0, 0.9), dyn (bell (3500, 0, 1.4), -28, -3, 3, 3, 80) }, WARM);
    x ("Elétrica sem lama", "Electric Guitar Mud Cleanup", "Limpa os 250–500 Hz de guitarras distorcidas.", "Cleans 250–500 Hz on distorted guitars.",
       { lc (100, S24), dyn (bell (250, 0, 1.2), -24, -4, 3, 10, 150), bell (500, -1.5, 1.2) });
    x ("Elétrica menos dura", "Electric Guitar Harshness Reduction", "Suaviza os 3 e 5 kHz e fecha o fizz.", "Smooths 3 and 5 kHz and closes the fizz.",
       { dyn (bell (3000, 0, 2.0), -28, -4, 3, 3, 70), dyn (bell (5000, 0, 2.5), -30, -4, 3, 2, 60), hc (11000, S12) });
    x ("Ressonância da coluna", "Guitar Cabinet Resonance Control", "Doma o boom da coluna e o pico do altifalante.", "Tames cabinet boom and speaker peak.",
       { lc (90, S18), dyn (bell (120, 0, 4), -24, -4, 4, 8, 150), dyn (bell (2200, 0, 5), -28, -3, 4, 3, 80) });
    x ("Foco nos médios", "Guitar Midrange Focus", "Guitarra centrada nos 800 Hz.", "Guitar centred on 800 Hz.",
       { lc (120, S24), bell (800, 2.0, 0.9), hc (9000, S12) });
    x ("Agudos suaves na guitarra", "Guitar High-End Smoothing", "Agudos mais macios acima dos 7 kHz.", "Softer highs above 7 kHz.",
       { hs (7000, -2.5, 0.7), hc (13000, S12) });
    x ("Guitarra apertada", "Guitar Low-Cut Tightening", "Corte aos 120 Hz para dar espaço ao baixo.", "120 Hz cut to leave room for the bass.",
       { lc (120, S36), bell (200, -1.0, 1.0) });
    x ("Guitarras separadas (L/R)", "Guitar Stereo Separation", "Modo Esq./Dir.: curvas complementares para separar duas guitarras.", "Left/Right mode: complementary curves to separate two guitars.",
       { at (bell (1500, 1.5, 1.0), LEFT), at (bell (1500, -1.5, 1.0), RIGHT), at (bell (3000, -1.0, 1.0), LEFT), at (bell (3000, 1.5, 1.0), RIGHT) }, LR);
    x ("Guitarra solo à frente", "Guitar Lead Focus", "Solo à frente: 1,2 e 3 kHz com ar.", "Lead forward: 1.2 and 3 kHz with air.",
       { lc (150, S24), bell (1200, 2.0, 1.0), bell (3000, 2.0, 1.0), hs (9000, 1.0, 0.7) });
    x ("Guitarra rítmica recuada", "Guitar Rhythm Carving", "Recua a rítmica; com a voz no sidechain abre os 1,5 kHz.", "Pushes the rhythm back; with the vocal on the sidechain it opens 1.5 kHz.",
       { lc (150, S24), dyn (bell (1500, 0, 1.0), -36, -3, 2.5, 10, 150, RM, SC), bell (2500, -2.0, 0.9) });
    x ("Guitarra vintage", "Guitar Vintage Roll-Off", "Agudos fechados aos 6 kHz e médios redondos.", "Highs rolled off at 6 kHz and round mids.",
       { lc (80, S12), bell (800, 1.5, 1.0), hc (6000, S12) }, WARM);
    x ("Guitarra lo-fi", "Guitar Lo-Fi Tone", "Banda estreita 300 Hz–3,5 kHz.", "Narrow 300 Hz–3.5 kHz band.",
       { lc (300, S24), hc (3500, S24), bell (1000, 3.0, 1.0) }, WARM);

    // ======================================================================= 5. Teclas e Synths (81–100)
    x.folder = "Teclas e Synths";
    x ("Calor no piano", "Piano Warmth", "Corpo aos 250 Hz e agudos suaves.", "Body at 250 Hz and soft highs.",
       { lc (35, S12), bell (250, 1.5, 0.8), hs (9000, -1.0, 0.7) }, SUBTLE);
    x ("Clareza no piano", "Piano Clarity", "Menos 350 Hz, definição aos 3 kHz.", "Less 350 Hz, definition at 3 kHz.",
       { lc (40, S12), bell (350, -2.0, 1.0), bell (3000, 1.5, 1.0) });
    x ("Ataque dos martelos", "Piano Hammer Attack", "Realça o ataque aos 2,5–4 kHz nas notas.", "Lifts the attack at 2.5–4 kHz on the notes.",
       { lc (40, S12), bell (2500, 2.5, 1.2), dyn (bell (4000, 0, 1.4), -28, 2, 2, 1, 50, PK) });
    x ("Piano sem graves a mais", "Piano Low-End Cleanup", "Corte aos 60 Hz e graves dinâmicos aos 150 Hz.", "60 Hz cut and dynamic lows at 150 Hz.",
       { lc (60, S24), dyn (bell (150, 0, 1.2), -24, -4, 3, 10, 150) });
    x ("Ar no piano", "Piano Air Enhancement", "Ar acima dos 11 kHz.", "Air above 11 kHz.",
       { lc (40, S12), hs (11000, 2.5, 0.6) });
    x ("Ressonâncias do piano de cauda", "Grand Piano Resonance Control", "Doma as ressonâncias da caixa aos 200, 600 e 1200 Hz.", "Tames body resonances at 200, 600 and 1200 Hz.",
       { dyn (bell (200, 0, 4), -26, -3, 3, 10, 150), dyn (bell (600, 0, 5), -28, -3, 3, 8, 150), dyn (bell (1200, 0, 5), -30, -3, 3, 5, 120) });
    x ("Calor no Rhodes", "Rhodes Warmth", "Corpo aos 220 Hz e agudos arredondados.", "Body at 220 Hz and rounded highs.",
       { lc (50, S18), bell (220, 2.0, 0.8), hs (6000, -2.0, 0.7) }, WARM);
    x ("Sino do Rhodes", "Rhodes Bell Presence", "Realça o 'sino' aos 1,6 e 3,2 kHz.", "Brings out the bell tone at 1.6 and 3.2 kHz.",
       { lc (60, S18), bell (1600, 2.5, 1.2), bell (3200, 1.5, 1.4) });
    x ("Mordida do Wurlitzer", "Wurlitzer Midrange Bite", "Mordida aos 1 kHz e agudos macios.", "Bite at 1 kHz and soft highs.",
       { lc (80, S18), bell (1000, 3.0, 1.2), hs (8000, -1.5, 0.7) }, SUBTLE);
    x ("Graves do órgão", "Organ Low-End Control", "Shelf dinâmico aos 120 Hz para os pedais e drawbars.", "Dynamic 120 Hz shelf for pedals and drawbars.",
       { lc (40, S24), dyn (ls (120, 0, 0.7), -22, -4, 3, 15, 200) });
    x ("Presença do synth lead", "Synth Lead Presence", "Presença aos 2,5 kHz e picos controlados aos 5 kHz.", "Presence at 2.5 kHz and controlled peaks at 5 kHz.",
       { lc (150, S24), bell (2500, 2.5, 1.0), dyn (bell (5000, 0, 2.0), -28, -3, 3, 2, 60) });
    x ("Calor no pad", "Synth Pad Warmth", "Pad mais quente e agudos recuados.", "Warmer pad with recessed highs.",
       { lc (80, S18), bell (300, 1.5, 0.7), hs (8000, -2.0, 0.6) }, WARM);
    x ("Ar no pad", "Synth Pad Air", "Pad aberto: menos 500 Hz e ar acima dos 10 kHz.", "Open pad: less 500 Hz and air above 10 kHz.",
       { lc (150, S24), bell (500, -1.5, 0.8), hs (10000, 3.0, 0.6) });
    x ("Definição do synth bass", "Synth Bass Definition", "Médio-grave limpo e definição aos 1,2 kHz.", "Clean low mids and definition at 1.2 kHz.",
       { lc (30, S24), bell (200, -2.0, 1.0), bell (1200, 2.0, 1.2) });
    x ("Ressonâncias do synth", "Synth Resonance Suppression", "Doma picos de filtro ressonante aos 800 Hz, 1,6 e 3,2 kHz.", "Tames resonant filter peaks at 800 Hz, 1.6 and 3.2 kHz.",
       { dyn (bell (800, 0, 6), -28, -4, 4, 3, 80), dyn (bell (1600, 0, 6), -30, -4, 4, 3, 80), dyn (bell (3200, 0, 6), -32, -4, 4, 2, 70) });
    x ("Synth com agudos suaves", "Synth High-End Smoothing", "Shelf dinâmico acima dos 6 kHz e corte aos 15 kHz.", "Dynamic shelf above 6 kHz and cut at 15 kHz.",
       { dyn (hs (6000, 0, 0.7), -28, -4, 3, 2, 80), hc (15000, S12) });
    x ("Espaço estéreo do synth", "Synth Stereo Space", "Lados sem graves e com brilho, centro limpo.", "Sides without lows and with sheen, clean centre.",
       { at (lc (200, S12), SIDE), at (hs (6000, 2.5, 0.7), SIDE), at (bell (800, -1.0, 0.8), MID) }, with (MS, NAT));
    x ("Synth com médios abertos", "Synth Midrange Carving", "Cava os 1,5–3 kHz para a voz respirar.", "Carves 1.5–3 kHz so the vocal can breathe.",
       { lc (100, S18), bell (1500, -3.0, 0.8), bell (3000, -1.5, 1.0) });
    x ("Teclas vintage", "Keyboard Vintage Tone", "Agudos fechados e médios redondos.", "Closed highs and round mids.",
       { lc (60, S12), bell (500, 1.5, 0.9), hc (7000, S12) }, WARM);
    x ("Synth moderno brilhante", "Synth Modern Brightness", "Brilho acima dos 8 kHz com De-harsh.", "Brightness above 8 kHz with De-harsh.",
       { lc (60, S18), bell (300, -1.5, 1.0), hs (8000, 3.0, 0.6) }, { { "deharsh", 1.0f }, { "deharshAmt", 25.0f } });

    // ======================================================================= 6. Dinâmica e Ressonâncias (101–120)
    x.folder = "Dinâmica e Ressonâncias";
    x ("Low cut dinâmico", "Dynamic Low-Cut", "Shelf grave dinâmico que só limpa os graves quando sobem.", "Dynamic low shelf that only cleans the lows when they rise.",
       { dyn (ls (100, 0, 0.7), -30, -12, 4, 10, 150) });
    x ("Controlo dinâmico dos graves", "Dynamic Bass Control", "Shelf dinâmico aos 120 Hz.", "Dynamic shelf at 120 Hz.",
       { dyn (ls (120, 0, 0.7), -22, -5, 3, 10, 150) });
    x ("Lama dinâmica", "Dynamic Mud Reduction", "Corta os 300 Hz só quando acumulam.", "Cuts 300 Hz only when it builds up.",
       { dyn (bell (300, 0, 1.2), -24, -5, 3, 10, 140) });
    x ("Caixa dinâmica", "Dynamic Boxiness Removal", "Corta os 500 Hz quando soam a caixa.", "Cuts 500 Hz when it sounds boxy.",
       { dyn (bell (500, 0, 1.6), -26, -5, 3, 8, 120) });
    x ("Nasal dinâmico", "Dynamic Nasal Control", "Doma os 1,2 kHz quando ficam nasais.", "Tames 1.2 kHz when it turns nasal.",
       { dyn (bell (1200, 0, 2.5), -28, -4, 3, 5, 90) });
    x ("Presença dinâmica (realce)", "Dynamic Presence Enhancement", "Realça os 3 kHz quando falta presença.", "Lifts 3 kHz when presence is lacking.",
       { below (dyn (bell (3000, 0, 1.0), -32, 3, 2, 10, 150)) });
    x ("Dureza dinâmica", "Dynamic Harshness Reduction", "Doma os 3,5 kHz nos momentos fortes.", "Tames 3.5 kHz on the loud moments.",
       { dyn (bell (3500, 0, 1.8), -28, -5, 3, 3, 70) });
    x ("Sibilância dinâmica", "Dynamic Sibilance Control", "De-ess rápido aos 7 kHz.", "Fast de-ess at 7 kHz.",
       { dyn (bell (7000, 0, 2.0), -30, -7, 4, 1, 50, PK) });
    x ("Shelf agudo dinâmico", "Dynamic High-Shelf Control", "Shelf acima dos 8 kHz que só desce quando o brilho sobe.", "Shelf above 8 kHz that only drops when the brightness rises.",
       { dyn (hs (8000, 0, 0.7), -28, -4, 3, 3, 90) });
    x ("Ar dinâmico", "Dynamic Air Enhancement", "Ar acima dos 12 kHz só quando falta.", "Air above 12 kHz only when it is lacking.",
       { below (dyn (hs (12000, 0, 0.6), -40, 3, 2, 10, 200)) });
    x ("Supressão adaptativa de ressonâncias", "Adaptive Resonance Suppression", "Cinco cortes dinâmicos de 1 oitava em 1 oitava — afine com Smart Ops → Deresonate.", "Five dynamic cuts spaced an octave apart — fine-tune with Smart Ops → Deresonate.",
       { dyn (bell (315, 0, 5), -30, -4, 4, 3, 80), dyn (bell (630, 0, 5), -30, -4, 4, 3, 80), dyn (bell (1250, 0, 5), -30, -4, 4, 3, 80),
         dyn (bell (2500, 0, 5), -32, -4, 4, 2, 70), dyn (bell (5000, 0, 5), -34, -4, 4, 2, 60) });
    x ("Notches estreitos", "Narrow Resonance Notching", "Quatro notches dinâmicos estreitos: arraste-os para as ressonâncias.", "Four narrow dynamic notches: drag them onto the resonances.",
       { dyn (bell (440, 0, 12), -30, -6, 4, 2, 80), dyn (bell (880, 0, 12), -30, -6, 4, 2, 80), dyn (bell (1760, 0, 12), -32, -6, 4, 2, 70), dyn (bell (3520, 0, 12), -34, -6, 4, 2, 60) });
    x ("Suavização larga de ressonâncias", "Broadband Resonance Smoothing", "De-harsh e dois cortes largos dinâmicos.", "De-harsh plus two broad dynamic cuts.",
       { dyn (bell (1000, 0, 0.7), -26, -2, 2, 5, 100), dyn (bell (4000, 0, 0.7), -28, -2, 2, 3, 80) }, { { "deharsh", 1.0f }, { "deharshAmt", 50.0f } });
    x ("Ducking seletivo", "Frequency-Selective Ducking", "Baixa só os 1 kHz quando o sidechain toca.", "Ducks only 1 kHz while the sidechain plays.",
       { dyn (bell (1000, 0, 1.0), -36, -6, 4, 5, 150, RM, SC) });
    x ("Ducking do baixo pelo bombo", "Kick-Triggered Bass Ducking", "No baixo, bombo no sidechain: os graves saem do caminho nas pancadas.", "On the bass, kick on the sidechain: the lows step aside on the hits.",
       { dyn (ls (120, 0, 0.7), -30, -8, 4, 1, 100, PK, SC) });
    x ("Ducking do instrumental pela voz", "Vocal-Triggered Instrument Ducking", "No instrumental, voz no sidechain: abre os 2–4 kHz quando a voz entra.", "On the instrumental, vocal on the sidechain: opens 2–4 kHz when the vocal comes in.",
       { dyn (bell (2000, 0, 0.8), -38, -4, 3, 5, 150, RM, SC), dyn (bell (4000, 0, 1.0), -40, -3, 3, 5, 150, RM, SC) });
    x ("Médios dinâmicos", "Dynamic Midrange Control", "Controlo largo dos 800 Hz e 2 kHz.", "Broad control of 800 Hz and 2 kHz.",
       { dyn (bell (800, 0, 0.8), -24, -4, 3, 8, 120), dyn (bell (2000, 0, 1.0), -26, -3, 3, 5, 100) });
    x ("Subgrave dinâmico", "Dynamic Sub-Frequency Control", "Segura os 45 Hz quando o sub dispara.", "Holds 45 Hz when the sub spikes.",
       { lc (20, S24), dyn (bell (45, 0, 1.2), -20, -5, 3, 15, 200) });
    x ("Transientes tonais", "Dynamic Transient Tonal Shaping", "Mais brilho no ataque e menos graves-médios no sustain.", "More brightness on the attack, fewer low mids on the sustain.",
       { dyn (bell (200, 0, 1.0), -24, -2, 2, 10, 120), dyn (bell (4000, 0, 1.0), -30, 3, 2, 1, 40, PK) });
    x ("Equilíbrio espectral adaptativo", "Adaptive Spectral Balance", "Quatro zonas largas que se seguram quando sobem.", "Four broad ranges that hold back when they rise.",
       { dyn (ls (120, 0, 0.7), -24, -3, 2, 20, 250), dyn (bell (400, 0, 0.8), -26, -2, 2, 20, 250), dyn (bell (2500, 0, 0.8), -28, -2, 2, 10, 200),
         dyn (hs (8000, 0, 0.7), -30, -3, 2, 10, 200) }, NAT);

    // ======================================================================= 7. Estéreo e M/S (121–140)
    x.folder = "Estéreo e M/S";
    x ("Graves focados no Mid", "Mid Channel Low-End Focus", "Graves no centro, lados limpos abaixo dos 120 Hz.", "Lows in the centre, sides clean below 120 Hz.",
       { at (ls (100, 1.5, 0.7), MID), at (lc (120, S24), SIDE) }, MS);
    x ("Low cut nos lados", "Side Channel Low-Cut", "Corta os lados abaixo dos 150 Hz.", "Cuts the sides below 150 Hz.",
       { at (lc (150, S24), SIDE) }, MS);
    x ("Presença no Mid", "Mid Channel Presence Boost", "Presença aos 3 kHz só no centro.", "Presence at 3 kHz in the centre only.",
       { at (bell (3000, 2.0, 0.9), MID) }, MS);
    x ("Ar nos lados", "Side Channel Air Boost", "Ar acima dos 10 kHz só nos lados.", "Air above 10 kHz on the sides only.",
       { at (hs (10000, 2.5, 0.6), SIDE) }, MS);
    x ("Largura tonal", "Stereo Width Tonal Enhancement", "Lados mais brilhantes e sem graves: imagem mais larga.", "Brighter sides without lows: a wider image.",
       { at (lc (120, S12), SIDE), at (bell (3000, 1.5, 0.8), SIDE), at (hs (9000, 2.0, 0.6), SIDE), at (bell (3000, -0.5, 0.8), MID) }, with (MS, NAT));
    x ("Graves estéreo limpos", "Stereo Low-End Cleanup", "Rumble fora e lados limpos abaixo dos 120 Hz.", "Rumble out and sides clean below 120 Hz.",
       { lc (25, S24), at (lc (120, S24), SIDE) }, MS);
    x ("Graves separados M/S", "Mid/Side Bass Separation", "Graves firmes no centro, nada nos lados abaixo dos 200 Hz.", "Firm lows in the centre, nothing on the sides below 200 Hz.",
       { at (ls (80, 1.0, 0.7), MID), at (bell (250, -1.5, 1.0), MID), at (lc (200, S24), SIDE) }, MS);
    x ("Voz clara em M/S", "Mid/Side Vocal Clarity", "Voz central mais clara; lados recuam aos 3 kHz.", "Clearer centre vocal; sides step back at 3 kHz.",
       { at (bell (300, -1.5, 1.0), MID), at (bell (3000, 2.0, 1.0), MID), at (bell (3000, -1.5, 1.0), SIDE) }, MS);
    x ("Agudos equilibrados M/S", "Mid/Side High-Frequency Balance", "Agudos do centro para os lados.", "Moves top end from the centre to the sides.",
       { at (hs (8000, -1.0, 0.7), MID), at (hs (8000, 1.5, 0.7), SIDE) }, MS);
    x ("Ressonâncias nos lados", "Stereo Resonance Control", "Cortes dinâmicos aos 2,5 e 5 kHz só nos lados.", "Dynamic cuts at 2.5 and 5 kHz on the sides only.",
       { at (dyn (bell (2500, 0, 4), -32, -4, 3, 3, 80), SIDE), at (dyn (bell (5000, 0, 4), -34, -4, 3, 2, 70), SIDE) }, MS);
    x ("Correção do canal esquerdo", "Left Channel EQ Correction", "Modo Esq./Dir.: bandas só no canal esquerdo — ajuste ao desvio.", "Left/Right mode: bands on the left channel only — adjust to the offset.",
       { at (bell (400, -1.0, 1.0), LEFT), at (hs (8000, 1.0, 0.7), LEFT) }, LR);
    x ("Correção do canal direito", "Right Channel EQ Correction", "Modo Esq./Dir.: bandas só no canal direito — ajuste ao desvio.", "Left/Right mode: bands on the right channel only — adjust to the offset.",
       { at (bell (400, -1.0, 1.0), RIGHT), at (hs (8000, 1.0, 0.7), RIGHT) }, LR);
    x ("Igualar tom L/R", "Stereo Image Tonal Matching", "Tilts opostos e pequenos em L e R para uma imagem desequilibrada.", "Small opposite tilts on L and R for a lopsided image.",
       { at (tilt (1000, 0.5), LEFT), at (tilt (1000, -0.5), RIGHT) }, LR);
    x ("Médios mais largos", "Midrange Width Enhancement", "Mais médios nos lados, menos no centro.", "More mids on the sides, less in the centre.",
       { at (bell (1000, 2.0, 0.8), SIDE), at (bell (1000, -0.7, 0.8), MID) }, MS);
    x ("Agudos mais largos", "Stereo High-Shelf Expansion", "Shelf acima dos 6 kHz nos lados.", "Shelf above 6 kHz on the sides.",
       { at (hs (6000, 2.5, 0.7), SIDE) }, MS);
    x ("Menos graves-médios nos lados", "Stereo Low-Mid Reduction", "Limpa os 300 Hz dos lados.", "Cleans 300 Hz on the sides.",
       { at (bell (300, -2.5, 0.9), SIDE) }, MS);
    x ("Dureza nos lados", "Side Channel Harshness Control", "Doma os 3,5 kHz dos lados nos momentos fortes.", "Tames the sides' 3.5 kHz on the loud moments.",
       { at (dyn (bell (3500, 0, 1.5), -32, -4, 3, 3, 70), SIDE) }, MS);
    x ("Calor no Mid", "Mid Channel Warmth", "Calor aos 200 Hz no centro.", "Warmth at 200 Hz in the centre.",
       { at (bell (200, 1.5, 0.8), MID) }, with (MS, SUBTLE));
    x ("Profundidade estéreo", "Stereo Depth Enhancement", "Lados mais escuros recuam, centro à frente: sensação de profundidade.", "Darker sides step back, centre forward: a sense of depth.",
       { at (bell (2500, 1.0, 1.0), MID), at (bell (2500, -1.0, 1.0), SIDE), at (hs (8000, -1.0, 0.7), SIDE) }, MS);
    x ("Foco estéreo por frequência", "Frequency-Dependent Stereo Focus", "Graves mono, médios contidos, agudos largos.", "Mono lows, contained mids, wide highs.",
       { at (lc (180, S24), SIDE), at (bell (600, -1.0, 0.8), SIDE), at (hs (5000, 2.0, 0.7), SIDE) }, MS);

    // ======================================================================= 8. Mistura e Masking (141–160)
    x.folder = "Mistura e Masking";
    x ("Voz sobre guitarra", "Vocal vs Guitar Unmasking", "Na guitarra, voz no sidechain: abre os 2,5 kHz quando a voz canta.", "On the guitar, vocal on the sidechain: opens 2.5 kHz while the vocal sings.",
       { lc (120, S24), dyn (bell (2500, 0, 1.0), -38, -4, 3, 5, 150, RM, SC) });
    x ("Voz sobre synth", "Vocal vs Synth Unmasking", "No synth, voz no sidechain: abre os 1,5 e 3,5 kHz.", "On the synth, vocal on the sidechain: opens 1.5 and 3.5 kHz.",
       { dyn (bell (1500, 0, 0.9), -38, -3, 3, 5, 150, RM, SC), dyn (bell (3500, 0, 1.0), -40, -3, 3, 5, 150, RM, SC) });
    x ("Bombo e baixo separados", "Kick vs Bass Separation", "No baixo: cava o espaço do bombo; com o bombo no sidechain abre mais nas pancadas.", "On the bass: carves room for the kick; with the kick on the sidechain it opens more on the hits.",
       { lc (30, S24), bell (60, -2.0, 1.4), bell (100, 1.5, 1.2), dyn (bell (55, 0, 1.4), -30, -4, 4, 1, 90, PK, SC) });
    x ("Tarola e guitarra separadas", "Snare vs Guitar Separation", "Na guitarra, tarola no sidechain: abre os 200 Hz e 5 kHz nas pancadas.", "On the guitar, snare on the sidechain: opens 200 Hz and 5 kHz on the hits.",
       { dyn (bell (200, 0, 1.2), -32, -3, 3, 1, 80, PK, SC), dyn (bell (5000, 0, 1.2), -34, -3, 3, 1, 80, PK, SC) });
    x ("Piano e voz separados", "Piano vs Vocal Separation", "No piano: menos 2,5 kHz e, com a voz no sidechain, menos 1,2 kHz quando ela canta.", "On the piano: less 2.5 kHz and, with the vocal on the sidechain, less 1.2 kHz while it sings.",
       { bell (2500, -1.5, 0.9), dyn (bell (1200, 0, 0.9), -38, -3, 3, 5, 150, RM, SC) });
    x ("Baixo e teclas separados", "Bass vs Keyboard Separation", "Nas teclas: tira os graves que pertencem ao baixo.", "On the keys: removes the lows that belong to the bass.",
       { lc (120, S24), bell (250, -2.0, 1.0) });
    x ("Guitarra e teclas separadas", "Guitar vs Keyboard Separation", "Nas teclas: cava os 1,5 kHz e realça os 3 kHz. Na guitarra, faça o oposto.", "On the keys: carves 1.5 kHz and lifts 3 kHz. Do the opposite on the guitar.",
       { lc (80, S12), bell (1500, -2.0, 1.0), bell (3000, 1.0, 1.0) });
    x ("Hi-hat e sibilância separados", "Hi-Hat vs Vocal Sibilance Separation", "No hi-hat, voz no sidechain: recua os 7 kHz nos 'S'.", "On the hi-hat, vocal on the sidechain: pulls back 7 kHz on the 'S' sounds.",
       { lc (300, S24), dyn (bell (7000, 0, 1.5), -36, -4, 3, 1, 60, PK, SC) });
    x ("Lead e pad separados", "Lead vs Pad Separation", "No pad: menos 2,5 kHz e, com o lead no sidechain, abre os 1,2 kHz.", "On the pad: less 2.5 kHz and, with the lead on the sidechain, opens 1.2 kHz.",
       { bell (2500, -2.0, 0.8), dyn (bell (1200, 0, 0.8), -38, -3, 3, 10, 200, RM, SC) });
    x ("Espaço para os coros", "Backing Vocal Space Carving", "Nos coros: menos graves e presença, mais ar — ficam atrás da principal.", "On backing vocals: fewer lows and less presence, more air — they sit behind the lead.",
       { lc (180, S24), bell (3000, -2.0, 0.9), hs (10000, 1.0, 0.7) });
    x ("Médios do instrumental limpos", "Instrumental Midrange Cleanup", "Limpa os 400 Hz e 1 kHz e segura os 2,5 kHz.", "Cleans 400 Hz and 1 kHz and holds 2.5 kHz.",
       { bell (400, -1.5, 0.9), bell (1000, -1.0, 1.0), dyn (bell (2500, 0, 0.9), -30, -2, 2, 5, 150) });
    x ("Descongestionar graves-médios", "Mix Low-Mid Decongestion", "Corte dinâmico largo aos 250 Hz e leve aos 500 Hz.", "Broad dynamic cut at 250 Hz and a light one at 500 Hz.",
       { dyn (bell (250, 0, 1.0), -22, -3, 2.5, 15, 200), bell (500, -0.8, 0.9) }, NAT);
    x ("Gestão da presença", "Mix Presence Management", "Segura os 3 kHz nos picos e dá definição aos 5 kHz.", "Holds 3 kHz on the peaks and adds definition at 5 kHz.",
       { dyn (bell (3000, 0, 1.0), -26, -2.5, 2.5, 5, 120), bell (5000, 0.8, 1.0) }, NAT);
    x ("Agudos da mistura suaves", "Mix High-Frequency Smoothing", "Shelf dinâmico acima dos 7 kHz e De-harsh leve.", "Dynamic shelf above 7 kHz and light De-harsh.",
       { dyn (hs (7000, 0, 0.7), -28, -2.5, 2.5, 3, 100) }, with (NAT, { { "deharsh", 1.0f }, { "deharshAmt", 25.0f } }));
    x ("Contraste tonal", "Mix Tonal Contrast", "Graves e ar à frente, médios recuados.", "Lows and air forward, mids recessed.",
       { ls (80, 1.5, 0.7), bell (500, -1.5, 0.8), hs (10000, 1.5, 0.6) });
    x ("Fatias de frequência", "Frequency Slot Allocation", "Quatro fatias de 2 oitavas: baixe as que pertencem a outro instrumento.", "Four 2-octave slots: lower the ones that belong to another instrument.",
       { bell (100, -1.5, 1.5), bell (400, -1.5, 1.5), bell (1600, -1.5, 1.5), bell (6400, -1.5, 1.5) });
    x ("EQ complementar", "Complementary EQ Pairing", "Par A: +500 Hz / −2 kHz. No outro instrumento use o oposto.", "Pair A: +500 Hz / −2 kHz. Use the opposite on the other instrument.",
       { bell (500, 1.5, 1.0), bell (2000, -1.5, 1.0) });
    x ("Masking dinâmico", "Dynamic Masking Reduction", "Com o outro instrumento no sidechain: abre 300 Hz, 1,2 e 3 kHz só quando ele toca.", "With the other instrument on the sidechain: opens 300 Hz, 1.2 and 3 kHz only while it plays.",
       { dyn (bell (300, 0, 0.9), -36, -3, 3, 5, 150, RM, SC), dyn (bell (1200, 0, 0.9), -38, -3, 3, 5, 150, RM, SC), dyn (bell (3000, 0, 0.9), -40, -3, 3, 5, 150, RM, SC) });
    x ("Escavar pelo sidechain", "Sidechain Frequency Carving", "Corte largo aos 2 kHz comandado pelo sidechain.", "Broad 2 kHz cut keyed by the sidechain.",
       { dyn (bell (2000, 0, 0.7), -40, -5, 4, 5, 150, RM, SC) });
    x ("Menos sobreposição espectral", "Spectral Overlap Reduction", "Menos 250 Hz fixo e cortes pelo sidechain aos 800 Hz e 3 kHz.", "Fixed 250 Hz cut and sidechain cuts at 800 Hz and 3 kHz.",
       { bell (250, -1.5, 1.0), dyn (bell (800, 0, 1.0), -36, -3, 3, 5, 150, RM, SC), dyn (bell (3000, 0, 1.0), -38, -3, 3, 5, 150, RM, SC) });

    // ======================================================================= 9. Mastering (161–180)
    x.folder = "Mastering";
    x ("Equilíbrio tonal de master", "Mastering Tonal Balance", "Correções largas e discretas em fase linear.", "Broad, discreet corrections in linear phase.",
       { lc (18, S24), ls (80, 0.5, 0.6), bell (350, -0.5, 0.7), bell (3000, 0.4, 0.8), hs (12000, 0.8, 0.6) }, LIN);
    x ("Graves apertados (master)", "Mastering Low-End Tightening", "Corte aos 22 Hz e shelf dinâmico suave aos 90 Hz.", "22 Hz cut and gentle dynamic shelf at 90 Hz.",
       { lc (22, S24), dyn (ls (90, 0, 0.7), -20, -2, 2, 15, 150) }, LIN);
    x ("Subgrave limpo (master)", "Mastering Sub-Bass Cleanup", "Rumble fora e lados mono abaixo dos 100 Hz.", "Rumble out and mono sides below 100 Hz.",
       { lc (28, S48), at (lc (100, S12), SIDE) }, with (MS, LIN));
    x ("Calor nos graves (master)", "Mastering Bass Warmth", "Shelf aos 100 Hz e Character Subtle.", "100 Hz shelf and Character Subtle.",
       { ls (100, 1.0, 0.6), bell (220, 0.5, 0.8) }, with (SUBTLE, NAT));
    x ("Correção de graves-médios", "Mastering Low-Mid Correction", "Menos 300 Hz fixo e controlo dinâmico aos 250 Hz.", "Fixed 300 Hz dip and dynamic control at 250 Hz.",
       { bell (300, -1.0, 0.8), dyn (bell (250, 0, 1.0), -24, -1.5, 2, 20, 200) }, LIN);
    x ("Clareza de médios (master)", "Mastering Midrange Clarity", "Menos 600 Hz, mais 2 kHz — muito subtil.", "Less 600 Hz, more 2 kHz — very subtle.",
       { bell (600, -0.7, 0.8), bell (2000, 0.6, 0.8) }, LIN);
    x ("Presença (master)", "Mastering Presence Enhancement", "Presença aos 3,5 kHz que se segura nos picos.", "Presence at 3.5 kHz that holds back on the peaks.",
       { bell (3500, 0.8, 0.7), dyn (bell (3500, 0, 0.9), -24, -1.0, 2, 5, 120) }, LIN);
    x ("Ar (master)", "Mastering High-Frequency Air", "Ar acima dos 14 kHz com De-harsh leve.", "Air above 14 kHz with light De-harsh.",
       { hs (14000, 1.2, 0.6) }, with (LIN, { { "deharsh", 1.0f }, { "deharshAmt", 15.0f } }));
    x ("Dureza (master)", "Mastering Harshness Reduction", "Cortes dinâmicos suaves aos 3,2 e 6 kHz e De-harsh.", "Gentle dynamic cuts at 3.2 and 6 kHz plus De-harsh.",
       { dyn (bell (3200, 0, 1.0), -26, -1.5, 2, 5, 120), dyn (bell (6000, 0, 1.2), -28, -1.5, 2, 3, 100) }, with (NAT, { { "deharsh", 1.0f }, { "deharshAmt", 30.0f } }));
    x ("Graves dinâmicos (master)", "Mastering Dynamic Bass Control", "Graves e sub que se seguram quando sobem.", "Lows and sub that hold back when they rise.",
       { dyn (bell (50, 0, 1.2), -20, -2, 2, 20, 200), dyn (ls (110, 0, 0.7), -18, -2, 2, 15, 180) }, NAT);
    x ("Presença dinâmica (master)", "Mastering Dynamic Presence", "Realça os 3 kHz nas partes mais escuras.", "Lifts 3 kHz in the darker sections.",
       { below (dyn (bell (3000, 0, 0.8), -30, 1.5, 2, 10, 200)) }, NAT);
    x ("Equilíbrio estéreo (master)", "Mastering Stereo Tonal Balance", "Lados com ar e sem graves; centro com presença.", "Sides with air and no lows; centre with presence.",
       { at (lc (80, S12), SIDE), at (hs (10000, 0.8, 0.6), SIDE), at (bell (2500, 0.4, 0.8), MID) }, with (MS, LIN));
    x ("Correção M/S (master)", "Mastering Mid/Side Correction", "Centro mais limpo, lados mais suaves e abertos.", "Cleaner centre, smoother and more open sides.",
       { at (bell (300, -0.8, 0.8), MID), at (lc (100, S12), SIDE), at (bell (4000, -0.5, 0.8), SIDE), at (hs (12000, 1.0, 0.6), SIDE) }, with (MS, LIN));
    x ("Ressonâncias (master)", "Mastering Resonance Cleanup", "Cortes dinâmicos discretos aos 160, 315 e 630 Hz.", "Discreet dynamic cuts at 160, 315 and 630 Hz.",
       { dyn (bell (160, 0, 4), -28, -2, 2.5, 10, 150), dyn (bell (315, 0, 4), -28, -2, 2.5, 8, 150), dyn (bell (630, 0, 4), -30, -2, 2.5, 5, 120) }, NAT);
    x ("Tilt suave (master)", "Mastering Gentle Tilt EQ", "Tilt de 0,8 dB à volta de 1 kHz.", "0.8 dB tilt around 1 kHz.",
       { tilt (1000, 0.8) }, LIN);
    x ("Tom vintage (master)", "Mastering Vintage Tone", "Graves redondos, agudos sedosos e Character Warm.", "Round lows, silky highs and Character Warm.",
       { ls (60, 1.0, 0.7), bell (200, -0.5, 0.8), hs (12000, 1.0, 0.6), hc (18000, S12) }, with (WARM, NAT));
    x ("Brilho moderno (master)", "Mastering Modern Brightness", "Agudos abertos acima dos 9 kHz com De-harsh.", "Open highs above 9 kHz with De-harsh.",
       { lc (25, S24), bell (250, -0.6, 0.8), hs (9000, 1.5, 0.6) }, with (LIN, { { "deharsh", 1.0f }, { "deharshAmt", 20.0f } }));
    x ("Igualar referência (master)", "Mastering Reference Tonal Matching", "Ponto de partida: referência no sidechain e Smart Ops → Igualar referência.", "Starting point: reference on the sidechain and Smart Ops → Match reference.",
       {}, LIN);
    x ("Tradução para streaming", "Mastering Streaming Translation", "Graves controlados, presença discreta e agudos que não cansam.", "Controlled lows, discreet presence and non-fatiguing highs.",
       { lc (25, S24), dyn (ls (80, 0, 0.7), -20, -1.5, 2, 15, 150), bell (2500, 0.5, 0.8), dyn (hs (10000, 0, 0.7), -28, -1.5, 2, 3, 100) }, LIN);
    x ("Polimento final", "Mastering Final Polish", "Toques finais muito leves com Character Subtle.", "Very light final touches with Character Subtle.",
       { lc (18, S24), ls (70, 0.5, 0.6), bell (400, -0.4, 0.7), hs (15000, 0.8, 0.6) }, with (SUBTLE, NAT));

    // ======================================================================= 10. Criativo e vintage (181–200)
    x.folder = "Criativo";
    x ("Passa-banda de telefone", "Telephone Bandpass", "Passa-banda íngreme 350 Hz–3,4 kHz.", "Steep 350 Hz–3.4 kHz band-pass.",
       { lc (350, S72), hc (3400, S72), bell (1800, 3.0, 1.0) });
    x ("Rádio AM vintage", "Vintage AM Radio", "Banda AM estreita com médios projetados.", "Narrow AM band with projected mids.",
       { lc (300, S48), hc (4000, S48), bell (1200, 4.0, 1.0) }, WARM);
    x ("Rádio FM vintage", "Vintage FM Radio", "Banda FM larga, graves cheios e agudos limitados aos 15 kHz.", "Wide FM band, full lows and highs limited at 15 kHz.",
       { lc (50, S24), ls (100, 1.5, 0.7), bell (3000, 1.5, 0.8), hc (15000, S48) }, SUBTLE);
    x ("Lo-fi banda reduzida", "Lo-Fi Bandwidth Reduction", "Largura de banda reduzida 150 Hz–6 kHz.", "Reduced 150 Hz–6 kHz bandwidth.",
       { lc (150, S24), hc (6000, S36) }, WARM);
    x ("Debaixo de água", "Underwater EQ", "Tudo acima dos 500 Hz fora, graves inchados.", "Everything above 500 Hz out, swollen lows.",
       { ls (150, 3.0, 0.7), bell (300, 2.0, 1.0), hc (500, S48) });
    x ("Som distante", "Distant Sound Perspective", "Menos graves e agudos — a fonte afasta-se.", "Fewer lows and highs — the source moves away.",
       { lc (200, S12), bell (1000, -2.0, 0.7), hc (4000, S12) });
    x ("Megafone", "Megaphone Effect", "Banda estreita e pico agressivo aos 1,8 kHz.", "Narrow band and an aggressive peak at 1.8 kHz.",
       { lc (600, S48), hc (4000, S48), bell (1800, 8.0, 1.5) }, WARM);
    x ("Walkie-talkie", "Walkie-Talkie Effect", "Banda muito estreita 500 Hz–2,8 kHz.", "Very narrow 500 Hz–2.8 kHz band.",
       { lc (500, S72), hc (2800, S72), bell (1400, 6.0, 2.0) }, WARM);
    x ("Vinil antigo", "Old Vinyl Tone", "Graves quentes, agudos que caem a partir dos 9 kHz.", "Warm lows, highs rolling off from 9 kHz.",
       { lc (40, S12), ls (150, 1.5, 0.7), bell (2500, -1.0, 0.8), hc (9000, S18) }, WARM);
    x ("Cassete", "Cassette Tape Tone", "Graves gordos e agudos de fita.", "Fat lows and tape-like highs.",
       { lc (45, S12), ls (100, 1.5, 0.7), hs (8000, -3.0, 0.7), hc (13000, S24) }, WARM);
    x ("Calor de consola analógica", "Analog Console Warmth", "Curva larga e Character Warm.", "Broad curve and Character Warm.",
       { ls (100, 1.0, 0.6), bell (3000, 0.5, 0.6), hs (16000, -0.5, 0.6) }, with (WARM, NAT));
    x ("Curva estilo Pultec", "Vintage Pultec-Style Curve", "Realce e corte simultâneos nos graves; sino largo aos 10 kHz.", "Simultaneous low boost and dip; broad bell at 10 kHz.",
       { ls (60, 3.0, 0.6), bell (200, -1.5, 0.8), bell (10000, 2.5, 0.6), hs (16000, -1.0, 0.7) }, with (SUBTLE, NAT));
    x ("Presença de consola britânica", "British Console Presence", "Presença aos 3,5 kHz e graves firmes.", "Presence at 3.5 kHz and firm lows.",
       { bell (120, 1.0, 1.0), bell (3500, 2.0, 0.9) }, WARM);
    x ("Médios de consola americana", "American Console Midrange", "Médios gordos aos 700 Hz e 1,5 kHz.", "Fat mids at 700 Hz and 1.5 kHz.",
       { ls (80, 1.0, 0.7), bell (700, 1.5, 0.8), bell (1500, 1.0, 1.0) }, WARM);
    x ("Curva sorriso", "Smile Curve EQ", "Graves e agudos à frente, médios recuados.", "Lows and highs forward, mids recessed.",
       { ls (100, 3.0, 0.7), bell (800, -2.0, 0.6), hs (8000, 3.0, 0.7) });
    x ("Curva carrancuda", "Frown Curve EQ", "Médios à frente, pontas recuadas.", "Mids forward, extremes recessed.",
       { ls (100, -3.0, 0.7), bell (1000, 2.5, 0.6), hs (8000, -3.0, 0.7) });
    x ("Tom cinematográfico escuro", "Dark Cinematic Tone", "Graves largos e agudos escurecidos.", "Wide lows and darkened highs.",
       { ls (80, 2.0, 0.7), hs (4000, -4.0, 0.6), hc (12000, S12) }, WARM);
    x ("Tom hyperpop brilhante", "Bright Hyperpop Tone", "Médios-graves fora e agudos muito brilhantes.", "Low mids out and very bright highs.",
       { lc (80, S24), bell (300, -2.0, 1.0), hs (6000, 4.0, 0.7), bell (12000, 3.0, 0.8) });
    x ("Transição com varrimento", "Frequency Sweep Transition", "Low cut ressonante na banda 1: automatize a frequência (20 Hz → 2 kHz).", "Resonant low cut on band 1: automate its frequency (20 Hz → 2 kHz).",
       { lc (20, S24, 1.4) });
    x ("Filtro para automação", "Automated Filter Movement", "High cut ressonante na banda 1, pronto para automatizar a frequência.", "Resonant high cut on band 1, ready for frequency automation.",
       { hc (20000, S24, 1.6) });
}
} // namespace presets
