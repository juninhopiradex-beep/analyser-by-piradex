// Ecra de login do EQMotion (1.0.2: ecra unico Piradex do kit partilhado).
// Enquanto esta visivel o plugin deixa passar o audio sem processar.
#pragma once
#include "../License.h"
#include "../i18n/PiradexKitUI.h"

class LoginOverlay : public juce::Component
{
public:
    LoginOverlay()
    {
        screen.onSubmit = [this] (const juce::String& pw, bool rem) { submit (pw, rem); };
        addAndMakeVisible (screen);
    }
    void resized() override { screen.setBounds (getLocalBounds()); }
    void visibilityChanged() override { if (isVisible() && isShowing()) screen.focusField(); }
    void mouseDown (const juce::MouseEvent&) override {}   // nao deixa cliques passar para o plugin

    std::function<void()> onUnlocked;

private:
    piradex::kit::LoginScreen screen { "EQMotion", juce::Colour (0xffffb547), JucePlugin_VersionString };
    bool checking = false;

    void submit (const juce::String& pw, bool rem)
    {
        if (checking) return;
        checking = true;
        screen.setBusy (true);
        juce::Component::SafePointer<LoginOverlay> safe (this);
        // PBKDF2 (lento de proposito) e PCC (rede) numa thread de fundo: a interface nao congela
        juce::Thread::launch ([safe, pw, rem]
        {
            bool expired = false;
            const int idx = lic::License::matchPassword (pw, expired);
            piradex::pcc::Check online;                                   // chave de licenca: pergunta ao PCC
            if (idx < 0 && ! expired) online = lic::License::checkOnline (pw);
            juce::MessageManager::callAsync ([safe, idx, expired, rem, online]
            {
                const auto r = online.outcome != piradex::pcc::Outcome::Disabled ? lic::License::get().applyOnline (online, rem)
                                                                                 : lic::License::get().applyMatch (idx, expired, rem);
                auto* self = safe.getComponent();
                if (self == nullptr) return;
                self->checking = false;
                self->screen.setBusy (false);
                if (r == lic::License::Result::Ok)
                {
                    self->screen.clearField(); self->screen.setStatus ({}, false);
                    if (self->onUnlocked) self->onUnlocked();
                    return;
                }
                self->screen.setStatus (r == lic::License::Result::Online  ? lic::License::get().onlineText()
                                      : r == lic::License::Result::Expired ? piradex::i18n::tr ("Esta senha beta j\xc3\xa1 expirou.", "This beta password has expired.")
                                                                           : piradex::i18n::tr ("Senha ou licen\xc3\xa7" "a incorreta.", "Incorrect password or licence."), true);
            });
        });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoginOverlay)
};
