#!/usr/bin/env bash
# Astral Chain PC: starts the game on Linux under SDL3/Vulkan.
set -e
APP_ROOT="${AC_APP_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
DATA_ROOT="${AC_DATA_DIR:-$APP_ROOT}"
cd "$DATA_ROOT"

if [ -n "${AC_PYTHON:-}" ] && [ -x "$AC_PYTHON" ]; then
    PY_EXEC="$AC_PYTHON"
elif [ -x "$APP_ROOT/usr/bin/python3" ]; then
    PY_EXEC="$APP_ROOT/usr/bin/python3"
elif [ -f "$DATA_ROOT/.venv/bin/python" ]; then
    PY_EXEC="$DATA_ROOT/.venv/bin/python"
elif command -v python3 >/dev/null 2>&1; then
    PY_EXEC="python3"
else
    echo "Python environment missing. Please run ./setup.sh first." >&2
    exit 1
fi

DEFAULT_ENV="AC_PARALLEL=1;AC_IGNORE_FOCUS=;AC_AUDIO_RESYNC=1;LARECOMP_VK_LAZY_WB=0;LARECOMP_VK_COPY_VERTICES=0;AC_PRESENT_INTERVAL=1"
HAS_ENV=0
LAUNCH_ENV=""
prev=""
for arg in "$@"; do
    if [ "$arg" = "--env" ] || [ "$arg" = "-Env" ]; then
        HAS_ENV=1
    fi
    if [ "$prev" = "--env" ] || [ "$prev" = "-Env" ]; then
        LAUNCH_ENV="$arg"
    fi
    prev="$arg"
done

# [extra] The FPS patch variant is a build input (setup_game.py active_patches), so it has to reach
# setup_game below; the launcher only puts it in the --env string meant for run_guarded.sh.
# Values are passed through as the user wrote them: fps_unlocked() is the single place that
# decides what counts as on, so the build and the runtime cannot disagree.
IFS=';' read -ra ENV_ITEMS <<< "$LAUNCH_ENV"
if [ "${#ENV_ITEMS[@]}" -gt 0 ]; then
    for item in "${ENV_ITEMS[@]}"; do
        case "$item" in
            AC_FPS_UNLOCKED=*) export AC_FPS_UNLOCKED="${item#AC_FPS_UNLOCKED=}" ;;
        esac
    done
fi

export PYTHONPATH="$APP_ROOT/src${PYTHONPATH:+:$PYTHONPATH}"
"$PY_EXEC" -m astral_chain.build.setup_game --rebuild-if-stale

if [ "$HAS_ENV" -eq 1 ]; then
    exec "$APP_ROOT/src/astral_chain/tools/run_guarded.sh" --run build/game --seconds 604800 "$@"
else
    exec "$APP_ROOT/src/astral_chain/tools/run_guarded.sh" --run build/game --seconds 604800 --env "$DEFAULT_ENV" "$@"
fi
