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

| Upstream file | Hook / change | Extra module | Purpose | Added |
|---|---|---|---|---|
| `astral_chain/runtime/host_runtime.c.in` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de ponte POSIX no runtime | 3 lines |
| `astral_chain/runtime/host_runtime.c.in` | `nvn_lazy_vectored` | `extra/core/memory_trampoline.h` | Hipótese de interceptação de falta lazy | 4 lines |
| `astral_chain/runtime/app_window.h` | `app_set_fullscreen` | `extra/core/window_trampoline.h` | Alternância de tela cheia via SDL3 | 3 lines |
| `astral_chain/runtime/app_window.h` | `app_pump` | `extra/core/window_trampoline.h` | Bombeamento de eventos de entrada via SDL3 | 3 lines |
| `astral_chain/runtime/app_window.h` | `app_open` | `extra/core/window_trampoline.h` | Criação de janela de inicialização via SDL3 | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `backend_init` / `backend_clear_window` | `device.c` | Orquestrador enxuto (~20 linhas) integrando submódulos de domínio em `device/` | 19 lines |
| `astral_chain/runtime/las/nvn/vulkan/device/` | `instance`, `physical_device`, `logical_device`, `surface`, `swapchain`, `present`, `postprocess` | `device/` | Separação do backend Vulkan por ciclo de vida, responsabilidade funcional e telemetria P50/P95/P99 | 7 modules |
| `astral_chain/runtime/guest_threads.h` | `guest_tls_allocate` | `extra/core/sync_trampoline.h` | Alocação de página TLS para threads hóspedes | 3 lines |
| `astral_chain/runtime/guest_threads.h` | `guest_parallel_start` | `extra/core/sync_trampoline.h` | Inicialização de threads hóspedes em paralelo | 3 lines |
| `astral_chain/runtime/wave_output.h` | `#include "extra/hooks.h"` | `extra/core/audio_trampoline.h` | Redirecionamento da API WaveOutput para SDL3 | 3 lines |
| `astral_chain/runtime/las/button_prompts.c` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de cabeçalho unificado de hooks | 3 lines |
| `astral_chain/runtime/las/button_prompts.c` | `enabled` / `texpatch_*` / `cut_text` | `extra/core/font_trampoline.h` | Substituição de GDI por FreeType e troca dinâmica de prompts (Switch <-> Teclado/Mouse) | 16 lines |
| `astral_chain/runtime/las/input_bindings.h` | `ib_defaults` | `extra/core/window_trampoline.h` | Mapeamento ergonômico padrão WASD + Mouse estilo BBPort no Linux | 7 lines |
| `astral_chain/runtime/las/hle.h` | `HLE_SECTION` | `extra/core/platform_posix.h` | Definição de atributo de seção de símbolos ELF | 3 lines |
| `astral_chain/runtime/nvn_hle.h` | `nvn_linked_lookup` | `extra/core/platform_posix.h` | Descoberta de símbolos HLE entre limites ELF | 3 lines |
| `astral_chain/runtime/save_store.h` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de cabeçalho unificado de hooks | 3 lines |
| `astral_chain/runtime/save_store.h` | `save_store_ensure` | `extra/core/storage_trampoline.h` | Normalização de diretório de save no Linux | 4 lines |
| `astral_chain/runtime/save_fs.h` | `save_resolve` | `extra/core/storage_trampoline.h` | Formatação de caminhos e conversão mbstowcs | 6 lines |
| `astral_chain/runtime/las/las_compat.h` | `#ifndef _WIN32` | `extra/core/storage_trampoline.h` | Inclusão de rotinas de compatibilidade POSIX | 4 lines |
| `astral_chain/native_trace.py` | `run-env.txt` | `run_guarded.sh` | Exportação de backend vinculado e glslang | 4 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec.c` | `vk_exec_reset` | `exec.c` | Inclusão de `guest_vcopy_flush()` no reset do backend Vulkan | 1 line |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/vcache.c` | `guest_direct` (`min_direct`) | `vcache.c` | Fallback adaptativo de buffers dinâmicos de interface (≤ 512 KB) para cópia da RAM do host no Linux | 11 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/gcopy.c` | `guest_copy_flush` | `gcopy.c` | Desacoplamento da invalidação do cache de vértices de atualizações de cbuf | 2 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/cache/vcopy.c` | `guest_copy_vtx` | `vcopy.c` | Hash de 64-bit incluindo `size` e correspondência estrita `size == size` | 4 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/commands/draw.c` | `draw_` (streams & IB) | `draw.c` | Unificação de chave de cópia para buffers ≤ 512 KB e cópia transiente isolada de índices | 27 lines |
| `astral_chain/runtime/las/nvn/vulkan/exec/commands/present.c` | `vk_present_draw` / `post_pass` | `present.c` | Sincronização de scissor filter via `ss_forget()` eliminando corte retangular escuro | 7 lines |
| `astral_chain/runtime/las/nvn/objects.c` | `register_texture` | `objects.c` | Invalidação do gerador de views de textura (`vk_view_gen++`) no registro de pools | 1 line |
| `astral_chain/runtime/las/nvn/vulkan/textures.c` | `mark_dynamic` | `extra/core/vulkan_trampoline.h` | Amostragem FNV para texturas de vídeo dinâmicas | 5 lines |
| `astral_chain/runtime/las/settings.h` | `settings_load` | `extra/ui/launcher` | Suporte a variável de ambiente `AC_UPSCALER=fsr1` | 4 lines |
| `astral_chain/runtime/hid_service.h` | `hid_xinput` | `extra/core/window_trampoline.h` | Leitura de gamepad via SDL3 no Linux | 4 lines |
| `astral_chain/runtime/pl_hle.h` | `pl_load_fonts` | `extra/core/platform_posix.h` | Caminhos de fontes padrão do sistema Linux | 5 lines |
| `astral_chain/runtime/las/maxwell_ext.h` | `#include "extra/core/maxwell_extra.h"` | `extra/core/maxwell_extra.h` | Tradução de instruções compute Maxwell (família LEA) | 1 line |
| `extra/ui/launcher/` | `presentMode` / `LARECOMP_VK_PRESENT` / `asyncSubmit` | `extra/ui/launcher` | Seletor de sincronização de monitor (FIFO, Mailbox, Immediate) e submissão gráfica assíncrona | 25 lines |
| `astral_chain/runtime/nvn_hle.h` | `nvn_present_pace` | `extra/core/pace_trampoline.h` | Suporte dinâmico a taxa de quadros alvo (AC_FPS e pacing customizado) | 4 lines |

