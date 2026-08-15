# tools/ci

The quality gates. Every script here is run by `tools/checks.sh` and by the
hosted `quality-gates` workflow, and each one exits non-zero with the offending
paths on failure. Rationale for every rule lives in `docs/gates.md`; this file is
how to run them.

This is a uv project: `uv run <script>` works with nothing to install. Commands
below assume you are in this directory.

## The gates

```bash
uv run check_file_length.py
uv run check_no_comments.py
uv run check_banned_chars.py
uv run check_machine_paths.py
uv run check_gui_isolation.py
uv run check_host_isolation.py
uv run check_preset_layers.py
uv run check_preset_states.py
uv run run_format.py
uv run run_tidy.py
```

| script | what it fails on |
|---|---|
| `check_file_length.py` | a tracked `.cpp/.h/.hpp` over 1000 lines |
| `check_no_comments.py` | a comment in any tracked file with a known comment syntax, outside `no_comments_exempt.json` |
| `check_banned_chars.py` | em/en dashes, smart quotes, non-breaking spaces and their HTML entities |
| `check_machine_paths.py` | an absolute drive-letter path in a tracked file |
| `check_gui_isolation.py` | ImGui symbols outside `src/gui/` and `tests/gui/` |
| `check_host_isolation.py` | `PresetHost::` / `Scene3dHost::` / `Gc2dHost::` inside `src/gui/timeline/` or `src/gui/gui_preset_library.*` |
| `check_preset_layers.py` | a 2D layer a built-in preset draws with no `background` row in `docs/preset_layers.md`, or a hidden part with no `chrome` row |
| `check_preset_states.py` | a preset marker or option choice with no row in `docs/preset_states.md`, and a documented state no preset exposes |
| `run_format.py` | clang-format differences (`--fix` formats in place) |
| `run_tidy.py` | clang-tidy findings on changed files |

`run_format.py` and `run_tidy.py` resolve the pip-pinned binary out of the venv
rather than trusting PATH, and refuse to run on a version mismatch.

## The preset gates need a build

`check_preset_layers.py` and `check_preset_states.py` read the preset documents
the renderer dumps, so `bin/573Renderer.exe` has to exist (`build.bat`, which
`tools/checks.sh` runs first). They shell out to
`573Renderer.exe --preset-dump-defaults <tmpdir>` themselves; no game data and no
window are involved. To check an existing dump instead, for example one written
by hand while debugging a gate:

```bash
uv run check_preset_layers.py --dump ../../build/preset_dump
```

The exe is run WITH the dump directory as its working directory, because the
renderer writes `renderer.log` next to wherever that is: the log lands in the
temporary dump and goes away with it instead of being left here, and when the dump
exits non-zero the gate prints the tail of that log with the exit code. The exe's
own stdout is not it - `Log::Init` gives the process a fresh console, so a pipe
placed on stdout captures nothing.

## Self-test

```bash
uv run pytest
```

`tests/test_host_isolation.py` builds throwaway git repositories and asserts the
host-isolation gate passes on a command-only editor file, fails on each of the three
hosts with the file and line named, covers the preset library, and ignores panels
outside its scope.

`tests/test_preset_gates.py` builds synthetic dump directories and asserts the two
preset gates fail on each blind spot they are supposed to catch: an empty dump, a
dump with no sprite clips, a dump with no markers or choices, an unclassified
layer, an undocumented marker, a documented state the preset lost, and a docs row
naming a preset that does not exist. It needs no exe, so it runs in the hosted
gates job too.
