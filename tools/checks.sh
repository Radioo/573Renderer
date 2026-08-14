#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_LOG="$(mktemp)"
BUILD_BAT="$(cygpath -w "$ROOT/build.bat")"
MSYS_NO_PATHCONV=1 cmd.exe /c "$BUILD_BAT" 2>&1 | tee "$BUILD_LOG"
if ! grep -q "Build successful" "$BUILD_LOG"; then
    echo "checks: build.bat never reported success - build step did not run or failed"
    rm -f "$BUILD_LOG"
    exit 1
fi
rm -f "$BUILD_LOG"

CTEST_EXE="ctest"
if [ -f build/CMakeCache.txt ]; then
    CMAKE_EXE="$(grep -m1 '^CMAKE_COMMAND:INTERNAL=' build/CMakeCache.txt | cut -d= -f2-)"
    if [ -n "$CMAKE_EXE" ]; then
        CAND="$(dirname "$CMAKE_EXE")/ctest.exe"
        if [ -f "$CAND" ]; then
            CTEST_EXE="$CAND"
        fi
    fi
fi

"$CTEST_EXE" --test-dir build -L ci --output-on-failure

uv run --project tools/ci python tools/ci/check_file_length.py
uv run --project tools/ci python tools/ci/check_no_comments.py
uv run --project tools/ci python tools/ci/check_banned_chars.py
uv run --project tools/ci python tools/ci/check_machine_paths.py
uv run --project tools/ci python tools/ci/check_gui_isolation.py
uv run --project tools/ci python tools/ci/check_preset_layers.py
uv run --project tools/ci python tools/ci/run_format.py
uv run --project tools/ci --group dev ruff check tools/
uv run --project tools/ci python tools/ci/run_tidy.py

echo "ALL CHECKS PASSED"
