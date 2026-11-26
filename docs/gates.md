# Quality gates

Every quality rule is enforced by a machine gate that fails CI
(`.github/workflows/quality-gates.yml`, the `gates` job: ubuntu, under a
minute, no C++ toolchain needed). The gates are ratcheted: checked-in
baseline and scope files decide where each gate applies strictly, and those
files may only move toward full coverage. CI has the final say; the same
scripts run locally with plain `python tools/ci/<script>.py`.

## The ratchet model

The refactoring plan migrates the codebase module by module. A migrated
module must (a) link `r573::warnings`, (b) be clean under clang-tidy,
(c) be comment-free, (d) leave the file-length baseline. Code that has not
been migrated may not get worse:

- The file-length baseline is gone (deleted at P9, once it was empty after
  the P7/P8 decompositions). The gate is now a hard 1000-line limit on every
  tracked `.cpp/.h/.hpp`, with no baseline and no way back in: a file that
  grows past the limit fails CI until it is split up.
- The no-comments gate reached whole-repo coverage at P9. It now checks every
  tracked file with a known comment syntax and forbids comments in all of
  them. `tools/ci/no_comments_exempt.json` lists the only exceptions, paths and
  globs that keep their comments on purpose: `.idea/*` (IDE config) and
  `vcpkg-overlays/*` (vendored third-party portfiles). A new source file is
  caught by default, and there is no per-file opt-in list any more. The
  retired `no_comments_scope.json` allowlist was the ratchet used to get here
  (it grew per migrated file through P8 and was removed at P9, when the gate
  switched to checking everything not exempt).
- The machine-path gate (`check_machine_paths.py`) has no baseline. It has run
  over every tracked file from the start, since any hit is a violation whatever
  the migration state. It flags absolute drive-letter paths (`X:\...` /
  `X:/...`) in any tracked file. `tests/*` is exempt because test fixtures use
  arbitrary path strings on purpose (the `Settings::SameGameDir`
  case-folding cases and the CLI parse cases), and the shared
  `no_comments_exempt.json` globs apply too. Machine-local path config belongs
  in gitignored files (`tools/render_regress/game_dirs.json`, with the tracked
  `game_dirs.example.json` as the template).
- The banned-characters gate (`check_banned_chars.py`) bans em and en dashes,
  smart quotes and non-breaking spaces (plus their HTML entities) in every
  tracked file, which makes the owner's no-dash rule a check. The banned set is
  built from `chr()` code points so the gate file itself stays clean.
