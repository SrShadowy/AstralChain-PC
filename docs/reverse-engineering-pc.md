# Engenharia Reversa do Binário para Recursos Nativos de PC

**Status:** documentação de análise estática (somente leitura), 2026-11-01. Nenhuma alteração de
código derivada desta RE foi aplicada; este arquivo é a fonte de referência para decisões futuras de
port. Acompanha `AGENTS.md` (análise baseada em dados do binário, sem speculation).

> **Escopo.** A meta é levantar dados concretos no binário que suportem recursos mais nativos de PC:
> localização/idioma, teclado de texto (`nn::swkbd`), configuração/estado, gráficos, saves e mouse.
> O método é **análise estática** dos NSO ARM64 (`game/update-exefs/{main,sdk,rtld}`), usando o parser
> `astral_chain/nso.py` + Capstone 5.0.7. Nada foi executado em runtime.

---

## 0. Como ler este documento

Toda evidência cita um endereço real no `main` (base `0x7000010000`) e, quando relevante, o PLT do
import de `nn::*` correspondente. Os segmentos de `main` são:

| Segmento | Intervalo |
|---|---|
| `.text`   | `[0x0, 0xa0e7d0)` |
| `.rodata` | `[0xa0f000, +0x325848)` |
| `.data`   | `[0xd35000, +0x8f870)` |
| BSS       | `[0xdc5000, …)` |

- **Import → call site.** Um import (`R_AARCH64_JUMP_SLOT`, reloc kind 1026) passa por uma stub PLT
  (padrão `ADRP/LDR/ADD/BR`) e é alcançado por `BL` nos call sites. O mapa completo dos call sites
  do `main` foi salvo em `/tmp/opencode/main_callsites.json` (símbolo → lista de call-site addresses).
- **Region de globais de jogo** em `0xdc2000` (referida como "BSS de estado"). Vários deslocamentos
  abaixo se apoiam nela (`0xdc2000 + off`).

Os tópicos abaixo são independentes; cada seção é autocontida.

---

## 1. Sistema de Idioma e Localização

### 1.1 Cadeia de chamadas

| Peça | Endereço | Evidência |
|---|---|---|
| Função de seleção de idioma | `main+0x9c2870` | chama `GetDesiredLanguage` |
| Call site `GetDesiredLanguage` | `main+0x9c2884` | PLT `0xa0e3d0` |
| `nn::settings::LanguageCode::Make` | PLT `0xa0d9a0` | call sites `0x56fdd8` e `0x9c2890..0x9c2a74` (16) |
| `operator==` (LanguageCode) | PLT `0xa0e3e0` | call sites `0x9c28a0..0x9c2a84` |
| Getter do índice interno | `main+0x9c2e70` | lê `[0xdc2000 + 0x6f8]` |
| Global do idioma desejado | `0xdc2750` | `= 0xdc2000 + 0x750` |

A função em `0x9c2870` consulta `nn::oe::GetDesiredLanguage` e, com `LanguageCode::Make` +
`operator==`, converte o enum `nn::settings::Language` num **índice interno 0..14** (15 idiomas).
O `LanguageCode` desejado é gravado em `0xdc2750`; o getter em `0x9c2e70` retorna o índice interno a
partir de `0xdc2000 + 0x6f8`.

### 1.2 Tabela enum → índice interno

O enum `nn::settings::Language` do SDK é mapeado para o índice interno do jogo assim:

| Enum (`nn::settings::Language`) | Nome SDK | Índice interno | Idioma |
|---|---|---|---|
| 0  | `Ja`     | 0  | Japanese |
| 1  | `EnUS`   | 1  | English (US) |
| 12 | `EnGB`   | 2  | English (GB) |
| 3  | `De`     | 3  | German |
| 2  | `Fr`     | 4  | French |
| 5  | `Es`     | 5  | Spanish |
| 4  | `It`     | 6  | Italian |
| 9  | `Pt`     | 7  | Portuguese |
| 8  | `Nl`     | 8  | Dutch |
| 10 | `Ru`     | 9  | Russian |
| 13 | `FrCA`   | 10 | French (Canada) |
| 14 | `Es419`  | 11 | Spanish (LatAm) |
| 15 | `ZhHans` | 12 | Chinese (Simplified) |
| 16 | `ZhHant` | 13 | Chinese (Traditional) |
| 7  | `Ko`     | 14 | Korean |

**Observações**

