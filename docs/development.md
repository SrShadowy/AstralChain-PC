# Convenções de Desenvolvimento e Guia Técnico (AstralChain-PC)

Este documento estabelece as diretrizes práticas de engenharia, organização modular de código, convenções de hooks e referência de variáveis de ambiente para o projeto **AstralChain-PC**, operando sob a diretriz **LINUX FIRST**.

---

## 1. Princípios de Organização de Código (Linux First)

O projeto prioriza a melhor experiência de execução, estabilidade e desempenho no ecossistema nativo Linux:

### A. Estrutura do Código
- **Código-Fonte Unificado (`src/`)**: Todo o código-fonte do projeto é organizado por domínio sob `src/`:
  - `src/hooks/` e `src/additions/`: Trampolines de plataforma (POSIX/SDL3), camada de interface gráfica in-game (`overlay/`), Discord RPC (`rpc/`) e toggles (`toggles/`).
  - `src/launcher/`: Aplicação nativa em Qt6 / C++20 do Launcher oficial.
  - `src/runtime/`: Runtime em C do Horizon e motor NVN/Vulkan 1.3.
  - `src/resources/`: Patches `.pchtxt` (`patches/`) e bancos de variantes de shaders (`data/`).
  - `src/astral_chain/`: Orquestradores Python de tradução estática, compilação incremental e cache.
- **Builds (`build/`) e Depuração (`debug/`)**:
  - `build/game/`, `build/launcher/`, `build/resources/`, `build/objects/`, `build/images/`.
  - `debug/frames/` (capturas `frame_*.png`, `window.png`, `pass_*.png`) e `debug/traces/` (`trace.c`, `trace.o`, `compiler.log`, `relocation-targets.bin`).

### B. Distribuição de Pastas sob `src/` e `tools/`
| Diretório | Responsabilidade |
|---|---|
| `src/hooks/platform/` | Trampolines POSIX: tipos Win32 (`win32_types.h`), memória (`mmap`/`SIGSEGV`), threads/sync, tempo, pacing, crypto e base ELF |
| `src/hooks/sdl/` | Tipos de janela/GDI (`win32_window_types.h`), janela/superfície Vulkan SDL3 (`window_trampoline.h`) e áudio (`audio_trampoline.h`) |
| `src/hooks/input/` | Mouse/menus, detecção de contexto de entrada e códigos virtuais |
| `src/hooks/gpu/` | Trampolines Vulkan/Maxwell, FSR 3.1, pool VMA de write-tracking e shaderc POSIX |
| `src/hooks/storage/` | Paths POSIX (`storage_trampoline.h`), operações de diretório (`dir_trampoline.h`), busca (`find_trampoline.h`) e LayeredFS (`romfs_overlay.h`) |
| `src/hooks/ui/` | Renderização de fontes FreeType para prompts de botões |
| `src/hooks/hooks.h` | Cabeçalho unificado de re-exportação para integração modular com `src/runtime/` |
| `src/additions/overlay/` | Dashboard tático ImGui ("IRIS // Legatus Terminal"), RE Info e HUD de performance |
| `src/additions/rpc/` | Integração nativa com Discord Rich Presence via IPC socket |
| `src/additions/toggles/` | Toggles de runtime (`ac_toggles`) consumidos pela overlay e pelo backend Vulkan |
| `src/launcher/` | Código-fonte C++20 do Launcher oficial nativo Qt6 (`include/`, `src/`, `CMakeLists.txt`) |
| `tools/qa/` | Scripts de validação estática e testes de automação (`audit.sh`, `qa_runner.sh`) |

---

## 2. Separação entre C e C++

- **Núcleo do Runtime e Código Gerado (C)**:
  - As ~122 unidades de tradução de instruções do jogo, os serviços HLE de baixo nível e o despachante de SVCs (`src/runtime/`) permanecem em **C** puro (`-std=gnu17` / C11) para compatibilidade direta com o lifter estático.
  - O código gerado pelo lifter não deve ser editado manualmente; ajustes estruturais devem ser efetuados nas ferramentas geradoras (`lifter.py` / `native_trace.py`).
- **Camada Downstream e Interface (C++20)**:
  - O código em `src/additions/` e o Launcher Qt6 (`src/launcher/`) utilizam C++20 moderno com tipagem forte, RAII, ausência de ponteiros crus (`new`/`delete`) e encapsulamento em namespaces (`ac::launcher`).
