// =============================================================================
//  EQMotion — identidade visual (paleta, tipografia, LookAndFeel, ícones)
//  Grafite profundo com âmbar "motion" e cores de banda próprias.
// =============================================================================
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"

namespace pal
{
using juce::Colour;
const Colour bg0      { 0xff0a0c10 };   // fundo da janela
const Colour bg1      { 0xff11141a };   // painéis
const Colour bg2      { 0xff181c24 };   // elevados
const Colour bg3      { 0xff20252f };   // controlos
const Colour line     { 0xff262c37 };
const Colour lineSoft { 0xff1a1f28 };
const Colour grid     { 0xff1b2029 };
const Colour text     { 0xffece8e1 };
const Colour textDim  { 0xff9ca2ad };
const Colour textFaint{ 0xff5f6672 };
const Colour accent   { 0xffffb547 };   // âmbar EQMotion
const Colour accent2  { 0xffff6b4a };   // coral (Delta, Listen, alertas)
const Colour ok       { 0xff52d69a };
const Colour curve    { 0xfff6f1e7 };
const Colour pre      { 0xff6e7c92 };
const Colour postFill { 0xff2c3646 };
const Colour sc       { 0xffb58cff };
const Colour mid      { 0xffffd08a };
const Colour side     { 0xff8fd3ff };

inline Colour band (int i)
{
    static const Colour c[] = { Colour (0xffffb547), Colour (0xffff7a59), Colour (0xffff5c8a), Colour (0xffd46cff),
                                Colour (0xff8c7bff), Colour (0xff4fa3ff), Colour (0xff3fd0e0), Colour (0xff3ee0a1),
                                Colour (0xffa6e35a), Colour (0xfff2d54b), Colour (0xffffa07a), Colour (0xffc9a7ff) };
    return c[(size_t) (i % 12)];
}
}

namespace fonts
{
inline juce::Typeface::Ptr face (int w)
{
    static juce::Typeface::Ptr r = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    static juce::Typeface::Ptr s = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    static juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    switch (w) { case 1: return m; case 2: return s; case 3: return mono; default: return r; }
}
// w: 0 normal, 1 medium, 2 semibold
inline juce::Font ui (float h, int w = 0, float tracking = 0.0f)
{
    return juce::Font (juce::FontOptions (face (w)).withHeight (h).withKerningFactor (tracking));
}
inline juce::Font mono (float h) { return juce::Font (juce::FontOptions (face (3)).withHeight (h)); }
}

