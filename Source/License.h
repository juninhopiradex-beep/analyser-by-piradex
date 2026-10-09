// =============================================================================
//  ANALYSER by Piradex — acesso por senha (offline)
//
//  O binário NÃO contém as senhas: só um "salt" aleatório e o hash
//  PBKDF2-HMAC-SHA256 (60 000 iterações) de cada uma.
//  Para criar novas senhas beta: tools/nova_senha_beta.py  (cola a linha
//  gerada em kKeys, em License.cpp, e recompila).
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>

namespace lic
{
enum class Kind { None, Master, Trial };

struct KeyEntry
{
    const char* id;
    Kind        kind;
    const char* saltHex;
    const char* hashHex;
    int expYear, expMonth, expDay;   // só para Trial (válida até ao fim deste dia)
};

class License
{
public:
    enum class Result { Ok, Wrong, Expired };

    static License& get();

    bool   isUnlocked() const  { return unlocked.load(); }
    Kind   kind() const        { return currentKind; }
    juce::String statusText() const;       // ex.: "BETA · válida até 31/12/2026"

    // Verifica uma senha escrita no ecrã de login (lento de propósito: PBKDF2).
    // matchPassword pode correr numa thread de fundo; applyMatch na thread de mensagens.
    static int matchPassword (const juce::String& password, bool& expired);   // índice ou -1
    Result applyMatch (int index, bool expired, bool remember);
    Result tryPassword (const juce::String& password, bool remember);
    void   lock();                         // terminar sessão neste computador
    void   checkExpiry();                  // bloqueia se a senha beta expirar durante o uso

    // Funções puras (testáveis)
    static juce::MemoryBlock pbkdf2 (const juce::String& password, const juce::MemoryBlock& salt, int iterations, int dkLen);
    static juce::MemoryBlock fromHex (const char* hex);
    static bool isExpired (const KeyEntry& k, juce::Time now = juce::Time::getCurrentTime());

    static constexpr int kIterations = 60000;

   #if defined (ANL_ALLOW_TEST_UNLOCK)   // só existe nas builds de teste, nunca no plugin distribuído
    void unlockForTests();
   #endif

private:
    License();
    void tryAutoUnlock();
    void setUnlocked (const KeyEntry& k);
    static juce::String tokenFor (const KeyEntry& k);
    std::unique_ptr<juce::PropertiesFile> props;
    std::atomic<bool> unlocked { false };
    Kind currentKind = Kind::None;
    const KeyEntry* currentKey = nullptr;
};
} // namespace lic
