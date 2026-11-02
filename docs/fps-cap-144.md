# Diagnóstico do Teto de ~60 FPS e Caminho para 144 FPS

**Status:** documentação de análise estática (somente leitura), 2026-11-01. Nenhuma alteração de
código derivada desta investigação foi aplicada. Este arquivo registra **por que** o trabalho de
144 FPS foi adiado e o que precisa ser feito quando for retomado.

> **Decisão registrada.** Após o diagnóstico completo, a equipe decidiu **adiar** o trabalho de
> 144 FPS. A razão não é falta de mapeamento — a causa do teto está totalmente identificada —, mas o
> fato de que subir além de 60 FPS exige **re-derivar as janelas de tempo do jogo** (input, câmera,
> combate), não apenas "levantar um cap". Este documento é a fonte de referência para retomar o
> trabalho sem refazer a engenharia reversa.

---

## 0. Resumo executivo

O jogo roda a ~55–61 FPS durante o gameplay (não 144) por uma **soma de três premissas de 60 FPS**,
nenhuma delas um gargalo de CPU/GPU nem o VSync do host:

1. **30 FPS nativo** — o alvo da simulação é `fmov s0,#30.0` hardcoded no binário.
2. **Patch Dynamic-FPS** — sobe isso para **60 FPS**, e é explicitamente um *"60 fps cap"*.
3. **Pacer do port** — o default de apresentação é `EXTRA_DEFAULT_TARGET_FPS 60`, o throttle real.

O monitor é 144 Hz com present mode *mailbox* (não é o limitador), e a telemetria mostra CPU ~37 % /
GPU ~12 % (não é bottleneck). A alavanca para 144 é trivial (`AC_FPS=144` ou overlay → 144), mas a
**fluidez percebida** só melhora se câmera + input também forem desacopladas de 60 — o que exige
rework das janelas de tempo, não um cap.

---

## 1. As três camadas do teto de 60 FPS

### 1.1 Camada 1 — 30 FPS nativo no binário

A função de setup do timer é chamada de `0x3e3cf4` com uma constante de FPS hardcoded:

```
0x3e3d08: fmov s0, #30.0        ; alvo de 30 FPS fixo no código
0x3e3d10: bl   #0x91d8a0        ; setup do timer  -> [timer+0xDC] = 30/1000
```

- `[timer+0xDC]` = `fps/1000` é a **escala de velocidade do jogo** (step = `dt_ms × fps/1000`),
  **não** um cap de render. A simulação é um tick fixo de 30 Hz.
- O objeto timer é a global BSS `0xDCBFD0` (GOT `0xDB89A0`); métodos: reset `0x91d820`, setup
  `0x91d8a0`, lock `0x91d930`, producer `0x91dbbc`.

> **Nota importante (hipótese descartada).** A tabela rodata `[60.0, 120.0]` em `0xb5e8f8` seleciona
> campos de "tipo" de frame-rate (`+0x2008/+0x287c/+0x1268/+0x650`), **não** a taxa de produção de
> frames. Os nomes `MotionFrameRateType`/`DrawFrameRateType`/`ProgressFrameRateType` também são
> propriedades de efeito/animação, não um seletor de 30/60 (ver `reverse-engineering-pc.md` §7.3).

### 1.2 Camada 2 — Patch Dynamic-FPS (60 FPS, um *"60 fps cap"* explícito)

`third_party/AstralChain-Dynamic-FPS/1.0.1.pchtxt`, linha 3:

> `# Astral Chain [...] v1.0.1 - Dynamic-60fps v1.1.1 (60 fps cap)`

O que o patch faz (endereços 1:1 com offsets do `main`, flag `offset_shift 0x100`):

| Endereço | Patch | Efeito |
|---|---|---|
| `0x9E8D84` / `0x9DB6BC` | `mov w1,#1` | força o modo de 60 FPS (init / runtime) |
| `0x91DBDC` / `0x91DC10` | branch p/ cave | passo de tempo medido pelo frame real (dt-based) |
| `0xA0E7D0` | `ldr s1,[x19,#0xDC]` | lê `frames per ms` do timer |
| `0xA0E7D4` | `fmul s8,s0,s1` | passo medido = `dt × fps/1000` |
| `0x48BE14` / `0x57E7DC` | `ldr w8,[..,#0xD8]` | fix dos timers de ação/input (contam ticks inteiros) |

As duas correções que o patch aplica **especificamente para 60 FPS** (e que são o ponto de risco
para 144) estão documentadas nas linhas 4–6 do próprio patch:

- *"Action and input timers (move input windows, double-tap windows, action delays) count whole
  30 FPS frames."*
- *"Camera smoothing uses the frame time instead of a fixed fraction per frame."*

### 1.3 Camada 3 — Pacer do port (o throttle real)

O present do jogo passa por `nvn_present_pace(w->interval)` (`astral_chain/runtime/las/nvn/objects.c:813`)
→ `extra_nvn_present_pace(interval)` (`extra/platform/pace_trampoline.h`), que faz o *park* do thread de
apresentação até o próximo tick do grid de vsync emulado.

- Default de FPS: `#define EXTRA_DEFAULT_TARGET_FPS 60` (`pace_trampoline.h:23`).
- Sobrescrito por `AC_FPS` / `AC_TARGET_FPS` / `g_extra_fps_override` (overlay).
- O overlay já expõe `s_fps_steps = {0, 30, 60, 90, 120, 144, 165}` (`extra/ui/overlay/ac_menu.cpp:107`).
- `objects.c:810` é explícito sobre a premissa de 60 Hz: *"a Switch display refreshes at 60 Hz and
  the window shows a new image at most every `interval` refreshes."*