- **Fronteira de Interoperabilidade (`extern "C"`)**:
  - Funções em `src/hooks/` e `src/additions/` invocadas pelo código em C do runtime são declaradas com vinculação `extern "C"`.
  - Exceções de C++ nunca devem cruzar a fronteira para o código em C. Erros devem ser tratados localmente ou mapeados para códigos de status.

---

## 3. Uso Pragmático de Hooks, Constantes, Formatação e Marcadores `// [extra]`

A integração entre o código herdado e a camada Linux é feita de maneira pragmática:

### A. Princípio dos Hooks, Zero Números Mágicos e Zero Código Comprimido
- Sempre que uma funcionalidade de plataforma no Linux substituir uma rotina Win32 herdada, a lógica é implementada no módulo adequado sob `src/hooks/` ou `src/additions/` e acionada por uma chamada pontual em `src/runtime/`.
- **Zero números mágicos**: qualquer constante de tempo, tamanho de buffer, alinhamento, limite de pool ou dimensão de UI deve ser declarada com `#define`, `enum` ou `constexpr` com nome descritivo e unidade explícita.
- **Zero código comprimido**: nunca comprima múltiplas instruções em uma única linha (ex.: `if (cond) { SetLastError(...); return FALSE; }`) para reduzir artificialmente a contagem de linhas. Cada statement deve estar em sua própria linha com indentação clara. Se um arquivo crescer além do limite ideal, refatore e divida-o em cabeçalhos/arquivos coesos.

### B. Uso dos Marcadores `// [extra]`
- O marcador `// [extra]` serve para **demarcar claramente onde `src/runtime/` foi editado**, facilitando revisões e auditorias estáticas (`tools/qa/audit.sh`).
- **Use estritamente quando necessário**: não exagere na marcação nem polua o código com comentários redundantes linha a linha.
  - Para alterações ou chamadas pontuais (linha única):
    ```c
    #include "hooks/hooks.h" // [extra]
    extra_pad_read_sensors(device, data); // [extra]
    ```
  - Para regiões ou blocos editados de múltiplas linhas: marque apenas a **primeira linha (início)** e a **última linha (fim)** do bloco:
    ```c
    #ifndef _WIN32 // [extra]
    extra_storage_init_paths();
    extra_sync_bootstrap();
    #endif // [extra]
    ```
- Registre adições ou modificações relevantes no ledger `SHADOW_CHANGES.md`.

---

## 4. Limites de Tamanho e Responsabilidade das Funções

Diretrizes práticas verificadas automaticamente por scripts de conformidade (`./tools/qa/audit.sh`):

| Elemento | Meta Ideal | Contexto Coeso Permitido | Ação Obrigatória |
|---|---|---|---|
| **Arquivo Fonte (`src/hooks`, `src/additions`, `src/launcher`, `tools/qa`)** | ≤ 350 linhas | 350 a 450 linhas | > 450 linhas: obrigatório refatorar e dividir em arquivos coesos |
| **Funções** | 20 a 40 linhas | > 40 linhas | > 50 linhas: avaliar decomposição lógica |
| **Parâmetros de Função** | ≤ 5 parâmetros | 6 a 7 parâmetros | > 7 parâmetros: agrupar em struct de contexto |

**Regra de Ouro da Divisão**: Nunca comprima linhas para caber em um limite arbitrário. Se o arquivo passar do tamanho ideal, avalie se há subdomínios claros para extrair em novos arquivos (como `win32_types.h`, `win32_window_types.h`, `dir_trampoline.h`); se o contexto for estritamente coeso, é permitido ficar entre 350 e 450 linhas sem comprimir código.

---

## 5. Validação Proporcional e Testes

A validação de alterações deve ser ágil e proporcional ao risco da modificação:

1. **Alteração de Documentação**:
   - Verificar precisão das informações técnicas, consistência de links relativos e formatação markdown.
2. **Alteração de Ferramentas / Launcher Qt6**:
   - Executar `./tools/qa/audit.sh` (valida marcadores necessários, confere limites de linhas, verifica ausência de blocos comprimidos e compila o launcher Qt6 via Ninja).
3. **Alteração de Código do Runtime / Compilação**:
   - Executar o Fast-Relink via Python:
     ```bash
     PYTHONPATH=src .venv/bin/python -m astral_chain.build.setup_game --rebuild-if-stale
     ```
     Verifica a compilação incremental e linkage do executável ELF em ~8 segundos.