- A numeração do enum bate exatamente com o `nn::settings::Language` padrão do Switch (o que valida
  a decodificação: `0=Ja,1=EnUS,2=Fr,3=De,4=It,5=Es,7=Ko,8=Nl,9=Pt,10=Ru,12=EnGB,13=FrCA,14=Es419,
  15=ZhHans,16=ZhHant`). O jogo ignora `6` (que no SDK é `ZhCN`/legado) e ordena por índice interno.
- O índice interno 0..14 provavelmente indexa os diretórios de idioma do RomFS (ver §6).
- Os índices 12–14 (zhHans, zhHant, ko) são os "CJK" e recebem tratamento distinto no teclado (§2).

### 1.3 O que isto habilita

- O idioma é derivado de `GetDesiredLanguage` (perfil do usuário). Para um port de PC, o mesmo canal
  pode ser alimentado por uma preferência do launcher sem tocar no layout do jogo: basta gravar o
  `LanguageCode` desejado em `0xdc2750` (ou o índice em `0xdc2000+0x6f8`) antes de `0x9c2870` rodar.
- `// shortcut:` não há ainda confirmação de que `0x9c2870` roda uma única vez no boot; se rodar a
  cada frame, o override deve ser persistente e idempotente.

---

## 2. Teclado de Texto (`nn::swkbd`)

### 2.1 Wrapper de teclado

O `main` importa o namespace `nn::swkbd` e o usa através de um wrapper em `0x9ab540`, que encadeia:

`InitializeKeyboardConfig → MakePreset → GetRequiredWorkBufferSize →
 GetRequiredStringBufferSize → SetInitialText → ShowKeyboard`

| Peça | Endereço |
|---|---|
| Wrapper | `main+0x9ab540` |
| `ShowKeyboard` | `main+0x9ab670` |

### 2.2 Tabela de configuração dependente de idioma

Em `0xc1b0a0` há uma tabela de **15 entradas** (uma por índice interno de §1.2). Cada entrada define
o preset do teclado conforme o idioma: índice `0` para idiomas ocidentais (alfabético) e `8` para os
índices CJK 12–14. Isto confirma que a experiência de digitação é sensível ao idioma escolhido em §1.

### 2.3 O que isto habilita

- Todo campo de texto do jogo (nome do save, etc.) já passa por este wrapper. Para um port de PC, a
  entrada de texto pode ser interceptada no wrapper: substituir `ShowKeyboard` por um campo de texto
  nativo (SDL/Qt) e devolver a string via `SetInitialText`/buffer de resultado é o ponto de hook
  único, sem alterar os chamadores.
- A tabela em `0xc1b0a0` é o ponto natural para mapear idioma → layout de teclado físico (por ex.
  `enUS` vs `fr` vs CJK), se o port quiser respeitar a localização do host.

---

## 3. Estado e Configuração do Sistema

O boot inicializa um flags word a partir de três consultas a `nn::oe`:

| Consulta | PLT | Significado |
|---|---|---|
| `GetCurrentFocusState` | `0xa0e3f0` | foco da aplicação |
| `GetOperationMode`     | `0xa0e400` | bit 29 = handheld (vs docked) |
| `GetPerformanceMode`   | `0xa0e410` | modo de performance da CPU/GPU |

Os três resultados são combinados e gravados num word de flags apontado por `[x19]` na função de
init. `SetPerformanceConfiguration` (PLT `0xa0e3c0`) é o import que aplica a configuração de
performance escolhida.

### 3.1 O que isto habilita

- **Operation mode** (handheld/docked) é a chave que o jogo usa para decidir resolução interna e a
  existência do painel touch. Para o port, forçar docked (ou expor handheld como opção) passa por
  `GetOperationMode`.
- **Performance mode** (Boost mode do Switch) e `SetPerformanceConfiguration` são os pontos onde um
  "perfil gráfico" de PC poderia se ancorar: o port já substitui `nn::oe` via HLE; ajustar o valor
  retornado por `GetPerformanceMode` ou o que `SetPerformanceConfiguration` recebe é o canal para
  opções gráficas de CPU/GPU no launcher.

---

## 4. Sistema de Fontes

### 4.1 Inicialização de fonte

| Peça | Endereço |
|---|---|
| Função de init de fonte | `main+0x56ef80` |
| `GetSharedFont` (call site) | `main+0x56fdec` |

