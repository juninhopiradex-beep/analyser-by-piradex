// Identidade visual do MIXSPACE by Piradex: grafite escuro, controlos maquinados,
// arco de valor âmbar, sidechain em ciano, main em branco quente.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace pal
{
    const juce::Colour bgTop      { 0xff1c1f23 };
    const juce::Colour bgBottom   { 0xff111316 };
    const juce::Colour plate      { 0xff171a1e };
    const juce::Colour plateEdge  { 0xff262b31 };
    const juce::Colour display    { 0xff0a0c0e };
    const juce::Colour grid       { 0xff171c21 };
    const juce::Colour gridStrong { 0xff232a31 };
    const juce::Colour text       { 0xffd9dde2 };
    const juce::Colour textDim    { 0xff858d98 };
    const juce::Colour textFaint  { 0xff4b535c };
    const juce::Colour main       { 0xffece6da };
    const juce::Colour sc         { 0xff40d3f2 };
    const juce::Colour red        { 0xffff9c3d };
    const juce::Colour redHot     { 0xffff6b2c };
    const juce::Colour ledOff     { 0xff2b3036 };
    const juce::Colour bypass     { 0xffff5b4f };
}

inline juce::Font uiFont (float size, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

class MixspaceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MixspaceLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1d21));
        setColour (juce::PopupMenu::textColourId, pal::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::red.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, pal::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const bool big = (bool) s.getProperties().getWithDefault ("big", false);
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float size = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float rOuter = size * 0.5f - 2.0f;
        const float arcR = rOuter - (big ? 5.0f : 4.0f);
        const float arcW = big ? 4.0f : 3.0f;
        const float bodyR = rOuter * (big ? 0.74f : 0.70f);
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool enabled = s.isEnabled();

        // marcas de escala
        const int ticks = big ? 21 : 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
            const bool major = (i % (big ? 5 : 5)) == 0;
            const float r1 = rOuter + (big ? 1.0f : 0.5f), r0 = r1 - (major ? 4.0f : 2.5f);
            g.setColour (major ? pal::textFaint.brighter (0.2f) : pal::textFaint.withAlpha (0.7f));
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), major ? 1.2f : 0.8f);
        }

        // trilho e arco de valor
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR - 3.0f, arcR - 3.0f, 0.0f, startAngle, endAngle, true);
        g.setColour (pal::ledOff);
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (pos > 0.001f && enabled)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR - 3.0f, arcR - 3.0f, 0.0f, startAngle, angle, true);
            g.setColour (pal::red.withAlpha (0.18f));
            g.strokePath (val, juce::PathStrokeType (arcW + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (pal::red, c.x - arcR, c.y + arcR, pal::redHot, c.x + arcR, c.y - arcR, false));
            g.strokePath (val, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // sombra e corpo maquinado
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f + 6.0f, bodyR * 2.0f + 6.0f).withCentre (c.translated (0.0f, 3.0f)));
        const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d424a), c.x, body.getY(), juce::Colour (0xff17191d), c.x, body.getBottom(), false));
        g.fillEllipse (body);
        const auto face = body.reduced (bodyR * 0.09f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2f35), c.x, face.getY(), juce::Colour (0xff1e2125), c.x, face.getBottom(), false));
        g.fillEllipse (face);
        for (int i = 1; i <= 7; ++i)   // escovado concêntrico subtil
        {
            g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.018f : 0.010f));
            g.drawEllipse (face.reduced ((float) i * face.getWidth() * 0.055f), 1.0f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        // ponteiro
        const float p0 = bodyR * (big ? 0.66f : 0.55f), p1 = bodyR * 0.90f;
        g.setColour (enabled ? pal::main : pal::textFaint);
        g.drawLine (c.x + p0 * std::sin (angle), c.y - p0 * std::cos (angle),
                    c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle), big ? 3.0f : 2.2f);

        // valor no centro
        auto txt = s.getTextFromValue (s.getValue());
        juce::String num = txt, unit;
        if (txt.containsChar (' ')) { num = txt.upToLastOccurrenceOf (" ", false, false); unit = txt.fromLastOccurrenceOf (" ", false, false); }
        const float numSize = big ? size * 0.19f : size * 0.145f;
        const float us = juce::jmax (8.5f, size * (big ? 0.062f : 0.078f));
        const float lift = unit.isEmpty() ? 0.0f : us * 0.55f;
        g.setColour (enabled ? pal::text : pal::textFaint);
        g.setFont (uiFont (numSize, true));
        const auto numBox = juce::Rectangle<float> (bodyR * 1.5f, numSize * 1.1f).withCentre (c.translated (0.0f, -lift));
        g.drawText (num, numBox, juce::Justification::centred, false);
        if (unit.isNotEmpty())
        {
            g.setColour (pal::textDim);
            g.setFont (uiFont (us, true, 0.12f));
            const auto unitBox = juce::Rectangle<float> (bodyR * 1.5f, us * 1.3f)
                                     .withCentre ({ c.x, c.y - lift + numSize * 0.5f + us * 0.85f });
            g.drawText (unit, unitBox, juce::Justification::centred, false);
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const bool segment = (bool) b.getProperties().getWithDefault ("segment", false);
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) pal::red.getARGB()));
        const float corner = r.getHeight() * (segment ? 0.22f : 0.5f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262a30).brighter (over ? 0.08f : 0.0f), 0.0f, r.getY(),
                                                 juce::Colour (0xff1b1e22).darker (down ? 0.2f : 0.0f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        if (on && segment)
        {
            g.setColour (accent.withAlpha (0.16f));
            g.fillRoundedRectangle (r, corner);
        }
        g.setColour (on ? accent.withAlpha (segment ? 0.75f : 0.55f) : pal::plateEdge.brighter (0.15f));
        g.drawRoundedRectangle (r, corner, 1.0f);

        if (! segment && (bool) b.getProperties().getWithDefault ("led", true))
        {
            const float d = 6.0f;
            const auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
            if (on)
            {
                g.setColour (accent.withAlpha (0.35f));
                g.fillEllipse (led.expanded (3.0f));
            }
            g.setColour (on ? accent : pal::ledOff);
            g.fillEllipse (led);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        const bool segment = (bool) b.getProperties().getWithDefault ("segment", false);
        const bool led = ! segment && (bool) b.getProperties().getWithDefault ("led", true);
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) pal::red.getARGB()));
        auto r = b.getLocalBounds();
        if (led) r.removeFromLeft (r.getHeight() - 4);
        g.setFont (uiFont (segment ? 11.0f : 11.5f, true, 0.14f));
        g.setColour (on ? (segment ? accent.brighter (0.3f) : pal::text) : pal::textDim);
        g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred, false);
    }

    juce::Font getPopupMenuFont() override { return uiFont (14.0f); }
};
