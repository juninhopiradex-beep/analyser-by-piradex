#include "SpectrumDisplay.h"

namespace
{
    constexpr float kFMin = 20.0f, kFMax = 20000.0f;
    constexpr float kDbTop = -6.0f, kDbBottom = -96.0f;
    constexpr float kTiltPerOct = 3.0f;       // inclinação visual (música fica "plana")

    void addSmoothCurve (juce::Path& p, const std::vector<juce::Point<float>>& pts, bool startNew)
    {
        if (pts.empty()) return;
        if (startNew) p.startNewSubPath (pts.front()); else p.lineTo (pts.front());
        for (size_t i = 1; i + 1 < pts.size(); ++i)
        {
            const auto mid = (pts[i] + pts[i + 1]) * 0.5f;
            p.quadraticTo (pts[i], mid);
        }
        if (pts.size() > 1) p.lineTo (pts.back());
    }

    juce::String freqLabel (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f || std::fmod (f, 1000.0f) < 0.5f ? 0 : 1) + "k"
                            : juce::String ((int) f);
    }
}

SpectrumDisplay::SpectrumDisplay (MixspaceProcessor& p)
    : proc (p),
      lowParam (p.apvts.getParameter (ids::lowCut)),
      highParam (p.apvts.getParameter (ids::highCut)),
      rangeParam (p.apvts.getParameter (ids::range))
{
    dMain.fill (-200.0f);
    dSc.fill (-200.0f);
    dRed.fill (0.0f);

    freezeButton.setClickingTogglesState (true);
    freezeButton.getProperties().set ("accent", (juce::int64) pal::sc.getARGB());
    lang.bind ([this]
    {
        freezeButton.setButtonText (tr ("Fixar", "Freeze"));
        freezeButton.setTooltip (tr ("Fixa o desenho do espectro (o processamento continua)", "Freezes the spectrum (processing continues)"));
    });
    lang.onChange = [this] { repaint(); };
    freezeButton.onClick = [this] { frozen = freezeButton.getToggleState(); };
    addAndMakeVisible (freezeButton);

    startTimerHz (30);
}

void SpectrumDisplay::resized()
{
    freezeButton.setBounds (getWidth() - 12 - 84, 7, 84, 20);
}

juce::Rectangle<float> SpectrumDisplay::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedRight (34.0f).withTrimmedTop (34.0f).withTrimmedBottom (22.0f);
}

float SpectrumDisplay::xForFreq (float f) const
{
    const auto r = plotArea();
    return r.getX() + r.getWidth() * std::log (juce::jlimit (kFMin, kFMax, f) / kFMin) / std::log (kFMax / kFMin);
}

float SpectrumDisplay::freqForX (float x) const
{
    const auto r = plotArea();
    return kFMin * std::pow (kFMax / kFMin, juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}

float SpectrumDisplay::yForLevel (float db, float f) const
{
    const auto r = plotArea();
    const float v = db + kTiltPerOct * std::log2 (f / 1000.0f);
    const float t = juce::jlimit (0.0f, 1.0f, (v - kDbBottom) / (kDbTop - kDbBottom));
    return r.getBottom() - t * r.getHeight();
}

float SpectrumDisplay::yForReduction (float db) const
{
    const auto r = plotArea();
    return r.getY() + juce::jlimit (0.0f, 12.0f, db) / 12.0f * r.getHeight() * 0.46f;
}

void SpectrumDisplay::timerCallback()
{
    if (! frozen)
    {
        auto& a = proc.analysis;
        nb = juce::jlimit (0, kMax, a.numBands.load());
        for (int b = 0; b < nb; ++b)
        {
            const size_t i = (size_t) b;
            freq[i] = a.freq[i].load (std::memory_order_relaxed);
            const float m = a.mainDb[i].load (std::memory_order_relaxed);
            const float s = a.scDb[i].load (std::memory_order_relaxed);
            const float r = a.redDb[i].load (std::memory_order_relaxed);
            // ataque rápido, queda suave (~27 dB/s) para leitura confortável
            dMain[i] = m > dMain[i] ? dMain[i] + (m - dMain[i]) * 0.6f : juce::jmax (m, dMain[i] - 0.9f);
            dSc[i]   = s > dSc[i]   ? dSc[i]   + (s - dSc[i]) * 0.6f   : juce::jmax (s, dSc[i] - 0.9f);
            dRed[i]  = dRed[i] + (r - dRed[i]) * 0.5f;
        }
        masking += (a.masking.load() - masking) * 0.25f;
        avgRed += (a.avgRed.load() - avgRed) * 0.25f;
    }
    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = plotArea();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e1114), 0.0f, 0.0f, pal::display, 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 10.0f);

    paintGrid (g, plot);
    paintZone (g, plot);
    paintCurves (g, plot);
    paintOverlay (g, plot);

    // moldura interior
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 9.0f, 1.0f);
}