- The strict MSVC warning set (`r573::warnings`, /W4 + curated /w14xxx) is
  linked into every compiled target, including the `renderer` monolith
  executable that held the un-migrated sources and every test executable.
  The five newest test targets were missed for a while, so a CMake assertion
  at the bottom of CMakeLists.txt now fails the configure if any
  EXECUTABLE/STATIC_LIBRARY target omits `r573::warnings`. Getting there meant
  making the monolith warning-clean: dead statics, unused params and dead
  locals removed, and every `getenv`/`fopen`/`sscanf`/`strncpy` (C4996 "unsafe
  function") routed through a safe path. Warnings are errors:
  `CMAKE_COMPILE_WARNING_AS_ERROR=ON` lives in the base preset
  (CMakePresets.json), so every TU compiles with `-WX` in dev and ci alike,
  and clang-tidy (whole-tree scope at P9) fails on regressions too.

## Gate details

### File length (`check_file_length.py`)

Hard limit of 1000 lines per `.cpp/.h/.hpp` (a repo rule older than the
refactor). Files meet it by being split into modules, never through
`foo_part2.cpp` shims. clang-tidy's `readability-function-size` complements
it (LineThreshold 60 / ParameterThreshold 8). The migration-era threshold of
100 was lowered to 60 when P9 was complete, by owner decision, and the whole
tree is clean at 60.

### No comments (`check_no_comments.py`)

Owner rule: code documents itself, and knowledge lives in `docs/`. It applies
to all file types, not only C++ (owner decision). `// NOLINT` and
`// clang-format off` markers are comments and are banned too, so clang-tidy
suppressions must be structural (per-directory `.clang-tidy` overrides or
check subtraction in config), never per-line.

For C++, comment detection is a single token-alternation regex
(`CPP_TOKEN_RE`) that consumes raw strings, ordinary strings and char
literals before it can see `//` or `/* */`, so `//` inside string literals
(URLs, Windows paths) cannot cause a false positive. That gives the same
result as a lexer without the libclang dependency the gate used to carry (the
switch is recorded in docs/ci.md). Python uses the stdlib `tokenize` module.
YAML, CMake and gitignore use a quote-aware `#` scan, batch files match
`rem`/`::` statements, and JSON matches `//`-style lines (real JSON cannot
carry comments; the check is there so JSONC never sneaks in). Markdown under
`docs/` is exempt, since prose is what it is for.

### GUI isolation (`check_gui_isolation.py`)

This gate turns the P8 shell rule into a check: `ImGui::` / `ImGuiIO` /
`#include <imgui...` may appear only under `src/gui/` and `tests/gui/`. Code
outside the gui module talks to the UI through App::State (requests, view
state), and business logic never renders widgets. The gate scans tracked and
untracked `src/**.cpp|.h` and `tests/**.cpp|.h`, excluding those two
directories. The codebase was already fully clean when the gate was added, so
it starts with no baseline and any hit fails CI. `tests/gui/` is exempt because
the headless GUI suite has to drive ImGui directly (docs/gui_tests.md). It is
the only test directory allowed to, and it tests the shell rule instead of
breaking it.

### Qt isolation (`check_qt_isolation.py`)

The same shape as the gui-isolation gate, for the other toolkit. Qt headers,
`Q_OBJECT`, `Qt::` and the common `Q*` types may appear only under `editor/`;
`src/**` and `tests/**` are scanned and must stay clean. The editor's document
model (docs/document.md) and the preview protocol live in `src/` so they can be
tested in the renderer's `ci` suite, which links no Qt at all, and so the same
code could serve a different front end. The gui-isolation gate now scans
`editor/` too, so together the two gates keep ImGui out of the editor and Qt out
of everything the renderer builds.

The codebase was clean when the gate was added, so it starts with no baseline
and any hit fails CI.

### Host isolation (`check_host_isolation.py`)

This gate works in the opposite direction to the gui-isolation gate and makes a
narrower claim. `check_gui_isolation.py` keeps ImGui out of the engine; this one
keeps the engine hosts out of the scene preset editor: no `PresetHost::`,
`Scene3dHost::` or `Gc2dHost::` may appear in `src/gui/timeline/**` or
`src/gui/gui_preset_library.*`. Those files run on the GUI thread while the
render thread is inside `PresetHost::RenderFrame`, and there is no mutex in
`src/preset`, `src/scene3d` or `src/gc2d`. The editor therefore posts
`PresetCmd`s through `App::State` and reads `App::PresetStatus` back, and
`Backend::ApplyPresetCommand` applies them on the render thread at a frame
boundary (docs/gui.md 3.5).

The scope is limited on purpose. `gui_scene3d_panel.cpp`, `gui_gc2d_panel.cpp` and
the visibility predicates in `panel_registry.cpp` still call the hosts directly and
keep doing so until a separate change. The gate covers only the editor and the
library, so it starts clean and any hit inside that scope fails CI. The old Screens
panel (`gui_preset_panel.cpp`), which did call `PresetHost::` from the GUI thread, was
deleted in M7, and the library that replaced it is inside the scope.
To add a file to the scope, add it to `SCOPES` in the script.

`tools/ci/tests/test_host_isolation.py` tests the gate itself by building throwaway
git repositories: an editor file that only posts commands passes, one that calls
each of the three hosts fails with the file, line and host named, a library file is
in scope, and a panel outside the scope is ignored (`checked == 0`, which also shows
the gate is not silently scanning nothing).

### Generated layer verdicts (`gen_layer_verdicts.py`, build step, not a gate)

`docs/preset_layers.md` is also read at build time. A CMake custom command runs
`tools/ci/gen_layer_verdicts.py docs/preset_layers.md <build>/generated/preset_layer_verdicts.cpp`,
and the generated file is compiled into `r573_app`, so the editor can print a layer's verdict
beside a hidden part without shipping the markdown. It is not a gate and never fails a build over
a classification; it fails only when the markdown has no classification rows at all. The file
lives in the build tree and is never committed. See docs/preset_document.md.

### Preset layers (`check_preset_layers.py`) and preset states (`check_preset_states.py`)

Both gates read the same input: the JSON the renderer itself dumps with
`573Renderer.exe --preset-dump-defaults <tmp>` (`tools/ci/preset_dump.py` runs it
into a temporary directory and loads `*/*.json`; `--dump <dir>` reuses an existing
dump instead). They stopped regex-parsing `scene_presets_*.cpp` when the built-in
presets became documents: the C++ these gates used to scrape no longer described
what a preset draws, and M8 deleted those files.

For that reason both gates run in `tools/checks.sh` (which builds first) and not in
the hosted `quality-gates` job, which has no C++ toolchain. The hosted job runs the
gates' own pytest self-test, which needs no exe.

- The layers gate collects, per document, every `sprite.draw` / `sprite.animate`
  clip's (asset dir, cell or animation) and every `hidden_parts` entry, and checks
  each against `docs/preset_layers.md`: drawn layers must have a `background`
  verdict, hidden parts a `chrome` one, and a layer with no row at all fails.
- The states gate collects every `markers[].label` and every option choice label
  per document and checks `docs/preset_states.md` in both directions: a documented
  state no preset exposes fails, a marker or choice with no row fails, and a row
  naming a preset id no built-in provides fails. No id rule of any kind is left,
  neither the original "the id must start with `iidx`" nor a `<build>-` prefix. A
  row is recognised by the table it sits in (the one headed `| preset | state |`,
  and `| preset | state still missing |` for the gaps) and is keyed by document id
  alone, and the dump decides which ids exist. A prefix rule would have skipped the
  very rows worth catching, since a mistyped id usually matches no prefix either.

Both gates fail on an empty input instead of passing with nothing checked: zero
documents, zero sprite clips or zero states each produce a FAILED line saying the
gate would otherwise see nothing. This replaced the old "declared versus parsed
count" self-check, which existed for the same reason (a parser that silently
matched nothing).

