# SHADOW CHANGES & DIRETRIZES DO PROJETO

## 1. Diretrizes de Código e Arquitetura Downstream

1. **Modularidade e Extensão dos Arquivos**:
   - Cada arquivo mantém responsabilidade única, com extensão ideal <= 300 linhas e teto rígido de 400 linhas.
   - Código desacoplado em módulos independentes facilitando auditoria e isolamento de escopo.

2. **C++ Moderno e Nomenclatura**:
   - Utilização de C++20 com tipos explícitos, gerenciamento RAII e encapsulamento em classes.
   - Módulos downstream organizados no namespace `ac::launcher` (UI) e prefixos `extra_*` (C) / `Extra::` (C++).

3. **Isolamento de Modificações no Runtime (Hooks / Trampolines)**:
   - Todo código customizado downstream reside sob o diretório `extra/`.
   - Modificações em arquivos upstream restritas a no máximo 1 include (`extra/hooks.h`) e 3 linhas por ponto de hook.
   - Hooks de linha única terminam com `// [extra]`; regiões de múltiplas linhas levam `// [extra]` apenas na linha inicial e na linha final do bloco.

4. **Registro Neutro e Hipotético**:
   - Descrições estritamente técnicas e neutras, sem juízos de valor ("melhoria", "conserto").
   - Alterações formuladas como hipóteses de adaptação ou substituição de implementação.

---

## 2. Upstream Touch Ledger

### Reorganização estrutural por módulos (2026-11-02)

O repositório foi reorganizado em pastas por domínio/módulo, sem alteração de lógica de código:

- **`extra/`**: `core/` dividido em `platform/` (POSIX: memória, sync, tempo, pacing, crypto, ELF), `sdl/` (janela/áudio), `input/` (mouse/menus), `gpu/` (Vulkan/Maxwell/write-tracking/shaderc), `storage/` (saves/find), `ui/font_trampoline.h` e `toggles/` (`ac_toggles.*`). `extra/hooks.h` e os ~12 sites de include (`las/`, overlay, tools) tiveram apenas os caminhos de include atualizados (mesmos marcadores `// [extra]` nas mesmas linhas).
- **`astral_chain/runtime/`**: headers e ferramentas C agrupados em `gpu/`, `audio/`, `input/`, `storage/`, `services/`, `kernel/` e `tools/`. Nenhuma linha de código C alterada: includes continuam por bare-name, resolvidos via `-I` por subpasta (`runtime_include_flags()` em `astral_chain/build/native_build.py`). `host_runtime.c.in` e `las/` permanecem na raiz de `runtime/`.
- **`third_party/LASRecomp/` (vendorado)**: `tools/lifter.py` + `simd_c.py` → `translator/`; raiz de `runtime/` em `cpu/`, `kernel/`, `build/`, `input/` e `hle/hle.h`. Includes relativos `../x.h` reescritos como bare-name dentro dessa árvore (apenas caminhos). `verify_reference` continua válido (lê apenas o arquivo `COMMIT`); o modo "checkout git completo" deixa de valer a pena porque os caminhos internos divergem do upstream — a cópia vendorada com `COMMIT` é a forma suportada.
- **Pacote Python `astral_chain/`**: dividido em `shared/`, `translator/`, `build/`, `gpu/` e `tools/`; imports relativos internos promovidos a absolutos (`astral_chain.<módulo>...`). Shims finos (`astral_chain/setup_game.py`, `astral_chain/native_trace.py`) preservam os comandos `python -m astral_chain.setup_game` / `native_trace` herdados.
- **`astral_chain/gpu/shader_cache.py`**: o comando de compilação do `shader_precompile` passa a usar `runtime_include_flags()` (subpastas do runtime vendorado) mantendo `-I nvn/maxwell` explícito para `shader.h`.
- Consequência de cache esperada: unidades guest (~122 objetos) **não** são recompiladas (proveniência por hash de conteúdo preservada); a camada NVN é recompilada uma única vez (chave inclui caminhos de `extra/`) na primeira build após a reorganização.