---

## 3. Registro de Alterações (Decisões & Hipóteses)

### Remoção de Binários e Scripts Win32
- Remoção de executáveis `.exe`, scripts `.cmd`/`.ps1` e código C# do launcher original.
- Supressão de binários pré-compilados Windows para garantir portabilidade nativa em Linux.

### Módulo de Sincronização (`extra/core/sync_trampoline.h`)
- Hipótese de emulação de `HostEvent`, mutex e timers Win32 via POSIX `pthread_cond` e `clock_gettime`.
- Implementação de TLS hóspede com `mmap` e inicialização de threads paralelas via `pthread_create`.

### Módulo de Memória Virtual (`extra/core/memory_trampoline.h`)
- Substituição de `VirtualAlloc` por `mmap(MAP_NORESERVE | MAP_FIXED_NOREPLACE)` e `mprotect`.
- Tratamento de falta de página de escrita GPU via tratador `sigaction(SIGSEGV)`.

### Módulo de Janela e Entrada (`extra/core/window_trampoline.h`)
- Criação e ciclo de vida de janela via `SDL_CreateWindow` com flags Vulkan e redimensionamento.
- Mapeamento de eventos de teclado, foco, captura relativa de mouse e alternância de fullscreen.

### Módulo de Apresentação Vulkan (`extra/core/vulkan_trampoline.h`)
- Substituição das extensões de superfície Win32 por `SDL_Vulkan_CreateSurface`.
- Coleta de extensões de instância requeridas através de `SDL_Vulkan_GetInstanceExtensions`.

