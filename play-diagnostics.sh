#!/usr/bin/env bash
# Astral Chain PC: run game with frame diagnostics on Linux.
export LARECOMP_VK_PROF=1
export AC_CRASH_DUMP=1
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/play.sh" "$@"