| Upstream file | Hook / change | Extra module | Purpose | Added |
|---|---|---|---|---|
| `astral_chain/runtime/host_runtime.c.in` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de ponte POSIX no runtime | 3 lines |
| `astral_chain/runtime/host_runtime.c.in` | `nvn_lazy_vectored` | `extra/platform/memory_trampoline.h` | Hipótese de interceptação de falta lazy | 4 lines |
| `astral_chain/runtime/services/app_window.h` | `app_set_fullscreen` | `extra/sdl/window_trampoline.h` | Alternância de tela cheia via SDL3 | 3 lines |
| `astral_chain/runtime/services/app_window.h` | `app_pump` | `extra/sdl/window_trampoline.h` | Bombeamento de eventos de entrada via SDL3 | 3 lines |
| `astral_chain/runtime/services/app_window.h` | `app_open` | `extra/sdl/window_trampoline.h` | Criação de janela de inicialização via SDL3 | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `backend_init` / `backend_clear_window` | `device.c` | Orquestrador enxuto (~20 linhas) integrando submódulos de domínio em `device/` | 19 lines |
| `astral_chain/runtime/las/nvn/vulkan/device/` | `instance`, `physical_device`, `logical_device`, `surface`, `swapchain`, `present`, `postprocess` | `device/` | Separação do backend Vulkan por ciclo de vida, responsabilidade funcional e telemetria P50/P95/P99 | 7 modules |
| `astral_chain/runtime/kernel/guest_threads.h` | `guest_tls_allocate` | `extra/platform/sync_trampoline.h` | Alocação de página TLS para threads hóspedes | 3 lines |
| `astral_chain/runtime/kernel/guest_threads.h` | `guest_parallel_start` | `extra/platform/sync_trampoline.h` | Inicialização de threads hóspedes em paralelo | 3 lines |
| `astral_chain/runtime/audio/wave_output.h` | `#include "extra/hooks.h"` | `extra/sdl/audio_trampoline.h` | Redirecionamento da API WaveOutput para SDL3 | 3 lines |
| `astral_chain/runtime/las/button_prompts.c` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de cabeçalho unificado de hooks | 3 lines |
| `astral_chain/runtime/las/button_prompts.c` | `enabled` / `texpatch_*` / `cut_text` | `extra/ui/font_trampoline.h` | Substituição de GDI por FreeType e troca dinâmica de prompts (Switch <-> Teclado/Mouse) | 16 lines |
| `astral_chain/runtime/las/input_bindings.h` | `ib_defaults` | `extra/sdl/window_trampoline.h` | Mapeamento ergonômico padrão WASD + Mouse estilo BBPort no Linux | 7 lines |
| `astral_chain/runtime/las/hle.h` | `HLE_SECTION` | `extra/platform/platform_posix.h` | Definição de atributo de seção de símbolos ELF | 3 lines |
| `astral_chain/runtime/gpu/nvn_hle.h` | `nvn_linked_lookup` | `extra/platform/platform_posix.h` | Descoberta de símbolos HLE entre limites ELF | 3 lines |
| `astral_chain/runtime/storage/save_store.h` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de cabeçalho unificado de hooks | 3 lines |
| `astral_chain/runtime/storage/save_store.h` | `save_store_ensure` | `extra/storage/storage_trampoline.h` | Normalização de diretório de save no Linux | 4 lines |
| `astral_chain/runtime/storage/save_fs.h` | `save_resolve` | `extra/storage/storage_trampoline.h` | Formatação de caminhos e conversão mbstowcs | 6 lines |
| `astral_chain/runtime/las/las_compat.h` | `#ifndef _WIN32` | `extra/storage/storage_trampoline.h` | Inclusão de rotinas de compatibilidade POSIX | 4 lines |
| `astral_chain/translator/native_trace.py` | `run-env.txt` | `run_guarded.sh` | Exportação de backend vinculado e glslang | 4 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec.c` | `vk_exec_reset` | `exec.c` | Inclusão de `guest_vcopy_flush()` no reset do backend Vulkan | 1 line |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/vcache.c` | `guest_direct` (`min_direct` / `pt_init`) | `vcache.c` | Bypassa fallback de cópia quando write tracking (pt_init) está ativo, permitindo VRAM direta em todos os tamanhos | 15 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/gcopy.c` | `guest_copy_flush` | `gcopy.c` | Desacoplamento da invalidação do cache de vértices de atualizações de cbuf | 2 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/vcopy.c` | `guest_copy_vtx` | `vcopy.c` | Hash de 64-bit incluindo `size` e correspondência estrita `size == size` | 4 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/commands/draw.c` | `draw_` (streams & IB) | `draw.c` | Redução do limiar small_max de 1024 para 16 vértices e teto de cópia de 64 KB, liberando bind direto de malhas | 24 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/commands/present.c` | `vk_present_draw` / `post_pass` | `present.c` | Sincronização de scissor filter via `ss_forget()` eliminando corte retangular escuro | 7 lines |
| `astral_chain/runtime/las/nvn/objects.c` | `register_texture` | `objects.c` | Invalidação do gerador de views de textura (`vk_view_gen++`) no registro de pools | 1 line |
| `astral_chain/runtime/las/nvn/vulkan/textures.c` | `mark_dynamic` | `extra/gpu/vulkan_trampoline.h` | Amostragem FNV para texturas de vídeo dinâmicas | 5 lines |
| `astral_chain/runtime/las/settings.h` | `settings_load` | `extra/ui/launcher` | Suporte a variável de ambiente `AC_UPSCALER=fsr1` | 4 lines |
| `astral_chain/runtime/input/hid_service.h` | `hid_xinput` | `extra/sdl/window_trampoline.h` | Leitura de gamepad via SDL3 no Linux | 4 lines |
| `astral_chain/runtime/services/pl_hle.h` | `pl_load_fonts` | `extra/platform/platform_posix.h` | Caminhos de fontes padrão do sistema Linux | 5 lines |
| `astral_chain/runtime/las/maxwell_ext.h` | `#include "extra/gpu/maxwell_extra.h"` | `extra/gpu/maxwell_extra.h` | Tradução de instruções compute Maxwell (família LEA) | 1 line |
| `astral_chain/runtime/gpu/nvn_hle.h` | `nvn_present_pace` | `extra/platform/pace_trampoline.h` | Suporte dinâmico a taxa de quadros alvo (AC_FPS e pacing customizado) | 4 lines |
| `astral_chain/runtime/gpu/nvn_hle.h` | `plat_sleep_until_ns` | `extra/platform/time_trampoline.h` | Espera híbrida precisa com clock_nanosleep e spin final de 250 µs | 1 line |
| `astral_chain/runtime/host_runtime.c.in` | `rt_svc` (`imm == 0xb`) | `extra/platform/time_trampoline.h` | Espera híbrida para svcSleepThread eliminando polling com sched_yield | 1 line |
| `astral_chain/runtime/input/mouse.h` | `mouse_config` / `mouse_look_stick` | `extra/input/mouse_mode.h` | Curva de resposta suave (power 1.35), escala vertical Y e unificação com árbitro | 3 lines |
| `astral_chain/runtime/services/app_window.h` | `app_pump` / `app_open` / callbacks | `extra/input/menu_detect.h` / `mouse_menu.h` | Registro de app_window_thread, foco no Linux, política de modo, cursor e botões em MouseRing | 38 lines |
| `astral_chain/runtime/input/hid_service.h` | `hid_publish` / `hid_touch_publish` / `hid_poll` | `extra/input/mouse_menu.h` | Aplicação de botões de menu e contato de touch com gate e publicação da liberação | 11 lines |
| `astral_chain/runtime/las/nvn/objects.c` | `aq_enabled` + `#include "extra/toggles/ac_toggles.h"` | `extra/toggles/ac_toggles.h` | Kill switch runtime da submissão assíncrona (A/B de flicker via menu Insert) | 2 lines |
| `astral_chain/runtime/las/nvn/vulkan/device/swapchain.c` | `dev_present_mode` + include | `extra/toggles/ac_toggles.h` | Override do present mode pela overlay com recriação de swapchain | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device/postprocess.c` | `backend_present` + include | `extra/ui/overlay/ac_overlay.h` | Menu ImGui in-game (Insert): pré-frame (swapchain pendente) e desenho na imagem da janela | 3 lines |
| `astral_chain/runtime/las/game_hooks_neutral.c` | `smaa_area_tex`/`smaa_search_tex`/`smaa_hlsl_src` (stubs removidos) | `nvn/smaa_data.c` | Vendorização do SMAA 1x: os três símbolos deixam os stubs `FATAL` e passam a vir dos dados vendidos | 1 line |
| `astral_chain/runtime/host_runtime.c.in` | `mapping_generation` / `resize_heap` | `kernel/exclusive_pair.h` | Geração incremental de mapeamentos de memória para cache de validação em operações atômicas | 3 lines |
| `astral_chain/runtime/gpu/memory_attributes.h` | `update_uncached` | `kernel/exclusive_pair.h` | Invalidação de cache em alteração de atributos de memória não cacheada | 1 line |
| `astral_chain/runtime/input/shared_memory.h` | `shared_granules` (`SHARED_GRANULE_COUNT`) | `kernel/exclusive_pair.h` / `input/shared_memory.h` | Expansão de grânulos de 64 para 1024 (previne STOP shared-granule-limit em sessões longas) e invalidação de cache | 8 lines |
| `astral_chain/runtime/kernel/stack_memory.h` | `stack_map` / `stack_unmap` | `kernel/exclusive_pair.h` | Invalidação de cache em mapeamento de pilhas de threads hóspedes | 2 lines |
| `astral_chain/runtime/kernel/transfer_memory.h` | `transfer_create` / `transfer_release` | `kernel/exclusive_pair.h` | Invalidação de cache em criação/liberação de memória de transferência | 2 lines |
| `astral_chain/runtime/kernel/exclusive_pair.h` | `pair_memory` | `kernel/exclusive_pair.h` | Cache TLS do último intervalo validado eliminando sobrecarga repetida de `query()` (~6.5% de ganho de CPU) | 9 lines |
| `astral_chain/runtime/gpu/nvn_hle.h` | `nvn_heap_mirror_interest` | `gpu/nvn_hle.h` | Tabela hash TLS de 1024 entradas para retorno imediato de faixas registradas no espelho do heap (-0.7ms render worker) | 10 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/memory/gfast.c` | `gfast_prepare` | `exec/memory/gfast.c` | Desativação por padrão de cópias de compute global em GPUs integradas (Intel Arc/AMD APUs/Steam Deck) | 4 lines |
| `astral_chain/runtime/host_runtime.c.in` | modularização arquitetural | `kernel/heap_memory.h`, `services/ipc_session.h`, `services/ipc_dispatch.h`, `kernel/svc_dispatch.h`, `kernel/horizon_defs.h` | Desmembramento do monólito de 1.389 linhas para orquestrador enxuto de 221 linhas | 5 módulos |
| `astral_chain/runtime/kernel/horizon_defs.h` | `HorizonSvc`/`HorizonResult`/`HorizonHandleRange` | `kernel/horizon_defs.h` | Definições arquiteturais do Horizon OS eliminando números mágicos no subsistema kernel/IPC | 170 lines |
| `astral_chain/runtime/kernel/heap_memory.h` | `snapshot`/`query`/`resize_heap` | `kernel/heap_memory.h` | Módulo dedicado de gestão de heap hóspede e query de mapeamentos virtuais sem constantes mágicas | 278 lines |
| `astral_chain/runtime/services/ipc_session.h` | tabelas e handles IPC | `services/ipc_session.h` | Tipos, ciclo de vida de sessões e empacotamento de respostas CMIF com partições explícitas de handles | 240 lines |
| `astral_chain/runtime/services/ipc_dispatch.h` | roteamento CMIF | `services/ipc_dispatch.h` | Despacho de mensagens IPC e roteamento de comandos de serviços Switch legível e sem números mágicos | 650 lines |
| `astral_chain/runtime/services/ipc_dispatch.h` | `resolve_system_language` | `services/ipc_dispatch.h` | Mapeamento virtual de pt-BR para slot en-US (renderizador latino) | 6 lines |
| `astral_chain/runtime/storage/romfs_storage.h` | `extra_romfs_overlay_init` / `extra_romfs_overlay_read` | `extra/storage/romfs_overlay.h` | LayeredFS virtual não-destrutivo para mods/traducao_ptbr e mods/ | 5 lines |
| `third_party/LASRecomp/runtime/kernel/settings.c` | `voice_language` e `pt-BR` | `third_party/LASRecomp/runtime/kernel/settings.c` | Suporte a idioma virtual pt-BR e persistência de dublagem | 22 lines |
| `astral_chain/runtime/kernel/svc_dispatch.h` | `rt_svc`/`rt_svc_body` | `kernel/svc_dispatch.h` | Despachante central de chamadas de supervisor (SVC) e frame pacing descompactado e legível | 420 lines |
| `astral_chain/runtime/las/settings.h` | `UPSCALE_FSR3` / `framegen` / `fsr_jitter` / `fsr_sharpness` / `render_scale` | `extra/toggles/ac_toggles.h` | Persistência em `larecomp.ini` e overrides de env para AMD FSR 3.1 Upscaler, Camera Jitter, RCAS e Frame Generation | 30 lines |
| `astral_chain/runtime/las/nvn/vulkan/shaders.c` | `builtin_src` + `#include "extra/gpu/fsr3_shaders.h"` | `extra/gpu/fsr3_shaders.h` | Registro dos shaders GLSL 450 do pipeline FSR 3.1 (`fsr3_mv.frag`, `fsr3_taa.frag`, `fsr3_rcas.frag`, `fsr3_fg.frag`) | 6 lines |
| `astral_chain/runtime/las/nvn/vulkan/textures.c` | `vk_sampler` + `#include "extra/gpu/fsr3_extra.h"` | `extra/gpu/fsr3_extra.h` | Ajuste seletivo de `mipLodBias` negativo (-0.25) exclusivamente em samplers anisotrópicos com mipmapping (`max_aniso > 1`) | 2 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/commands/present.c` | `post_barrier` / `post_process` + `s_fg_real_view` | `extra/gpu/fsr3_extra.h` / `fsr3_shaders.h` | Barreira leve de estágio fragmento (`post_barrier`) e pipeline FSR 3.1 fundido (Half-Res MV + EASU 12-tap isotrópico + Jitter Tangencial + RCAS em registrador + YCoCg TAAU + FrameGen) | 62 lines |
| `astral_chain/runtime/las/nvn/vulkan/device/postprocess.c` | `backend_present` + `record_present_sample` + `extra_fsr3_frame_step` | `extra/gpu/fsr3_extra.h` | Dupla apresentação cadenciada na Swapchain Vulkan (FrameGen 2x), telemetria P50/P95/P99/1%Low em nanosegundos (`SDL_GetTicksNS`) e avanço de fase Halton | 40 lines |
| `astral_chain/runtime/host_runtime.c.in` | `klock_wait_ns` | `extra/platform/sync_trampoline.h` | Espera em variável de condição do kernel com resolução de nanosegundos (`extra_sleep_cv_ns`) | 5 lines |
| `astral_chain/runtime/kernel/guest_threads.h` | `parallel_block_wait` | `extra/platform/sync_trampoline.h` / `submission.c` | Espera direta no semáforo de linha do tempo Vulkan (`renderer_wait_seq_timeout`) em `THREAD_WAIT_GPU` e espera em nanosegundos (`klock_wait_ns`) sem truncamento em milissegundos | 18 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/submission/submission.c` | `renderer_wait_seq_timeout` | `extra/platform/sync_trampoline.h` | Exposição de `vkWaitSemaphores` com timeout em nanosegundos para threads hóspedes paralelas | 12 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/memory/heap_mirror.c` | `gmem_wb_record` / `gmem_writeback_apply` / `gmem_reset` | `extra/gpu/heap_mirror_extra.h` | Pool de reciclagem de buffers de staging VMA (`extra_wb_pool_alloc/free/reset`) e sincronização de `wb_flight` sob `lazy_lock` | 10 lines |
| `astral_chain/runtime/las/nvn/objects.c` | `nvnQueuePresentTexture` | `extra/platform/pace_trampoline.h` | Sincronização no deadline de `nvn_present_pace` antes do despacho de `AJ_PRESENT` / `backend_present`, absorvendo variações de tempo de gravação por frame | 2 lines |

---

## 3. Registro de Alterações (Decisões & Hipóteses)

### Módulos de Mouse, Menus e Política de Cursor (`extra/input/mouse_mode.h`, `mouse_menu.h` & `menu_detect.h`)
- Árbitro de captura de ponteiro SDL3 com alternância via F10 e tecla de pausa do jogo (MINUS), `!e.key.repeat`, unificação de estado e zeragem de acumuladores na transição (sem chicote de câmera).
- **A aplicação inicia com o cursor livre**, para a tela de título ser clicável sem nenhuma tecla; uma tecla de stick (nunca D-pad) toma o mouse para a câmera.
- Curva de aceleração de câmera configurável via `AC_MOUSE_CURVE` (padrão power 1.35) e escala de eixo Y (`AC_MOUSE_YSCALE`), com fórmula exata $out = 0.10 + 0.90 \cdot m^{curva}$.
- Eliminação do piso rígido de 0.2 com transição suave acima do deadzone do motor.
- Alt+Tab devolve o cursor e o restaura ao recuperar o foco, preservando a escolha explícita de menu (`extra_mouse_mode_suspend/resume`).
- **Ponteiro de menu sem zonas hard-coded**: movimento gera passos de D-pad a cada `AC_MOUSE_MENU_CELL` pontos, clique esquerdo injeta A, direito injeta B e a roda injeta D-pad cima/baixo. Funciona em qualquer tela do jogo porque usa o canal que os menus realmente obedecem (ver engenharia reversa em `docs/plan-mouse-menu.md` §4).
- **Seleção posicional absoluta descartada por medição**: o jogo não expõe geometria de item nem o índice selecionado, e o objeto de estado de mouse que ele constrói a cada frame (`*(0xdc2540)`) não é lido por nenhum código do binário. Uma lógica de hit-testing por células chegou a ser implementada e foi revertida; `extra/tools/mouse_pointer_test.c` guarda a regressão de que posicionar ou saltar o cursor não gera passo nenhum.
- **Hook de render investigado e descartado** (`docs/plan-mouse-menu.md` §4.6): os 212 draws com viewport do frame do LOAD DATA usam a viewport inteira (nenhum por item), os vertex buffers trazem quads unitários em espaço local e a posição final sai do vertex shader. Extrair retângulos exigiria emular shaders, e ainda faltaria o índice selecionado — que o jogo não expõe.
- **Touch testado também em handheld** (`AC_HANDHELD=1`), fechando o furo de que o painel só existe fora do dock no Switch: `GetTouchScreenState<16>` é chamado incondicionalmente (`0x9adb1c`, sem consulta a `GetOperationMode`) e o contato continua sem selecionar o slot. A posição publicada foi conferida no anel (`374,372` pontos → `299,297` no painel).
- Contato de touch opcional (`AC_MOUSE_MENU_TOUCH=1`) na posição do cursor, mapeado por *fit* 16:9 centrado na janela em pontos SDL (sem 1600×900 fixo), com liberação publicada para o jogo não reter contato. O contato chega ao jogo (máquina de estados em `ctx+0x24` percorre `0→2→3→1→0`), mas nenhum menu exercitado usa touch para selecionar item (`docs/plan-mouse-menu.md` §4.4).
- Warp de centralização ao entrar no modo menu não conta como movimento (`extra_mouse_menu_skip_motion`), evitando avançar a lista sozinho.
- Teste automatizado `extra/tools/mouse_pointer_test.c`, executado na etapa 4 do `audit.sh`.
- Conexão das coordenadas absolutas reais (`mouse_x`, `mouse_y`) e da máscara de botões (`mouse_buttons`) ao anel oficial do SDK (`MouseRing` em `hid_page + 0x3400`) — ABI mantido; o `main` lê esse anel por frame, mas só constrói um objeto interno que nada consome.
- **Gate de foco corrigido**: `g_sdl_input_focus` nunca virava 1 em execução sem interação (o evento `WINDOW_FOCUS_GAINED` do SDL não chegava), então o bloco `if(connected && focused)` de `hid_publish` era pulado e toda a injeção de menu ficava silenciosamente desligada. Uma linha em `extra/sdl/window_trampoline.h`, após `SDL_RaiseWindow`, fixa o foco; o evento de foco do SDL continua baixando-o quando a janela perde o foco.
- Rolagem precisa de listas via roda do mouse (D-Pad Up / Down), com fila e folga entre pulsos para o jogo ver cada passo.
- Modularização com widget dedicado `MouseSettingsWidget` (`extra/ui/launcher/include/MouseSettingsWidget.hpp`), preservando `SettingsDialog` enxuto (<= 300 linhas).
- Suporte a seleção de modo de apresentação Vulkan (`presentMode`: mailbox/fifo/immediate) nos controles da aba Game do Launcher oficial Qt6.

### Remoção de Binários e Scripts Win32
- Remoção de executáveis `.exe`, scripts `.cmd`/`.ps1` e código C# do launcher original.
- Supressão de binários pré-compilados Windows para garantir portabilidade nativa em Linux.

### Módulo de Sincronização (`extra/platform/sync_trampoline.h`)
- Hipótese de emulação de `HostEvent`, mutex e timers Win32 via POSIX `pthread_cond` e `clock_gettime`.
- Implementação de TLS hóspede com `mmap` e inicialização de threads paralelas via `pthread_create`.

### Módulo de Memória Virtual (`extra/platform/memory_trampoline.h`)
- Substituição de `VirtualAlloc` por `mmap(MAP_NORESERVE | MAP_FIXED_NOREPLACE)` e `mprotect`.
- Tratamento de falta de página de escrita GPU via tratador `sigaction(SIGSEGV)`.

### Módulo de Janela e Entrada (`extra/sdl/window_trampoline.h`)
- Criação e ciclo de vida de janela via `SDL_CreateWindow` com flags Vulkan e redimensionamento.
- Mapeamento de eventos de teclado, foco, captura relativa de mouse e alternância de fullscreen.
- `g_sdl_input_focus = 1` imediatamente após `SDL_RaiseWindow`: o evento `SDL_EVENT_WINDOW_FOCUS_GAINED` pode não chegar em execução sem interação do usuário, e o gate `if(connected && focused)` de `hid_publish` mantinha a injeção de menu silenciosamente desligada. O evento de perda de foco continua zerando a flag.

### Módulo de Apresentação Vulkan (`extra/gpu/vulkan_trampoline.h`)
- Substituição das extensões de superfície Win32 por `SDL_Vulkan_CreateSurface`.
- Coleta de extensões de instância requeridas através de `SDL_Vulkan_GetInstanceExtensions`.

### Módulo de Saída de Áudio (`extra/sdl/audio_trampoline.h`)
- Substituição da API `waveOut` por `SDL_AudioStream` em formato float32 interleaved.
- Emulação de eventos de callback e reprodução de buffers de áudio via chamadas SDL3.

### Módulo de Sistema de Arquivos (`extra/storage/storage_trampoline.h`)
- Conversão da enumeração de arquivos de `FindFirstFile` para `opendir`/`readdir` POSIX.
- Manipulação recursiva de pastas e arquivos através de chamadas de sistema `stat` e `mkdir`.

### Ponte de Plataforma POSIX (`extra/platform/platform_posix.h`)
- Definição de tipos, macros e constantes Win32 remapeados para chamadas do ecossistema POSIX.
- Implementação de `MsgWaitForMultipleObjectsEx` com temporização via `usleep`.

### Segmentos HLE em Binários ELF (`hle.h`, `nvn_hle.h`)
- Hipótese de ordenação de interceptadores HLE via símbolos fracos de seção ELF `__start/__stop`.
- Remoção da dependência exclusiva do linker PE/COFF para descoberta de funções exportadas.

### Prompts de Interface Neutros (`button_prompts.c`)
- Introdução de fallback neutro quando o subsistema GDI não estiver disponível no ambiente.
- Manutenção do ciclo de renderização gráfica sem dependências de bibliotecas Win32.

### Rotinas de Configuração do Jogo (`setup_game.py`)
- Atualização dos URLs de download para pacotes Linux nativos (Zig `.tar.xz`, glslang, nstool).
- Supressão de sufixos `.exe` e implementação de descompactação com suporte a tarballs xz.

### Pipeline de Compilação NvN e Nativa (`native_build.py`, `nvn_build.py`)
- Configuração de alvo de compilação `x86_64-linux-gnu` e ligação com `-lvulkan`, `-lSDL3`, `-lc++`.
- Inclusão do diretório raiz do projeto nos caminhos de busca para resolução de `extra/hooks.h`.

### Utilitários de Diagnóstico e Replay
- Ajuste das ferramentas de suporte (`shader_cache.py`, `bc7_cache.py`, `native_trace.py`) para Linux.
- Adequação de nomes de binários gerados para executáveis ELF e caminhos de biblioteca nativos.

### Scripts de Execução do Usuário
- Criação de `setup.sh` para preparação de ambiente virtual Python e verificação de ferramentas.
- Criação de `play.sh`, `play-diagnostics.sh`, `launcher.sh` e `astral_chain/tools/run_guarded.sh`.

### Launcher Qt6 Modular (`extra/ui/launcher/`)
- Implementação de launcher nativo em C++20 / Qt6 estruturado em classes no namespace `ac::launcher`.
- Arquivos modulares (Config, ProcessRunner, ControlsDialog, SettingsDialog, SetupDialog, MainWindow) <= 292 linhas.

### Diálogo Nativo de Configuração e Instalação (`SetupDialog.hpp`, `SetupDialog.cpp`)
- Substituição do seletor gráfico em Tkinter por diálogo modal nativo Qt6 com `QFileDialog`.
- Validação e persistência dos caminhos de chaves e pacotes de jogo em `userdata/setup.json`.

### Monitoramento de Progresso em Tempo Real (`setup_game.py`, `MainWindow.cpp`)
- Emissão de tags formatadas `PROGRESS:<pct>:<mensagem>` via streaming de saída padrão do `nstool`.
- Leitura e atualização contínua da `QProgressBar` no launcher durante as etapas de extração.

### Seletor de Destino e Redirecionamento de Dados (`SetupDialog`, `setup_game.py`)
- Adição de campo de pasta de instalação com persistência em `userdata/setup.json` e repasse via `--install-dir`.
- Vinculação transparente de `game/` via link simbólico permitindo alocar dados pesados em unidades externas.

### Minimização de Dependências Python (`setup_game.py`, `setup.sh`)
- Supressão integral de `tkinter` e uso de entrada padrão em modo terminal quando argumentos não forem informados.
- Detecção prévia de pacotes mínimos no interpretador do sistema, dispensando `.venv` quando disponíveis.

### Shim de Compilação de Shaders POSIX (`extra/gpu/shaderc_posix.h`)
- Substituição do pipeline de pipes e processos Win32 por `pipe`/`fork`/`exec` POSIX.
- Implementação isolada de `shaderc` executando `glslang` de forma limpa em ambiente Linux.

### Compatibilidade de Cabeçalhos NVN no Linux (`las_compat.h`, `platform_posix.h`)
- Proteção de inclusões de `<direct.h>` sob `_WIN32` e remapeamento de `_fseeki64` para `fseeko`.
- Declaração de `GetActiveProcessorCount` com `sysconf(_SC_NPROCESSORS_ONLN)` e `ALL_PROCESSOR_GROUPS`.

### Compatibilidade de Áudio Host (`extra/sdl/audio_trampoline.h`)
- Adição dos campos `format` e `header` em `AudioHookBuffer` e `AudioHookOutput` para compatibilidade de tipos.
- Definição do alias `WaveBuffer` permitindo consumo direto pelas rotinas de `audout_service.h`.

### Suporte a Fuso Horário POSIX (`extra/platform/time_trampoline.h`)
- Implementação de `__time64_t`, `_localtime64_s` e `GetTimeZoneInformation` mapeando `tm_gmtoff` para viés UTC.
- Fornecimento de `GetLocalTime` preenchendo a estrutura `SYSTEMTIME` com campos de horário do sistema local.

### Operações de Armazenamento e Consulta de Arquivos (`extra/storage/storage_trampoline.h`)
- Suporte a `BY_HANDLE_FILE_INFORMATION`, `GetFileInformationByHandle` e `GetDiskFreeSpaceExW` via `stat` e `statvfs`.
- Mapeamento de `GetFileAttributesA`, `CreateDirectoryA` e declaração das constantes `ERROR_NO_MORE_FILES` e `ERROR_INVALID_PARAMETER`.

### Enumeração de Diretórios e Padrões (`extra/storage/find_trampoline.h`)
- Separação modular de `FindFirstFileA`/`FindFirstFileW` e `FindNextFileA`/`FindNextFileW` com filtro por extensão.
- Encadeamento de fechamento de busca via `extra_close_handle` assegurando liberação de instâncias de `DIR`.

### Descritores e Sincronização com Contagem de Referência (`extra/platform/sync_trampoline.h`, `platform_posix.h`)
- Inclusão de `ref_count` na estrutura `ExtraHandle` com suporte a incremento em chamadas `DuplicateHandle`.
- Implementação de `SwitchToThread` via `sched_yield`, `ExitThread` via `pthread_exit` e QPC/QPF como funções inline `BOOL`.

### Pré-compilador de Shaders Nativo (`astral_chain/shader_cache.py`, `shader_precompile.c`)
- Inclusão dos cabeçalhos do projeto e Vulkan (`third_party/Vulkan-Headers/include`) na chamada do compilador.
- Vinculação das bibliotecas `-lvulkan`, `-lSDL3`, `-lpthread`, `-ldl`, `-lm` e proteção de `<windows.h>` sob `_WIN32`.

### Módulo de Memória Compartilhada POSIX (`extra/platform/shared_memory_trampoline.h`)
- Criação de memória compartilhada anônima via chamada de sistema `SYS_memfd_create` e ajuste com `ftruncate`.
- Mapeamento direto de granularidade e reposição de reserva de páginas hóspede através de `mmap(MAP_SHARED | MAP_FIXED)`.

### Rastreamento de Alocação Anônima (`extra/platform/memory_trampoline.h`)
- Registro de ponteiros e tamanhos para alocações anônimas em chamadas `VirtualAlloc(NULL, ...)` sem endereço base.
- Liberação correspondente via `VirtualFree(..., 0, MEM_RELEASE)` e preservação de faixa em `MEM_PRESERVE_PLACEHOLDER`.

### Modularização da Interface do Launcher (`Theme.hpp`, `Theme.cpp`, `MainWindowUi.cpp`)
- Extração de regras QSS e de instanciação de widgets para submódulos isolados limitando arquivos a <= 300 linhas.
- Preservação da estética visual e do pipeline de sinais sem ultrapassar o teto estipulado de código por arquivo.

### Vinculação e Retenção de Símbolos NVN ELF (`hle.h`, `platform_posix.h`, `native_trace.py`)
- Adição de atributo `retain` em `hle_entries` prevenindo descarte de tabela de funções pelo linker sob `--gc-sections`.
- Persistência das definições de `AC_NVN_HLE_LINKED` e `AC_GLSLANG` no arquivo de ambiente `run-env.txt`.

### Resolução de Executável Glslang (`extra/gpu/shaderc_posix.h`)
- Busca de fallback sequencial por variável de ambiente, caminho relativo local e utilitários instalados no sistema.
- Disponibilização contínua de compilador de shaders prevenindo interrupção na inicialização do subsistema Vulkan.

### Execução de Compilador de Shaders sem Pipes (`extra/gpu/shaderc_posix.h`, `platform_posix.h`)
- Substituição de pipes anônimos por arquivos transitórios para entrada e logs, prevenindo deadlock e interrupção `SIGPIPE`.
- Instalação de manipulador construtor com `signal(SIGPIPE, SIG_IGN)` garantindo resiliência em escritas com descritor encerrado.

### Sincronização de Áudio e Vídeo & Compensação de Underrun (`extra/sdl/audio_trampoline.h`)
- Rastreamento por hardware de posição de reprodução via `SDL_GetAudioStreamQueued` e cálculo de `end_frame` sequencial.
- Callback contínuo de áudio `SDL_SetAudioStreamGetCallback` disparando evento de conclusão assincronamente na thread de áudio.
- Algoritmo de resincronização com amortecimento proporcional de underruns (`AC_AUDIO_RESYNC=1`) mantendo áudio e vídeo alinhados após stutters.

### Infraestrutura de QA e Auditoria Automatizada (`extra/tools/qa_runner.sh`, `audit.sh`, `AGENTS.md`)
- Pipeline de 4 estágios (Auditoria de marcadores/tamanho, Fast-Relink incremental, Smoke Test de runtime/input/saves/FPS e Análise Visual de Quadrantes/Flashes Verdes).
- Auditoria de marcadores estrita porém limpa: exige `// [extra]` apenas em hooks de 1 linha ou nas linhas inicial e final de regiões multi-linha.

