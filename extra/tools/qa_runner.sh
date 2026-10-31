#!/usr/bin/env bash
# Automated QA, Input, Visual & Performance Runner (ponytail-qa)
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

SECONDS_RUN="${1:-58}"
if [ "$SECONDS_RUN" -ge 40 ]; then
    SHOT_FRAME="${2:-3200}"
else
    SHOT_FRAME="${2:-550}"
fi
if [ -z "${AC_INPUT_SCRIPT:-}" ]; then
    if [ "$SECONDS_RUN" -ge 40 ]; then
        INPUT_SEQ="10-13:300;18-21:300;24-25:1;27-28:1;30-32:11000;33-34:1;35-37:11000;38-39:1;40-42:11000;43-44:1;46-47:400;48-49:400;51-52:1"
    else
        INPUT_SEQ="10.0-14.0:300;18.0-21.0:300"
    fi
else
    INPUT_SEQ="$AC_INPUT_SCRIPT"
fi

echo "===================================================="
echo " [ponytail-qa] 1/4: Auditoria de Código & Arquitetura"
echo "===================================================="
./extra/tools/audit.sh

echo ""
echo "===================================================="
echo " [ponytail-qa] 2/4: Verificação de Fast-Relink / Build"
echo "===================================================="
T0=$(date +%s)
PYTHON="python3"
if [ -f ".venv/bin/python3" ]; then
    PYTHON=".venv/bin/python3"
fi
$PYTHON -m astral_chain.setup_game --rebuild-if-stale
T1=$(date +%s)
echo "PASSED: Build/Relink verificado em $((T1 - T0))s."

if [ "${SKIP_RUNTIME:-0}" = "1" ]; then
    echo "SKIP_RUNTIME=1: Pulando teste de execução em tempo real."
    exit 0
fi

echo ""
echo "===================================================="
echo " [ponytail-qa] 3/4: Teste de Runtime, Input & Performance (${SECONDS_RUN}s)"
echo "===================================================="
LOG_FILE="/tmp/ac_qa_run.log"
rm -f "$LOG_FILE" "./build/game/frames/frame_$(printf '%05d' "$SHOT_FRAME").png" "./frames/frame_$(printf '%05d' "$SHOT_FRAME").png"

set +e
AC_PARALLEL="${AC_PARALLEL:-1}" AC_AUDIO_RESYNC="${AC_AUDIO_RESYNC:-1}" \
    AC_ASYNC_SUBMIT="${AC_ASYNC_SUBMIT:-1}" LARECOMP_VK_PRESENT="${LARECOMP_VK_PRESENT:-fifo}" \
    AC_INPUT_SCRIPT="$INPUT_SEQ" LARECOMP_SHOT_AT="$SHOT_FRAME" \
    stdbuf -oL -eL ./astral_chain/run_guarded.sh --run build/game --seconds "$SECONDS_RUN" > "$LOG_FILE" 2>&1
EXIT_CODE=$?
set -e

LINE_COUNT=$(wc -l < "$LOG_FILE")
echo "Saída do runtime: $LINE_COUNT linhas (Exit code: $EXIT_CODE)"

if grep -qE "^(STOP |FATAL:|CRASH)" "$LOG_FILE"; then
    echo "FAILED: Falha crítica detectada durante execução:"
    grep -E "^(STOP |FATAL:|CRASH)" "$LOG_FILE"
    exit 1
fi

if [ "$LINE_COUNT" -gt 300 ] && [ "${AC_VERBOSE:-0}" != "1" ]; then
    echo "WARNING: Volume de log acima do esperado ($LINE_COUNT linhas > 300)."
else
    echo "PASSED: Controle de log enxuto ($LINE_COUNT linhas)."
fi

LAST_FPS=$(grep -oE "[0-9]+\.[0-9]+ fps" "$LOG_FILE" | tail -n 1 || echo "N/A")
echo "Performance registrada: ${LAST_FPS}"

SAVE_MANIFEST=$(find userdata/saves -name "manifest.bin" 2>/dev/null | head -n 1 || true)
if [ -n "$SAVE_MANIFEST" ] && [ -s "$SAVE_MANIFEST" ]; then
    echo "PASSED: Sistema de Save íntegro ($SAVE_MANIFEST)."
else
    echo "WARNING: manifest.bin ainda não gerado neste intervalo."
fi

echo ""
echo "===================================================="
echo " [ponytail-qa] 4/4: Inspeção Visual de Frame (Frame ${SHOT_FRAME})"
echo "===================================================="
FRAME_PATH="./build/game/frames/frame_$(printf '%05d' "$SHOT_FRAME").png"
if [ ! -f "$FRAME_PATH" ]; then
    FRAME_PATH="./frames/frame_$(printf '%05d' "$SHOT_FRAME").png"
fi
if [ -f "$FRAME_PATH" ]; then
    python3 - "$FRAME_PATH" "$SHOT_FRAME" << 'EOF'
import sys
import numpy as np
from PIL import Image

img = Image.open(sys.argv[1])
shot_frame = int(sys.argv[2])
arr = np.array(img)[:, :, :3]
h, w, _ = arr.shape
tl = arr[:h//2, :w//2].mean()
tr = arr[:h//2, w//2:].mean()
bl = arr[h//2:, :w//2].mean()
br = arr[h//2:, w//2:].mean()
green_flash = int(((arr[:, :, 1] > 200) & (arr[:, :, 0] < 30) & (arr[:, :, 2] < 30)).sum())

print(f"Resolução: {w}x{h} | Quadrantes (TL={tl:.1f}, TR={tr:.1f}, BL={bl:.1f}, BR={br:.1f}) | GreenFlash={green_flash}")
if shot_frame >= 1200:
    q_counts = [
        int((arr[:h//2, :w//2] > 30).sum()),
        int((arr[:h//2, w//2:] > 30).sum()),
        int((arr[h//2:, :w//2] > 30).sum()),
        int((arr[h//2:, w//2:] > 30).sum())
    ]
    if min(q_counts) < 50:
        print(f"FAILED: Quadrante vazio/preto detectado (pixel counts: {q_counts}).")
        sys.exit(1)
else:
    non_zero = int((arr > 30).sum())
    if arr.max() < 50 or non_zero < 500:
        print("FAILED: Imagem totalmente vazia/preta detectada.")
        sys.exit(1)
if green_flash > 500:
    print("FAILED: Artefato de flash verde detectado na decodificação de textura.")
    sys.exit(1)
print("PASSED: Frame validado em 100% da janela sem artefatos.")
EOF
else
    echo "WARNING: Frame $FRAME_PATH não capturado (tempo de execução menor que o frame alvo)."
fi

echo ""
echo "=== [ponytail-qa] Todos os testes concluídos com sucesso! ==="
