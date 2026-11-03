# EQMotion by Piradex

**Um equalizador dinâmico que mostra o que está a fazer.**
Versão **2.1.0** · VST3, Audio Unit e AAX para macOS (10.13+, Intel e Apple Silicon — corre num MacBook Pro 2013 com Catalina) e VST3 + AAX para Windows 10/11 64 bits · interface em **Português e Inglês**.

![EQMotion](docs/ui_principal.png)

## O que tem

| Área | Funções |
|---|---|
| **Curva** | Até 24 bandas · 9 formas: sino, low/high shelf, low/high cut, notch, passa-banda, all-pass, tilt · cortes de 6 a 96 dB/oct + Brickwall · duplo clique para criar, arrastar, roda para o Q, duplo clique no nó para apagar · analisador ao vivo por baixo |
| **Dinâmica** | Qualquer sino/shelf/tilt passa a dinâmico · limiar, range (com sinal), ratio, ataque, hold, release, joelho · acima/abaixo (comprimir/expandir) · detetor da pista ou sidechain · Peak/RMS · lookahead global · halo e ponto "ao vivo" no gráfico + medidor no painel |
| **Smart Ops** | 15 operações: Deresonate, De-ess, Limpar lama, Caixa e nasal, Domar dureza, Remover zumbido (50/60 Hz), Auto cortes, Controlar graves, Auto dinâmico, Unmask, Realçar presença/ar/calor, Equilíbrio tonal, Igualar referência (sidechain ou outra instância) · Aprender (≥ 2 s) → Rever (tracejado no gráfico + lista das bandas) → Aplicar · Intensidade em tempo real · uma aprendizagem serve várias operações · Aplicar e Reverter são um passo de desfazer cada; Reverter só remove as bandas da operação |
| **Comparar** | A/B com cópia · desfazer/refazer · Auto gain (comparações à mesma loudness) · Delta (só o que o EQ muda, alinhado em latência) · Listen e Bypass por banda |
| **Fase e estéreo** | Zero Latency · Natural Phase (igual ao analógico até 20 kHz) · Linear Phase (3 qualidades) · Estéreo, Esq./Dir., Mid/Side · colocação por banda: estéreo, L, R, M ou S |
| **Character** | Clean (sem harmónicos, nulo exato) · Subtle · Warm · De-harsh dinâmico |
| **Instâncias e presets** | Todas as instâncias do projeto num painel, com curvas e espetros sobrepostos · 246 presets de fábrica em 15 pastas (200 novos na 2.1) · presets do utilizador com pesquisa e favoritos |
| **Idioma** | Seletor **PT \| EN** em *Definições › Idioma* e no ecrã de senha · troca imediata · primeira abertura segue o idioma do sistema · escolha comum a todos os plugins Piradex · nomes dos presets de fábrica traduzidos (o nome guardado na sessão não muda) · nomes de automação no DAW fixos |
| **Acesso** | Ecrã de senha igual aos outros plugins Piradex (senha master + senhas beta com validade, "lembrar neste computador"). Sem sessão o plugin deixa passar o áudio sem processar. |

## Novo na 2.1.0

* **Smart Ops: passo 5 "Aplicar" corrigido** — os botões Aplicar/Reverter ficam sempre visíveis no fundo do painel (também na janela mínima) e o texto da revisão já não se sobrepõe.
* **9 operações novas**: Limpar lama, Caixa e nasal, Domar dureza, Remover zumbido 50/60 Hz, Controlar graves (boom), Auto dinâmico, Realçar presença, Realçar ar, Realçar calor — menu agrupado em *Corrigir*, *Dinâmica* e *Realçar e equilibrar*.
* A revisão mostra a lista das bandas propostas; depois de aplicar, a aprendizagem fica para a próxima operação.
* **200 presets novos** em 10 categorias (Voz, Bateria, Baixo, Guitarras, Teclas e Synths, Dinâmica e Ressonâncias, Estéreo e M/S, Mistura e Masking, Mastering, Criativo) — 246 no total, com nomes e descrições PT/EN.

## Downloads

Cada push compila e valida no GitHub Actions (auval Intel + Apple Silicon, pluginval, testes de áudio) e publica a versão **EQMotion by Piradex 2.1.0** na página *Releases* (no repositório do ANALYSER: `eqmotion-v2.1.0`):

* `EQMotion-by-Piradex-<versão>-macOS.dmg` — instalador `.pkg` + plugins para instalação manual
* `EQMotion-by-Piradex-<versão>-Windows-Setup.exe` — instalador (ou o `.zip` para copiar à mão)
* `EQMotion-by-Piradex-<versão>-macOS-AAX-*.zip` e `...-Windows-AAX-*.zip` — Pro Tools. O Pro Tools normal só carrega AAX assinados pela PACE: com os *Secrets* `PACE_ACCOUNT`, `PACE_PASSWORD`, `PACE_WCGUID`, `PACE_EDEN_URL`, `PACE_EDEN_WIN_URL`, `PACE_KEYFILE_B64`, `PACE_KEYPASSWORD` o CI assina sozinho (`...-AAX-assinado.zip`); sem eles sai `...-AAX-nao-assinado.zip` (só *Pro Tools Developer*).

## Compilar localmente

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release           # descarrega o JUCE 8.0.15
cmake --build build --config Release --target EQMotion_VST3 EQMotion_AU   # (AU só no Mac)
```

Testes:

```bash
g++ -O2 -std=c++17 -ISource tests/test_dsp.cpp -o test_dsp && ./test_dsp
g++ -O2 -std=c++17 -ISource tests/test_smartops.cpp -o test_smartops && ./test_smartops
cmake -B build -DEQM_BUILD_TESTS=ON && cmake --build build --target eqm_plugin_test && ./build/eqm_plugin_test
```

## Senhas

O código só contém o *hash* PBKDF2-HMAC-SHA256 (60 000 iterações) de cada senha. Para criar mais senhas beta:

```bash
python3 tools/nova_senha_beta.py 2027-03-31
```

e cole a linha gerada em `kKeys` (`Source/License.cpp`). O push volta a compilar tudo.

## Documentação

* [Manual](docs/MANUAL.md)
* [Arquitetura, decisões de DSP e latência](docs/ARQUITETURA.md)
* [Validação e critérios de aceitação](docs/VALIDACAO.md)

---
Piradex · www.beatfreakstudio.com · suporte: juninhopiradex@hotmail.com