### Módulo de Saída de Áudio (`extra/core/audio_trampoline.h`)
- Substituição da API `waveOut` por `SDL_AudioStream` em formato float32 interleaved.
- Emulação de eventos de callback e reprodução de buffers de áudio via chamadas SDL3.

### Módulo de Sistema de Arquivos (`extra/core/storage_trampoline.h`)
- Conversão da enumeração de arquivos de `FindFirstFile` para `opendir`/`readdir` POSIX.
- Manipulação recursiva de pastas e arquivos através de chamadas de sistema `stat` e `mkdir`.

### Ponte de Plataforma POSIX (`extra/core/platform_posix.h`)
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
- Criação de `play.sh`, `play-diagnostics.sh`, `launcher.sh` e `astral_chain/run_guarded.sh`.

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

### Shim de Compilação de Shaders POSIX (`extra/core/shaderc_posix.h`)
- Substituição do pipeline de pipes e processos Win32 por `pipe`/`fork`/`exec` POSIX.
- Implementação isolada de `shaderc` executando `glslang` de forma limpa em ambiente Linux.

### Compatibilidade de Cabeçalhos NVN no Linux (`las_compat.h`, `platform_posix.h`)
- Proteção de inclusões de `<direct.h>` sob `_WIN32` e remapeamento de `_fseeki64` para `fseeko`.
- Declaração de `GetActiveProcessorCount` com `sysconf(_SC_NPROCESSORS_ONLN)` e `ALL_PROCESSOR_GROUPS`.

### Compatibilidade de Áudio Host (`extra/core/audio_trampoline.h`)
- Adição dos campos `format` e `header` em `AudioHookBuffer` e `AudioHookOutput` para compatibilidade de tipos.
- Definição do alias `WaveBuffer` permitindo consumo direto pelas rotinas de `audout_service.h`.

### Suporte a Fuso Horário POSIX (`extra/core/time_trampoline.h`)
- Implementação de `__time64_t`, `_localtime64_s` e `GetTimeZoneInformation` mapeando `tm_gmtoff` para viés UTC.
- Fornecimento de `GetLocalTime` preenchendo a estrutura `SYSTEMTIME` com campos de horário do sistema local.

### Operações de Armazenamento e Consulta de Arquivos (`extra/core/storage_trampoline.h`)
- Suporte a `BY_HANDLE_FILE_INFORMATION`, `GetFileInformationByHandle` e `GetDiskFreeSpaceExW` via `stat` e `statvfs`.
- Mapeamento de `GetFileAttributesA`, `CreateDirectoryA` e declaração das constantes `ERROR_NO_MORE_FILES` e `ERROR_INVALID_PARAMETER`.

### Enumeração de Diretórios e Padrões (`extra/core/find_trampoline.h`)
- Separação modular de `FindFirstFileA`/`FindFirstFileW` e `FindNextFileA`/`FindNextFileW` com filtro por extensão.
- Encadeamento de fechamento de busca via `extra_close_handle` assegurando liberação de instâncias de `DIR`.

### Descritores e Sincronização com Contagem de Referência (`extra/core/sync_trampoline.h`, `platform_posix.h`)
- Inclusão de `ref_count` na estrutura `ExtraHandle` com suporte a incremento em chamadas `DuplicateHandle`.
- Implementação de `SwitchToThread` via `sched_yield`, `ExitThread` via `pthread_exit` e QPC/QPF como funções inline `BOOL`.

### Pré-compilador de Shaders Nativo (`astral_chain/shader_cache.py`, `shader_precompile.c`)
- Inclusão dos cabeçalhos do projeto e Vulkan (`third_party/Vulkan-Headers/include`) na chamada do compilador.
- Vinculação das bibliotecas `-lvulkan`, `-lSDL3`, `-lpthread`, `-ldl`, `-lm` e proteção de `<windows.h>` sob `_WIN32`.

### Módulo de Memória Compartilhada POSIX (`extra/core/shared_memory_trampoline.h`)
- Criação de memória compartilhada anônima via chamada de sistema `SYS_memfd_create` e ajuste com `ftruncate`.
- Mapeamento direto de granularidade e reposição de reserva de páginas hóspede através de `mmap(MAP_SHARED | MAP_FIXED)`.

