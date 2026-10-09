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
    g.setColour (juce::Colours::black.withAlpha (0.75f));
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

void GraphView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    const auto p = plotArea();

    switch (res.view)
    {
        case View::Generator:  drawFreqGrid (g); drawGenerator (g); break;
        case View::Harmonics:  drawFreqGrid (g); drawHarmonics (g); break;
        case View::Response:
        case View::Music:      drawFreqGrid (g); drawResponse (g); break;
        case View::None:       drawFreqGrid (g); drawMessage (g, U ("A iniciar…"), U ("A aguardar o primeiro bloco de áudio do host.")); break;
    }

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
    drawMessage (g, U ("MODO GERADOR — a emitir sinal de teste"), sub);
}

void GraphView::drawResponse (juce::Graphics& g)
{
    const auto p = plotArea();
    const float R = (float) CurveAnalyzerProcessor::rangeForIndex (proc.paramIndex (ids::range));
    const bool showPhase = proc.paramIndex (ids::showPhase) == 1;
    auto yDb = [p, R] (float db) { return p.getCentreY() - juce::jlimit (-1.2f, 1.2f, db / R) * p.getHeight() * 0.5f; };
    auto yPh = [p] (float deg) { return p.getCentreY() - deg / 180.0f * p.getHeight() * 0.5f; };

    // Grelha dB
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

    if (res.sidechainMissing)
    {
        drawMessage (g, U ("Sidechain inativo"),
                     U ("Ative o sidechain desta instância e envie para ele o sinal ORIGINAL (antes do plugin/hardware). "
                        "A entrada principal deve receber o sinal processado."));
        return;
    }
    if (! res.hasSignal)
    {
        if (res.view == View::Music)
            drawMessage (g, U ("À espera de música…"), U ("Reproduza a sessão: o sidechain recebe o dry e a entrada o wet."));
        else
            drawMessage (g, U ("À espera do sinal de teste…"),
                         U ("Coloque uma instância em modo Gerador ANTES do plugin a medir (mesmo grupo) e "
                            "reproduza/monitorize a faixa. Esta instância fica DEPOIS do plugin."));
        return;
    }

    g.saveState();
    g.reduceClipRegion (p.toNearestInt());

    // Coerência (modo música): faixa inferior, 0..1
    if (res.view == View::Music)
    {
        auto yC = [p] (float c) { return p.getBottom() - juce::jlimit (0.0f, 1.0f, c) * p.getHeight() * 0.18f; };
        juce::Path cp; bool d = false;
        for (size_t i = 0; i < res.curve.freq.size(); ++i)
        {
            if (! res.curve.valid[i]) { d = false; continue; }
            const float x = xForFreq (res.curve.freq[i]), y = yC (res.curve.coh[i]);
            if (! d) { cp.startNewSubPath (x, y); d = true; } else cp.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff6d6d6d));
        g.strokePath (cp, juce::PathStrokeType (1.0f));
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (U ("coerência"), juce::Rectangle<float> (p.getX() + 6.0f, p.getBottom() - p.getHeight() * 0.18f - 16.0f, 100.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    // Referências guardadas
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
        g.strokePath (curvePath (res.curve, yPh, true), juce::PathStrokeType (1.6f));
    }
    g.setColour (pxcol::green);
    g.strokePath (curvePath (res.curve, yDb, false),
                  juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();

    if (hovering && p.contains (hover) && ! res.curve.empty())
    {
        const float f = freqForX (hover.x);
        size_t best = 0;
        for (size_t i = 1; i < res.curve.freq.size(); ++i)
            if (std::abs (std::log (res.curve.freq[i] / f)) < std::abs (std::log (res.curve.freq[best] / f))) best = i;
        if (res.curve.valid[best])
        {
            juce::String t = juce::String (juce::roundToInt (f)) + " Hz, " + juce::String (res.curve.magDb[best], 1) + " dB";
            if (showPhase) t << ", " << juce::roundToInt (res.curve.phaseDeg[best]) << U (" graus");
            if (res.view == View::Music) t << U (", coer. ") << juce::String (res.curve.coh[best], 2);
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

    if (! res.hasSignal)
    {
        drawMessage (g, U ("À espera do seno de teste…"),
                     U ("Gerador em \"Seno (harmónicos)\" antes do plugin/equipamento, Analisador depois. "
                        "Suba o Nível no Gerador para levar o equipamento à saturação."));
        return;
    }

    const auto& h = res.harm;
    g.saveState();
    g.reduceClipRegion (p.toNearestInt());

    // Espetro
    juce::Path sp = curvePath (res.curve, yDb, false);
    juce::Path fill = sp;
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();
    g.setColour (pxcol::green.withAlpha (0.07f));
    g.fillPath (fill);
    g.setColour (pxcol::green.withAlpha (0.9f));
    g.strokePath (sp, juce::PathStrokeType (1.3f));

    // Piso de ruído
    if (h.noiseFloorDb > -200.0)
    {
        const float y = yDb ((float) h.noiseFloorDb);
        const float dashes[] { 4.0f, 4.0f };
        g.setColour (pxcol::dim.withAlpha (0.6f));
        g.drawDashedLine (juce::Line<float> (p.getX(), y, p.getRight(), y), dashes, 2, 1.0f);
    }

    // Marcadores dos harmónicos
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

    // Painel de leitura
    // Painel no lado onde não há harmónicos (f0 alto -> esquerda, f0 baixo -> direita)
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

    setupCombo (role,      "Papel",  ids::role);
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
    latency.setTooltip (U ("Latência extra (amostras) somada à detetada pelo Auto Sync"));

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

    addAndMakeVisible (graph);

    login.onUnlocked = [this] { login.setVisible (false); repaint(); };
    addChildComponent (login);
    login.setVisible (! lic::License::get().isUnlocked());

    setResizable (true, true);
    setResizeLimits (960, 560, 2600, 1500);
    setSize (1240, 700);
    startTimerHz (30);
}

CurveAnalyzerEditor::~CurveAnalyzerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
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
    auto r = getLocalBounds();

    // separadores do cabeçalho
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (r.removeFromTop (100));

    // rodapé com a marca e o estado
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

    // linha de estado (por cima do rodapé)
    auto st = getLocalBounds().withTrimmedBottom (40).removeFromBottom (22).reduced (14, 0);
    g.setColour (pxcol::dim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawFittedText (status, st, juce::Justification::centredLeft, 1);
}

void CurveAnalyzerEditor::resized()
{
    const int margin = 12, rowH = 32, gap = 8;

    // Coloca uma fila de itens (label/controlo) com larguras desejadas, escaladas se faltar espaço
    struct Item { juce::Component* c; int w; };
    auto placeRow = [&] (int y, std::vector<Item> left, std::vector<Item> right)
    {
        int need = 0;
        for (auto& i : left)  need += i.w + gap;
        for (auto& i : right) need += i.w + gap;
        const int avail = getWidth() - 2 * margin - 24;
        const float k = need > avail ? (float) avail / (float) need : 1.0f;
        int x = margin;
        for (auto& i : left) { const int w = (int) (i.w * k); i.c->setBounds (x, y, w, rowH); x += w + (int) (gap * k); }
        x = getWidth() - margin;
        for (auto it = right.rbegin(); it != right.rend(); ++it)
        {
            const int w = (int) (it->w * k);
            x -= w;
            it->c->setBounds (x, y, w, rowH);
            x -= (int) (gap * k);
        }
    };

    placeRow (14,
              { { &role.label, 48 }, { &role.box, 122 }, { &group.label, 50 }, { &group.box, 58 },
                { &signal.label, 46 }, { &signal.box, 172 }, { &levelLabel, 46 }, { &level, 104 },
                { &fft.label, 36 }, { &fft.box, 86 }, { &sineLabel, 44 }, { &sine, 104 } },
              { { &phaseBtn, 66 }, { &syncBtn, 108 }, { &muteBtn, 112 } });

    placeRow (56,
              { { &source.label, 48 }, { &source.box, 164 }, { &channel.label, 50 }, { &channel.box, 104 },
                { &averages.label, 50 }, { &averages.box, 66 }, { &smoothing.label, 62 }, { &smoothing.box, 108 },
                { &range.label, 52 }, { &range.box, 94 }, { &latencyLabel, 66 }, { &latency, 128 } },
              {});

    // Botões de ação no rodapé, à direita
    auto foot = getLocalBounds().removeFromBottom (40).reduced (margin, 5);
    for (juce::Button* b : std::initializer_list<juce::Button*> { &exportBtn, &clearRefBtn, &refBtn, &resetBtn, &freezeBtn })
    {
        const int w = b == &clearRefBtn ? 112 : (b == &exportBtn || b == &freezeBtn ? 98 : 72);
        b->setBounds (foot.removeFromRight (w));
        foot.removeFromRight (8);
    }

    graph.setBounds (getLocalBounds().withTrimmedTop (100).withTrimmedBottom (40 + 22).reduced (margin, 4));
    login.setBounds (getLocalBounds());
}

void CurveAnalyzerEditor::updateEnablement()
{
    const bool gen = proc.paramIndex (ids::role) == 1;
    const bool music = proc.paramIndex (ids::source) == 1;
    const bool linked = last.linked;
    const bool sineSig = proc.paramIndex (ids::signal) == 3;

    // Sinal: editável no Gerador; no Analisador só quando não há Gerador ligado (e não em modo música)
    const bool sigEditable = gen || (! linked && ! music);
    for (juce::Component* c : { (juce::Component*) &signal.box, (juce::Component*) &level, (juce::Component*) &fft.box })
        c->setEnabled (sigEditable || (c == &fft.box && music && ! gen));
    sine.setEnabled (sigEditable && sineSig);

    for (juce::Component* c : { (juce::Component*) &source.box, (juce::Component*) &channel.box, (juce::Component*) &averages.box,
                                (juce::Component*) &smoothing.box, (juce::Component*) &range.box, (juce::Component*) &latency,
                                (juce::Component*) &phaseBtn, (juce::Component*) &syncBtn, (juce::Component*) &freezeBtn,
                                (juce::Component*) &muteBtn, (juce::Component*) &resetBtn, (juce::Component*) &refBtn,
                                (juce::Component*) &clearRefBtn, (juce::Component*) &exportBtn })
        c->setEnabled (! gen);
}

void CurveAnalyzerEditor::timerCallback()
{
    // Outra instância desbloqueou, ou a senha beta expirou entretanto
    const bool locked = ! lic::License::get().isUnlocked();
    if (login.isVisible() != locked) { login.setVisible (locked); repaint(); }

    AnalysisResults r;
    proc.getResults (r);
    const bool changed = r.serial != last.serial;
    last = std::move (r);
    if (changed) graph.setResults (last);

    // Quando ligado a um Gerador, os controlos de sinal desta instância passam a mostrar
    // (e guardar) as definições reais do Gerador — evita confusão e serve de reserva.
    if (last.linked && proc.paramIndex (ids::role) == 0 && proc.paramIndex (ids::source) == 0)
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
            t = (last.linked ? U ("Ligado ao Gerador do grupo ") + grp : U ("Sem Gerador no grupo ") + grp + U (" — a usar definições próprias"))
              + U (" · ") + signalName (s.signal) + " " + juce::String (s.levelDb, 1) + U (" dBFS · FFT ") + juce::String (s.N)
              + " (" + juce::String (binHz, 1) + " Hz/bin)";
            if (last.view == View::Response)
                t << U (" · Latência: ") << juce::roundToInt (last.delaySamples) << U (" amostras (") << juce::String (latMs, 2) << " ms)";
            else
                t << U (" · f0 = ") << juce::String (last.harm.f0, 1) << U (" Hz (centrado no bin)");
            t << U (" · Nível: ") << juce::String (last.levelDb, 1) << U (" dBFS · Blocos: ") << last.frames;
            break;
        case View::Music:
            t = U ("Música: sidechain (dry) vs entrada (wet) · FFT ") + juce::String (s.N)
              + U (" · Latência alinhada: ") + juce::String (juce::roundToInt (last.delaySamples)) + U (" amostras (")
              + juce::String (latMs, 2) + U (" ms) · Dry ") + juce::String (last.dryLevelDb, 1) + U (" dBFS · Wet ")
              + juce::String (last.levelDb, 1) + U (" dBFS · Blocos: ") + juce::String (last.frames);
            break;
        case View::None: break;
    }
    if (proc.paramIndex (ids::freeze) == 1 && ! proc.paramIndex (ids::role)) t = U ("CONGELADO · ") + t;
    if (t != status) { status = t; repaint (getLocalBounds().withTrimmedBottom (40).removeFromBottom (22)); }
}

void CurveAnalyzerEditor::exportCsv()
{
    AnalysisResults r;
    proc.getResults (r);
    if (r.curve.empty()) return;

    chooser = std::make_unique<juce::FileChooser> (U ("Exportar curva (CSV)"),
                                                   juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                                       .getChildFile ("Piradex_Curve.csv"), "*.csv");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [r] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;
        // Formato PT: separador ';' e vírgula decimal (abre direto no Excel em português)
        auto num = [] (double v, int dec) { return juce::String (v, dec).replaceCharacter ('.', ','); };
        juce::String out;
        if (r.view == View::Harmonics)
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