4. **Alteração Gráfica / Comportamental do Jogo**:
   - Executar o runner de QA determinístico:
     ```bash
     # Smoke test completo com input e validação visual de frame (20s)
     ./tools/qa/qa_runner.sh 20 550

     # Verificação em ambiente headless (sem display/GPU ativa)
     SKIP_RUNTIME=1 ./tools/qa/qa_runner.sh
     ```

---

## 6. O Launcher Oficial do Projeto (Qt6 C++20)

O launcher oficial para o projeto é a aplicação nativa em **Qt6 / C++20** localizada em `src/launcher/`.

> [!IMPORTANT]
> O launcher herdado em C# (`Launcher.cs` / `Astral Chain Launcher.exe`) pertence ao upstream Windows original e **não é compilado nem utilizado no Linux**. Não gaste tempo tentando portar ou manter o launcher em C#.

### Compilação do Launcher Qt6
```bash
# Configurar e compilar via CMake + Ninja
cmake -B build/launcher -S src/launcher -GNinja
cmake --build build/launcher

# Execução direta via helper
./launcher.sh
```

O launcher gerencia configurações gráficas (resolução, apresentação FIFO/Mailbox/Immediate, tela cheia), seleção de arte de fundo e inicialização segura do executável ELF.

---

## 7. Referência de Variáveis de Ambiente e Diagnóstico

O runtime suporta uma série de variáveis de ambiente para controle de performance, depuração gráfica e testes automatizados:

### A. Controle Geral e Performance
| Variável | Valores / Padrão | Efeito |
|---|---|---|
| `AC_PARALLEL` | `1` (padrão) | Executa threads do guest Horizon em paralelo nas threads do host |
| `AC_FPS` | `60` (padrão) / `0` (ilimitado) / `1..1000` | Taxa de quadros alvo do motor e do pacing NVN (valor inválido cai no padrão) |
| `AC_FPS_UNLOCKED` | `0` (padrão) / `1` | Entrada de **build**: aplica `dynamic-unlocked-1.0.1.pchtxt` e remove o cap de 60 FPS do patch Dynamic-FPS. Mudar o valor reconstrói (o launcher cuida disso) |
| `AC_ASYNC_SUBMIT` | `1` (launcher) | Submete comandos gráficos e fences em uma thread dedicada do backend Vulkan |
| `AC_CUTSCENE_FPS` | `60` (padrão) / `30` | `30` roda cutscenes no passo original; `60` utiliza o patch de Dynamic-FPS |
| `AC_GPU_HANG_MS` | `1200` (ms) | Tempo limite antes de acionar o watchdog interno e encerrar o jogo por travamento de GPU |
| `AC_FULLSCREEN` | `1` / `0` | Inicializa a janela em tela cheia sem bordas (borderless fullscreen) |
| `AC_OVERLAY` | `1` / `0` (padrão) | Inicia com o menu ImGui in-game aberto (o mesmo da tecla **Insert**; útil para validação headless de renderização em conjunto com `LARECOMP_WINDOW_SHOT=<frame>`) |
| `AC_PERF_HUD` | `0` (padrão) / `1..3` | Inicia com o **Overlay de Performance IRIS** ativo mesmo com o menu escondido (`1` = Compacto, `2` = Detalhado com gráfico, `3` = Mínimo) |
| `AC_REINFO` | `1` / `0` (padrão) | Inicia com a janela **RE Info** aberta (a mesma da tecla **F8**): valores ao vivo dos slots de estado do jogo e as tabelas id→nome extraídas do texto. Somente leitura |
| `AC_HANDHELD` | `1` / `0` | Simula modo portátil do Switch (renderiza 1280x720 recortado para janela 1600x900) |
| `AC_VERBOSE` | `1` / `0` | Ativa saída verbosa de logs no terminal (padrão enxuto mantido em 0) |

