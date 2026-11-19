# Roadmap Técnico da Adaptação Linux (AstralChain-PC)

Este documento estabelece o planejamento técnico, dependências, etapas de execução e critérios de aceitação para o port nativo Linux do **AstralChain-PC**, operando sob a diretriz **LINUX FIRST**.

O objetivo primordial é assegurar uma execução nativa estável, de alta performance e livre de falhas no ecossistema Linux moderno (Vulkan 1.3, SDL3, Qt6, C++20, glibc). O upstream original de Windows (`Puddz`) é tratado como base histórica inativa, não havendo esforço alocado para manter compatibilidade com Windows ou APIs não utilizadas (como DirectX 12).

---

## 1. Classificação do Escopo

Todas as frentes de trabalho são classificadas em quatro categorias explícitas:

1. **Requisitos Obrigatórios (Linux First)**: Itens essenciais para que o jogo compile nativamente no Linux, execute sem emuladores e atinja estabilidade e performance plenas via Vulkan 1.3 e SDL3.
2. **Tarefas Planejadas**: Implementações arquiteturais já decididas que agregam robustez, ergonomia ou melhorias comprovadas para usuários e desenvolvedores Linux.
3. **Melhorias Opcionais**: Funcionalidades de conveniência ou refinamentos secundários (ex.: upscaling espacial, vibração avançada) que não bloqueiam a experiência de jogo.
4. **Hipóteses sob Investigação**: Propostas técnicas alternativas que necessitam de comprovação empírica ou testes de viabilidade antes de qualquer adoção.

> [!NOTE]
> Propostas como suporte a DirectX 12, reescrita em HLSL ou manutenção de scripts/compiladores para Windows foram descartadas por serem irrelevantes para o objetivo do projeto.

---

## 2. Etapas do Roadmap

---

### Etapa 1: Isolamento de Dependências Win32 Herdadas
- **Objetivo**: Identificar e redirecionar chamadas Win32 herdadas para os trampolines de plataforma sob `extra/` (módulos `platform/`, `sdl/`, `gpu/`, `input/`, `storage/`).
- **Dependências**: Análise do código herdado em `astral_chain/runtime/`.
- **Tarefas**:
  - [x] Redirecionar manipulação de memória virtual Win32 (`VirtualAlloc`/`VirtualProtect`) para POSIX `mmap`/`mprotect`.
  - [x] Substituir rastreio de memória GPU (`GetWriteWatch`) por tratador lazy `SIGSEGV`.
  - [x] Redirecionar saída de áudio `waveOut` para SDL3 Audio Stream.
  - [x] Redirecionar criação de janela e eventos Win32 para SDL3.
  - [x] Redirecionar estruturas de thread/sincronização Win32 para `pthreads`.
  - [x] Substituir renderizador de fontes GDI por FreeType nativo.
  - [x] Estabelecer o ledger `SHADOW_CHANGES.md` para governança das alterações.
- **Critérios Objetivos de Aceitação**: Nenhuma dependência Win32 ativa bloqueando a compilação ou execução nativa no Linux.
- **Riscos / Pontos Não Resolvidos**: Nenhum. Etapa concluída.

---

### Etapa 2: Camada de Abstração de Plataforma (HAL / Trampolines)
- **Objetivo**: Fornecer implementações modulares de plataforma sob `extra/` (módulos `platform/`, `sdl/`, `gpu/`, `input/`, `storage/`) desacopladas do código herdado.
- **Dependências**: Etapa 1; cabeçalho de re-exportação `extra/hooks.h`.
- **Tarefas**:
  - [x] Criar `extra/platform/platform_posix.h` com atributos de linkage e tipos portáveis ELF.
  - [x] Implementar `extra/platform/sync_trampoline.h` com `pthread_mutex`, `pthread_cond` e threads nativas.
  - [x] Implementar `extra/storage/storage_trampoline.h` para caminhos POSIX e diretórios de save em `userdata/`.
  - [x] Implementar `extra/platform/time_trampoline.h` com relógio monotônico do kernel Linux (`CLOCK_MONOTONIC`).
  - [x] Desenvolver script de auditoria estática `extra/tools/audit.sh` (teto ≤ 400 linhas e marcadores necessários).
