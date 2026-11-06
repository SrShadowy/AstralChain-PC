# Arquitetura do AstralChain-PC (Port Linux)

Este documento descreve detalhadamente o funcionamento interno, os subsistemas técnicos e a organização do código do projeto **AstralChain-PC**.

O projeto é um port nativo para Linux de *Astral Chain* (Nintendo Switch, 2019) baseado em **recompilação estática**, projetado sob a diretriz **LINUX FIRST**: o foco primordial de engenharia é a estabilidade, performance e ergonomia no ecossistema Linux moderno (Vulkan 1.3, SDL3, Qt6, C++20 e POSIX). O repositório original de Windows (`Puddz/AstralChain-PC`) serve como base histórica e não recebe mais atualizações frequentes ou relevantes, portanto o projeto não compromete sua arquitetura para manter compatibilidade com Windows.

---

## 1. Visão Geral da Arquitetura

Diferente de emuladores convencionais ou interpretadores JIT, o projeto não traduz instruções em tempo de execução:

1. **Recompilação Estática (AOT)**: O código de máquina ARM64 do executável do Switch (`main`, `rtld`, `sdk`) é traduzido previamente para código-fonte em linguagem C.
2. **Compilação Nativa Linux**: O código C gerado é compilado junto com a camada de runtime diretamente para um binário ELF nativo x86_64 utilizando o compilador Clang do Zig.
3. **Execução Direta no Host**: As instruções rodam nativamente na CPU à velocidade máxima, sem overhead de emulação.
4. **Camada Gráfica Vulkan 1.3**: As chamadas gráficas da API NVN do Switch são traduzidas diretamente para Vulkan 1.3 via LASRecomp. O projeto não utiliza nem planeja suporte a DirectX 12.
5. **Organização Pragmática**: O código customizado reside principalmente em `extra/` (`core/`, `ui/launcher/`, `tools/`), integrando-se ao runtime através de trampolines modulares e hooks pontuais quando estritamente necessário.

```
[Nintendo Switch Dump]
  ├── ExeFS (main, rtld, sdk) ──> [LASRecomp Lifter] ──> [Código C por Páginas]
  └── RomFS (update v1.0.1)   ──> [romfs_tool.c]    ──> [game/romfs.bin]
                                                              │
[Compilador Zig Clang] <──────────────────────────────────────┘
  ├── ~122 Units em C (Lógica do Jogo)
  ├── Runtime Horizon & HLE Services (C)
  ├── NVN on Vulkan Layer (C / Vulkan 1.3)
  └── Platform Trampolines (extra/{platform,sdl,gpu,input,storage}/ - C++20 / SDL3 / POSIX)
       │
       ▼
[Binário Nativo ELF x86_64: build/game/round-00/trace]
```

---

## 2. Pipeline de Extração, Tradução e Compilação

O ciclo de preparação do jogo é orquestrado por scripts Python e utilitários auxiliares:

### A. Extração de Ativos (`astral_chain/build/setup_game.py`)
- O usuário fornece seu arquivo de chaves (`prod.keys`), o dump base do jogo (NSP ou XCI, title ID `01007300020FA000`) e a atualização v1.0.1 (NSP, title ID `01007300020FA800`).
- A ferramenta `nstool` descompacta as NCAs.
- O utilitário nativo compilado `romfs_tool.c` mescla a árvore da atualização sobre a base e empacota o sistema de arquivos virtual em um único arquivo de imagem contígua: `game/romfs.bin`.

### B. Aplicação de Patches Pchtxt
Antes da etapa de tradução, modificações binárias pontuais são aplicadas ao executável `main` descompactado:
- **Dynamic-FPS (`1.0.1.pchtxt`)**: Desvincula a taxa de atualização da física interna, permitindo taxa de quadros alvo de 60 FPS com passo de tempo medido pelo frame real.
- **Dynamic-Unlocked (`dynamic-unlocked-1.0.1.pchtxt`)**: opcional (`AC_FPS_UNLOCKED=1`), aplicado depois do Dynamic-FPS. Duas palavras removem o cap de 60 FPS do mod, deixando o passo medido valer de 15 a 480 FPS. Detalhes em [`docs/fps-cap-144.md`](fps-cap-144.md).
- **Cutscene-Catchup (`cutscene-catchup-1.0.1.pchtxt`)**: Permite que cutscenes compensem atrasos pontuais com passos de tempo maiores para preservar sincronização labial e sonora.

