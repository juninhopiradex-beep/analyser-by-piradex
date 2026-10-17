// Ecrã de login do MIXSPACE: tapa a interface até a senha ser aceite.
// Enquanto está visível o plugin deixa passar o beat sem processar.
#pragma once
#include "License.h"
#include "MixspaceLookAndFeel.h"

class LoginOverlay : public juce::Component
{
public:
    LoginOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}   // não deixa cliques passar para o plugin

    std::function<void()> onUnlocked;

private:
    void submit();
    void check();
    juce::Rectangle<int> card() const;
    bool checking = false;

    juce::TextEditor password;
    juce::ToggleButton remember;
    juce::TextButton enter;
    juce::Label message;
    piradex::i18n::LanguageSwitch language;
    piradex::i18n::Binder lang;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoginOverlay)
};
