# Engenharia Reversa — UI, Estado, Performance e Pontos de Patch

**Status:** análise **estática** (somente leitura) do `game/update-exefs/main` (NSO v1.0.1,
`nsobid 4B159F0F7A360669738390EE8657B4F0`). Nada foi executado. Data: 2026-11-01.
Complementa (não substitui) [`docs/reverse-engineering-pc.md`](reverse-engineering-pc.md).

> **Convenção de endereços.** Todos os endereços são **file offsets no NSO**
> (`X − 0x7000010000` em runtime, com o port usando base `0x7000010000`). Segmentos do `main`:
> `.text [0x0, 0xa0e7d0)`, `.rodata [0xa0f000, +0x325848)`, `.data [0xd35000, +0x8f870)`,
> `.bss` em `0xdc5000` (+`0xbf21790`). Ferramentas: `astral_chain/nso.py` + Capstone 5.0.7;
> scanners em `/tmp/opencode/re_scan.py` etc. (não versionados — ver §9 como regenerar).

---

## 1. Como o jogo nomeia as coisas (o que as strings dizem)

6.759 strings ASCII ≥5 em rodata+data. Não há arquivos de layout de UI (`.nui`/`.bflyt`/`.bflan`)
referenciados pelo `main` — a UI do jogo é código + texto (chaves `MENU_*`/`CORE_*`/`core_GUI_*`)
mais recursos empacotados. As famílias de strings relevantes:

| Família | Exemplos | Leitura |
|---|---|---|
| Efeitos sonoros de UI | `core_GUI_Menu_Cusor` (298 xrefs), `core_GUI_Menu_Decide` (175), `core_GUI_Menu_Cancel` (130) | Os eventos de "mover cursor", "confirmar", "cancelar" são tocados como SE — cada xref é um lugar onde um menu está interativo. |
| Estados/posição | `Index`, `Offset`, `OffsetY`, `ExecSubPhaseIndex`, `AreaIndex`, `State_FinishSlowOff`, `MenuCursorItemKind` | Nomes de campos/propriedades serializadas (BXM/save), lidos via reflexão — não flags globais. |
| Entrada | `UndLegPadA` (19 xrefs), `UndLegPadB` (12) | Legendas de botões por idioma; usados pelo mapeador de input. |
| Config/opções | `option` (28 xrefs), `Config`, `OPTION_ELEMENT_COL_%02d`, `Preset%d` | Ver §3 — o "menu de opções" é uma lista de propriedades data-driven. |
| Propriedades de efeito/render | `PostAdjustType`, `EffectRenderSetting`, `SettingParamIntArgs`, `EffectTextureSetting{Normal,Color,Mask}` | Ver §4/§5 — pipeline data-driven. |
| Timers/FPS | `HeaderFps` (13 xrefs), `MotionFrameRateType`, `DrawFrameRateType`, `ProgressFrameRateType` | **Não** são opção de FPS do usuário — são chaves de serialização. Ver §6. |

---

## 2. UI: estado, "o que está aberto", posição e seleção

### 2.1 Não existe uma flag global "menu aberto"

Confirmado por scan de xrefs: nenhuma string `isOpen`/`menuOpen`/similar tem consumidor em `.bss`.
O estado de UI vive **dentro de objetos** (instâncias), acessados via vtables, não em globais
endereçáveis. Consequência para o port: o overlay/menu do PC (Insert) deve manter **seu próprio
estado** (já é o caso em `extra/ui/overlay/`) e não tentar "espelhar" uma flag do jogo.

### 2.2 Onde vivem os objetos de UI

Os menus são objetos C++ com vtable. Os construtores/registradores relevantes:

| Função | Papel | Evidência |
|---|---|---|
| `0x625e60` | Manipulador do cursor de menu (`core_GUI_Menu_Cusor`) | lê `w8 = [x0+0x8C]` (campo **kind** do item do cursor), subtrai 1, e despacha por jump-table em `0xbe9190` com 8 casos (`0x629d90`, `0x62a0e0`, `0x62a580`, …). |
| `0x61bed8` | Controlador principal de menu (consome `core_GUI_Menu_Cusor`) | puxa ponteiros de uma tabela `.data` em `0xdb8010..0xdb8268` (inicializados com o **thunk PLT** `0xa0d480` — `adrp x16,#0xdb7000; ldr x17,[x16,#0x8c8]; br x17`, ou seja, "handler padrão = import do SDK") e referencia chaves `MENU_*` (`MENU_SELECT_CASE_DIALOG_02`, `MENU_SAVE_DIALOG_23`, `MENU_SKILL_LEARN_DIALOG_04`, …). |
| `0x62c4c0` | Menu de **opções** | referencia `option` e tabela `.data` `0xdb8010..0xdb8460`; strings `CORE_ITEM_NAME_%04x`, `core_item_%04x`, `MENU_OPTION_DIALOG_01/02`, `OPTION_ELEMENT_COL_%02d`. |
| `0x3fe260` | Construtor do `ShaderSettingManager` (ver §4) | registra `g_ShaderSettingManager` e as strings `Filter01..Filter07` + constantes `1280`×14, `720`×14. |

> **Ponto de seleção/posição do cursor.** O campo `[this+0x8C]` lido em `0x625e68` é o melhor
> candidato documentado de "o que está selecionado" dentro de um menu: é ele que decide o som
> (`core_GUI_Menu_Cusor` vs variantes) e o destino do dispatch. Offsets `+0x00`, `+0x04` do
> mesmo objeto aparecem como posição/flags na função pai (cursor lê `ldp`/`stp` em pares).

### 2.3 Consumo de input pela UI

- **Mouse:** [`docs/plan-mouse-menu.md`](plan-mouse-menu.md) §4 — o mouse **é lido por frame**
  (`GetMouseState` no callsite `0x9acafc`, dentro de `fn 0x9abc88`) e o resultado é copiado
  para um struct global apontado por **`[0xdc2588]`**. Verificação: o único `str` direto a
  esse ponteiro está em `fn 0x9c7700` (init do subsistema, que zera um objeto grande em
  `0x9c94b0..0x9c95xx` com offsets `0x470..0x7d8` — o mouse é um sub-struct dentro dele).
  Na leitura por frame (`fn 0x9abc88`): o `GetMouseState` preenche a pilha (`sp+8..`) e o
  código copia campos para o struct: **`[+0x04]=pos.x, +0x08=pos.y, +0x0c=rodas/botões altos,
  +0x10..+0x13=botões`** (`ldp w11,w9,[sp,#0x18]; stp w11,w9,[x14,#0xc]` etc. em `0x9acb1c`).
  **Nenhum menu faz hit-test** com esse struct (confirmado por ausência de leitores de
  `0xdc2588` fora do provedor).
- **Touch:** callsite `0x9adb1c`, mesmo padrão (struct global, sem consumidor de menu).
- **Botões analógicos "UndLegPadA/B":** mapeados em `fn 0x47e44` e `0x1c7f90`; são legenda,
  não estado.
- **Npad:** `GetNpadState` no callsite `0x9ae50c`, consumo dentro de `fn 0x9ae4e0`.
  O resultado alimenta o sistema de ação do jogo (não a UI diretamente).

---

## 3. Opções/Config — como o jogo guarda escolha do usuário

O menu de opções é **data-driven**: cada entrada é uma linha num arquivo serializado que
registra pares `(nome, valor)`. As funções de init que constroem essa tabela:

| Função | Registra | Observação |
|---|---|---|
| `0x8ad5f0` | `SettingParamIntArgs`, `ObjList.bxm`, `ObjSubInfo.bxm`, `AttachList.bxm`, `AttachDataNum`, `ObjId`, `SubNo`, `CreateType`, `ReleaseType`, `LaunchFlag`, `Fovy`, `ParamId`, `ValueBegin/ValueEnd`, `PostAdjustType`, `ProgressFrameRateType`, `MotionFrameRateType`, `DrawFrameRateType`, … | É o **registrador-mestre** de parâmetros serializáveis (BXM). ~89 strings-chave. |
| `0x870bc8` | `Config` | owner da string `Config` e da tabela `.data` `0xdbb010..0xdbb088`. |

**Padrão de acesso a opção** (visto em cada caso, ex. `ProgressFrameRateType` em `0x8b5f04`):

