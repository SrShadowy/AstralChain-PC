#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "=== Downstream Audit ==="

echo "--- 1. Marker Audit (Region Start/End or Single-Line Hook) ---"
UNMARKED=$(git diff --unified=0 HEAD -- ':!extra' ':!*.py' ':!*.md' ':!*.sh' ':!*.txt' ':!*.json' ':!*.gitignore' ':!*.cmd' ':!*.ps1' ':!*.cs' ':!*.manifest' ':!*.exe' | awk '
function check_hunk() {
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

echo "--- 2. File Size Audit (<= 300 lines ideal, max 400) ---"
OVERSIZED=0
while IFS= read -r f; do
    lines=$(wc -l < "$f")
    if [ "$lines" -gt 400 ]; then
        echo "ERROR: $f has $lines lines (exceeds 400 hard cap)"
        OVERSIZED=1
    elif [ "$lines" -gt 300 ]; then
        echo "WARNING: $f has $lines lines (>300 lines)"
    fi
done < <(find extra -type f \( -name "*.h" -o -name "*.c" -o -name "*.cpp" -o -name "*.hpp" \))

if [ "$OVERSIZED" -ne 0 ]; then
    echo "FAILED: File size audit failed."
    exit 1
fi
echo "PASSED: All extra source files within size limits."

echo "--- 3. Build Check (Qt6 Launcher) ---"
cmake -B build-launcher -S extra/ui/launcher -GNinja > /dev/null
cmake --build build-launcher > /dev/null
echo "PASSED: Qt6 Launcher builds cleanly."

echo "=== All audits passed successfully! ==="
