#include "LoginOverlay.h"

LoginOverlay::LoginOverlay()
{
    password.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    password.setFont (uiFont (18.0f, true, 0.12f));
    password.setJustification (juce::Justification::centred);
    password.setColour (juce::TextEditor::backgroundColourId, pal::display);
    password.setColour (juce::TextEditor::textColourId, pal::text);
    password.setColour (juce::TextEditor::highlightColourId, pal::red.withAlpha (0.3f));
    password.setColour (juce::CaretComponent::caretColourId, pal::red);
    password.setColour (juce::TextEditor::outlineColourId, pal::plateEdge.brighter (0.2f));
    password.setColour (juce::TextEditor::focusedOutlineColourId, pal::red.withAlpha (0.8f));
    password.setIndents (12, 0);
    password.onReturnKey = [this] { submit(); };
    addAndMakeVisible (password);

    remember.setToggleState (true, juce::dontSendNotification);
    remember.setColour (juce::ToggleButton::textColourId, pal::textDim);
    remember.setColour (juce::ToggleButton::tickColourId, pal::red);
    remember.setColour (juce::ToggleButton::tickDisabledColourId, pal::textFaint);
    addAndMakeVisible (remember);

    enter.setToggleState (true, juce::dontSendNotification);   // LED aceso
    enter.getProperties().set ("accent", (juce::int64) pal::red.getARGB());
    enter.onClick = [this] { submit(); };
    addAndMakeVisible (enter);

    message.setJustificationType (juce::Justification::centred);
    message.setFont (uiFont (13.0f));
    addAndMakeVisible (message);

    language.accent = pal::red;
    language.background = pal::display;
    language.outline = pal::plateEdge.brighter (0.2f);
    language.text = pal::text;
    language.textDim = pal::textDim;
    language.hoverFill = juce::Colour (0xff22272d);
    addAndMakeVisible (language);
    lang.bind ([this]
    {
        password.setTextToShowWhenEmpty (tr ("Senha", "Password"), pal::textFaint.brighter (0.2f));
        remember.setButtonText (tr ("Lembrar neste computador", "Remember this computer"));
        enter.setButtonText (tr ("Entrar", "Sign in"));
        message.setText ({}, juce::dontSendNotification);
        password.repaint();
    });
    lang.onChange = [this] { repaint(); };
}

juce::Rectangle<int> LoginOverlay::card() const
{
    return getLocalBounds().withSizeKeepingCentre (440, 360);
}

void LoginOverlay::paint (juce::Graphics& g)
{
    g.fillAll (pal::bgBottom.withAlpha (0.94f));

    const auto c = card().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (c.translated (0.0f, 6.0f).expanded (2.0f), 14.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c2025), 0.0f, c.getY(), pal::plate, 0.0f, c.getBottom(), false));
    g.fillRoundedRectangle (c, 12.0f);
    g.setColour (pal::plateEdge.brighter (0.1f));
    g.drawRoundedRectangle (c.reduced (0.5f), 12.0f, 1.0f);

    // marca MIXSPACE (barras main · espaço · sidechain)
    const float cx = c.getCentreX(), top = c.getY() + 40.0f;
    g.setColour (pal::main.withAlpha (0.85f));
    g.fillRoundedRectangle (cx - 13.0f, top - 13.0f, 4.0f, 26.0f, 2.0f);
    g.setColour (pal::red);
    g.fillRoundedRectangle (cx - 2.0f, top - 7.0f, 4.0f, 14.0f, 2.0f);
    g.setColour (pal::sc);
    g.fillRoundedRectangle (cx + 9.0f, top - 13.0f, 4.0f, 26.0f, 2.0f);

    g.setColour (pal::text);
    g.setFont (uiFont (30.0f, true, 0.32f));
    g.drawText ("MIXSPACE", juce::Rectangle<float> (c.getX(), top + 22.0f, c.getWidth(), 36.0f), juce::Justification::centred);
    g.setColour (pal::textDim);
    g.setFont (uiFont (14.0f, false, 0.06f));
    g.drawText ("by Piradex", juce::Rectangle<float> (c.getX(), top + 56.0f, c.getWidth(), 18.0f), juce::Justification::centred);
    g.setColour (pal::textFaint.brighter (0.35f));
    g.setFont (uiFont (13.0f));
    g.drawText (tr ("Introduza a sua senha para começar", "Enter your password to get started"),
                juce::Rectangle<float> (c.getX(), top + 84.0f, c.getWidth(), 18.0f), juce::Justification::centred);

    g.setColour (pal::textFaint.brighter (0.15f));
    g.setFont (uiFont (11.0f));
    g.drawText (tr ("Sem sessão iniciada o MIXSPACE deixa passar o áudio sem processar.", "While signed out, MIXSPACE passes the audio through unprocessed."),
                juce::Rectangle<float> (c.getX(), c.getBottom() + 14.0f, c.getWidth(), 16.0f), juce::Justification::centred);
    g.drawText ("www.beatfreakstudio.com  " + juce::String::fromUTF8 ("·") + "  juninhopiradex@hotmail.com",
                juce::Rectangle<float> (c.getX(), c.getBottom() + 32.0f, c.getWidth(), 16.0f), juce::Justification::centred);
}

void LoginOverlay::resized()
{
    auto c = card().reduced (56, 0);
    c.removeFromTop (164);
    password.setBounds (c.removeFromTop (42));
    c.removeFromTop (12);
    remember.setBounds (c.removeFromTop (24).withSizeKeepingCentre (220, 24));
    c.removeFromTop (14);
    enter.setBounds (c.removeFromTop (34).withSizeKeepingCentre (150, 34));
    c.removeFromTop (8);
    message.setBounds (c.removeFromTop (34));
    language.setBounds (card().removeFromBottom (40).removeFromRight (98).reduced (14, 8));
}

void LoginOverlay::visibilityChanged()
{
    if (isVisible() && isShowing())
        password.grabKeyboardFocus();
}

void LoginOverlay::submit()
{
    if (password.isEmpty() || checking) return;
    checking = true;
    message.setColour (juce::Label::textColourId, pal::textDim);
    message.setText (tr ("A verificar…", "Checking…"), juce::dontSendNotification);

    // deixa a mensagem aparecer antes da verificação (PBKDF2, lenta de propósito)
    juce::Component::SafePointer<LoginOverlay> safe (this);
    juce::Timer::callAfterDelay (40, [safe]
    {
        if (auto* self = safe.getComponent())
            self->check();
    });
}

void LoginOverlay::check()
{
    juce::MouseCursor::showWaitCursor();
    const auto r = lic::License::get().tryPassword (password.getText(), remember.getToggleState());
    juce::MouseCursor::hideWaitCursor();
    checking = false;

    if (r == lic::License::Result::Ok)
    {
        password.clear();
        message.setText ({}, juce::dontSendNotification);
        if (onUnlocked) onUnlocked();
        return;
    }
    password.selectAll();
    message.setColour (juce::Label::textColourId, pal::bypass);
    message.setText (r == lic::License::Result::Expired ? tr ("Esta senha beta já expirou.", "This beta password has expired.")
                                                        : tr ("Senha incorreta.", "Incorrect password."),
                     juce::dontSendNotification);
}