### C. Tradução Estática ARM64 → C (`astral_chain/translator/native_trace.py`)
- Utiliza o motor estático `lifter.py` do LASRecomp.
- Todo o código executável dos segmentos `.text` é convertido página por página (4 KB por página de memória guest).
- Cada página torna-se uma função em C no formato `void page_XXXX(Cpu *c)`.
- Opções fundamentais do build:
  - `--lean`: Remove contadores de passo e tracing verboso de chamadas.
  - `--direct-calls`: Chamadas de ramificação com link (`BL`) invocam diretamente a página de destino em C em vez de passar por um despachante indireto.
  - `--jumps`: Saltos condicionais e incondicionais internos à mesma página são convertidos em instruções `goto` do C.

### D. Compilação Nativa (`astral_chain/build/native_build.py`)
- As unidades geradas (~122 arquivos C de código de jogo), os arquivos de serviços do runtime (`runtime/host_runtime.c.in`) e a camada NVN Vulkan (`runtime/las/`) são compilados com:
  ```bash
  zig clang -O2 -march=x86-64-v3 -target x86_64-linux-gnu ...
  ```
- Os arquivos-objeto compilados são cacheados sob `build/native-objects/` utilizando hashes SHA-256 de conteúdo, permitindo o Fast-Relink (recompilação apenas do runtime em ~8s sem reconstruir as unidades do guest).

---

## 3. Runtime Horizon e Componentes HLE

O runtime emula em nível de host (HLE — High-Level Emulation) as chamadas de sistema (SVCs) do microkernel Horizon e os serviços do sistema operacional da Nintendo:

- **SVCs (Supervisor Calls)**: Interceptam manipulação de memória, semáforos, portas IPC e criação de threads hóspedes.
- **Threads Hóspedes (`runtime/kernel/guest_threads.h`)**: Threads do guest rodam em paralelo em threads nativas do sistema operacional host sob a flag `AC_PARALLEL=1`, coordenada por estruturas de sincronização POSIX em Linux (`extra/platform/sync_trampoline.h`).
- **Sistema de Arquivos e Saves (`runtime/storage/save_store.h`, `runtime/storage/save_fs.h`)**:
  - `romfs_storage.h` mapeia o arquivo `game/romfs.bin` em memória somente leitura.
  - As operações de salvamento de estado do jogo são mapeadas para o diretório nativo `userdata/saves/<title>/<profile>/` via `extra/storage/storage_trampoline.h`.
- **Serviços de Sistema**:
  - `time_service.h`: Fornece timestamps baseados em relógios monotônicos do sistema (`CLOCK_MONOTONIC`).
  - `account_service.h`: Gerencia perfis e credenciais fictícias de usuário local.
  - `vi_service.h`: Gerencia a camada de exibição e eventos de sincronização vertical (V-Sync).

---

## 4. Camada Gráfica NVN e Renderização Vulkan 1.3

O Nintendo Switch utiliza a API proprietária NVN para comunicação com a GPU Maxwell da NVIDIA. O projeto implementa essa API integralmente e exclusivamente sobre o **Vulkan 1.3**:

### A. Interceptação e Despacho
- A inicialização do subsistema gráfico intercepta a função de carregamento `nvnBootstrapLoader`.
- As chamadas NVN de manipulação de buffers de comando, render targets, barreiras e submissão são convertidas em equivalentes Vulkan pela camada LASRecomp (`runtime/las/nvn/vulkan/`).

### B. Tradução e Cache de Shaders
- Os binários compilados de shader da arquitetura Maxwell são traduzidos para GLSL compatível.
- A biblioteca `glslang` embutida compila o GLSL intermediário para bytecode SPIR-V.
- O cache de shaders SPIR-V é gravado em `build/game/cache/` e reutilizado entre execuções.
- As variantes de pipelines conhecidas são distribuídas em `data/pipeline_variants.bin` e `data/shader_variants.bin`, evitando engasgos por compilação durante o gameplay.
- **Warm-up Antecipado (`AC_GPU_WARM=1`)**: Antes de iniciar a sessão principal, o launcher aciona uma rotina que pré-carrega todos os pipelines conhecidos na GPU.
- **Shader Libraries (`VK_EXT_graphics_pipeline_library`)**: Em GPUs com suporte a essa extensão, os estágios de shader são compilados separadamente em background, reduzindo o stutter ao compilar pipelines novos.

