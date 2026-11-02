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
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/vcache.c` | `guest_direct` (`min_direct`) | `vcache.c` | Fallback adaptativo de buffers dinâmicos de interface (≤ 512 KB) para cópia da RAM do host no Linux | 11 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/gcopy.c` | `guest_copy_flush` | `gcopy.c` | Desacoplamento da invalidação do cache de vértices de atualizações de cbuf | 2 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/vcopy.c` | `guest_copy_vtx` | `vcopy.c` | Hash de 64-bit incluindo `size` e correspondência estrita `size == size` | 4 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/commands/draw.c` | `draw_` (streams & IB) | `draw.c` | Unificação de chave de cópia para buffers ≤ 512 KB e cópia transiente isolada de índices | 27 lines |
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
| `astral_chain/runtime/las/nvn/vulkan/textures.c` | `vk_sampler` (`extra_lod_bias`/`extra_lod_min`/`extra_no_shadow`) + include | `extra/toggles/ac_toggles.h` | Override ao vivo de bias de LOD (qualidade de textura), piso de LoD e desligamento de sombra na montagem de samplers | 6 lines |
| `astral_chain/runtime/las/game_hooks_neutral.c` | `smaa_area_tex`/`smaa_search_tex`/`smaa_hlsl_src` (stubs removidos) | `nvn/smaa_data.c` | Vendorização do SMAA 1x: os três símbolos deixam os stubs `FATAL` e passam a vir dos dados vendidos | 1 line |

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