- **Critérios Objetivos de Aceitação**: Todos os trampolines compilam limpamente em C++20 com `audit.sh` passando 100%.
- **Riscos / Pontos Não Resolvidos**: Nenhum. Etapa concluída.

---

### Etapa 3: Subsistema de Memória e GPU Write-Watch
- **Objetivo**: Garantir que as escritas de texturas dinâmicas da CPU sejam rastreadas e sincronizadas na GPU com máxima performance no Linux.
- **Dependências**: Etapa 2; suporte a sinais POSIX e chamadas de sistema do kernel Linux.
- **Tarefas**:
  - [x] Implementar alocação de memória virtual via `mmap` com flags `MAP_NORESERVE` e `MAP_FIXED_NOREPLACE` (`memory_trampoline.h`).
  - [x] Implementar proteção de páginas via `mprotect` e captura de faltas lazy via tratador de sinal `SIGSEGV` em espaço de usuário.
  - [ ] Investigar a viabilidade e latência do mecanismo `userfaultfd` com proteção de escrita (`UFFD_FEATURE_PAGEFAULT_FLAG_WP`) como alternativa sem overhead de sinais (*Hipótese sob Investigação*).
  - [ ] Otimizar tamanho de lote de páginas marcadas para evitar tempestades de sinais em uploads intensivos de texturas dinâmicas.
- **Critérios Objetivos de Aceitação**: Texturas alteradas dinamicamente pela CPU são atualizadas na GPU sem corrupção visual nem travamentos de memória.
- **Riscos / Pontos Não Resolvidos**: `userfaultfd` sem privilégios requer kernel moderno (Linux 5.7+) e configuração de sysctl (`vm.unprivileged_userfaultfd=1`). O tratador atual de `SIGSEGV` atende bem ao gameplay normal.

---

### Etapa 4: Renderização Vulkan 1.3 e Pipeline NVN
- **Objetivo**: Executar a camada gráfica do Switch nativamente em Vulkan 1.3 com alta performance e sem travamentos de shader.
- **Dependências**: Vulkan 1.3 Loader (`libvulkan`), compilador `glslang` e bibliotecas `VulkanMemoryAllocator`.
- **Tarefas**:
  - [x] Vincular a inicialização gráfica a uma superfície Vulkan criada pelo SDL3 (`SDL_Vulkan_CreateSurface`).
  - [x] Habilitar compilação assíncrona de shaders Maxwell para SPIR-V via worker threads dedicados.
  - [x] Suportar a extensão `VK_EXT_graphics_pipeline_library` para montagem modular de pipelines em background.
  - [x] Implementar rotina de warm-up antecipado de GPU antes do início do jogo (`AC_GPU_WARM`).
  - [x] Implementar decodificador de texturas ASTC via compute shader na GPU (`astc_gpu.c`).
  - [x] Suportar modos de apresentação configuráveis no launcher (FIFO/V-Sync, Mailbox/Unlocked, Immediate).
  - [ ] Validar compatibilidade de apresentação com taxa de atualização variável (VRR / G-Sync / FreeSync) sob Wayland nativo (*Planejado*).
- **Critérios Objetivos de Aceitação**: Renderização fluida a 60 FPS estáveis sem falhas de apresentação ou piscar de tela (flicker).
- **Riscos / Pontos Não Resolvidos**: Diferenças sutis de extensões entre drivers Mesa (RADV/ANV) e driver proprietário NVIDIA.

---

### Etapa 5: Subsistemas de Áudio e Entrada via SDL3
- **Objetivo**: Integrar áudio de baixa latência e suporte nativo a múltiplos tipos de controles no Linux.
- **Dependências**: Biblioteca SDL3 (`libsdl3`) instalada no sistema host.
- **Tarefas**:
  - [x] Substituir a saída waveOut por `SDL_AudioStream` em float32 estéreo 48 kHz (`audio_trampoline.h`).
  - [x] Implementar captura de eventos de teclado e mouse com conversão para analógico de controle (`window_trampoline.h`).
  - [x] Implementar mapeamento automático de Gamepads através da API `SDL_Gamepad`.
  - [x] Substituir renderizador de fontes de interface GDI por biblioteca nativa FreeType (`font_trampoline.h`).
  - [ ] Avaliar suporte a efeitos de vibração do controle via SDL3 Force Feedback (*Melhoria Opcional*).
  - [ ] Implementar remapeamento interativo visual de teclas dentro do Launcher Qt6 (*Planejado*).