A dump that exits non-zero is reported with its exit code and the tail of the
renderer's own log. `Log::Init` gives the process a fresh console
(`AllocConsole` plus `freopen("CONOUT$")`), so a pipe on the exe's stdout captures
nothing, and the first version of this report printed `exited 2:` followed by two
spaces. `preset_dump.py` therefore runs the exe with the dump directory as its
working directory, which is where `renderer.log` is written, and reads the log back
from there. The log is deleted with the temporary dump instead of being left in
whatever directory the gate was started from.

`tools/ci/tests/test_preset_gates.py` (`uv run pytest` from `tools/ci`) tests the
gates themselves: it builds synthetic dump directories in a tmp dir and asserts that
each of those failure modes fails, along with the real problems (an unclassified
layer, an undocumented marker, a documented state a preset lost, a docs row whose
id carries no build prefix and matches no document). Two cases put a fake renderer
(a one-line `.bat` or `sh` script that exits non-zero, with and without writing a
log) in place of the exe, so the failure path runs without breaking the real one.
The self-test is what shows these two gates can fail.

`tools/local/preset_sweep.py` reads the same dump. It renders every built-in of a
build, once per marker (`--preset-test ... <marker frame + 60>`) and once per
choice of the first option (`--preset-option <id>=<label>`), into `screenshots/`.

### Format (`clang-format --dry-run --Werror`)

`.clang-format` writes down the style the codebase already uses: LLVM base,
4-space indent, 100 columns, attached braces, left pointer alignment, short
guard-ifs allowed on one line, no namespace indentation. Intended departures
from the LLVM defaults:

- `FixNamespaceComments: false`: the default would add `// namespace X`
  comments, which break the no-comments rule.
- `SortIncludes: Never`: include order in this codebase can matter
  (windows.h before d3d9 headers, imgui backend order), and reordering
  changes behaviour, which a formatter must never do. Include hygiene is
  handled during module migration instead.
- `ReflowComments: false`: comments are being moved to docs, not
  reformatted, until P9 removes them entirely.

The tool version is pinned through pip (`clang-format==19.1.7`) in CI and for
local runs. Formatting output differs across major versions, so the pin keeps
local and CI in agreement. To upgrade, bump the pin and reformat the repo in
the same commit.

The gate runs through `tools/ci/run_format.py` (`--fix` formats in place), and
`run_tidy.py` is the equivalent for clang-tidy. Both scripts do two things a
bare `clang-format`/`clang-tidy` call cannot:

1. They resolve the tool to the pip wheel's own bundled binary
   (`<site-packages>/clang_format/data/bin/clang-format[.exe]`, likewise
   for clang_tidy) through `importlib.util.find_spec`, instead of trusting
   PATH order. The GitHub windows runner ships a system LLVM (19.1.5) that
   comes before the pip-installed scripts dir on PATH, so a bare `clang-tidy`
   runs 19 even right after `pip install clang-tidy==21.1.6` succeeds.
   Resolving through the module makes local and CI use the pinned binary
   whatever system install shadows it.
