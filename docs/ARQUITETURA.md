# EQMotion by Piradex — arquitetura

Este documento cumpre o "plano de arquitetura antes da implementação" pedido na especificação: módulos, escolhas de DSP e respetivos compromissos, modelo de threads, modelo de estado e modelo de latência.

## 1. Módulos

```
Source/
├── dsp/                         núcleo DSP — C++17 puro, sem JUCE (testável isoladamente)
│   ├── Types.h                  tipos de banda, inclinações, colocações, modos
│   ├── FilterDesign.h           protótipos analógicos, bilinear com pre-warp, Chebyshev II (brickwall), magnitudes
│   ├── Biquad.h                 biquad TDF-II em 64 bits, linha de atraso, suavizadores (exponencial e rampa linear)
│   ├── Dynamics.h               detetor por banda, lei de ganho, De-harsh, banco de dinâmica
│   ├── IIREngine.h              motor Zero Latency / Natural (24 bandas em série)
│   ├── LinearPhaseEngine.h      motor de fase linear (FIR por salto + overlap-add, matriz L/R/M/S)
│   ├── FFT.h                    FFT real radix-2 em 64 bits
│   ├── Character.h              Character (ADAA), Auto gain (BS.1770), filtro de Listen
│   ├── Spectrum.h               analisador (até 16384 pontos) e grelha log de 1/48 oct
│   └── SmartOps.h               aprendizagem e heurísticas das Smart Ops
├── Params.h                     470 parâmetros automatizáveis (14 globais + 24 × 19)
├── CurveMath.h                  curvas do ecrã = mesmas funções do motor
├── PluginProcessor.*            cadeia de áudio, troca de motor/latência, análise, Smart Ops
├── StateManager.*               desfazer/refazer, A/B, etiquetas Smart Ops, copiar/colar, presets
├── Presets.*                    46 presets de fábrica (10 pastas), presets do utilizador, favoritos
├── InstanceRegistry.h           instâncias do projeto (espetro, média longa, curva)
├── License.*                    acesso por senha (igual aos outros plugins Piradex)
└── ui/                          interface (gráfico, inspetor, coluna Master, Smart Ops, janelas)
```

## 2. Decisões de DSP

| Subsistema | Decisão | Compromisso |
|---|---|---|
| Filtros estáticos | Protótipos analógicos normalizados (RBJ para sino/shelves) → bilinear com *pre-warp* em f0 → biquads TDF-II em `double` | Barato e estável; perto de Nyquist o bilinear comprime a forma ("cramping") — resolvido no modo Natural |
| Suavização | No domínio dos parâmetros (log-f, dB, log-Q), constante de 12 ms, recálculo por sub-bloco de 16–64 amostras só quando muda | Evita "zipper" e a instabilidade de interpolar coeficientes; custo desprezável |
| Mudanças de forma | Tipo, inclinação, colocação, ligar/desligar: rampa de mistura de 5 ms até zero, troca, rampa de volta | Sem cliques; durante ~10 ms a banda age parcialmente |
| Cortes | Cascatas de Butterworth (1.ª + 2.ª ordem); o Q do utilizador muda a secção menos ressonante | Num corte de 12 dB/oct o Q é exatamente o pedido |
| Brickwall | Chebyshev tipo II de 16.ª ordem, 100 dB de rejeição, −3 dB em fc (rejeição total a ~1,3·fc) | Fase mínima em ZL/NP; em LP é usado só o módulo |
| Dinâmica | detetor filtrado na zona da banda (passa-banda 0 dB / passa-baixo / passa-alto) → envolvente Peak ou RMS com ataque/hold/release → lei com joelho e ratio → desvio limitado pelo range (com sinal) somado ao ganho estático | A parte dinâmica é independente da curva estática, por isso é desenhada à parte (área tracejada + ponto ao vivo) |
| Estéreo | L/R ↔ M/S exato por amostra, por banda; detetores estéreo ligados (mesmo ganho nos dois canais) | Sem "imagem a passear" |
| Natural Phase | Motor IIR a ≥176,4 kHz (4× a 44,1/48 kHz, 2× a 88,2/96 kHz) com filtros de meia-banda IIR polifásicos | Magnitude = analógica até 20 kHz (erro ≤ 0,69 dB em sinos e shelves com Q ≤ 2, ≤ 1,9 dB em shelves muito ressonantes), fase mínima como os EQs analógicos, 4 amostras de latência a 44,1 kHz. Testado contra o protótipo analógico em `tests/test_dsp.cpp` |
| Linear Phase | Em cada salto R a magnitude analógica (com os ganhos dinâmicos do momento) vira um FIR simétrico de K coeficientes (janela de Blackman) aplicado por overlap-add FFT; L/R/M/S tratados como matriz real 2×2 por bin (continua fase linear, 2 FFT + 2 IFFT por salto) | Latência R + (K−1)/2; a dinâmica atualiza por salto (11–43 ms); bandas L/R são aplicadas antes das M/S |
| Character | `y = x + h(x)`: só a pequena parte harmónica h é gerada (tanh assimétrico + termo par), com anti-aliasing por antiderivada (ADAA) e bloqueio de DC só em h | Sem latência; Clean é bit a bit transparente; THD a −6 dBFS ≈ −51 dB (Subtle) e −39 dB (Warm) |
| De-harsh | Razão de energia 2,5–8 kHz / banda larga comparada com a sua média de 2,5 s; quando sobe acima disso, um sino largo a 4,2 kHz baixa até 10 dB × Amount | Limiar contextual, não fixo; atua só quando "morde" |
| Auto gain | Loudness ponderada K (BS.1770) da entrada alinhada vs saída, média de 1,5 s, limite ±18 dB, congela em silêncio | Ajuda de comparação, não maximizador |
| Analisador | Blackman-Harris 4 termos, FFT 2048–16384, salto ≤1024, balística (subida/queda) por velocidade, compensação de inclinação 0/3/4,5/6 dB/oct, pico | Corre numa thread de trabalho |