void SpectrumDisplay::paintGrid (juce::Graphics& g, juce::Rectangle<float> plot)
{
    // linhas menores
    g.setColour (pal::grid);
    for (float dec : { 10.0f, 100.0f, 1000.0f, 10000.0f })
        for (int m = 2; m <= 9; ++m)
        {
            const float f = dec * (float) m;
            if (f > kFMin && f < kFMax) g.drawVerticalLine ((int) xForFreq (f), plot.getY(), plot.getBottom());
        }

    // linhas principais + legendas
    g.setFont (uiFont (10.0f, true, 0.05f));
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (pal::gridStrong);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (pal::textFaint);
        g.drawText (freqLabel (f), juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ x, plot.getBottom() + 11.0f }), juce::Justification::centred);
    }

    // níveis (escala do espectro, com inclinação de 3 dB/oitava referida a 1 kHz)
    for (float db = -18.0f; db >= kDbBottom + 1.0f; db -= 18.0f)
    {
        const float y = yForLevel (db, 1000.0f);
        g.setColour (pal::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (pal::textFaint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (plot.getX() - 32.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
    }

    // escala de redução (âmbar, à direita)
    g.setColour (pal::red.withAlpha (0.45f));
    for (float r : { 0.0f, 3.0f, 6.0f, 9.0f, 12.0f })
    {
        const float y = yForReduction (r);
        g.drawText (r < 0.5f ? "0" : "-" + juce::String ((int) r), juce::Rectangle<float> (plot.getRight() + 4.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredLeft);
    }
    g.setColour (pal::red.withAlpha (0.10f));
    g.drawHorizontalLine ((int) yForReduction (rangeParam->convertFrom0to1 (rangeParam->getValue())), plot.getX(), plot.getRight());
}

void SpectrumDisplay::paintZone (juce::Graphics& g, juce::Rectangle<float> plot)
{
    const float lo = lowParam->convertFrom0to1 (lowParam->getValue());
    const float hi = highParam->convertFrom0to1 (highParam->getValue());
    const float xl = xForFreq (lo), xh = xForFreq (hi);

    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (plot.getX(), plot.getY(), juce::jmax (plot.getX(), xl), plot.getBottom()));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (plot.getRight(), xh), plot.getY(), plot.getRight(), plot.getBottom()));

    auto edge = [&] (float x, int which, const juce::String& label, float f)
    {
        const bool active = hoverEdge == which || dragEdge == which;
        g.setColour ((active ? pal::red : pal::textDim).withAlpha (active ? 0.9f : 0.45f));
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (x, plot.getY(), x, plot.getBottom()), dashes, 2, 1.0f);
        const auto tab = juce::Rectangle<float> (8.0f, 18.0f).withCentre ({ x, plot.getCentreY() });
        g.setColour (active ? pal::red : juce::Colour (0xff3a4048));
        g.fillRoundedRectangle (tab, 3.0f);
        if (active)
        {
            g.setFont (uiFont (10.5f, true, 0.08f));
            const auto txt = label + "  " + (f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String ((int) std::round (f)) + " Hz");
            const float w = 120.0f;
            const float tx = which == 1 ? x + 8.0f : x - 8.0f - w;
            g.drawText (txt, juce::Rectangle<float> (tx, plot.getBottom() - 20.0f, w, 14.0f),
                        which == 1 ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }
    };
    edge (xl, 1, tr ("GRAVES", "LOW CUT"), lo);
    edge (xh, 2, tr ("AGUDOS", "HIGH CUT"), hi);
}