```
ldr  x8, [obj];  ldr x8, [x8, #0x138]   // vtable[+0x138] = get-by-name
adrp x2, <str>;  blr x8 -> índice
ldr  x9, [obj];  ldr x9, [x9, #0x170]   // vtable[+0x170] = set-by-index
blr  x9
```

Ou seja: opções são **(nome → índice → valor)** via reflexão, gravadas em BXM/save.
**Implicação prática para o port:** para expor uma opção "gráfica" nova basta escrever no
slot da propriedade (via a mesma tabela), não há um `if quality==HIGH` espalhado no código.

---

## 4. Gráficos — cadeia de pós-processamento e pontos de patch

### 4.1 A cadeia `Filter01..Filter09`

Todas as strings `Filter01..Filter09` (exceto 04/08, ausentes no binário) têm **2 xrefs cada,
ambos na função `0x3fe260`** (o construtor do ShaderSettingManager). Ou seja, o dispatch dos
filtros é **uma tabela estática** construída em boot, não um switch por frame. Isso facilita
patch: trocar/skippar um passo = editar a entrada da tabela (valor = ponteiro de função do
filtro) ou zerar o flag do passo correspondente.

### 4.2 O que cada família de shader cobre (strings)

| Tema | Strings-chave |
|---|---|
| Sombras | `ShadowCast_VS/PS_XOOXX` (43 xrefs na fn `0x3fe260`), `shadowreceiver_lv2/3/4`, `m_LightMapShadowPow`, `m_lightOptionFlag` |
| Depth/normal pre-pass | `ZMap_VS_*` (10 variantes, 3 xrefs cada) |
| Motion blur / velocity | `FilterShaderCameraVelocity`, `FilterShaderDirectionVelocityMap`, `modelshadervelocitymap`, `modelshadervelocitymapscr`, `shadowreceiver_*_velocity`, `not_motion_camera` |
| Bloom/tonemap/HDR | `filtershadertonemap`, `FilterShaderHDRComposite`, `hdr_range`, `FilterShaderZLinearConversion` |
| AA | `FilterSMAACopy`, `uiradialchromatic`, `FilterShaderGatherNoise` |
| Pós data-driven | `PostAdjustType`, `EffectRenderSetting`, chain `Filter01..09` |

### 4.3 Constantes de resolução (pendência §7.7 do doc anterior — resolvida)

| Constante | File offset | Função que usa | Significado |
|---|---|---|---|
| `1920` / `1080` (int, u32) | `0xd87e74` (struct: `1920,0,0,0, 0,1,-1,768`) | dentro de tabelas `.data` lidas por `fn 0x6f6e90` (12 usos de 1920) | **viewport/struct de resolução de boot** — 1920×1080 "docked". 768 = `0x300` = valor de um campo auxiliar. |
| `1600` / `1696` (u32) | `0xdb22f8` (struct: `1600,0,0,0, 0x4143CF41,96, 1696,0,…`) | tabela de escala interna | grade de resolução intermediária (provável handheld ↔ docked). |
| `1280` / `720` (int) | muitos | `fn 0x3fe260` (14× cada) — junto com `Filter01..09` | **tamanho dos buffers da cadeia de filtros** (pós-processamento 720p interno é o piso do jogo). |
| `720.0f` | `0xb5dc00` | `fn 0x774120` | único literal float 720 usado isolado. |

> **Patch de resolução.** O port já renderiza em 1080p (ver `extra/gpu/vtx_debug_extra.h` e
> janela). Os campos `1920/1080` em `0xd87e74` são a *fonte de verdade* do jogo sobre o
> backbuffer lógico; mudá-los **não** basta — a cadeia de filtros tem tamanho 1280/720
> embutido em `0x3fe260`. Um patch de internal-resolution precisa tocar os dois lugares.

### 4.4 MSAA / AA

`nvnMultisampleStateSetSamples`, `nvnTextureBuilderSetSamples` estão presentes
(resolvidos via slots `0xdc2bb8`, `0xdc2e80` — ver §5), mas **não há antialiasing geométrico
configurável** no jogo: o AA real é pós-processamento (`FilterSMAACopy` + `uiradialchromatic`).
O hook do port `AC_ANTIALIAS` em `extra/` é o caminho correto; não há "opçãoMSAA" do jogo para
espelhar.