// Ícones simples desenhados em caminho (0..1)
namespace icons
{
inline juce::Path undo()
{
    juce::Path p;
    p.startNewSubPath (0.82f, 0.78f);
    p.cubicTo (0.86f, 0.42f, 0.62f, 0.26f, 0.30f, 0.30f);
    p.startNewSubPath (0.42f, 0.14f); p.lineTo (0.24f, 0.31f); p.lineTo (0.44f, 0.46f);
    return p;
}
inline juce::Path redo() { auto p = undo(); p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, 0.5f, 0.5f)); return p; }
inline juce::Path copy()
{
    juce::Path p;
    p.addRoundedRectangle (0.32f, 0.30f, 0.50f, 0.56f, 0.08f);
    p.startNewSubPath (0.20f, 0.66f); p.lineTo (0.20f, 0.20f); p.lineTo (0.62f, 0.20f);
    return p;
}
inline juce::Path power()
{
    juce::Path p;
    p.addCentredArc (0.5f, 0.55f, 0.32f, 0.32f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    p.startNewSubPath (0.5f, 0.12f); p.lineTo (0.5f, 0.50f);
    return p;
}
inline juce::Path gear()
{
    juce::Path p;
    p.addEllipse (0.36f, 0.36f, 0.28f, 0.28f);
    for (int i = 0; i < 8; ++i)
    {
        const float a = i * juce::MathConstants<float>::twoPi / 8.0f;
        p.startNewSubPath (0.5f + 0.24f * std::sin (a), 0.5f - 0.24f * std::cos (a));
        p.lineTo (0.5f + 0.38f * std::sin (a), 0.5f - 0.38f * std::cos (a));
    }
    return p;
}
inline juce::Path layers()
{
    juce::Path p;
    p.startNewSubPath (0.5f, 0.18f); p.lineTo (0.86f, 0.38f); p.lineTo (0.5f, 0.58f); p.lineTo (0.14f, 0.38f); p.closeSubPath();
    p.startNewSubPath (0.14f, 0.56f); p.lineTo (0.5f, 0.76f); p.lineTo (0.86f, 0.56f);
    return p;
}
inline juce::Path spark()   // Smart Ops
{
    juce::Path p;
    p.startNewSubPath (0.5f, 0.10f); p.quadraticTo (0.54f, 0.46f, 0.90f, 0.5f); p.quadraticTo (0.54f, 0.54f, 0.5f, 0.90f);
    p.quadraticTo (0.46f, 0.54f, 0.10f, 0.5f); p.quadraticTo (0.46f, 0.46f, 0.5f, 0.10f); p.closeSubPath();
    return p;
}
inline juce::Path star (bool filled)
{
    juce::Path p;
    p.addStar ({ 0.5f, 0.52f }, 5, 0.18f, 0.44f, 0.0f);
    juce::ignoreUnused (filled);
    return p;
}
inline juce::Path ear()   // Listen
{
    juce::Path p;
    p.startNewSubPath (0.30f, 0.40f);
    p.cubicTo (0.30f, 0.12f, 0.74f, 0.10f, 0.74f, 0.40f);
    p.cubicTo (0.74f, 0.58f, 0.56f, 0.60f, 0.56f, 0.74f);
    p.cubicTo (0.56f, 0.90f, 0.36f, 0.90f, 0.34f, 0.78f);
    return p;
}

// Forma de cada tipo de filtro (mini-curva para os botões)
inline juce::Path filterShape (int type, juce::Rectangle<float> r)
{
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), w = r.getWidth(), yc = r.getCentreY(), h = r.getHeight();
    auto pt = [&] (float fx, float fy) { return juce::Point<float> (x0 + fx * w, yc - fy * h * 0.42f); };
    const int N = 28;
    for (int i = 0; i <= N; ++i)
    {
        const float t = (float) i / N, x = t * 2.0f - 1.0f;
        float y = 0;
        switch (type)
        {
            case 0: y = std::exp (-x * x * 9.0f); break;                                   // sino
            case 1: y = 0.8f / (1.0f + std::exp ((x + 0.1f) * 9.0f)); break;              // low shelf
            case 2: y = 0.8f / (1.0f + std::exp (-(x - 0.1f) * 9.0f)); break;             // high shelf
            case 3: y = -1.2f / (1.0f + std::exp ((x + 0.2f) * 10.0f)); break;            // low cut
            case 4: y = -1.2f / (1.0f + std::exp (-(x - 0.2f) * 10.0f)); break;           // high cut
            case 5: y = -1.3f * std::exp (-x * x * 60.0f); break;                          // notch
            case 6: y = std::exp (-x * x * 6.0f) * 1.1f - 0.9f; break;                     // passa-banda
            case 7: y = 0.0f; break;                                                       // all-pass
            case 8: y = x * 0.7f; break;                                                   // tilt
            default: break;
        }
        const auto q = pt (t, y);
        if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
    }
    if (type == 7) { p.addEllipse (x0 + w * 0.5f - 3, yc - 3, 6, 6); }
    juce::ignoreUnused (x1);
    return p;
}
}

