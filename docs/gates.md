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
- The machine-path gate has no baseline: it runs over every tracked file
  from day one, since any hit is a violation regardless of migration state.
- The strict MSVC warning set (`r573::warnings`, /W4 + curated /w14xxx) is
  linked into EVERY target as of P9, including the `renderer` monolith
  executable that held the un-migrated sources. Reaching that meant driving
  the monolith warning-clean: dead statics / unused params / dead locals
  removed, and every `getenv`/`fopen`/`sscanf`/`strncpy` (C4996 "unsafe
  function") routed through a safe path. There is no `/WX`, so a new warning
  does not fail the build by itself - but the codebase is at zero warnings,
  and clang-tidy (whole-tree scope at P9) fails on regressions.

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

Comment detection is lexer-based, never regex-over-lines for C++: libclang
(`clang.cindex`) tokenizes exactly like the compiler, so `//` inside string
literals (URLs, Windows paths) cannot false-positive. Python uses the stdlib
`tokenize` module. YAML/CMake/gitignore use a quote-aware `#` scan, batch
files match `rem`/`::` statements, JSON matches `//`-style lines (real JSON
cannot carry comments; the checker exists so JSONC never sneaks in).
Markdown under `docs/` is exempt by nature - prose is the point.

### GUI isolation (`check_gui_isolation.py`)

The P8 shell rule made checkable: `ImGui::` / `ImGuiIO` / `#include <imgui...`
may appear ONLY under `src/gui/`. Everything outside the gui module talks to
the UI through App::State (requests, view state) - business logic never
renders widgets. The gate scans tracked + untracked `src/**.cpp|.h` excluding
`src/gui/`; the codebase was already 100% clean when the gate was
introduced, so it starts with no baseline and any hit fails CI.

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
`HeaderFilterRegex`.

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

The DLL-dependent pixel tier runs separately (never hosted):
`python tools/local/render_regression.py` - see docs/local_regression.md.