### Launcher Qt6 Avançado & Ciclo de Vida do Processo (`extra/ui/launcher/`)
- Criação de `LogWidget` (`LogWidget.hpp`, `LogWidget.cpp`) inspirado no `BBPort`, com busca textual dinâmica, seleção de níveis (Todos, Erros/Avisos, FPS/Status), autoscroll, atalhos de cópia/exportação/pasta e gravação simultânea em disco (`userdata/logs/latest.log`).
- Isolamento de grupo de processos em `ProcessRunner` via `setpgid(0, 0)` no POSIX e encerramento em cascata via `SIGTERM` / `SIGKILL` prevenindo processos zumbis ou órfãos.
- Alternância dinâmica do botão principal para `⏹ PARAR` com estilo e glow carmim durante a partida, além de confirmação de encerramento seguro ao fechar a janela.
- Novas opções de controle de motor no `SettingsDialog` (`AC_VERBOSE=1` para depuração e `AC_CUTSCENE_FPS=30` para sincronia de cutscenes).

### Encerramento de Janela SDL & Sinais POSIX (`window_trampoline.h`, `platform_posix.h`, `play.sh`, `run_guarded.sh`)
- Tratamento ativo de `SDL_EVENT_WINDOW_CLOSE_REQUESTED`, `SDL_EVENT_QUIT` e atalhos de saída `Alt+F4` e `Ctrl+Q` com ocultação instantânea da janela (`SDL_HideWindow`), disparo de notificação de fechamento ao motor (`app_enqueue(4)`) e watchdog assíncrono para terminação limpa (`_exit(0)`).
- Registro de manipulador de sinais `SIGTERM` e `SIGINT` em `platform_posix.h` finalizando imediatamente todos os threads hóspedes via `_exit(0)` sem deadlocks em locks inativos.
- Substituição de chamadas de shell por `exec` em `play.sh`, `play-diagnostics.sh` e execução direta sem `timeout` em `run_guarded.sh`, garantindo que os sinais enviados pelo launcher cheguem diretamente ao executável `trace`.

### Sistema de Logs Discriminatórios e Categorização (`platform_posix.h`, `LogWidget`, `Theme.cpp`)
- Implementação de canais discriminatórios explícitos no runtime: `[audio]`, `[gpu]`, `[vulkan]`, `[video]`, `[shaders]`, `[backend]`, `[logic]`, `[input]`, `[app]`, `fps`, `STOP`, `FATAL`, `Error`, `CRASH`.
- Inclusão de seletor de categorias no `LogWidget` do Launcher Qt6 com colorização e filtragem em tempo real (Audio em ciano, GPU/Vulkan em violeta, Shaders em âmbar, Video em verde, Backend em laranja, Logic em azul celeste, Input em teal, Erros em vermelho).
- Configuração de buffer de linha via `setvbuf(stdout, NULL, _IOLBF, 0)` e `stdbuf -oL -eL` assegurando descarga imediata de logs mesmo durante interceptações do `timeout`.

### Rasterização de Prompts de Botões e Fontes POSIX (`extra/ui/font_trampoline.h`, `button_prompts.c`)
- Implementação de `extra_cut_text` substituindo a dependência Win32 GDI (`gdi32.dll`) por carregamento dinâmico do FreeType (`libfreetype.so.6`) com fallback matricial 8x8 integrado.
- Centralização horizontal e vertical de glifos nos keycaps (`[bx0, by0]..[bx1, by1]`) com corte nítido de opacidade e suavização por supersampling (SS=4).
- Suporte a seleção de prompts no Launcher (`AC_PROMPTS=auto`, `switch`, `keyboard`), permitindo alternar dinamicamente entre ícones originais do Nintendo Switch e teclas de PC (letras e botões de mouse).

### Tradução de Instruções Maxwell Compute e Correção de Iluminação (`extra/gpu/maxwell_extra.h`, `maxwell_ext.h`)
- Identificação da falha de tradução nos compute shaders `72ac59bb00` (`36d7008000173737`) e `72ac5a0900` (`18ff7f8400070709`) associados aos passes de culling e iluminação de superfícies dinâmicas.
- Mapeamento e implementação das instruções da família `LEA` na arquitetura NVIDIA Maxwell GM20B (`LEA` imm20 `0x36d0...`, `LEA.HI` cbuf `0x1800...`, `LEA` cbuf `0x4bd0...` e `LEA` reg `0x5bd0...`) com cálculos de deslocamento de 64-bit e compilação limpa em SPIR-V.
- Inclusão isolada em `extra/gpu/maxwell_extra.h` (30 linhas) através de hook único de 1 linha em `astral_chain/runtime/las/maxwell_ext.h`.

### Suporte a Command Wrapper e Variáveis Customizadas (`Config`, `SettingsDialog`, `run_guarded.sh`, `MainWindow`)
- Suporte nativo a wrappers de comando (`mangohud`, `gamemoderun`, `gamescope`, etc.) e variáveis de ambiente adicionais na aba Avançado do Launcher.
- Propagação via `--wrapper` no `run_guarded.sh` e injeção transparente das variáveis na cadeia de inicialização de processos do jogo.
- Teardown síncrono e encerramento seguro do grupo de processos no destrutor do `ProcessRunner` prevenindo processos órfãos ao fechar o launcher.

### Ícones Visuais de Botões no Painel de Controles (`extra/ui/launcher/src/ControlsDialog.cpp`)
- Identificação gráfica com ícones de botões e sticks na tabela de remapeamento de controles do Launcher 
- Preservação integral das chaves do arquivo `.ini` e compatibilidade total com o sistema de bindings upstream.

### Submissão Gráfica Assíncrona e Controle de Apresentação (`Config`, `SettingsDialog`, `AC_ASYNC_SUBMIT=1`, `LARECOMP_VK_PRESENT_MODE`)
- Exposição da opção de submissão gráfica assíncrona na aba Avançado do Launcher Qt6 com persistência em `userdata/launcher.ini`.
- Injeção de `AC_ASYNC_SUBMIT=1` no ambiente de lançamento, desacoplando a gravação de command buffers da execução, fences e apresentação Vulkan na GPU.
- Vinculação do ajuste de VSync ao modo de apresentação `LARECOMP_VK_PRESENT_MODE=fifo` (prevenindo que o modo `mailbox` consuma 85% de GPU em execuções sem pacing) e controle limpo de cópia dinâmica de vértices sem corrupção de atlas de texturas.

### Arquitetura de Memória Gráfica no Linux e Resolução Integral de HUD, Menus e Projeção de Quadrantes

#### 1. Divergência Arquitetural de Memória (Windows vs. Linux First)
- **Upstream Windows Original**: A arquitetura original do emulador NVN no Windows apoiava-se na API `GetWriteWatch(WRITE_WATCH_FLAG_RESET, ...)` do Win32. Através de `pt_init()` e `track_harvest()`, o runtime monitorava continuamente quais páginas de 4 KB do heap de 3 GB haviam sido gravadas pela CPU. Antes de cada submissão Vulkan, a rotina `gmem_sync()` executava o espelhamento incremental das páginas sujas (`stat_mirror_bytes`) para o buffer `gmem_buf` em VRAM.
- **Limitação no Linux Nativo**: No Linux, a API `GetWriteWatch` não existe nativamente, e a implementação de `track_harvest()` em `nvn_hle.h` era um no-op (`(void)lo; (void)hi;`), resultando em `pt_init() == false`. Adicionalmente, a tentativa de importação direta de ponteiro de host via extensão Vulkan (`VK_EXT_external_memory_host`) falhava no driver AMD RADV com o erro `-1000072003` (`VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR`) ao tentar fixar a alocação de ~2.85 GB. Consequentemente, o backend recorria obrigatoriamente a um espelho privado em VRAM (`gmem_mirror` / `gmem_alloc != NULL`).
- **Falha de Coerência de Memória**: Como `track_harvest()` não atualizava `track_gen`, a função `nvn_heap_mirror_dirty()` retornava 0 páginas após o boot inicial. O buffer `gmem_buf` em VRAM permanecia perpetuamente congelado com os dados da primeira amostragem, nunca refletindo as escritas dinâmicas subsequentes executadas pela CPU.

#### 2. Mecânica do Colapso de Projeção no Quadrante Inferior Direito (Tela de Título)
- **Ocorrência**: A tela de título (Frame 800) e os elementos de menu eram exibidos estritamente restritos ao quadrante inferior direito ($X \in [800, 1600], Y \in [450, 900]$), enquanto os outros 3 quadrantes permaneciam em escuridão total (`TL=0.0, TR=0.0, BL=0.0, BR=47.3`).
- **Causa Analítica**: O vertex shader de tela cheia `72ac5a8400_vs.glsl` lê as coordenadas de quad dinâmico a partir do buffer hóspede `0x7223a0dc00`. No Windows, essas coordenadas são calculadas a cada quadro pela CPU. No Linux, `guest_direct()` presumia `copy = 0`, vinculando o shader diretamente ao endereço congelado `0x7223a0dc00 - HEAP_BASE` dentro de `gmem_buf`. Como a área em `gmem_buf` continha zeros, o valor de atributo $W$ resultava em $0.0$, anulando a coluna de translação da matriz de projeção ortográfica e mapeando as coordenadas normalizadas de $[-1, 1]$ para o espaço de tela $[0, 1] \times [-1, 0]$, forçando o desenho no quadrante 4.

#### 3. Mecânica das Falhas de Menus e HUD
- **Sintomas**: Textos de caixas de diálogo e menus truncados ou ausentes, retículas de mira não atualizadas e indicadores gráficos vazios.
- **Causa Analítica**: Elementos dinâmicos de interface (fontes renderizadas via FreeType, layouts de HUD e janelas de menu) são alocados pelo jogo em pools circulares / heaps de memória scratch da CPU. Por compartilharem o mesmo heap hóspede sem write tracking, a leitura direta de `gmem_buf` lia buffers estagnados do quadro 1, corrompendo a geometria de cada glifo e elemento 2D.

#### 4. Solução: Fallback Adaptativo Cirúrgico (`min_direct` em `vcache.c`)
- **Princípio**: Não é necessário copiar gigabytes de geometria 3D estática para manter a interface funcional. Elementos dinâmicos de interface (quads, texto, HUD) possuem footprint volumétrico reduzido (sempre $\le 512$ KB por buffer), enquanto malhas de ambiente 3D, personagens e cenários estáticos possuem múltiplos megabytes e já foram devidamente sincronizados para VRAM no carregamento de fase.
- **Implementação**:
  - Em `vcache.c` (`guest_direct`), adicionada verificação de limiar adaptativo:
    ```c
    #ifndef _WIN32 // [extra]
        static uint64_t min_direct = 0;
        if (!min_direct) {
            const char *m = getenv("LARECOMP_VK_DIRECT_MIN_SIZE");
            min_direct = m ? (uint64_t)strtoull(m, NULL, 0) : (512ull * 1024);
        }
        if (size < min_direct) return false;
    #endif // [extra]
    ```
  - Buffers de UI, HUD e menus ($< 512$ KB) rejeitam `guest_direct()` e realizam fallback imediato para `guest_copy_vtx()`, lendo dados frescos da RAM do host.
  - Malhas estáticas pesadas ($\ge 512$ KB) continuam vinculadas em `gmem_buf` em VRAM, garantindo taxa sustentada de 60 FPS com zero consumo de PCIe.
  - O limiar pode ser ajustado em tempo de execução via variável de ambiente `LARECOMP_VK_DIRECT_MIN_SIZE` (padrão 524288 bytes).

#### 5. Desacoplamento do Cache de Vértices e Resolução de Upload Throttling (`gcopy.c`, `exec.c`, `draw.c`)
- **Problema de Throttling**: A ativação global de cópia de vértices (`LARECOMP_VK_COPY_VERTICES=1`) provocava queda brusca de performance para ~13 FPS com consumo brutal de 37 GB/s de PCIe na cena in-game (HQ).
- **Causa Raiz**: O módulo `gcopy.c` foi concebido para buffers de constantes (`cbuf` / uniform buffers). A rotina `guest_copy_flush()` continha uma chamada espúria a `guest_vcopy_flush()`. Como cada draw call in-game emite atualizações frequentes de uniform buffers (`CMD_UPDATE_UNIFORM_BUFFER`), a geração `vcopy_gen` era incrementada dezenas de vezes por quadro, invalidando todo o cache `vcopy` e forçando re-cópia integral de malhas em cada chamada de desenho.
- **Resoluções**:
  1. **Desacoplamento em `gcopy.c`**: Remoção de `guest_vcopy_flush()` de dentro de `guest_copy_flush()`. Atualizações de cbuf não mais afetam o cache de vértices.
  2. **Ciclo de Vida Correto em `exec.c`**: Inclusão de `guest_vcopy_flush()` exclusivamente em `vk_exec_reset()`.
  3. **Unificação de Chave de Cache em `draw.c`**: Para buffers dinâmicos $\le 512$ KB, a cópia utiliza o tamanho integral vinculado `v.size` em vez de um `need` variável derivado de `max_index`. Isso unifica a chave `(addr, size)` no hash de 64-bit de `vcopy.c`, elevando a taxa de acertos de cache para quase 100% em múltiplos draws do mesmo atlas de UI.

#### 6. Erradicação de Projeções de HUD no Chão 3D e Memoização de Texturas (`objects.c`, `vcopy.c`)
- **Problema**: Ícones e miras de HUD apareciam ocasionalmente texturizados sobre as geometrias do chão da delegacia.
- **Causas e Correções**:
  1. Em `vcopy.c`, a condição frouxa `vcopy[slot].size >= size` permitia que alocações subsequentes de scratch memory de tamanho menor reutilizassem slots pertencentes a draws de HUD. Corrigido com correspondência estrita `vcopy[slot].size == size` e inclusão de `size` no hash de 64 bits.
  2. Em `objects.c` (`register_texture`), a reconfiguração de entradas em `NvnDescPool` não invalidava a tabela de memoização de views de textura em `descriptors.c`. Adicionado `extern uint32_t vk_view_gen; vk_view_gen++; // [extra]`, forçando a reavaliação de descritores no Vulkan sempre que o jogo remapeia um índice de textura.

#### 7. Sincronização do Filtro de Estado Vulkan e Eliminação de Seams (`present.c`)
- **Problema**: Caixa retangular escura cortando o quadrante inferior direito da delegacia no frame 3200.
- **Causa**: `present.c` emitia `vkCmdSetScissor` manuais ao final do quadro para operações de apresentação e screenshot sem atualizar a estrutura `StateFilter ss`. No quadro seguinte, draws 3D acreditavam que o scissor cobria a tela toda e não o reemitiam, resultando no vazamento do scissor restrito.
- **Correção**: Remoção das chamadas manuais de scissor e adição de `ss_forget() // [extra]` nos pós-passes (`post_pass`, `vk_present_draw`, `renderer_save_texture_tex`), restabelecendo a coerência de estado entre o motor e o command buffer Vulkan.

#### 8. Pacing Dinâmico e Controle de Taxa de Quadros (`extra/platform/pace_trampoline.h`, `AC_FPS`, `Config`, `SettingsDialog`)
- **Problema**: Alterar o Target FPS no launcher (para cima ou para baixo de 60) não surtia efeito: a taxa de quadros permanecia fixa em 60 FPS mesmo em modo imediato (`Present Mode: IMMEDIATE`).
- **Causas**:
  1. A rotina de apresentação `nvn_present_pace` em `astral_chain/runtime/gpu/nvn_hle.h` utilizava uma grade rígida de 60 Hz (`period = 320000` ticks @ 19.2 MHz) que forçava cada quadro a esperar pelo menos 1 slot de 1/60s.
  2. A rotina `Config::buildLaunchEnv()` injetava `AC_PRESENT_INTERVAL=1` de forma fixa e não propagava a propriedade `m_graphics.fpsLimit`.
- **Resoluções**:
  1. Criação do trampoline desacoplado `extra/platform/pace_trampoline.h` com leitura de `AC_FPS` / `AC_TARGET_FPS` e cálculo dinâmico do período de quadro (`19200000 / current_fps`), suportando taxas arbitrárias (30..240 FPS) ou desativação de pacing (`AC_FPS=0`).
  2. Preservação da taxa fixa de 30 FPS em cutscenes quando `AC_CUTSCENE_FPS=30` está ativado, com reinício limpo de cadência (`prev_period != period`).
  3. Atualização de `Config::buildLaunchEnv()` para exportar `AC_FPS=<fpsLimit>` (e `AC_PRESENT_INTERVAL=0` quando 0).
  4. Extensão do controle de Target FPS em `SettingsDialog` para permitir `0` (exibindo "0 (Ilimitado)").

