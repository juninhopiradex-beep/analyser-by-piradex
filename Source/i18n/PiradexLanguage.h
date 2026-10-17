// =============================================================================
//  Piradex — idioma da interface (PT / EN), módulo partilhado por todos os
//  plugins Piradex.
//
//  - Os dois textos ficam lado a lado no código:  tr ("Guardar", "Save")
//  - A escolha fica num ficheiro comum a todos os plugins Piradex
//    (macOS: ~/Library/Application Support/Piradex/Piradex.settings,
//     Windows: %APPDATA%\Piradex\Piradex.settings), chave "language".
//  - Primeira utilização: segue o idioma do sistema (pt* → PT, resto → EN).
//  - Mudar o idioma avisa todas as janelas abertas (troca imediata).
//  - Os nomes dos parâmetros vistos pelo DAW (automação) NÃO mudam.
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>

namespace piradex::i18n
{
enum class Language { PT = 0, EN = 1 };

Language current() noexcept;
void     set (Language, bool persist = true);   // chamar na thread da mensagem
void     reload();                               // reler o ficheiro (outro plugin pode ter mudado)
bool     isPortuguese() noexcept;
juce::String code();                             // "pt" / "en"

// Texto conforme o idioma actual (ambos em UTF-8)
juce::String tr (const char* pt, const char* en);

struct Listener
{
    virtual ~Listener() = default;
    virtual void languageChanged() = 0;
};
void addListener (Listener*);
void removeListener (Listener*);
} // namespace piradex::i18n
