# EQMotion by Piradex — validação

Tudo o que está abaixo corre automaticamente em cada push (GitHub Actions) e pode ser repetido localmente (ver README).

| Bateria | Ficheiro | Verificações |
|---|---|---|
| Núcleo DSP (sem JUCE) | `tests/test_dsp.cpp` | 82 |
| Smart Ops com sinais sintéticos | `tests/test_smartops.cpp` | 29 |
| Plugin real (processador + editor) | `tests/test_plugin.cpp` | 46 |
| Audio Unit | `auval` em Apple Silicon e em Intel (Rosetta) | — |
| VST3 | pluginval nível 5 (macOS e Windows) | — |
| Binários | arquiteturas `x86_64 + arm64`, macOS mínimo 10.13, VST3 Windows sem dependência do VC++ Redistributable | — |

## Critérios de aceitação

| # | Critério | Evidência |
|---|---|---|
| 1 | 24 bandas em simultâneo, sem instabilidade nem "zipper" | 24 bandas (todas as formas, 6–96 dB/oct, brickwall, colocações, dinâmica) em 44,1 / 48 / 88,2 / 96 / 176,4 / 192 kHz × ZL/NP/LP, blocos aleatórios de 1–3000 amostras e automação brusca de frequência e tipo: sem NaN/Inf. Suavização no domínio dos parâmetros + rampas de forma. |
| 2 | Todas as bandas estáticas; dinâmica completa com sidechain interno/externo | Lei de ganho (limiar, ratio, joelho, range, acima/abaixo) verificada; seno a −10 dBFS → −8,00 dB exatos (IIR e LP); sidechain silencioso → 0 dB, ativo → −10 dB (range). |
| 3 | Smart Ops pré-visualiza antes de aplicar; Reverter nunca apaga edições manuais | Ponta a ponta no plugin: aprender 5 s → ressonância de 1850 Hz encontrada → nada muda antes de Aplicar → Aplicar cria as bandas → edição manual posterior → Reverter remove só as da operação; edição anterior intacta; Reverter é um passo de desfazer. |
| 4 | Delta alinhado em latência; Listen/Bypass por banda previsíveis | Natural: Delta sem bandas = 1,6·10⁻¹⁴ (nulo). LP: Delta a 300 Hz com sino a 2 kHz quase nulo. Listen numa banda a 4 kHz: 100 Hz cai 44 dB. |
| 5 | A/B com auto gain permite comparação à mesma loudness | Sino +9 dB largo com Auto gain: diferença de energia de 0,41 dB. A/B guardado e restaurado com o projeto. |
| 6 | Estéreo, L/R e M/S com colocação correta por banda | Corte no Side deixa um sinal mono intacto (erro 1,5·10⁻⁸); em LP, banda no Side não toca no Mid (erro < 10⁻⁶). |
| 7 | ZL, Natural e Linear reportam a latência correta | ZL 0; NP 4 amostras a 44,1 kHz (Delta nulo prova o alinhamento); LP Média 3071 com o pico do impulso exatamente na amostra 3071 e resposta simétrica (10⁻¹⁶). |
| 8 | Analisador preciso, suave e leve; congelar e sobreposição de instâncias | FFT até 16384 numa thread própria; congelar; registo de instâncias com espetro, média longa e curva; capturas `docs/ui_*.png`. |
| 9 | Clean transparente; Subtle/Warm controlados e sem aliasing evitável | Clean: nulo exato (bit a bit). Subtle THD −51 dB, Warm −39 dB a −6 dBFS; ADAA nas harmónicas. |
| 10 | Sobrevive a stress, guardar/restaurar e mudanças de taxa/bloco | Estado com A/B, etiquetas e preset restaurado noutra instância; `prepareToPlay` com outra taxa/bloco; 46 presets de fábrica processados sem valores inválidos; processamento em 64 bits igual ao de 32 bits. |

## Precisão dos filtros