`0x56ef80` inicializa o `ScalableFontEngine` e usa os serviços `nn::pl` (fontes compartilhadas) e
`nn::fontll`:

`ScalableFontEngine::Initialize → pl GetSharedFontSize/GetSharedFontAddress →
 LoadFont → SetFont → SetScale(0x10) → GetFontMetrics`

O fator de escala `0x10` (16) é aplicado às fontes compartilhadas do sistema.

### 4.2 O que isto habilita

- O port já substitui a renderização de fonte de UI por FreeType (`extra/ui/font_trampoline.h`,
  ver `docs/architecture.md` §5.C). Os símbolos `nn::pl`/`nn::fontll` são os serviços que fornecem o
  atlas de glifos do sistema; interceptá-los é o caminho para oferecer fontes arbitrárias de PC.
- `GetFontMetrics`/`SetScale` definem as métricas de layout; para suportar fontes maiores/DPI alto no
  PC, o ponto de escala é `SetScale(0x10)`.

---

## 5. Entrada: Mouse, Teclado, Touch e Pad

Esta seção é um índice compacto; a análise profunda está em `docs/plan-mouse-menu.md` §4 e
`docs/architecture.md` §5.D.

| API | Call site (`main`) | PLT |
|---|---|---|
| `GetMouseState`          | `0x9acafc` | `0xa0e050` |
| `GetKeyboardState`       | `0x9ac9cc` | — |
| `GetTouchScreenState<16>`| `0x9adb1c` | — |
| `GetNpadState`           | `0x9ae50c` | — |
| Init do mouse            | `0x9acd24` | — |

**Conclusão já medida:** o mouse é lido por frame (`0x9acafc`), mas o objeto resultante (`0xdc2540`)
não é consumido por nenhum código de UI — não existe hit-testing de mouse. Nenhuma flag do jogo
expõe "menu aberto". Ver `docs/plan-mouse-menu.md` para a decisão de projeto decorrente (ponteiro →
D-pad relativo).

---

## 6. RomFS: Diretórios de Idioma e Conteúdo

A árvore do RomFS (`game/romfs.bin`, ~10 GB) expõe 104 diretórios. Diretórios relevantes por tópico:

- **Idioma:** `English(US)`, `Japanese` (e os demais 15 idiomas de §1.2), alinhados com o índice
  interno do jogo.
- **UI/texto:** `Text`, `ui`, `font`.
- **Mídia:** `movie`, `bgm`, `stream`.

> **Estado da análise.** O parser de entradas de diretório do RomFS produziu offsets de nome
> inválidos (layout de struct incorreto); somente a extração do string-pool funcionou. Os nomes
> acima vêm do string-pool e são confiáveis, mas o mapeamento diretório→arquivo por idioma ainda
> precisa do layout correto antes de se listar, por exemplo, a árvore de localização completa.

### 6.1 O que isto habilita

- Confirmar a correspondência 1:1 entre o índice interno (15) e os diretórios `English(US)`,
  `Japanese`, etc. permite um seletor de idioma no launcher que apenas aponta o jogo para o índice
  correto (§1.2), sem reempacotar o RomFS.

---

## 7. Opções Gráficas e Pipeline de Render

### 7.1 Conclusão central (resposta à pergunta "quais opções gráficas existem?")

**O jogo não tem menu de qualidade gráfica.** Não existe, em nenhuma tela, seletor de qualidade de
textura, LoD, motion blur, sombra ou anti-aliasing. Isso é típico de título de Switch: todos esses
recursos estão **fixos** no pipeline de render, sem plumbing de configuração voltada ao usuário.

O que existe são duas coisas distintas, que convém não confundir:

1. **Recursos gráficos implementados internamente** (AA, DOF, bloom, tonemap, motion blur, sombras,
   LOD, MSAA), dirigidos por dados através de um gerenciador de settings de shader — tunáveis só em
   nível de código/dado, não por menu (§7.3).
2. **Opções de usuário reais** — áudio, idioma (texto e voz separados), câmera, legenda, vibração e
   cores — que são o que o menu de opções de fato expõe (§7.4).

### 7.2 Superfície de import gráfica