#### 9. Modos de Apresentação Vulkan no Launcher e Runtime (`swapchain.c`, `run_guarded.sh`, `SettingsDialog`, `Config`)
- **Problema**: O launcher não oferecia os modos de apresentação de forma consistente, `run_guarded.sh:64` forçava `LARECOMP_VK_PRESENT="fifo"`, conflitando com o padrão do emulador (`mailbox`) e introduzindo fence waits e latência no compositor Wayland/X11. Modos como `FIFO Relaxed` (VSync Adaptativo) não eram suportados no runtime.
- **Resoluções**:
  1. Suporte a `VK_PRESENT_MODE_FIFO_RELAXED_KHR` em `astral_chain/runtime/las/nvn/vulkan/device/swapchain.c` (`fifo_relaxed` e `fifo-relaxed`).
  2. Atualização de `extra/ui/launcher/src/SettingsDialog.cpp` expondo claramente os modos de apresentação:
     - `Mailbox (Sem Tearing / Baixa Latência - Recomendado)` (`mailbox`)
     - `FIFO (VSync Estrito / 60 Hz Travado)` (`fifo`)
     - `FIFO Relaxed (VSync Adaptativo / Sem Stutter)` (`fifo_relaxed`)
     - `Immediate (Sem Sincronização / Menor Latência)` (`immediate`)
  3. Sincronização entre a caixa de seleção de VSync e o modo de apresentação, e propagação direta de `LARECOMP_VK_PRESENT=<mode>` em `Config::buildLaunchEnv()`.
  4. Alinhamento de `astral_chain/tools/run_guarded.sh` para usar `mailbox` como padrão.

#### 10. Refatoração de Heap Mirror e Eliminação de Cópias Inúteis de Compute (`extra/gpu/heap_mirror_extra.h`, `heap_mirror.c`, `platform_posix.h`, `pace_trampoline.h`)
- **Problema**: Telemetria registrava `write-back 3939 MB in 2226` a cada 2 segundos (~2 GB/s de tráfego de PCIe contínuo e `memcpy` na CPU). Toda submissão com shaders compute (estágio 5) gravando em storage buffers (listas de agrupamento de luzes de 9.6 MB e 16.8 MB, culling) alocava staging buffers via `vmaCreateBuffer` e copiava esses dados de volta para a memória guest, mesmo que o CPU do jogo nunca lesse esses dados (eles são consumidos exclusivamente por fragment shaders na GPU). Adicionalmente, `Sleep(0)` mapeado para `usleep(0)` em `platform_posix.h` girava sem ceder CPU, queimando mais de 2 núcleos no pacing.
- **Resoluções**:
  1. **Constantes Arquiteturais e Anotações Válidas (`extra/gpu/heap_mirror_extra.h`)**:
     - Definição de constantes limpas (`EXTRA_PAGE_SIZE_4K`, `EXTRA_PAGE_MASK_4K`, `EXTRA_PAGE_SHIFT_4K`, `EXTRA_MAX_DIRTY_PAGES`, `EXTRA_MAX_WB_PENDING`, `EXTRA_MAX_WB_FLIGHT`, etc.), eliminando números mágicos espalhados.
     - Documentação técnica detalhada sobre o pipeline de espelhamento e controle de coerência.
  2. **Eliminação de Cópias Redundantes (`extra_wb_should_skip`, `LARECOMP_VK_WB`)**:
     - Implementação de filtro ativo em `gmem_wb_note`: buffers de compute $\ge 1\text{ MB}$ (como as listas de luzes de 9.6 MB e 16.8 MB) são ignorados para write-back, pois seus dados já residem no buffer `gmem_buf` da GPU.
     - Redução medida do volume de write-back de **3.939 MB para 9-11 MB a cada 2 s** (**redução de 99.72%**), eliminando ~2 GB/s de PCIe e CPU memcpy.
     - Suporte a controle dinâmico via `LARECOMP_VK_WB` (`0`/`none`/`off` para desativação total, `all`/`legacy` para comportamento legado) e `LARECOMP_VK_WB_THRESHOLD`.
  3. **Fast-Path Sem Contenção de Lock (`gmem_writeback_apply`)**:
     - Verificação antecipada com `extra_wb_has_work`, evitando aquisição desnecessária de `lazy_lock` em submissões sem write-back pendente.
  4. **Semântica POSIX de Espera e Hybrid Pacing (`platform_posix.h`, `pace_trampoline.h`)**:
     - Correção de `Sleep(0)` para `sched_yield()` e `Sleep(ms>0)` para `nanosleep` bloqueante real em `extra/platform/platform_posix.h`.
     - Implementação de hybrid wait em `extra/platform/pace_trampoline.h`, dormindo via `nanosleep` quando restam $> 300\,\mu\text{s}$ e aplicando spin curto nos últimos microssegundos para precisão sub-milissegundo, recuperando mais de 2 núcleos de CPU.

#### 11. Eliminação de Spin-Waits e Recuperação de Núcleos de CPU (`extra/platform/time_trampoline.h`, `nvn_hle.h`, `host_runtime.c.in`, `platform_posix.h`)
- **Problema**: O diagnóstico de desempenho (`docs/perf-review-2026-10-31.md`, Seção 4 / P1) constatou que threads de trabalho hóspedes e caminhos de sincronização consumiam entre 6.7 e 8.5 núcleos para 60 FPS, com múltiplas threads cravadas em 1.00 núcleo e 45% de tempo em modo kernel (`sys`), devido a laços de busy-wait com `sched_yield` em `plat_sleep_until_ns` e `svcSleepThread` (`imm == 0xb`).
- **Resoluções**:
  1. **Rotinas de Espera Híbrida (`extra/platform/time_trampoline.h`)**:
     - `extra_plat_sleep_until_ns(uint64_t t)`: Implementação de espera em tempo real baseada em `CLOCK_MONOTONIC` com `TIMER_ABSTIME`. Bloqueia o processo no kernel via `clock_nanosleep` para durações $> 250\,\mu\text{s}$, e aplica `sched_yield()` apenas nos últimos microssegundos restantes para preservar precisão sub-milissegundo de pacing.
     - `extra_svc_sleep_thread(int64_t ns)`: Substituição do loop Win32 legado em `rt_svc` (`imm == 0xb`) por espera híbrida de alta resolução em nanosegundos, permitindo que threads de trabalho em polling descansem sem consumir núcleos inteiros.
  2. **Tratamento de Sinais em POSIX Sleep (`extra/platform/platform_posix.h`)**:
     - Atualização de `extra_sleep_ms` para reiniciar o sono sob interrupção de sinais (`EINTR`), garantindo que esperas maiores que 0 ms sejam sempre bloqueantes.
  3. **Conexão Cirúrgica no Runtime Herdado**:
     - Conexão de `plat_sleep_until_ns` em `nvn_hle.h` via hook de 1 linha (`extra_plat_sleep_until_ns`).
     - Conexão de `imm == 0xb` em `host_runtime.c.in` (e `trace.c`) via hook de 1 linha (`extra_svc_sleep_thread`).
  4. **Resultados e Métricas Medidas**:
     - Tempo de sistema (`sys`) despencou de ~2.83 para **0.28**.
     - As threads de trabalho deixaram de marcar 1.00 core contínuo, reduzindo o consumo agregado de CPU de ~6.7–8.5 núcleos para **~1.36–2.5 núcleos**.
     - Eliminação da contenção no `klock` do runtime, estabilizando a métrica `p95` em **~26 ms** e `p99` em **~28 ms**.

#### 12. Rastreamento Nativo de Escrita de Páginas e Guard de Vértices/Iluminação (`extra/gpu/write_tracking_extra.h`, `extra/gpu/vtx_debug_extra.h`, `nvn_hle.h`, `draw.c`)
- **Problema**: O diagnóstico de desempenho (`docs/perf-review-2026-10-31.md`, Seção 4 / P2) identificou que a ausência de write tracking nativo no Linux (`[vk] no page write tracking`) desativava completamente o cache de vértices (`vcache`) e forçava 1220 texturas por frame a recalcular hashes na CPU (`untracked 1220`), degradando o PCIe e aumentando o custo de transferência em runtime. Além disso, variações em coordenadas de vértices podiam gerar explosões visuais ou distorções de iluminação.
- **Resoluções**:
  1. **Page Write Tracking de Alto Desempenho (`extra/gpu/write_tracking_extra.h`, `nvn_hle.h`)**:
     - Implementação nativa Linux via `userfaultfd` assíncrono em espaço de usuário (`UFFD_USER_MODE_ONLY | UFFD_FEATURE_WP_ASYNC`, Linux 6.7+) em conjunto com o `ioctl(PAGEMAP_SCAN)` e a flag `PM_SCAN_WP_MATCHING` (`PAGE_IS_WRITTEN`).
     - A descoberta e a reativação de proteção contra escrita ocorrem no mesmo ioctl de forma atômica por span/intervalo (`[lo, hi)`), eliminando o custo proibitivo de varreduras globais de VMA do processo. O tempo por consulta foi de ~1000–2000 µs (em `clear_refs`) para **~1.8 µs por consulta** (~9 ms totais para quase 5.000 consultas).
     - Ativação bem-sucedida de `pt_init()` no backend NVN: a contagem de texturas não rastreadas caiu de **1.220 para 0** (`tex untracked 0`), habilitando o pipeline de cache de vértices (`vcache`) e eliminando o re-hashing desnecessário na CPU.
     - Fallbacks graduais e seguros para kernels legados via `PAGE_IS_SOFT_DIRTY` (com throttling de `clear_refs` limitado a no máximo 1x por 16ms) e `pread` de `/proc/self/pagemap`.
  2. **Depurador e Guardião de Vértices e Iluminação (`extra/gpu/vtx_debug_extra.h`, `draw.c`)**:
     - `extra_vtx_check_stream`: Inspeção de fluxos de atributos de posição em draw calls para detectar coordenadas anômalas (NaN, $\pm\infty$ ou valores com magnitude $> 50.000,0$). Em modo de proteção (`LARECOMP_GUARD_VTX=1`), grampeia as coordenadas explosivas para zero, prevenindo vértices projetados para o infinito.
     - `extra_light_check_cbuf`: Validação de multiplicadores de iluminação em constant buffers para prevenir superexposição visual ou blackouts na renderização.
     - Conexão cirúrgica em `astral_chain/runtime/las/nvn/vulkan/exec/commands/draw.c`.
     - **Correção de ordenação (2026-11-01)**: o hook unificado original executava a inspeção de vértices **após** o loop que copia/vincula os streams (`guest_copy_vtx` para buffers ≤ 512 KB), de modo que o grampeamento caía na RAM do hóspede tarde demais — a cópia Vulkan já havia sido feita com o dado explosivo e a guarda era ineficaz exatamente no caminho dos buffers dinâmicos. O hook foi dividido: `extra_vtx_guard_stream` executa **antes** do loop de streams (resolvendo a varredura de índices adiada somente quando a flag está ativa, para o `max_index` estar correto) e `extra_vtx_guard_cbuf` permanece antes da montagem de descritores, cuja cópia de cbuf ocorre mais tarde (em `stage_set`) e portanto recebe o valor grampeado.
  3. **Eliminação de Números Mágicos em Write Tracking e Temporização (`nvn_hle.h`)**:
     - Definição de constantes arquiteturais autodescritivas: `NVN_NS_PER_SEC` (1.000.000.000ULL), `NVN_PAGE_SIZE` (4096ULL), `NVN_PAGE_MASK` (4095ULL), `NVN_PAGE_SHIFT` (12ULL), `NVN_TRACK_MAX_PAGES` (65536ULL), `NVN_TRACK_CHUNK_SIZE` (256 MB), `NVN_TRACK_MAX_RANGES` (8192) e `NVN_TRACK_COALESCE_GAP_PAGES` (2048ULL).
     - Substituição de todos os literais numéricos mágicos espalhados em `plat_now_ns`, `track_harvest`, `written_pages`, `plat_write_harvest_u` e nas rotinas `nvn_heap_mirror_*`, padronizando também alocações com `sizeof(uint32_t)`.



#### 13. Menu ImGui In-Game (Insert) — Overlay Gráfico e de Debug (`extra/ui/overlay/`, `third_party/imgui/`, `extra/toggles/ac_toggles.*`)
- **Objetivo**: Trazer o padrão de portes modernos (TOTK recomp, bbport) para o AstralChain-PC: menu in-game com opções gráficas e de debug ao vivo, sem depender de edição de `launcher.ini` e relaunch.
- **Implementação**:
  1. **Vendor `third_party/imgui/`**: Dear ImGui 1.92.9b (MIT, `LICENSE.txt`) — núcleo + backends `imgui_impl_sdl3` e `imgui_impl_vulkan` (dynamic rendering). Sem `imgui_demo`.
  2. **`extra/ui/overlay/`**: `ac_overlay.h` (ABI C), `ac_overlay.cpp` (contexto/backends SDL3+Vulkan, toggle Insert, eventos), `ac_vk_glue.cpp` (init do backend no `g_vk`, adaptado do `imgui_glue.cpp` upstream; `PipelineInfoMain` do 1.92.x) e `ac_menu.cpp` (seções Performance/Graphics/Debug/About). Desenho ocorre dentro do dynamic rendering da imagem da janela, entre `vk_present_draw` e `vk_frame_end` — zero passes extras de Vulkan.
  3. **`extra/toggles/ac_toggles.*`**: globais de override (`-1`/`0` = env é o padrão): guard/debug vtx, alvo de FPS, kill switch de async submit e present mode. Ambiente continua sendo a fonte da verdade a cada launch; o menu é override puro em memória (sem persistência no v1).
  4. **Build (`astral_chain/nvn_build.py`)**: fontes imgui/overlay entram no grafo de objetos com nomes planos (`imgui_*`, `extra_*`) e chave de cache própria; os TUs que incluem SDL3 real recebem `/usr/include` via `-isystem` antes (rebaixando `las/shim`, que só tem protótipos de `SDL_GetTicksNS`).
- **Opções ao vivo**: Antialiasing (Off/SMAA), Upscaler (Off/FSR1) via `g_settings`; Limite de FPS (30–165) via `g_extra_fps_override` (o pacer re-ancora na troca de período); VSync/present mode (mailbox/fifo/fifo_relaxed/immediate) via `g_extra_present_mode` + recriação de swapchain **adiada** para o pré-frame (`ac_menu_apply_pending`, nunca dentro do rendering ativo); Vertex guard/debug (checkboxes), Async submit (kill switch one-way com `nvn_async_drain`; religar exige relaunch porque o worker nasce no boot), GPU profiler e dump de debug frame (F9).
- **Supressão de input**: com o menu aberto, eventos de teclado/mouse/gamepad deixam de alimentar o jogo (`window_trampoline.h`) e o mouse sai do modo relativo; ao fechar, o modo retorna conforme o foco.
- **Verificação visual headless**: `AC_OVERLAY=1` inicia com o menu aberto e `LARECOMP_WINDOW_SHOT=<frame>` salva a imagem da janela (incluindo a overlay) — `LARECOMP_SHOT_AT` salva apenas a textura do jogo e **não** serve para validar a overlay.
- **Atualização (2026-11-01)**: hotkey trocada de F1 para **Insert** (mesmo padrão do bbport) e tema **Adwaita Dark** (libadwaita/GTK4) com o acento teal do projeto mantido nos elementos interativos (`ac_menu_apply_style` em `extra/ui/overlay/ac_menu.cpp`).

#### 14. Opções Gráficas Externas — Qualidade de Textura, LoD e Sombras (`extra/toggles/ac_toggles.*`, `extra/ui/overlay/`, `extra/ui/launcher/`)
- **Objetivo**: expor fora dos menus do jogo (que não têm opções gráficas — ver `docs/reverse-engineering-pc.md` §7) o controle de **qualidade de textura**, **nível de LoD** e **sombras**, editáveis ao vivo no menu ImGui e persistidos no launcher Qt6.
- **Mecanismo**: as três opções agem na montagem de samplers Vulkan (`textures.c` `vk_sampler`), a mesma camada que já traduz NVN→Vulkan. Nenhuma alteração no código ARM64 do jogo.
  1. **Qualidade de textura** = bias de LOD global (`si.mipLodBias = d->lod_bias + bias`). Bias positivo empurra a amostragem para mips menores (cada +1 ≈ metade da resolução), reduzindo a leitura de textura.
  2. **Nível de LoD** = piso `minLod` (`si.minLod = fmaxf(si.minLod, lod_min)`), reaproveitando o `LARECOMP_LOD_MIN` já existente, agora com override ao vivo.
  3. **Sombras** = `compareOp = VK_COMPARE_OP_ALWAYS` quando desligado, reaproveitando o `LARECOMP_NO_SHADOW` já existente.
- **Contrato de override**: globais `g_extra_lod_bias` / `g_extra_lod_min` / `g_extra_no_shadow` seguem o padrão `-1` = "cai para a env" (mesmo contrato de `g_extra_vtx_guard`). O menu ImGui e o launcher escrevem sobre a env `LARECOMP_LOD_BIAS` (nova) / `LARECOMP_LOD_MIN` / `LARECOMP_NO_SHADOW`.
- **Launcher**: `GraphicsSettings` ganhou `lodBias` / `lodMin` / `noShadow` (persistidos em `launcher.ini`), expostos na aba Graphics como combos de presets ("Qualidade de Textura" 0–3, "Nível de LoD" 0–4) e checkbox ("Desativar Sombras"); `buildLaunchEnv()` emite as envs correspondentes.
- **ImGui**: seção Graphics ganhou sliders "Texturas (bias LOD)" 0–3 e "LoD mínimo" 0–4, e checkbox "Sombras".
- **Nota**: motion blur permanece fora de escopo (pass de shader do próprio jogo, exige `LARECOMP_SKIP_FSHASH` com hash identificado em runtime — ver §7.6 do doc de RE).

