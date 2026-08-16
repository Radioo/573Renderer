# Quality gates

Every quality rule is enforced by a machine gate that fails CI
(`.github/workflows/quality-gates.yml`, the `gates` job: ubuntu, under a
minute, no C++ toolchain needed). The gates are ratcheted: checked-in
baseline/scope files decide where each gate applies strictly, and those files
may only move toward full coverage. CI is the sole authority; the same
scripts run locally with plain `python tools/ci/<script>.py`.

## The ratchet model

The refactoring plan migrates the codebase module by module. A migrated
module must (a) link `r573::warnings`, (b) be clean under clang-tidy,
(c) be comment-free, (d) leave the file-length baseline. Nothing
unmigrated is allowed to get worse:

- The file-length baseline is GONE (deleted at P9 once it reached empty
  after the P7/P8 decompositions). The gate is now a hard 1000-line limit
  on every tracked `.cpp/.h/.hpp` with no baseline mechanism and no
  re-entry path - a file that outgrows the limit fails CI until it is
  decomposed.
- The no-comments gate reached whole-repo coverage at P9: it now checks
  EVERY tracked file with a known comment syntax and forbids comments in all
  of them. `tools/ci/no_comments_exempt.json` lists the only exceptions -
  paths/globs that are intentionally left with comments: `.idea/*` (IDE
  config) and `vcpkg-overlays/*` (vendored third-party portfiles). A new source file
  is caught by default; there is no per-file opt-in to grow anymore. The
  retired `no_comments_scope.json` allowlist was the ratchet used to reach
  this point (it grew per migrated file through P8; removed at P9 when the
  gate flipped to catch-all-minus-exempt).
- The machine-path gate (`check_machine_paths.py`) has no baseline: it runs
  over every tracked file from day one, since any hit is a violation
  regardless of migration state. It flags absolute drive-letter paths
  (`X:\...` / `X:/...`) in any tracked file; `tests/*` is exempt because
  test fixtures legitimately use arbitrary path strings (the
  `Settings::SameGameDir` case-folding cases and the CLI parse cases), and
  the shared `no_comments_exempt.json` globs apply too. Machine-local path
  config belongs in gitignored files (`tools/render_regress/game_dirs.json`,
  with the tracked `game_dirs.example.json` as the template).
- The banned-characters gate (`check_banned_chars.py`) bans em/en dashes,
  smart quotes, and non-breaking spaces (plus their HTML entities) in every
  tracked file - the owner's no-dash rule made mechanical. The banned set is
  built from `chr()` code points so the gate file itself stays clean.
- The strict MSVC warning set (`r573::warnings`, /W4 + curated /w14xxx) is
  linked into EVERY compiled target, including the `renderer` monolith
  executable that held the un-migrated sources and every test executable
  (the five newest test targets were missed for a while; a CMake assertion
  at the bottom of CMakeLists.txt now fails the configure if any
  EXECUTABLE/STATIC_LIBRARY target omits `r573::warnings`). Reaching that
  meant driving the monolith warning-clean: dead statics / unused params /
  dead locals removed, and every `getenv`/`fopen`/`sscanf`/`strncpy`
  (C4996 "unsafe function") routed through a safe path. Warnings ARE
  errors: `CMAKE_COMPILE_WARNING_AS_ERROR=ON` lives in the base preset
  (CMakePresets.json), so every TU compiles with `-WX` in dev and ci alike,
  and clang-tidy (whole-tree scope at P9) fails on regressions too.

## Gate details

### File length (`check_file_length.py`)

Hard limit 1000 lines per `.cpp/.h/.hpp` (repo rule predating the refactor).
The limit is meant to be met by decomposition into modules, never by
`foo_part2.cpp` shims. Complemented by clang-tidy's
`readability-function-size` (LineThreshold 60 / ParameterThreshold 8; the
migration-era 100 threshold was ratcheted to 60 at P9 completion per owner
decision - the whole tree is clean at 60).

### No comments (`check_no_comments.py`)

