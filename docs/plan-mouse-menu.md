# Plano: sensibilidade de câmera por mouse + cursor e clique em menus

**Status:** implementado na working tree (não commitado), 2026-11-01. O documento registra o que
foi medido no binário e no runtime, a arquitetura resultante e o único ponto que depende de
validação visual em jogo. Segue `AGENTS.md` (Linux First, isolamento em `extra/`, marcador
`// [extra]`, teto de 400 linhas, sem retrocesso).

---

## 1. Arquitetura entregue

| Peça | Arquivo | O que faz |
|---|---|---|
| Curva de câmera | `extra/input/mouse_mode.h` | velocidade (contagens/s) → deflexão do stick direito: `pow()` por curva, piso de 10 %, `yscale`, inversão |
| Árbitro de modo | `extra/input/mouse_mode.h` | `extra_mouse_mode_set/toggle/is_captured/suspend/resume`; o app **nasce com o cursor livre** |
| Ponteiro de menu | `extra/input/mouse_menu.h` | movimento → passos de D-pad a cada `AC_MOUSE_MENU_CELL` pontos; esquerdo → A; direito → B; roda → D-pad; contato de touch opcional |
| Política de menu | `extra/input/menu_detect.h` | chave de pausa (MINUS) espelha o menu de pausa; tecla de stick toma a câmera; F10 alterna |
| Config runtime | `extra/input/mouse_mode.h` | `AC_MOUSE_SENS/INVERT/YSCALE/CURVE` |
| Launcher | `extra/ui/launcher/src/MouseSettingsWidget.cpp` | "Mouse & Câmera (PC)": sensibilidade, escala Y, curva, inverter, navegação/clique em menus, passo |
| Hook no HID | `astral_chain/runtime/input/hid_service.h` | `extra_mouse_menu_apply` no publish; `extra_mouse_menu_fill_touch` no anel de touch |
| Hook na câmera | `astral_chain/runtime/input/mouse.h` | `mouse_config` e `mouse_look_stick` delegam a `extra/` |
| Eventos SDL3 | `extra/sdl/window_trampoline.h` | F10/F11 com `!repeat`, cursor de menu, suspend/resume no foco |
| Callbacks de input | `astral_chain/runtime/services/app_window.h` | motion/button/wheel redirecionam ao ponteiro de menu quando o cursor está livre |

Curva de câmera (`extra_mouse_calculate_stick`):

```
x,y    = velocidade * sens / 1500          (y escalado por yscale e invert)
m      = |(x,y)|, saturado em 1.0
curved = m ^ curve_exp                      (linear=1.0, power=1.35, aggressive=1.75)
out    = 0.10 + 0.90 * curved
stick  = direção * out * 32767
```

`AC_MOUSE_SENS=100` + `AC_MOUSE_CURVE=linear` reproduz a resposta anterior; a única diferença
intencional é o piso de deflexão (0.2 → 0.10), que remove a deriva residual em movimento lento.

---

## 2. Como o ponteiro chega a um menu

O ponto central da investigação: **qual canal o jogo lê por frame**. Resposta medida (§4.1): o
`main` lê `NpadState`, `TouchScreenState`, teclado **e** o mouse a cada frame — mas o wrapper do
mouse só constrói um objeto que nada mais no binário lê, e o jogo nunca envia o comando HID de
inicialização do mouse. Não existe, portanto, hit-testing de mouse a alimentar. Os canais que um
menu realmente obedece são o pad e (em tese) o painel:

| Entrada | Tradução | Por que funciona em qualquer tela |
|---|---|---|
| movimento | passos de D-pad a cada N pontos | toda lista do jogo responde a D-pad |
| clique esquerdo | tap de A (0.06 s, com 0.05 s de folga entre taps) | confirma o item que já está selecionado |
| clique direito | tap de B | volta, universal |
| roda | D-pad cima/baixo | rolagem de lista |
| opcional (`AC_MOUSE_MENU_TOUCH=1`) | contato real de touch no ponto do cursor | telas que fazem hit-test no painel |

