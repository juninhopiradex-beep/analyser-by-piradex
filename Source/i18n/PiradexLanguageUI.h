// =============================================================================
//  Piradex — componentes de UI do idioma (PT / EN), partilhados pelos plugins.
//
//  LanguageSwitch : pílula "PT | EN" (troca imediata). Cores configuráveis.
//  Binder         : bind ([this] { botao.setButtonText (tr (..)); }) aplica já e
//                   volta a aplicar a cada troca de idioma; onChange = repaint.
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PiradexLanguage.h"

namespace piradex::i18n
{
class Binder : private Listener
{
public:
    Binder()  { addListener (this); }
    ~Binder() override { removeListener (this); }
    void bind (std::function<void()> f) { f(); fns.push_back (std::move (f)); }
    std::function<void()> onChange;
private:
    std::vector<std::function<void()>> fns;
    void languageChanged() override { for (auto& f : fns) f(); if (onChange) onChange(); }
};

class LanguageSwitch : public juce::Component, public juce::SettableTooltipClient, private Listener
{
public:
    juce::Colour accent { 0xff40e0c0 }, background { 0xff101216 }, outline { 0xff3a3f4a },
                 text { 0xffe6e9ef }, textDim { 0xff8a919e }, hoverFill { 0xff1c2027 };
    float fontHeight = 13.0f;
    bool bold = true;

    LanguageSwitch()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (juce::String::fromUTF8 ("Idioma da interface  ·  Interface language"));
        addListener (this);
    }
    ~LanguageSwitch() override { removeListener (this); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = b.getHeight() * 0.5f;
        g.setColour (background);
        g.fillRoundedRectangle (b, rad);
        g.setColour (outline);
        g.drawRoundedRectangle (b, rad, 1.0f);
        const int sel = (int) current();
        static const char* names[] = { "PT", "EN" };
        for (int i = 0; i < 2; ++i)
        {
            auto r = half (i).reduced (2.0f);
            if (i == sel)
            {
                g.setColour (accent.withAlpha (0.18f));
                g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
                g.setColour (accent.withAlpha (0.9f));
                g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
            }
            else if (i == hover)
            {
                g.setColour (hoverFill);
                g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            }
            g.setColour (i == sel ? text : textDim);
            g.setFont (juce::Font (juce::FontOptions (juce::jmin (fontHeight, r.getHeight() * 0.6f), bold ? juce::Font::bold : juce::Font::plain)));
            g.drawText (names[i], r, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 2; ++i)
            if (half (i).contains (e.position)) set ((Language) i);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = half (0).contains (e.position) ? 0 : (half (1).contains (e.position) ? 1 : -1);
        if (h != hover) { hover = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

private:
    int hover = -1;
    juce::Rectangle<float> half (int i) const
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        return i == 0 ? r.removeFromLeft (r.getWidth() * 0.5f) : r.withTrimmedLeft (r.getWidth() * 0.5f);
    }
    void languageChanged() override { repaint(); }
};
} // namespace piradex::i18n