| Namespace | Papel | Relevância para PC |
|---|---|---|
| `nn::oe`          | idioma, foco, modo, performance | localização + perfil gráfico (§1, §3) |
| `nn::oe::SetPerformanceModeChangedNotificationEnabled` | notificação de mudança de modo | o jogo **reage** a mudança de performance |
| `nn::settings::LanguageCode` | enum de idioma | localização (§1) |
| `nn::pl` / `nn::fontll` | fontes | renderização de texto (§4) |
| `nn::hid`         | entrada (pad/touch/mouse/teclado) | mouse/menus (§5) |
| `nn::swkbd`       | teclado de texto | entrada de texto nativa (§2) |
| `nn::fs`          | saves/config | persistência de configuração |
| `nn::vi` / `nv` / `nvnBootstrapLoader` | exibição e GPU | resolução/tela cheia |

### 7.3 Recursos gráficos internos (fixos, mas tunáveis por código/dado)

O binário expõe a cadeia de pós-processamento e os recursos por nome de shader/parâmetro. Nenhum é
um toggle de menu, mas todos são **parâmetros reais** que um menu de PC poderia vir a controlar:

| Recurso | Evidência no binário |
|---|---|
| **Gerenciador de settings de shader** | `g_ShaderSettingManager` (com `--- … startup ---`), `FrustumParam`, `pre_GradingLUT` (color grading LUT), `zmap startup` |
| **Anti-aliasing** | `FilterSMAAEdge/Blend/Copy`, `FilterShaderMLAA`, `FilterShaderDLAA2`, `FilterArea` |
| **DOF / bokeh** | `FilterDof`, `FilterDofBlur`, `FilterDofReady`, `EffectBoke`, `cDofWork` |
| **Bloom** | `FilterBloomBlur_VS/PS`, `hexagonalblur2/3`, `filtershadersssssblurv/h`, `m_BloomThreshold`, `m_BloomThresBias` |
| **Tonemap / HDR** | `filtershadertonemap`, `filtershadertonemapcapture/debug`, `FilterShaderHDRComposite`, `hdr_range` |
| **Motion blur / velocity** | `FilterShaderCameraVelocity`, `FilterShaderDirectionVelocityMap`, `modelshadervelocitymap`, `shadowreceiver_*_velocity`, `not_motion_camera` |
| **Aberração cromática / grain** | `uiradialchromatic`, `FilterShaderGatherNoise` |
| **Sombras** | `ShadowCast_VS/PS_*` (muitas variantes), `shadowreceiver_lv2/3/4`, `m_LightMapShadowPow`, `m_LightMapShadowPow2`, `m_lightOptionFlag` |
| **LOD** | `LOD0/1/2`, `bg3400_base_LOD0`, `UseLostLOD`, `nvnSamplerBuilder{Get,Set}LodSnap/LodBias/LodClamp` |
| **MSAA** | `nvnTextureBuilderSetSamples`, `nvnMultisampleStateSetCoverageToColorOutput/Enable` |
| **Pós-processamento data-driven** | `PostAdjustType`, `SettingParamIntArgs`, `EffectRenderSetting`, `EffectTextureSetting{Normal,Color,Mask}`, cadeia `Filter01..Filter09` |

> **Cuidado com falsos positivos.** Os nomes `MotionFrameRateType`, `DrawFrameRateType` e
> `ProgressFrameRateType` parecem "opção de FPS", mas na verdade são **nomes de propriedade do
> sistema de parâmetros de efeito/animação** (vizinhos de `MotionAct`, `MotionSpeed`, `PosObjId`,
> `CustomParamIntArg1`, `ValueEnd`, `IsLoop`), não um seletor de 30/60 fps de usuário. A taxa de
> quadros real é governada pelo patch Dynamic-FPS (ver `docs/architecture.md` §2.B), não por um menu.

### 7.4 Opções de usuário reais (o que o menu de opções de fato expõe)

Levantadas por nome de string/chave e por import:

| Categoria | Evidência |
|---|---|
| **Áudio — volumes** | `Volume_Master`, `Volume_BGM`, `Volume_SE`, `Volume_Voice`, `Volume_Environment` |
| **Áudio — dispositivo** | `AudioBuiltInSpeakerOutput`, `AudioUsbDeviceOutput`, `AudioStereoJackOutput`, `ExistSpeaker`, `nn::audio::ListAudioDeviceName` |
| **Idioma de texto** | `system_language` (+ §1) |
| **Idioma de voz** (separado!) | `voice_language`, `system_voice_language` |
| **Câmera** | `camera_thumb_01/02/03`, `core_GUI_Camera_{Move,Zoom,Decide,Shutter,Mode_In/Out,Album,Cursor}` |
| **Legenda** | `SubtitleManager`, `SubtitleInfo_%04x.csv`, `SubtitleInfo_Common.csv` |
| **Vibração** | `nn::hid::VibrationPlayer{Load,Play,Stop}` |
| **Cores do Legion** | `LegionColorPreset.csv`, `LegionColorSet.csv`, `PlColor{Skin,Eye,Hair,Equip}.csv`, `CharacterCustomizeColorSet.csv`, `PresetColor.csv` |
| **Flags de sistema** | `system_play_mode`, `system_share_play`, `system_event_skip`, `system_initial_start` |
| **Menu de opções** | `Option_text`, `Config`, `option` |