O loop principal do jogo (`0x3e3948`) gira em cima do flag de conclusão de present `[0xdcc430]`
(reloc → `0xdcc430`, limpo pela callback `0x3e2c80`), então a **taxa de produção de frames = taxa de
present = 60 FPS**.

Isso explica a telemetria: durante **streaming** o jogo apresenta com `interval=0` (unthrottled →
400–620 FPS), mas no **gameplay** `interval=1` ativa o pacer de 60 (55–61 FPS, p50≈17 ms).

---

## 2. Por que não é "aplicar as mesmas mudanças do patch" para 144

A **estratégia** do patch (passo dt-based + recontagem de janelas + câmera por frame-time) é correta
e reutilizável para 144. Mas os **números** estão quantizados para 60, então não é copy-paste:

| Fix do patch | Premissa de 60 FPS | O que muda a 144 FPS |
|---|---|---|
| Janelas de input / double-tap / delays | contam *frames inteiros* de 60 Hz (16.7 ms) | 1 frame = 6.9 ms; cada janela em "N frames" vale N/2.4 em tempo real — precisa re-derivação por janela |
| Clamp de passo máximo `fmov s1,#2.0` | 2 ticks = 66 ms de catch-up | 2 ticks = 66 ms é pouco demais para stalls a 144 Hz; precisa re-escala (o port já usa 8.0 no `cutscene-catchup`) |
| Camera smoothing (frame-time based) | — | **escala naturalmente** (já usa dt real, não fração fixa) |

Conclusão: o único fix que "sobe sozinho" é a câmera. Input/timers/clamp precisam de re-derivação
explícita para 144.

---

## 3. As três taxas independentes (por que "uncap só a produção de frame" é insuficiente)

O jogo tem três taxas desacopláveis, e só duas importam para fluidez percebida:

| Taxa | O que é | Impacto na fluidez |
|---|---|---|
| **Render/present** | a taxa que o monitor mostra | sozinha, quase nada |
| **Câmera + input** | onde o input é lido e a câmera se move | **dominante** |
| **Simulação** (combate/física) | tick lógico 30 Hz | quase irrelevante p/ fluidez |

Implicações práticas:

- **Uncap só da produção de frame** (render 144, câmera/sim 30) = **ganho marginal**. A câmera
  continua atualizando a cada ~4.8 frames → o *pan* continua com o mesmo "pulo" de 33 ms, agora
  renderizado em 144. Animações são keyframed a 30 Hz e não interpolam sozinhas.
- **Uncap de câmera + input** (ler input e mover câmera a 144, sim continuando dt-correta) = **é isso
  que dá fluidez real**. Por isso o patch de 60 tem o fix dedicado de câmera por frame-time.
- A **simulação de gameplay** (combate, timers de ação) pode permanecer na cadência baixa desde que
  seja dt-correta — ela não afeta a suavidade que o olho percebe.

> A hipótese "manter sim padrão e só uncap frame/input" está **parcialmente correta**: o "sim padrão"
> que se pode manter é o tick lógico; **câmera e pipeline de input precisam ser uncapped** junto com
> a produção de frame. Manter câmera/input em 30/60 enquanto só o render sobe não entrega fluidez.

---

## 4. Caminho recomendado (quando retomar)

1. **Validação barata primeiro**: `AC_FPS=144` (ou overlay → 144). Só a alavanca de throttle, zero
   patch no binário. Confirma empiricamente que a câmera é o gargalo de fluidez.
2. **Passo sim dt-based**: já pronto (Dynamic-FPS).
3. **Câmera por frame-time**: já pronto, escala para 144.
4. **Re-derivar janelas de input/double-tap/ação para 144** — o ponto que o patch de 60 deixou
   quantizado em 60 (linhas 4–6 do patch).
5. **Re-escalar o clamp de passo máximo** (`s1=2.0` → valor proporcional a 144, no mesmo padrão do
   `cutscene-catchup` que já usa 8.0).

---

## 5. Como regenerar a evidência

Análise feita inline (shell + Python) sobre `astral_chain/nso.py` + Capstone 5.0.7, do NSO
`game/update-exefs/main` (build_id `4b159f0f...`). Endereços relativos ao `main` (base guest
`0x7000010000`), válidos para v1.0.1 com `1.0.1.pchtxt` + `cutscene-catchup-1.0.1.pchtxt` aplicados.

- Caller do setup do timer (`fmov s0,#30.0`): `0x3e3d08`/`0x3e3d10`.
- Timer global BSS `0xDCBFD0` (GOT `0xDB89A0`); métodos `0x91d820`/`0x91d8a0`/`0x91d930`/`0x91dbbc`/`0x91dce0`.
- Flag de conclusão de present `[0xdcc430]`: loop principal `0x3e39d8` (spin), callback `0x3e2c80` (clear).
- Pacer: `extra/platform/pace_trampoline.h` (`EXTRA_DEFAULT_TARGET_FPS 60`), `submission.c` `vk_pace_frame`
  (busy > 16.2 ms → `pace_slow` 30 FPS), `objects.c:813` `nvn_present_pace`.

---

## 6. Convenções e segurança

- Análise **somente leitura**; nenhum arquivo de código foi modificado.
- Endereços invalidam-se com qualquer rebase/re-patch; re-derivar conforme §5.
- Nenhum asset proprietário, dump, chave ou shader foi versionado.