### C. Decodificação de Texturas ASTC & BC7
- O hardware de desktop não possui suporte nativo para amostragem de texturas no formato móvel ASTC.
- **Decodificador GPU ASTC (`astc_gpu.c`)**: Um compute shader no Vulkan decodifica os blocos ASTC descompactados diretamente para texturas RGBA8 na memória de vídeo. O decodificador em CPU (`astc.c`) permanece disponível como fallback.
- **Compressão BC7 (`bc7_gpu.c` / `bc7_cache.py`)**: Sob a flag `LARECOMP_BC7=1`, as texturas decodificadas podem ser recompactadas em formato BC7 (reduzindo em até 50% o consumo de VRAM em relação ao RGBA8 puro).

---

## 5. Plataforma Linux: Integração com SDL3, Áudio e Entrada

O port para Linux substitui a infraestrutura baseada em Win32 por tecnologias modernas e portáveis:

### A. Janela e Superfície de Renderização
- **SDL3 (`extra/sdl/window_trampoline.h`)**:
  - Criação de janelas nativas via `SDL_CreateWindow`.
  - A superfície Vulkan é criada diretamente através de `SDL_Vulkan_CreateSurface`, compatível tanto com Wayland quanto com X11.
  - Alternância de tela cheia sem bordas (`app_set_fullscreen`) e captura de foco.

### B. Subsistema de Áudio
- **SDL3 Audio Stream (`extra/sdl/audio_trampoline.h`)**:
  - O jogo original gera amostras PCM em ponto flutuante de 32 bits (48 kHz estéreo).
  - No Linux, o trampoline intercepta as chamadas em `runtime/audio/wave_output.h` e roteia as amostras diretamente para um `SDL_AudioStream` vinculado ao dispositivo padrão de áudio, eliminando a dependência do `waveOut` de Windows.

### C. Entrada e Controles (Gamepad, Teclado e Mouse)
- **Captura SDL3 e Emulação HID**:
  - O bombeamento de eventos do loop de jogo (`app_pump`) consome a fila de eventos do SDL3.
  - Gamepads são gerenciados nativamente via `SDL_Gamepad`, mapeando botões para o protocolo virtual de HID do Switch.
  - Suporte ergonômico a Teclado e Mouse (WASD para movimento, botão direito para Legion, botão esquerdo para ação e movimento relativo de mouse convertendo para deslocamento do analógico direito com compensação de zona morta via `runtime/input/mouse.h`).
- **Substituição de Prompts de Botão (`extra/ui/font_trampoline.h`)**:
  - A renderização de fontes de interface via GDI do Windows foi substituída por renderização nativa de glifos via **FreeType**, desenhando dinamicamente prompts de teclado/mouse ou botões Switch no atlas de UI.

### D. Memória Compartilhada HID e o que o jogo realmente lê (`nn::hid`)

O port publica, na memória compartilhada HID, os anéis de entrada que o `nn::hid` do SDK consome.
A engenharia reversa de 2026-11-01 mediu **quais** desses anéis o jogo lê por frame — e a resposta
mudou o desenho da entrada de mouse.

- **Call sites vivos no `main`** (relocações `R_AARCH64_JUMP_SLOT`, kind 1026), um por API:
  `GetMouseState` (`main+0x9acafc`), `GetKeyboardState` (`main+0x9ac9cc`),
  `GetTouchScreenState<16>` (`main+0x9adb1c`), `GetNpadState` (`main+0x9ae50c`).
- **O mouse é lido por frame, mas nada consome o resultado.** A cadeia por frame é
  `0x3e4028 → 0x3e4120 → 0x577000 → 0x9acd50 → {0x9ac1d0 touch, 0x9ac980 teclado, 0x9acad0 mouse}`.
  O wrapper do mouse (`0x9acad0`) constrói um objeto em `*(0xdc2540)` e **nenhum código do binário
  lê os campos dele**: os 14 acessos via ponteiro são o próprio writer e um reset em `0x9ac180`.
  Não há hit-testing de mouse na UI para alimentar, e o jogo nunca envia o comando HID 21, então
  `MouseState` permanece zerado.