### B. Diagnóstico Gráfico e Vulkan
| Variável | Valores | Efeito |
|---|---|---|
| `LARECOMP_VK_PROF` | `1` | Exibe breakdown de CPU/GPU a cada 2s, contadores de textura e estatísticas de pipeline |
| `LARECOMP_SHOT_AT` | `<frame>,...` | Captura screenshots determinísticas nos números de frame especificados |
| `LARECOMP_SCREENSHOT_EVERY` | `<frames>` | Salva capturas periódicas a cada N frames em `frame_N.png` |
| `LARECOMP_DEBUG_FRAME` | `<frame>` | Despeja cada render pass do frame indicado para `build/pass_NNN.png` com hashes de shaders |
| `LARECOMP_DEBUG_EACH` | `1` | Encerra o render pass antes de cada draw call individual para inspeção atômica de imagens |
| `LARECOMP_SKIP_FSHASH` | `<hash>,...` | Ignora a execução de draw calls que utilizem o fragment shader especificado |
| `LARECOMP_WATCH_FS` | `<hash>` | Registra em log todas as submissões do fragment shader especificado |
| `LARECOMP_GPL` | `0` / `1` | `0` desativa `VK_EXT_graphics_pipeline_library` e força compilação monolítica |
| `LARECOMP_GUARD_VTX` | `1` / `0` | `1` detecta e previne explosão de vértices (NaN/Inf/>50k) e iluminação anômala |
| `AC_GPU_WARM` | `0` / `1` | `0` pula o warm-up antecipado de pipelines na inicialização |

### C. Texturas e Decodificação ASTC / BC7
| Variável | Valores | Efeito |
|---|---|---|
| `LARECOMP_GPU_ASTC` | `1` (padrão) / `0` | `1` decodifica texturas ASTC via compute shader na GPU; `0` usa fallback na CPU |
| `LARECOMP_BC7` | `1` / `0` | Ativa o armazenamento de texturas ASTC comprimidas como BC7 (reduz consumo de VRAM) |
| `AC_BC7_PRECOMPUTE` | `1` | Modo de setup que gera previamente o cache de texturas BC7 em `build/game/cache/bc7` |
| `LARECOMP_BC7_LOG` | `1` | Emite linha de log com hash e layout para cada upload de textura ASTC/BC7 |
| `LARECOMP_DUMP_TEX` | `<min_width>` | Salva o nível de mipmap 0 de texturas descompactadas em arquivos binários `tex_*.bin` |

### C.1 Qualidade de Textura, LoD e Sombras (override de sampler)
Atuam na montagem de samplers Vulkan (`textures.c` `vk_sampler`) e são editáveis ao vivo no menu
ImGui (Insert) e persistidos pelo launcher Qt6; a env continua sendo o default de boot.
| Variável | Valores / Padrão | Efeito |
|---|---|---|
| `LARECOMP_LOD_BIAS` | `0` (padrão) / `0..3` | Bias de LOD global somado ao bias do sampler (`si.mipLodBias`); positivo empurra a amostragem para mips menores (textura mais leve) |
| `LARECOMP_LOD_MIN` | `0` (padrão) / `0..4` | Piso de LOD (`si.minLod = max(minLod, valor)`) em texturas com mipmapping |
| `LARECOMP_NO_SHADOW` | ausente / `1` | `1` troca o teste de comparação de shadow-map para `VK_COMPARE_OP_ALWAYS` (sombras desligadas) |

### C.2 AMD FSR 3.1 (Temporal Upscaler, Camera Jitter, RCAS & Frame Generation)
Configuráveis ao vivo no menu ImGui (**Insert**) e persistidos em `larecomp.ini` e no Launcher oficial Qt6:
| Variável | Valores / Padrão | Efeito |
|---|---|---|
| `AC_UPSCALER` | `fsr3` (padrão) / `fsr1` / `bilinear` | Seleciona o motor de upscaling: `fsr3` (AMD FSR 3.1 Temporal + Jitter + RCAS), `fsr1` (Espacial EASU+RCAS) ou `bilinear` |
| `AC_RENDER_SCALE` | `1.00` (padrão) / `0.50..2.00` | Escala de resolução interna / preset de qualidade FSR (`0.50` Performance, `0.58` Balanced, `0.67` Quality, `0.77` Ultra Quality, `1.00` Native AA / FSRAA) |
| `AC_FSR_JITTER` | `1` (padrão) / `0` | Ativa o Camera Jitter Sub-Pixel **Halton(2,3)** nos passes 3D com depth test e Negative MIP LOD Bias automático nos samplers |
| `AC_FSR_SHARPNESS` | `0.80` (padrão) / `0.00..1.00` | Intensidade do estágio **RCAS** (Robust Contrast-Adaptive Sharpening) do FSR 3.1 |
| `AC_FRAMEGEN` | `0` (padrão) / `1` | Ativa o **FSR 3.1 Frame Generation (2x FPS)** com interpolação óptica bidirecional ($U_{t-0.5}$), proteção de HUD/UI e dupla apresentação cadenciada na Swapchain Vulkan |

