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
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `#include "extra/hooks.h"` | `extra/hooks.h` | Inclusão de cabeçalho unificado de hooks | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `window_size` | `extra/core/window_trampoline.h` | Obtenção de dimensões em pixels da janela | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `create_instance` | `extra/core/vulkan_trampoline.h` | Consulta de extensões Vulkan do SDL3 | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `window_init` | `extra/core/vulkan_trampoline.h` | Criação de superfície Vulkan via SDL3 | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `vk_walk_files` | `extra/core/storage_trampoline.h` | Enumeração recursiva de arquivos via POSIX | 3 lines |
| `astral_chain/runtime/las/nvn/vulkan/device.c` | `vk_init_for_warm` | `extra/core/vulkan_trampoline.h` | Janela oculta para aquecimento de shaders | 3 lines |
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
| `astral_chain/runtime/las/nvn/vulkan/exec.c` | `gmem_writeback_apply` / `guest_direct` / `vk_present_draw` | `extra/core/vulkan_trampoline.h` | Eager writeback no Linux (0 faltas SIGSEGV), bypass de cópia de vértices e scissor dinâmico | 8 lines |
| `astral_chain/runtime/las/nvn/vulkan/textures.c` | `mark_dynamic` | `extra/core/vulkan_trampoline.h` | Amostragem FNV para texturas de vídeo dinâmicas | 5 lines |
| `astral_chain/runtime/las/settings.h` | `settings_load` | `extra/ui/launcher` | Suporte a variável de ambiente `AC_UPSCALER=fsr1` | 4 lines |
| `astral_chain/runtime/hid_service.h` | `hid_xinput` | `extra/core/window_trampoline.h` | Leitura de gamepad via SDL3 no Linux | 4 lines |
| `astral_chain/runtime/pl_hle.h` | `pl_load_fonts` | `extra/core/platform_posix.h` | Caminhos de fontes padrão do sistema Linux | 5 lines |
| `astral_chain/setup_game.py` | `fingerprint` / `try_fast_relink` | `extra/tools/qa_runner.sh` | Vinculação incremental rápida e hash de `extra/` | 8 lines |
| `astral_chain/runtime/las/maxwell_ext.h` | `#include "extra/core/maxwell_extra.h"` | `extra/core/maxwell_extra.h` | Tradução de instruções compute Maxwell (família LEA) | 1 line |

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