**A seleção não pode ser mirada por posição absoluta.** Medido em §4.4: o jogo não expõe geometria
de item nem o índice selecionado, e o único menu em que um contato de touch foi aplicado sobre um
item (LOAD DATA, slots A e B) **não** mudou a seleção. Uma "seleção posicional" derivada de células
seria inventada: em um menu de 2 itens, andar 45 passos à esquerda volta ao item de origem, o que
já produziu capturas em que a seleção parecia não responder. O ponteiro, portanto, mapeia para
passos relativos; `extra/tools/mouse_pointer_test.c` guarda essa decisão como regressão.

Sem zonas hard-coded, sem coordenadas de tela de título: o comportamento é o mesmo em título,
pausa, IRIS, loja e diálogos. A única limitação é inerente ao jogo — menus que não respondem a
D-pad nem a touch não têm como receber um ponteiro.

O passo direcional é ligado por padrão (`AC_MOUSE_MENUS=1`); `AC_MOUSE_MENU_CELL` (px, padrão 40)
calibra a sensibilidade da lista. `AC_MOUSE_MENU_MOTION=0` desliga só os passos (clique e roda
continuam).

---

## 3. Política de modo (cursor livre × câmera)

O jogo não expõe "há um menu aberto" ao host (§4.2), então a política segue o jogador:

1. **Início**: cursor livre e centrado. A tela de título é clicável sem tocar no teclado.
2. **Tecla de stick** (WASD/FTHG por padrão, nunca D-pad): o mouse é tomado para a câmera — mas só
   se o ponteiro estiver parado há mais de 1,2 s. Um jogador que acabou de usar o mouse e anda a
   lista com WASD mantém o cursor; quem começa a jogar a partir de um ponteiro em repouso toma a
   câmera sem apertar mais nada.
3. **Tecla de pausa** (MINUS, Esc por padrão): alterna. É a mesma tecla que abre e fecha o menu de
   pausa, então o cursor acompanha o ciclo exato.
4. **F10**: alterna em qualquer momento.

| Variável | Padrão | Efeito |
|---|---|---|
| `AC_MOUSE_AUTO_MENU` | `1` | regra 2 ligada; `0` deixa só pausa/F10 |
| `AC_MOUSE_MENU_TIMEOUT` | `0` | `1` devolve a câmera após 5 s sem uso do ponteiro |
| `AC_MOUSE_MENUS` | `1` | ponteiro de menu (passos, A/B, roda) |
| `AC_MOUSE_MENU_MOTION` | `1` | passos de D-pad ao mover; `0` deixa só clique e roda |
| `AC_MOUSE_MENU_TOUCH` | `0` | `1` publica contato de touch no cursor |
| `AC_MOUSE_MENU_CELL` | `40` | pontos por passo de D-pad (10–200) |

Alt+Tab devolve o cursor e o restaura ao voltar (`extra_mouse_mode_suspend/resume`), preservando a
escolha explícita de menu. O warp que centraliza o ponteiro ao entrar no modo menu é marcado para
não contar como movimento (`extra_mouse_menu_skip_motion`), senão o salto até o centro avançaria a
lista sozinho.

---

## 4. Engenharia reversa (`nn::hid`) — o que foi realmente medido

Binários: `game/update-exefs/{main,sdk,rtld}`. `main` com base `0x7000010000`, `sdk` com
`0x700cd00000`. Segmentos de `main`: `.text [0x0,0xa0e7d0)`, `.rodata [0xa0f000,+0x325848)`,
`.data [0xd35000,+0x8f870)`, BSS `[0xdc5000,…)`.

### 4.1 O mouse **é** lido por frame, mas nada consome o resultado