### 7.5 Implicação para um menu gráfico de PC

- **O jogo não expõe** "qualidade de textura / LoD / motion blur / sombra / AA" em menu; mas **o port
  já tem a camada de controle externo** — a tradução NVN→Vulkan (`astral_chain/runtime/las/nvn/`) e o
  `g_settings` (`settings.h`) com flags `AC_*`/`LARECOMP_*`. É nela que esses controles entram, sem
  tocar no código ARM64 do jogo. Ver §7.6.
- **Âncoras no jogo** (para quem quiser patchear o próprio `main`): `GetPerformanceMode` /
  `SetPerformanceConfiguration` (§3), o `g_ShaderSettingManager`, e os parâmetros `m_BloomThreshold`,
  `m_LightMapShadowPow`, `m_lightOptionFlag`, o número de samples MSAA e o LodBias/LodSnap dos
  samplers.
- **O que já é configurável de verdade** no jogo (áudio, idioma texto/voz, câmera, legenda, vibração,
  cores) pode ser exposto no launcher **reaproveitando** a persistência `nn::fs` (§7.7), sem tocar
  no pipeline de render.

### 7.6 Edição externa: textura, LoD e motion blur — onde fica o hook

As três edições pedidas ("texturas mais leves", "LoD", "motion blur") não vêm do menu do jogo, mas o
port já intercepta exatamente as chamadas NVN que as governam. Mapa concreto (arquivo: linha):

| Alvo | Hook no port | O que faz hoje |
|---|---|---|
| **Texturas mais leves** | `nvn/vulkan/textures.c:288` (`levels = t->levels`) | nº de mips da textura; capar aqui = menos mips |
| | `nvn/vulkan/textures.c:1285` (`si.mipLodBias = d->lod_bias`) | bias de LOD aplicado a todo sampler |
| | `nvn/objects.c:386` (`nvnTextureBuilderSetLevels`) | onde o jogo fixa o nº de mips |
| | `nvn/vulkan/textures.c:301+` (decode ASTC/BC7) | decodificar a meia resolução = leitura pela metade |
| **LoD (nível)** | `nvn/vulkan/textures.c:1281-1282` (`si.minLod`/`si.maxLod`) | faixa de LOD de cada sampler |
| | `nvn/objects.c:589` (`nvnSamplerBuilderSetLodClamp`) | jogo seta `lod_min`/`lod_max` |
| | `nvn/objects.c:590` (`nvnSamplerBuilderSetLodBias`) | jogo seta `lod_bias` |
| | `nvn/objects.c:597` (`nvnSamplerBuilderSetLodSnap`) | **no-op** no port (ponto para implementar snap) |
| **Motion blur** | *game-side* (filtro de velocidade) | ver abaixo |

**Controles externos que já existem no port** (estendíveis, sem mudar o jogo):

| Variável | Arquivo | Efeito |
|---|---|---|
| `LARECOMP_LOD_MIN` | `textures.c:1288` | força `minLod` (empurra amostragem para mips menores → textura mais leve) |
| `LARECOMP_NO_SHADOW` | `textures.c:1295` | desativa comparação de sombra (`COMPARE_OP_ALWAYS`) |
| `LARECOMP_BC7` | `textures.c` / `bc7_gpu.c` | recompressão ASTC→BC7 (≈50 % VRAM) |
| `AC_ANTIALIAS` | `settings.h` + `present.c:212` | `off`/`smaa` |
| `AC_UPSCALER` | `settings.h` + `present.c:216` | `bilinear`/`fsr1` |
| `AC_HANDHELD` / `AC_WIDTH`/`AC_HEIGHT` | — | resolução/recorte |

