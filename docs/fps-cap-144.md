# Diagnóstico do Teto de ~60 FPS e Caminho para 144 FPS

**Status:** análise estática original de 2026-11-01, **revisada**. A conclusão da época — que subir
acima de 60 FPS exigiria re-derivar as janelas de tempo do jogo — estava **errada** para o patch que
o projeto usa (§2). O teto agora é uma opção do launcher (`AC_FPS_UNLOCKED`, patch
`dynamic-unlocked-1.0.1.pchtxt`) e o pacing do port deriva o orçamento de frame de `AC_FPS`.

> **O que mudou.** O patch vendorizado já converte os timers de ação/input para **ticks inteiros de
> 30 FPS**, então uma janela de "15 frames" vale 15/30 s em qualquer taxa de atualização. Não há
> re-derivação por janela pendente. O que de fato faltava eram dois pontos no runtime do port: o
> veredito de "frames não cabem" (`busy > 16.2 ms`, orçamento fixo de 60 Hz) e o piso de tempo
> reportado no modo 30 fps (`PACE_SLOW_NS`, também de 60 Hz). Ambos agora escalam com o alvo.

> **Nota histórica.** O texto original (§2, tabela) é preservado abaixo com a correção marcada, para
> que a conclusão errada não seja redescoberta como se fosse nova.

---

## 0. Resumo executivo

O jogo roda a ~55–61 FPS durante o gameplay (não 144) por uma **soma de três premissas de 60 FPS**,
nenhuma delas um gargalo de CPU/GPU nem o VSync do host:

1. **30 FPS nativo** — o alvo da simulação é `fmov s0,#30.0` hardcoded no binário.
2. **Patch Dynamic-FPS** — sobe isso para **60 FPS**, e é explicitamente um *"60 fps cap"*. A variante
   `Dynamic-Unlocked` do mesmo mod remove esse cap com duas instruções (§1.2).
3. **Pacer do port** — o default de apresentação é `EXTRA_DEFAULT_TARGET_FPS 60`, o throttle real.

O monitor é 144 Hz com present mode *mailbox* (não é o limitador), e a telemetria mostra CPU ~37 % /
GPU ~12 % (não é bottleneck). A alavanca para 144 é trivial (`AC_FPS=144` ou overlay → 144), mas a
**fluidez percebida** só melhora se câmera + input também forem desacopladas de 60 — e, no patch
atual, elas já estão (§2).

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

As duas correções que o patch aplica estão documentadas nas linhas 4–6 do próprio patch:

- *"Action and input timers (move input windows, double-tap windows, action delays) count whole
  30 FPS frames."*
- *"Camera smoothing uses the frame time instead of a fixed fraction per frame."*

**Onde está o cap, e como removê-lo.** `0x9E8D84`/`0x9DB6BC` recebem `mov w1,#1`, e esse `1` é a
única coisa que prende o jogo a 60 FPS. O mesmo autor publica a variante `Dynamic-Unlocked`
(`yogesh1239/AstralChain-Dynamic-FPS`, `mods/Dynamic-Unlocked/exefs/1.0.1.pchtxt`) com exatamente
duas palavras diferentes — `01008052` (`mov w1,#0`) nos dois endereços. O arquivo do repositório é
*o delta*, não uma cópia do mod:

- `astral_chain/patches/dynamic-unlocked-1.0.1.pchtxt` — 2 palavras, aplicado **depois** do vendor
  e **antes** do `cutscene-catchup`. Motivo: um refresh do patch vendorizado é herdado
  automaticamente, em vez de criar um fork de 43 KB que envelhece em silêncio.
- `astral_chain/tools/fps_patch_test.py` fixa o delta (os dois endereços, os dois imediatos, o
  mesmo build ID), então uma edição acidental em qualquer um dos três arquivos falha o teste.
- Launcher: *Settings → Graphics → "Desbloquear o teto de 60 FPS do jogo"* grava `fps_unlocked` e
  exporta `AC_FPS_UNLOCKED`; `play.sh` promove a variável para o build (não só para a execução),
  porque a escolha do patch é entrada de build e muda o `guest_fingerprint`.

O mod tem dois limites de passo, ambos em ticks de 30 FPS: um **piso** de `1/16` tick
(`0xA0E7EC`: `mov w8,#0x3D800000` → `fmov s1,w8`, aplicado com `fmaxnm`) e um **teto** de `2.0` ticks
no gameplay (`0xA0E7F8`), que o `cutscene-catchup` troca por `8.0` em cutscenes. O piso é o teto
efetivo de taxa: no máximo 16 frames por tick de simulação, ou seja **480 FPS**; o autor declara
15–480 FPS com velocidade correta.

