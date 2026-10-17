// =============================================================================
//  EQMotion — ecrã de login (mesma lógica dos outros plugins Piradex)
//  A verificação (PBKDF2, lenta de propósito) corre numa thread de fundo para
//  o DAW não congelar. Sem sessão, o plugin deixa passar o áudio sem processar.
// =============================================================================
#pragma once
#include "Theme.h"
#include "../License.h"

class LoginOverlay : public juce::Component
{
public:
    LoginOverlay()
    {
        password.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        password.setFont (fonts::ui (18.0f, 1));
        password.setJustification (juce::Justification::centred);
        password.setIndents (12, 0);
        password.onReturnKey = [this] { submit(); };
        addAndMakeVisible (password);

        remember.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (remember);

        enter.getProperties().set ("primary", true);
        enter.onClick = [this] { submit(); };
        addAndMakeVisible (enter);

        message.setJustificationType (juce::Justification::centred);
        message.setFont (fonts::ui (13.0f));
        addAndMakeVisible (message);

        language.accent = pal::accent;
        language.background = pal::bg0;
        language.outline = pal::line;
        language.text = pal::text;
        language.textDim = pal::textDim;
        language.hoverFill = pal::bg3;
        addAndMakeVisible (language);
        lang.bind ([this]
        {
            password.setTextToShowWhenEmpty (tr ("Senha", "Password"), pal::textFaint.brighter (0.2f));
            password.setTitle (tr ("Senha", "Password"));
            remember.setButtonText (tr ("Lembrar neste computador", "Remember this computer"));
            enter.setButtonText (tr ("Entrar", "Sign in"));
            message.setText ({}, juce::dontSendNotification);
            password.repaint();
        });
        lang.onChange = [this] { repaint(); };
    }

    std::function<void()> onUnlocked;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (pal::bg0.withAlpha (0.94f));
        const auto c = card().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (c.translated (0, 8).expanded (2), 16);
        g.setGradientFill (juce::ColourGradient (pal::bg2, 0, c.getY(), pal::bg1, 0, c.getBottom(), false));
        g.fillRoundedRectangle (c, 14);
        g.setColour (pal::line);
        g.drawRoundedRectangle (c.reduced (0.5f), 14, 1);

        // marca
        const float cx = c.getCentreX(), top = c.getY() + 46;
        auto m = juce::Rectangle<float> (cx - 22, top - 22, 44, 44);
        g.setColour (pal::accent.withAlpha (0.15f)); g.fillEllipse (m);
        g.setColour (pal::accent); g.drawEllipse (m.reduced (1), 2.0f);
        juce::Path w;
        for (int i = 0; i <= 32; ++i)
        {
            const float t = i / 32.0f, x = m.getX() + 9 + t * (m.getWidth() - 18);
            const float y = m.getCentreY() - std::sin (t * juce::MathConstants<float>::twoPi * 1.2f) * 8.0f * std::sin (t * juce::MathConstants<float>::pi);
            if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
        }
        g.strokePath (w, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setFont (fonts::ui (30.0f, 2, -0.01f));
        const float wEQ = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "EQ"), wM = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "Motion");
        const float x0 = cx - (wEQ + wM) * 0.5f;
        g.setColour (pal::text);   g.drawText ("EQ", juce::Rectangle<float> (x0, top + 32, wEQ + 2, 38), juce::Justification::centredLeft);
        g.setColour (pal::accent); g.drawText ("Motion", juce::Rectangle<float> (x0 + wEQ, top + 32, wM + 4, 38), juce::Justification::centredLeft);
        g.setColour (pal::textDim);
        g.setFont (fonts::ui (13.5f, 1));
        g.drawText ("by Piradex", juce::Rectangle<float> (c.getX(), top + 68, c.getWidth(), 18), juce::Justification::centred);
        g.setColour (pal::textFaint.brighter (0.3f));
        g.setFont (fonts::ui (13.0f));
        g.drawText (tr ("Introduza a sua senha para começar", "Enter your password to get started"), juce::Rectangle<float> (c.getX(), top + 96, c.getWidth(), 18), juce::Justification::centred);

        g.setColour (pal::textFaint.brighter (0.1f));
        g.setFont (fonts::ui (11.5f));
        g.drawText (tr ("Sem sessão iniciada o EQMotion deixa passar o áudio sem processar.", "While signed out, EQMotion passes the audio through unprocessed."),
                    juce::Rectangle<float> (c.getX() - 60, c.getBottom() + 16, c.getWidth() + 120, 16), juce::Justification::centred);
        g.drawText (juce::String::fromUTF8 ("www.beatfreakstudio.com  ·  juninhopiradex@hotmail.com"),
                    juce::Rectangle<float> (c.getX() - 60, c.getBottom() + 34, c.getWidth() + 120, 16), juce::Justification::centred);
    }

    void resized() override
    {
        auto c = card().reduced (60, 0);
        c.removeFromTop (180);
        password.setBounds (c.removeFromTop (44));
        c.removeFromTop (12);
        remember.setBounds (c.removeFromTop (24).withSizeKeepingCentre (230, 24));
        c.removeFromTop (14);
        enter.setBounds (c.removeFromTop (36).withSizeKeepingCentre (160, 36));
        c.removeFromTop (8);
        message.setBounds (c.removeFromTop (32));
        language.setBounds (card().removeFromBottom (42).removeFromRight (100).reduced (14, 9));
    }

    void visibilityChanged() override
    {
        if (isShowing()) juce::Timer::callAfterDelay (50, [safe = juce::Component::SafePointer<LoginOverlay> (this)] { if (safe != nullptr && safe->isShowing()) safe->password.grabKeyboardFocus(); });
    }
    void mouseDown (const juce::MouseEvent&) override {}   // não deixa cliques passar

private:
    juce::Rectangle<int> card() const { return getLocalBounds().withSizeKeepingCentre (460, 380); }

    void submit()
    {
        if (password.isEmpty() || checking) return;
        checking = true;
        enter.setEnabled (false);
        message.setColour (juce::Label::textColourId, pal::textDim);
        message.setText (tr ("A verificar…", "Checking…"), juce::dontSendNotification);
        const auto pw = password.getText();
        const bool rem = remember.getToggleState();
        juce::Component::SafePointer<LoginOverlay> safe (this);
        juce::Thread::launch ([safe, pw, rem]
        {
            bool expired = false;
            const int idx = lic::License::matchPassword (pw, expired);
            juce::MessageManager::callAsync ([safe, idx, expired, rem]
            {
                const auto r = lic::License::get().applyMatch (idx, expired, rem);
                if (safe != nullptr) safe->finished (r);
            });
        });
    }

    void finished (lic::License::Result r)
    {
        checking = false;
        enter.setEnabled (true);
        if (r == lic::License::Result::Ok)
        {
            password.clear();
            message.setText ({}, juce::dontSendNotification);
            if (onUnlocked) onUnlocked();
            return;
        }
        password.selectAll();
        message.setColour (juce::Label::textColourId, pal::accent2);
        message.setText (r == lic::License::Result::Expired ? tr ("Esta senha beta já expirou.", "This beta password has expired.") : tr ("Senha incorreta.", "Incorrect password."),
                         juce::dontSendNotification);
    }

    juce::TextEditor password;
    juce::ToggleButton remember;
    juce::TextButton enter;
    juce::Label message;
    bool checking = false;
    piradex::i18n::LanguageSwitch language;
    piradex::i18n::Binder lang;
};