- **Não existe flag de UI do jogo.** `[0xdcc434]` é o modo single-thread do loop de frame (gate de
  `0x577000`), não estado de menu; `command 201` do IPC HID é polling de vibração. O `main` importa
  `nn::oe::GetCurrentFocusState` (foco da aplicação), não estado de interface. Também não expõe
  geometria de item nem índice selecionado, o que impede seleção posicional absoluta.
- **Layout dos anéis do SDK**: `RingLifo<TouchScreenState<16>>` `ReadImpl` em `sdk+0x233f70`,
  stride `0x1a0`, seq em `entry+0x20`, estado em `entry+0x28`, `tail` `+0x10`, `count` `+0x18`;
  anel de mouse global `0xc01648`, stride `0x30`, `MouseState` em `entry+0x28`.
- **Offset padrão Switch** (`hid_service.h`): touch `0x400`, mouse `0x3400`, keyboard `0x3800`,
  npad `0x9a00`. Os `_Static_assert` (`HidTouchSample==0x290`, `HidTouchRing==0x2c38`,
  `MouseRing==0x350`) conferem com o SDK — sem bug de offset.

Consequência prática: o anel de mouse continua sendo publicado (ABI correto, custo nulo), mas
**nenhum menu pode ser alimentado por ele**. O ponteiro de menu em `extra/input/mouse_menu.h`
traduz movimento e cliques no canal que os menus realmente obedecem — o D-pad —, com contato de
touch opcional para telas que venham a fazer hit-test do painel (`AC_MOUSE_MENU_TOUCH=1`).
Seleção posicional absoluta não é implementável sem geometria exposta pelo jogo; ver
[`docs/plan-mouse-menu.md`](plan-mouse-menu.md) §4.4.

---

## 6. Histórico de Origem: Windows (Legado) vs Linux (Downstream Nativo)

Para fins de documentação técnica sobre as origens do código herdado, a tabela a seguir resume as substituições efetuadas:

| Componente | Upstream Windows (Legado / Inativo) | Linux First (Implementação Atual) |
|---|---|---|
| **Rastreio de Memória GPU (Write-Watch)** | Win32 `GetWriteWatch` | POSIX `mprotect` + tratador de sinal `SIGSEGV` em `extra/platform/memory_trampoline.h` |
| **Alocação de Memória Virtual** | `VirtualAlloc` / `VirtualProtect` | POSIX `mmap` (`MAP_NORESERVE \| MAP_FIXED_NOREPLACE`) e `mprotect` |
| **Janela & Display** | Win32 GDI / Mensagens HWND | SDL3 Window (`SDL_CreateWindow`) e `SDL_Vulkan_CreateSurface` |
| **Saída de Áudio** | API Win32 legada `waveOut` | SDL3 Audio Stream (`SDL_CreateAudioStream`, float32) |
| **Entrada e Gamepad** | Win32 Raw Input e XInput | Eventos SDL3 e `SDL_Gamepad` nativo com hot-plug |
| **Prompts de Botões de Interface** | Win32 GDI Font Rendering | Renderizador nativo FreeType (`extra/ui/font_trampoline.h`) |
| **Sincronização e Threads** | `CreateThread`, `WaitForSingleObject` | POSIX Threads (`pthread_create`, `pthread_mutex`, `pthread_cond`) |
| **Launcher do Jogo** | C# (.NET Framework 4 / WinForms - **Não compilado/usado no Linux**) | Aplicação nativa C++20 Qt6 (`extra/ui/launcher/`) |
| **Watchdog de Falhas** | PowerShell lendo Windows System Event Log | Watchdog interno do renderer via `AC_GPU_HANG_MS` |

---

## 7. Estrutura do Código e Fronteiras C / C++