- **Critérios Objetivos de Aceitação**: Áudio sincronizado com cutscenes sem underrun e resposta instantânea aos comandos de entrada.
- **Riscos / Pontos Não Resolvidos**: Dispositivos de áudio Bluetooth com latência excessiva no pulseaudio/pipewire caso o buffer seja configurado de forma muito restrita.

---

### Etapa 6: Toolchain Linux e Launcher Oficial Qt6
- **Objetivo**: Proporcionar um fluxo de execução e compilação ágil, descartando ferramentas Windows legadas.
- **Dependências**: Python 3.10+, Zig clang, Qt6 e Ninja.
- **Tarefas**:
  - [x] Criar scripts de inicialização nativos para terminal (`setup.sh`, `play.sh`, `play-diagnostics.sh`, `launcher.sh`).
  - [x] Implementar Fast-Relink via cache de objetos em `setup_game.py --rebuild-if-stale` (~8s de compilação).
  - [x] Desenvolver o Launcher nativo Qt6 C++20 (`extra/ui/launcher/`).
  - [x] Descartar dependência do launcher legado em C# de Windows no ambiente Linux.
  - [x] Criar suíte de testes de regressão automatizada `extra/tools/qa_runner.sh` com suporte a modo headless (`SKIP_RUNTIME=1`).
  - [ ] Desenvolver suíte de golden files (`tests/golden/`) para validação estrita do lifter (*Planejado*).
- **Critérios Objetivos de Aceitação**: Ambiente 100% autossuficiente no Linux, sem necessidade de ferramentas ou binários Windows.
- **Riscos / Pontos Não Resolvidos**: Nenhum.

---

### Etapa 7: Profiling e Desempenho
- **Objetivo**: Diagnosticar gargalos de CPU/GPU e eliminar microtravamentos no Linux.
- **Dependências**: Variáveis de diagnóstico (`LARECOMP_VK_PROF`) e ferramentas de profiling (`perf`).
- **Tarefas**:
  - [x] Eliminar gargalo de cópia de vértices através do cache isolado `vcopy` em `exec.c`.
  - [x] Implementar telemetria de percentis P50, P95, P99 e 1% Low de apresentação de quadros em nanosegundos (`SDL_GetTicksNS`) no backend Vulkan.
  - [x] Suportar perfilamento amostral de CPU via `AC_PROFILE=1`.
  - [x] Eliminar picos de latência no agendador paralelo (`THREAD_WAIT_GPU` via `vkWaitSemaphores` direto e `klock_wait_ns` em nanosegundos), corrida *catch-up* no pacer (`pace_trampoline.h`) e churn de alocação VMA via pool de reciclagem (`heap_mirror_extra.h`).
  - [ ] Investigar pausas pontuais na compressão de texturas BC7 sob demanda em GPU (*Planejado*).
  - [x] Implementar upscaling espacial (AMD FSR 1.0) e temporal (AMD FSR 3.1 TAAU + Camera Jitter Halton(2,3) + RCAS + Frame Generation 2x) para máxima nitidez e fluidez.
  - [x] Eliminar gargalo de hash de vertex shader por draw, reconstrução de estados/render-pass e colisão/eviction de `view_tab` em `textures.c` (`Wave 1 & 2`).
  - [x] **Onda A — Desbloqueio de 60 FPS em cenas 3D densas (CPU / HLE / NVN)** (*Concluído*):
    - [x] Profiler amostral POSIX multi-thread via `SIGRTMIN+2` (`AC_LPROF` / `tgkill`) compatível com threads SDL3 (`lprof_report.py`).
    - [x] `--restrict` em `native_trace.py`, `-O2 -mcpu=native` (correção em `try_fast_relink()`), salto direto pós-`BL` (`_DIRECT_CALL_TAIL`), patch dos 33 *spin-wait* back-edges no `main` 1.0.1 (`rt_spin_gate`), `romfs_read()` `pread` fora de `klock()` e `svcSleepThread()` sem busy-spin.
    - [x] Otimização NVN->Vulkan: `feedback.c` extraído (`descriptors/`), cache zero-span UBO (`ZERO_UBO_CACHE_BYTES`), `tex_memo` 2-way, `rscale_span`, cache 2-way de `VkDescriptorSet` + elisão `bound_sets` (`LARECOMP_VK_DESC_CACHE`), redução de re-hash periódico em `textures.c` (`1800` frames). (`PushDescriptor` / `DrawIndexedIndirectCount` dispensados por já atingir `~63.8+ fps` em Harmony Square.)