2. They refuse to run when the resolved binary's version does not match the
   pin, and print the exact pip command instead of producing phantom
   findings.

Both guards were added after real failures. A background pip upgrade once
moved both tools to LLVM 22 on a dev machine (v22-formatted output passed the
local v22 dry-run, then failed CI's v19 check, and v22's new tidy checks fired
on files that were already clean), and the CI runner's system LLVM 19 shadowed
the pinned 21 for clang-tidy.

## clang-tidy

`.clang-tidy` at the root enables whole check groups and subtracts from them
instead of picking checks one by one: bugprone, clang-analyzer, concurrency,
cppcoreguidelines, misc, modernize, performance, portability, readability, with
`WarningsAsErrors: '*'`. Documented subtractions:

- `modernize-use-trailing-return-type`: style churn that finds no defects.
- `readability-identifier-length`: short names (`x`, `id`, `uv`) are normal
  in render math.
- `readability-magic-numbers` + `cppcoreguidelines-avoid-magic-numbers`
  (an alias pair; both names must be disabled or the check still fires): the
  codebase is full of RE-derived constants whose meaning lives in docs, and
  wrapping each in a named constant adds indirection without adding knowledge.
- `bugprone-easily-swappable-parameters`: fires on every (x, y, w, h)-style
  signature in a renderer, so the noise outweighs the findings.
- `misc-non-private-member-variables-in-classes`: plain structs with public
  data are how this codebase passes state, on purpose.
- `cert-*` / `hicpp-*` are not added on top: they are mostly aliases of
  checks already enabled and would report every finding twice.
- `readability-braces-around-statements.ShortStatementLines: 2` permits the
  codebase's single-line guard idiom (`if (x) return;`), which
  `.clang-format` allows on purpose through
  `AllowShortIfStatementsOnASingleLine: WithoutElse`. The two configs must
  agree, or every guard clause becomes a finding.
- `portability-avoid-pragma-once` (new in LLVM 21): `#pragma once` is the
  project convention. This is an MSVC-only project where it is fully
  reliable, and converting to include guards would be churn for nothing.
- `cppcoreguidelines-owning-memory`: it only understands the `gsl::owner<>`
  vocabulary, so it fires on the project's correct RAII pattern
  (`std::unique_ptr` with a custom deleter calling the CRT release
  function). The codebase does not use GSL.
- `readability-function-cognitive-complexity.IgnoreMacros: true`: Catch2
  assertion macros expand to try/catch machinery that inflates the metric,
  and the reader never sees that complexity.
- `bugprone-suspicious-stringview-data-usage`: contradicts
  `cppcoreguidelines-pro-bounds-pointer-arithmetic` on the usual
  `std::from_chars(v.data(), std::to_address(v.end()))` idiom. The check
  cannot see the size flowing through `to_address`, and the spelling it would
  accept (`v.data() + v.size()`) is what the bounds check bans. from_chars
  receives the full range either way, so the finding is a false positive, and
  the bounds check covers far more code, so it wins.
- `misc-no-recursion`: this codebase walks trees (afp clip trees, the lazy
  sub-layer tree, future scene graphs), where recursion is the right tool.
  The check even fires inside MSVC STL templates instantiated for recursive
  value types (vector-of-children nodes), where the flagged frames are STL
  headers and nothing in our code could change them.
- `misc-include-cleaner.IgnoreHeaders: stdio.h`: include-cleaner attributes
  the Microsoft CRT extensions (`fopen_s`, `_fseeki64`) to `stdio.h`, while
  `modernize-deprecated-headers` bans that header. `<cstdio>` provides them
  on MSVC in practice, so the missing-include diagnostic is suppressed for
  that one header instead of disabling either check.

clang-tidy runs against the MSVC compile database directly (verified with
the pip `clang-tidy==19.1.0` wheel in cl driver mode against
`build/compile_commands.json`), so no clang-cl configure is required. Local
run: `pip install clang-tidy==19.1.0`, then
`clang-tidy -p build --quiet src/<file>.cpp`.

Structural suppression (the approved replacement for NOLINT) is now a
three-layer chain of per-directory configs:

- `src/.clang-tidy` is the transitional config for the flat monolith layer
  (plus `src/gui/`, which inherits it). It subtracts the checks that this
  codebase's approved designs break by their nature, as opposed to code that
  could be fixed. FFI/DLL interop (GetProcAddress casts, base+offset pokes
  into game DLL data segments, PE walking): `pro-type-reinterpret-cast`,
  `pro-type-cstyle-cast`, `pro-bounds-pointer-arithmetic`,
  `performance-no-int-to-ptr`, `pro-type-union-access` (D3D/engine record
  types), `no-malloc` (the libavs allocator contract hands malloc'd heaps
  to the game DLLs). Logging/UI printf design (`LOG`, `ImGui::Text`):
  `pro-type-vararg`, `pro-bounds-array-to-pointer-decay`, `macro-usage`.
  C-API interop buffers: `avoid-c-arrays` (+ its modernize alias),
  `pro-bounds-constant-array-index`. Win32 APIs that take non-const
  pointers they never write through (WIC `WritePixels`):
  `pro-type-const-cast`. The documented app_globals seam
  (docs/ownership.md): `avoid-non-const-global-variables`. Plus
  `misc-const-correctness.AnalyzePointers: false` (the support template)
  and `readability-function-size.ParameterThreshold: 9` (afp-core's
  TexUpload callback ABI is a fixed 9-parameter signature; the module
  configs set the threshold back to 8). These subtractions go away file by
  file as files migrate into modules.
- `src/preset/defaults/.clang-tidy` subtracts `readability-function-size` for
  that directory alone. Those files are the 18 built-in preset documents
  (docs/preset_document.md): one function per screen, whose whole body is a
  single `return Document{...}` data literal, generated once from the converter M8
  deleted (docs/preset_document.md, "How the defaults were produced"). The
  line threshold measures control-flow complexity a person has to keep in
  mind, and a data literal has none; splitting one into 60-line pieces would add
  call indirection to a table. Every other check still applies there, including the
  1000-line file limit that forced the ending into two halves.
- `src/gui/timeline/.clang-tidy` subtracts `bugprone-exception-escape` for the
  timeline editor alone. Every editor gesture defers its document change to the
  end of the GUI frame as an `Editor::Edit` (`std::function<bool(Document&)>`,
  docs/gui.md 3.5), and MSVC's `std::function` converting constructor is
  conditionally `noexcept` when the callable fits its small-object buffer. Those
  lambdas capture clip and track ids by value, so clang-tidy follows the lambda's
  copy constructor from inside a `noexcept` frame, finds `std::string`'s allocating
  copy, and reports an escaping `bad_array_new_length` for every one of them. The
  callables do have to own their captures, because the locals they read are gone by
  the time the edit runs, and nothing else in the tree hits this, because the other
  `std::function` seams (`App::State::MutateLiveOverrides`, the progress callbacks)
  take reference captures. Every other check applies there, including the function
  size and cognitive-complexity thresholds, which is why the transport row, the track
  header and the shortcut table are each split into three functions.
- `src/support/math/.clang-tidy` subtracts
  `cppcoreguidelines-pro-bounds-constant-array-index` for that directory alone.
  It holds `float_trig.cpp`, the deterministic `Support::Sinf` / `Support::Cosf`
  transcription of FreeBSD msun (docs/support.md, "Deterministic float
  trigonometry"), whose Payne-Hanek reduction indexes constant tables with
  computed indices by design. Every other `src/` directory already has that
  check off, and `src/support` is the one that turns it back on. Every other
  check applies there.
- `clang-analyzer-optin.core.EnumCastOutOfRange` (an opt-in analyzer
  check) is subtracted in the root config, not per layer. It only ever fires
  on system-header patterns we cannot change: the D3D9 SDK's own
  `D3DTS_WORLD` transform-state macro (= `(D3DTRANSFORMSTATETYPE)256`, a
  documented valid value outside the base enum, hit by the DDR
  fixed-function SetTransform path) and the MSVC STL's internal
  `__std_fs_stats_flags` cast inside `std::filesystem::directory_iterator`
  (hit by any TU that iterates a directory under root strictness, such as
  tests).
- Each migrated module dir (`src/{cli,formats,loop,media,render,settings,
  state}/.clang-tidy`) turns that whole list back on (clang-tidy merges
  parent then child, and the last match wins), so the clean modules keep full
  root strictness and lose nothing to the transitional layer.
- `src/support/.clang-tidy` keeps only its own four FFI subtractions
  (vararg, reinterpret-cast, macro-usage, pointer-arithmetic, since it is the
  FFI layer) and turns the rest of the src-level list back on.

The blocking tidy gate is `tools/ci/run_tidy.py`. It runs the pinned
`clang-tidy==21.1.6` (pip) over every tracked `*.cpp` that appears in the
MSVC compile database at `build/compile_commands.json` (the whole tree; the
old `tools/ci/tidy_scope.json` allow-list is gone). Every TU the build compiles
is tidied, so a new file is covered as soon as it is added to the build, the
same way the no-comments gate works. A standalone source outside the main
CMake build has no database entry and is therefore left out.
It runs in cl driver mode (no clang-cl configure needed) in the build
workflow, after the Test step, since a configured build dir must exist. Header
findings surface through the TUs that include them via `HeaderFilterRegex`,
which names the module dirs explicitly:
`formats|media|cli|settings|state|loop|render`. Two exclusions are on purpose:

- `src/support` headers are left out even though the module has its own config.
  Header diagnostics are attributed under the config of the TU that includes
  them, and support's FFI patterns (the `DLL_LOAD_AS` stringize macro, the
  GetProcAddress `reinterpret_cast` in dll_loader.h) are approved by
  `src/support/.clang-tidy` only for TUs inside that dir. Any other includer
  would report them as findings it cannot fix. The support TUs themselves are
  still tidied under full strictness minus the four documented FFI
  subtractions.
