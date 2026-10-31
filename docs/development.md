# Convenções de Desenvolvimento e Guia Técnico (AstralChain-PC)

Este documento estabelece as diretrizes práticas de engenharia, organização modular de código, convenções de hooks e referência de variáveis de ambiente para o projeto **AstralChain-PC**, operando sob a diretriz **LINUX FIRST**.

---

## 1. Princípios de Organização de Código (Linux First)

O projeto prioriza a melhor experiência de execução, estabilidade e desempenho no ecossistema nativo Linux:

### A. Estrutura do Código
- **Código Customizado Downstream (`extra/`)**: Novas funcionalidades, trampolines de plataforma (POSIX/SDL3), camada de interface gráfica e scripts de validação residem preferencialmente sob `extra/` para manter a base limpa e desacoplada.
- **Código Herdado (`astral_chain/`)**: Contém o runtime em C e o motor de recompilação estática herdados do projeto original. Visto que o repositório upstream original de Windows (`Puddz`) está inativo e não traz mais atualizações relevantes, não há necessidade de manter compatibilidade rígida com Windows ou contorcer soluções para preservar paridade com upstream.

### B. Distribuição de Pastas sob `extra/`
| Diretório | Responsabilidade |
|---|---|
| `extra/core/` | Trampolines de plataforma POSIX, emulação de memória (`mmap`/`SIGSEGV`), janela/input SDL3, áudio e sincronização |
| `extra/ui/launcher/` | Código-fonte C++20 do Launcher oficial nativo Qt6 (`include/`, `src/`, `CMakeLists.txt`) |
| `extra/tools/` | Scripts de validação estática e testes de automação (`audit.sh`, `qa_runner.sh`) |
| `extra/hooks.h` | Cabeçalho unificado de re-exportação para integração modular com o código herdado |

---

## 2. Separação entre C e C++

- **Núcleo do Runtime e Código Gerado (C)**:
  - As ~122 unidades de tradução de instruções do jogo, os serviços HLE de baixo nível e o despachante de SVCs permanecem em **C** puro (`-std=gnu17` / C11) para compatibilidade direta com o lifter estático.
  - O código gerado pelo lifter não deve ser editado manualmente; ajustes estruturais devem ser efetuados nas ferramentas geradoras (`lifter.py` / `native_trace.py`).
- **Camada Downstream e Interface (C++20)**:
  - Todo o código em `extra/` e o Launcher Qt6 utilizam C++20 moderno com tipagem forte, RAII, ausência de ponteiros crus (`new`/`delete`) e encapsulamento em namespaces (`ac::launcher`).
- **Fronteira de Interoperabilidade (`extern "C"`)**:
  - Funções em `extra/` invocadas pelo código em C do runtime são declaradas com vinculação `extern "C"`.
  - Exceções de C++ nunca devem cruzar a fronteira para o código em C. Erros devem ser tratados localmente ou mapeados para códigos de status.

---

## 3. Uso Pragmático de Hooks e Marcadores `// [extra]`

A integração entre o código herdado e a camada Linux é feita de maneira pragmática:

### A. Princípio dos Hooks
- Sempre que uma funcionalidade de plataforma no Linux substituir uma rotina Win32 herdada, a lógica é implementada em `extra/core/` e acionada por uma chamada pontual no código herdado.
- Evite burocracias artificiais: implemente a solução mais simples e direta que resolva o problema com clareza.