**Motion blur é o único sem hook pronto.** O desfoque de movimento é um filtro do próprio jogo
(buffer de velocidade: `modelshadervelocitymap`, `FilterShaderCameraVelocity`,
`FilterShaderDirectionVelocityMap`, `not_motion_camera`, `shadowreceiver_*_velocity`), desenhado como
um pass fullscreen com shader do jogo — não passa por um parâmetro de sampler/textura. Dois caminhos:

1. **Pela camada do port (sem patchear o jogo):** identificar o fragment shader do pass de motion blur
   via `LARECOMP_DEBUG_FRAME`/`LARECOMP_WATCH_FS` e então pular o pass com
   `LARECOMP_SKIP_FSHASH=<hash>` (mecanismo já existente em `development.md` §7.B). É o caminho de
   menor alteração, mas exige a identificação visual do hash correto.
2. **Patchando o `main`:** achar o dispatch do pass de motion blur no código do jogo e curto-circuitar
   a chamada (âncoras: as strings de shader acima). Mais invasivo; ainda não mapeado.

> **Atalho honesto.** `LARECOMP_LOD_MIN` já entrega a "textura mais leve" sem escrever código novo.
> Um `AC_TEXTURE_QUALITY`/`AC_LOD_BIAS` no `settings.h` é a generalização natural (mesmo padrão de
> `AC_ANTIALIAS`), e `LARECOMP_SKIP_FSHASH` já entrega o "liga/desliga" de qualquer pass de fragment
> shader — motion blur incluso — uma vez identificado o hash.

### 7.7 Pendências abertas (não resolvidas nesta rodada)

1. ~~**Constantes de resolução/coordenada de UI.**~~ *(Resolvido em
   [`reverse-engineering-ui-perf.md`](reverse-engineering-ui-perf.md) §4.3: `0xd87e74` = struct
   de viewport `1920/1080`; `0xdb22f8` = grade intermediária `1600/…`. `EffectRenderSetting` em
   `0xdb23a8` continua pendente — ponteiro zerado no snapshot.)*
2. **Estrutura de globais em `0xdc2000`.** O sinal de "menu aberto" não existe como global (UI é
   orientada a objetos — ver [`reverse-engineering-ui-perf.md`](reverse-engineering-ui-perf.md) §2);
   a região está só parcialmente mapeada (`+0x6f8` = índice de idioma, `+0x750` = LanguageCode).
3. **Layout do diretório RomFS.** Corrigir o struct de entrada para listar a árvore de localização.
4. **Saves/config (`nn::fs`).** Mapear os call sites de `nn::fs` para identificar onde o jogo
   lê/grava opções persistentes (volume, idioma de voz, legenda, câmera).
5. ~~**`g_ShaderSettingManager`.**~~ *(Resolvido em [`reverse-engineering-ui-perf.md`](reverse-engineering-ui-perf.md) §4.1: construtor `fn 0x3fe260`, cadeia `Filter01..09` em tabela estática de boot, junto às constantes `1280/720` dos buffers de filtro.)*

---

## 8. Como regenerar a evidência

A análise foi feita inline (shell + Python) sobre `astral_chain/nso.py`; nada foi persistido como
ferramenta reutilizável. Para reproduzir ou estender:

1. Abrir o NSO com `Nso` de `astral_chain/nso.py` e iterar relocações de kind 1026 (`JUMP_SLOT`).
2. Reconhecer stubs PLT (padrão `ADRP/LDR/ADD/BR`) e varrer `.text` com `md.skipdata=True` para
   coletar call sites (`/tmp/opencode/main_callsites.json`).
3. Para cada símbolo de interesse (`GetDesiredLanguage`, `LanguageCode::Make`, `ShowKeyboard`,
   `GetSharedFont`, etc.), seguir os call sites até a função que os usa.

> **Decisão pendente.** Persistir o mapeador import→PLT→call-site como ferramenta reutilizável sob
> `astral_chain/` (ou `extra/tools/`) evitaria re-derivar o passo 2 a cada investigação. Não feito
> ainda para manter o diff enxuto (Ponytail): só vale persistir quando houver uma segunda
> investigação que o reutilize.

---

## 9. Observações de segurança e convenções