#### 15. RE Estática do Binário — UI, Estado, Pacing e Slots NVN (`docs/reverse-engineering-ui-perf.md`)
- **Objetivo**: consolidar, a partir de análise **somente estática** do `main` NSO v1.0.1, os endereços concretos que sustentam patches gráficos com opções e melhoria de desempenho, sem executar o jogo.
- **Achados-chave**:
  1. **Sem flag global "menu aberto"** — estado de UI vive em objetos com vtable; o campo `[this+0x8C]` em `fn 0x625e60` é o "item selecionado" (alimenta o dispatch do cursor em jump-table `0xbe9190`, 8 casos).
  2. **Opções do usuário = reflexão BXM**: padrão `(nome→índice)[vtable+0x138]` / `(índice→valor)[vtable+0x170]`, registrador-mestre `fn 0x8ad5f0` (`SettingParamIntArgs`, `PostAdjustType`, `*FrameRateType` — estes últimos são chaves de serialização, não opção de FPS).
  3. **Resolução (pendência §7.7 resolvida)**: `1920/1080` em struct `.data` `0xd87e74` (backbuffer lógico) e o piso de filtros `1280/720` em `fn 0x3fe260`; patch de internal-resolution exige tocar os dois.
  4. **Cadeia de pós-processamento**: `Filter01..Filter09` (04/08 ausentes) têm 2 xrefs cada, todos em `fn 0x3fe260` → dispatch estático em tabela construída no boot.
  5. **Resolução NVN por nome**: só `nvnBootstrapLoader` é import dinâmico; as outras 512 `nvn*` são resolvidas em boot (`fn 0x9ee4f0..0x9ef8xx`) para slots em `0xdc2xxx`/`0xdc3xxx` (mapa em `/tmp/opencode/re/nvn_slots.json`). Slots úteis: `nvnTextureBuilderSetSamples`→`0xdc2e80`, `nvnSamplerBuilderSetLodBias`→`0xdc3530`, `nvnWindow*PresentInterval`→`0xdc3278/0xdc2ec0/0xdc3270`.
  6. **Pacing (Dynamic-FPS v1.1.1)**: struct global em `0xdb89a0` (`+0xD0` step float, `+0xD4` remainder, `+0xD8` whole ticks, `+0xDC` frames/ms, `+0xE4` avg count); overrides de cap em `0x9E8D84`/`0x9DB6BC`; consumidores anti-truncamento em `0x48BE14`/`0x57E7DC`/cave `0xA0E7D0`.
- **Escopo**: documentação nova em `docs/`; nenhum código tocado; análise feita por scanners em `/tmp/opencode` (não versionados).

