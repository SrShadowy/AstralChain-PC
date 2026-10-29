#!/usr/bin/env bash
# Quick launcher runner
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "build-launcher/astral_chain_launcher" ]; then
    echo "Building launcher..."
    cmake -B build-launcher -S extra/ui/launcher -GNinja
    cmake --build build-launcher
fi

exec ./build-launcher/astral_chain_launcher "$@"
