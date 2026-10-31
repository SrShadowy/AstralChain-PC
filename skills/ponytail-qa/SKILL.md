---
name: ponytail-qa
description: >
  Agente de teste automatizado, otimização de performance e correção cirúrgica de
  falhas para AstralChain-PC (Linux First), integrando a filosofia /ponytail (menor diff completo),
  a camada Linux nativa (SDL3/Vulkan 1.3) e as diretrizes de isolamento
  em extra/ (marcadores // [extra] quando estritamente necessários e teto de 400 linhas).
---

# Ponytail-QA — Agente de Teste Especializado, Otimização e Diagnóstico (Linux First)

Este skill é acionado quando a tarefa demandar investigação de crashes de runtime, diagnósticos visuais de renderização, validação de input ou verificação cirúrgica de regressões no **AstralChain-PC**.

O projeto opera sob a diretriz **LINUX FIRST**: o objetivo primordial é a estabilidade e performance no ecossistema Linux nativo (Vulkan 1.3, SDL3, Qt6). O upstream Windows original é inativo e não impõe paridade forçada.

Para regras globais e arquitetura, consulte [`AGENTS.md`](../../AGENTS.md) e [`docs/architecture.md`](../../docs/architecture.md).

---

## 1. Princípios Operacionais de QA

1. **Correção Cirúrgica (`/ponytail`)**:
   - Resolva a causa raiz com o menor diff legível. Proibidos wrappers, camadas intermediárias ou opções sem demanda imediata.
   - Qualquer atalho com limite técnico deve conter: `// shortcut: <limite>, <quando atualizar>`.
2. **Organização Pragmática**:
   - Correções de plataforma residem em `extra/core/`.
   - Modificações no código herdado devem ser pontuais e objetivas.
   - O marcador `// [extra]` deve ser utilizado **estritamente quando necessário** para demarcar os locais editados fora de `extra/`, evitando exageros ou marcações redundantes.
   - Teto rígido de **400 linhas** por arquivo em `extra/` (meta ideal **≤ 300 linhas**), verificado por `audit.sh`.
   - O launcher oficial é o Launcher Qt6 C++20 (`extra/ui/launcher/`). O launcher legado em C# de Windows não é utilizado nem compilado no Linux.
3. **Validação Rápida e Feedback Contínuo**:
   - Utilize o Fast-Relink (`python3 -m astral_chain.setup_game --rebuild-if-stale`) para recompilar alterações de código em ~8 segundos sem reconstruir as unidades de guest inalteradas.
   - Mantenha a saída de log limpa e enxuta (logs verbosos ativados apenas sob `AC_VERBOSE=1`).

---

## 2. Runbook de Diagnóstico de Falhas

### A. Falhas de Runtime (`STOP <motivo>`, `FATAL`, `SIGSEGV`)
1. **Identificação da Chamada**:
   - Localize a mensagem de erro exata no código do runtime utilizando `grep_search` no diretório `astral_chain/runtime/`.
2. **Inspeção de Contexto**:
   - Verifique quais chamadas de sistema Win32/POSIX ou manipulações de ponteiros precedem a instrução `boundary(...)` ou o ponto de parada (ex.: `GetLastError()`, descritores de arquivos abertos, separadores de diretório `/` vs `\`, fontes ausentes).
3. **Resolução no Trampoline Pertinente**:
   - Ajuste o tratamento no respectivo trampoline em `extra/core/`:
     - Memória e sinais: `extra/core/memory_trampoline.h` (`mmap`, `mprotect`, `SIGSEGV`).
     - Arquivos e saves: `extra/core/storage_trampoline.h` e `find_trampoline.h`.
     - Janela e eventos: `extra/core/window_trampoline.h`.
     - Sincronização e threads: `extra/core/sync_trampoline.h`.
     - Tipos e linkage ELF: `extra/core/platform_posix.h`.

### B. Falhas Gráficas e Renderização (Viewport Cortado, Flicker, Flash Verde)
1. **Captura Determinística de Frame**:
   - Capture o quadro exato da falha via variável `LARECOMP_SHOT_AT=<frame>` utilizando o script `extra/tools/qa_runner.sh`.
2. **Inspeção de Brilho dos Quadrantes**:
   - Analise a distribuição de luminância média nos 4 quadrantes da tela:
     - **TL** (Top-Left), **TR** (Top-Right), **BL** (Bottom-Left), **BR** (Bottom-Right).
     - Valores médios de luminância abaixo de 5 indicam render target excessivamente escuro ou não preenchido; desbalanceamentos extremos entre quadrantes apontam viewport recortado incorretamente.
3. **Detecção de Pixels Verdes Espúrios**:
   - Pixels puramente verdes (`R < 10, G > 200, B < 10`) indicam falha de decodificação ASTC ou amostragem de textura não vinculada. Verifique `astc_gpu.c` e a decodificação em compute shader.
4. **Inspeção de Viewport e Scissor**:
   - Verifique se a aplicação de viewport dinâmico em `exec.c` ou em `device/postprocess.c` está honrando a resolução real da janela SDL3 e respeitando o aspecto de apresentação configurado (`nvnWindowSetCrop`).
5. **Inspeção de Texturas Dinâmicas**:
   - Quando texturas de vídeo ou de interface piscarem, inspecione o rastreio por amostragem de hash FNV (`fnv_sample`) em `runtime/las/nvn/vulkan/textures.c`.

### C. Falhas de Entrada e Controles (Teclado, Mouse e Gamepad)
1. **Reprodução Determinística**:
   - Injete sequências temporizadas utilizando a sintaxe `AC_INPUT_SCRIPT="t0-t1:mask;..."`.
   - Exemplo: `AC_INPUT_SCRIPT="10-13:300;18-21:300"` segura ZL+ZR (máscara `0x300`) nos segundos 10–13 e 18–21 para avançar diálogos e menus.
2. **Inspeção do Loop de Eventos**:
   - Verifique se a função `extra_window_pump_events` em `extra/core/window_trampoline.h` está sendo invocada a cada quadro no laço principal de renderização.
   - Verifique a leitura de estado do gamepad e das teclas virtuais em `runtime/hid_service.h` e `runtime/mouse.h`.

---

## 3. Comandos Operacionais de Automação

### 1. Auditoria Estática e Checagem de Limites
Valida marcadores `// [extra]`, teto de 400 linhas em `extra/` e compilação do launcher Qt6:
```bash
./extra/tools/audit.sh
```

### 2. Fast-Relink do Runtime (~8s)
Verifica compilação incremental de código nativo sem reconstruir units de guest inalteradas:
```bash
python3 -m astral_chain.setup_game --rebuild-if-stale
```

### 3. Suíte Integrada de QA (Auditoria + Fast-Relink + Runtime + Visual)
Executa o ciclo completo de validação:
```bash
# Execução padrão (20 segundos de runtime, análise do frame 550)
./extra/tools/qa_runner.sh 20 550

# Execução em ambiente headless (sem display/GPU ativa)
SKIP_RUNTIME=1 ./extra/tools/qa_runner.sh

# Teste estendido com script de navegação e análise do frame 1800
AC_INPUT_SCRIPT="10-13:300;18-21:300;24-25:1;27-28:1" ./extra/tools/qa_runner.sh 35 1800
```

---

## 4. Verificação de Regressões em Modificações

Antes de submeter qualquer correção ou ajuste:

1. **Auditoria de Marcadores**:
   - Confirme se os pontos editados no código herdado estão identificados com `// [extra]` onde estritamente necessário.
2. **Atualização do Ledger**:
   - Registre alterações estruturais no código herdado em `SHADOW_CHANGES.md`.
3. **Checagem de Integridade de Saves**:
   - Confirme se a execução gerou ou preservou o arquivo de integridade `userdata/saves/.../manifest.bin`.

---

## 5. Formato de Saída Obrigatório do Skill

Ao concluir qualquer diagnóstico, correção ou validação técnica especializada, encerre o relatório com o formato padronizado:

```text
Changed:      <arquivos em extra/ e cada arquivo herdado tocado>
Hooks:        <pontos de alteração/hooks adicionados ou modificados>
Verified:     <auditoria estática, tempo de relink, FPS, validação visual de frame e integridade de saves>
Not verified: <o que não foi testado nesta sessão e o motivo>
Limitations:  <riscos técnicos conhecidos, atalhos implementados ou pontos pendentes>
```