- Esta análise é **somente leitura**; nenhum arquivo de código foi modificado.
- Os endereços citados são relativos ao `main` com base `0x7000010000` e valem para o build v1.0.1
  com os patches `1.0.1.pchtxt` + `cutscene-catchup-1.0.1.pchtxt` aplicados. O linker
  (`astral_chain/translator/linker.py`) empacota `rtld`, `main` e `sdk` a partir de `0x7000000000`;
  como `rtld` ocupa 16 KiB, `main` é o **segundo** módulo e começa em `0x7000010000`. `MAIN_BASE` em
  `third_party/LASRecomp/runtime/cpu/rt.h` é a base do *primeiro* módulo, não do `main`. Qualquer
  rebase/re-patch invalida os offsets e exige re-derivação.
- Nenhum asset proprietário, dump, chave ou shader foi versionado.

---

## 10. Slots de estado e a janela RE Info (overlay F8)

O overlay `extra/ui/overlay/ac_reinfo.cpp` expõe estes valores ao vivo, em modo somente leitura. Cada
slot global em `.data`/`.bss` guarda um **ponteiro** para o bloco real, então todo valor custa duas
desreferências (`base + slot → ponteiro → valor`), com `base = 0x7000010000` (ver §9: `main` é o
segundo módulo, depois do `rtld`). Toda leitura é checada contra a janela guest
(`GUEST_LO 0x7000000000` .. `GUEST_HI 0x7320000000`) antes de acontecer; slot nulo ou fora da faixa
aparece como `(n/d)` em vez de derrubar o jogo.

### 10.1 Slots globais

| Offset | Tipo | Campo | Getter |
|---|---|---|---|
| `0xdbcf10` | `u32` | área de trigger (`AreaName.csv` / `CORE_MAP_AREA_`) | `0x56c240` |
| `0xdbcf20` | `u32` | sub-área | `0x56c270` |
| `0xdbc420` | `u32` | missão ativa (combate / subcaso) | `0x4be600` |
| `0xdbc950` | `u32` | id do boss | `0x69f5a0` (resolvedor de nome) |
| `0xdbc928` | `u32` | boss ativo | `0x5407f0` (`== 1`) |
| `0xdbc930` | `u32` | fase do boss | — |
| `0xdbc940` | `u32` | estado de batalha | — |
| `0xdbc948` | `f32` | timer de batalha | reset para `30.0f` (`0x41f00000`) |
| `0xdb83d8` | `ptr` | struct de save | — |
| `0xdb84c8` | `ptr` | struct de cena | — |
| `0xdb84f8` | `u32` | id de fase (`p100..pb01` → `0x0100..0x0b01`) | `0x561320` |
| `0xdb8930` | `ptr` | gerenciador `ScenarioCounter` (`FILE` e `File_Scenario`) | `0x4fb2f0`, `0x522df0` |

Campos derivados das structs:
- **Save (`*(0xdb83d8)`)**: playtime `u64` em `save+0`, File salvo (`u16` little-endian `1..12`) em `save+0x16` (`0x6f573c`), dificuldade raw em `save+0xaefa0`, flags em `save+0xa8b8c`, e pares `{group_hash, step_hash}` do `ScenarioCounter` a partir de `save+0x14d064` (`0x522480`, com `File_Scenario` em `save+0x14d06c` / `save+0x14d070`).
- **Cena (`*(0xdb84c8)`)**: `scene_id` em `scene+0x180` (`0x53fc94`). Nos mapas terrestres da Arca (`0x11xx..0x1cxx`), o **byte alto** identifica o estágio (`0x11` Grand Avenue / Rayleigh Plaza, `0x12` Harmony Square, `0x13` Sector V, `0x14` Ark Mall, `0x15` Ark Sewer Management, `0x16` ARI, `0x18` Zone 09, `0x19` Police HQ, `0x1a` Maison Forest, `0x1b` Harmony Square Station, `0x1c` Eastside Highway) e o **byte baixo em BCD** (`0x01..0x12`) identifica o File (`1..12`). No **Astral Plane**, `scene_id` assume `0x6xxx` (`0x6000`, `0x6180`, `0x61a0`, `0x6420`, `0x6460`, `0x6480`, `0x64c0`, `0x64e0`) e `phase_id` (`*(0xdb84f8)`) assume `0x0a00..0x0bff` (`pa00..pb01`).
- **`ScenarioCounter` (`mgr = *(0xdb8930)`)**: `*(s32*)(mgr + 0x12f0)` indexa o grupo `FILE` (`0`) em `*(u64*)(mgr + 0x28 + idx*8)`, cujo campo `+0x2c` (`s32`) guarda o File atual (`1..12`, ou `0` na tela de título). `*(s32*)(mgr + 0x12f4)` indexa o grupo `File_Scenario` (`1`), cujo campo `+0x2c` (`s32`, `0..326`) é o passo exato de `ScenarioCounter.csv` usado por `0x522df0` para exibir o objetivo atual do caso no HUD (`CORE_PURPOSE_FILE_%04x`, ex.: `0x0309` = `"Rescue Isabella."`).