### D. Input Determinístico e Controles
- **`AC_INPUT_SCRIPT="t0-t1:mask[@period/duty];..."`**:
  Permite injetar sequências automatizadas de entrada baseadas em timestamps (em segundos) desde o primeiro quadro.
  - **Mapeamento de Bits de Botões**:
    - `0x1`: Botão A
    - `0x2`: Botão B
    - `0x100`: Botão ZL
    - `0x200`: Botão ZR
    - `0x400`: Botão + (Plus)
    - `0x1000` a `0x8000`: D-Pad (Esquerda, Cima, Direita, Baixo)
    - `0x10000` a `0x80000`: Analógico Esquerdo (Esquerda, Cima, Direita, Baixo)
    - Deslocamento de `<< 4` nos bits do analógico esquerdo controla o analógico direito.
- **Configuração de Mouse e Câmera**:
  - `AC_MOUSE_LOOK=0`: Desativa o controle de câmera pelo movimento do mouse.
  - `AC_MOUSE_SENS=<percent>`: Ajusta a sensibilidade de rotação da câmera (padrão 100, suporta de 5% a 4000%).
  - `AC_MOUSE_YSCALE=<float>`: Escala independente de sensibilidade para o eixo vertical Y (padrão 1.0).
  - `AC_MOUSE_CURVE=power|linear|aggressive|<exp>`: Curva de aceleração da câmera (padrão `power` com expoente 1.35; `linear` para 1:1; `aggressive` com 1.75).
  - `AC_MOUSE_INVERT=1`: Inverte o eixo vertical da câmera no mouse.