- Flat `src/*.h`, `src/gui`, `src/backend`, `src/scene3d` and `src/gc2d`
  headers are left out: they are the transitional layer, and their headers
  join the filter as they migrate into module dirs, like their TUs.

When `loop` and `render` joined the filter, the sweep found 2 real header
problems (blend_map.h missing designated initializers, frame_pacer.h enum base
type). Both were fixed, not suppressed.

`run_tidy.py` caches per-file results in `.tidycache/`. A file's cache key
hashes the pinned tool version, the compile command, the source, every header
the ninja dep graph says the TU includes, and the entire `.clang-tidy` config
chain (every tracked `.clang-tidy` file, root and per-directory). The chain
hash is needed because per-directory configs are not compiler dependencies.
Before it existed, an edit to `src/.clang-tidy` invalidated nothing and every
affected file got a stale cache hit, so a suppression could pass CI without
ever being analysed. The CI cache key in build-renderer.yml uses the same glob
set for the same reason.

Version notes, all of which came up during the first migration:

- The MSVC STL hard-asserts a minimum Clang version (`yvals_core.h`
  STL1000). clang-tidy 19 cannot parse the VS 18 STL, and clang-tidy 21 can.
  The tidy pin has to move together with Visual Studio updates; a red tidy
  job right after a VS upgrade is expected and means "bump the pin".
- The `/Zc:` conformance flags in the compile database are cl-only. clang
  warns `unused-command-line-argument`, which `WarningsAsErrors: '*'`
  turns into an error, so `run_tidy.py` passes
  `--extra-arg=/clang:-Qunused-arguments`.
- Wildcard check groups gain new checks across LLVM releases
  (`portability-avoid-pragma-once` appeared in 21 and needed a policy
  decision), so an unpinned tidy plus `WarningsAsErrors: '*'` breaks CI on
  toolchain updates, with no code change.

Local run: `pip install clang-tidy==21.1.6`, then `python
tools/ci/run_tidy.py` (or `clang-tidy -p build --quiet <file>` for one
file).

## The editor is built, tested and packaged in CI

The `editor` job of `.github/workflows/build-renderer.yml` configures with the
`editor` preset, builds every editor target and runs both suites the way
`tools/checks.sh` does: `editor_widget_tests.exe` then
`editor_window_tests.exe`, each as one process instead of through ctest. The
editor's cases are not registered with `catch_discover_tests`, because
discovery would launch a process per case and every window case builds a whole
`Editor::Window`; one process per file takes seconds instead of minutes. The
live cases skip themselves, since `R573_IIDX_DIR` is not set on a runner.

It needs a different dependency set from the renderer job: the `editor` feature
of `vcpkg.json` with `VCPKG_MANIFEST_NO_DEFAULT_FEATURES`, so no ffmpeg and no
imgui, though with the same `x64-windows-static` triplet as the renderer (ADR
0009). Its vcpkg archive cache therefore has its own key
(`vcpkg-archives-editor-...-x64-windows-static-...`) and does not evict the
renderer's.

**The first run on a cold cache builds Qt from source**, which takes most of
the job's time. Every later run restores qtbase, qtsvg and the docking system
from the archive cache. The first run after the switch to the static triplet
was such a cold run. Two things evict the cache: a bump of the vcpkg submodule
or of `vcpkg.json` (both are in the key), and GitHub's own 7-day idle eviction
and 10 GB per-repository limit, which this cache shares with the renderer's. If
a run suddenly takes hours, the cache is the cause, not the code.

