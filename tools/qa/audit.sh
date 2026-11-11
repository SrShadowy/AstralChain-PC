#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

# The Python self-tests need capstone (the ARM64 decoder). setup.sh installs it
# into .venv when the system interpreter lacks it, so prefer the venv and fall
# back to python3 when that one already has the module.
PYTHON=python3
if [ -x "$ROOT_DIR/.venv/bin/python" ]; then
    PYTHON="$ROOT_DIR/.venv/bin/python"
fi
if ! "$PYTHON" -c "import capstone" >/dev/null 2>&1 && python3 -c "import capstone" >/dev/null 2>&1; then
    PYTHON=python3
fi

export PYTHONPATH="$ROOT_DIR/src${PYTHONPATH:+:$PYTHONPATH}"

echo "=== Downstream Audit ==="

echo "--- 1. Marker Audit (Region Start/End or Single-Line Hook) ---"
UNMARKED=$(git -c diff.algorithm=histogram diff -M --unified=0 HEAD -- 'src/runtime' 'astral_chain/runtime' | awk '
function is_whole_file_extra(f,    cmd, first, last, line) {
    if (f in whole_cache) return whole_cache[f]
    first = ""; last = ""
    while ((getline line < f) > 0) {
        if (line !~ /^[[:space:]]*$/) {
            if (first == "") first = line
            last = line
        }
    }
    close(f)
    whole_cache[f] = (first ~ /\/\/ \[extra\][[:space:]]*$/ && last ~ /\/\/ \[extra\][[:space:]]*$/) ? 1 : 0
    return whole_cache[f]
}
function check_hunk() {
    if (n > 0 && is_whole_file_extra(file)) {
        n = 0
        return
    }
    if (n == 1) {
        if (lines[1] !~ /\/\/ \[extra\][[:space:]]*$/) {
            printf "%s (%s): single-line hook missing // [extra]:\n  %s\n", file, hunk, lines[1]
        }
    } else if (n > 1) {
        if (lines[1] !~ /\/\/ \[extra\][[:space:]]*$/) {
            printf "%s (%s): region start missing // [extra]:\n  %s\n", file, hunk, lines[1]
        }
        if (lines[n] !~ /\/\/ \[extra\][[:space:]]*$/) {
            printf "%s (%s): region end missing // [extra]:\n  %s\n", file, hunk, lines[n]
        }
    }
    n = 0
}
/^diff --git / { check_hunk(); file = $4; sub(/^b\//, "", file); next }
/^@@ /         { check_hunk(); hunk = $0; next }
/^\+\+\+ /     { next }
/^\+/          {
    if ($0 !~ /^\+[[:space:]]*$/) {
        lines[++n] = $0
    }
    next
}
END            { check_hunk() }
')

if [ -n "$UNMARKED" ]; then
    echo "FAILED: The following upstream regions/hooks lack '// [extra]':"
    echo "$UNMARKED"
    exit 1
fi
echo "PASSED: All upstream C/C++ hooks (1 line) and regions (start/end) marked with // [extra]."

echo "--- 2. File Size & Readability Audit (<= 350 lines ideal, max 450; no compressed blocks) ---"
OVERSIZED=0
while IFS= read -r f; do
    lines=$(wc -l < "$f")
    if [ "$lines" -gt 450 ]; then
        echo "ERROR: $f has $lines lines (exceeds 450 mandatory split cap)"
        OVERSIZED=1
    elif [ "$lines" -gt 350 ]; then
        echo "INFO: $f has $lines lines (>350 lines, cohesive context)"
    fi
done < <(find src/hooks src/additions src/launcher tools/qa -type f \( -name "*.h" -o -name "*.c" -o -name "*.cpp" -o -name "*.hpp" \))

if [ "$OVERSIZED" -ne 0 ]; then
    echo "FAILED: File size audit failed (refactor and split files exceeding 450 lines)."
    exit 1
fi

COMPRESSED=$("$PYTHON" -c '
import glob, re
files = glob.glob("src/hooks/**/*.*", recursive=True) + glob.glob("src/additions/**/*.*", recursive=True) + glob.glob("src/launcher/**/*.*", recursive=True)
bad = []
for f in sorted(files):
    if not f.endswith((".c", ".cpp", ".h", ".hpp")):
        continue
    with open(f) as fp:
        for idx, line in enumerate(fp, 1):
            s = line.strip()
            if s.startswith("//") or s.startswith("#") or s.startswith("\"") or s.startswith("QPushButton") or s.startswith("QLabel"):
                continue
            s_nostr = re.sub(r"\"(?:\\.|[^\"\\])*\"", "\"\"", s)
            s_nostr = re.sub(r"//.*$", "", s_nostr).strip()
            if re.search(r"\{[^}]*;[^}]*;[^}]*\}", s_nostr):
                bad.append(f"{f}:{idx}: {s}")
if bad:
    print("\n".join(bad))
')
if [ -n "$COMPRESSED" ]; then
    echo "FAILED: Compressed multi-statement blocks found:"
    echo "$COMPRESSED"
    exit 1
fi
echo "PASSED: All downstream source files within size limits and free of compressed blocks."

echo "--- 3. Build Check (Qt6 Launcher) ---"
cmake -B build/launcher -S src/launcher -GNinja > /dev/null
cmake --build build/launcher > /dev/null
echo "PASSED: Qt6 Launcher builds cleanly."

echo "--- 4. Menu Pointer Self-Test ---"
if pkg-config --exists sdl3 2>/dev/null; then
    TEST_BIN="$(mktemp -t ac-pointer-test.XXXXXX)"
    if gcc -O1 -Wall -Wextra -Isrc -o "$TEST_BIN" tools/qa/mouse_pointer_test.c \
            $(pkg-config --cflags --libs sdl3) > /dev/null 2>&1 && \
            "$TEST_BIN" | grep -v '^\[input\]'; then
        rm -f "$TEST_BIN"
    else
        rm -f "$TEST_BIN"
        echo "FAILED: menu pointer self-test."
        exit 1
    fi
else
    echo "SKIPPED: SDL3 development files not found (menu pointer self-test)."
fi

echo "--- 5. RE Helper Self-Test (menu cursor / hit-test scan) ---"
if "$PYTHON" -m astral_chain.tools.re_cursor_test > /dev/null 2>&1 && \
        "$PYTHON" -m astral_chain.tools.re_cursor_e2e_test > /dev/null 2>&1; then
    echo "PASSED: re_cursor decoder and report self-tests."
else
    echo "FAILED: re_cursor self-test ($PYTHON -m astral_chain.tools.re_cursor_test)."
    exit 1
fi

echo "--- 6. FPS Patch Delta Self-Test (Dynamic-Unlocked == base + 2 words) ---"
if "$PYTHON" -m astral_chain.tools.fps_patch_test > /dev/null 2>&1; then
    echo "PASSED: Dynamic-Unlocked patch is exactly the base plus the two cap words."
else
    echo "FAILED: fps patch self-test ($PYTHON -m astral_chain.tools.fps_patch_test)."
    exit 1
fi

echo "--- 7. Frame-Pacing Self-Test (60 fps unchanged, other targets derived) ---"
PACE_CC="${CC:-gcc}"
if ! command -v "$PACE_CC" > /dev/null 2>&1; then
    echo "SKIPPED: no C compiler (frame-pacing self-test)."
else
PACE_BIN="$(mktemp -t ac-pace-test.XXXXXX)"
pace_run() {  # every target the heuristic branches on, plus unusable values
    "$PACE_BIN" > /dev/null \
        && AC_FPS=0 "$PACE_BIN" > /dev/null \
        && AC_FPS=30 "$PACE_BIN" > /dev/null \
        && AC_FPS=144 "$PACE_BIN" > /dev/null \
        && AC_FPS=1000 "$PACE_BIN" > /dev/null \
        && AC_FPS=1001 "$PACE_BIN" > /dev/null \
        && AC_FPS=abc "$PACE_BIN" > /dev/null \
        && AC_FPS=-5 "$PACE_BIN" > /dev/null \
        && AC_FPS=60fps "$PACE_BIN" > /dev/null \
        && AC_FPS=1234567890123456 "$PACE_BIN" > /dev/null \
        && AC_FPS= AC_TARGET_FPS=90 "$PACE_BIN" > /dev/null
}
if "$PACE_CC" -std=c11 -O1 -Wall -Wextra -Werror -Isrc -o "$PACE_BIN" tools/qa/pace_target_test.c -lm > /dev/null 2>&1 \
        && pace_run; then
    rm -f "$PACE_BIN"
    echo "PASSED: pacing heuristic matches the old 60 fps behaviour; thresholds scale."
else
    "$PACE_BIN" || true
    rm -f "$PACE_BIN"
    echo "FAILED: frame-pacing self-test (tools/qa/pace_target_test.c)."
    exit 1
fi
fi

echo "--- 8. Discord RPC Formatting Self-Test ---"
CXX="${CXX:-g++}"
if ! command -v "$CXX" > /dev/null 2>&1; then
    echo "SKIPPED: no C++ compiler (Discord RPC self-test)."
else
    RPC_BIN="$(mktemp -t ac-rpc-test.XXXXXX)"
    if "$CXX" -std=c++20 -O1 -Wall -Wextra -Werror -Isrc -o "$RPC_BIN" src/additions/rpc/ac_game_data.cpp tools/qa/rpc_format_test.cpp > /dev/null 2>&1 \
            && "$RPC_BIN" > /dev/null; then
        rm -f "$RPC_BIN"
        echo "PASSED: Discord Rich Presence format and activity snapshot self-test."
    else
        rm -f "$RPC_BIN"
        echo "FAILED: Discord Rich Presence self-test (tools/qa/rpc_format_test.cpp)."
        exit 1
    fi
fi

echo "--- 9. FSR 3.1 Jitter & Math Self-Test ---"
if command -v "$PACE_CC" > /dev/null 2>&1; then
    FSR_BIN="$(mktemp -t ac-fsr3-test.XXXXXX)"
    if "$PACE_CC" -std=c11 -O1 -Wall -Wextra -Werror -Isrc -o "$FSR_BIN" tools/qa/fsr3_jitter_test.c -lm > /dev/null 2>&1 \
            && "$FSR_BIN"; then
        rm -f "$FSR_BIN"
    else
        rm -f "$FSR_BIN"
        echo "FAILED: FSR 3.1 jitter self-test (tools/qa/fsr3_jitter_test.c)."
        exit 1
    fi
fi

echo "=== All audits passed successfully! ==="