### 10.2 Dificuldade

O getter `0x4c26e0` (`game_difficult_type`) indexa a tabela de saltos em `0xbe08fc` com
`índice = valor + 2`, válida para `-2..2`:
`-2 super_casual`, `-1 error`, `0 casual`, `1 standard`, `2 hardcore`.
O jogo normaliza o valor salvo antes de exibir (`0x6f2240`, `0x6f6588`): flags `+0xa8b8c == 0` força
`-2`, e o campo `+0xaefa0` é reescrito com o índice selecionado.

### 10.3 Chaves de texto

| Offset | Formato | Uso |
|---|---|---|
| `0xb6327d` | `MENU_DATA_AREA_NAME_%04x` | nome canônico da área (`menu_text`, `0x5000..0x5230`) |
| `0xb76832` | `MENU_MISSION_NAME_%04x` | nome da missão (`menu_text`) |
| `0xb74339` | `EVENT_BOSS_NAME_%04x` | nome do boss (`GameWord`) |
| `0xb61e7c` | `CORE_MAP_AREA_%04x` | sub-região do mapa (`ui_core_us.dat -> messcore.mcd`, `AreaName.csv`) |
| `0xb6a2d3` | `CORE_MAP_AREA_%02x` | região principal do mapa (`área >> 8` em `messcore.mcd`) |
| `0xb62fb7` | `%03d:%02d:%02d` | playtime |

### 10.4 Tabelas id→nome
 
`src/additions/overlay/ac_reinfo_tables.h` é **gerado** por `tools/qa/reinfo_tables.py`, que lê
`game/romfs.bin → /Text/Text.pkz` (`menu_text_USen.bin`, `GameWord_USen.bin`, `Core_text_USen.bin`) e
`/core/core.pkz → core.dat` (`ScenarioCounter.csv`, `PlannerAutoSaveList.csv`).

Resultado:
- **22 áreas canônicas** (`MENU_DATA_AREA_NAME_*` em `menu_text`, `0x5000..0x5230`)
- **118 objetivos de caso** (`CORE_PURPOSE_FILE_*` em `Core_text`, ex.: `"Rescue Isabella."`)
- **327 passos de `File_Scenario`** (`k_ac_scenario_steps`: `{step_hash, file_num, purpose_id, area_id}`)
- **292 missões** (`FILE_MISSION_*` em `GameWord` + `MENU_MISSION_NAME_*` em `menu_text`)
- **82 ids de boss** (`EVENT_BOSS_NAME_*` em `GameWord`)
- **12 Files da história** (`k_ac_files`: Startup, Awake, Link, Siege, Accord, Complicit, Wild, Peace, Salvation, Madness, Reckoning, Rebirth)
- **5 Legions** (`k_ac_legions`: Sword, Arrow, Arm, Beast, Axe)

> **Armadilhas registradas.**
> 1. `*(0xdbcf10)` (`0x56c240`) armazena IDs de trigger de `AreaName.csv` (`0x1000..0x9900`, onde o nibble alto `1..9` corresponde à família da fase `p100..p900`), e **não** IDs `0x5000..0x5230` de `MENU_DATA_AREA_NAME_`. Em particular, `0x5000` em `*(0xdbcf10)` na fase `p500` é `Ark Sewer Management (Canal Control Room)` (`0x5230`), e ao entrar no Astral Plane (`scene_id = 0x6xxx`, `phase_id = 0x0a00..0x0bff`) `*(0xdbcf10)` retém o último trigger terrestre visitado (ex.: `0x8404` Maison Forest). Por isso, `AcGameData::ResolveSnapshot` verifica primeiro `scene_id`/`phase_id` do Astral Plane e valida se `(raw_area >> 12)` pertence à fase atual antes de mapear para `0x5010..0x5230`.
> 2. No buffer de save (`*(0xdb83d8)`), o File atual é um `uint16_t` little-endian em `+0x16` (`0x6f573c`); ler `+0x17` retorna sempre `0` (byte alto do `u16`).