- **Critérios Objetivos de Aceitação**: Taxa estável de 60 FPS (ou 120+ FPS efetivos com `AC_FPS_UNLOCKED=1` / FSR 3.1 Frame Generation) com frame time linear, 1% Low próximo à média e sem hitches perceptíveis.
- **Riscos / Pontos Não Resolvidos**: Otimizações agressivas em estruturas centrais do runtime ou `--restrict`/PGO podem causar regressões sutis em cutscenes ou aliasing; validar cada subitem em cenários 3D reais e `qa_runner.sh`.

---

### Etapa 9: Consolidação PC, Resolução Interna Avançada e Refinos Visuais (Ondas B & C)
- **Objetivo**: Consolidar recursos de qualidade de vida, escalonamento nativo até 4K e recursos visuais avançados em Vulkan 1.3 sob governança Linux First.
- **Dependências**: Etapa 7 (Onda A concluída para folga de CPU/frame pacing).
- **Tarefas**:
  - [x] **Onda B — Resolução Interna & UX PC** (*Core Concluído; refinamentos opcionais pendentes*):
    - [x] Suporte nativo até 4K (`3840x2160` em grade `32n x 18n`) crescendo os heaps `BUFFER ROOT` (`0x575a30`) e `GRAPHIC BUFFER` (`0x575c94`), budget `0x40e0b0`, headroom em `extra_memory_budget()` (`heap_memory.h`) e mipmap bias (`0x9ed178`).
    - [x] Separação real `Output Res` (`AC_OUTPUT_RES`) × `Scene Res` (`render_scale < 1.0`) via Dynamic-Res ladder (`0x40f184..0x40f1dc`, alinhado em `32n x 18n`), preservando shaders `SUST`/`TLDS` sem escalar render targets coloridos à força no backend.
    - [x] Patches nativos lockless (`SVC #0xB3`) de `Draw Distance` (`100%..400%`, `AC_DRAW_DISTANCE`) e `Shadow Scale` (`1x/2x/4x` no mapa de sombras solar `2048x2048 D24S8` em `textures.c`).
    - [x] UX In-Game & Launcher: atalho rápido No-HUD (`F12` / `AC_HUD` ocultando apenas `hud2`), opção `hud_area` (`16:9` centralizado vs `screen` em `draw.c`), badges `[Env]` / `[Reinício]` no terminal ImGui (`ac_menu_tabs.cpp`) e controles sincronizados no Launcher Qt6 (`Config` / `SettingsDialogTabs`).
    - [ ] Itens opcionais pendentes: entrada `PC Settings` nativa dentro do menu *Options* do jogo e *sub-texel gather bias* (`subTexelPrecisionBits`) no tradutor de shaders.
  - [ ] **Onda C — Refino Visual & Latência (após validação das Ondas A & B)** (*Planejado*):
    - Modo de Baixa Latência (`VK_AMD_anti_lag` no Mesa RADV, `VK_NV_low_latency2` no driver NVIDIA e *sleep* adaptativo built-in) + detecção/auto-lock VRR (`vsync = auto`).
    - Efeitos pós-processamento Vulkan puros: Ambient Occlusion (GTAO fog/ambient-aware) + Contact Shadows + avaliação de GI (Radiance Cascades).
    - Saída HDR10 nativa em compositores Wayland HDR via SDL3 preservando curva de tonemap e HUD em paper-white.
