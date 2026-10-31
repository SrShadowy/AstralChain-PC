# Análise de Desempenho e Frame Time — Revisão de 2026-10-31

> Documento de análise (não normativo). Registra o diagnóstico realizado sobre uma sessão real
> no host de desenvolvimento, as medições que sustentam cada conclusão e as ideias de melhoria
> derivadas delas. Complementa [`roadmap-linux.md`](roadmap-linux.md) (Etapa 7) sem substituí-lo.

---

## 1. Contexto e método

O ponto de partida foi a percepção de que o port consumia "muito mais recursos que a plataforma
original" e de que o *frame time* oscilava sem motivo aparente. A suspeita natural nesse tipo de
projeto é a GPU — recompilação estática de ARM64 para C costuma produzir código host pesado, e a
camada NVN→Vulkan faz tradução de estado a cada chamada. A instrumentação embutida no backend
desmentiu essa hipótese.

Todas as conclusões abaixo vêm de execuções reais no host de desenvolvimento (AMD Ryzen 9 7900,
Radeon RX 9070 XT / RADV GFX1201, CachyOS, kernel 7.3, Wayland). Foram usados: a telemetria de
percentis já presente em `device/postprocess.c`, o perfilador por categoria ativado por
`LARECOMP_VK_PROF=1`, leitura direta de `/proc/<pid>/task/*/stat` para atribuir consumo por thread
e comparativos A/B de variáveis de ambiente com a mesma cena e o mesmo save. Nada foi inferido por
leitura de código isolada — cada afirmação quantitativa tem uma medição correspondente.

---

## 2. O quadro que os números pintam

A linha de telemetria do regime "normal" de gameplay é esta:

```
[vk] 60.0 fps [p50=19.0ms p95=34.0ms p99=36.0ms 1%low=27.8fps] max 39ms
     (7763 draws, 0 skipped, 5600 passes, 768 dispatches, 240 flushes,
      wait 1ms, compile 0, upload 223.7MB, video memory 3601MB / 13043MB)
     | gpu 396-406 ms
```

Duas leituras imediatas e ambas contraintuitivas. Primeiro, `p50 ≈ 17–19 ms` significa que o frame
mediano **cabe** em 60 FPS — não há escassez de capacidade no caso comum. Segundo, `gpu ≈ 400 ms`
por janela de 2 s indica **GPU ociosa em ~80% do tempo**, e `wait 1ms` diz que a render thread quase
nunca espera a GPU. Ou seja: não se trata de um port pesado demais para o hardware. Trata-se de um
port que **desperdiça** o hardware disponível, e o sintoma aparece inteiro em `p95 = 34 ms` contra
`p50 = 17 ms`.

Um `p95` exatamente o dobro do `p50` não é coincidência estatística: é a assinatura clássica de
perda de slot de vsync. O frame seguinte ao que perde o slot espera um refresh inteiro e cai em
33 ms. É por isso que o `1% low` fica preso em ~27 FPS mesmo com a média em 60: um a cada vinte
frames escorrega para o slot seguinte.

---

## 3. Causa 1 — FIFO sobre um jogo que já se auto-limita

A comparação mais direta que fiz foi trocar só o modo de apresentação, mantendo cena, save e
duração constantes (~45 s por execução):

| `LARECOMP_VK_PRESENT` | p50 | p95 | 1% low |
|---|---|---|---|
| `fifo` | 17–26 ms | **34 ms** | **27–28 fps** |
| `mailbox` | 16–18 ms | **23–25 ms** | **37–42 fps** |
| `immediate` | 17–18 ms | 19–25 ms | 40–52 fps |

A diferença de p95 entre FIFO e mailbox é de ~9 a 10 ms, e o ganho de `1% low` chega a +50%. Isso
não é um tradeoff de qualidade — é trabalho duplicado. O jogo **já** faz o seu próprio pacing de
60 Hz em `nvn_present_pace` (`runtime/nvn_hle.h`), alinhando a apresentação a uma grade de ticks de
19,2 MHz herdada do Switch. Empilhar FIFO por cima aplica uma segunda espera de refresh sobre a
primeira, e é justamente esse empilhamento que produz o padrão 17/34.