```text
AstralChain-PC/
├── astral_chain/                  # Pacote Python + runtime C (organizado por módulos)
│   ├── patches/                   # Patches binários Pchtxt (Dynamic-FPS, Cutscenes)
│   ├── data/                      # Variantes de pipeline/shader pré-computadas
│   ├── shared/                    # Formatos e contêineres (containers.py, nso.py)
│   ├── translator/                # "Tradutor": pipeline de recompilação estática ARM64 -> C
│   │   ├── native_trace.py        # Orquestrador do lifter (sys.path -> third_party/.../translator)
│   │   ├── native_probe.py        # Sonda/verificação do binário e do pin LASRecomp
│   │   ├── block_codegen.py / lift_extensions.py / static_targets.py / linker.py
│   │   └── runtime_memory.py / process_meta.py / control_meta.py / account_profile.py / save_config.py
│   ├── build/                     # Setup e compilação nativa
│   │   ├── setup_game.py          # CLI principal (extração, romfs, fast-relink)
│   │   ├── native_build.py        # Compilador Zig clang + runtime_include_flags()
│   │   └── romfs_image.py         # Imagem RomFS (constantes e inspeção)
│   ├── gpu/                       # Camada NVN/Vulkan e shaders (lado Python)
│   │   ├── nvn_build.py           # Objetos cacheados da camada NVN (runtime/las)
│   │   ├── shader_build.py / shader_cache.py / shader_replay.py
│   │   └── bc7_cache.py / astc_gpu_test.py / bc7_gpu_test.py
│   ├── tools/                     # Utilitários (reports, input_scripts, run_guarded.sh)
│   ├── setup_game.py              # Shim de compatibilidade -> build.setup_game
│   ├── native_trace.py            # Shim de compatibilidade -> translator.native_trace
│   └── runtime/                   # Núcleo de serviços HLE e kernel Horizon (em C)
│       ├── host_runtime.c.in      # Ponto de montagem principal (inclui todos os módulos por bare-name)
│       ├── gpu/                   # Canais, engine NVDRV, nvmap, buffers de display, NVN HLE
│       ├── audio/                 # audout/audren/hwopus + saída wave (redirecionada via hook)
│       ├── input/                 # HID service, teclado, mouse, applets, memória compartilhada
│       ├── storage/               # RomFS, saves, storages de applet
│       ├── services/              # account/time/vi/nifm/prepo/pl e janela (app_window)
│       ├── kernel/                # IPC, threads hóspedes, memória de stack/transferência
│       ├── tools/                 # Ferramentas/probes em C (romfs_tool, shader_*, *_test)
│       └── las/                   # Camada gráfica NVN para Vulkan (C)
│           └── nvn/vulkan/        # Renderizador Vulkan 1.3, shaders e texturas
│
├── extra/                         # Código downstream nativo para Linux (por módulos)
│   ├── hooks.h                    # Re-exportação unificada de hooks
│   ├── platform/                  # POSIX: mmap/SIGSEGV, pthreads, tempo, pacing, crypto, ELF
│   ├── sdl/                       # Janela/superfície Vulkan SDL3 + Audio Stream SDL3
│   ├── input/                     # Modo de mouse, ponteiro de menus, detecção de contexto, vk_codes
│   ├── gpu/                       # Trampolines Vulkan/Maxwell, write/vtx tracking, shaderc POSIX
│   ├── storage/                   # Paths POSIX, normalização de saves, find/glob de diretórios
│   ├── ui/                        # font_trampoline.h (FreeType), launcher/ Qt6, overlay/ ImGui
│   ├── toggles/                   # ac_toggles.c/h (kill switches e overrides de runtime)
│   └── tools/                     # Scripts de verificação (audit.sh, qa_runner.sh)
│
├── third_party/                   # Dependências externas vendoradas (LASRecomp, etc.)
│   └── LASRecomp/                 # Motor de recompilação estática (pin em COMMIT)
│       ├── translator/            # lifter.py, simd_c.py (núcleo ARM64 -> C)
│       └── runtime/               # cpu/ (cpu.h, simd), kernel/ (ipc, kernel, settings),
│                                  # build/ (romfs_build, gamebuild), hle/, nvn/, services/...
├── launcher.sh, setup.sh, play.sh # Scripts de execução e ambiente Linux
└── SHADOW_CHANGES.md              # Ledger de auditoria de alterações no código herdado
```

