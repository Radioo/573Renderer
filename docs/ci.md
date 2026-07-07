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
`docs/build.md`). The `ci` preset additionally sets
`CMAKE_COMPILE_WARNING_AS_ERROR=ON`. The build dir is `build/` at the repo
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

## Local-only tiers

Tests that need the proprietary game DLLs or real game data can never run
hosted. They are excluded by CTest label (`ctest -LE local_dll` in CI); the
DLL-dependent tiers run manually on the owner machine.