### The artefact is two executables that run

`preview_host.exe` belongs to the renderer side: it loads the game's own avs2 and
afp DLLs and draws through D3D9, so it links `r573_afp_host` and the render
stack, and the editor preset (`R573_BUILD_RENDERER=OFF`) returns from the
top-level `CMakeLists.txt` before that target exists. `FindPreviewHost` looks for
it beside the editor and then in the renderer's `build/`. That is why a developer
with both projects built never notices, and why the editor's own build directory
has no copy.

So the `editor` job `needs` the renderer `windows` job, which saves the x64
`build/preview_host.exe` to the Actions cache under
`preview-host-<os>-<sha>`. The editor job restores it to the same path
(`fail-on-cache-miss`), so it is never an artefact of its own; alone it is
useless. `tools/ci/stage_editor.py` copies it and `ifs_editor.exe` into
`dist/`, and nothing else. Qt, its plugins and the C++ runtime are linked into
the exe (ADR 0009), so the stager copies no DLL or plugin folder, and a build
that starts needing one again fails the start check below instead of shipping.
That folder is the artefact.

`tools/ci/check_editor_starts.py` then runs the staged `ifs_editor.exe` under
`QT_QPA_PLATFORM=minimal` for six seconds and fails if it exits. A missing DLL
exits at once (`0xc0000135`), which no file listing would have shown, and so
does a missing platform plugin, which is why `qminimal` is linked into the
shipped exe. The check runs from a temporary working directory, so the editor's
own log and `dev/` folder do not end up in the artefact. `tools/checks.sh` runs
both scripts after the editor suites, so the packaging is checked locally as
well.

The first editor artefact was a hand-written list of paths: the exe, the Qt
DLLs, the plugin folders. It was missing `preview_host.exe`, so the download
started and then said "preview_host.exe was not found next to the editor" as
soon as a game install was set, although the list looked complete. That is why
the staging goes through a script that names what it requires, and why the
check starts the result instead of counting its files.

### What the contrast gate counts as ink

`Theme::InkColours` lists the colours the interface paints words with, and
nothing else. The accent and its hover shade are not in it: the stylesheet uses
them for a background, a 1 px focus border, a 2 px tab underline and a slider
fill, never for text (`PanelTabBar::paintEvent`'s `fillRect` is the only
painted use outside the sheet). While they were in the list, the pixel walk
reported the worst-contrast token inside a widget's rect, so a focus ring made
it say `search_query "depth" #806226 on #1a1c20 is 3.00:1` about a word that is
painted in `kText`. The claim was false and the threshold was the wrong one:
WCAG asks 4.5:1 of text and 3:1 of a non-text mark.

The marks are checked instead by `Theme::kLeastMark` (3:1) in the accent sweep,
against the page, a panel and a field, for every accent in the list.

That gap only showed once the editor was built in CI. The walk measures
whatever accent the host reports, the runner reports the Windows default blue,
and `Fitted` used to darken a dark accent so that white text on it would read,
which pushed the mark itself down to 2.98:1 against a field. `Fitted` now
lightens instead, until the accent both carries readable ink and clears 3:1 on
a field. `Every word stays readable whatever accent Windows reports` pins two
accents (the Windows default, and an olive that lands between the two
thresholds) and walks the window under each, so the machine's own accent no
longer decides whether the gate passes.

## Running everything locally

```
pip install clang-format==19.1.7 libclang==18.1.1
python tools/ci/check_file_length.py
python tools/ci/check_no_comments.py
python tools/ci/check_gui_isolation.py
python tools/ci/check_qt_isolation.py
git ls-files '*.cpp' '*.h' '*.hpp' | xargs clang-format --dry-run --Werror
```

The DLL-dependent pixel leg runs separately (never hosted):
`python tools/local/render_regression.py`, see docs/local_regression.md.

## The 2D blend equations are unit tested, pixel exact, with no GPU

`GcAnim::FactorsFor(Blend)` and `GcAnim::TexelDiscarded(alpha)` are the single
description of the 2D pipeline's per-pixel behaviour. `Gc2d::Renderer` translates
them into D3D9 render states, and `gcanim_tests` composites with them on the CPU,
so a blend change is checked against exact expected pixels without a device, a
window or any game data.

The test guards against one regression: a logo cell drawn over a background must
not punch a black rectangle through it. A fully transparent texel has to leave the
destination byte-identical. That broke as soon as the replace blend shipped
without the alpha test, and it broke the IIDX 17 SIRIUS title screen.