Owner rule: code is self-documenting; knowledge lives in `docs/`. Applies to
ALL file types, not just C++ (owner decision). `// NOLINT` and
`// clang-format off` markers are comments and are BANNED too - clang-tidy
suppressions must be structural (per-directory `.clang-tidy` overrides or
check subtraction in config), never per-line.

Comment detection for C++ is a single token-alternation regex
(`CPP_TOKEN_RE`) that consumes raw strings, ordinary strings, and char
literals BEFORE it can ever see `//` or `/* */`, so `//` inside string
literals (URLs, Windows paths) cannot false-positive - same effect as a
lexer without the libclang dependency the gate originally carried (the
switch is recorded in docs/ci.md). Python uses the stdlib `tokenize`
module. YAML/CMake/gitignore use a quote-aware `#` scan, batch files match
`rem`/`::` statements, JSON matches `//`-style lines (real JSON cannot
carry comments; the checker exists so JSONC never sneaks in). Markdown
under `docs/` is exempt by nature - prose is the point.

### GUI isolation (`check_gui_isolation.py`)

The P8 shell rule made checkable: `ImGui::` / `ImGuiIO` / `#include <imgui...`
may appear ONLY under `src/gui/` and `tests/gui/`. Everything outside the gui
module talks to the UI through App::State (requests, view state) - business
logic never renders widgets. The gate scans tracked + untracked
`src/**.cpp|.h` and `tests/**.cpp|.h` excluding those two directories; the
codebase was already 100% clean when the gate was introduced, so it starts
with no baseline and any hit fails CI. `tests/gui/` is exempt because the
headless GUI suite necessarily drives ImGui directly (docs/gui_tests.md); it
is the only test directory allowed to, and it tests the shell rather than
violating it.

### Host isolation (`check_host_isolation.py`)

The OPPOSITE direction to the gui-isolation gate, and a narrower claim.
`check_gui_isolation.py` keeps ImGui out of the engine; this one keeps the ENGINE
HOSTS out of the scene preset editor: no `PresetHost::`, `Scene3dHost::` or
`Gc2dHost::` may appear in `src/gui/timeline/**` or `src/gui/gui_preset_library.*`.
Those files run on the GUI thread while the render thread is inside
`PresetHost::RenderFrame`, and there is no mutex in `src/preset`, `src/scene3d` or
`src/gc2d`; the editor therefore posts `PresetCmd`s through `App::State` and reads
`App::PresetStatus` back, and `Backend::ApplyPresetCommand` applies them on the
render thread at a frame boundary (docs/gui.md 3.5).

The claim is deliberately SCOPED. `gui_scene3d_panel.cpp`, `gui_gc2d_panel.cpp`,
`gui_preset_panel.cpp` and the visibility predicates in `panel_registry.cpp` still
call the hosts directly and keep that convention until a separate change; the gate
covers only the editor, so it starts clean and any hit inside that scope fails CI.
Adding a file to the scope means adding it to `SCOPES` in the script.

Self-tested by `tools/ci/tests/test_host_isolation.py`, which builds throwaway git
repositories: one editor file that only posts commands passes, one that calls each
of the three hosts fails with the file, line and host named, a library file is in
scope, and a panel outside the scope is ignored (`checked == 0`, so the test also
proves the gate is not silently scanning nothing).

### Generated layer verdicts (`gen_layer_verdicts.py`, build step, not a gate)

`docs/preset_layers.md` is also read at BUILD time. A CMake custom command runs
`tools/ci/gen_layer_verdicts.py docs/preset_layers.md <build>/generated/preset_layer_verdicts.cpp`
and that generated file is compiled into `r573_app`, so the editor can print a layer's verdict
beside a hidden part without shipping the markdown. It is not a gate and never fails a build on
classification; it fails only when the markdown has no classification rows at all. The file
lives in the build tree and is never committed. See docs/preset_document.md.

### Preset layers (`check_preset_layers.py`) and preset states (`check_preset_states.py`)

Both gates read the SAME input: the JSON the renderer itself dumps with
`573Renderer.exe --preset-dump-defaults <tmp>` (`tools/ci/preset_dump.py` runs it
into a temporary directory and loads `*/*.json`; `--dump <dir>` reuses an existing
dump instead). They stopped regex-parsing `scene_presets_*.cpp` when the built-in
presets became documents: the C++ these gates used to scrape no longer describes
what a preset draws.

