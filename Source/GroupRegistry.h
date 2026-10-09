// Registo partilhado entre instâncias do plugin no mesmo processo.
// Um Gerador publica as definições do sinal no seu grupo; o Analisador do mesmo
// grupo adota-as automaticamente (não é preciso configurar duas vezes).
// Se o host isolar plugins em processos separados, o Analisador usa as suas
// próprias definições — basta escolher o mesmo Sinal/FFT/Nível nos dois.
#pragma once

#include <atomic>
#include <cstdint>

namespace pca
{
struct GroupSlot
{
    std::atomic<uint32_t> lastSeenMs { 0 };
    std::atomic<int>      signal     { 1 };
    std::atomic<int>      fftIndex   { 1 };
    std::atomic<float>    levelDb    { -18.0f };
    std::atomic<float>    sineHz     { 1000.0f };
    std::atomic<double>   sampleRate { 0.0 };

    // Varrimentos automáticos: o Analisador pede, o Gerador executa
    std::atomic<int>      sweepRequest { 0 };   // 1 nível, 2 frequência, -1 parar (consumido pelo Gerador)
    std::atomic<int>      sweepKind    { 0 };   // varrimento em curso (0 = nenhum)
    std::atomic<int>      sweepStep    { -1 };  // passo atual
    std::atomic<int>      sweepRunId   { 0 };   // muda a cada varrimento novo
};

struct GroupRegistry
{
    static constexpr int kNumGroups = 8;

    static GroupSlot& get (int group)
    {
        static GroupSlot slots[kNumGroups + 1];
        if (group < 1 || group > kNumGroups) group = 1;
        return slots[group];
    }
};
} // namespace pca
