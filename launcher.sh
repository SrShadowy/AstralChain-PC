#!/usr/bin/env bash
# Quick launcher runner
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -x "$SCRIPT_DIR/usr/bin/astral_chain_launcher" ]; then
    export AC_APP_DIR="${AC_APP_DIR:-$SCRIPT_DIR}"
    export AC_DATA_DIR="${AC_DATA_DIR:-$SCRIPT_DIR}"
    export LD_LIBRARY_PATH="$SCRIPT_DIR/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export QT_PLUGIN_PATH="$SCRIPT_DIR/usr/plugins"
    export QT_QPA_PLATFORM_PLUGIN_PATH="$SCRIPT_DIR/usr/plugins/platforms"
    if [ -d "$SCRIPT_DIR/usr/lib/python3.10" ]; then
        export PYTHONHOME="$SCRIPT_DIR/usr"
    fi
    if [ -x "$SCRIPT_DIR/usr/bin/python3" ]; then
        export AC_PYTHON="$SCRIPT_DIR/usr/bin/python3"
    fi
    exec "$SCRIPT_DIR/usr/bin/astral_chain_launcher" "$@"
fi

if [ ! -f "build/launcher/astral_chain_launcher" ]; then
    echo "Building launcher..."
    cmake -B build/launcher -S src/launcher -GNinja
    cmake --build build/launcher
fi

exec ./build/launcher/astral_chain_launcher "$@"