### 1.3 Camada 3 — Pacer do port (o throttle real)

O present do jogo passa por `nvn_present_pace(w->interval)` (`astral_chain/runtime/las/nvn/objects.c:813`)
→ `extra_nvn_present_pace(interval)` (`extra/platform/pace_trampoline.h`), que faz o *park* do thread de
apresentação até o próximo tick do grid de vsync emulado.

- Default de FPS: `#define EXTRA_DEFAULT_TARGET_FPS 60` (`extra/platform/pace_target.h`).
- Sobrescrito por `AC_FPS` / `AC_TARGET_FPS` / `g_extra_fps_override` (overlay).
- Valores aceitos: `0` (ilimitado) até `EXTRA_TARGET_FPS_MAX` (1000). Um valor vazio, com 16 bytes ou
  mais, com lixo (`60fps`), negativo ou acima do teto é tratado como **ausente** e cai no padrão
  (antes, `atoi` lia o prefixo numérico de `60fps` como 60 e `abc` como 0, e um número enorme era
  indefinido). O limite superior também garante que `19200000 / fps` no pacer nunca chegue a zero.
- O overlay já expõe `s_fps_steps = {0, 30, 60, 90, 120, 144, 165}` (`extra/ui/overlay/ac_menu.cpp:107`).
- `objects.c:810` é explícito sobre a premissa de 60 Hz: *"a Switch display refreshes at 60 Hz and
  the window shows a new image at most every `interval` refreshes."*

O loop principal do jogo (`0x3e3948`) gira em cima do flag de conclusão de present `[0xdcc430]`
(reloc → `0xdcc430`, limpo pela callback `0x3e2c80`), então a **taxa de produção de frames = taxa de
present = alvo do pacer** (`interval=1` → 1 present por refresh do alvo).

Isso explica a telemetria: durante **streaming** o jogo apresenta com `interval=0` (unthrottled →
400–620 FPS), mas no **gameplay** `interval=1` ativa o pacer (55–61 FPS com o alvo em 60, p50≈17 ms).

**A heurística de "modo 30 fps" também era de 60 Hz.** `vk_pace_frame`
(`.../exec/submission/submission.c`) derrubava o jogo para o modo 30 fps quando 12 dos últimos 60
frames passavam de **16.2 ms de busy**, e reportava `PACE_SLOW_NS = 20 ms` como piso nos contadores
de GPU. A 144 Hz o orçamento real é 6.9 ms: os dois números deixavam passar frame com o dobro (ou
mais) do orçamento. Agora ambos derivam de `AC_FPS` (`extra/platform/pace_target.h`):

| Alvo | Over budget | Headroom | Piso reportado |
|---|---|---|---|
| 30 | 32.4 ms | 26.0 ms | 40 ms |
| 60 | 16.2 ms | 13.0 ms | 20 ms |
| 90 | 10.8 ms | 8.667 ms | 13.33 ms |
| 120 | 8.1 ms | 6.5 ms | 10 ms |
| 144 | 6.75 ms | 5.417 ms | 8.333 ms |

Os três valores de 60 fps (16.2, 13.0 e 20.0 ms) são as **constantes de referência** em
`extra/platform/pace_target.h` (`EXTRA_PACE_OVER_MS` / `_CALM_MS` / `_SLOW_MS`) e os demais alvos saem
de `valor * 60 / AC_FPS`. Ancorar nos valores e não no período (`1000/AC_FPS * fração`) é o que torna
60 fps **bit-a-bit** o antigo: `(1000/60) * 0.780` cai 1 ULP acima de `13.0` (`13.000000000000002`),
então um frame de exatamente 13.0 ms de média deixaria de contar como headroom. O teste em
`extra/tools/pace_target_test.c` compara frame a frame com a implementação antiga e trava os três
literais com `==`, não com tolerância.

Acima de 60 o fallback fica **desligado**: com o passo medido do Dynamic-FPS o mundo já corre na
velocidade certa, e entrar em 30 fps só jogaria fora a suavidade que o alvo maior comprou.

---

## 2. Por que não é "aplicar as mesmas mudanças do patch" para 144