// =============================================================================
class EQMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EQMLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, pal::bg0);
        setColour (Label::textColourId, pal::text);
        setColour (TextButton::buttonColourId, pal::bg3);
        setColour (TextButton::textColourOffId, pal::textDim);
        setColour (TextButton::textColourOnId, pal::bg0);
        setColour (ComboBox::backgroundColourId, pal::bg3);
        setColour (ComboBox::textColourId, pal::text);
        setColour (ComboBox::outlineColourId, pal::line);
        setColour (ComboBox::arrowColourId, pal::textDim);
        setColour (PopupMenu::backgroundColourId, pal::bg2);
        setColour (PopupMenu::textColourId, pal::text);
        setColour (PopupMenu::highlightedBackgroundColourId, pal::accent.withAlpha (0.18f));
        setColour (PopupMenu::highlightedTextColourId, pal::text);
        setColour (PopupMenu::headerTextColourId, pal::textFaint);
        setColour (TextEditor::backgroundColourId, pal::bg0);
        setColour (TextEditor::textColourId, pal::text);
        setColour (TextEditor::outlineColourId, pal::line);
        setColour (TextEditor::focusedOutlineColourId, pal::accent.withAlpha (0.7f));
        setColour (TextEditor::highlightColourId, pal::accent.withAlpha (0.3f));
        setColour (CaretComponent::caretColourId, pal::accent);
        setColour (Slider::textBoxTextColourId, pal::text);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, pal::accent.withAlpha (0.3f));
        setColour (TooltipWindow::backgroundColourId, pal::bg3);
        setColour (TooltipWindow::textColourId, pal::text);
        setColour (TooltipWindow::outlineColourId, pal::line);
        setColour (ListBox::backgroundColourId, pal::bg1);
        setColour (ScrollBar::thumbColourId, pal::bg3.brighter (0.2f));
        setColour (ToggleButton::textColourId, pal::textDim);
        setColour (ToggleButton::tickColourId, pal::accent);
        setColour (AlertWindow::backgroundColourId, pal::bg2);
        setColour (AlertWindow::textColourId, pal::text);
        setColour (AlertWindow::outlineColourId, pal::line);
        setDefaultSansSerifTypeface (fonts::face (0));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return fonts::ui (juce::jmin (13.0f, h * 0.48f), 1); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::ui (13.0f, 1); }
    juce::Font getPopupMenuFont() override { return fonts::ui (13.5f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    juce::Font getAlertWindowMessageFont() override { return fonts::ui (14.0f); }
    juce::Font getAlertWindowTitleFont() override { return fonts::ui (16.0f, 2); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float rad = b.getProperties().getWithDefault ("radius", 6.0f);
        auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) pal::accent.getARGB()));
        const bool flat = b.getProperties().getWithDefault ("flat", false);
        juce::Colour fill = flat ? juce::Colours::transparentBlack : pal::bg3;
        if (on) fill = b.getProperties().getWithDefault ("onFill", false) ? accent : accent.withAlpha (0.16f);
        if (! on && (bool) b.getProperties().getWithDefault ("primary", false) && b.isEnabled()) fill = accent.withAlpha (0.9f);
        if (over) fill = fill.brighter (0.06f).withMultipliedAlpha (flat && ! on ? 0.0f : 1.0f);
        if (over && flat && ! on) fill = pal::bg3.withAlpha (0.6f);
        if (down) fill = fill.brighter (0.1f);
        if (! b.isEnabled()) fill = fill.withMultipliedAlpha (0.5f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, rad);
        if (on && ! (bool) b.getProperties().getWithDefault ("onFill", false))
        {
            g.setColour (accent.withAlpha (0.55f));
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
        else if (! flat)
        {
            g.setColour (pal::line);
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) pal::accent.getARGB()));
        juce::Colour c = on ? ((bool) b.getProperties().getWithDefault ("onFill", false) ? pal::bg0 : accent) : pal::textDim;
        if (! on && (bool) b.getProperties().getWithDefault ("primary", false) && b.isEnabled()) c = pal::bg0;
        if (! b.isEnabled()) c = pal::textFaint;
        g.setColour (c);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float h = 16.0f, w = 28.0f;
        auto sw = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - h / 2, w, h);
        const bool on = b.getToggleState();
        auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) pal::accent.getARGB()));
        g.setColour (on ? accent.withAlpha (0.9f) : pal::bg3.brighter (over ? 0.15f : 0.0f));
        g.fillRoundedRectangle (sw, h / 2);
        g.setColour (on ? pal::bg0 : pal::textDim);
        const float k = h - 6;
        g.fillEllipse (on ? sw.getRight() - k - 3 : sw.getX() + 3, sw.getY() + 3, k, k);
        g.setColour (b.isEnabled() ? (on ? pal::text : pal::textDim) : pal::textFaint);
        g.setFont (fonts::ui (12.5f, 1));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) w + 8), juce::Justification::centredLeft, 1);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (true) ? pal::accent.withAlpha (0.6f) : pal::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path a;
        const float ax = (float) w - 14.0f, ay = h * 0.5f;
        a.startNewSubPath (ax - 4, ay - 2); a.lineTo (ax, ay + 2); a.lineTo (ax + 4, ay - 2);
        g.setColour (pal::textDim);
        g.strokePath (a, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
        l.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (pal::bg2);
        g.setColour (pal::line);
        g.drawRect (0, 0, w, h);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        auto col = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("colour", (juce::int64) pal::accent.getARGB()));
        if (! s.isEnabled()) col = pal::textFaint;
        const bool bip = s.getProperties().getWithDefault ("bipolar", false);
        const float ang = a0 + pos * (a1 - a0);
        const float th = juce::jmax (2.5f, rad * 0.13f);

        // corpo
        g.setGradientFill (juce::ColourGradient (pal::bg3.brighter (0.08f), c.x, c.y - rad, pal::bg1, c.x, c.y + rad, false));
        g.fillEllipse (c.x - rad + th + 2, c.y - rad + th + 2, (rad - th - 2) * 2, (rad - th - 2) * 2);
        g.setColour (pal::line);
        g.drawEllipse (c.x - rad + th + 2, c.y - rad + th + 2, (rad - th - 2) * 2, (rad - th - 2) * 2, 1.0f);

        juce::Path track;
        track.addCentredArc (c.x, c.y, rad - th * 0.5f, rad - th * 0.5f, 0, a0, a1, true);
        g.setColour (pal::bg3);
        g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        const float from = bip ? (a0 + a1) * 0.5f : a0;
        val.addCentredArc (c.x, c.y, rad - th * 0.5f, rad - th * 0.5f, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (col);
        g.strokePath (val, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ponteiro
        const float pr = rad - th - 6;
        juce::Point<float> tip (c.x + pr * std::sin (ang), c.y - pr * std::cos (ang));
        g.setColour (s.isEnabled() ? pal::text : pal::textFaint);
        g.drawLine ({ c.getPointOnCircumference (pr * 0.35f, ang), tip }, 2.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        // barra de valor (estilo "pill"): fundo + preenchimento + marca
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        auto col = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("colour", (juce::int64) pal::accent.getARGB()));
        g.setColour (pal::bg3);
        g.fillRoundedRectangle (r, 5.0f);
        auto fill = r.withRight (pos);
        g.setColour (col.withAlpha (s.isEnabled() ? 0.22f : 0.08f));
        g.fillRoundedRectangle (fill, 5.0f);
        g.setColour (col.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle (pos - 1.5f, r.getY() + 5, 3.0f, r.getHeight() - 10, 1.5f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (fonts::mono (12.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, pal::text);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textWhenEditingColourId, pal::text);
        l->setColour (juce::TextEditor::backgroundColourId, pal::bg0);
        return l;
    }

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) override
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (path, juce::AffineTransform::translation (0, 4));
        g.setColour (pal::bg1);
        g.fillPath (path);
        g.setColour (pal::line.brighter (0.15f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }
    int getCallOutBoxBorderSize (const juce::CallOutBox&) override { return 16; }
    float getCallOutBoxCornerSize (const juce::CallOutBox&) override { return 10.0f; }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.setColour (pal::bg3);
        g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
        g.setColour (pal::line);
        g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f, 1.0f);
        g.setColour (pal::text);
        g.setFont (fonts::ui (12.5f));
        g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 4);
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (320, (int) juce::GlyphArrangement::getStringWidth (fonts::ui (12.5f), text) + 20);
        const int lines = 1 + (int) (juce::GlyphArrangement::getStringWidth (fonts::ui (12.5f), text) / 300.0f);
        const int h = 10 + 16 * lines;
        return juce::Rectangle<int> (pos.x + 12, pos.y + 18, w, h).constrainedWithin (parent);
    }
};

// Botão de ícone
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, juce::Path icon, bool filledIcon = false)
        : juce::Button (name), path (std::move (icon)), filled (filledIcon) {}
    void setIcon (juce::Path p) { path = std::move (p); repaint(); }
    juce::Colour onColour = pal::accent;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        if (over || on || down)
        {
            g.setColour (on ? onColour.withAlpha (0.16f) : pal::bg3.withAlpha (down ? 1.0f : 0.7f));
            g.fillRoundedRectangle (r, 6.0f);
        }
        if (on) { g.setColour (onColour.withAlpha (0.5f)); g.drawRoundedRectangle (r, 6.0f, 1.0f); }
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.62f;
        auto ir = r.withSizeKeepingCentre (s, s);
        auto p = path;
        p.applyTransform (juce::AffineTransform::scale (ir.getWidth(), ir.getHeight()).translated (ir.getX(), ir.getY()));
        auto c = ! isEnabled() ? pal::textFaint : on ? onColour : over ? pal::text : pal::textDim;
        g.setColour (c);
        if (filled) g.fillPath (p);
        else g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    juce::Path path;
    bool filled;
};
