// =============================================================================
//  MIXSPACE by Piradex — acesso por senha (offline)
//
//  O binário NÃO contém as senhas: só um "salt" aleatório e o hash
//  PBKDF2-HMAC-SHA256 (60 000 iterações) de cada uma.
//  Para criar novas senhas beta: tools/nova_senha_beta.py  (cola a linha
//  gerada em kKeys, em License.cpp, e recompila).
//  Chaves de licença (XXX-XXXX-XXXX-XXXX) são validadas online pelo PCC: ver pcc/PccSession.h.
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include "pcc/PccSession.h"

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
    enum class Result { Ok, Wrong, Expired, Online };   // Online: o PCC recusou ou não respondeu (texto em onlineText())

    static License& get();

    static bool inTrial()
    {
       #if defined (MXS_ALLOW_TEST_UNLOCK) || defined (ANL_ALLOW_TEST_UNLOCK) || defined (EQM_ALLOW_TEST_UNLOCK) \
        || defined (DLX_ALLOW_TEST_UNLOCK) || defined (ROOMX_ALLOW_TEST_UNLOCK) || defined (PIRADEX_NO_TRIAL)
        return false;   // nas builds de teste o trial fica desligado (para testar o bloqueio)
       #else
        return juce::Time::getCurrentTime() < juce::Time (2027, 0, 1, 0, 0, 0); // aberto ate 31/12/2026
       #endif
    }
    bool   isUnlocked() const  { return unlocked.load() || inTrial(); }
    Kind   kind() const        { return currentKind; }
    juce::String statusText() const;       // ex.: "BETA · válida até 31/12/2026"

    // Verifica uma senha escrita no ecrã de login (lento de propósito: PBKDF2).
    // matchPassword pode correr numa thread de fundo; applyMatch na thread de mensagens.
    static int matchPassword (const juce::String& password, bool& expired);   // índice ou -1
    Result applyMatch (int index, bool expired, bool remember);
    Result tryPassword (const juce::String& password, bool remember);
    // Chave de licença online (PCC): checkOnline numa thread de fundo, applyOnline na thread de mensagens.
    static piradex::pcc::Check checkOnline (const juce::String& password) { return get().pcc.check (password); }
    Result applyOnline (const piradex::pcc::Check& c, bool remember);
    juce::String onlineText() const { return lastOnline; }
    bool claimOnlineRecheck();              // barato (thread de mensagens): true = lançar revalidateOnline() agora
    void revalidateOnline();                // fundo, no máximo 1x/dia; revogada no PCC => bloqueia
    void   lock();                         // terminar sessão neste computador
    void   checkExpiry();                  // bloqueia se a senha beta expirar durante o uso

    // Funções puras (testáveis)
    static juce::MemoryBlock pbkdf2 (const juce::String& password, const juce::MemoryBlock& salt, int iterations, int dkLen);
    static juce::MemoryBlock fromHex (const char* hex);
    static bool isExpired (const KeyEntry& k, juce::Time now = juce::Time::getCurrentTime());

    static constexpr int kIterations = 60000;

   #if defined (MXS_ALLOW_TEST_UNLOCK)   // só existe nas builds de teste, nunca no plugin distribuído
    void unlockForTests();
   #endif

private:
    License();
    void tryAutoUnlock();
    void setUnlocked (const KeyEntry& k);
    void setUnlockedOnline();
    static juce::String tokenFor (const KeyEntry& k);
    std::unique_ptr<juce::PropertiesFile> props;
    std::atomic<bool> unlocked { false };
    Kind currentKind = Kind::None;
    const KeyEntry* currentKey = nullptr;
    piradex::pcc::Session pcc { "MIXSPACE", PIRADEX_PCC_VERSION };
    std::atomic<bool> online { false };
    std::atomic<juce::int64> lastRecheckTry { 0 };
    juce::String lastOnline;
};
} // namespace lic
