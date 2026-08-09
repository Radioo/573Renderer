# CI (.github/workflows/build-renderer.yml)

## Purpose and triggers

One workflow builds the renderer on `windows-2022` (MSVC 2022, Ninja, CMake,
pwsh preinstalled):

- push to `master` / PRs to `master`: verify it builds; upload the exe as an
  artifact so a reviewer can try a PR without building locally (30-day
  retention - long enough for review, short enough for the free Actions
  storage quota).
- tag push matching `v*` (e.g. `v0.2.0`): additionally attach exe + PDB to a
  GitHub Release with auto-generated notes (needs `contents: write`
  permission and `fetch-depth: 0` for the release-notes generator; PR runs
  stay read-only).
- `workflow_dispatch`: on-demand rebuilds.

A `concurrency` group cancels in-flight builds of the same ref when a new
commit lands; a stale build never gives useful information.

The configure/build steps run `cmake --preset ci` / `cmake --build --preset
ci`, so CI uses exactly the knobs in `CMakePresets.json` (same generator,
build type, triplet, toolchain, overlay ports as local builds - see
`docs/build.md`). `CMAKE_COMPILE_WARNING_AS_ERROR=ON` lives in the BASE
preset, so dev and ci compile with identical flags. It used to be ci-only,
which let a benign-looking C4324 (struct padding from a needless
`alignas(16)` in gpu_context.h) print in every local build while failing
only in CI; the local gate must never be laxer than CI. If a preset ever
diverges again, the divergence itself is the bug. The build dir is `build/` at the repo
root, same as local, which simplifies reproducing CI failures.

## Caching (the entire design of this workflow)

The heaviest CI cost by far is vcpkg compiling ffmpeg with aom + libvpx +
x265 + nvcodec from source: ~20-40 min on a 2-core runner. Everything is
structured to pay that only when the dependency set actually changes.

1. **vcpkg binary cache** - the big one. `VCPKG_BINARY_SOURCES` uses vcpkg's
   `files` backend pointed at `${{ github.workspace }}/vcpkg_cache`, persisted
   with `actions/cache@v4`. Each built package is a zip keyed internally by
   its full ABI hash (port + triplet + features + compiler + vcpkg tool
   version), so a restored cache is always safe: vcpkg rebuilds only archives
   whose ABI moved. The actions/cache key mixes the resolved vcpkg submodule
   commit + `hashFiles(vcpkg.json, vcpkg-configuration.json)`; `restore-keys`
   allow warm-starting from the newest prior cache when a manifest changes,
   after which vcpkg saves a fresh entry under the new exact key.

   History: vcpkg's built-in `x-gha` backend (GitHub Actions Cache API) was
   REMOVED upstream and silently became a no-op, which made every run rebuild
   ffmpeg/aom/libvpx from source. The `files` + `actions/cache` pairing is
   the supported replacement and needs no NuGet/PAT setup. Do not migrate
   back to anything x-gha-shaped.

2. **vcpkg.exe bootstrap cache**: building vcpkg itself costs ~60 s. Cached
   with a key of OS + bootstrap script hashes + the resolved submodule
   commit. The commit must be resolved via `git -C vendor/vcpkg rev-parse
   HEAD` (the "Resolve vcpkg revision" step): `.gitmodules` does NOT change
   on a pin bump, so hashing it would silently reuse stale caches forever.

3. **Renderer TUs are not cached**: they compile in well under a minute and
   the link is fast; a ccache layer would add complexity without meaningful
   savings.

## Other steps

