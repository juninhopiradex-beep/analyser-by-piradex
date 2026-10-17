#include "PiradexLanguage.h"
#include <juce_data_structures/juce_data_structures.h>

namespace piradex::i18n
{
namespace
{
    std::atomic<int> lang { -1 };

    juce::PropertiesFile& settings()
    {
        static std::unique_ptr<juce::PropertiesFile> props = []
        {
            juce::PropertiesFile::Options o;
            o.applicationName     = "Piradex";
            o.folderName          = "Piradex";
            o.filenameSuffix      = ".settings";
            o.osxLibrarySubFolder = "Application Support";
            o.storageFormat       = juce::PropertiesFile::storeAsXML;
            return std::make_unique<juce::PropertiesFile> (o);
        }();
        return *props;
    }

    juce::ListenerList<Listener>& listeners()
    {
        static juce::ListenerList<Listener> l;
        return l;
    }

    Language fromSettingsOrSystem()
    {
        const auto stored = settings().getValue ("language");
        if (stored == "pt") return Language::PT;
        if (stored == "en") return Language::EN;
        const auto sys = juce::SystemStats::getUserLanguage().toLowerCase();
        return sys.startsWith ("pt") ? Language::PT : Language::EN;
    }
}

Language current() noexcept
{
    int v = lang.load();
    if (v < 0)
    {
        v = (int) fromSettingsOrSystem();
        lang.store (v);
    }
    return (Language) v;
}

bool isPortuguese() noexcept { return current() == Language::PT; }
juce::String code()          { return isPortuguese() ? "pt" : "en"; }

void set (Language l, bool persist)
{
    if (persist)
    {
        settings().setValue ("language", l == Language::PT ? "pt" : "en");
        settings().saveIfNeeded();
    }
    if (lang.exchange ((int) l) != (int) l)
        listeners().call ([] (Listener& x) { x.languageChanged(); });
}

void reload()
{
    settings().reload();
    const auto l = fromSettingsOrSystem();
    if (lang.exchange ((int) l) != (int) l)
        listeners().call ([] (Listener& x) { x.languageChanged(); });
}

juce::String tr (const char* pt, const char* en)
{
    return juce::String::fromUTF8 (isPortuguese() ? pt : en);
}

juce::String upper (const juce::String& s)
{
    juce::String out;
    out.preallocateBytes (s.getNumBytesAsUTF8() + 4);
    for (auto p = s.getCharPointer(); ! p.isEmpty();)
    {
        auto c = p.getAndAdvance();
        if (c >= 'a' && c <= 'z')                         c = (juce::juce_wchar) (c - 32);
        else if (c >= 0xE0 && c <= 0xFE && c != 0xF7)     c = (juce::juce_wchar) (c - 0x20);
        else if (c > 0x7F)                                c = juce::CharacterFunctions::toUpperCase (c);
        out += juce::String::charToString (c);
    }
    return out;
}

void addListener (Listener* l)    { listeners().add (l); }
void removeListener (Listener* l) { listeners().remove (l); }
} // namespace piradex::i18n