* Bilinear (ZL): sino exato no centro (±0,01 dB), shelves nos extremos (±0,05 dB), inclinações dos cortes 6–96 dB/oct medidas (±0,6 dB/oct), −3,01 dB na frequência de corte, Q do corte de 12 dB/oct exatamente o pedido.
* Brickwall: −3 dB em fc, ≥ 85 dB de rejeição a 1,4·fc, banda passante plana.
* Natural (4×): erro face ao protótipo analógico até 20 kHz ≤ 0,69 dB (sinos; shelves com Q ≤ 2), ≤ 1,93 dB no pior caso (shelf com Q 6 acima de Nyquist). Sino a 18 kHz no plugin real: 0,36 dB.
* Linear: erro de magnitude 0,012 dB (Baixa), 0,003 dB (Média), 0,001 dB (Alta).

## Smart Ops (sinais sintéticos)

| Operação | Sinal | Resultado |
|---|---|---|
| Deresonate | ruído + ressonâncias a 630 Hz e 2,5 kHz | as duas encontradas, só 2 bandas; ruído puro → nenhuma; < 2 s → não sugere; o limiar aprendido atua (até −9 dB) |
| Auto cortes | conteúdo 150 Hz–6 kHz | low cut 75 Hz e high cut 12 kHz; com sub real a 40 Hz → não corta |
| Igualar referência | referência = +6 dB @ 2 kHz e −4 dB shelf @ 100 Hz | 5 bandas, desvio máximo 2,6 dB face à diferença real; Amount 50 % escala o ganho |
| Unmask | voz 1,5–3,5 kHz no sidechain | 2 bandas dinâmicas pelo sidechain dentro da zona da voz; sem sidechain → recusa e explica |
| De-ess | rajadas sibilantes a 6,5 kHz | 1 banda a 6,8 kHz; −5,4 dB nas rajadas e 0 dB fora delas |
| Equilíbrio tonal | ruído branco | baixa os agudos; todas as correções dentro de ±4 dB |

## CPU (1 s de áudio estéreo a 48 kHz, Linux, 1 núcleo)

| Modo | 8 bandas | 24 bandas dinâmicas |
|---|---|---|
| Zero Latency | 11,8 ms (1,2 %) | 33 ms (3,3 %) |
| Natural | 21,9 ms (2,2 %) | 78 ms (7,8 %) |
| Linear (Média) | 57 ms (5,7 %) | 151 ms (15 %) |

## Checklist manual (DAW)

- [ ] Logic Pro (AU): inserir, sessão iniciada, bandas, automação, guardar e reabrir o projeto.
- [ ] Ableton Live / Cubase / Studio One / Reaper (VST3): idem; sidechain ligado a uma voz para Unmask.
- [ ] Mudar ZL → NP → LP com a reprodução a correr: sem cliques; o DAW compensa a latência.
- [ ] Bypass do DAW e ⏻ do plugin: sem saltos de fase/tempo.
- [ ] Duas instâncias: sobrepor curvas, copiar/colar bandas entre elas, igualar referência por instância.
- [ ] MacBook Pro 2013 (Catalina): abrir, redimensionar, 24 bandas em LP.
- [ ] Windows: instalador e desinstalador.

## Problemas conhecidos / limitações

* Linear Phase: a dinâmica atualiza por salto (11 ms em Baixa, 21 ms em Média, 43 ms em Alta a 48 kHz) — para dinâmica muito rápida use ZL ou NP. Bandas Esquerda/Direita são aplicadas antes das Mid/Side.
* Natural: shelves muito ressonantes (Q > 2) muito perto de 20 kHz podem afastar-se até ~2 dB do protótipo analógico.
* A ligação estéreo dos detetores é fixa (bandas estéreo usam sempre o mesmo ganho nos dois canais).
* As Smart Ops aprendem a partir do que o analisador recebe: num *bounce* mais rápido que tempo real podem perder-se blocos de análise (o áudio nunca é afetado). Aprenda em reprodução normal.
* Sem assinatura/notarização pagas: no primeiro arranque o macOS/Windows pedem confirmação (ver LEIA-ME).