## 3. Modelo de threads

* **Áudio** — só DSP. Toda a memória é alocada em `prepareToPlay`; nenhuma alocação, lock, ficheiro ou UI. Lê os parâmetros por ponteiros atómicos; escreve as amostras para a análise num `AbstractFifo` (se a análise atrasar, perde-se um bloco de análise, nunca áudio).
* **Análise** (`EQMotion analysis`, prioridade baixa) — lê o FIFO, calcula espetros, alimenta a aprendizagem das Smart Ops e publica para a UI (spin-lock) e para as outras instâncias. Só trabalha quando há algum editor aberto ou uma aprendizagem a decorrer.
* **Mensagens** — latência e troca de motor, desfazer/A-B/presets, verificação de expiração da licença, cálculo das sugestões (Amount e intervalo recalculam em tempo real), curvas.
* **Login** — a verificação PBKDF2 corre numa thread temporária; o resultado volta pela fila de mensagens.

## 4. Modelo de estado

* 470 parâmetros do host (todos automatizáveis, IDs com versão 1).
* `EQM_STATE`: slot ativo, os dois slots A/B (fotografias completas), etiquetas Smart Ops por banda, nome do preset.
* `EQM_UI`: preferências do analisador, escala do gráfico, zoom, tamanho da janela, instâncias sobrepostas, banda selecionada.
* `EQM_SMART`: operação, intervalo, Amount, última operação aplicada (para o Reverter funcionar depois de reabrir o projeto).
* **Desfazer**: fotografias de todos os parâmetros. Cada operação explícita (aplicar preset, Smart Ops, limpar, colar…) é exatamente um passo; os gestos na UI fecham um passo quando o rato é largado. Mudanças vindas do DAW (automação) não entram no histórico. Cada slot A/B tem o seu histórico.

## 5. Modelo de latência

| Modo | Latência reportada | Notas |
|---|---|---|
| Zero Latency | 0 | lookahead desativado |
| Natural Phase | latência dos filtros de sobreamostragem (4 amostras a 44,1 kHz) + lookahead (0–20 ms) | o caminho seco passa pelos mesmos filtros → Delta e bypass exatos |
| Linear Phase | Baixa 1535 · Média 3071 · Alta 6143 amostras | pico do impulso exatamente na latência (testado) |

A troca de modo, qualidade ou lookahead é feita na thread de mensagens: reporta-se primeiro a nova latência ao DAW e só depois o motor troca, com desvanecimento de saída/entrada (~20 ms) para não haver cliques. Bypass, Delta, Listen e o estado "sem sessão" usam sempre o sinal original alinhado com a mesma latência.

## 6. Plano por etapas (cumprido)

| Etapa | Conteúdo | Evidência |
|---|---|---|
| M1 | EQ estático + gráfico | `test_dsp`: desenho bilinear, inclinações, brickwall; `test_plugin`: precisão ZL |
| M2 | Dinâmica | lei de ganho, banda dinâmica real (IIR e LP), sidechain |
| M3 | Analisador | capturas de ecrã; FFT até 16384 |
| M4 | M/S e modos de fase | Natural vs analógico, LP simétrico, M/S isolado, latências |
| M5 | Smart Ops | `test_smartops` com sinais sintéticos + ponta a ponta no plugin |
| M6 | Presets e instâncias | 46 presets processados; copiar/colar entre instâncias |
| M7 | QA, otimização, instaladores | robustez (6 taxas × 3 modos × blocos 1–3000), CPU, auval, pluginval, .dmg/.exe |
