#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

run_build() {
    local script="$1"
    local log
    log="$(mktemp)"
    MSYS_NO_PATHCONV=1 cmd.exe /c "$(cygpath -w "$ROOT/$script")" 2>&1 | tee "$log"
    if ! grep -q "Build successful" "$log"; then
        echo "checks: $script never reported success - build step did not run or failed"
        rm -f "$log"
        exit 1
    fi
    rm -f "$log"
}

run_build build.bat
run_build build32.bat

EDITOR_LOG="$(mktemp)"
MSYS_NO_PATHCONV=1 cmd.exe /c "$(cygpath -w "$ROOT/editor/build.bat")" 2>&1 | tee "$EDITOR_LOG"
if ! grep -q "Editor build succeeded" "$EDITOR_LOG"; then
    echo "checks: editor/build.bat never reported success"
    rm -f "$EDITOR_LOG"
    exit 1
fi
rm -f "$EDITOR_LOG"
"$ROOT/build-editor/editor_widget_tests.exe"
"$ROOT/build-editor/editor_window_tests.exe"

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
uv run --project tools/ci python tools/ci/check_raw_dll_offsets.py
uv run --project tools/ci python tools/ci/check_gui_isolation.py
uv run --project tools/ci python tools/ci/check_qt_isolation.py
uv run --project tools/ci python tools/ci/check_host_isolation.py
uv run --project tools/ci python tools/ci/check_preset_layers.py
uv run --project tools/ci python tools/ci/check_preset_states.py
(cd tools/ci && uv run pytest -q)
uv run --project tools/ci python tools/ci/run_format.py
uv run --project tools/ci --group dev ruff check tools/
uv run --project tools/ci python tools/ci/run_tidy.py

echo "ALL CHECKS PASSED"
