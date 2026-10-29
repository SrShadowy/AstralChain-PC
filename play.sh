#!/usr/bin/env bash
# Astral Chain PC: starts the game on Linux under SDL3/Vulkan.
set -e
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

if [ -f ".venv/bin/python" ]; then
    PY_EXEC=".venv/bin/python"
elif command -v python3 >/dev/null 2>&1; then
    PY_EXEC="python3"
else
    echo "Python environment missing. Please run ./setup.sh first." >&2
    exit 1
fi

"$PY_EXEC" -m astral_chain.setup_game --rebuild-if-stale

DEFAULT_ENV="AC_PARALLEL=1;AC_IGNORE_FOCUS=;AC_AUDIO_RESYNC=1;LARECOMP_VK_LAZY_WB=0;LARECOMP_VK_COPY_VERTICES=0;AC_PRESENT_INTERVAL=1"
HAS_ENV=0
for arg in "$@"; do
    if [ "$arg" = "--env" ] || [ "$arg" = "-Env" ]; then
        HAS_ENV=1
        break
    fi
done

if [ "$HAS_ENV" -eq 1 ]; then
    exec ./astral_chain/run_guarded.sh --run build/game --seconds 604800 "$@"
else
    exec ./astral_chain/run_guarded.sh --run build/game --seconds 604800 --env "$DEFAULT_ENV" "$@"
fi
