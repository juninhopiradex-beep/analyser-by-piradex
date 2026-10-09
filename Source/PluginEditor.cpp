#include "PluginEditor.h"

using View = AnalysisResults::View;
static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

static juce::String hzText (double f)
{
    return f < 1000.0 ? juce::String (juce::roundToInt (f)) + " Hz"
                      : juce::String (f / 1000.0, f < 10000.0 ? 2 : 1) + " kHz";
}

static juce::String signalName (pca::Signal s)
{
    switch (s)
    {
        case pca::Signal::Impulse:   return "Impulso";
        case pca::Signal::LogSweep:  return "Sweep log";
        case pca::Signal::PinkNoise: return U ("Ruído rosa");
        case pca::Signal::Sine:      return "Seno";
    }
    return {};
}

// =============================================================================
//  Look & Feel
// =============================================================================
PxLookAndFeel::PxLookAndFeel()
{
    using namespace pxcol;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::black);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, text);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, green.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, green);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxOutlineColourId, outline);
    setColour (juce::Slider::trackColourId, green.withAlpha (0.22f));
    setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::black);
    setColour (juce::TextEditor::textColourId, juce::Colours::white);
    setColour (juce::TextEditor::highlightColourId, green.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, green);
}

void PxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (down ? juce::Colour (0xff222222) : juce::Colour (0xff111111));
    g.fillRoundedRectangle (r, 5.0f);
    auto edge = on ? pxcol::green : (over ? juce::Colour (0xff8a8a8a) : pxcol::outline);
    if (! b.isEnabled()) edge = edge.withAlpha (0.35f);
    g.setColour (edge);
    g.drawRoundedRectangle (r, 5.0f, on ? 2.0f : 1.4f);
}

void PxLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto c = b.getToggleState() ? pxcol::green : pxcol::text;
    if (! b.isEnabled()) c = c.withAlpha (0.35f);
    g.setColour (c);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void PxLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isEnabled() ? pxcol::outline : pxcol::outline.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 5.0f, 1.4f);
    juce::Path tri;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (box.isEnabled() ? pxcol::text : pxcol::text.withAlpha (0.35f));
    g.fillPath (tri);
}

void PxLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font PxLookAndFeel::getComboBoxFont (juce::ComboBox&)       { return juce::Font (juce::FontOptions (14.0f)); }
juce::Font PxLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
juce::Font PxLookAndFeel::getLabelFont (juce::Label& l)           { return l.getFont(); }
juce::Font PxLookAndFeel::getPopupMenuFont()                      { return juce::Font (juce::FontOptions (14.0f)); }

void PxLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos,
                                      float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (pxcol::green.withAlpha (s.isEnabled() ? 0.18f : 0.06f));
    g.fillRoundedRectangle (r.withRight (juce::jlimit (r.getX(), r.getRight(), pos)), 5.0f);
    g.setColour (s.isEnabled() ? pxcol::outline : pxcol::outline.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 5.0f, 1.4f);
}

// =============================================================================
//  Gráfico
// =============================================================================
juce::Rectangle<float> GraphView::plotArea() const
{
    auto r = getLocalBounds().toFloat();
    return { r.getX() + 58.0f, r.getY() + 8.0f, r.getWidth() - 58.0f - 48.0f, r.getHeight() - 8.0f - 26.0f };
}

float GraphView::xForFreq (float f) const
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
}