This is why both gates run in `tools/checks.sh` (which builds first) and NOT in
the hosted `quality-gates` job, which has no C++ toolchain. What the hosted job
does run is the gates' own pytest self-test, which needs no exe.

- The layers gate collects, per document, every `sprite.draw` / `sprite.animate`
  clip's (asset dir, cell or animation) and every `hidden_parts` entry, and checks
  each against `docs/preset_layers.md`: drawn layers must have a `background`
  verdict, hidden parts a `chrome` one, and a layer with no row at all fails.
- The states gate collects every `markers[].label` and every option choice label
  per document and checks `docs/preset_states.md` in both directions: a documented
  state no preset exposes fails, a marker or choice with no row fails, and a row
  naming a preset id no built-in provides fails. There is NO id rule of any kind
  left, neither the original "the id must start with `iidx`" nor a `<build>-`
  prefix: a row is recognised by the table it sits in (the one headed
  `| preset | state |`, and `| preset | state still missing |` for the gaps) and is
  keyed by document id alone, with the dump as the authority on which ids exist.
  A prefix rule would have quietly skipped exactly the rows worth catching, since
  a mistyped id is usually one that matches no prefix either.

Both gates fail loudly on an empty input instead of passing vacuously: zero
documents, zero sprite clips, or zero states each produce a FAILED line saying the
gate would otherwise be blind. That replaced the old "declared versus parsed
count" self-check, which existed for the same reason (a parser that silently
matched nothing).

A dump that exits non-zero is reported with its exit code and the tail of the
renderer's own log. `Log::Init` hands the process a fresh console
(`AllocConsole` plus `freopen("CONOUT$")`), so a pipe on the exe's stdout captures
nothing at all and the first version of this printed `exited 2:` and two spaces.
`preset_dump.py` therefore runs the exe WITH the dump directory as its working
directory, which is where `renderer.log` is written, and reads it back from there;
the log dies with the temporary dump instead of being left in whatever directory
the gate was started from.

`tools/ci/tests/test_preset_gates.py` (`uv run pytest` from `tools/ci`) is the
gate self-test: it builds synthetic dump directories in a tmp dir and asserts each
of those failure modes actually fails, plus the real problems (an unclassified
layer, an undocumented marker, a documented state a preset lost, a docs row whose
id carries no build prefix and matches no document). Two cases stand a fake
renderer (a one-line `.bat` or `sh` script that exits non-zero, with and without
writing a log) in for the exe, so the failure path is exercised without breaking
the real one. A gate that cannot fail is worth nothing, so the self-test is what
proves these two can.

`tools/local/preset_sweep.py` reads the same dump: it renders every built-in of a
build, once per marker (`--preset-test ... <marker frame + 60>`) and once per
choice of the first option (`--preset-option <id>=<label>`), into `screenshots/`.

### Format (`clang-format --dry-run --Werror`)

`.clang-format` codifies the style the codebase already uses: LLVM base,
4-space indent, 100 columns, attached braces, left pointer alignment, short
guard-ifs allowed on one line, no namespace indentation. Deliberate
deviations from LLVM defaults:

- `FixNamespaceComments: false`: the default would ADD `// namespace X`
  comments, violating the no-comments rule.
- `SortIncludes: Never`: include order in this codebase can be
  load-bearing (windows.h before d3d9 headers, imgui backend order);
  reordering is a behavior change a formatter must never make. Include
  hygiene is handled during module migration instead.
- `ReflowComments: false`: comments are being migrated to docs, not
  reformatted, until P9 removes them entirely.

The tool version is pinned via pip (`clang-format==19.1.7`) in CI and for
local runs; formatting output differs across major versions, so the pin is
what makes local and CI agree. Upgrade deliberately: bump the pin,
reformat the repo in the same commit.

The gate runs through `tools/ci/run_format.py` (`--fix` to format in
place); `run_tidy.py` is the equivalent for clang-tidy. Both scripts do two
things a bare `clang-format`/`clang-tidy` invocation cannot:

1. They resolve the tool to the pip wheel's OWN bundled binary
   (`<site-packages>/clang_format/data/bin/clang-format[.exe]`, likewise
   for clang_tidy) via `importlib.util.find_spec`, instead of trusting
   PATH order. This matters because the GitHub windows runner ships a
   system LLVM (19.1.5) that appears on PATH BEFORE the pip-installed
   scripts dir, so a bare `clang-tidy` runs 19 even right after
   `pip install clang-tidy==21.1.6` succeeds. Resolving through the module
   makes local and CI deterministically use the pinned binary regardless
   of any shadowing system install.
2. They REFUSE to run when the resolved binary's version does not match the
   pin, failing loudly with the exact pip command instead of producing
   phantom findings.

Both guards were added after real incidents: a background pip upgrade once
moved both tools to LLVM 22 on a dev machine (v22-formatted output passed
the local v22 dry-run, then failed CI's v19 check; v22's new tidy checks
fired on already-clean files), and the CI runner's system LLVM 19 shadowed
the pinned 21 for clang-tidy.

## clang-tidy

`.clang-tidy` at the root enables whole check groups and subtracts, rather
than cherry-picking: bugprone, clang-analyzer, concurrency, cppcoreguidelines,
misc, modernize, performance, portability, readability, with
`WarningsAsErrors: '*'`. Documented subtractions:

- `modernize-use-trailing-return-type`: stylistic churn, no defect value.
- `readability-identifier-length`: short names (`x`, `id`, `uv`) are idiomatic
  in render math.
- `readability-magic-numbers` + `cppcoreguidelines-avoid-magic-numbers`
  (alias pair - both names must be disabled or the check still fires): the
  codebase is full of RE-derived constants whose meaning lives in docs;
  wrapping each in a named constant adds indirection without knowledge.
- `bugprone-easily-swappable-parameters`: fires on every (x, y, w, h)-style
  signature in a renderer; noise outweighs signal here.
- `misc-non-private-member-variables-in-classes`: plain structs with public
  data are the codebase's deliberate state-passing idiom.
- `cert-*` / `hicpp-*` are NOT added on top: they are mostly aliases of
  already-enabled checks and would double-report every finding.
- `readability-braces-around-statements.ShortStatementLines: 2` permits the
  codebase's single-line guard idiom (`if (x) return;`), which
  `.clang-format` deliberately allows via
  `AllowShortIfStatementsOnASingleLine: WithoutElse` - the two configs must
  agree or every guard clause becomes a finding.
- `portability-avoid-pragma-once` (new in LLVM 21): `#pragma once` is the
  project convention; this is an MSVC-only project where it is fully
  reliable, and converting to include guards would be churn without value.
- `cppcoreguidelines-owning-memory`: only understands `gsl::owner<>`
  vocabulary, so it fires on the project's correct RAII pattern
  (`std::unique_ptr` with a custom deleter calling the CRT release
  function). The codebase does not use GSL.
- `readability-function-cognitive-complexity.IgnoreMacros: true`: Catch2
  assertion macros expand to try/catch machinery that inflates the metric;
  the reader never sees that complexity.
- `bugprone-suspicious-stringview-data-usage`: contradicts
  `cppcoreguidelines-pro-bounds-pointer-arithmetic` on the canonical
  `std::from_chars(v.data(), std::to_address(v.end()))` idiom - the check
  cannot see the size flowing through `to_address`, while the spelling it
  would accept (`v.data() + v.size()`) is exactly what the bounds check
  bans. from_chars receives the full range either way, so the finding is a
  false positive; the bounds check covers far more surface and wins.
- `misc-no-recursion`: this codebase's domain is tree walking (afp clip
  trees, the lazy sub-layer tree, future scene graphs), where recursion is
  the correct idiom. The check even fires inside MSVC STL templates
  instantiated for recursive value types (vector-of-children nodes), where
  the flagged frames are STL headers and no code of ours could change.
- `misc-include-cleaner.IgnoreHeaders: stdio.h`: the Microsoft CRT
  extensions (`fopen_s`, `_fseeki64`) are attributed to `stdio.h` by
  include-cleaner while `modernize-deprecated-headers` bans that header -
  `<cstdio>` provides them on MSVC in practice, so the missing-include
  diagnostic for that one header is suppressed rather than either check
  disabled.