- **Critérios Objetivos de Aceitação**: Escalonamento 4K/Scene sem crashes de heap nem artefatos em `SUST`/`TLDS`, menus in-game estáveis e passes visuais opcionais validados em RADV sem queda abaixo de 60 FPS.
- **Riscos / Pontos Não Resolvidos**: Compatibilidade de extensão `VK_AMD_anti_lag` / HDR Wayland varia conforme versão de Mesa/compositor; manter fallbacks seguros.

---

### Etapa 8: Compatibilidade de Drivers Linux e Testes Estendidos
- **Objetivo**: Assegurar funcionamento uniforme em diversas configurações de hardware e distribuições Linux.
- **Dependências**: Acesso a distribuições Linux com GPUs AMD, NVIDIA e Intel.
- **Tarefas**:
  - [x] Testar e validar compatibilidade em Arch Linux / CachyOS.
  - [x] Testar e validar em Ubuntu 24.04+ e Fedora 40+.
  - [ ] Realizar baterias de teste em GPUs AMD com driver Mesa RADV (*Planejado*).
  - [ ] Realizar testes em GPUs Intel Arc / Xe via driver ANV (*Planejado*).
  - [ ] Executar campanha contínua de jogo completa no Linux registrando integridade de saves (*Planejado*).
- **Critérios Objetivos de Aceitação**: Jogo totalmente funcional do início ao fim no Linux sem travamentos críticos ou corrupção de salvamentos.
- **Riscos / Pontos Não Resolvidos**: Peculiaridades de extensões Vulkan opcionais entre diferentes fornecedores de GPU.

---

## 3. Matriz Consolidada de Prioridades e Status

| Fase | Domínio Técnico | Categoria | Status Atual |
|---|---|---|---|
| **01** | Isolamento de Chamadas Win32 Herdadas | Requisito Obrigatório | **Concluído** |
| **02** | Abstração de Plataforma (HAL Trampolines em `extra/`) | Requisito Obrigatório | **Concluído** |
| **03** | Rastreio de Memória GPU (`mprotect` + `SIGSEGV`) | Requisito Obrigatório | **Funcional / Em Refinamento** |
| **04** | Backend Gráfico Vulkan 1.3 e Shaders SPIR-V | Requisito Obrigatório | **Concluído** |
| **05** | Áudio Stream e Entrada SDL3 | Requisito Obrigatório | **Concluído** |
| **06** | Launcher Nativo C++20 Qt6 (Substituindo C#) | Requisito Obrigatório | **Concluído** |
| **07** | Pipeline de Fast-Relink e QA Automatizado | Requisito Obrigatório | **Concluído** |
| **08** | Suíte de Testes Golden Files (`tests/golden/`) | Tarefa Planejada | **Planejado** |
| **09** | Validação Estendida de Drivers (AMD Mesa / Intel ANV) | Tarefa Planejada | **Em Andamento** |
| **10** | Campanha Completa de Jogo em Linux | Tarefa Planejada | **Em Andamento** |
| **11** | Alternativa de Write-Watch via `userfaultfd` | Hipótese sob Investigação | **Investigação** |
| **12** | Upscaling AMD FSR 3.1 (TAAU + Jitter + RCAS + FrameGen) & FSR 1.0 | Melhoria Opcional | **Concluído** |
| **13** | Backend DirectX 12 / Ferramentas Windows | Fora de Escopo | **Descartado / Irrelevante** |
| **14** | Suporte a 144 FPS (desacoplar câmera/input/timers) | Hipótese sob Investigação | **Adiado — ver [`fps-cap-144.md`](fps-cap-144.md)** |
| **15** | Onda A: Desbloqueio 60 FPS CPU/HLE/NVN (`AC_LPROF`, spin-waits, `--restrict`, `-O2 -mcpu=native`, cache DS) | Requisito Obrigatório | **Concluído** |
| **16** | Onda B: Resolução Interna 4K/Scene (`GRAPHIC BUFFER`), Draw Distance, Shadow Scale, No-HUD (`F12`) & UX | Tarefa Planejada | **Core Concluído (Refinos Opcionais Pendentes)** |
| **17** | Onda C: Low Latency, VRR, GTAO/Contact Shadows & Wayland HDR10 | Melhoria Opcional | **Planejado (após testes A+B)** |
