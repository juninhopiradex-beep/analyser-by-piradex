#include "License.h"
#include "i18n/PiradexLanguage.h"
#include <iterator>
#include <cstring>

namespace lic
{
// ---------------------------------------------------------------------------
//  Chaves aceites (só salt + hash; as senhas nunca aparecem no código)
// ---------------------------------------------------------------------------
static const KeyEntry kKeys[] =
{
    { "master", Kind::Master,
      "283abc210a5887cd63f1f8f4227c394f",
      "086af2479ad81e006c788f87a64881befecb208076dd0d6ecdb049e111831dbe", 0, 0, 0 },

    { "beta-1", Kind::Trial,
      "4be5dd53270dacfb8e40522cd87fcd1b",
      "b34bbab9025bc83fba849d76806c8f07bf8e4d357562950b42c23a8b16c5f816", 2026, 12, 31 },

    { "beta-2", Kind::Trial,
      "ef7e781d0bcf1c3370167ad9c638fc5b",
      "93ee1df21e1d40d4084fb94c71cd64d7d8217aeb87609b3e6cb40e3e903b8cd1", 2026, 12, 31 },

    { "beta-3", Kind::Trial,
      "7b2e312707a85495582dc84c9235ecd9",
      "13c10474f12947e4a602de84feb388ec77dc334e1ca974f16aee9cb07ae6b58f", 2026, 12, 31 },

    { "beta-4", Kind::Trial,
      "249ce027b4f30171e516bb62a2c0adda",
      "c66c3680aed7e549ecdc4d44ad9fd54c92876134a5cdf319635575e3e79a63ac", 2026, 12, 31 },

    { "beta-5", Kind::Trial,
      "24ea1fbf31b3009431923420ac70bdd6",
      "9885b1b2dfa6ffdc06402524f0d8e9315099c0728f9f56c17c3d9dade49dc56a", 2026, 12, 31 },

    { "promo-loja", Kind::Trial,
      "ce6d515ca53e53c7833a604fd0e51aa8",
      "be7be21837b7c318edb91276c6e0b080292045e0b3f0303b88b70a1733467c9f", 2026, 11, 1 },
};

// ---------------------------------------------------------------------------
static juce::MemoryBlock sha256 (const void* data, size_t size)
{
    return juce::SHA256 (data, size).getRawData();
}

static juce::MemoryBlock hmacSha256 (const juce::MemoryBlock& key, const juce::MemoryBlock& msg)
{
    constexpr size_t B = 64;
    juce::MemoryBlock k = key.getSize() > B ? sha256 (key.getData(), key.getSize()) : key;
    k.ensureSize (B, true);
    uint8_t ipad[B], opad[B];
    for (size_t i = 0; i < B; ++i)
    {
        ipad[i] = (uint8_t) (static_cast<const uint8_t*> (k.getData())[i] ^ 0x36);
        opad[i] = (uint8_t) (static_cast<const uint8_t*> (k.getData())[i] ^ 0x5c);
    }
    juce::MemoryBlock inner (ipad, B);
    inner.append (msg.getData(), msg.getSize());
    const auto ih = sha256 (inner.getData(), inner.getSize());
    juce::MemoryBlock outer (opad, B);
    outer.append (ih.getData(), ih.getSize());
    return sha256 (outer.getData(), outer.getSize());
}

juce::MemoryBlock License::pbkdf2 (const juce::String& password, const juce::MemoryBlock& salt, int iterations, int dkLen)
{
    const auto pw = password.toRawUTF8();
    const juce::MemoryBlock key (pw, strlen (pw));
    juce::MemoryBlock out;
    for (uint32_t block = 1; (int) out.getSize() < dkLen; ++block)
    {
        juce::MemoryBlock s = salt;
        const uint8_t be[4] { (uint8_t) (block >> 24), (uint8_t) (block >> 16), (uint8_t) (block >> 8), (uint8_t) block };
        s.append (be, 4);
        auto u = hmacSha256 (key, s);
        juce::MemoryBlock t = u;
        for (int i = 1; i < iterations; ++i)
        {
            u = hmacSha256 (key, u);
            auto* tp = static_cast<uint8_t*> (t.getData());
            auto* up = static_cast<const uint8_t*> (u.getData());
            for (size_t j = 0; j < t.getSize(); ++j) tp[j] ^= up[j];
        }
        out.append (t.getData(), t.getSize());
    }
    out.setSize ((size_t) dkLen);
    return out;
}

juce::MemoryBlock License::fromHex (const char* hex)
{
    juce::MemoryBlock mb;
    mb.loadFromHexString (hex);
    return mb;
}

bool License::isExpired (const KeyEntry& k, juce::Time now)
{
    if (k.kind != Kind::Trial) return false;
    const juce::Time end (k.expYear, k.expMonth - 1, k.expDay, 23, 59, 59, 0, true);
    return now > end;
}

static bool constantTimeEquals (const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    if (a.getSize() != b.getSize()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.getSize(); ++i)
        diff |= (uint8_t) (static_cast<const uint8_t*> (a.getData())[i] ^ static_cast<const uint8_t*> (b.getData())[i]);
    return diff == 0;
}

// ---------------------------------------------------------------------------
License& License::get()
{
    static License instance;
    return instance;
}

License::License()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "EQMotion by Piradex";
    o.folderName          = "Piradex";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (o);
    tryAutoUnlock();
}