clang-tidy runs against the MSVC compile database directly (verified with
the pip `clang-tidy==19.1.0` wheel in cl driver mode against
`build/compile_commands.json`); no clang-cl configure is required. Local
run: `pip install clang-tidy==19.1.0`, then
`clang-tidy -p build --quiet src/<file>.cpp`.

Structural suppression (the sanctioned pattern replacing NOLINT) is now a
three-layer per-directory config chain:

- `src/.clang-tidy` is the TRANSITIONAL config for the flat monolith layer
  (plus `src/gui/`, which inherits it): it subtracts the checks that are
  structurally violated by this codebase's sanctioned designs, not by
  fixable code. FFI/DLL interop (GetProcAddress casts, base+offset pokes
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
  configs re-pin the threshold to 8). These subtractions die per-file as
  files migrate into modules.
- `src/preset/defaults/.clang-tidy` subtracts `readability-function-size` for
  that directory alone. Those files are the 18 built-in preset documents
  (docs/preset_document.md): one function per screen, whose whole body is a
  single `return Document{...}` data literal generated from the converter. The
  line threshold measures control-flow complexity a human has to hold in their
  head, and a data literal has none; splitting one into 60-line pieces would add
  call indirection to a table. Every other check, including the 1000-line file
  limit that forced the ending into two halves, still applies there.
- `src/gui/timeline/.clang-tidy` subtracts `bugprone-exception-escape` for the
  timeline editor alone. Every editor gesture defers its document mutation to the
  end of the GUI frame as an `Editor::Edit` (`std::function<bool(Document&)>`,
  docs/gui.md 3.5), and MSVC's `std::function` converting constructor is
  CONDITIONALLY `noexcept` when the callable fits its small-object buffer. Those
  lambdas capture clip and track ids by value, so clang-tidy walks the lambda's
  copy constructor from inside a `noexcept` frame, finds `std::string`'s allocating
  copy, and reports an escaping `bad_array_new_length` for every one of them. The
  callables genuinely must own their captures - the locals they read are gone by the
  time the edit runs - and nothing else in the tree hits this, because the other
  `std::function` seams (`App::State::MutateLiveOverrides`, the progress callbacks)
  take reference captures. Every other check applies there, including the function
  size and cognitive-complexity thresholds, which is why the transport row, the track
  header and the shortcut table are each split into three functions.
- `clang-analyzer-optin.core.EnumCastOutOfRange` (an OPT-IN analyzer
  check) is subtracted at the ROOT config, not per-layer: it only ever
  fires on system-header patterns we cannot change - the D3D9 SDK's own
  `D3DTS_WORLD` transform-state macro (= `(D3DTRANSFORMSTATETYPE)256`, a
  documented-valid value outside the base enum, hit by the DDR
  fixed-function SetTransform path) and the MSVC STL's internal
  `__std_fs_stats_flags` cast inside `std::filesystem::directory_iterator`
  (hit by any TU iterating a directory under root strictness, e.g. tests).
- Each MIGRATED module dir (`src/{cli,formats,loop,media,render,settings,
  state}/.clang-tidy`) positively RE-ENABLES that whole list (clang-tidy
  merges parent-then-child, last match wins), so the clean modules keep
  full root strictness and lose nothing to the transitional layer.
- `src/support/.clang-tidy` keeps only its own four FFI subtractions
  (vararg, reinterpret-cast, macro-usage, pointer-arithmetic - it IS the
  FFI layer) and re-enables the rest of the src-level list.

The blocking tidy gate is `tools/ci/run_tidy.py`: it runs the pinned
`clang-tidy==21.1.6` (pip) over EVERY tracked `*.cpp` that appears in the
MSVC compile database at `build/compile_commands.json` (whole-tree
catch-all; the old `tools/ci/tidy_scope.json` allow-list is gone). Any TU
the build compiles is tidied automatically - new files opt IN by being
added to the build, mirroring the no-comments gate. A standalone source
outside the main CMake build is naturally excluded because it has no database
entry.
It runs in cl driver mode - no clang-cl configure is needed - in the build
workflow (a configured build dir must exist), after the Test step. Header
findings surface through the TUs that include them via
`HeaderFilterRegex`, which names the module dirs explicitly:
`formats|media|cli|settings|state|loop|render`. Two deliberate exclusions:

- `src/support` headers are OUT even though the module has its own config.
  Header diagnostics are attributed under the INCLUDING TU's config, and
  support's FFI patterns (the `DLL_LOAD_AS` stringize macro, the
  GetProcAddress `reinterpret_cast` in dll_loader.h) are sanctioned by
  `src/support/.clang-tidy` only for TUs inside that dir - any other
  includer would report them as findings it cannot fix. The support TUs
  themselves are still tidied under full strictness minus the four
  documented FFI subtractions.
- Flat `src/*.h`, `src/gui`, `src/backend`, `src/scene3d`, `src/gc2d`
  headers are OUT: they are the transitional layer; their headers join the
  filter as they migrate into module dirs, same as their TUs.

When `loop` and `render` joined the filter, the sweep surfaced 2 real
header findings (blend_map.h missing designated initializers,
frame_pacer.h enum base type) - fixed, not suppressed.

`run_tidy.py` caches per-file results in `.tidycache/`: a file's cache key
hashes the pinned tool version, the compile command, the source, every
header the ninja dep graph says the TU includes, AND the ENTIRE `.clang-tidy`
config chain (every tracked `.clang-tidy` file, root and per-directory).
The chain hash matters: per-directory configs are not compiler dependencies,
so before it existed an edit to `src/.clang-tidy` invalidated nothing and
every affected file returned a stale cache hit - a suppression could go
green in CI without ever being analysed. The CI cache key in
build-renderer.yml mirrors the same glob set for the same reason.

Version notes, all hit in practice during the first migration:

- The MSVC STL hard-asserts a minimum Clang version (`yvals_core.h`
  STL1000); clang-tidy 19 cannot parse the VS 18 STL, clang-tidy 21 can.
  The tidy pin must move together with Visual Studio updates - a red tidy
  job right after a VS upgrade is expected and means "bump the pin".
- The `/Zc:` conformance flags in the compile database are cl-only; clang
  warns `unused-command-line-argument`, which `WarningsAsErrors: '*'`
  promotes. `run_tidy.py` passes `--extra-arg=/clang:-Qunused-arguments`.
- Wildcard check groups grow new checks across LLVM releases
  (`portability-avoid-pragma-once` appeared in 21 and needed a policy
  decision), so an unpinned tidy plus `WarningsAsErrors: '*'` breaks CI on
  toolchain updates, not code changes.

Local run: `pip install clang-tidy==21.1.6`, then `python
tools/ci/run_tidy.py` (or `clang-tidy -p build --quiet <file>` for one
file).

## Running everything locally

```
pip install clang-format==19.1.7 libclang==18.1.1
python tools/ci/check_file_length.py
python tools/ci/check_no_comments.py
python tools/ci/check_gui_isolation.py
git ls-files '*.cpp' '*.h' '*.hpp' | xargs clang-format --dry-run --Werror
```

The DLL-dependent pixel leg runs separately (never hosted):
`python tools/local/render_regression.py` - see docs/local_regression.md.

## The 2D blend equations are unit tested, pixel exact, with no GPU

`GcAnim::FactorsFor(Blend)` and `GcAnim::TexelDiscarded(alpha)` are the single
description of the 2D pipeline's per-pixel behaviour. `Gc2d::Renderer` translates
them into D3D9 render states, and `gcanim_tests` composites with them on the CPU,
so a blend change is checked against exact expected pixels without a device, a
window, or any game data.

The regression that test exists for: a logo cell drawn over a background must not
punch a black rectangle through it. A fully transparent texel has to leave the
destination byte-identical. That failed the moment the replace blend shipped
without the alpha test, and it broke the IIDX 17 SIRIUS title screen.

Build the synthetic package in the test rather than reaching for real game files:
a package is a few `SysIdx::Cell` and `SysIdx::Record` values, and a test that
needs a game install cannot run in CI.