> Correção de uma medição anterior deste documento. A conclusão antiga ("o mouse não é
> consumido por frame") era **falsa**: o scan com Capstone parava na primeira instrução inválida e
> decodificava só um fragmento do `.text`. Com `md.skipdata=True` o scan cobre 2.636.276
> instruções até `0xa0e7d0`, e a cadeia aparece inteira.

| Evidência | Resultado |
|---|---|
| Call site `GetMouseState` | `main+0x9acafc` (stub PLT `0xa0e050`) — 1 ocorrência |
| Cadeia por frame | `0x3e4028 → 0x3e4120 → 0x577000 → 0x9acd50 → {0x9ac1d0 (touch), 0x9ac980 (teclado), 0x9acad0 (mouse)}` |
| Call site `GetKeyboardState` | `main+0x9ac9cc` (1) |
| Call site `GetTouchScreenState<16>` | `main+0x9adb1c` (1) |
| Call site `GetNpadState` | `main+0x9ae50c` (1) |
| Objeto de estado do mouse | `*(0xdc2540)`; campos `x,y` em `+0x14/+0x18`, bordas em `+0x1c/+0x20` |
| Quem **lê** os campos desse objeto | **ninguém**: os 14 acessos via ponteiro são o próprio writer (`0x9acad0`) e um reset em `0x9ac180` |
| Estado medido em runtime | sempre `raw=[0 0 0 0] x=0.0 y=0.0`, porque o jogo **nunca** envia o comando HID 21 (`mouse_active` fica 0) |

Ou seja: o jogo lê o mouse por frame, mas só constrói um objeto que nenhum código de UI consulta —
não há hit-testing de mouse para alimentar. Publicar o anel continua correto e barato, mas não move
nenhum menu.

Cadeia de init e de frame (confirmada por desmontagem):

```
loop de frame 0x3e4028 → bl 0x3e4120 → bl 0x577000 → bl 0x9acd50
  0x9ac1d0 (update de input)   bl 0x9ac300  → 0x9af990 (Npad) e 0x9ad9f0 (touch)
                               0x9ac980     (teclado)
                               0x9acad0     (wrapper do mouse: lê GetMouseState e preenche 0xdc2540)
0x9ac300 lê o touch duas vezes: 0x9ad9f0(+0x18 e +0x3d8), cada uma chamando
0x9adb1c (GetTouchScreenState<16>) após 0x9ad890 (GetNpadStyleSet + estado do pad)
```

O gate `[0xdcc434]` de `0x577000` é o modo single-thread do loop, **não** estado de UI. Nenhuma
flag do jogo reflete menu: o `main` importa `nn::oe::GetCurrentFocusState` (foco da aplicação), não
um estado de interface.

**Consequência de projeto:** o anel de mouse em memória compartilhada continua sendo publicado
(é o ABI correto e não custa nada), mas nenhum menu pode ser alimentado por ele.

### 4.2 Layout do anel do SDK (válido e já compatível)

- `RingLifo<TouchScreenState<16>>`: `ReadImpl` em `sdk+0x233f70`, stride `0x1a0`, seq em
  `entry+0x20`, estado em `entry+0x28`, `tail` em `+0x10`, `count` em `+0x18`.
- Anel de mouse: global `0xc01648`, stride `0x30`, seq em `entry+0x20`, `MouseState` em
  `entry+0x28`.
- Offsets padrão do port (`hid_service.h`): touch `0x400`, mouse `0x3400`, keyboard `0x3800`,
  npad `0x9a00`; `_Static_assert` de `HidTouchSample==0x290`, `HidTouchRing==0x2c38`,
  `MouseRing==0x350` batem com o SDK.

### 4.3 Lição de instrumentação

Três armadilhas invalidaram medições desta investigação; ficam registradas para não repetirem:

1. **Capstone para na primeira instrução inválida.** Sem `md.skipdata=True` o scan decodifica só
   um fragmento do `.text`, e toda contagem de call site sai errada (foi o que produziu o
   falso-negativo "o mouse não é lido por frame"). Com `skipdata` o scan cobre 2.636.276 instruções.
2. **`printf` de stdout na thread hóspede é descartado por buffering.** Probes de runtime precisam
   escrever em arquivo (`fopen`/`fprintf`/`fclose` em `/tmp/*.txt`).
3. **Frames não são determinísticos entre execuções.** O mesmo `AC_INPUT_SCRIPT` leva a telas
   diferentes em runs diferentes, então comparar um frame de uma run com um frame de outra não
   prova nada. Use capturas **antes e depois na mesma execução** (`LARECOMP_SHOT_AT=a,b`).

### 4.4 Nenhum menu testado faz hit-test do painel

Um contato de touch publicado no anel chega ao jogo: observando a máquina de estados própria em
`ctx+0x24` (`ctx = *(0x7000DD2520)`), um contato produz `0 → 2 → 3 → 1 → 0`
(idle → began → held → released). O ABI do anel também confere com o SDK (§4.2) e a posição
publicada foi conferida no anel (`374,372` pontos de janela → `299,297` no painel 1280×720).

Mesmo assim, o contato **não seleciona o item sob o dedo**:

| Tela | Modo | Contato publicado | Resultado |
|---|---|---|---|
| overlay de tutorial (`Space Go Back / E Next / Tab Close`) | docked | sobre `Tab Close` | nenhuma mudança |
| **LOAD DATA** (slots A e B lado a lado) | docked | sobre o slot A (não selecionado) | nenhuma mudança; B continua selecionado |
| Legatus Menu / MAP | docked | sobre o mapa | nenhuma mudança |
| **LOAD DATA** | **handheld** (`AC_HANDHELD=1`) | sobre o slot A | nenhuma mudança; B continua selecionado |

A linha em handheld fecha o furo mais óbvio: no Switch o painel só existe fora do dock, então era
possível que o jogo descartasse o touch por estar em modo docked. Não é o caso —
`GetTouchScreenState<16>` (`0x9adb1c`) é chamado **incondicionalmente** dentro de `0x9ad9f0`, sem
consulta a `GetOperationMode`, e o contato não move a seleção nem onde o painel existe de verdade.

O que **funciona**, medido do mesmo jeito (antes/depois na mesma execução): um único pulso de D-pad
injetado pelo ponteiro, com os 0.06 s de `AC_MOUSE_MENU_PULSE_TICKS`, move LOAD DATA de B para A.
Segurar por 3 s também move. Pulsos repetidos parecem "não fazer nada" em um menu de 2 itens porque
a seleção vai e volta — uma leitura ingênua desse frame sugere falsamente que a injeção falhou.

**Conclusão:** o ponteiro dirige o pad, e é o pad que os menus obedecem. Seleção posicional
absoluta não é implementável sem que o jogo exponha geometria de item ou o índice selecionado.

### 4.6 Por que o hook de render não resolve

A alternativa investigada foi extrair a geometria dos itens do caminho de desenho. O resultado da
medição é negativo, por três motivos independentes:

| Evidência | Medição |
|---|---|
| Passes de draw do frame do LOAD DATA (`LARECOMP_DEBUG_FRAME=2400`) | 212 draws com viewport `1600×900` inteira, os demais em `1280×720`, `800×450`, `400×225`… — **nenhum** com viewport/scissor por item |
| Conteúdo dos vertex buffers (`LARECOMP_DEBUG_DATA=1`) | posições em espaço **local** (`0 0 0 1`, `±1`, `0 0 1 0`): são quads unitários e cubos |
| Uniforms (`vcb3`/`vcb4`) | matrizes de projeção/view e cor; a posição final sai da multiplicação no vertex shader |

Ou seja: o retângulo na tela é o resultado de um quad unitário transformado pelo vertex shader. Para
obter o retângulo seria preciso **emular os shaders** ou rasterizar e detectar regiões — e mesmo
assim faltaria o dado essencial, que não é geométrico: **qual item está selecionado**. O jogo não
expõe índice de seleção; sem ele não há como saber quantos passos de D-pad dar desde a posição atual.

Fechar o loop exigiria ainda inferir a seleção a partir da aparência do highlight (cor/borda), o que
acopla o ponteiro ao estilo visual de cada tela — exatamente o tipo de heurística que produz
"funciona aqui, não ali". Pelo princípio de menor alteração completa, o caminho foi descartado em
favor do D-pad relativo, que usa o canal que os menus comprovadamente obedecem.

### 4.5 O bloqueio real era o foco da janela

Nenhum log de runtime mostrava `WINDOW focus`, e `g_sdl_input_focus` nunca virava 1. Como
`extra_mouse_menu_apply` é chamado dentro do bloco `if(connected && focused)` de `hid_publish`,
**toda** a injeção de menu (passos de D-pad, taps A/B, contato de touch) ficava silenciosamente
desligada — inclusive em runs sem interação do usuário. A correção é uma linha em
`extra/sdl/window_trampoline.h`, logo após `SDL_RaiseWindow`: `g_sdl_input_focus = 1`, com o
evento de foco do SDL tratando de baixá-lo quando a janela perde o foco.

---

## 5. Correções aplicadas

| # | Item |
|---|---|
| 1 | F10/F11 com `!e.key.repeat` (segurar não alterna) e F10 retirado do `KEY_UP` para não vazar tecla |
| 2 | Uma única fonte de verdade: `mouse.h` consulta `extra_mouse_mode_is_captured()`; acumuladores zerados na transição |
| 3 | `extra_mouse_mode_set` com early-return; `SDL_WarpMouseInWindow` ao entrar no modo livre; `SDL_HideCursor` ao capturar |
| 4 | Globais de módulo `static` (sem `weak` em header) |
| 5 | Touch só é publicado com gate (`hid_touch_active` ou contato do ponteiro) e a release é publicada; janela medida em **pontos** SDL, não 1600×900 fixo |
| 6 | Navegação por movimento **ligada por padrão**; o launcher controla `AC_MOUSE_MENUS` e `AC_MOUSE_MENU_CELL` |
| 7 | Detecção de menu explícita e documentada; sem timeout que rouba o cursor por padrão |
| 8 | Alt+Tab libera e restaura o cursor preservando o modo escolhido |
| 9 | O warp de entrada no modo menu não conta como movimento (`skip_motion`); sem isso o salto até o centro avançava a lista |
| 10 | Com `AC_MOUSE_MENUS=0` o clique esquerdo volta a tomar a câmera, como antes dessa entrega |
| 11 | Teste automatizado do ponteiro (`extra/tools/mouse_pointer_test.c`) roda dentro do `audit.sh` |
| 12 | Ledger e documentação alinhados ao código |

---

## 6. Não-regressão

1. Toda automação nasce atrás de flag; `AC_MOUSE_MENUS=0` e `AC_MOUSE_AUTO_MENU=0` devolvem o
   controle puramente manual (F10).
2. `AC_MOUSE_SENS=100` + `AC_MOUSE_CURVE=linear` mantém a fórmula anterior (só o piso muda).
3. Baseline: `./extra/tools/qa_runner.sh 20 550` + hash de `frames/frame_00550.png`.
4. Critério de aceite: frame 550 com hash igual com as funcionalidades desligadas e p50 de frame
   dentro de ±1 ms.
5. `extra/tools/mouse_pointer_test.c` cobre mapeamento de painel, passos, pulsos, clique/roda e a
   política de modo; roda em `./extra/tools/audit.sh` (etapa 4) e reprova a auditoria se a tradução
   do ponteiro quebrar. Inclui a regressão de §4.4: posicionar o cursor (ou saltá-lo) **não** pode
   gerar passo nenhum, para que uma "seleção posicional" inventada não volte.

---

## 7. Pendência de validação

O que falta, e por quê:

1. **Ponteiro real dirigindo um menu de ponta a ponta.** Não há `xdotool`/`ydotool` no ambiente e a
   sessão é Wayland, então não foi possível mover o ponteiro físico durante uma run. O caminho foi
   verificado em duas metades medidas: (a) um pulso de D-pad injetado por `extra_mouse_menu_apply`
   com os mesmos 0.06 s move LOAD DATA de B para A; (b) `extra_mouse_menu_on_motion` → fila →
   pulso é coberto pelo self-test. O que **não** foi visto é um movimento de mouse de verdade
   percorrendo esse caminho dentro do jogo.
2. **Seleção posicional absoluta.** Investigada e descartada com medição (§4.6). Retomar só se
   aparecer uma forma de ler o índice selecionado na memória do jogo — sem ele, nenhuma geometria
   extraída do render fecha o loop.
3. **Hit-test de painel em menus tardios.** Só foram exercitados o overlay de tutorial, LOAD DATA
   (docked e handheld) e Legatus/Mapa. Nada garante que uma tela posterior (IRIS, loja, treino) não
   faça hit-test do touch; com `AC_MOUSE_MENU_TOUCH=1` o contato já está disponível para essas telas.
4. **Clique como `A` em telas com foco implícito.** O código injeta o mesmo bit que o botão A, então
   o comportamento deve ser idêntico ao do controle; a confirmação final é visual.