Build the synthetic package in the test instead of reaching for real game files:
a package is a few `SysIdx::Cell` and `SysIdx::Record` values, and a test that
needs a game install cannot run in CI.

## Crash reports carry a symbolized stack

`Support::CaptureStackAddresses` walks the faulting thread with `StackWalk64`,
and `Support::DescribeAddresses` turns those addresses into
`module+0x... Symbol (file.cpp:line)` through DbgHelp, reading the `.pdb` that
sits next to the binary. `[CRASH]` lines and `RenderSeh`'s caught render faults
both print one frame per line.

The work is split in two because MSVC refuses `__try` in a function that has to
unwind objects, so the SEH filter stores plain addresses into a fixed array on
`FaultReport` and the symbolizing happens outside it.

This was added after three rounds of diagnosing IIDX 34 faults from a bare
`module+offset`, which needed an out-of-process DbgHelp script each time to turn
into a file and line. With the stack in the report, one run is enough where it
used to take three.

## raw-dll-offset gate

`tools/ci/check_raw_dll_offsets.py` fails the build when a literal offset is
added to a loaded DLL base, as in `(uint8_t*)afpcore + 0x377B0`. Every such
address belongs in `AfpProfiles::DllOffsetSet`, read through `ActiveOffsets()`,
so each game build carries its own measured value, and an unknown build gets zero
and skips the call instead of jumping into the middle of an instruction.

The gate was added after four separate hardcoded afp-core addresses shipped in
the qpro code and each one crashed IIDX 34 in a different way, one per debugging
round: the mc-work lookup, the definition lookup, and the matrix stack's base
and depth. Review missed them because they sat inside lambdas and static
initializers, away from the other offsets.

## Text contrast gate

Neither interface may draw text that is hard to read. Two test cases enforce
this, and both run in `tools/checks.sh`, so the gate fails before anything is
pushed.

`editor/tests/editor_contrast_window_tests.cpp` builds the real editor window
off-screen with the theme applied, grabs it, and walks every visible, enabled
widget that carries text. For each one it takes the most common colour inside
the widget as the surface and looks for the theme's own ink colours
(`Theme::InkColours`) among the widget's pixels, ignoring the surface colour
itself. A token that covers at least a few pixels is text (or an icon) that
was really painted there. The worst pair goes through `Theme::Contrast`, the
WCAG relative-luminance ratio, and anything under `Theme::kLeastContrast`
(4.5:1, the WCAG AA threshold for normal text) fails the case with its name,
colour and ratio. Sampling painted pixels instead of the palette lets it see
through Qt stylesheets: a `:checked` rule's colour never reaches
`QWidget::palette()`, and snapping to the theme's exact tokens keeps
antialiased glyph edges from being taken for ink. The cases cover the start
screen, an open animation with a depth chosen, selected keyframes, the history
panel, the graph tab, a project's chips, the command search and a popover. A
fourth case checks the pairs the panels paint themselves, where there is no
widget to sample: the row delegate's names and details on a panel, a hovered
and a chosen row, the panel tab strip, and the timeline's frame numbers, depth
numbers, depth names and the names on each kind of bar.

`tests/gui/contrast_tests.cpp` does the static half for the renderer's ImGui
interface. It applies the style for every profile accent and checks
`ImGuiCol_Text` against every surface text sits on (window, popup, frame,
title, menu bar, table header, button, header and tab, in both their plain and
hovered forms), compositing translucent surfaces over the window first.
Disabled text is exempt in both, as WCAG exempts inactive controls.

A fifth case sweeps the accent itself. The editor takes its accent from
Windows, so the derivation in `Theme::UseAccent` runs for the design blue, the
machine's own accent, the Windows default, a bright yellow, a near-black navy,
a mid grey and a magenta, and every pair that accent colours (ink on it, the
chip on it, ink on its hover, the text and detail on a chosen row, the tint on
one) has to clear the threshold for all of them. That sweep is why the hover
tilts away from the ink instead of always toward white, and why the chosen
row's tint is pushed until it reads.

The gate was added because the start screen's "Ctrl O" chip shipped as
`#8a909b` on the accent `#4c9dff`, a ratio of 1.16:1, so the text could only be
found by someone who knew it was there. The same sweep found the row delegate
drawing its detail line in `#8a909b` on a chosen row's `#1b3350` (4.00:1). Both
are fixed, and the gate keeps them from coming back.