> **Correção (2026-11-06).** A tabela abaixo está **errada** quanto às janelas de input. O patch
> conta ticks inteiros de **30 FPS**, não frames de 60 Hz: a janela de "N frames" vale `N/30` s em
> qualquer taxa de atualização. O próprio código confirma — o cave `0xA0E83C` (e os 104 sítios
> `EXPERIMENTAL r2/r3/r4/r5`, 21 caves) faz `ldr w9,[x9,#0xd8]` (ticks inteiros deste frame), `cbz` para 0
> ticks, e então `cmp/ccmp/csel/subs w8,w8,w9` — subtrai N ticks, não 1. A linha "precisa
> re-derivação por janela" foi escrita a partir do comentário do patch ("count whole 30 FPS
> frames"), mas interpretada como frames de 60. Mantida para registro; ver §4 para o estado real.

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

## 4. Estado real (revisado em 2026-11-06)

| Item | Estado |
|---|---|
| Passo sim dt-based | **pronto** (Dynamic-FPS) |
| Câmera por frame-time | **pronto** (escala naturalmente) |
| Janelas de input/double-tap/ação | **pronto** — contam ticks de 30 FPS (ver correção na §2) |
| Cap de 60 FPS do patch | **removível** — `AC_FPS_UNLOCKED=1` / opção no launcher (§1.2) |
| Orçamento de frame do port | **corrigido** — deriva de `AC_FPS` (§1.3) |
| Clamp de passo máximo | 2 ticks no gameplay (66 ms), 8 no cutscene (`cutscene-catchup`) — suficiente em 144 |
| Papel de papel do "sim 30 Hz" | **não se aplica**: o passo medido mantém a velocidade do mundo |

O que resta não é engenharia reversa, é **medição**: subir o alvo e verificar (a) que a velocidade do
jogo continua correta e (b) que a câmera, agora alimentada pelo mouse em 144 Hz, não apresenta o
"pulo" descrito na §3. Se aparecer artefato, a causa provável é apresentação (present mode, tearing,
latência), não o passo da simulação.

Passos:

1. Launcher → *Settings → Graphics*: marcar **"Desbloquear o teto de 60 FPS do jogo"** e pôr
   *Target FPS* no valor do monitor (ex. 144). O primeiro *Play* reconstrói (o patch é entrada de
   build) e o launcher mostra `Build: stale` até isso acontecer.
2. Sem launcher: `AC_FPS_UNLOCKED=1 AC_FPS=144 ./play.sh`.
3. Telemetria: `LARECOMP_LOG_PACE=1` imprime `period/blocked/busy` por frame; os contadores de GPU
   mostram `pace target, N frames over` (antes: `pace 60` ou `pace 30`).
4. Se o jogo acelerar ou desacelerar, o passo medido não está sendo aplicado — conferir se o build
   realmente pegou o patch (`Applied N patch words from ...dynamic-unlocked-1.0.1.pchtxt` no log do
   `setup_game`).

---

## 5. Como regenerar a evidência

Análise feita inline (shell + Python) sobre `astral_chain/nso.py` + Capstone 5.0.7, do NSO
`game/update-exefs/main` (build_id `4b159f0f...`). Endereços relativos ao `main` (base guest
`0x7000010000`), válidos para v1.0.1 com `1.0.1.pchtxt` + `dynamic-unlocked-1.0.1.pchtxt` (opcional)
+ `cutscene-catchup-1.0.1.pchtxt` aplicados.

- Caller do setup do timer (`fmov s0,#30.0`): `0x3e3d08`/`0x3e3d10`.
- Timer global BSS `0xDCBFD0` (GOT `0xDB89A0`); métodos `0x91d820`/`0x91d8a0`/`0x91d930`/`0x91dbbc`/`0x91dce0`.
- Flag de conclusão de present `[0xdcc430]`: loop principal `0x3e39d8` (spin), callback `0x3e2c80` (clear).
- Pacer: `extra/platform/pace_target.h` (`EXTRA_DEFAULT_TARGET_FPS 60`, `extra_pace_budget_ms`),
  `extra/platform/pace_trampoline.h` (`extra_nvn_present_pace`), `submission.c` `vk_pace_frame`
  (orçamento derivado de `AC_FPS`), `objects.c:813` `nvn_present_pace`.
- Delta unlocked: `astral_chain/tools/fps_patch_test.py` valida os dois endereços e imediatos.

---

## 6. Convenções e segurança

- Análise de RE é **somente leitura**; as mudanças desta revisão são no runtime do port e no launcher.
- Endereços invalidam-se com qualquer rebase/re-patch; re-derivar conforme §5.
- Nenhum asset proprietário, dump, chave ou shader foi versionado.