### Rastreamento de Alocação Anônima (`extra/core/memory_trampoline.h`)
- Registro de ponteiros e tamanhos para alocações anônimas em chamadas `VirtualAlloc(NULL, ...)` sem endereço base.
- Liberação correspondente via `VirtualFree(..., 0, MEM_RELEASE)` e preservação de faixa em `MEM_PRESERVE_PLACEHOLDER`.

### Modularização da Interface do Launcher (`Theme.hpp`, `Theme.cpp`, `MainWindowUi.cpp`)
- Extração de regras QSS e de instanciação de widgets para submódulos isolados limitando arquivos a <= 300 linhas.
- Preservação da estética visual e do pipeline de sinais sem ultrapassar o teto estipulado de código por arquivo.

### Vinculação e Retenção de Símbolos NVN ELF (`hle.h`, `platform_posix.h`, `native_trace.py`)
- Adição de atributo `retain` em `hle_entries` prevenindo descarte de tabela de funções pelo linker sob `--gc-sections`.
- Persistência das definições de `AC_NVN_HLE_LINKED` e `AC_GLSLANG` no arquivo de ambiente `run-env.txt`.

### Resolução de Executável Glslang (`extra/core/shaderc_posix.h`)
- Busca de fallback sequencial por variável de ambiente, caminho relativo local e utilitários instalados no sistema.
- Disponibilização contínua de compilador de shaders prevenindo interrupção na inicialização do subsistema Vulkan.

### Execução de Compilador de Shaders sem Pipes (`extra/core/shaderc_posix.h`, `platform_posix.h`)
- Substituição de pipes anônimos por arquivos transitórios para entrada e logs, prevenindo deadlock e interrupção `SIGPIPE`.
- Instalação de manipulador construtor com `signal(SIGPIPE, SIG_IGN)` garantindo resiliência em escritas com descritor encerrado.

### Sincronização de Áudio e Vídeo & Compensação de Underrun (`extra/core/audio_trampoline.h`)
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

### Rasterização de Prompts de Botões e Fontes POSIX (`extra/core/font_trampoline.h`, `button_prompts.c`)
- Implementação de `extra_cut_text` substituindo a dependência Win32 GDI (`gdi32.dll`) por carregamento dinâmico do FreeType (`libfreetype.so.6`) com fallback matricial 8x8 integrado.
- Centralização horizontal e vertical de glifos nos keycaps (`[bx0, by0]..[bx1, by1]`) com corte nítido de opacidade e suavização por supersampling (SS=4).
- Suporte a seleção de prompts no Launcher (`AC_PROMPTS=auto`, `switch`, `keyboard`), permitindo alternar dinamicamente entre ícones originais do Nintendo Switch e teclas de PC (letras e botões de mouse).

### Tradução de Instruções Maxwell Compute e Correção de Iluminação (`extra/core/maxwell_extra.h`, `maxwell_ext.h`)
- Identificação da falha de tradução nos compute shaders `72ac59bb00` (`36d7008000173737`) e `72ac5a0900` (`18ff7f8400070709`) associados aos passes de culling e iluminação de superfícies dinâmicas.
- Mapeamento e implementação das instruções da família `LEA` na arquitetura NVIDIA Maxwell GM20B (`LEA` imm20 `0x36d0...`, `LEA.HI` cbuf `0x1800...`, `LEA` cbuf `0x4bd0...` e `LEA` reg `0x5bd0...`) com cálculos de deslocamento de 64-bit e compilação limpa em SPIR-V.
- Inclusão isolada em `extra/core/maxwell_extra.h` (30 linhas) através de hook único de 1 linha em `astral_chain/runtime/las/maxwell_ext.h`.

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

#### 8. Pacing Dinâmico e Controle de Taxa de Quadros (`extra/core/pace_trampoline.h`, `AC_FPS`, `Config`, `SettingsDialog`)
- **Problema**: Alterar o Target FPS no launcher (para cima ou para baixo de 60) não surtia efeito: a taxa de quadros permanecia fixa em 60 FPS mesmo em modo imediato (`Present Mode: IMMEDIATE`).
- **Causas**:
  1. A rotina de apresentação `nvn_present_pace` em `astral_chain/runtime/nvn_hle.h` utilizava uma grade rígida de 60 Hz (`period = 320000` ticks @ 19.2 MHz) que forçava cada quadro a esperar pelo menos 1 slot de 1/60s.
  2. A rotina `Config::buildLaunchEnv()` injetava `AC_PRESENT_INTERVAL=1` de forma fixa e não propagava a propriedade `m_graphics.fpsLimit`.