---

## 5. Tabela de resolução NVN (nome → slot de função)

O NSO **só importa `nvnBootstrapLoader`** como símbolo dinâmico. Todas as outras ~512 funções
`nvn*` são **resolvidas por nome em boot** (padrão em `fn 0x9ee4f0..0x9ef8xx`: `adrp x1, str;
bl resolver; adrp x8, #0xdc2/0xdc3; ldr x8,[x8,#imm]; str x0,[x8]`). O slot final fica em
`.data/.bss` e é o que o código chama. Slots medidos (úteis para hooks/telemetria):

| Função NVN | string @ | slot (ptr em memória) |
|---|---|---|
| `nvnTextureBuilderSetSamples` | `0xb6834e` | `0xdc2e80` |
| `nvnMultisampleStateSetSamples` | `0xb683bc` | `0xdc2bb8` |
| `nvnSamplerBuilderSetLodBias` | `0xb83a5e` | `0xdc3530` |
| `nvnSamplerBuilderSetLodSnap` | `0xb6a917` | `0xdc3558` |
| `nvnSamplerBuilderSetLodClamp` | `0xb735e1` | `0xdc3530` |
| `nvnSamplerBuilderSetMaxAnisotropy` | `0xb6bb7f` | `0xdc3550` |
| `nvnWindowSetPresentInterval` | `0xb7b4cb` | `0xdc3278` |
| `nvnWindowBuilderSetPresentInterval` | `0xb7a144` | `0xdc2ec0` |
| `nvnWindowGetPresentInterval` | `0xb809b3` | `0xdc3270` |
| `nvnWindowBuilderSetNumActiveTextures` | `0xb6ee00` | `0xdc2ec8` |
| `nvnDeviceBuilderSetFlags` | `0xb79138` | `0xdc3070` |

> O mapa completo (512 entradas) está em `/tmp/opencode/re/nvn_slots.json`; regenerável pelo
> script do §9. `nvnWindowGetPresentInterval` existe — o jogo lê o intervalo configurado,
> mas o port ignora/vence via swapchain (ver `extra/ui/overlay/ac_vk_glue.cpp`).

---

## 6. Performance — o que o Dynamic-FPS realmente toca

O patch `third_party/AstralChain-Dynamic-FPS/1.0.1.pchtxt` (v1.1.1, "Dynamic-60fps") já
resolve o pacing; este documento **explica os endereços** para patches próprios do port.

### 6.1 O struct central do pacing

A página `0xdb8000` contém o objeto global de pacing. O patch referencia:

| Endereço | Offset no objeto | Conteúdo |
|---|---|---|
| `0xdb89a0` | `+0x00` | ponteiro do objeto (carregado por `ldr x8,[x16,#0x9a0]`) |
| `0xdb89a0+0xD0` | `+0xD0` | **frame step** (float) — o valor que o `FCVTZS` original truncava |
| `0xdb89a0+0xD4` | `+0xD4` | remainder |
| `0xdb89a0+0xD8` | `+0xD8` | **whole ticks** (int) — frames inteiros de 30 FPS acumulados |
| `0xdb89a0+0xDC` | `+0xDC` | **frames per millisecond** (float) |
| `0xdb89a0+0xE4` | `+0xE4` | avg sample count |

### 6.2 Os dois pontos de override (60 FPS cap)

| Site | Antes | Depois (patch) | Efeito |
|---|---|---|---|
| `0x9E8D84` (init) | `csinc w1, w8, wzr, ne` (w1 = 1 ou 2) | `mov w1, #1` | força o modo de timestep adaptativo |
| `0x9DB6BC` (runtime) | `csinc w1, w9, wzr, ne` | `mov w1, #1` | idem, no loop por frame |

### 6.3 Consumidores do step (o "anti-truncamento")

O ponto crítico: o código original aplicava `FCVTZS s→int` no step float, truncando tempos
sub-frame (daí timers/input quebrando em FPS alto). O patch substitui por leitura dos
campos inteiros do struct:

| Site | Patch | Significado |
|---|---|---|
| `0x48BE14` | `FCVTZS w8,s0` → `ldr w8,[x8,#0xD8]` | usa **whole ticks**; o `+0xD8` acumulado pelo producer |
| `0x57E7DC` | `ldr w8,[x24,#0xD8]` + `cbz w8,*+0x30` | idem, no segundo caminho de timers |
| `0xA0E7D0` (cave) | `ldr s1,[x19,#0xDC]; fmul s8,s0,s1; b 0xA0E7EC` | measured sample: step float = `frames_per_ms × dt` |

E uma cadeia de correções "r2/r3/r4" (clamps de tempo de animação/ação) nos endereços
`0x308A9C`, `0xE7F64`, `0xF7CE8`, `0xF8720`, `0xD5E28`, `0x2828AC`, `0x283100`, `0x28311C`,
`0x28759C..0x2875DC`, `0x283044` — todos no mesmo padrão (`subs`/`csel` substituídos por
saltos para uma cave que lê `[0xdb89a0]+0xD8`).

> **O que NÃO fazer:** o port já tem `extra/platform/pace_trampoline.h` e o overlay desliga
> vsync via swapchain. Re-aplicar o pchtxt **por cima** do binário já recompilado não é
> necessário — os endereços acima são a **referência do que foi traduzido**, não código a
> re-aplicar.

---

## 7. Âncoras para patches gráficos com opções

Pontos de entrada concretos (todos file offsets), prontos para hook no port:

| Objetivo | Gancho | Como |
|---|---|---|
| Desligar motion blur | tabela `Filter01..09` em `fn 0x3fe260` | anular a entrada do pass `FilterShaderCameraVelocity`/`DirectionVelocityMap` na tabela estática (2 xrefs cada). Alternativa: `LARECOMP_SKIP_FSHASH` no port após achar o hash do pass. |
| Forçar internal-res | campos `1920/1080` em `0xd87e74` + tamanhos 1280/720 na fn `0x3fe260` | reescrever ambos; ver §4.3. |
| LOD bias | slot `nvnSamplerBuilderSetLodBias` (`0xdc3530`) | o jogo já chama; hookar o chamador no port (`extra/`) em vez do slot — caminho já usado por `LARECOMP_LOD_MIN`. |
| Present interval | slots `nvnWindowBuilderSetPresentInterval` (`0xdc2ec0`) / `nvnWindowSetPresentInterval` (`0xdc3278`) | **não** tocar: o port já governa vsync via `vkDeviceWaitIdle`+recreate da swapchain; o jogo lê (`GetPresentInterval`) mas quem manda é o port. |
| Opções do usuário | tabela de `SettingParamIntArgs` em `fn 0x8ad5f0` | escrever via o mesmo caminho `(nome→índice→valor)`; ver §3. |

---

## 8. O que falta (pendências declaradas)

1. **Mapa `EffectRenderSetting`:** o ponteiro de reloc em `0xdb23a8` lê `0x0` (campo zerado no
   snapshot). Precisa de uma passada dinâmica (runtime) para ver o alvo real — **fora do
   escopo desta RE estática**.
2. **Hash do pass de motion blur:** o esquema `LARECOMP_SKIP_FSHASH` do port já existe; o
   hash exato do pass depende de rodar o FS-hash em runtime — pendente, não bloqueia o patch
   pela tabela (§7, linha 1).
3. **Hit-test de mouse nos menus:** confirmado ausente (§2.3). Implementar é trabalho de
   `extra/`, não de RE.

---

## 9. Como regenerar esta evidência

```bash
# scanners (ficam fora do repo, em /tmp/opencode)
.venv/bin/python /tmp/opencode/re_scan.py    # strings / xrefs / bl_sites / funcs -> /tmp/opencode/re/
.venv/bin/python /tmp/opencode/re_ui.py      # ranking por categoria -> re/ui_strings.json
.venv/bin/python /tmp/opencode/re_deep*.py   # análises dirigidas (UI, pacing, NVN, resolução)
```

Artefatos: `strings.json` (6.759), `xrefs_rw.json` (22.605), `bl_sites.json` (15.398),
`funcs.json` (616), `nvn_slots.json` (512). O `nsobid` do alvo é
`4B159F0F7A360669738390EE8657B4F0`.