O próprio código do backend já documenta esse modo de falha em `vulkan/device/swapchain.c`, onde
mailbox é descrito como o default para evitar que "cada submissão ficasse enfileirada atrás da que
espera por uma imagem da janela". A documentação e o comportamento efetivo divergem, porém:

- `astral_chain/run_guarded.sh:64` fixa `LARECOMP_VK_PRESENT="${LARECOMP_VK_PRESENT:-fifo}"`;
- `userdata/launcher.ini` traz `present_mode=immediate` convivendo com `vsync=true`, combinação
  contraditória que o launcher resolve silenciosamente para FIFO ao montar o ambiente.

O resultado é que a via de execução por terminal e a via de execução pelo launcher chegam a modos
diferentes, e nenhuma das duas expressa a intenção do código.

---

## 4. Causa 2 — spin-wait queimando ~2,3 núcleos

Medindo consumo agregado por janela de 12–14 s via `/proc/<pid>/task/*/stat`, o processo consome
**6,7 a 8,5 núcleos** para entregar 60 FPS de um jogo desenhado para 3 núcleos de Switch. O detalhe
que localiza o problema é a distribuição por thread:

```
cores   user    sys   tid
 1.00   0.56   0.45   2851882
 1.00   0.55   0.45   2851881
 1.00   0.55   0.45   2851942
 0.95   0.53   0.42   2852407
```

Várias threads marcando **exatamente 1,00 core** com aproximadamente 45% do tempo em modo *system*
não é trabalho computacional — trabalho real tem variância. É a assinatura de um laço que gira
fazendo syscall. A origem está no shim POSIX que traduz a API de espera do Win32:

```c
// extra/core/platform_posix.h:243
#define Sleep(ms) usleep((ms) * 1000)
```

No Windows, `Sleep(0)` cede o restante do quantum ao escalonador, o que é semanticamente "não
consuma CPU desnecessariamente". Mapeado para `usleep(0)`, o comportamento é oposto: a chamada
resolve via `nanosleep` com prazo zero, retorna imediatamente e o laço **continua girando**. Em
`plat_sleep_until_ns` (`runtime/nvn_hle.h:241`) isso vira um spin puro:

```c
void plat_sleep_until_ns(uint64_t t) {
    for (uint64_t now = plat_now_ns(); now < t; now = plat_now_ns()) Sleep(0);
}
```

Esse laço é chamado no caminho de pacing, e o pacer tem um `while (rt_ticks() < target) Sleep(0)`
equivalente. Cada frame que espera ~16 ms de grade queima um núcleo inteiro em vez de dormir.

A prova isolada: desligando apenas o pacing (`AC_FPS=0 AC_PRESENT_INTERVAL=0`), o consumo cai de
**6,70 para 4,43 núcleos**, com tempo de sistema de 2,83 para 1,64. São **~2,3 núcleos recuperados**
sem alterar uma linha de lógica do jogo.

O custo não é só energético. Threads girando competem pelo `klock` do runtime com as threads que
realmente produzem trabalho; a contenção resultante aparece como jitter de agendamento, ou seja,
alimenta diretamente o `p95` que se quer estabilizar.

---

## 5. Causa 3 — write tracking de páginas inexistente no Linux

Este é o item estrutural, e o mais caro de todos. O runtime herdado consulta o sistema operacional
para saber quais páginas de 4 KB do heap convidado (2,8 GB) a CPU reescreveu desde a última
consulta. No Windows isso é `GetWriteWatch`. Em `runtime/nvn_hle.h:265`, a implementação POSIX é um
no-op:

```c
static void track_harvest(uint64_t lo, uint64_t hi) {
#ifndef _WIN32
 (void)lo; (void)hi;
```

Consequência direta no log de inicialização:

```
[vk] no page write tracking (-1, page 4096)
```

Com `pt_init()` falhando, três otimizações inteiras caem de uma vez, e todas aparecem zeradas na
telemetria:

**a) O vertex cache nunca entra em operação.** A linha de stats mostra
`vcache 0 hit 0 in place 0.0 MB copied, 0 queries`. O cache device-local que existe para evitar
tráfego de vértices pelo barramento é simplesmente código morto nesta plataforma.