### B. Uso dos Marcadores `// [extra]`
- O marcador `// [extra]` serve para **demarcar claramente onde o código herdado foi editado**, facilitando revisões e auditorias estáticas (`audit.sh`).
- **Use estritamente quando necessário**: não exagere na marcação nem polua o código com comentários redundantes linha a linha.
  - Para alterações ou chamadas pontuais (linha única):
    ```c
    #include "extra/hooks.h" // [extra]
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

Diretrizes práticas verificadas automaticamente por scripts de conformidade (`extra/tools/audit.sh`):

| Elemento | Meta Ideal | Limite de Revisão | Ação Obrigatória |
|---|---|---|---|
| **Arquivo Fonte (`extra/`)** | ≤ 300 linhas | 300 a 400 linhas | > 400 linhas: dividir por responsabilidade independente |
| **Funções** | 20 a 40 linhas | > 40 linhas | > 50 linhas: avaliar decomposição lógica |
| **Parâmetros de Função** | ≤ 5 parâmetros | 6 a 7 parâmetros | > 7 parâmetros: agrupar em struct de contexto |

**Regra de Ouro da Divisão**: Arquivos e funções devem ser divididos por **responsabilidade clara**, nunca de forma fragmentada ou artificial apenas para satisfazer contadores de linha.

---

## 5. Validação Proporcional e Testes

A validação de alterações deve ser ágil e proporcional ao risco da modificação:

1. **Alteração de Documentação**:
   - Verificar precisão das informações técnicas, consistência de links relativos e formatação markdown.
2. **Alteração de Ferramentas / Launcher Qt6**:
   - Executar `./extra/tools/audit.sh` (valida marcadores necessários, confere limites de 400 linhas e compila o launcher Qt6 via Ninja).
3. **Alteração de Código do Runtime / Compilação**:
   - Executar o Fast-Relink via Python:
     ```bash
     python3 -m astral_chain.setup_game --rebuild-if-stale
     ```
     Verifica a compilação incremental e linkage do executável ELF em ~8 segundos.
4. **Alteração Gráfica / Comportamental do Jogo**:
   - Executar o runner de QA determinístico:
     ```bash
     # Smoke test completo com input e validação visual de frame (20s)
     ./extra/tools/qa_runner.sh 20 550

     # Verificação em ambiente headless (sem display/GPU ativa)
     SKIP_RUNTIME=1 ./extra/tools/qa_runner.sh
     ```

---

## 6. O Launcher Oficial do Projeto (Qt6 C++20)

O launcher oficial para o projeto é a aplicação nativa em **Qt6 / C++20** localizada em `extra/ui/launcher/`.

> [!IMPORTANT]
> O launcher herdado em C# (`Launcher.cs` / `Astral Chain Launcher.exe`) pertence ao upstream Windows original e **não é compilado nem utilizado no Linux**. Não gaste tempo tentando portar ou manter o launcher em C#.

### Compilação do Launcher Qt6
```bash
# Configurar e compilar via CMake + Ninja
cmake -B build-launcher -S extra/ui/launcher -GNinja
cmake --build build-launcher

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
| `AC_FPS` | `60` (padrão) / `0` (ilimitado) / `30..240` | Taxa de quadros alvo do motor e do pacing NVN |
| `AC_ASYNC_SUBMIT` | `1` (launcher) | Submete comandos gráficos e fences em uma thread dedicada do backend Vulkan |
| `AC_CUTSCENE_FPS` | `60` (padrão) / `30` | `30` roda cutscenes no passo original; `60` utiliza o patch de Dynamic-FPS |
| `AC_GPU_HANG_MS` | `1200` (ms) | Tempo limite antes de acionar o watchdog interno e encerrar o jogo por travamento de GPU |
| `AC_FULLSCREEN` | `1` / `0` | Inicializa a janela em tela cheia sem bordas (borderless fullscreen) |
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
| `AC_GPU_WARM` | `0` / `1` | `0` pula o warm-up antecipado de pipelines na inicialização |

### C. Texturas e Decodificação ASTC / BC7
| Variável | Valores | Efeito |
|---|---|---|
| `LARECOMP_GPU_ASTC` | `1` (padrão) / `0` | `1` decodifica texturas ASTC via compute shader na GPU; `0` usa fallback na CPU |
| `LARECOMP_BC7` | `1` / `0` | Ativa o armazenamento de texturas ASTC comprimidas como BC7 (reduz consumo de VRAM) |
| `AC_BC7_PRECOMPUTE` | `1` | Modo de setup que gera previamente o cache de texturas BC7 em `build/game/cache/bc7` |
| `LARECOMP_BC7_LOG` | `1` | Emite linha de log com hash e layout para cada upload de textura ASTC/BC7 |
| `LARECOMP_DUMP_TEX` | `<min_width>` | Salva o nível de mipmap 0 de texturas descompactadas em arquivos binários `tex_*.bin` |

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
  - `AC_MOUSE_SENS=<percent>`: Ajusta a sensibilidade de rotação da câmera (padrão 100).
  - `AC_MOUSE_INVERT=1`: Inverte o eixo vertical da câmera no mouse.
  - `AC_PROMPTS=switch`: Força a exibição dos ícones originais de controle do Switch em vez dos ícones de PC gerados via FreeType.

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
python3 -m astral_chain.setup_game --rebuild-if-stale

# 3. Teste automatizado de QA determinístico (20s de execução com validação de frame 550)
./extra/tools/qa_runner.sh 20 550

# 4. Compilação direta do launcher Qt6
cmake -B build-launcher -S extra/ui/launcher -GNinja && cmake --build build-launcher

# 5. Formatação de código
clang-format -i $(git ls-files 'extra/*.cpp' 'extra/*.h' 'extra/*.c')
```