- **Cursor e ponteiro de menu** (`extra/input/mouse_mode.h`, `mouse_menu.h`, `menu_detect.h`):
  - A aplicação **inicia com o cursor livre** e centrado: a tela de título é clicável sem tecla alguma. Uma tecla de stick (movimento/câmera: WASD/FTHG por padrão) toma o mouse para a câmera, mas só se o ponteiro estiver parado há mais de 1,2 s — quem acabou de usar o mouse e anda a lista pelo teclado mantém o cursor. Teclas de D-pad nunca tomam o mouse.
  - `AC_MOUSE_AUTO_MENU=0`: desativa essa captura automática por tecla de stick (restam a tecla de pausa e F10).
  - `AC_MOUSE_MENU_TIMEOUT=1`: devolve a câmera após 5 s sem uso do ponteiro (padrão 0: nunca esconde um cursor que o jogador ainda pode precisar).
  - `AC_MOUSE_MENUS=1` / `0`: Ativa ou desativa o ponteiro de menu (passos direcionais, clique, roda). Padrão 1.
  - `AC_MOUSE_MENU_MOTION=0`: Desativa só os passos direcionais ao mover o mouse (clique e roda continuam). Padrão 1.
  - `AC_MOUSE_MENU_CELL=<px>`: Passo em pixels de tela para cada avanço discreto de item ao mover o mouse (padrão 40, faixa 10–200).
  - `AC_MOUSE_MENU_TOUCH=1`: Publica um contato real de touch na posição do cursor, para telas que façam hit-test do painel. Padrão 0. O contato chega ao jogo (a máquina de estados dele em `ctx+0x24` percorre `0→2→3→1→0`, e a posição publicada confere com o painel), mas nenhum menu alcançado no boot — overlay de tutorial, LOAD DATA (docked e handheld) e Legatus/Mapa — usa touch para selecionar item (`docs/plan-mouse-menu.md` §4.4).
  - Movimento do mouse gera passos de D-pad, o clique esquerdo injeta A e o direito injeta B; a roda rola a lista. Isso vale em qualquer tela do jogo, sem zonas hard-coded — o jogo não expõe flag de UI nem geometria de item, e o objeto de mouse que ele constrói por frame não é lido por nada. **Seleção posicional absoluta não é possível**: o ponteiro mapeia para passos relativos (`docs/plan-mouse-menu.md` §4).
  - Tecla **F10** e tecla de **pausa do jogo** (MINUS, `Esc` por padrão): alternam entre o modo de jogo (`CAPTURED`, câmera ativa) e o modo de menu (`RELEASED`, cursor do sistema visível e câmera travada, com acumuladores zerados para não haver chicoteio ao voltar). A tecla de pausa foi escolhida por espelhar exatamente o ciclo abrir/fechar do menu de pausa do jogo.
  - Alt+Tab devolve o cursor e o restaura ao recuperar o foco, preservando a escolha explícita de menu.
  - **Gate de foco**: a injeção de menu (passos D-pad, taps A/B e contato de touch) é aplicada dentro de `if(connected && focused)` em `hid_publish`. `window_trampoline.h` fixa `g_sdl_input_focus = 1` logo após `SDL_RaiseWindow` porque o evento `WINDOW focus` do SDL pode nunca chegar em execução sem interação; sem isso toda a injeção fica silenciosamente desligada.
  - `./tools/qa/audit.sh` roda `tools/qa/mouse_pointer_test.c` (mapeamento de painel, passos, pulsos, clique/roda e política de modo) e reprova a auditoria se a tradução do ponteiro quebrar.
  - `AC_PROMPTS=auto|xbox|switch|keyboard`: Define o layout de ícones de botões no HUD e menus (`auto` alterna dinamicamente entre **Controle Xbox** para gamepads padrão/Xbox, **Nintendo Switch** para Pro Controller/Joy-Con e **Teclado & Mouse** ao usar teclado/mouse; também configurável ao vivo no menu IRIS via tecla `Insert`).
  - `AC_XBOX_CONFIRM_A=1|0`: Quando ativo (`1`, padrão), troca o botão de **Confirmar / Ação / Interagir** de `B` para `A` no controle (mapeando o botão físico inferior `A` para `IB_A` e o direito `B` para `IB_B`, exibindo `(A)` nas ações/confirmações e ajustando o losango de 4 botões sem alterar `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `View` ou `Menu`). Configurável no Launcher Qt6 e ao vivo no menu IRIS (`Insert`).
  - **Insert ou Segurar `L3 + R3` por 2 segundos no Controle (menu ImGui in-game)**: abre o overlay de Performance/Graphics/Debug/About (`src/additions/overlay/`). Com o menu aberto, teclado/mouse/gamepad deixam de chegar ao jogo, a navegação por controle fica ativa (`D-Pad`/Analógico para mover, `A` para selecionar, `B` para fechar combos, `LB`/`RB` para trocar de aba) e o mouse sai do modo relativo; `Insert` ou segurar `L3 + R3` por 2s fecha e restaura o modo conforme o foco. Antialiasing, Upscaler, Limite de FPS, Prompts de Botões, Confirmar com A e flags de debug valem no frame seguinte; VSync recria a swapchain no pré-frame; Async submit é kill switch one-way (religar exige relaunch).

---

## 8. Dados Técnicos de Texturas BC7 (Benchmark de Referência)

Medição realizada em ambiente de teste padrão (RTX 4090, rota da rodovia, 3 execuções com estados limpos intercalados entre frames 1000 e 19950):

| Métrica | Desativado (RGBA8) | GPU (Compute Shader) | Pré-computado (Cache) |
|---|---|---|---|
| **Memória de Vídeo (Média / Pico)** | 7.2 / 8.7 GB | 4.7 / 5.2 GB | 4.7 / 5.1 GB |
| **Total de Pausas por Sessão** | 7 pausas (~460 ms) | 12 pausas (~980 ms) | 6 pausas (~500 ms) |
| **Tempo de Upload de Textura** | 9.4 s | 9.2 s | 10.4–11.3 s (I/O de disco) |
| **Qualidade PSNR (300 texturas)** | Referência nativa | 46.2 dB médio / 27.2 dB pior | 51.0 dB médio / 34.2 dB pior |

---

## 9. Comandos Úteis do Desenvolvedor

```bash
# 1. Auditoria estática de conformidade e build do launcher Qt6
./extra/tools/audit.sh

# 2. Recompilação ultrarrápida após editar arquivos em extra/ ou runtime (~8 segundos)
python3 -m astral_chain.build.setup_game --rebuild-if-stale

# 3. Teste automatizado de QA determinístico (20s de execução com validação de frame 550)
./extra/tools/qa_runner.sh 20 550

# 4. Compilação direta do launcher Qt6
cmake -B build-launcher -S extra/ui/launcher -GNinja && cmake --build build-launcher

# 5. Formatação de código
clang-format -i $(git ls-files 'extra/*.cpp' 'extra/*.h' 'extra/*.c')
```