**b) Todo o rastreio de textura é perdido.** A telemetria reporta `tex unwritten 0 untracked 1220`
— ou seja, **1220 de 1220** texturas na categoria "sem rastreio". O caminho `LARECOMP_VK_TEX_WATCH`,
que deveria evitar re-hashing de texturas não modificadas, converte-se em fallback de hash amostral
contínuo: **533 MB re-hasheados a cada 2 s** (~270 MB/s na render thread). Isso é fonte direta de
hitch, e explica picos de `p99` que não se correlacionam com a cena.

**c) O espelho do heap fica congelado após o primeiro registro.** Sem saber o que a CPU escreveu, o
buffer device-local não tem como ser atualizado de forma incremental. A adaptação pragmática
existente assume que "buffer acima de 512 KB é estático" (`vulkan/exec/cache/vcache.c:320`,
`LARECOMP_VK_DIRECT_MIN_SIZE`). É uma heurística que funciona na prática, mas é uma aposta de
**corretude**, não uma otimização: qualquer conteúdo dinâmico acima do limiar leria dados
estagnados.

O detalhe importante é que **a solução existe e é nativa**. Validei o mecanismo *soft-dirty* do
kernel Linux nesta máquina: `clear_refs` com valor 4 zera os bits, e o bit 55 do
`/proc/self/pagemap` marca exatamente as páginas escritas. Com `MADV_NOHUGEPAGE` (que o heap já
aplica em `memory_trampoline.h`), o discriminador é preciso:

```
page   10 dirty=1      ← única página escrita
total dirty pages of 4096: 1
```

Sem `MADV_NOHUGEPAGE` a Transparent Huge Page (`[always]` neste host) contamina o resultado e marca
todas as páginas do bloco de 2 MB — razão pela qual a recomendação de `NOHUGEPAGE` é obrigatória, e
o motivo de o teste ingênuo dar falso positivo.

O custo de varredura também foi medido, e é aceitável: **~6,7 GB/s** (151 ms por GB, incluindo o
`clear_refs` e a leitura do pagemap). Como a varredura só precisa cobrir as faixas registradas — não
os 2,8 GB inteiros — o custo por submissão fica na ordem de microssegundos, comparável ao
`GetWriteWatch` que o upstream usa no Windows.

---

## 6. Causa 4 — 3,9 GB de write-back a cada dois segundos

A telemetria registra `write-back 3906.4MB in 2206` de forma estável, o que equivale a **~2 GB/s de
`memcpy`** sustentado. O caminho lazy existe para evitar isso, mas está desligado por padrão no
Linux (`play.sh:18` e o launcher forçam `LARECOMP_VK_LAZY_WB=0`), e o motivo é técnico e sólido:
o mecanismo se apoia em `mprotect` + `SIGSEGV`, e o ledger do projeto (`SHADOW_CHANGES.md`) já
registra que a troca de proteções fragmenta o VAD e torna consultas subsequentes várias vezes mais
lentas.

Confirmei o tradeoff medindo o caminho alternativo: com `LARECOMP_VK_LAZY_WB=1` o consumo **sobe**
para 9,83 núcleos (contra 6,70), com p95 equivalente. Ou seja, a decisão de desligar o lazy está
correta e não deve ser revertida.

O problema que resta é outro: o write-back copia **faixas inteiras** que a GPU escreveu, mesmo
quando quase todo o conteúdo é GPU-only (listas de luzes são o exemplo citado no próprio comentário
de `heap_mirror.c`). O caminho de melhoria não é tornar a cópia mais rápida, é copiar menos.

---

## 7. Observação secundária — granularidade de render pass

O log mostra ~2.800 render passes por segundo, ou ~47 por frame, e cada `begin_pass` emite um
barrier `ALL_COMMANDS → ALL_COMMANDS` (`exec/commands/render_pass.c:60`). Com a GPU em 20% isso não
é o gargalo hoje, e não deve ser atacado antes de P0–P2. Mas vale registrar como a próxima fronteira:
se o objetivo passar a ser margem para resolução mais alta ou 120 Hz, a granularidade de pass e a
amplitude dos barriers serão o limite seguinte.

---

## 8. Ideias de melhoria, em ordem de retorno