juce::String License::tokenFor (const KeyEntry& k)
{
    const juce::String s = juce::SystemStats::getUniqueDeviceID() + "|" + k.id + "|" + k.hashHex + "|EQMOTION";
    return juce::SHA256 (s.toUTF8()).toHexString();
}

void License::setUnlocked (const KeyEntry& k)
{
    currentKey  = &k;
    currentKind = k.kind;
    unlocked.store (true);
}

void License::tryAutoUnlock()
{
    const auto id = props->getValue ("key");
    const auto token = props->getValue ("token");
    if (id.isEmpty() || token.isEmpty()) return;
    for (auto& k : kKeys)
        if (id == k.id && token == tokenFor (k) && ! isExpired (k))
        {
            setUnlocked (k);
            return;
        }
}

int License::matchPassword (const juce::String& password, bool& expired)
{
    const auto pw = password.trim();
    expired = false;
    for (int i = 0; i < (int) std::size (kKeys); ++i)
    {
        const auto& k = kKeys[i];
        const auto expected = fromHex (k.hashHex);
        const auto got = pbkdf2 (pw, fromHex (k.saltHex), kIterations, (int) expected.getSize());
        if (! constantTimeEquals (got, expected)) continue;
        if (isExpired (k)) { expired = true; continue; }
        return i;
    }
    return -1;
}

License::Result License::applyMatch (int index, bool expired, bool remember)
{
    if (index < 0 || index >= (int) std::size (kKeys))
        return expired ? Result::Expired : Result::Wrong;

    const auto& k = kKeys[index];
    setUnlocked (k);
    if (remember)
    {
        props->setValue ("key", juce::String (k.id));
        props->setValue ("token", tokenFor (k));
    }
    else
    {
        props->removeValue ("key");
        props->removeValue ("token");
    }
    props->saveIfNeeded();
    return Result::Ok;
}

License::Result License::tryPassword (const juce::String& password, bool remember)
{
    bool expired = false;
    const int idx = matchPassword (password, expired);
    return applyMatch (idx, expired, remember);
}

void License::lock()
{
    unlocked.store (false);
    currentKind = Kind::None;
    currentKey = nullptr;
    props->removeValue ("key");
    props->removeValue ("token");
    props->saveIfNeeded();
}

#if defined (EQM_ALLOW_TEST_UNLOCK)
void License::unlockForTests() { setUnlocked (kKeys[0]); }
#endif

void License::checkExpiry()
{
    if (unlocked.load() && currentKey != nullptr && isExpired (*currentKey))
        lock();
}

juce::String License::statusText() const
{
    if (! unlocked.load() || currentKey == nullptr) return {};
    if (currentKind == Kind::Master) return piradex::i18n::tr ("Licença completa", "Full license");
    return piradex::i18n::tr ("BETA · válida até ", "BETA · valid until ")
         + juce::String (currentKey->expDay).paddedLeft ('0', 2) + "/"
         + juce::String (currentKey->expMonth).paddedLeft ('0', 2) + "/" + juce::String (currentKey->expYear);
}
} // namespace lic