Notas de organização (2026-11-02):
- O pacote Python foi dividido por módulo (`shared/`, `translator/`, `build/`, `gpu/`, `tools/`). Shims finos na raiz do pacote preservam `python -m astral_chain.setup_game` e `python -m astral_chain.native_trace`.
- `astral_chain/runtime/` e `extra/` foram divididos por domínio; os includes seguem por bare-name resolvidos via `-I` por subpasta (`runtime_include_flags()` em `build/native_build.py`).
- Em `third_party/LASRecomp/` (vendorado): o lifter vive em `translator/` e o runtime em subpastas por domínio (`cpu/`, `kernel/`, `build/`, `input/`, `hle/`, `nvn/`, `services/`, `ui/`, `platform/`). A verificação de pin (`verify_reference`) lê apenas o arquivo `COMMIT` da raiz.

### Regras de Fronteira:
- O código gerado pela recompilação e o runtime central permanecem em **C** para máxima velocidade e interoperabilidade com o lifter.
- A camada downstream em `extra/` adota **C++20** moderno com RAII, tipos explícitos e namespaces (`ac::launcher`).
- Toda comunicação entre o runtime em C e as implementações em C++ em `extra/` é feita através de funções declaradas com vinculação `extern "C"`.
- O launcher em C# de Windows pertencente ao upstream original é **completamente ignorado** no Linux; a interface oficial é o Launcher Qt6 C++20.

---

## 8. Matriz de Status dos Componentes

| Componente | Status | Detalhes Técnicos |
|---|---|---|
| **Recompilação Estática (ARM64 → C)** | **Implementado** | Lifter LASRecomp funcional com flags `--lean --direct-calls --jumps`. |
| **Compilação Zig Clang (ELF x86_64)** | **Implementado** | Binário nativo gerado e linkado diretamente para Linux. |
| **Pipeline Gráfico Vulkan 1.3** | **Implementado** | Comandos NVN mapeados para Vulkan, shaders SPIR-V funcionais. Sem DirectX 12. |
| **Superfície & Janela SDL3** | **Implementado** | Suporte a fullscreen, redimensionamento e apresentação Vulkan (Wayland e X11). |
| **Áudio Stream SDL3** | **Implementado** | Fluxo PCM 48 kHz float32 estéreo substituindo waveOut. |
| **Entrada Teclado/Mouse & Gamepad** | **Implementado** | Mapeamento completo SDL3 com sensibilidade de câmera e zona morta. |
| **Launcher Oficial Qt6 C++20** | **Implementado** | Configurações gráficas, tela cheia, arte de fundo e setup integrados. |
| **Fast-Relink de Runtime** | **Implementado** | `setup_game.py --rebuild-if-stale` recompila runtime em ~8s. |
| **Write-Watch via `mprotect` + `SIGSEGV`** | **Parcialmente implementado** | Funcional no ciclo de jogo normal; monitorado em cenários de alta paginação de textura. |
| **Compressão de Texturas BC7** | **Parcialmente implementado** | Decodificação GPU ASTC implementada; modo BC7 pré-computado funcional. |
| **Compatibilidade Nativa Wayland Tearing** | **Parcialmente implementado** | Opera de forma estável; suporte a tearing/VRR explícito depende de extensões do compositor. |
| **Utilitário Monolítico C++ (`acr`)** | **Planejado** | Hipótese proposta no passado para unificar scripts Python; atualmente dispensável frente à agilidade do Fast-Relink. |
| **Testes Automatizados Golden Files** | **Planejado** | Estrutura de golden hashes (`tests/golden/`) para validar determinismo do lifter. |
| **Estabilidade em GPUs Intel / Nouveau / NVK** | **Ainda não verificado** | Validado prioritariamente em GPUs NVIDIA e AMD Mesa (RADV); comportamento em outros drivers ainda não testado extensivamente. |
| **Rastreio de Memória via `userfaultfd`** | **Ainda não verificado** | Hipótese alternativa ao `SIGSEGV`, dependente de permissões do kernel host. |
| **Campanha Completa de Jogo sem Falhas** | **Ainda não verificado** | Cenários de gameplay avançado e fases finais ainda demandam validação prolongada de gameplay no Linux. |