void SpectrumDisplay::paintCurves (juce::Graphics& g, juce::Rectangle<float> plot)
{
    if (nb < 2) return;

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    std::vector<juce::Point<float>> pm, ps, pr;
    pm.reserve ((size_t) nb + 2); ps.reserve ((size_t) nb + 2); pr.reserve ((size_t) nb + 2);
    pm.push_back ({ plot.getX(), yForLevel (dMain[0], freq[0]) });
    ps.push_back ({ plot.getX(), yForLevel (dSc[0], freq[0]) });
    pr.push_back ({ plot.getX(), yForReduction (dRed[0]) });
    bool anyRed = false;
    for (int b = 0; b < nb; ++b)
    {
        const size_t i = (size_t) b;
        const float x = xForFreq (freq[i]);
        pm.push_back ({ x, yForLevel (dMain[i], freq[i]) });
        ps.push_back ({ x, yForLevel (dSc[i], freq[i]) });
        pr.push_back ({ x, yForReduction (dRed[i]) });
        anyRed = anyRed || dRed[i] > 0.02f;
    }
    pm.push_back ({ plot.getRight(), pm.back().y });
    ps.push_back ({ plot.getRight(), ps.back().y });
    pr.push_back ({ plot.getRight(), pr.back().y });

    // MAIN — preenchimento suave
    {
        juce::Path line;
        addSmoothCurve (line, pm, true);
        juce::Path fill (line);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (pal::main.withAlpha (0.16f), 0.0f, plot.getY() + plot.getHeight() * 0.3f,
                                                 pal::main.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (pal::main.withAlpha (0.55f));
        g.strokePath (line, juce::PathStrokeType (1.2f));
    }

    // SIDECHAIN — linha ciano com brilho
    if (proc.isSidechainConnected())
    {
        juce::Path line;
        addSmoothCurve (line, ps, true);
        g.setColour (pal::sc.withAlpha (0.12f));
        g.strokePath (line, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (pal::sc.withAlpha (0.9f));
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // REDUÇÃO — área âmbar a descer do topo
    if (anyRed)
    {
        const float y0 = plot.getY();
        juce::Path curve;
        addSmoothCurve (curve, pr, true);
        juce::Path area;
        area.startNewSubPath (plot.getX(), y0);
        addSmoothCurve (area, pr, false);
        area.lineTo (plot.getRight(), y0);
        area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (pal::red.withAlpha (0.10f), 0.0f, y0,
                                                 pal::redHot.withAlpha (0.55f), 0.0f, yForReduction (12.0f), false));
        g.fillPath (area);
        g.setColour (pal::red);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.restoreState();
}

void SpectrumDisplay::paintOverlay (juce::Graphics& g, juce::Rectangle<float> plot)
{
    // legenda
    float x = plot.getX();
    const float y = 10.0f;
    g.setFont (uiFont (10.5f, true, 0.12f));
    auto legend = [&] (juce::Colour c, const juce::String& t)
    {
        g.setColour (c);
        g.fillRoundedRectangle (x, y + 5.0f, 14.0f, 3.0f, 1.5f);
        g.setColour (pal::textDim);
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + 4.0f;
        g.drawText (t, juce::Rectangle<float> (x + 20.0f, y, w, 14.0f), juce::Justification::centredLeft);
        x += 20.0f + w + 18.0f;
    };
    legend (pal::main, tr ("PRINCIPAL", "MAIN"));
    legend (pal::sc, "SIDECHAIN");
    legend (pal::red, tr ("REDUÇÃO", "REDUCTION"));

    // masking meter + redução média
    const float right = (float) freezeButton.getX() - 16.0f;
    const int segs = 12;
    const float segW = 5.0f, gap = 2.0f, meterW = segs * (segW + gap);
    const float mx = right - 34.0f - meterW;
    g.setColour (pal::textDim);
    g.drawText (tr ("COLISÃO", "MASKING"), juce::Rectangle<float> (mx - 70.0f, y, 64.0f, 14.0f), juce::Justification::centredRight);
    const int lit = (int) std::round (juce::jlimit (0.0f, 1.0f, masking) * segs);
    for (int i = 0; i < segs; ++i)
    {
        const float t = (float) i / (float) (segs - 1);
        const auto c = pal::sc.interpolatedWith (pal::red, t);
        g.setColour (i < lit ? c : pal::ledOff);
        g.fillRoundedRectangle (mx + (float) i * (segW + gap), y + 2.0f, segW, 10.0f, 1.0f);
    }
    g.setColour (pal::text);
    g.drawText (juce::String (juce::roundToInt (masking * 100.0f)) + "%", juce::Rectangle<float> (right - 32.0f, y, 32.0f, 14.0f), juce::Justification::centredRight);

    const float avgX = mx - 70.0f - 120.0f;
    g.setColour (avgRed > 0.05f ? pal::red : pal::textFaint);
    g.drawText (tr ("MÉDIA  ", "AVG  ") + juce::String (avgRed > 0.05f ? "-" : "") + juce::String (avgRed, 1) + " dB",
                juce::Rectangle<float> (avgX, y, 104.0f, 14.0f), juce::Justification::centredRight);

    // avisos de sidechain
    if (! proc.isSidechainConnected())
    {
        const auto box = juce::Rectangle<float> (440.0f, 78.0f).withCentre (plot.getCentre());
        g.setColour (juce::Colour (0xe0101316));
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (pal::red.withAlpha (0.6f));
        g.drawRoundedRectangle (box, 10.0f, 1.0f);
        g.setColour (pal::red);
        g.setFont (uiFont (17.0f, true, 0.22f));
        g.drawText (tr ("SIDECHAIN NÃO LIGADO", "SIDECHAIN NOT CONNECTED"), box.withTrimmedBottom (32.0f).withTrimmedTop (8.0f), juce::Justification::centred);
        g.setColour (pal::textDim);
        g.setFont (uiFont (12.0f));
        g.drawText (tr ("Envie a voz (ou o sinal prioritário) para a entrada sidechain do MIXSPACE.", "Send the vocal (or the priority signal) to the MIXSPACE sidechain input."),
                    box.withTrimmedTop (42.0f).withTrimmedBottom (10.0f), juce::Justification::centred);
    }
    else if (proc.isSidechainSilent() && ! proc.isMainSilent())
    {
        g.setColour (pal::textDim);
        g.setFont (uiFont (11.0f, true, 0.18f));
        g.drawText (tr ("SEM SINAL NO SIDECHAIN", "NO SIDECHAIN SIGNAL"), juce::Rectangle<float> (300.0f, 16.0f).withCentre ({ plot.getCentreX(), plot.getY() + 14.0f }), juce::Justification::centred);
    }

    if (frozen)
    {
        g.setColour (pal::sc.withAlpha (0.8f));
        g.setFont (uiFont (10.0f, true, 0.2f));
        g.drawText (tr ("FIXADO", "FROZEN"), juce::Rectangle<float> (plot.getRight() - 60.0f, plot.getBottom() - 18.0f, 56.0f, 14.0f), juce::Justification::centredRight);
    }
}

//==============================================================================
int SpectrumDisplay::edgeAt (juce::Point<float> p) const
{
    const auto plot = plotArea();
    if (p.y < plot.getY() || p.y > plot.getBottom()) return 0;
    const float xl = xForFreq (lowParam->convertFrom0to1 (lowParam->getValue()));
    const float xh = xForFreq (highParam->convertFrom0to1 (highParam->getValue()));
    const float dl = std::abs (p.x - xl), dh = std::abs (p.x - xh);
    if (dl < 7.0f && dl <= dh) return 1;
    if (dh < 7.0f) return 2;
    return 0;
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = edgeAt (e.position);
    if (h != hoverEdge) { hoverEdge = h; repaint(); }
    setMouseCursor (h != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&)
{
    hoverEdge = 0;
    repaint();
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragEdge = edgeAt (e.position);
    if (dragEdge == 1) lowParam->beginChangeGesture();
    if (dragEdge == 2) highParam->beginChangeGesture();
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEdge == 0) return;
    auto* prm = dragEdge == 1 ? lowParam : highParam;
    const auto range = prm->getNormalisableRange();
    const float f = juce::jlimit (range.start, range.end, freqForX (e.position.x));
    prm->setValueNotifyingHost (prm->convertTo0to1 (f));
    repaint();
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragEdge == 1) lowParam->endChangeGesture();
    if (dragEdge == 2) highParam->endChangeGesture();
    dragEdge = 0;
}

void SpectrumDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int edge = edgeAt (e.position);
    if (edge == 0) return;
    auto* prm = edge == 1 ? lowParam : highParam;
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getDefaultValue());
    prm->endChangeGesture();
}
