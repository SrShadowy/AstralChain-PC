#!/usr/bin/env bash
# Astral Chain PC: one-time setup on Linux. Builds the native game from dump and keys.
set -e
APP_ROOT="${AC_APP_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
DATA_ROOT="${AC_DATA_DIR:-$APP_ROOT}"
cd "$DATA_ROOT"

if [ -n "${AC_PYTHON:-}" ] && [ -x "$AC_PYTHON" ]; then
    PY_EXEC="$AC_PYTHON"
elif [ -x "$APP_ROOT/usr/bin/python3" ]; then
    PY_EXEC="$APP_ROOT/usr/bin/python3"
else
    PY_CMD=""
    for py in python3 python3.14 python3.13 python3.12 python3.11 python3.10; do
        if command -v "$py" >/dev/null 2>&1; then
            PY_CMD="$py"
            break
        fi
    done
    if [ -z "$PY_CMD" ]; then
        echo "Error: Python 3.10 or newer is required." >&2
        exit 1
    fi

    # Check if system python already has the minimal required packages
    if "$PY_CMD" -c "import capstone, lz4.block; import zstandard" >/dev/null 2>&1; then
        PY_EXEC="$PY_CMD"
    else
        if [ ! -f "$DATA_ROOT/.venv/bin/python" ]; then
            echo "Creating Python virtual environment (.venv)..."
            "$PY_CMD" -m venv "$DATA_ROOT/.venv" || { echo "Failed to create .venv" >&2; exit 1; }
            echo "Installing required Python dependencies (capstone, lz4, zstandard)..."
            "$DATA_ROOT/.venv/bin/python" -m pip install --disable-pip-version-check -q -r "$APP_ROOT/requirements.txt" || {
                echo "Failed to install Python packages." >&2
                exit 1
            }
        fi
        PY_EXEC="$DATA_ROOT/.venv/bin/python"
    fi
fi

export PYTHONPATH="$APP_ROOT/src${PYTHONPATH:+:$PYTHONPATH}"
"$PY_EXEC" -m astral_chain.build.setup_game "$@"