- **Resoluções**:
  1. Criação do trampoline desacoplado `extra/core/pace_trampoline.h` com leitura de `AC_FPS` / `AC_TARGET_FPS` e cálculo dinâmico do período de quadro (`19200000 / current_fps`), suportando taxas arbitrárias (30..240 FPS) ou desativação de pacing (`AC_FPS=0`).
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
  4. Alinhamento de `astral_chain/run_guarded.sh` para usar `mailbox` como padrão.

#### 10. Refatoração de Heap Mirror e Eliminação de Cópias Inúteis de Compute (`extra/core/heap_mirror_extra.h`, `heap_mirror.c`, `platform_posix.h`, `pace_trampoline.h`)
- **Problema**: Telemetria registrava `write-back 3939 MB in 2226` a cada 2 segundos (~2 GB/s de tráfego de PCIe contínuo e `memcpy` na CPU). Toda submissão com shaders compute (estágio 5) gravando em storage buffers (listas de agrupamento de luzes de 9.6 MB e 16.8 MB, culling) alocava staging buffers via `vmaCreateBuffer` e copiava esses dados de volta para a memória guest, mesmo que o CPU do jogo nunca lesse esses dados (eles são consumidos exclusivamente por fragment shaders na GPU). Adicionalmente, `Sleep(0)` mapeado para `usleep(0)` em `platform_posix.h` girava sem ceder CPU, queimando mais de 2 núcleos no pacing.
- **Resoluções**:
  1. **Constantes Arquiteturais e Anotações Válidas (`extra/core/heap_mirror_extra.h`)**:
     - Definição de constantes limpas (`EXTRA_PAGE_SIZE_4K`, `EXTRA_PAGE_MASK_4K`, `EXTRA_PAGE_SHIFT_4K`, `EXTRA_MAX_DIRTY_PAGES`, `EXTRA_MAX_WB_PENDING`, `EXTRA_MAX_WB_FLIGHT`, etc.), eliminando números mágicos espalhados.
     - Documentação técnica detalhada sobre o pipeline de espelhamento e controle de coerência.
  2. **Eliminação de Cópias Redundantes (`extra_wb_should_skip`, `LARECOMP_VK_WB`)**:
     - Implementação de filtro ativo em `gmem_wb_note`: buffers de compute $\ge 1\text{ MB}$ (como as listas de luzes de 9.6 MB e 16.8 MB) são ignorados para write-back, pois seus dados já residem no buffer `gmem_buf` da GPU.
     - Redução medida do volume de write-back de **3.939 MB para 9-11 MB a cada 2 s** (**redução de 99.72%**), eliminando ~2 GB/s de PCIe e CPU memcpy.
     - Suporte a controle dinâmico via `LARECOMP_VK_WB` (`0`/`none`/`off` para desativação total, `all`/`legacy` para comportamento legado) e `LARECOMP_VK_WB_THRESHOLD`.
  3. **Fast-Path Sem Contenção de Lock (`gmem_writeback_apply`)**:
     - Verificação antecipada com `extra_wb_has_work`, evitando aquisição desnecessária de `lazy_lock` em submissões sem write-back pendente.
  4. **Semântica POSIX de Espera e Hybrid Pacing (`platform_posix.h`, `pace_trampoline.h`)**:
     - Correção de `Sleep(0)` para `sched_yield()` e `Sleep(ms>0)` para `nanosleep` bloqueante real em `extra/core/platform_posix.h`.
     - Implementação de hybrid wait em `extra/core/pace_trampoline.h`, dormindo via `nanosleep` quando restam $> 300\,\mu\text{s}$ e aplicando spin curto nos últimos microssegundos para precisão sub-milissegundo, recuperando mais de 2 núcleos de CPU.