- `ilammy/msvc-dev-cmd@v1` puts the x64 `cl.exe`/`link.exe` on PATH (the CI
  equivalent of build.bat's `vcvarsall.bat x64`); it edits `GITHUB_ENV` so
  later steps inherit the toolchain without re-running vcvars.
- "Print toolchain versions" exists purely for debug-by-log when a cache key
  goes wrong and a cold build appears unexpectedly.
- "Verify build output" fails loudly if `bin/573Renderer.exe` is missing or
  suspiciously small (< 1 MB = the link probably failed);
  `actions/upload-artifact` would otherwise silently upload an empty
  directory.

## clang-tidy legs

The x64 matrix leg runs the whole-tree tidy gate on every trigger; the x86
leg runs it only on pushes (matrix key `tidy: always | push`). x86 gets its
own `.tidycache` cache (key includes the arch) and points `run_tidy.py` at
`build32/` via the `TIDY_BUILD_DIR` env override. The x86 leg exists
because 32-bit compilation has genuinely different diagnostics (pointer
truncation, `corecrt_malloc.h` attribution - docs/x86_build.md records an
x86-only `.clang-tidy` fix found by hand before this leg existed); running
it on push-only keeps PR latency down.

## ASan job

A third job (`asan`) configures the `asan` preset (`R573_ASAN=ON`, which
adds `/fsanitize=address` + `/INCREMENTAL:NO` on top of RelWithDebInfo into
`build-asan/`) and builds + runs ONLY the eight dependency-light test
suites: formats, loop, render, state, settings, cli, support, media_format.
These exercise the binary-input decoders (DXT/LZSS/Blowfish/AES/TXP2/
sysidx/gcz/inz/xfile) where a heap overflow would hide, and none of them
link FFmpeg or D3D9, so the job shares the vcpkg binary cache but never
pays an ffmpeg rebuild. The suites are invoked as bare Catch2 exes rather
than through ctest labels: Catch's POST_BUILD discovery flattens a
multi-label list (`LABELS "ci;pure"` arrives as two property tokens), so a
second label cannot be attached reliably. The `/MT` static CRT makes MSVC
ASan link the static `clang_rt.asan` - no runtime DLL deployment problem.
The ASan config also defines `_DISABLE_STRING_ANNOTATION` /
`_DISABLE_VECTOR_ANNOTATION` / `_DISABLE_OPTIONAL_ANNOTATION`: vcpkg's
Catch2 is built WITHOUT ASan, and the MSVC STL hard-fails the link
(LNK2038 `annotate_string` mismatch) when instrumented and
non-instrumented objects disagree on container annotations. Disabling
annotations keeps full heap-overflow/UAF checking and only gives up
container-overflow precision inside STL containers; the alternative (an
instrumented triplet) would rebuild every dependency including ffmpeg.
UBSan is deliberately absent: MSVC has no `/fsanitize=undefined`, and a
clang-cl leg would need a second Catch2 triplet.

## GUI tests in the hosted matrix

`gui_tests` carries the `ci` label like every other suite, so the existing
`ctest -LE local_dll` step picks it up with no workflow change. It needs no
display, no D3D9 device and no game data: it drives the real panels through the
Dear ImGui Test Engine against a null backend (docs/gui_tests.md). 117 cases,
about 6 seconds - the bulk of the `ctest -L ci` wall time, and still cheap
enough that the local gate stays under ten seconds. It is
deliberately absent from the ASan job's target list, which stays restricted to
the dependency-light pure-logic suites - `gui_tests` links `r573_app`, and with
it FFmpeg and D3D9, so adding it there would trade a large rebuild for coverage
of code that is not doing raw buffer decoding.

## Local-only tiers

Tests that need the proprietary game DLLs or real game data can never run
hosted. They are excluded by CTest label (`ctest -LE local_dll` in CI); the
DLL-dependent tiers run manually on the owner machine.

## Local aggregate gate: tools/checks.sh

`bash tools/checks.sh` is the one-command local equivalent of every hosted
gate and is the required exit criterion for any refactor slice: it runs
build.bat (dev preset), `ctest -L ci` (locating ctest.exe next to the
cmake.exe recorded in build/CMakeCache.txt, since the VS-bundled toolchain
is not on the Git Bash PATH), then the seven gate scripts
(check_file_length, check_no_comments, check_banned_chars,
check_machine_paths, check_gui_isolation, run_format, run_tidy), and prints
`ALL CHECKS PASSED` only if every step succeeded.
run_tidy needs the pip-pinned clang-tidy (see docs/tidy_migration.md) and
the build dir's compile_commands.json, which the build step guarantees.

Gate latency is a design requirement: an incremental `checks.sh` run is
seconds, not minutes.

- check_no_comments lexes C++ with a regex tokenizer (raw strings, string
  and char literals consumed first, then `//` and `/* */` tokens) instead
  of a libclang parse. The old libclang version parsed every TU and took
  minutes for the whole tree; the tokenizer does all files in well under a
  second and ignores comment-lookalikes inside strings (`https://...`,
  raw-string bodies). Digit separators (`1'000'000`) are consumed by the
  char-literal branch, which cannot produce a false comment.
- build.bat skips `cmake --preset dev` when `build/CMakeCache.txt` exists:
  ninja re-runs CMake itself when configure inputs change (CMakeLists,
  presets, and vcpkg.json via the toolchain's CMAKE_CONFIGURE_DEPENDS), so
  the explicit configure only pays off on a fresh build dir.
- run_tidy analyses changed files only (content-hash cache); run_format
  checks the whole tree but clang-format is fast. ctest -L ci is ~2.5s.
- checks.sh invokes build.bat as `MSYS_NO_PATHCONV=1 cmd.exe /c
  <absolute-windows-path>` (cygpath). The old `//c build.bat` form passed
  `//c` through literally once MSYS_NO_PATHCONV disabled slash conversion,
  so cmd exited 0 WITHOUT running the build and `set -e` never tripped.
  checks.sh therefore also greps the build output for the "Build
  successful" marker and hard-fails if it is absent: a silently skipped
  build must never produce ALL CHECKS PASSED.
- Steady-state full gate: ~5s; with one changed TU: ~7-8s. If it drifts
  back toward minutes, time the stages individually before guessing.