float GraphView::freqForX (float x) const
{
    const auto p = plotArea();
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

void GraphView::setResults (const AnalysisResults& r)
{
    const bool animatable = (r.view == View::Response || r.view == View::Music)
                         && r.view == res.view && shown.freq.size() == r.curve.freq.size() && ! r.curve.empty();
    res = r;
    if (! animatable) shown = res.curve;
    repaint();
}

// Transição suave: a curva no ecrã aproxima-se ~50% da nova em cada frame (30 Hz)
void GraphView::tick()
{
    if (shown.freq.size() != res.curve.freq.size()) { shown = res.curve; return; }
    for (size_t i = 0; i < shown.freq.size(); ++i)
    {
        if (! res.curve.valid[i]) { shown.valid[i] = 0; continue; }
        if (! shown.valid[i]) { shown.magDb[i] = res.curve.magDb[i]; shown.valid[i] = 1; }
        shown.magDb[i] += 0.5f * (res.curve.magDb[i] - shown.magDb[i]);
        shown.phaseDeg[i] = res.curve.phaseDeg[i];
        shown.coh[i] = res.curve.coh[i];
    }
}

void GraphView::drawFreqGrid (juce::Graphics& g)
{
    const auto p = plotArea();
    for (float dec : { 10.0f, 100.0f, 1000.0f, 10000.0f })
        for (int m = 1; m <= 9; ++m)
        {
            const float f = dec * (float) m;
            if (f < 20.0f || f > 20000.0f) continue;
            const bool major = (m == 1 || m == 2 || m == 5);
            g.setColour (major ? pxcol::gridMajor : pxcol::grid);
            g.drawVerticalLine (juce::roundToInt (xForFreq (f)), p.getY(), p.getBottom());
        }
    g.setColour (pxcol::dim);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    const std::pair<float, const char*> labels[] { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" },
                                                   { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" } };
    for (auto& [f, t] : labels)
        g.drawText (t, juce::Rectangle<float> (xForFreq (f) - 24.0f, p.getBottom() + 4.0f, 48.0f, 16.0f), juce::Justification::centred);
}

juce::Path GraphView::curvePath (const pca::Curve& c, std::function<float (float)> yOf, bool phase) const
{
    juce::Path path;
    bool drawing = false;
    float prevPh = 0.0f;
    for (size_t i = 0; i < c.freq.size(); ++i)
    {
        if (! c.valid[i]) { drawing = false; continue; }
        const float v = phase ? c.phaseDeg[i] : c.magDb[i];
        const float x = xForFreq (c.freq[i]), y = yOf (v);
        if (! drawing || (phase && std::abs (v - prevPh) > 180.0f)) { path.startNewSubPath (x, y); drawing = true; }
        else path.lineTo (x, y);
        prevPh = v;
    }
    return path;
}

void GraphView::drawMessage (juce::Graphics& g, const juce::String& title, const juce::String& sub)
{
    const auto p = plotArea();
    auto box = p.withSizeKeepingCentre (juce::jmin (p.getWidth() - 40.0f, 760.0f), 120.0f);
    g.setColour (juce::Colours::black.withAlpha (0.78f));
    g.fillRoundedRectangle (box, 8.0f);
    g.setColour (pxcol::outline);
    g.drawRoundedRectangle (box, 8.0f, 1.2f);
    g.setColour (pxcol::green);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText (title, box.removeFromTop (48.0f).reduced (16.0f, 0.0f), juce::Justification::centredBottom);
    g.setColour (pxcol::text);
    g.setFont (juce::Font (juce::FontOptions (14.5f)));
    g.drawFittedText (sub, box.reduced (20.0f, 8.0f).toNearestInt(), juce::Justification::centredTop, 3);
}

void GraphView::drawReadout (juce::Graphics& g, const juce::String& text)
{
    const auto p = plotArea();
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawVerticalLine (juce::roundToInt (hover.x), p.getY(), p.getBottom());
    juce::Font f (juce::FontOptions (17.0f));
    const float w = juce::GlyphArrangement::getStringWidth (f, text) + 40.0f;
    auto box = juce::Rectangle<float> (p.getCentreX() - w * 0.5f, p.getY(), w, 34.0f);
    g.setColour (juce::Colours::black);
    g.fillRect (box);
    g.setColour (pxcol::outline);
    g.drawRect (box, 1.2f);
    g.setColour (juce::Colours::white);
    g.setFont (f);
    g.drawText (text, box, juce::Justification::centred);
}

// Mensagens de "à espera" comuns às vistas. Devolve true se desenhou uma.
bool GraphView::drawWaitingIfNeeded (juce::Graphics& g)
{
    if (res.sidechainMissing)
    {
        drawMessage (g, U ("Sidechain inativo"),
                     U ("Ative o sidechain desta instância e envie para ele o sinal ORIGINAL (antes do plugin/hardware). "
                        "A entrada principal deve receber o sinal processado. Ou use o modo HOST."));
        return true;
    }
    if (res.hasSignal) return false;
    if (res.hostMode && ! res.hostLoaded)
        drawMessage (g, U ("Modo HOST — carregue um plugin"),
                     U ("Clique em CARREGAR PLUGIN, escolha o seu EQ, compressor ou saturador e depois ABRIR PLUGIN. "
                        "Mexa nos botões dele e veja a curva mudar aqui."));
    else if (res.view == View::Music)
        drawMessage (g, U ("À espera de música…"), res.hostMode ? U ("Reproduza a faixa: a música passa pelo plugin carregado.")
                                                                : U ("Reproduza a sessão: o sidechain recebe o dry e a entrada o wet."));
    else if (res.view == View::Harmonics)
        drawMessage (g, U ("À espera do seno de teste…"),
                     U ("Gerador em \"Seno (harmónicos)\" antes do plugin/equipamento, Analisador depois. "
                        "Suba o Nível no Gerador para levar o equipamento à saturação."));
    else
        drawMessage (g, U ("À espera do sinal de teste…"),
                     U ("[ANALYSER: GERADOR] → [o seu plugin] → [ANALYSER: ANALISADOR], no mesmo Grupo. "
                        "Reproduza ou monitorize a faixa. Ou use o modo HOST com uma só instância."));
    return true;
}

void GraphView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    const auto p = plotArea();

    const bool sweepTab = tab == ViewTab::Sweep && (res.view == View::Harmonics || res.view == View::Response
                                                    || res.sweepShownKind != 0);
    if (res.view == View::Generator)          { drawFreqGrid (g); drawGenerator (g); }
    else if (res.view == View::None)          { drawFreqGrid (g); drawMessage (g, U ("A iniciar…"), U ("A aguardar o primeiro bloco de áudio do host.")); }
    else if (sweepTab)                        drawSweep (g);
    else if (tab == ViewTab::Wave)            drawWave (g);
    else if (tab == ViewTab::Spectrum && res.view == View::Music) { drawFreqGrid (g); drawSpectrum (g); }
    else if (res.view == View::Harmonics)     { drawFreqGrid (g); drawHarmonics (g); }
    else                                      { drawFreqGrid (g); drawResponse (g); }

    g.setColour (pxcol::outline);
    g.drawRect (p, 1.0f);
}

void GraphView::drawGenerator (juce::Graphics& g)
{
    const auto& s = res.spec;
    const double ms = s.fs > 0 ? 1000.0 * s.N / s.fs : 0.0;
    juce::String sub = U ("Sinal: ") + signalName (s.signal)
                     + (s.signal == pca::Signal::Sine ? " " + hzText (s.sineHz) : juce::String())
                     + U (" · Nível: ") + juce::String (s.levelDb, 1) + U (" dBFS · Período: ")
                     + juce::String (s.N) + U (" amostras (") + juce::String (juce::roundToInt (ms)) + " ms)\n"
                     + U ("Cadeia: [Gerador] → [plugin ou insert de hardware] → [Analisador, mesmo grupo]");
    if (res.sweepKind != 0)
        sub = U ("A executar varrimento de ") + (res.sweepKind == 1 ? U ("nível") : U ("frequência"))
            + U (" — passo ") + juce::String (res.sweepStep + 1) + " / " + juce::String (res.sweepTotal) + "\n" + sub;
    drawMessage (g, U ("MODO GERADOR — a emitir sinal de teste"), sub);
}

void GraphView::drawResponse (juce::Graphics& g)
{
    const auto p = plotArea();
    const float R = (float) CurveAnalyzerProcessor::rangeForIndex (proc.paramIndex (ids::range));
    const bool showPhase = proc.paramIndex (ids::showPhase) == 1;
    auto yDb = [p, R] (float db) { return p.getCentreY() - juce::jlimit (-1.2f, 1.2f, db / R) * p.getHeight() * 0.5f; };
    auto yPh = [p] (float deg) { return p.getCentreY() - deg / 180.0f * p.getHeight() * 0.5f; };

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    for (int i = -4; i <= 4; ++i)
    {
        const float db = R * (float) i / 4.0f, y = yDb (db);
        g.setColour (i == 0 ? juce::Colour (0xff5a5a5a) : pxcol::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (pxcol::green.withAlpha (0.85f));
        const juce::String t = (db > 0 ? "+" : "") + juce::String (db, R < 6.0f ? 2 : (std::abs (std::fmod (db, 1.0f)) < 1.0e-4f ? 0 : 1)) + " dB";
        g.drawText (t, juce::Rectangle<float> (2.0f, y - 8.0f, 54.0f, 16.0f), juce::Justification::centredRight);
    }
    if (showPhase)
    {
        g.setColour (pxcol::purple);
        for (int d : { 180, 90, 0, -90, -180 })
            g.drawText (juce::String (d) + U ("°"), juce::Rectangle<float> (p.getRight() + 4.0f, yPh ((float) d) - 8.0f, 44.0f, 16.0f),
                        juce::Justification::centredLeft);
    }

    if (drawWaitingIfNeeded (g)) return;

    g.saveState();
    g.reduceClipRegion (p.toNearestInt());

    if (res.view == View::Music)
    {
        auto yC = [p] (float c) { return p.getBottom() - juce::jlimit (0.0f, 1.0f, c) * p.getHeight() * 0.18f; };
        juce::Path cp; bool d = false;
        for (size_t i = 0; i < shown.freq.size(); ++i)
        {
            if (! shown.valid[i]) { d = false; continue; }
            const float x = xForFreq (shown.freq[i]), y = yC (shown.coh[i]);
            if (! d) { cp.startNewSubPath (x, y); d = true; } else cp.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff6d6d6d));
        g.strokePath (cp, juce::PathStrokeType (1.0f));
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (U ("coerência"), juce::Rectangle<float> (p.getX() + 6.0f, p.getBottom() - p.getHeight() * 0.18f - 16.0f, 100.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    static const juce::Colour refCols[] { juce::Colour (0xff9a9a9a), juce::Colour (0xfff2a65a), juce::Colour (0xff5ac8f2), juce::Colour (0xfff2e15a) };
    for (size_t i = 0; i < proc.references.size(); ++i)
    {
        g.setColour (refCols[i % 4].withAlpha (0.75f));
        g.strokePath (curvePath (proc.references[i], yDb, false),
                      juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (showPhase)
    {
        g.setColour (pxcol::purple);
        g.strokePath (curvePath (shown, yPh, true), juce::PathStrokeType (1.6f));
    }
    g.setColour (pxcol::green);
    g.strokePath (curvePath (shown, yDb, false),
                  juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();

    if (hovering && p.contains (hover) && ! shown.empty())
    {
        const float f = freqForX (hover.x);
        size_t best = 0;
        for (size_t i = 1; i < shown.freq.size(); ++i)
            if (std::abs (std::log (shown.freq[i] / f)) < std::abs (std::log (shown.freq[best] / f))) best = i;
        if (shown.valid[best])
        {
            juce::String t = juce::String (juce::roundToInt (f)) + " Hz, " + juce::String (shown.magDb[best], 1) + " dB";
            if (showPhase) t << ", " << juce::roundToInt (shown.phaseDeg[best]) << U (" graus");
            if (res.view == View::Music) t << U (", coer. ") << juce::String (shown.coh[best], 2);
            drawReadout (g, t);
        }
    }
}

void GraphView::drawHarmonics (juce::Graphics& g)
{
    const auto p = plotArea();
    const float bottomDb = -150.0f;
    auto yDb = [p, bottomDb] (float db) { return p.getY() + juce::jlimit (0.0f, 1.0f, db / bottomDb) * p.getHeight(); };

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    for (int db = 0; db >= (int) bottomDb; db -= 20)
    {
        const float y = yDb ((float) db);
        g.setColour (db == 0 ? juce::Colour (0xff5a5a5a) : pxcol::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (pxcol::green.withAlpha (0.85f));
        g.drawText (juce::String (db) + " dB", juce::Rectangle<float> (2.0f, y - 8.0f, 54.0f, 16.0f), juce::Justification::centredRight);
    }
    g.setColour (pxcol::dim);
    g.drawText ("dBFS", juce::Rectangle<float> (p.getRight() + 4.0f, p.getY(), 44.0f, 16.0f), juce::Justification::centredLeft);

    if (drawWaitingIfNeeded (g)) return;

    const auto& h = res.harm;
    g.saveState();
    g.reduceClipRegion (p.toNearestInt());

    juce::Path sp = curvePath (res.curve, yDb, false);
    juce::Path fill = sp;
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();
    g.setColour (pxcol::green.withAlpha (0.07f));
    g.fillPath (fill);
    g.setColour (pxcol::green.withAlpha (0.9f));
    g.strokePath (sp, juce::PathStrokeType (1.3f));

    if (h.noiseFloorDb > -200.0)
    {
        const float y = yDb ((float) h.noiseFloorDb);
        const float dashes[] { 4.0f, 4.0f };
        g.setColour (pxcol::dim.withAlpha (0.6f));
        g.drawDashedLine (juce::Line<float> (p.getX(), y, p.getRight(), y), dashes, 2, 1.0f);
    }

    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    for (int k = 1; k <= pca::HarmonicResult::kMaxH; ++k)
    {
        if (! h.present[k]) continue;
        const float x = xForFreq ((float) (h.f0 * k)), y = yDb ((float) h.ampDb[k]);
        const auto c = k == 1 ? juce::Colours::white : (k % 2 == 0 ? pxcol::even : pxcol::odd);
        g.setColour (c);
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.drawText ("H" + juce::String (k), juce::Rectangle<float> (x - 18.0f, y - 22.0f, 36.0f, 14.0f), juce::Justification::centred);
    }
    g.restoreState();

    const bool panelLeft = xForFreq ((float) h.f0) > p.getCentreX();
    auto box = juce::Rectangle<float> (panelLeft ? p.getX() + 12.0f : p.getRight() - 264.0f, p.getY() + 44.0f,
                                       252.0f, 18.0f * 9 + 26.0f + 18.0f * 7 + 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.82f));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (pxcol::outline);
    g.drawRoundedRectangle (box, 6.0f, 1.2f);
    auto in = box.reduced (12.0f, 8.0f);
    g.setColour (pxcol::text);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (U ("HARMÓNICOS (dBc)  f0 ") + hzText (h.f0), in.removeFromTop (22.0f), juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    for (int k = 2; k <= pca::HarmonicResult::kMaxH; ++k)
    {
        auto row = in.removeFromTop (18.0f);
        const auto c = k % 2 == 0 ? pxcol::even : pxcol::odd;
        g.setColour (c);
        g.drawText ("H" + juce::String (k), row.removeFromLeft (30.0f), juce::Justification::centredLeft);
        auto valR = row.removeFromRight (64.0f);
        if (! h.present[k]) { g.setColour (pxcol::dim); g.drawText (U ("> Nyq."), valR, juce::Justification::centredRight); continue; }
        const float frac = juce::jlimit (0.0f, 1.0f, (float) (h.relDb[k] + 120.0) / 120.0f);
        g.setColour (c.withAlpha (0.55f));
        g.fillRect (row.reduced (2.0f, 4.0f).withWidth (row.reduced (2.0f, 4.0f).getWidth() * frac));
        g.setColour (pxcol::text);
        g.drawText (juce::String (h.relDb[k], 1), valR, juce::Justification::centredRight);
    }
    in.removeFromTop (6.0f);
    auto line = [&] (const juce::String& a, const juce::String& b, juce::Colour c = pxcol::text)
    {
        auto row = in.removeFromTop (18.0f);
        g.setColour (pxcol::dim);
        g.drawText (a, row, juce::Justification::centredLeft);
        g.setColour (c);
        g.drawText (b, row, juce::Justification::centredRight);
    };
    line ("THD", juce::String (h.thdPct, h.thdPct < 0.1 ? 4 : 3) + " %");
    line ("THD+N", juce::String (h.thdnPct, h.thdnPct < 0.1 ? 4 : 3) + " %");
    line (U ("Pares (H2,H4…)"), juce::String (h.evenDb, 1) + " dBc", pxcol::even);
    line (U ("Ímpares (H3,H5…)"), juce::String (h.oddDb, 1) + " dBc", pxcol::odd);
    line ("Ganho (fundamental)", (h.gainDb >= 0 ? "+" : "") + juce::String (h.gainDb, 2) + " dB");
    line (U ("Piso de ruído"), h.noiseFloorDb < -160.0 ? U ("< -160 dBFS") : juce::String (h.noiseFloorDb, 1) + " dBFS");
    juce::String character;
    if (h.thdPct < 0.001)                character = U ("Linear (sem coloração)");
    else if (h.evenDb > h.oddDb + 3.0)   character = U ("Pares dominam: quente (válvula/fita)");
    else if (h.oddDb > h.evenDb + 3.0)   character = U ("Ímpares dominam: transístor/clip");
    else                                 character = U ("Pares e ímpares equilibrados");
    g.setColour (pxcol::green);
    g.drawText (character, in.removeFromTop (18.0f), juce::Justification::centredLeft);

    if (hovering && p.contains (hover) && ! res.curve.empty())
    {
        const float f = freqForX (hover.x);
        size_t best = 0;
        for (size_t i = 1; i < res.curve.freq.size(); ++i)
            if (std::abs (std::log (res.curve.freq[i] / f)) < std::abs (std::log (res.curve.freq[best] / f))) best = i;
        drawReadout (g, juce::String (juce::roundToInt (f)) + " Hz, " + juce::String (res.curve.magDb[best], 1) + " dBFS");
    }
}

// ---------------------------------------------------------------------------
//  Vista ONDA: osciloscópio (harmónicos), resposta impulsional, dry/wet (música)
// ---------------------------------------------------------------------------
void GraphView::drawWave (juce::Graphics& g)
{
    auto p = plotArea();
    if (drawWaitingIfNeeded (g)) return;

    const bool harm = res.view == View::Harmonics;
    const bool ir   = res.view == View::Response;
    juce::Rectangle<float> inset;
    if (harm)    // curva de transferência (entrada × saída) à direita
    {
        const float s = juce::jmin (p.getHeight(), p.getWidth() * 0.34f);
        inset = juce::Rectangle<float> (p.getRight() - s, p.getY(), s, s);
        p = p.withRight (inset.getX() - 24.0f);
    }

    float peak = 1.0e-6f;
    for (float v : res.waveOut) peak = juce::jmax (peak, std::abs (v));
    for (float v : res.waveIn)  peak = juce::jmax (peak, std::abs (v));
    const float scale = peak * 1.15f;
    auto yOf = [p, scale] (float v) { return p.getCentreY() - v / scale * p.getHeight() * 0.5f; };

    // grelha
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    for (int i = -2; i <= 2; ++i)
    {
        const float v = scale * (float) i / 2.0f, y = yOf (v);
        g.setColour (i == 0 ? juce::Colour (0xff5a5a5a) : pxcol::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (pxcol::green.withAlpha (0.85f));
        g.drawText (juce::String (v, scale < 0.1f ? 4 : 3), juce::Rectangle<float> (2.0f, y - 8.0f, 54.0f, 16.0f), juce::Justification::centredRight);
    }
    const int nTicks = 6;
    g.setColour (pxcol::dim);
    for (int i = 0; i <= nTicks; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / nTicks;
        g.setColour (pxcol::grid);
        g.drawVerticalLine (juce::roundToInt (x), p.getY(), p.getBottom());
        g.setColour (pxcol::dim);
        const double ms = res.waveStartMs + res.waveSpanMs * i / nTicks;
        g.drawText (juce::String (ms, res.waveSpanMs < 5.0 ? 2 : 1) + " ms",
                    juce::Rectangle<float> (x - 34.0f, p.getBottom() + 4.0f, 68.0f, 16.0f), juce::Justification::centred);
    }
    if (ir)
    {
        const float x0 = p.getX() + p.getWidth() * (float) (-res.waveStartMs / res.waveSpanMs);
        g.setColour (pxcol::purple.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x0), p.getY(), p.getBottom());
    }

    auto trace = [&] (const std::vector<float>& v, juce::Colour c, float w)
    {
        if (v.size() < 2) return;
        juce::Path path;
        for (size_t i = 0; i < v.size(); ++i)
        {
            const float x = p.getX() + p.getWidth() * (float) i / (float) (v.size() - 1);
            if (i == 0) path.startNewSubPath (x, yOf (v[i])); else path.lineTo (x, yOf (v[i]));
        }
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    g.saveState();
    g.reduceClipRegion (p.toNearestInt());
    trace (res.waveIn, juce::Colour (0xff8a8a8a), 1.5f);
    trace (res.waveOut, pxcol::green, 2.4f);
    g.restoreState();

    // legenda
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    auto leg = juce::Rectangle<float> (p.getX() + 12.0f, p.getY() + 10.0f, 420.0f, 18.0f);
    if (ir)
    {
        g.setColour (pxcol::green);
        g.drawText (U ("RESPOSTA IMPULSIONAL (0 ms = pico, latência removida)"), leg, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (juce::Colour (0xff8a8a8a));
        g.drawText (harm ? U ("— entrada (seno puro)") : U ("— antes (dry)"), leg, juce::Justification::centredLeft);
        g.setColour (pxcol::green);
        g.drawText (harm ? U ("— saída do plugin/equipamento") : U ("— depois (wet)"), leg.translated (0.0f, 18.0f), juce::Justification::centredLeft);
    }

    if (harm && res.waveIn.size() == res.waveOut.size() && ! res.waveIn.empty())
    {
        g.setColour (juce::Colour (0xff050505));
        g.fillRect (inset);
        g.setColour (pxcol::grid);
        g.drawHorizontalLine (juce::roundToInt (inset.getCentreY()), inset.getX(), inset.getRight());
        g.drawVerticalLine (juce::roundToInt (inset.getCentreX()), inset.getY(), inset.getBottom());
        auto map = [inset, scale] (float xv, float yv)
        {
            return juce::Point<float> (inset.getCentreX() + xv / scale * inset.getWidth() * 0.5f,
                                       inset.getCentreY() - yv / scale * inset.getHeight() * 0.5f);
        };
        g.setColour (juce::Colour (0xff4a4a4a));
        g.drawLine (juce::Line<float> (map (-scale, -scale), map (scale, scale)), 1.0f);
        juce::Path tp;
        for (size_t i = 0; i < res.waveIn.size(); ++i)
        {
            const auto pt = map (res.waveIn[i], res.waveOut[i]);
            if (i == 0) tp.startNewSubPath (pt); else tp.lineTo (pt);
        }
        g.saveState();
        g.reduceClipRegion (inset.toNearestInt());
        g.setColour (pxcol::green);
        g.strokePath (tp, juce::PathStrokeType (2.0f));
        g.restoreState();
        g.setColour (pxcol::outline);
        g.drawRect (inset, 1.0f);
        g.setColour (pxcol::dim);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (U ("Curva de transferência (entrada → saída)"), inset.withHeight (18.0f).translated (0.0f, inset.getHeight() + 4.0f),
                    juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------
//  Vista ESPETRO (música): antes vs depois
// ---------------------------------------------------------------------------
void GraphView::drawSpectrum (juce::Graphics& g)
{
    const auto p = plotArea();
    const float top = 0.0f, bottom = -120.0f;
    auto yDb = [p, top, bottom] (float db) { return p.getY() + juce::jlimit (0.0f, 1.0f, (top - db) / (top - bottom)) * p.getHeight(); };
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    for (int db = 0; db >= (int) bottom; db -= 12)
    {
        const float y = yDb ((float) db);
        g.setColour (db == 0 ? juce::Colour (0xff5a5a5a) : pxcol::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (pxcol::green.withAlpha (0.85f));
        g.drawText (juce::String (db) + " dB", juce::Rectangle<float> (2.0f, y - 8.0f, 54.0f, 16.0f), juce::Justification::centredRight);
    }
    if (drawWaitingIfNeeded (g)) return;

    g.saveState();
    g.reduceClipRegion (p.toNearestInt());
    juce::Path dryP = curvePath (res.specDry, yDb, false), wetP = curvePath (res.specWet, yDb, false);
    juce::Path fill = wetP;
    fill.lineTo (p.getRight(), p.getBottom()); fill.lineTo (p.getX(), p.getBottom()); fill.closeSubPath();
    g.setColour (pxcol::green.withAlpha (0.08f));
    g.fillPath (fill);
    g.setColour (juce::Colour (0xff9a9a9a));
    g.strokePath (dryP, juce::PathStrokeType (1.6f));
    g.setColour (pxcol::green);
    g.strokePath (wetP, juce::PathStrokeType (2.2f));
    g.restoreState();

    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    auto leg = juce::Rectangle<float> (p.getX() + 12.0f, p.getY() + 10.0f, 300.0f, 18.0f);
    g.setColour (juce::Colour (0xff9a9a9a));
    g.drawText (U ("— antes (dry)"), leg, juce::Justification::centredLeft);
    g.setColour (pxcol::green);
    g.drawText (U ("— depois (wet)"), leg.translated (0.0f, 18.0f), juce::Justification::centredLeft);

    if (hovering && p.contains (hover) && ! res.specWet.empty())
    {
        const float f = freqForX (hover.x);
        size_t best = 0;
        for (size_t i = 1; i < res.specWet.freq.size(); ++i)
            if (std::abs (std::log (res.specWet.freq[i] / f)) < std::abs (std::log (res.specWet.freq[best] / f))) best = i;
        if (res.specWet.valid[best] && res.specDry.valid[best])
        {
            const float d = res.specWet.magDb[best] - res.specDry.magDb[best];
            drawReadout (g, juce::String (juce::roundToInt (f)) + " Hz, antes " + juce::String (res.specDry.magDb[best], 1)
                          + " / depois " + juce::String (res.specWet.magDb[best], 1) + " dB (" + (d >= 0 ? "+" : "") + juce::String (d, 1) + ")");
        }
    }
}

// ---------------------------------------------------------------------------
//  Vista VARRIMENTO: THD/H2/H3 vs nível ou frequência
// ---------------------------------------------------------------------------
void GraphView::drawSweep (juce::Graphics& g)
{
    const auto p = plotArea();
    const int kind = res.sweepShownKind != 0 ? res.sweepShownKind : res.sweepKind;
    const bool byLevel = kind != 2;
    auto xOf = [this, p, byLevel] (double x)
    {
        return byLevel ? p.getX() + p.getWidth() * (float) ((x + 42.0) / 42.0) : xForFreq ((float) x);
    };
    const float top = 0.0f, bottom = -120.0f;
    auto yDb = [p, top, bottom] (float db) { return p.getY() + juce::jlimit (0.0f, 1.0f, (top - db) / (top - bottom)) * p.getHeight(); };
    auto yGain = [p] (float db) { return p.getCentreY() - juce::jlimit (-12.0f, 12.0f, db) / 12.0f * p.getHeight() * 0.5f; };

    // grelha
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    for (int db = 0; db >= (int) bottom; db -= 20)
    {
        const float y = yDb ((float) db);
        g.setColour (pxcol::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (pxcol::green.withAlpha (0.85f));
        g.drawText (juce::String (db) + " dBc", juce::Rectangle<float> (0.0f, y - 8.0f, 56.0f, 16.0f), juce::Justification::centredRight);
    }
    g.setColour (pxcol::purple);
    for (int d : { 12, 6, 0, -6, -12 })
        g.drawText ((d > 0 ? "+" : "") + juce::String (d) + " dB", juce::Rectangle<float> (p.getRight() + 4.0f, yGain ((float) d) - 8.0f, 46.0f, 16.0f),
                    juce::Justification::centredLeft);
    g.setColour (pxcol::dim);
    if (byLevel)
    {
        for (int lv = -42; lv <= 0; lv += 6)
        {
            const float x = xOf (lv);
            g.setColour (pxcol::grid);
            g.drawVerticalLine (juce::roundToInt (x), p.getY(), p.getBottom());
            g.setColour (pxcol::dim);
            g.drawText (juce::String (lv) + " dBFS", juce::Rectangle<float> (x - 34.0f, p.getBottom() + 4.0f, 68.0f, 16.0f), juce::Justification::centred);
        }
    }
    else
        drawFreqGrid (g);

    const bool any = std::any_of (res.sweep.begin(), res.sweep.end(), [] (const pca::SweepPoint& s) { return s.valid; });
    if (! any)
    {
        const bool canRun = res.hostMode || res.linked;
        drawMessage (g, res.sweepKind != 0 ? U ("A preparar o varrimento…") : U ("Varrimento automático de THD"),
                     canRun ? U ("VARRER NÍVEL: seno de -42 a 0 dBFS, mostra onde o equipamento começa a saturar. "
                                 "VARRER FREQUÊNCIA: THD de 31 Hz a 10 kHz ao nível escolhido.")
                            : U ("Precisa de um Gerador ligado no mesmo grupo (ou do modo HOST)."));
        return;
    }

    auto series = [&] (std::function<double (const pca::SweepPoint&)> val, juce::Colour c, bool gain)
    {
        juce::Path path; bool started = false;
        for (auto& s : res.sweep)
        {
            if (! s.valid) { started = false; continue; }
            const float x = xOf (s.x), y = gain ? yGain ((float) val (s)) : yDb ((float) val (s));
            if (! started) { path.startNewSubPath (x, y); started = true; } else path.lineTo (x, y);
            g.setColour (c);
            g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
        }
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (gain ? 1.6f : 2.4f));
    };
    g.saveState();
    g.reduceClipRegion (p.expanded (4.0f).toNearestInt());
    series ([] (const pca::SweepPoint& s) { return s.gainDb; }, pxcol::purple.withAlpha (0.8f), true);
    series ([] (const pca::SweepPoint& s) { return s.h3Db; }, pxcol::odd, false);
    series ([] (const pca::SweepPoint& s) { return s.h2Db; }, pxcol::even, false);
    series ([] (const pca::SweepPoint& s) { return s.thdDb; }, pxcol::green, false);
    g.restoreState();

    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    auto leg = juce::Rectangle<float> (p.getX() + 12.0f, p.getY() + 10.0f, 420.0f, 18.0f);
    g.setColour (pxcol::green);  g.drawText ("THD", leg, juce::Justification::centredLeft);
    g.setColour (pxcol::even);   g.drawText ("H2", leg.translated (52.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (pxcol::odd);    g.drawText ("H3", leg.translated (92.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (pxcol::purple); g.drawText ("Ganho", leg.translated (132.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (pxcol::text);
    g.drawText (byLevel ? U ("THD vs NÍVEL de entrada") : U ("THD vs FREQUÊNCIA"), leg.translated (0.0f, 20.0f), juce::Justification::centredLeft);
    if (res.sweepKind != 0)
    {
        g.setColour (pxcol::even);
        g.drawText (U ("A varrer… passo ") + juce::String (res.sweepStep + 1) + " / " + juce::String (res.sweepTotal),
                    leg.translated (0.0f, 40.0f), juce::Justification::centredLeft);
    }

    if (hovering && p.contains (hover))
    {
        const pca::SweepPoint* best = nullptr;
        float bd = 1.0e9f;
        for (auto& s : res.sweep)
            if (s.valid && std::abs (xOf (s.x) - hover.x) < bd) { bd = std::abs (xOf (s.x) - hover.x); best = &s; }
        if (best != nullptr)
            drawReadout (g, (byLevel ? juce::String (best->x, 0) + " dBFS" : hzText (best->x))
                          + U (" · THD ") + juce::String (100.0 * std::pow (10.0, best->thdDb / 20.0), 3) + " %"
                          + U (" · H2 ") + juce::String (best->h2Db, 1) + U (" · H3 ") + juce::String (best->h3Db, 1)
                          + U (" dBc · ganho ") + juce::String (best->gainDb, 2) + " dB");
    }
}

// =============================================================================
//  Login
// =============================================================================
LoginOverlay::LoginOverlay()
{
    password.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    password.setFont (juce::Font (juce::FontOptions (18.0f)));
    password.setJustification (juce::Justification::centred);
    password.setTextToShowWhenEmpty ("Senha", pxcol::dim);
    password.setColour (juce::TextEditor::outlineColourId, pxcol::outline);
    password.setColour (juce::TextEditor::focusedOutlineColourId, pxcol::green);
    password.onReturnKey = [this] { submit(); };
    addAndMakeVisible (password);

    remember.setButtonText ("Lembrar neste computador");
    remember.setToggleState (true, juce::dontSendNotification);
    remember.setColour (juce::ToggleButton::textColourId, pxcol::text);
    remember.setColour (juce::ToggleButton::tickColourId, pxcol::green);
    remember.setColour (juce::ToggleButton::tickDisabledColourId, pxcol::outline);
    addAndMakeVisible (remember);

    enter.setToggleState (true, juce::dontSendNotification);   // contorno verde
    enter.onClick = [this] { submit(); };
    addAndMakeVisible (enter);

    message.setJustificationType (juce::Justification::centred);
    message.setFont (juce::Font (juce::FontOptions (14.0f)));
    addAndMakeVisible (message);
}

juce::Rectangle<int> LoginOverlay::card() const
{
    return getLocalBounds().withSizeKeepingCentre (440, 330);
}

void LoginOverlay::paint (juce::Graphics& g)
{
    g.fillAll (pxcol::bg.withAlpha (0.96f));
    const auto c = card().toFloat();
    g.setColour (juce::Colour (0xff121212));
    g.fillRoundedRectangle (c, 12.0f);
    g.setColour (pxcol::outline);
    g.drawRoundedRectangle (c, 12.0f, 1.4f);

    auto t = c.reduced (24.0f, 22.0f);
    g.setColour (pxcol::green);
    g.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
    g.drawText ("ANALYSER", t.removeFromTop (48.0f), juce::Justification::centred);
    g.setColour (pxcol::text);
    g.setFont (juce::Font (juce::FontOptions (16.0f)));
    g.drawText ("by Piradex", t.removeFromTop (22.0f), juce::Justification::centred);
    g.setColour (pxcol::dim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (U ("Introduza a sua senha para começar"), t.removeFromTop (30.0f), juce::Justification::centred);
}

void LoginOverlay::resized()
{
    auto c = card().reduced (40, 0);
    c.removeFromTop (138);
    password.setBounds (c.removeFromTop (40));
    c.removeFromTop (12);
    remember.setBounds (c.removeFromTop (26).withSizeKeepingCentre (230, 26));
    c.removeFromTop (12);
    enter.setBounds (c.removeFromTop (38).withSizeKeepingCentre (160, 38));
    c.removeFromTop (8);
    message.setBounds (c.removeFromTop (40));
}

void LoginOverlay::visibilityChanged()
{
    if (isVisible() && isShowing())
        password.grabKeyboardFocus();
}

void LoginOverlay::submit()
{
    if (password.isEmpty()) return;
    message.setColour (juce::Label::textColourId, pxcol::dim);
    message.setText (U ("A verificar…"), juce::dontSendNotification);
    juce::MouseCursor::showWaitCursor();

    const auto r = lic::License::get().tryPassword (password.getText(), remember.getToggleState());
    juce::MouseCursor::hideWaitCursor();

    if (r == lic::License::Result::Ok)
    {
        password.clear();
        message.setText ({}, juce::dontSendNotification);
        if (onUnlocked) onUnlocked();
        return;
    }
    password.selectAll();
    message.setColour (juce::Label::textColourId, juce::Colour (0xffff6b6b));
    message.setText (r == lic::License::Result::Expired ? U ("Esta senha beta já expirou.")
                                                        : U ("Senha incorreta."),
                     juce::dontSendNotification);
}

// =============================================================================
//  Editor
// =============================================================================
CurveAnalyzerEditor::CurveAnalyzerEditor (CurveAnalyzerProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p)
{
    setLookAndFeel (&lnf);

    // Papel: três botões grandes
    int idx = 0;
    for (auto* b : { &roleAna, &roleGen, &roleHost })
    {
        const int roleValue = idx == 0 ? 0 : (idx == 1 ? 1 : 2);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->onClick = [this, b, roleValue] { if (b->getToggleState()) setParam (ids::role, (float) roleValue); };
        addAndMakeVisible (b);
        ++idx;
    }
    roleGen.setTooltip (U ("Antes do plugin: emite o sinal de teste"));
    roleAna.setTooltip (U ("Depois do plugin: mede a curva"));
    roleHost.setTooltip (U ("Uma só instância: carrega o plugin aqui dentro"));

    // Separadores de vista
    idx = 0;
    for (auto* b : { &tabCurve, &tabWave, &tabSpec, &tabSweep })
    {
        const int v = idx++;
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1002);
        b->onClick = [this, b, v] { if (b->getToggleState()) setParam (ids::view, (float) v); };
        addAndMakeVisible (b);
    }

    setupCombo (group,     "Grupo",  ids::group);
    setupCombo (signal,    "Sinal",  ids::signal);
    setupCombo (fft,       "FFT",    ids::fftSize);
    setupCombo (source,    "Fonte",  ids::source);
    setupCombo (channel,   "Canal",  ids::channel);
    setupCombo (averages,  U ("Média"), ids::averages);
    setupCombo (smoothing, U ("Suaviz."), ids::smoothing);
    setupCombo (range,     "Escala", ids::range);

    setupBarSlider (level, levelLabel, levelAtt, U ("Nível"), ids::level, " dB");
    setupBarSlider (sine, sineLabel, sineAtt, "Seno", ids::sineFreq, " Hz");
    setupBarSlider (latency, latencyLabel, latencyAtt, U ("Latência"), ids::latency, "");
    latency.setSliderStyle (juce::Slider::IncDecButtons);
    latency.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 64, 30);

    setupToggle (phaseBtn,  phaseAtt,  "FASE",      ids::showPhase);
    setupToggle (syncBtn,   syncAtt,   "AUTO SYNC", ids::autoSync);
    setupToggle (freezeBtn, freezeAtt, "CONGELAR",  ids::freeze);
    setupToggle (muteBtn,   muteAtt,   U ("MUTE SAÍDA"), ids::muteOut);

    resetBtn.onClick = [this] { proc.requestReset(); };
    refBtn.onClick = [this]
    {
        AnalysisResults r; proc.getResults (r);
        if ((r.view == View::Response || r.view == View::Music) && r.hasSignal && ! r.curve.empty())
        {
            if (proc.references.size() >= 4) proc.references.erase (proc.references.begin());
            proc.references.push_back (r.curve);
            graph.repaint();
        }
    };
    clearRefBtn.onClick = [this] { proc.references.clear(); graph.repaint(); };
    exportBtn.onClick = [this] { exportCsv(); };
    for (auto* b : { &resetBtn, &refBtn, &clearRefBtn, &exportBtn }) addAndMakeVisible (b);

    // Modo Host
    loadBtn.onClick = [this] { showPluginMenu(); };
    openBtn.onClick = [this] { proc.openHostedEditor(); };
    removeBtn.onClick = [this] { proc.unloadHostedPlugin(); };
    // Varrimentos
    sweepLevelBtn.onClick = [this] { setParam (ids::view, (float) ViewTab::Sweep); proc.requestSweep (pca::SweepKind::Level); };
    sweepFreqBtn.onClick  = [this] { setParam (ids::view, (float) ViewTab::Sweep); proc.requestSweep (pca::SweepKind::Frequency); };
    sweepStopBtn.onClick  = [this] { proc.stopSweep(); };
    for (auto* b : { &loadBtn, &openBtn, &removeBtn, &sweepLevelBtn, &sweepFreqBtn, &sweepStopBtn })
        addChildComponent (b);

    addAndMakeVisible (graph);

    login.onUnlocked = [this] { login.setVisible (false); repaint(); };
    addChildComponent (login);
    login.setVisible (! lic::License::get().isUnlocked());

    setResizable (true, true);
    setResizeLimits (1000, 600, 2600, 1500);
    setSize (1280, 740);
    startTimerHz (30);
}

CurveAnalyzerEditor::~CurveAnalyzerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CurveAnalyzerEditor::setParam (const juce::String& id, float plain)
{
    if (auto* prm = proc.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

void CurveAnalyzerEditor::setupCombo (LabeledCombo& c, const juce::String& text, const juce::String& id)
{
    c.label.setText (text, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredRight);
    c.label.setFont (juce::Font (juce::FontOptions (15.0f)));
    c.label.setColour (juce::Label::textColourId, pxcol::text);
    addAndMakeVisible (c.label);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.box.addItemList (ch->choices, 1);
    else if (auto* pi = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter (id)))
        for (int v = pi->getRange().getStart(); v <= pi->getRange().getEnd(); ++v)
            c.box.addItem (juce::String (v), v - pi->getRange().getStart() + 1);
    addAndMakeVisible (c.box);
    c.att = std::make_unique<ComboAtt> (proc.apvts, id, c.box);
}

void CurveAnalyzerEditor::setupToggle (juce::TextButton& b, std::unique_ptr<ButtonAtt>& att, const juce::String& text, const juce::String& id)
{
    b.setButtonText (text);
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);
    att = std::make_unique<ButtonAtt> (proc.apvts, id, b);
}

void CurveAnalyzerEditor::setupBarSlider (juce::Slider& s, juce::Label& l, std::unique_ptr<SliderAtt>& att,
                                          const juce::String& text, const juce::String& id, const juce::String& suffix)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centredRight);
    l.setFont (juce::Font (juce::FontOptions (15.0f)));
    addAndMakeVisible (l);
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);
    att = std::make_unique<SliderAtt> (proc.apvts, id, s);
}

void CurveAnalyzerEditor::paint (juce::Graphics& g)
{
    g.fillAll (pxcol::bg);
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (getLocalBounds().removeFromTop (100));

    // indicador de estado na barra das vistas
    if (chipText.isNotEmpty())
    {
        const int right = (sweepLevelBtn.isVisible() ? sweepLevelBtn.getX()
                          : loadBtn.isVisible() ? loadBtn.getX() : getWidth() - 12) - 12;
        juce::Font f (juce::FontOptions (13.5f, juce::Font::bold));
        const int w = (int) juce::GlyphArrangement::getStringWidth (f, chipText) + 28;
        auto chip = juce::Rectangle<int> (right - w, 108, w, 28);
        g.setColour (chipColour.withAlpha (0.12f));
        g.fillRoundedRectangle (chip.toFloat(), 14.0f);
        g.setColour (chipColour);
        g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 14.0f, 1.2f);
        g.setFont (f);
        g.drawText (chipText, chip, juce::Justification::centred);
    }

    auto foot = getLocalBounds().removeFromBottom (40);
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (foot);
    auto brand = foot.reduced (14, 0);
    g.setColour (pxcol::green);
    g.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
    g.drawText ("ANALYSER", brand.removeFromLeft (108), juce::Justification::centredLeft);
    g.setColour (pxcol::text);
    g.setFont (juce::Font (juce::FontOptions (16.0f)));
    g.drawText ("by Piradex", brand.removeFromLeft (90), juce::Justification::centredLeft);
    const auto licText = lic::License::get().statusText();
    if (licText.isNotEmpty())
    {
        const bool beta = lic::License::get().kind() == lic::Kind::Trial;
        g.setColour (beta ? pxcol::even : pxcol::dim);
        g.setFont (juce::Font (juce::FontOptions (12.5f, beta ? juce::Font::bold : juce::Font::plain)));
        g.drawText (licText, brand.removeFromLeft (230), juce::Justification::centredLeft);
    }

    auto st = getLocalBounds().withTrimmedBottom (40).removeFromBottom (22).reduced (14, 0);
    g.setColour (pxcol::dim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawFittedText (status, st, juce::Justification::centredLeft, 1);
}

void CurveAnalyzerEditor::resized()
{
    const int margin = 12, rowH = 32, gap = 8;
    struct Item { juce::Component* c; int w; };
    auto placeRow = [&] (int y, std::vector<Item> left, std::vector<Item> right, int h)
    {
        int need = 0;
        for (auto& i : left)  need += i.w + gap;
        for (auto& i : right) need += i.w + gap;
        const int avail = getWidth() - 2 * margin - 24;
        const float k = need > avail ? (float) avail / (float) need : 1.0f;
        int x = margin;
        for (auto& i : left) { const int w = (int) (i.w * k); i.c->setBounds (x, y, w, h); x += w + (int) (gap * k); }
        x = getWidth() - margin;
        for (auto it = right.rbegin(); it != right.rend(); ++it)
        {
            const int w = (int) (it->w * k);
            x -= w;
            it->c->setBounds (x, y, w, h);
            x -= (int) (gap * k);
        }
    };

    placeRow (14,
              { { &roleGen, 104 }, { &roleAna, 118 }, { &roleHost, 80 }, { &group.label, 50 }, { &group.box, 58 },
                { &signal.label, 46 }, { &signal.box, 168 }, { &levelLabel, 46 }, { &level, 100 },
                { &fft.label, 36 }, { &fft.box, 86 }, { &sineLabel, 44 }, { &sine, 100 } },
              { { &phaseBtn, 66 }, { &syncBtn, 108 }, { &muteBtn, 112 } }, rowH);

    placeRow (56,
              { { &source.label, 48 }, { &source.box, 120 }, { &channel.label, 50 }, { &channel.box, 104 },
                { &averages.label, 50 }, { &averages.box, 76 }, { &smoothing.label, 62 }, { &smoothing.box, 108 },
                { &range.label, 52 }, { &range.box, 94 }, { &latencyLabel, 66 }, { &latency, 128 } },
              {}, rowH);

    // barra das vistas + contexto
    placeRow (106, { { &tabCurve, 86 }, { &tabWave, 80 }, { &tabSpec, 96 }, { &tabSweep, 118 } },
              loadBtn.isVisible() ? std::vector<Item> { { &loadBtn, 150 }, { &openBtn, 126 }, { &removeBtn, 96 } }
                                  : std::vector<Item> { { &sweepLevelBtn, 138 }, { &sweepFreqBtn, 178 }, { &sweepStopBtn, 80 } }, 30);

    auto foot = getLocalBounds().removeFromBottom (40).reduced (margin, 5);
    for (juce::Button* b : std::initializer_list<juce::Button*> { &exportBtn, &clearRefBtn, &refBtn, &resetBtn, &freezeBtn })
    {
        const int w = b == &clearRefBtn ? 112 : (b == &exportBtn || b == &freezeBtn ? 98 : 72);
        b->setBounds (foot.removeFromRight (w));
        foot.removeFromRight (8);
    }

    graph.setBounds (getLocalBounds().withTrimmedTop (144).withTrimmedBottom (40 + 22).reduced (margin, 2));
    login.setBounds (getLocalBounds());
}

bool CurveAnalyzerEditor::tabAvailable (ViewTab t) const
{
    switch (last.view)
    {
        case View::Response:  return t == ViewTab::Curve || t == ViewTab::Wave || (t == ViewTab::Sweep && (last.linked || last.hostMode));
        case View::Harmonics: return t != ViewTab::Spectrum;
        case View::Music:     return t != ViewTab::Sweep;
        default:              return t == ViewTab::Curve;
    }
}

ViewTab CurveAnalyzerEditor::effectiveTab() const
{
    const auto t = (ViewTab) juce::jlimit (0, 3, proc.paramIndex (ids::view));
    return tabAvailable (t) ? t : ViewTab::Curve;
}

void CurveAnalyzerEditor::updateEnablement()
{
    const Role rl = proc.role();
    const bool gen = rl == Role::Generator, host = rl == Role::Host, ana = rl == Role::Analyser;
    const bool music = proc.paramIndex (ids::source) == 1;
    const bool sineSig = proc.paramIndex (ids::signal) == 3;

    roleGen.setToggleState (gen, juce::dontSendNotification);
    roleAna.setToggleState (ana, juce::dontSendNotification);
    roleHost.setToggleState (host, juce::dontSendNotification);
    group.box.setEnabled (! host);

    // Sinal: editável no Gerador e no Host; no Analisador só sem Gerador ligado
    const bool sigEditable = gen || (host && ! music) || (ana && ! last.linked && ! music);
    signal.box.setEnabled (sigEditable);
    level.setEnabled (sigEditable);
    fft.box.setEnabled (sigEditable || (music && ! gen));
    sine.setEnabled (sigEditable && sineSig);

    for (juce::Component* c : { (juce::Component*) &source.box, (juce::Component*) &channel.box, (juce::Component*) &averages.box,
                                (juce::Component*) &smoothing.box, (juce::Component*) &range.box, (juce::Component*) &latency,
                                (juce::Component*) &phaseBtn, (juce::Component*) &syncBtn, (juce::Component*) &freezeBtn,
                                (juce::Component*) &muteBtn, (juce::Component*) &resetBtn, (juce::Component*) &refBtn,
                                (juce::Component*) &clearRefBtn, (juce::Component*) &exportBtn })
        c->setEnabled (! gen);

    // Separadores
    const auto eff = effectiveTab();
    int i = 0;
    for (auto* b : { &tabCurve, &tabWave, &tabSpec, &tabSweep })
    {
        const auto t = (ViewTab) i++;
        b->setVisible (! gen);
        b->setEnabled (tabAvailable (t));
        b->setToggleState (t == eff, juce::dontSendNotification);
    }
    graph.setTab (eff);

    // Botões de contexto
    const bool showHost = host;
    const bool showSweep = ! host && ! gen && eff == ViewTab::Sweep;
    const bool showSweepHost = host && eff == ViewTab::Sweep;
    bool relayout = false;
    auto vis = [&] (juce::Component& c, bool v) { if (c.isVisible() != v) { c.setVisible (v); relayout = true; } };
    vis (loadBtn, showHost && ! showSweepHost);
    vis (openBtn, showHost && ! showSweepHost);
    vis (removeBtn, showHost && ! showSweepHost);
    vis (sweepLevelBtn, showSweep || showSweepHost);
    vis (sweepFreqBtn, showSweep || showSweepHost);
    vis (sweepStopBtn, showSweep || showSweepHost);
    openBtn.setEnabled (proc.hasHostedPlugin());
    removeBtn.setEnabled (proc.hasHostedPlugin());
    const bool canSweep = (host && ! music) || (ana && last.linked);
    sweepLevelBtn.setEnabled (canSweep && last.sweepKind == 0);
    sweepFreqBtn.setEnabled (canSweep && last.sweepKind == 0);
    sweepStopBtn.setEnabled (last.sweepKind != 0);
    if (relayout) resized();

    // Indicador de estado
    juce::String chip; juce::Colour col = pxcol::dim;
    if (ana && ! music)
    {
        chip = last.linked ? U ("● LIGADO AO GERADOR · GRUPO ") + juce::String (proc.paramIndex (ids::group))
                           : U ("○ SEM GERADOR NO GRUPO ") + juce::String (proc.paramIndex (ids::group));
        col = last.linked ? pxcol::green : pxcol::even;
    }
    else if (ana && music)
    {
        chip = last.sidechainMissing ? U ("○ SIDECHAIN INATIVO") : U ("● DRY (SIDECHAIN) vs WET");
        col = last.sidechainMissing ? pxcol::even : pxcol::green;
    }
    else if (host)
    {
        chip = proc.hasHostedPlugin() ? U ("● ") + proc.getHostedName() : U ("○ NENHUM PLUGIN CARREGADO");
        col = proc.hasHostedPlugin() ? pxcol::green : pxcol::even;
    }
    if (chip != chipText || col != chipColour) { chipText = chip; chipColour = col; repaint (0, 100, getWidth(), 44); }
}

void CurveAnalyzerEditor::showPluginMenu()
{
    juce::PopupMenu menu;
    juce::MouseCursor::showWaitCursor();
    pluginList = proc.listInstalledPlugins();
    juce::MouseCursor::hideWaitCursor();

    juce::PopupMenu vst3, au;
    for (size_t i = 0; i < pluginList.size(); ++i)
        (pluginList[i].format == "AU" ? au : vst3).addItem ((int) i + 1, pluginList[i].name);
    if (vst3.getNumItems() > 0) menu.addSubMenu ("VST3", vst3);
    if (au.getNumItems() > 0)   menu.addSubMenu ("Audio Unit", au);
    if (menu.getNumItems() == 0) menu.addItem (-1, U ("Nenhum plugin encontrado nas pastas padrão"), false);
    menu.addSeparator();
    menu.addItem (100000, U ("Procurar ficheiro…"));

    juce::Component::SafePointer<CurveAnalyzerEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto done = [safe] (const juce::String& err)
        {
            if (safe != nullptr && err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "ANALYSER", err);
        };
        if (r == 100000)
        {
            self.chooser = std::make_unique<juce::FileChooser> (U ("Escolha um plugin (.vst3 ou .component)"), juce::File(), "*.vst3;*.component");
            self.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                       [safe, done] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                const auto f = fc.getResult();
                if (f == juce::File()) return;
                safe->proc.loadHostedPlugin (f.hasFileExtension ("component") ? "AU" : "VST3", f.getFullPathName(), done);
            });
            return;
        }
        if (r >= 1 && r <= (int) self.pluginList.size())
        {
            const auto& pl = self.pluginList[(size_t) r - 1];
            self.proc.loadHostedPlugin (pl.format, pl.fileOrId, done);
        }
    });
}

void CurveAnalyzerEditor::timerCallback()
{
    const bool locked = ! lic::License::get().isUnlocked();
    if (login.isVisible() != locked) { login.setVisible (locked); repaint(); }

    AnalysisResults r;
    proc.getResults (r);
    const bool changed = r.serial != last.serial;
    last = std::move (r);
    if (changed) graph.setResults (last);
    graph.tick();
    graph.repaint();

    // Ligado a um Gerador: os controlos de sinal mostram (e guardam) as definições reais dele
    if (last.linked && proc.role() == Role::Analyser && proc.paramIndex (ids::source) == 0 && last.sweepKind == 0)
    {
        auto sync = [this] (const juce::String& id, float plain)
        {
            auto* prm = proc.apvts.getParameter (id);
            if (std::abs (prm->convertFrom0to1 (prm->getValue()) - plain) > 1.0e-3f)
                prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        };
        sync (ids::signal, (float) (int) last.spec.signal);
        sync (ids::fftSize, (float) std::log2 (last.spec.N / 4096));
        sync (ids::level, (float) last.spec.levelDb);
        sync (ids::sineFreq, (float) last.spec.sineHz);
    }
    updateEnablement();

    const auto& s = last.spec;
    const double binHz = s.fs > 0 ? s.fs / s.N : 0.0;
    const double latMs = s.fs > 0 ? 1000.0 * last.delaySamples / s.fs : 0.0;
    const juce::String grp = juce::String (proc.paramIndex (ids::group));
    juce::String t;
    switch (last.view)
    {
        case View::Generator:
            t = U ("GERADOR · Grupo ") + grp + U (" · coloque um Analisador no mesmo grupo depois do plugin/equipamento a medir");
            break;
        case View::Response:
        case View::Harmonics:
            t = last.hostMode ? U ("HOST · ") + (last.hostLoaded ? last.hostName : U ("sem plugin"))
                              : (last.linked ? U ("Ligado ao Gerador do grupo ") + grp : U ("Sem Gerador no grupo ") + grp + U (" — definições próprias"));
            t << U (" · ") << signalName (s.signal) << " " << juce::String (s.levelDb, 1) << U (" dBFS · FFT ") << s.N
              << " (" << juce::String (binHz, 1) << " Hz/bin)";
            if (last.view == View::Response)
            {
                if (last.latencyReliable)
                    t << U (" · Latência: ") << juce::roundToInt (last.delaySamples) << U (" amostras (") << juce::String (latMs, 2) << " ms)";
                else
                    t << U (" · Latência: reproduza a sessão para medir");
            }
            else
                t << U (" · f0 = ") << juce::String (last.harm.f0, 1) << U (" Hz");
            t << U (" · Nível: ") << juce::String (last.levelDb, 1) << U (" dBFS");
            break;
        case View::Music:
            t = (last.hostMode ? U ("HOST · música através de ") + (last.hostLoaded ? last.hostName : U ("(sem plugin)"))
                               : U ("Música: sidechain (dry) vs entrada (wet)"))
              + U (" · FFT ") + juce::String (s.N)
              + U (" · Latência alinhada: ") + juce::String (juce::roundToInt (last.delaySamples)) + U (" amostras (")
              + juce::String (latMs, 2) + U (" ms) · Dry ") + juce::String (last.dryLevelDb, 1) + U (" dBFS · Wet ")
              + juce::String (last.levelDb, 1) + U (" dBFS");
            break;
        case View::None: break;
    }
    if (proc.paramIndex (ids::freeze) == 1 && proc.role() != Role::Generator) t = U ("CONGELADO · ") + t;
    if (t != status) { status = t; repaint (getLocalBounds().withTrimmedBottom (40).removeFromBottom (22)); }
}

void CurveAnalyzerEditor::exportCsv()
{
    AnalysisResults r;
    proc.getResults (r);
    const auto tab = effectiveTab();
    if (r.curve.empty() && r.sweep.empty()) return;

    chooser = std::make_unique<juce::FileChooser> (U ("Exportar (CSV)"),
                                                   juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                                       .getChildFile ("ANALYSER.csv"), "*.csv");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [r, tab] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;
        // Formato PT: separador ';' e vírgula decimal (abre direto no Excel em português)
        auto num = [] (double v, int dec) { return juce::String (v, dec).replaceCharacter ('.', ','); };
        juce::String out;
        if (tab == ViewTab::Sweep && ! r.sweep.empty())
        {
            out << (r.sweepShownKind == 2 ? U ("Frequência (Hz)") : U ("Nível (dBFS)")) << U (";THD (%);H2 (dBc);H3 (dBc);Ganho (dB)\n");
            for (auto& s : r.sweep)
                if (s.valid)
                    out << num (s.x, 1) << ";" << num (100.0 * std::pow (10.0, s.thdDb / 20.0), 5) << ";" << num (s.h2Db, 2)
                        << ";" << num (s.h3Db, 2) << ";" << num (s.gainDb, 3) << "\n";
        }
        else if (tab == ViewTab::Wave && ! r.waveOut.empty())
        {
            out << U ("Tempo (ms);Entrada;Saída\n");
            for (size_t i = 0; i < r.waveOut.size(); ++i)
                out << num (r.waveStartMs + r.waveSpanMs * (double) i / (double) juce::jmax ((size_t) 1, r.waveOut.size() - 1), 4) << ";"
                    << (i < r.waveIn.size() ? num (r.waveIn[i], 6) : juce::String()) << ";" << num (r.waveOut[i], 6) << "\n";
        }
        else if (tab == ViewTab::Spectrum && ! r.specWet.empty())
        {
            out << U ("Frequência (Hz);Antes (dB);Depois (dB)\n");
            for (size_t i = 0; i < r.specWet.freq.size(); ++i)
                if (r.specWet.valid[i])
                    out << num (r.specWet.freq[i], 2) << ";" << num (r.specDry.magDb[i], 2) << ";" << num (r.specWet.magDb[i], 2) << "\n";
        }
        else if (r.view == View::Harmonics)
        {
            out << U ("Harmónico;Frequência (Hz);dBFS;dBc\n");
            for (int k = 1; k <= pca::HarmonicResult::kMaxH; ++k)
                if (r.harm.present[k])
                    out << "H" << k << ";" << num (r.harm.f0 * k, 1) << ";" << num (r.harm.ampDb[k], 2) << ";" << num (r.harm.relDb[k], 2) << "\n";
            out << "THD %;" << num (r.harm.thdPct, 5) << "\nTHD+N %;" << num (r.harm.thdnPct, 5) << "\n\n";
            out << U ("Frequência (Hz);Espetro (dBFS)\n");
            for (size_t i = 0; i < r.curve.freq.size(); ++i)
                out << num (r.curve.freq[i], 2) << ";" << num (r.curve.magDb[i], 2) << "\n";
        }
        else
        {
            out << U ("Frequência (Hz);Magnitude (dB);Fase (graus)") << (r.view == View::Music ? U (";Coerência") : juce::String()) << "\n";
            for (size_t i = 0; i < r.curve.freq.size(); ++i)
            {
                if (! r.curve.valid[i]) continue;
                out << num (r.curve.freq[i], 2) << ";" << num (r.curve.magDb[i], 3) << ";" << num (r.curve.phaseDeg[i], 2);
                if (r.view == View::Music) out << ";" << num (r.curve.coh[i], 4);
                out << "\n";
            }
        }
        file.replaceWithText (out, false, false, "\r\n");
    });
}