#### 16. Vendorização do SMAA 1x — Antialiasing funcional (`third_party/smaa/`, `nvn/smaa_data.c`)
- **Problema**: o combo "Antialiasing" do menu ImGui (entrada #13) expõe `"Off\0SMAA\0"`, mas o SMAA não estava vendido — os stubs em `game_hooks_neutral.c` chamavam `FATAL("SMAA data not vendored")` e o shader era `#error`, então selecionar SMAA crashava o jogo imediatamente.
- **Resolução**: vendorização do SMAA 1x de referência (iryoku/smaa, MIT) com o mesmo layout do LASRecomp:
  1. **`third_party/smaa/`**: `SMAA.hlsl` (fonte verbatim, 1361 linhas), `LICENSE.txt` (MIT) e `README.md` (origem + artefatos gerados).
  2. **`nvn/smaa_data.c`** (glue) expõe os três símbolos consumidos pelo backend Vulkan: `smaa_hlsl_src` (o `SMAA.hlsl` como string C), `smaa_area_tex()` (160×560 R8G8) e `smaa_search_tex()` (64×16 R8). Inclui `AreaTex.h`, `SearchTex.h` e `smaa_hlsl.inc` do mesmo diretório.
  3. **Artefatos gerados** (`AreaTex.h`, `SearchTex.h`, `smaa_hlsl.inc`): `SMAA.hlsl` convertido para string C escapada (CRLF→LF, `\`→`\\`, `"`→`\"`, byte não-ASCII `0xb7`→`\267` octal); texturas de lookup do diretório `Textures/` upstream.
  4. **`game_hooks_neutral.c`**: stubs removidos (uma linha de nota apontando para `smaa_data.c`).
  5. **`nvn_build.py`**: `nvn/smaa_data.c` adicionado ao `VULKAN_LAYER` e sufixo `.inc` incluído na lista de entradas rastreadas (cache de objetos).
- **Consumo (inalterado)**: `shaders.c` monta o fonte GLSL como `#version 450` + push constant (`SMAA_RT_METRICS pc.a`) + `SMAA_GLSL_4`/`SMAA_PRESET_HIGH` + entry points por pass (`smaa_edge.frag`/`smaa_weight.frag`/`smaa_blend.frag`); `present.c` envia as texturas de lookup como R8G8/R8. O modo `SMAA_GLSL_4` mapeia `SMAATexture2D`→`sampler2D`, casando com os bindings `t0`/`t1`/`t2` declarados.
- **Verificação**: compilação isolada de `smaa_data.c` limpa (`cc -c`); sem símbolo `smaa_*` duplicado (só `smaa_data.c` define; `shaders.c`/`present.c` usam `extern`).

#### 17. Botões Animados Estilo Astral Chain e Tema #3b3a4a no Launcher Qt6 (`extra/ui/launcher/`)
- **Objetivo**: modernizar os botões do launcher Qt6 (`CustomButton`) implementando a linguagem visual dos menus de *Astral Chain* (Nintendo Switch), incluindo animação de deslocamento suave no hover, moldura externa retangular com pulso de opacidade contínuo, sombra projetada e cantos em colchete estilo visor IRIS da Neuron, além de uniformizar o fundo do launcher e diálogos para a cor `#3b3a4a`.
- **Implementação**:
  1. **`CustomButton` (`extra/ui/launcher/include/CustomButton.hpp` e `src/CustomButton.cpp`)**:
     - Widget customizado encapsulando `QPushButton` com `setAttribute(Qt::WA_TranslucentBackground)`.
     - Animação de posição com `QPropertyAnimation` (160ms, curva `QEasingCurve::OutCubic`) deslocando o botão para a esquerda no hover.
     - Animação de pulso da moldura com `QPropertyAnimation` (900ms em loop, `QEasingCurve::InOutSine`, amplitude de opacidade 0.35 a 1.0).
     - Pintura vetorial em `paintEvent`: sombra projetada (`QColor(8, 10, 18)` offset horizontal +4px, vertical +5px), moldura retangular com espessura 1.8 e cantos de colchetes cibernéticos em ciano elétrico (`#00e5ff`).
     - Suporte a modo `isDanger`: quando o jogo está em execução, o botão Play se transforma em Stop com estilização vermelha de alerta (`#c62828` / `#ffebee`) e moldura pulsante carmesim.
     - Dimensionamento dinâmico e responsivo em `resizeEvent` adaptando a largura do botão à largura útil do painel lateral.
     - Detecção robusta de hover/leave com debounce via `m_leaveTimer` (35ms) e sincronização CSS da propriedade `hovered="true"`.
  2. **`Theme.cpp` e `MainWindow.cpp`**:
     - Fundo geral do launcher definido para `background-color: #3b3a4a;` em `QMainWindow`, `QWidget#centralWidget` e diálogos.
     - `MainWindow::paintEvent` preenche com a cor sólida `QColor("#3b3a4a")`.
     - Remoção de estilos redundantes legados e harmonização de elementos de interface (painel lateral, barra de progresso, abas e tabelas).
  3. **`MainWindowUi.cpp` e `MainWindow.cpp`**:
     - Substituição dos botões do painel lateral (`m_playBtn`, `m_diagBtn`, `m_setupBtn`, `m_shadersBtn`, `m_settingsBtn`, `m_controlsBtn`) por `CustomButton`.
     - Conexões de clique mapeadas diretamente ao sinal `&CustomButton::clicked`.
     - Remoção da função e botão Switch Art (`m_artBtn`, `onNextArt`, `loadArtList`, `m_artTimer`), eliminando assets de imagem em `launcher/art/`.
  4. **`LogWidget.cpp`**:
     - Divisão da barra de ferramentas do log em duas barras dedicadas: superior (status, busca e filtro de canais) e inferior (checkbox de autoscroll e botões de ação), eliminando sobreposição visual e truncamento de texto.
- **Verificação**: `./extra/tools/audit.sh` executado com sucesso (todos os arquivos <= 300 linhas, compilação limpa via CMake/Ninja e teste offscreen sem falhas).

#### 18. Modularização de `host_runtime.c.in` e Eliminação de Números Mágicos no Kernel Horizon (`kernel/`, `services/`, `horizon_defs.h`)
- **Problema**: O arquivo herdado `astral_chain/runtime/host_runtime.c.in` era um monólito de 1.389 linhas misturando inicialização de SO, gerenciamento de heap/páginas, tabelas de sessões IPC, despacho de centenas de comandos CMIF e o dispatcher de chamadas de sistema (SVCs). Além disso, a lógica de compensação de cutscenes injetada via patches continha dezenas de números mágicos sem qualquer documentação ou constantes explicativas (`19200000.0`, `9000.0f`, `6.0f`, `30.0f`, `7.0f`, etc.), prejudicando a manutenção e a legibilidade do código.
- **Resoluções**:
  1. **Separação Modular da Arquitetura Horizon**:
     - `astral_chain/runtime/kernel/horizon_defs.h`: Definições globais do Horizon OS (enums de SVCs, faixas de Handles, resultados de Kernel, Memory Types/Permissions, CMIF protocols e constantes de temporização do hardware Switch).
     - `astral_chain/runtime/kernel/heap_memory.h`: Gerenciamento de heap, alocação virtual, proteção de páginas (Vulkan lazy write-back) e dumps de diagnóstico (`STOP_DUMP`).
     - `astral_chain/runtime/services/ipc_session.h`: Enumeração de tipos de serviço (`SESSION_*`), descritores de sessões e empacotamento de requisições IPC/CMIF.
     - `astral_chain/runtime/services/ipc_dispatch.h`: Despacho e ciclo de vida de chamadas IPC (`ipc_svc`).
     - `astral_chain/runtime/kernel/svc_dispatch.h`: Dispatcher central de chamadas de sistema (`rt_svc`, `rt_svc_body`) e pacing dinâmico de cutscenes desacoplado.
     - Redução de `host_runtime.c.in` de 1.389 linhas para apenas **~220 linhas**.
  2. **Eliminação de Números Mágicos e Documentação Autodescritiva**:
     - `HORIZON_SYSTEM_TICK_FREQ` (19200000.0) / `HORIZON_SYSTEM_TICK_FREQ_U64` (19200000ULL): Frequência do contador genérico do ARM Cortex-A57 / Tegra X1 da Nintendo Switch (`CNTFRQ_EL0`, 19.2 MHz / 1 tick $\approx$ 52.083 ns). Implementação de `horizon_ticks_to_seconds(ticks)`.
     - `CUTSCENE_BASE_FPS` (30.0f) e `CUTSCENE_FRAME_MS_BASE` (1000.0f / 30.0f $\approx$ 33.333 ms): Cadência base fixa de renderização das animações de cutscenes de *Astral Chain*.
     - `CUTSCENE_STALL_THRESHOLD_STEPS` (6.0f): Limite de alerta de travamento (6 frames a 30 FPS $\approx$ 200 ms), limiar onde o engasgo se torna perceptível e gera dessincronização de áudio/vídeo.
     - `CUTSCENE_MAX_SANITY_STEPS` (9000.0f): Teto de sanidade (9.000 frames a 30 FPS = 300 segundos / 5 minutos) para descartar transições de cena, telas de carregamento ou floats não inicializados no registrador de medição.
     - `CUTSCENE_MAX_CATCHUP_PER_FRAME` (7.0f), `CUTSCENE_MIN_DEBT_LIMIT` (-2.0f), `CUTSCENE_DEFAULT_STEP` (1.0f), `CUTSCENE_MAX_MEASURED_STEP` (30.0f) e `CUTSCENE_TRACK_SLOT_COUNT` (64): Controle de recuperação e amortecimento de dívida temporal na reprodução das faixas de animação.
     - `SVC_LOCK_WARNING_THRESHOLD_MS` (30.0 ms): Limite de alerta de tempo de execução sob o lock de kernel.
     - Constantes de mapa de memória virtual: `HORIZON_ALIAS_REGION_ADDRESS` (0x4000000000ULL), `HORIZON_ALIAS_REGION_SIZE` (0x1000000000ULL), `HORIZON_STACK_REGION_SIZE` (0x100000000ULL).
  3. **Legibilidade e Estrutura Sem Condensação Artificial**:
     - Funções `handle_cutscene_pace(Cpu *c)` e `handle_cutscene_carry(Cpu *c)` extraídas com nomenclatura expressiva, espaçamento generoso e formatação não-exaustiva.
- **Verificação**:
  - `./extra/tools/audit.sh`: 100% aprovado em todas as 7 etapas.
  - Fast-Relink via `setup_game --rebuild-if-stale`: compilado e linkado em ~12 segundos.
  - `qa_runner.sh`: 100% de sucesso.

#### 19. Janela RE Info (F8) — Estado ao Vivo e Tabelas id→nome (`extra/ui/overlay/ac_reinfo.*`, `extra/tools/reinfo_tables.py`)
- **Objetivo**: expor em jogo, numa janela ImGui separada do menu Insert, todos os achados de engenharia reversa estática (área, missão, boss, playtime, dificuldade, slots, chaves de texto e tabelas id→nome) sem recompilar nem reiniciar a sessão.
- **Implementação**:
  1. **`extra/ui/overlay/ac_reinfo.cpp` / `.h`**: janela própria com toggle **F8** e visibilidade independente do menu Insert; inicia fechada (`AC_REINFO=1` abre, mesmo contrato do `AC_OVERLAY`). Seções: estado ao vivo (três colunas campo/id/nome), slots globais crus e tabelas completas com filtro de texto. Fecha/abre via `ac_reinfo_visible`/`ac_reinfo_toggle` (ABI C).
  2. **`extra/ui/overlay/ac_reinfo_probe.c`**: shim C de 16 linhas que expõe `ac_reinfo_main_base/guest_lo/guest_hi` (`AC_MAIN_BASE` + `GUEST_LO`/`GUEST_HI` de `rt.h`), mantendo o header do runtime ARM64 fora do TU C++ que inclui ImGui.
  3. **Leitura somente leitura e segura**: cada slot é uma dupla desreferência (slot → bloco → valor) com checagem de faixa `GUEST_LO..GUEST_HI` e de alinhamento; slot nulo ou fora do guest vira `(n/d)` em vez de derrubar o jogo.
  4. **`extra/ui/overlay/ac_reinfo_tables.h`**: tabelas `id → nome` embutidas (22 áreas, 71 missões, 82 ids de boss), sem ler `romfs.bin` em runtime.
  5. **`extra/tools/reinfo_tables.py`**: gerador versionado que lê `game/romfs.bin → /Text/Text.pkz`, descomprime com `zstandard` (fallback ao binário `zstd`), resolve os registros PTD por hash e escreve o header. Não entra no build — rodar após atualizar o RomFS.
- **Bug corrigido (filtro das tabelas não aceitava digitação)**: o buffer do `InputTextWithHint` era um `char[32]` local de `table_section`, zerado a cada frame — o ImGui edita o mesmo buffer entre frames, então o texto nunca persistia. Os três buffers (`f_areas`/`f_missions`/`f_bosses`) agora são `static` em `ac_reinfo_frame` e passados por parâmetro; `PushID(label)`/`PopID()` isola o estado de cada caixa.
- **Bug corrigido (extração de nomes de boss)**: o arquivo mantém cópias da tabela com **só os hashes** e strings vazias (`0xa448`); a tabela real de valores fica em `0x988a`, um offset **2-alinhado**. A varredura antiga com passo 4 pulava a tabela boa e produzia 50/75 acertos em `-----`. O gerador agora varre com passo 2 e resolve cada hash individualmente.
- **Detalhes de apresentação**: quebras de linha duras do texto do jogo (`ASTRAEUS-\r\nCAPANEUS`) são colapsadas; letras gregas (α/β/γ/Δ, usadas pelos Homunculus) são transliteradas porque o overlay usa a fonte default do ImGui (Latin-1) — `// shortcut` no gerador, revisar quando houver fonte com cobertura grega.
- **Integração**: `astral_chain/gpu/nvn_build.py` inclui os dois fontes novos no backend Vulkan; `ac_overlay.cpp` chama `ac_reinfo_frame()` a cada frame; `extra/sdl/window_trampoline.h` trata F8, suprime o repasse ao jogo com qualquer janela aberta e suspende/restaura o mouse; `ac_menu.cpp` (About) ganhou o bullet do F8.
- **Abertura**: três caminhos, todos idempotentes — (a) tecla **F8**; (b) checkbox **"Janela RE Info (F8)"** na seção Debug do menu Insert (`ac_menu.cpp`, chama `ac_reinfo_toggle`); (c) env `AC_REINFO=1` para abrir já no boot. A janela tem visibilidade independente do menu: fechar o Insert não fecha o RE Info e vice-versa. Altura padrão do menu Insert subiu de 560 para 660 px para o checkbox caber sem rolagem.
- **Dois bugs de integração corrigidos no link** (só aparecem no link completo, não na compilação isolada):
  1. `ac_overlay.cpp` declarava `void ac_reinfo_frame(void)` sem `extern "C"` — o compilador gerava `_Z15ac_reinfo_framev` e o linker não achava o símbolo C. Resolvido incluindo `ac_reinfo.h` (que tem o bloco `extern "C"`) em vez de redeclarar.
  2. `ac_reinfo_probe.c` lia `g_modules[0].base`, mas o build Linux **não linka `loader.c`** (as imagens entram por `reserve()` fixo gerado em `native_trace`). Substituído por `AC_MAIN_BASE`, derivado do layout do linker.
- **Base do `main` é `0x7000010000`, não `0x7000000000`**: `astral_chain/translator/linker.py` empacota `rtld` (16 KiB), `main` e `sdk` a partir de `0x7000000000`, então `main` é o segundo módulo. `MAIN_BASE` de `rt.h` é a base do *primeiro* módulo. Confirmado por leitura do binário em execução (área, missão, scene phase e dificuldade passaram a bater com a tela de título).
- **Verificação**: `./extra/tools/audit.sh` 100% aprovado (7 etapas); compilação limpa com `-Wall -Wextra -Werror` em `ac_overlay.cpp`, `ac_menu.cpp` e `ac_reinfo.cpp`; `ac_reinfo_probe.c` compila em C; `reinfo_tables.py` regenera 22/71/82 nomes com autotestes embutidos; `./extra/tools/qa_runner.sh 40 1200` passou nas 4 fases (build, runtime 40 s a 60 fps, save íntegro, frame 1200 sem artefatos); captura com `AC_REINFO=1 LARECOMP_WINDOW_SHOT=420` confirmou a janela desenhada com as tabelas expandidas mostrando nomes reais.
- **Limitação**: a maior parte dos valores ao vivo foi conferida apenas na tela de título (área/missão `-1`/`0xffffffff`, `scene phase 0x0138`, dificuldade `casual`); playtime, boss e batalha exigem uma sessão com save carregado.

#### 20. Discord Rich Presence (RPC) Nativo Linux e Overlay com Card de Prévia ao Vivo (`extra/rpc/*`, `extra/ui/overlay/ac_overlay_rpc.*`)
- **Objetivo**: prover suporte completo a Discord Rich Presence no Linux nativo com formatação de alta fidelidade e uma interface ImGui contendo configurações granulares e um **Card de Prévia em Tempo Real do Perfil do Discord**, seguindo o mesmo padrão estético e funcional de `bloodborne_pc`.
- **Implementação**:
  1. **Expansão de Missões e Capítulos (`extra/tools/reinfo_tables.py`, `extra/ui/overlay/ac_reinfo_tables.h`)**:
     - Descoberta das chaves `FILE_MISSION_` em `GameWord_USen.bin`, expandindo as missões conhecidas de 71 (apenas File 12) para **292 missões únicas** abrangendo todos os 11 Files da campanha principal e subcasos.
     - Mapeamento estático dos 12 Files (`k_ac_files`: *Startup, Awake, Link, Siege, Accord, Complicit, Wild, Peace, Salvation, Madness, Reckoning, Rebirth*) e das 5 Legions (`k_ac_legions`: *Sword, Arrow, Arm, Beast, Axe*).
     - Formatação compacta em linhas de até 100 caracteres mantendo o arquivo gerado em 161 linhas (meta ideal ≤ 300 linhas).
  2. **Agregador de Dados de Jogo (`extra/rpc/ac_game_data.h` / `.cpp`)**:
     - Leitura segura de memória guest (`ac_reinfo_probe.c`), resolução de IDs de área, missão, file, boss e dificuldade, cálculo de tempo de sessão decorrido e total de playtime do save.
     - Formatação determinística de payloads de atividade e prévia (`ActivityDisplay`), com resolução dinâmica de asset keys para o Discord: `large_image` mapeia para as 22 áreas do jogo (`01_hypermegafloat_ark`, etc.) e `small_image` mapeia para o capítulo atual (`file_01` a `file_11`), mantendo fallbacks para `astral_chain_logo` e `neuron_badge`.
  3. **Cliente Discord IPC Linux Nativo (`extra/rpc/ac_discord_rpc.h` / `.cpp`)**:
     - Conexão direta a sockets de domínio UNIX (`AF_UNIX`) cobrindo `$XDG_RUNTIME_DIR/discord-ipc-0..9`, caminhos Flatpak, Discord Canary, `/run/user/<uid>/` e `/tmp/`.
     - Worker thread em background não-bloqueante com auto-reconexão, handshake (opcode 0) e envio periódico de `SET_ACTIVITY` (opcode 1).
  4. **Overlay ImGui & Card de Prévia em Tempo Real (`extra/ui/overlay/ac_overlay_rpc.h` / `.cpp`)**:
     - Seção no menu principal (Insert) com indicador de status colorido (Verde = Conectado, Amarelo = Procurando, Cinza = Desativado) e botão para abrir a janela dedicada.
     - Janela de configuração com opções granulares: Exibir Arquivo/Capítulo, Exibir Caso/Missão, Exibir Dificuldade, Exibir Área, Exibir Boss, Exibir Tempo e personalização de Client ID (padrão oficial `1568955734606086196` com override via `AC_DISCORD_CLIENT_ID`).
     - **Card de Prévia ao Vivo do Discord**: child box estilizado na paleta escura do Discord (`#121315`), com badge customizada 50x50 com contorno teal e texto "AC", título "Astral Chain", e linhas de detalhes/estado/tempo atualizadas em tempo real conforme as opções e o estado do jogo mudam.
  5. **Integração**:
     - `extra/ui/overlay/ac_menu.cpp`: adicionada `section_discord` e altura padrão ajustada para 720 px.
     - `extra/ui/overlay/ac_overlay.cpp`: renderiza a janela de configuração do Discord RPC mesmo com o menu Insert fechado.
     - `astral_chain/gpu/nvn_build.py`: inclusão de `ac_game_data.cpp`, `ac_discord_rpc.cpp` e `ac_overlay_rpc.cpp` nos fontes Vulkan.
  6. **Autoteste de Formatação (`extra/tools/rpc_format_test.cpp`)**:
     - Teste de unidade cobrindo cenários de título, exploração de capítulo, combate a boss e combinações de toggles.
     - Integrado como etapa 8 do `./extra/tools/audit.sh`.
- **Verificação**:
  - `./extra/tools/audit.sh`: 100% aprovado nas 8 etapas.
  - `python3 -m astral_chain.build.setup_game --rebuild-if-stale`: Fast-Relink concluído em ~9 segundos com símbolos conferidos via `nm`.
  - Autoteste de formatação executado com sucesso.

#### 21. Extrator de Ícones e Assets PNG de Áreas, Legions, Ranks e Arquivos (`extra/tools/extract_icons.py`)
- **Objetivo**: extrair e converter do RomFS todos os ícones, badges, miniaturas e ilustrações de regiões/áreas do jogo para arquivos PNG transparentes organizados e prontos para uso em Rich Presence, interfaces e documentação externa (`/home/shadowy/Imagens/Icons AstralChain/`).
- **Pipeline de Deswizzling e Decodificação**:
  1. **Acesso ao RomFS e Descompactação Zstandard**: leitura direta do `game/romfs.bin` e extração de arquivos `.dat` e `.dtt` dos pacotes `.pkz` em `/miscTex/`.
  2. **Deswizzle Nativo Tegra X1 (`libtegra.so`)**: compilação e invocação via `ctypes` das rotinas de deswizzling de blocos lineares Tegra (`tegra.c` / `formats.c`), com cálculo dinâmico de `level_block_height` para evitar transbordamento de buffers em texturas pequenas.
  3. **Metadados UVD e Recorte Preciso**: leitura das coordenadas UVD embutidas nos arquivos `.dat` para recortar automaticamente o canvas inflado para a resolução útil exata de cada ícone (ex.: 240x240 para Legions, 1280x720 para Áreas, 1280x620 para Files, 54x54 para Armas, 158x158 para Ranks).
- **Categorias Extraídas (719 imagens totais)**:
  - `Areas/`: 22 ilustrações completas de áreas e regiões (`01_Hypermegafloat_Ark` a `22_Ark_Sewer_Management`) + miniatura panorâmica de Central City.
  - `Landmarks/`: 28 ilustrações de pontos de interesse e marcos da Arca (`Five-Story_Pagoda`, `Bar_District`, `Hermit_Base`, `Red_Moon`, etc.).
  - `Files/`: 12 banners de capítulos (`File_01_Startup` a `File_12_Rebirth`).
  - `Cases/`: 77 miniaturas de casos e missões (`Case_0100` a `Case_1100`).
  - `Legions/`: 5 ícones das Legions (`Legion_Sword`, `Legion_Arrow`, `Legion_Arm`, `Legion_Beast`, `Legion_Axe`).
  - `Characters/`: Retratos dos protagonistas masculino e feminino (`Protagonist_Male`, `Protagonist_Female`).
  - `Ranks/`: 20 patentes da polícia (`Police_Ranks/rank_thumb_01..20`) + 7 insígnias de avaliação (`Evaluation_Badges/Rank_Evaluation_D..S_Plus`).
  - `Weapons/`: 3 modos da X-Baton (`Weapon_XBaton`, `Weapon_Blaster`, `Weapon_Gladius`).
  - `Heliports/`: 6 miniaturas de zonas de viagem rápida por heliporto (`Heliport_01..06`).
  - `Abilities/` & `Shortcuts/`: 10 ícones de slots de habilidade e 12 de atalhos.
  - `Items/`: 541 ícones de itens e consumíveis.
- **Verificação**:
  - Execução de `.venv/bin/python3 extra/tools/extract_icons.py` gerando todas as 719 imagens em ~5 segundos sem erros.
  - `./extra/tools/audit.sh` 100% aprovado (344 linhas, dentro do limite).

#### 22. Resolução Precisa de Áreas e Capítulos via Scene Struct e Correção de "Unknown Zone"
- **Problema Diagnosticado**: Durante exploração/investigação (como na busca por desaparecidos em Maison Forest, File 03), o Discord Rich Presence exibia `"Unknown Zone (Explorando)"` e omitia o capítulo (File 03) e caso ("Missing").
  - **Causa Raiz 1 (`0xdbcf10` não-inicializado)**: O slot de área `0xdbcf10` é inicializado como `-1` (`0xFFFFFFFF`) e só é preenchido temporariamente ao cruzar gatilhos de volume de mapa. Como `0xFFFFFFFF` é interpretado como unsigned int sem correspondência em `k_ac_areas`, caía no fallback de erro `"Unknown Zone"`.
  - **Causa Raiz 2 (`mission_id == 0` na exploração)**: O slot `0xdbc420` só contém um ID não-nulo durante combates/sub-missões ativas. Na exploração livre, seu valor é `0`, fazendo `derive_file_num` retornar 0 e omitir o capítulo.
  - **Causa Raiz 3 (Scene Struct negligenciada)**: O ponteiro global `0xdb84c8` aponta para a `Scene Struct` do jogo, cujo campo em `+0x180` contém o `scene_id` canônico autoritativo (ex.: `0x1301` para File 03 Case 0301).
- **Implementação e Solução**:
  1. **Tabela e Mapeamento Canônico de Cenas (`extra/ui/overlay/ac_reinfo_tables.h` e `extra/tools/reinfo_tables.py`)**:
     - Definição de `AcSceneEntry` e da tabela `k_ac_scenes` com 54 cenas da história principal e pós-jogo (`0x1100..0x1c00`), associando cada `scene_id` à sua respectiva área canônica (`area_id` em `k_ac_areas`) e nome de caso oficial extraído dos arquivos de texto do jogo (`MENU_SELECT_CASE_NAME_`).
  2. **Leitor de Memória e Agregador (`extra/rpc/ac_game_data.cpp` e `.h`)**:
     - Leitura direta de `scene_ptr` em `slot64(0xdb84c8)` e do `scene_id` em `scene_ptr + 0x180`.
     - Tratamento explícito de `0xFFFFFFFF` (uninitialized -1) no slot de área `0xdbcf10`.
     - Fallback canônico: se a área por gatilho não estiver ativa, recupera a área canônica a partir do `scene_id` (ex.: `0x1301` / `0x1303` mapeiam diretamente para `0x5180` = "Maison Forest").
     - Derivação autoritativa do File: caso `mission_id == 0`, o capítulo é deduzido via `((scene_id >> 8) & 0xFF) - 0x10`, garantindo 100% de detecção de File 01..12 em qualquer fase (cutscenes, investigação, exploração e combate).
     - Nome do caso como missão durante exploração: quando não há missão de combate ativa, `mission_name` passa a exibir o nome do caso da história (ex.: `"Missing"` em Maison Forest).
     - Eliminação de qualquer exibição de `"Unknown Zone"`, com fallback seguro para `"Explorando a Arca"`.
  3. **RE Info Overlay (`extra/ui/overlay/ac_reinfo.cpp`)**:
     - Integração de `disp_area` e exibição do nome do caso ao lado do `scene_id` na tabela de estado ao vivo.
  4. **Autoteste de Formatação (`extra/tools/rpc_format_test.cpp`)**:
     - Casos 5 e 6 adicionados simulando o estado de exploração em Maison Forest e garantindo ausência de `"Unknown Zone"`.
- **Verificação**:
  - `./extra/tools/audit.sh`: 100% aprovado nas 8 etapas.
  - `python3 -m astral_chain.build.setup_game --rebuild-if-stale`: Fast-Relink concluído em ~9 segundos.
  - `SKIP_RUNTIME=1 ./extra/tools/qa_runner.sh`: aprovado.

#### 23. Seletor de Idioma do Jogo e Unificação Completa da Interface em Português
- **Objetivo**: Adicionar seletor de idiomas para o jogo (`Settings \ Game` no Launcher e no Overlay `Insert`), e unificar a interface eliminando o mix inconsistente de inglês e português.
- **Engenharia Reversa e Integração de Idiomas**:
  - Investigação da seleção de idiomas do Astral Chain no binário (`main+0x9c2870` que chama `GetDesiredLanguage` via PLT `0xa0e3d0`, command 21 de `SESSION_APPLICATION_FUNCTIONS` e command 0 de `SESSION_SETTINGS`).
  - Verificação dos 10 pacotes de texto empacotados no RomFS (`_USen.bin`, `_JPja.bin`, `_EUfr.bin`, `_EUde.bin`, `_EUes.bin`, `_EUit.bin`, `_EUru.bin`, `_CNzh.bin`, `_TWzh.bin`, `_KRko.bin`).
  - Implementação de `resolve_system_language()` em `astral_chain/runtime/services/ipc_dispatch.h`, convertendo a escolha do usuário em código little-endian de 64 bits (`uint64_t`), consultando dinamicamente `AC_LANGUAGE` / `LARECOMP_LANG` do ambiente, `larecomp.ini` ou locale do sistema (`LC_ALL` / `LANG`), com fallback seguro para `APPLICATION_LANGUAGE`.
  - Suporte completo no shim do runtime em `astral_chain/runtime/las/settings.h` e em `third_party/LASRecomp/runtime/kernel/settings.c` com `g_language_codes` e `g_language_names`, além de persistência automática em `larecomp.ini`.
- **Configuração no Launcher Qt6 (`extra/ui/launcher/`)**:
  - Adição de `QString language{"auto"}` em `GameSettings` (`Config.hpp` / `Config.cpp`), persistência em `userdata/launcher.ini` e exportação de `AC_LANGUAGE` e `LARECOMP_LANG` em `buildLaunchEnv()`.
  - Inclusão do seletor `m_languageCombo` em `SettingsDialog` com 12 opções: Automático (Padrão do Sistema / Host), Inglês (en-US), Japonês (ja), Espanhol (es), Francês (fr), Alemão (de), Italiano (it), Russo (ru), Chinês Simplificado (zh-CN), Chinês Tradicional (zh-TW), Coreano (ko) e Holandês (nl).
- **Seletor no Menu In-Game Overlay (`extra/ui/overlay/ac_menu.cpp`)**:
  - Nova seção `"Jogo & Idioma"` com combobox interativo ImGui, exibição do idioma ativo e salvamento imediato em `larecomp.ini`.
- **Unificação Integral dos Textos da Interface em Português**:
  - `SettingsDialog`: Janela renomeada para `"Astral Chain - Configurações"`, abas traduzidas (`"Jogo"`, `"Gráficos"`, `"Áudio"`, `"Avançado"`), botões `"Salvar"` / `"Cancelar"`, e todos os checkboxes e rótulos traduzidos com precisão técnica.
  - `ControlsDialog`: Janela renomeada para `"Astral Chain - Configuração de Controles"`, cabeçalho, colunas da tabela (`"Ação"`, `"Tecla / Mouse"`) e botões (`"Restaurar Padrões"`, `"Salvar"`, `"Cancelar"`).
  - `MainWindowUi` / `MainWindow`: Botões principais unificados (`"▶  JOGAR"`, `"  Jogar (Diagnósticos)"`, `"  Configurar Jogo"`, `"  Compilar Shaders"`, `"  Configurações"`, `"  Controles"`), e mensagens de status traduzidas.
  - `ac_menu.cpp`: Seções renomeadas para Adwaita em português (`"Desempenho"`, `"Graficos"`, `"Jogo & Idioma"`, `"Discord Rich Presence"`, `"Depuracao (Debug)"`, `"Sobre"`).
- **Verificação**:
  - `./extra/tools/audit.sh`: 100% aprovado nas 8 etapas (marcadores, limites ≤ 400 linhas, compilação do launcher Qt6 e 5 autotestes).
  - `.venv/bin/python -m astral_chain.build.setup_game --rebuild-if-stale`: Compilação e link do executável nativo `build/game/round-00/trace` aprovados em 11.1s.
  - `SKIP_RUNTIME=1 ./extra/tools/qa_runner.sh`: 100% aprovado.

#### 24. Suporte a Idioma Virtual (Português do Brasil) e Seletor de Idioma de Dublagem / Vozes
- **Objetivo**: Implementar suporte a idioma virtual para Português (Brasil) (pt-BR) através de overlay não-destrutivo do RomFS (sem modificar os dados originais do jogo) e adicionar opção de idioma das vozes / dublagem (Automático, Japonês, Inglês) no Launcher Qt6 e no menu in-game (Insert).
- **Mecanismo de Idioma Virtual (`pt-BR`)**:
  - Como o Astral Chain original para Switch não possui suporte nativo ao código `pt-BR`, expor o código diretamente causaria fallback para japonês ou falha de inicialização.
  - Implementado mapeamento transparente em `resolve_system_language()` (`astral_chain/runtime/services/ipc_dispatch.h`): quando `pt-BR` ou `pt` é selecionado, o Switch guest recebe o slot de linguagem base `en-US` (modo texto latino ocidental com suporte a acentuação UTF-8: `ç`, `ã`, `é`, `ó`).
  - Implementado interceptador e LayeredFS de RomFS não-destrutivo em `extra/storage/romfs_overlay.h` (`extra_romfs_overlay_init()` e `extra_romfs_overlay_read()`), conectado via hooks pontuais em `astral_chain/runtime/storage/romfs_storage.h`. Ele inspeciona a tabela `file_meta` no boot, mapeia arquivos de substituição em `mods/traducao_ptbr/` e `mods/` (como `Text/Text.pkz` e `ui/ui.pkz`), atualiza os offsets virtuais e serve os dados substituídos sem tocar em nenhum byte da imagem física `game/romfs.bin`.
- **Seletor e Controle de Dublagem / Vozes (Voice Language)**:
  - Engenharia reversa no binário do jogo (`main`) identificou o subsistema de áudio Wwise e as estruturas de opções:
    - `OptionManager::SetVoiceLanguage` (`main+0x8e1a40`): lê/escreve a opção de voz através do ponteiro relocável em `AC_MAIN_BASE + 0xdc13b0` (que aponta para `0xdafb14`: `0` = Japonês `"Japanese"`, `1` = Inglês `"English(US)"`, `-1` = Automático).
    - Sinaliza `dirty flag 1` em `*[AC_MAIN_BASE + 0xdc13b8]` (aponta para `0xcca5974`) e `dirty flag 2` em `*[AC_MAIN_BASE + 0xdc13c0]` (aponta para `0xcca5978`).
    - No loop principal por frame (`main+0x8ef3e0`), ao detectar `dirty flag 2 == 1`, a função `main+0x9cb820` é disparada chamando diretamente `AK::StreamMgr::SetCurrentLanguage()` com a tabela de strings Wwise em `0xdb5da0`, recarregando os bancos sonoros (`sound/Japanese/vo_*.bnk` ou `sound/English(US)/vo_*.bnk`) e limpando a flag.
    - Idioma ativo do motor sonoro é exposto em `*[AC_MAIN_BASE + 0xdc2908]` (aponta para `0xdb5cb0`). O slot `*[AC_MAIN_BASE + 0xdc26f8]` governa o idioma mestre dos textos do sistema e é mantido intacto para preservar legendas e interface em português/inglês.
  - Implementado `extra_get_guest_voice_language()` e `extra_set_guest_voice_language()` em `extra/ui/overlay/ac_reinfo_probe.c` manipulando exclusivamente os ponteiros de dublagem (`0xdc13b0`) e acionando as dirty flags (`0xdc13b8`, `0xdc13c0`) para recarregar o banco de som Wwise sem interferir no texto/UI.
  - Implementado monitoramento de boot em `apply_boot_voice_language()` (`ac_overlay.cpp`), garantindo que a preferência do usuário (ex: Japonês) persista mesmo que o jogo reinicialize opções durante os primeiros frames de carregamento.
- **Configuração no Launcher Qt6 e Menu In-Game**:
  - `extra/ui/launcher/include/Config.hpp` e `Config.cpp`: campo `voiceLanguage` ("auto", "ja", "en"), persistência em `userdata/launcher.ini`, e variáveis de ambiente `AC_VOICE_LANGUAGE` e `LARECOMP_VOICE_LANG`.
  - `extra/ui/launcher/src/SettingsDialog.cpp`: combobox de vozes ("Automático (Segue o Idioma do Texto)", "Japonês", "Inglês") e opção "Português (Brasil) [Tradução]" no combobox de idiomas.
  - `extra/ui/overlay/ac_menu.cpp`: combo "Idioma das Vozes" na seção "Jogo & Idioma" com alteração imediata da memória guest do jogo e persistência em `larecomp.ini`.
- **Verificação**:
  - `./extra/tools/audit.sh`: 100% aprovado nas 8 etapas (marcadores, limites ≤ 400 linhas, compilação do launcher Qt6 e todos os testes).
  - `python3 -m astral_chain.build.setup_game --rebuild-if-stale`: Fast-Relink concluído com sucesso e executável nativo atualizado.

#### 25. AMD FSR 3.1 (Temporal Upscaler + EASU 12-Tap + RCAS + Frame Generation) com Jitter Tangencial Sub-Pixel
- **Objetivo**: Implementar e otimizar o pipeline do **AMD FSR 3.1** no backend Vulkan 1.3 (`extra/gpu/fsr3_shaders.h` + `extra/gpu/fsr3_extra.h`), combinando **Reconstrução Direcional Isótropa EASU de 12 amostras** fundida com **Acumulação Temporal (TAAU YCoCg)**, **Jitter Sub-Pixel Halton(2,3) Projetado na Tangente**, **Nitidez Adaptativa de Contraste (RCAS) em Registrador** e **Frame Generation (2x FPS)** cadenciado sem penalizar a thread assíncrona `aq_worker`.
- **Preservação de Aliasing Zero-Cost e Passes Diferidos (`postprocess.c`, `present.c`)**:
  - Os render targets NVN internos do guest permanecem sempre em escala `1.0f` (`1600x900`), preservando 100% da elisão de cópia `alias_make` em `copy.c`, evitando erros de arredondamento ímpar na pirâmide de profundidade (`675 >> 1`) e mantendo o alinhamento exato com `gl_FragCoord.xy` (`TLDS` / `SUST`) nos passes diferidos de *cel-shading* e contorno da PlatinumGames.
  - Em vez de deslocar `vkCmdSetViewport` nos passes guest (o que desalinhava quads fullscreen diferidos em relação a `TLDS`), a sequência sub-pixel **Halton(2,3)** é projetada estritamente ao longo do **vetor tangente da borda** (`tang = vec2(-dir.y, dir.x)`) durante a reconstrução espacial-temporal (`tang * dot(j2, tang) * 0.28 * len * (1.0 - ui_mask)`), refinando diagonais serrilhadas sem jamais deslocar contornos perpendicularmente nem tremer a UI.
  - **Negative MIP LOD Bias seletivo (`textures.c`)**: aplica `-0.25` exclusivamente em samplers 3D anisotrópicos (`mf > 1 && d->max_aniso > 1 && extra_lod_bias() == 0.0f`), preservando LUTs e mapas especulares sem *shimmering*.
- **Pipeline Vulkan Fundido de Ultra-Baixo Custo (`extra/gpu/fsr3_shaders.h`, `present.c`)**:
  - **`post_barrier(cb)`**: substitui `vk_full_barrier(cb)` (`ALL_COMMANDS_BIT`) entre passes de pós-processamento por uma barreira leve de estágio fragmento (`COLOR_ATTACHMENT_OUTPUT_BIT` -> `FRAGMENT_SHADER_BIT | COLOR_ATTACHMENT_OUTPUT_BIT`).
  - **`fsr3_mv.frag` (Half-Res 5-Tap Cross + Lucas-Kanade)**: executado em meia resolução (`800x450` escalado por `rscale`), reduzindo em ~90% o consumo de banda de GPU na estimativa de movimento e sendo totalmente omitido quando nem FrameGen nem Jitter estão ativos (`!need_temp`).
  - **`fsr3_taa.frag` (Fused 12-Tap EASU + Tangent Jitter + YCoCg TAAU + In-Register RCAS)**: funde em um único passe a reconstrução elíptica rotacionada de 12 amostras do EASU (`setf` + `tapf` com trava anti-ringing dos 4 texels centrais), a reprojeção temporal com *Variance Clipping* $3\times 3$ em **YCoCg** e a nitidez **RCAS** (`p.w`) direto em registrador, eliminando um passe fullscreen inteiro em QuadHD/4K.
- **FSR 3.1 Frame Generation com Cadenciamento de Ponto Médio (Eliminação de Dente de Serra)**:
  - **Diagnóstico do Frametime Serrilhado / Dente de Serra**: Anteriormente, Present #1 (quadro interpolado) e Present #2 (quadro real) eram submetidos de forma quase simultânea ao final do frame (~0.2 ms entre eles). Em seguida, a thread `aq_worker` e o motor aguardavam todo o ciclo do próximo frame (~10 ms), fazendo com que as apresentações na Swapchain e no MangoHud alternassem constantemente entre 0.2 ms e 10 ms (padrão serrote/dente de serra com 1% low degradado).
  - **Pacing de Ponto Médio (`extra_fsr3_fg_pace_midpoint`)**: Mede continuamente a média móvel do ciclo de quadros reais (`s_smoothed_cycle_ns`) e sincroniza a submissão de Present #2 exatamente na metade do ciclo (`target_p2 = now_p1 + s_smoothed_cycle_ns / 2`). Utiliza espera híbrida precisa (`nanosleep` para intervalos grosseiros + micro-spin de ~250 µs via `_mm_pause` / `sched_yield`), dividindo o tempo de tela em 50%/50% entre o quadro interpolado e o real e creditando o tempo de espera em `g_blocked_ns`. O resultado é um frametime perfeitamente plano e estável no MangoHud sem oscilações alternadas e sem acionar `pace_slow`.
- **Verificação**:
  - `./extra/tools/audit.sh`: 9/9 etapas aprovadas.
  - `AC_UPSCALER=fsr3 AC_FSR_JITTER=1 AC_FSR_SHARPNESS=0.80 AC_FRAMEGEN=1 ./extra/tools/qa_runner.sh 20 550`: Fast-Relink, execução aprovada e inspeção visual do Frame 550 sem artefatos.

#### 26. Redesign Tático ImGui ("IRIS // Legatus Terminal") e Overlay de Performance em Jogo (`extra/ui/overlay/`)
- **Objetivo**: Alinhar a estética de todas as janelas ImGui (`Insert`, `F8 RE Info`, `Discord RPC`) à identidade visual do visor **IRIS / Legatus** da força-tarefa **Neuron** de *Astral Chain*, e adicionar um **Overlay de Performance em Jogo** opcional (configurável no próprio dashboard ImGui) que permanece visível quando o menu principal está escondido sem bloquear os controles.
- **Identidade Visual "IRIS // Legatus" (`extra/ui/overlay/ac_hud.h`, `ac_hud.cpp`)**:
  - Substituição da paleta cinza neutra Adwaita por superfícies táticas azul-marinho/obsidiana (`#0A101B` / `#0E1625`), destaques em **Ciano Elétrico IRIS** (`#00D4FF`), **Azul Cobalto Neuron** (`#3168B8`), alertas **Carmesim Redshift** (`#FF2E57`) e geometria angular (`WindowRounding = 3.0f`, `FrameRounding = 2.0f`).
  - Primitivas táticas `ac_ui_draw_iris_corners()`, `ac_ui_draw_window_chrome()` e `ac_ui_section_header()` desenhando cantoneiras luminosas em `L` do visor IRIS e cabeçalhos técnicos em todas as janelas ImGui (`ac_menu.cpp`, `ac_overlay_rpc.cpp`, `ac_reinfo.cpp`).
  - Carregamento automático de fonte TrueType do sistema (`FiraSans-Medium.ttf` / `NotoSans` / `DejaVuSans` em `15.5px`) em `ac_overlay.cpp` com suporte nativo a acentuação UTF-8 em português e fallback seguro para `AddFontDefault()`.
  - Reorganização do menu principal (`ac_menu.cpp`) com banner superior tático (`NEURON // LEGATUS SYSTEM TERMINAL`, GPU e resolução de saída via `ac_vk_extent`) e navegação estruturada em 4 abas (`Desempenho`, `Gráficos`, `Jogo & RPC`, `Diagnóstico`), reduzindo `ac_menu.cpp` para 244 linhas.
- **Overlay de Performance para Menu Oculto (`ac_perf_hud_tick`, `ac_perf_hud_render`)**:
  - Amostragem contínua por frame (`ac_perf_hud_tick()`) de FPS, Frametime (`ms`) e `1% Low FPS` com trava de leitura (`120 ms`) e slots tabulares de largura fixa baseados no template `"8888"` (`calc_tabular_width` / `draw_fixed_slot`), eliminando oscilações de largura da janela.
  - Renderizado com `ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize`, permitindo jogar normalmente com mouse/teclado/gamepad enquanto o menu principal está oculto.
  - Configurável ao vivo na aba `Desempenho` do dashboard (ou via variável `AC_PERF_HUD=1|2|3`): 3 estilos (`Compacto — Barra Tática IRIS`, `Detalhado — Telemetria + Gráfico`, `Mínimo — Pílula FPS`), seletor dos 4 cantos da tela, controle de opacidade (`0.30..1.00`) e opção de pré-visualização com o menu aberto.
- **Unificação Visual do Launcher Qt6 (`extra/ui/launcher/`)**:
  - `MainWindow.cpp` & `MainWindowUi.cpp`: Fundo em gradiente azul-marinho tático (`#0B1322` -> `#060A12`) com grade holográfica sutil e cantoneiras IRIS em `#00D4FF`; painel lateral com tag `NEURON // LEGATUS SYSTEM`, rodapé de atalhos in-game e `statusCard` superior com barra lateral ciano.
  - `CustomButton.cpp` & `Theme.cpp`: Hierarquia visual entre botão primário (`▶ JOGAR`) e secundários com barra lateral ciano/cobalto, além de cobertura completa de estilos Qt6 (`QSpinBox`, `QDoubleSpinBox`, `QGroupBox`, `QSlider`, `QScrollBar`, `QTabWidget`, `QTableWidget` e `QPushButton#primaryBtn`) nos diálogos `SettingsDialog`, `ControlsDialog` e `SetupDialog`.

#### 27. Linearidade de Frametime, Estabilização de 1% Low, Sincronização de Semáforo GPU e Pool de Staging VMA
- **Objetivo**: Eliminar oscilações de frametime (`p50=5.0ms, p95=16.0ms, p99=19.0ms`), picos de latência em espera de GPU/objetos no agendador paralelo e realocações contínuas de buffers de staging VMA, aproximando o **1% Low FPS** da média de quadros.
- **Pacer Híbrido Ativo em `parallel_mode` e Eliminação de Corrida Catch-Up (`extra/platform/pace_trampoline.h`, `time_trampoline.h`)**:
  - Em `extra_nvn_present_pace`, a condição `if (!others)` (sempre falsa quando as ~20 threads hóspedes rodam em `parallel_mode`) desviava a thread de apresentação para `THREAD_SLEEPING` com truncamento em milissegundos inteiros (`klock_wait(self, ms)`). Alterado para `if (parallel_mode || !others)`, liberando `klock` e `nvn_srw` durante a espera e executando `nanosleep` + micro-spin de `150 µs` via `extra_cpu_pause()` (`__builtin_ia32_pause()`) diretamente na `pthread` chamadora.
  - Quando um frame atrasa além do deadline (`now - origin > due * period`), o pacer reancora `origin = now; last_vsync = 0;` em vez de definir `last_vsync = due` no passado, eliminando a rajada subsequente de 3 a 5 frames não cadenciados (~5.0 ms) que tentavam "pagar" o tempo perdido.
  - Substituição de `sched_yield()` por `extra_cpu_pause()` nas janelas finais de `150 µs` de `extra_plat_sleep_until_ns()` e `extra_svc_sleep_thread()`, evitando preempções do agendador CFS/EEVDF do Linux nos últimos microssegundos antes do deadline.
- **Espera Direta no Semáforo de Linha do Tempo Vulkan e Precisão em Nanosegundos (`guest_threads.h`, `sync_trampoline.h`, `submission.c`, `host_runtime.c.in`)**:
  - Adicionada `renderer_wait_seq_timeout(uint64_t seq, uint64_t timeout_ns)` em `submission.c` e conectada ao estado `THREAD_WAIT_GPU` em `parallel_block_wait()` (`guest_threads.h`), despertando a thread hóspede diretamente por interrupção do driver Vulkan (`vkWaitSemaphores` em `g_vk.timeline`) em ~5 µs em vez de polling por timeout de `1 ms` em `klock_wait`.
  - Adicionadas `extra_sleep_cv_ns()` e `klock_wait_ns()` com conversão exata de ticks de 19.2 MHz para nanosegundos (`(deadline - now) * 625ull / 12ull`), eliminando o piso de `1 ms` em `THREAD_WAIT_OBJECTS`, `THREAD_WAIT_ADDRESS`, `THREAD_WAIT_CONDITION`, `THREAD_WAIT_MUTEX` e `THREAD_SLEEPING`.
- **Pool de Reciclagem de Buffers de Staging VMA e Thread-Safety (`extra/gpu/heap_mirror_extra.h`, `heap_mirror.c`)**:
  - Implementado pool thread-safe de 128 entradas alinhadas a 4 KB (`extra_wb_pool_alloc`, `extra_wb_pool_free`, `extra_wb_pool_reset`), eliminando o churn de ~2.200 chamadas/s a `vmaCreateBuffer`/`vmaDestroyBuffer` quando `LARECOMP_VK_LAZY_WB=0`.
  - Sincronizado o acesso a `wb_flight[wb_nflight++]` sob `lazy_lock` em `gmem_wb_record`, eliminando condição de corrida entre `aq_worker` (`AC_ASYNC_SUBMIT=1`) e threads hóspedes em `gmem_writeback_apply`.
- **Pacing de Ponto Médio Absoluto e Telemetria Unificada em Nanosegundos (`extra/gpu/fsr3_extra.h`, `postprocess.c`)**:
  - `extra_fsr3_fg_pace_midpoint` passa a usar `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` com janela de spin de `150 µs`, ancorando `now_p1` imediatamente após `vk_frame_end(NULL)` do Present #1.
  - Substituído `SDL_GetTicks() / 1000.0` (quantização inteira de `1 ms`) por `(double)SDL_GetTicksNS() / 1e9` e unificada a coleta de amostras (`record_present_sample`) em ambos os presents do Frame Generation, alinhando `fps`, `p50`, `p95`, `p99` e `1%low` na mesma escala temporal.
- **Alinhamento Automático de `AC_FPS_UNLOCKED` no Launcher Qt6 (`SettingsDialog.cpp`, `Config.cpp`, `MainWindow.cpp`)**:
  - Configurar `FPS Alvo > 60` ou `0 (Ilimitado)` ativa automaticamente `m_fpsUnlockedCheck` e exporta `AC_FPS_UNLOCKED=1` e `AC_PRESENT_INTERVAL`, impedindo que o teto de 60 FPS do motor permaneça ativo por engano em taxas altas (120/144/240 Hz).

#### 28. Reorganização Estrutural de Diretórios (`src/`, `build/`, `debug/`, `tools/qa/`) e Eliminação de Números Mágicos
- **Objetivo**: Separar de forma limpa código-fonte, artefatos de compilação, caches de recursos e arquivos de depuração, eliminando pastas misturadas na raiz (`astral_chain/`, `extra/`, `build-launcher/`, `cache/`) e substituindo números mágicos espalhados pelo código por constantes nomeadas (`#define`, `enum`, `constexpr`).
- **Nova Arquitetura de Diretórios**:
  - **`src/`**:
    - `src/runtime/`: Runtime C do Horizon, serviços HLE e camada NVN/Vulkan 1.3 (antigo `astral_chain/runtime`).
    - `src/hooks/`: Trampolines e hooks de plataforma Linux (`platform/`, `sdl/`, `gpu/`, `input/`, `storage/`, `ui/`, `hooks.h` — antigo `extra/{platform,sdl,gpu,input,storage,ui,hooks.h}`).
    - `src/additions/`: Subsistemas modulares (`overlay/`, `rpc/`, `toggles/` — antigo `extra/ui/overlay`, `extra/rpc`, `extra/toggles`).
    - `src/launcher/`: Launcher Nativo Qt6 C++20 (antigo `extra/ui/launcher`).
    - `src/resources/`: Patches `.pchtxt` (`patches/`) e bancos de variantes de shaders (`data/`).
    - `src/astral_chain/`: Módulos Python de build, tradução estática, cache de shaders/BC7 e ferramentas (`build/`, `translator/`, `gpu/`, `tools/`).
  - **`build/`**:
    - `build/game/`: Executável nativo (`round-00/trace`) e manifesto de unidades (`unit_objects.json`).
    - `build/launcher/`: Binário compilado do Launcher Qt6 (`build/launcher/astral-chain-launcher`).
    - `build/resources/`: Utilitários auxiliares compilados (`shader-precompile`, `bc7-convert`, `las-translator`, `glsl/`) e caches de recursos (`spirv/`, `bc7/`).
    - `build/objects/`: Cache SHA-256 dos 122 objetos `.obj` do guest e objetos do runtime/NVN (antigo `build/native-objects`).
    - `build/images/`: Cache de imagens de módulo traduzidas (antigo `build/native-images`).
  - **`debug/`**:
    - `debug/frames/`: Capturas de tela determinísticas (`frame_*.png`, `window.png`, `pass_*.png`).
    - `debug/traces/`: Código-fonte do runtime gerado (`trace.c`), objeto (`trace.o`), log do compilador (`compiler.log`) e alvos de relocação (`relocation-targets.bin`).
  - **`tools/qa/`**: Scripts de auditoria estática, execução de QA e self-tests (`audit.sh`, `qa_runner.sh`, `mouse_pointer_test.c`, `reinfo_tables.py`, `extract_icons.py`).
  - **Remoção de Scripts Legados Windows**: Removidos `Play Astral Chain.cmd`, `Play Astral Chain (diagnostics).cmd`, `setup.cmd`, `astral_chain/get_python.ps1` e `astral_chain/run_guarded.ps1`.
- **Sincronização Automática do Runtime em Fast-Relink (`src/astral_chain/build/setup_game.py`)**:
  - Adicionado `_refresh_trace_host_runtime()`, que atualiza automaticamente o cabeçalho de `debug/traces/trace.c` a partir de `src/runtime/host_runtime.c.in` antes do `compile_native_trace`, preservando os 122 arquivos `.obj` do guest em `build/objects/` e garantindo Fast-Relink em ~8s mesmo após edições no runtime ou reorganização de includes.
- **Eliminação de Números Mágicos**:
  - `src/runtime/las/nvn/vulkan/device/postprocess.c`: Constantes `VK_IDLE_REPRESENT_DELAY_MS`, `MAX_FT_SAMPLES`, `VK_MS_PER_SEC`, `VK_NS_PER_SEC_F`, `VK_NS_PER_MS_F`, `VK_NS_PER_MS`, `VK_ACQUIRE_WARN_THRESHOLD_NS`, `VK_ACQUIRE_WARN_MAX_LOGS`, `VK_PRESENT_LOG_LIMIT`, `VK_FG_STAGE_REAL`, `VK_FG_STAGE_INTERP`, `VK_FG_DEFAULT_CYCLE_NS`, `VK_FG_MIN_CYCLE_NS`, `VK_FG_MAX_CYCLE_NS`, `VK_FG_EMA_PREV_WEIGHT`, `VK_FG_EMA_TOTAL_DIV`, `VK_DEFAULT_RENDER_WIDTH`, `VK_SLOW_FRAME_DEFAULT_START`, `VK_PIPELINE_CACHE_SETTLE_NS`, `VK_GPUPROF_AUTO_THRESHOLD_NS`, `VK_GPUPROF_AUTO_MAX_COUNT`, `VK_DEFAULT_FPS_LOG_INTERVAL`, `VK_PCT_P50`, `VK_PCT_P95`, `VK_PCT_P99`, `VK_MIN_VALID_FT_MS`, `VK_DIR_MODE_DEFAULT`.
  - `src/hooks/gpu/heap_mirror_extra.h` & `src/runtime/las/nvn/vulkan/exec/memory/heap_mirror.c`: Constantes `EXTRA_WB_POOL_CAPACITY`, `EXTRA_WB_ALIGN_4K`, `EXTRA_PAGE_SIZE_4K`, `EXTRA_PAGE_SHIFT_4K`, `EXTRA_PAGE_MASK_4K`, `EXTRA_PAGE_OFFSET_MASK_4K`, `EXTRA_HEAP_ALIGN_64K`, `EXTRA_MAX_DIRTY_PAGES`, `EXTRA_MAX_RUN_PAGES`, `EXTRA_MIRROR_COPY_ALIGN`, `EXTRA_MAX_WB_PENDING`, `EXTRA_MAX_WB_FLIGHT`, `EXTRA_LAZY_TAB_MAX`, `EXTRA_LAZY_HOT_MAX`, `EXTRA_LAZY_UNEXPECTED_LOG_MAX`, `EXTRA_ZERO_FALLBACK_WORDS`, `EXTRA_ZERO_BUFFER_ALIGN`, `EXTRA_MB_SHIFT`.
  - `src/runtime/las/nvn/vulkan/exec/submission/submission.c`: Constantes `VK_MAX_SUB_DESC_POOLS`, `VK_WAIT_TAB_CAPACITY`, `VK_WAIT_SEQ_TIMEOUT_NS`, `VK_HANG_WATCH_POLL_MS`, `VK_HANG_DEFAULT_MS`, `VK_HANG_EXIT_CODE`, `VK_NS_PER_MS_ULL`, `VK_NS_PER_MS_F`, `NVN_GPU_TICK_NUM`, `NVN_GPU_TICK_DEN`, `VK_PACE_HITCH_THRESHOLD_NS`, `NVN_COUNTER_ENTRY_STRIDE`, `VK_QUERY_STRIDE_U64`, `VK_DL_RING_SIZE`, `VK_DL_RING_MASK`, `VK_DL_MSG_LEN`, `VK_GP_LABEL_LEN`, `VK_DESC_POOL_*`.
  - `src/hooks/platform/sync_trampoline.h`, `time_trampoline.h`, `pace_trampoline.h`, `src/hooks/gpu/fsr3_extra.h` & `src/runtime/kernel/guest_threads.h`: Constantes `EXTRA_GPU_WAIT_WINDOW_NS`, `EXTRA_GPU_WAIT_WORKER_NS`, `EXTRA_WAIT_DEFAULT_NS`, `EXTRA_WAIT_POLL_SLICE_NS`, `EXTRA_WAIT_MIN_SLICE_NS`, `EXTRA_WAIT_WINDOW_MAX_NS`, `EXTRA_TICK_TO_NS_NUM`, `EXTRA_TICK_TO_NS_DEN`, `FSR3_*`, `EXTRA_TZ_NAME_LEN`, `EXTRA_EINVAL_ERRNO`, `EXTRA_TIME_ZONE_INVALID`, `EXTRA_SECONDS_PER_MINUTE`, `EXTRA_TM_YEAR_BASE`.
  - `src/launcher/include/Config.hpp`, `src/launcher/src/Config.cpp`, `MainWindow.cpp`, `SettingsDialog.cpp`: Constantes `constexpr` para geometria de janela, grade IRIS, limites de FPS/nitidez/áudio e códigos `kVk*`.

#### 29. Eliminação de Código Comprimido, Unificação de `build/game/cache/` e Divisão Modular de Cabeçalhos (`win32_types.h`, `win32_window_types.h`, `dir_trampoline.h`)
- **Objetivo**: Eliminar todas as ocorrências de código comprimido em linha única (`if (...) { a; b; }`), dividir arquivos que cresciam ao expandir declarações em módulos coesos e unificar todos os caches em tempo de execução sob `build/game/cache/`.
- **Divisão Modular por Responsabilidade**:
  - `src/hooks/platform/win32_types.h` (219 linhas): Extraídos de `platform_posix.h` os tipos primitivos Win32 (`DWORD`, `HANDLE`, `FILETIME`, `SYSTEM_INFO`), conversões UTF-8/UTF-16 (`MultiByteToWideChar`, `WideCharToMultiByte`), `_wfopen` e variáveis de ambiente (`GetEnvironmentVariableA/W`), reduzindo `platform_posix.h` para 283 linhas legíveis.
  - `src/hooks/sdl/win32_window_types.h` (214 linhas): Extraídos de `window_trampoline.h` os tipos de janela/GDI (`POINT`, `RECT`, `PAINTSTRUCT`, `BITMAPINFOHEADER`, `WINDOWPLACEMENT`, `MONITORINFO`, `WNDCLASSW`, `WM_*`, `SWP_*`) e stubs GDI/mensagens, mantendo `window_trampoline.h` focado exclusivamente no ciclo de vida SDL3, eventos e XInput (386 linhas, 0 blocos comprimidos).
  - `src/hooks/storage/dir_trampoline.h` (194 linhas): Extraídos de `storage_trampoline.h` os helpers de criação recursiva de diretórios (`CreateDirectoryA/W`, `RemoveDirectoryA/W`), cópia/movimentação/remoção (`CopyFileW`, `MoveFileExW`, `DeleteFileW`), atributos (`GetFileAttributesExW`), espaço em disco (`GetDiskFreeSpaceExW`) e varredura recursiva (`extra_walk_dir`), reduzindo `storage_trampoline.h` para 251 linhas focadas na resolução e normalização de saves.
- **Descompressão Integral e Constantes Nomeadas**:
  - Descomprimidos todos os blocos `if (...) { a; b; }` e substituídos números mágicos em `storage_trampoline.h`, `dir_trampoline.h`, `find_trampoline.h`, `romfs_overlay.h`, `platform_posix.h`, `win32_types.h`, `crash_trampoline.h`, `pace_trampoline.h`, `sync_trampoline.h`, `shared_memory_trampoline.h`, `window_trampoline.h`, `win32_window_types.h`, `mouse_menu.h`, `menu_detect.h`, `vk_codes.h`, `shaderc_posix.h`, `ac_menu.cpp`, `ac_overlay.cpp`, `ac_vk_glue.cpp`, `ac_reinfo.cpp`, `ac_game_data.cpp`, `MainWindow.cpp`, `SettingsDialog.cpp`, `CustomButton.cpp` e `CustomButton.hpp`.
  - Atualizado `tools/qa/audit.sh` para auditar automaticamente a ausência de blocos comprimidos em linha única e aplicar a política de tamanho modular (meta ideal ≤ 350 linhas, contexto coeso permitido até ≤ 450 linhas, divisão obrigatória acima de 450 linhas).
- **Unificação do Cache de Runtime em `build/game/cache/` e Warm-Up Vulkan (`surface.c`, `vulkan_trampoline.h`, `shader_precompile.c`)**:
  - Atualizados `spirv_cache.c`, `shader_precompile.c`, `bc7_gpu.c`, `logical_device.c`, `program.c`, `pipeline.c` e `gpu_warm.c` para utilizar `build/game/cache/{spirv,bc7,vk_pipeline.bin,vk_programs.bin,vk_pipelines.bin}`, impedindo a recriação de uma pasta `cache/` avulsa na raiz do projeto.
  - Corrigido `extra_vk_init_warm(&g_vk.inst, ...)` em `surface.c` e `vulkan_trampoline.h` (passando `VkInstance*` e inicializando `SDL_Init(SDL_INIT_VIDEO)`), permitindo o pré-aquecimento completo de 225 programas, 1450 shader libraries e 190 pipelines em `1.2s`.

#### 30. Correção de Artefatos / Flicker do Atlas de UI no Modo IRIS e Suporte Nativo a Prompts de Controle Xbox (`EXTRA_PROMPT_MODE_XBOX`)
- **Objetivo**: Eliminar os artefatos visuais de 1 frame no modo IRIS (onde um quad inclinado do visor exibia o atlas inteiro da UI distorcido com letras, barras e ícones) e implementar o layout de botões **Xbox** (`A` embaixo, `B` direita, `X` esquerda, `Y` cima, `LB`/`RB`, `LT`/`RT`, `View`/`Menu` e diagramas em losango) com detecção automática de controle via SDL3.
- **Snapshot Determinístico de Vertex/Index Buffers de UI/HUD e Sincronização de Fences (`vcache.c`, `draw.c`, `gcopy.c`, `nvn.h`, `objects.c`, `cmdbuf.c`)**:
  - Em `guest_direct()` (`src/runtime/las/nvn/vulkan/exec/cache/vcache.c`), removido o guarda `if (!pt_init())` em torno de `if (size < min_direct) return false;` (`512 KB`). Como `gmem_buf` e `vc_tab` possuem apenas uma instância em VRAM atualizada ao final da submissão (`gmem_sync()` -> `vc_flush()`), fazer bind direto de buffers dinâmicos pequenos (`< 512 KB`) permitia que a CPU sobrescrevesse os vértices/UVs da UI e do visor IRIS antes da leitura pela GPU, desenhando o atlas completo da UI (`UV = [0,1]x[0,1]`) em quads inclinados do HUD por 1 frame.
  - Em `draw_()` (`src/runtime/las/nvn/vulkan/exec/commands/draw.c`), restaurado `small_max` (`LARECOMP_VK_COPY_SMALL`) de `16` para `1024`, elevado o teto de `STREAM_NEED()` de `32 KB` para `256 KB` e restaurado o limiar de `guest_copy_vtx` para `512 KB`, garantindo que lotes de quads de texto, molduras 9-slice e elementos do modo IRIS sejam copiados para memória transiente no instante exato do draw.
  - Em `guest_copy_flush()` (`src/runtime/las/nvn/vulkan/exec/cache/gcopy.c`), restaurada a chamada a `guest_vcopy_flush()` para invalidar `vcopy` após escritas inline na lista de comandos.
  - Em `nvnCommandBufferFenceSync` / `nvnSyncWait` (`src/runtime/las/nvn/nvn.h`, `objects.c`, `cmdbuf.c`), adicionado rastreamento `NvnSync::from_cmd` e `aq_last_exec` sob `aq_lock`, impedindo que `nvnSyncWait` retorne prematuramente antes de `aq_worker` executar `CMD_FENCE_SYNC` sob `AC_ASYNC_SUBMIT=1`.
- **Prompts de Botões no Padrão Xbox e Opção de Confirmar com A (`src/hooks/ui/xbox_prompts.h`, `gamepad_trampoline.h`, `window_trampoline.h`, `font_trampoline.h`, `button_prompts.c`, `SettingsDialog.cpp`, `Config.cpp`, `ac_menu.cpp`)**:
  - Criado `src/hooks/ui/xbox_prompts.h` (`extra_xbox_apply_atlas`) para patchear o atlas `256x512` (`ui_core`) preservando 100% das silhuetas nativas e da máscara RGB de sombra de 2px (`p[0..2]`) da PlatinumGames: permuta os blocos `26x26` de `Y`/`X` (`IB_Y <-> IB_X`), restaura o interior alfa (`p[3]`) de `IB_L`, `IB_R`, `IB_ZL`, `IB_ZR`, `IB_MINUS` e `IB_PLUS` e recorta `LB`, `RB`, `LT`, `RT`, `View` (`⧉`) e `Menu` (`≡`).
  - Adicionada a opção **"Confirmar / Ação com Botão A no Controle (Inverter B -> A)"** (`AC_XBOX_CONFIRM_A=1` por padrão, `xbox_confirm_a` no `launcher.ini`) tanto no Launcher Qt6 (`SettingsDialog.cpp` / `Config.cpp`) quanto ao vivo no menu ImGui IRIS (`Insert`, `ac_menu.cpp`): quando ativa, mapeia o botão físico inferior (`SDL_GAMEPAD_BUTTON_SOUTH` / Xbox **A**) para `EXTRA_HID_BIT_A` (Confirmar / Ação / Interagir) e o botão físico direito (`SDL_GAMEPAD_BUTTON_EAST` / Xbox **B**) para `EXTRA_HID_BIT_B` (Cancelar / Voltar / Esquiva), mantendo os glifos `(A)` e `(B)` nos slots `IB_A`/`IB_B` e permutando os diagramas de 4 círculos (`DIA_B <-> DIA_A`, `38x37`) sem alterar nenhum outro botão do jogo (`X`, `Y`, `LB`, `RB`, `LT`, `RT`, `View`, `Menu`, analógicos e D-Pad).
  - Em `gamepad_trampoline.h`, `window_trampoline.h`, `ac_overlay.cpp` e `ac_menu.cpp`, adicionados `extra_open_initial_gamepad()`, `extra_get_gamepad_prompt_mode()`, abertura/fechamento do menu ImGui pelo controle ao **segurar `L3 + R3` por 2 segundos** (`EXTRA_OVERLAY_HOLD_MS = 2000`), navegação completa por gamepad (`ImGuiConfigFlags_NavEnableGamepad`) e troca de abas com **`LB` / `RB`**.