**P0 — Alinhar o modo de apresentação (1 linha, ganho medido de ~9 ms no p95).**
Trocar o default de `run_guarded.sh:64` para `mailbox` e reconciliar `userdata/launcher.ini`, de
modo que a via terminal e a via launcher expressem a mesma intenção. Como o jogo já faz pacing
próprio, não há perda de suavidade nem introdução de tearing. É a correção com melhor relação
ganho/esforço/risco de todo o diagnóstico e deveria entrar isolada, para que o ganho seja
atribuível.

**P1 — Corrigir a semântica de `Sleep` no shim POSIX (~2,3 núcleos, baixo risco).**
`Sleep(0)` deve mapear para `sched_yield()` e `Sleep(ms>0)` para espera bloqueante real. Em paralelo,
os laços `while (now < target) Sleep(0)` devem virar espera absoluta com `clock_nanosleep`, mantendo
spin curto apenas nos últimos ~200 µs para preservar precisão de pacing. Isso resolve desperdício de
CPU e contenção de `klock` no mesmo movimento, e é o item que mais provavelmente estabiliza o p95 no
uso diário (fora de jogo, com outras tarefas competindo por núcleos).

**P2 — Implementar write tracking nativo via soft-dirty (o fix arquitetural).**
Fornecer `plat_written_pages` e `track_harvest` em `extra/core/`, usando `clear_refs` + `pagemap` +
`MADV_NOHUGEPAGE`, respeitando a fronteira de código estabelecida. Isso religa simultaneamente o
vertex cache, o `TEX_WATCH` e a coerência real do espelho — e permite **remover** a heurística dos
512 KB, trocando uma aposta de corretude por um mecanismo verificado. É o item que responde
diretamente à percepção original de "usa muito mais recursos que a plataforma original".

**P3 — Reduzir o volume do write-back.**
Registrar quais faixas a CPU efetivamente lê e copiar apenas essas, em vez de todas as faixas
escritas pela GPU. Ataca os ~2 GB/s sem reabilitar o caminho de `mprotect`, que já se provou pior.

**P4 — Avaliar granularidade de pass/barrier.**
Só depois de P0–P2, e com perfil de GPU (`LARECOMP_GPUPROF_AT`) em mão para justificar.

---

## 9. O que não foi verificado

As medições acima vêm de **uma cena** (delegacia/HQ, primeiros ~4.000 frames após o carregamento).
Os percentis de p95 são sensíveis à cena, e o ganho de P0/P1 pode variar em áreas mais densas ou em
cutscenes. O volume de 3,9 GB por janela é observado, mas não atribuído a faixas específicas — P3
exige instrumentação adicional antes de qualquer estimativa de ganho.

O protótipo soft-dirty foi validado quanto a **corretude** e **custo de varredura isolado**, não sob
carga real de 2,8 GB com múltiplas threads convidados escrevendo em paralelo. A semântica de
`clear_refs` é global ao processo, o que impõe um ponto de serialização entre consumidores — esse é
o principal risco de P2 e precisa de medição antes de adoção. Também não foi testado em Mesa/Intel
(ANV) nem em NVK, onde o comportamento de `pagemap` pode diferir.

Nenhuma das melhorias P0–P4 foi implementada ou re-testada em jogo até o momento desta análise.

---

## 10. Como reproduzir

```bash
# Telemetria de percentis + perfil por categoria
LARECOMP_VK_PROF=1 ./astral_chain/run_guarded.sh --run build/game --seconds 40

# Comparativo de modo de apresentação
for m in fifo mailbox immediate; do
  LARECOMP_VK_PRESENT=$m ./astral_chain/run_guarded.sh --run build/game --seconds 45
done

# Isolar o custo do spin-wait (pacing desligado)
AC_FPS=0 AC_PRESENT_INTERVAL=0 ./astral_chain/run_guarded.sh --run build/game --seconds 45

# Consumo por thread (janela de 12 s)
PID=$(pgrep -n -f 'round-00/trace')
for t in /proc/$PID/task/*; do
  awk -v t=$(basename $t) '{print t,$14,$15}' $t/stat
done
```

`AC_PARALLEL_STATS=1` e `LARECOMP_VK_PROF=1` fornecem, respectivamente, a atribuição de esperas por
thread convidado e o detalhamento de tempo por categoria na render thread — ambos já presentes no
runtime, sem necessidade de ferramentas externas.
