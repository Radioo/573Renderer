# Build system

## Line endings

`.gitattributes` pins the working tree to LF for everything except `.bat`
files, which check out as CRLF. LF everywhere is safe for a Windows-only
project (MSVC, CMake, Ninja, clang tooling, and modern editors all handle
it) and makes the ubuntu CI gates see byte-identical files. Batch files are
the one real exception: `cmd.exe` has parsing bugs with LF-only files
(`goto`/label scanning can misfire), so CRLF there is a correctness
requirement. The index was already fully LF-normalized when the attributes
were added; the file only removes the dependence on each machine's
`core.autocrlf`. After pulling the commit that adds it, refresh a stale
working tree with: `git rm --cached -r . && git reset --hard`.

## Entry points

| Path | Role |
|---|---|
| `build.bat` | Local developer build: locates VS via vswhere, runs `vcvarsall x64`, bootstraps the vcpkg submodule if needed, then `cmake --preset dev` + `cmake --build --preset dev`. |
| `CMakePresets.json` | Single source of truth for configure/build knobs. `dev` = local, `ci` = same plus `CMAKE_COMPILE_WARNING_AS_ERROR=ON`. CI and build.bat both go through presets so the two can never drift. |
| `CMakeLists.txt` | One executable target (`renderer`, output name `573Renderer.exe`) plus the `r573::warnings` interface target. |

## Why the CMake project is `Renderer573` but the exe is `573Renderer.exe`

CMake does not allow project/target names to start with a digit on every
generator. The project and target keep alpha-prefixed internal names; the
user-facing name is applied via the `OUTPUT_NAME` target property. The exe is
copied to `bin/` (`RUNTIME_OUTPUT_DIRECTORY`) so it sits next to nothing it
depends on and is easy to find in Task Manager / shortcuts.

## Toolchain and CRT

- x64 is mandatory (`CMAKE_SIZEOF_VOID_P` check): the Konami game DLLs the
  renderer loads are 64-bit.
- vcpkg triplet is `x64-windows-static`: every dependency links statically so
  `573Renderer.exe` ships as a single file with no runtime DLLs beside
  Windows system libraries.
- That triplet builds dependencies with the static CRT (`/MT`), so the
  renderer's own objects must match: `CMAKE_MSVC_RUNTIME_LIBRARY` is forced to
  `MultiThreaded$<$<CONFIG:Debug>:Debug>`. Without this, MSVC defaults to
  `/MD`; C dependencies (ffmpeg, aom) silently tolerate the mismatch but a
  C++ static dependency (x265, added for HEVC-alpha) fails the link with
  LNK2038 RuntimeLibrary errors. Switching the triplet later invalidates the
  entire vcpkg binary cache for it - every dependency rebuilds from scratch.

## Dependencies (vcpkg manifest mode)

Declared in `vcpkg.json`; resolved on first configure via the vcpkg toolchain
file that the presets point at (`vendor/vcpkg` submodule, pinned baseline in
`vcpkg-configuration.json`). No in-tree source builds of imgui/ffmpeg - vcpkg
owns them.

- `imgui` with `dx9-binding` + `win32-binding` features baked into the vcpkg
  build; exposes the `imgui::imgui` CMake target (headers for
  `imgui_impl_dx9.h` / `imgui_impl_win32.h` are part of its include tree).
- `ffmpeg` via vcpkg's custom `FindFFMPEG.cmake` (module mode, not config
  mode): populates `FFMPEG_LIBRARIES` / `FFMPEG_INCLUDE_DIRS` for the
  components requested. Because this is variable-style (not an IMPORTED
  target), its include dirs are consumed with
  `target_include_directories(... SYSTEM ...)` explicitly - that is what lets
  `/external:W0` silence third-party headers once the warnings target is
  linked.
- `vcpkg-overlays/` overrides registry ports. It ships an x265 overlay with
  `-DENABLE_ALPHA=ON` so ffmpeg's libx265 wrapper can emit HEVC-with-alpha
  (Safari-compatible transparent video); stock vcpkg x265 builds alpha OFF.
  The overlay path is a preset cache variable (`VCPKG_OVERLAY_PORTS`), so
  local and CI builds always apply it. Before presets, only build.bat set it
  via environment variable and CI silently built x265 without alpha.

## Windows link libraries

Link order: our objects, then vcpkg static libs, then Windows system libs.
ffmpeg's static libs pull in Win32 dependencies that must be linked
explicitly: `bcrypt` (crypto helpers), `ws2_32`/`secur32` (network code that
is feature-disabled but still referenced), `mfuuid`/`strmiids` (DirectShow
GUIDs). `windowscodecs` + `ole32` are for WIC PNG decode/encode - the vcpkg
ffmpeg feature set has no PNG codec; media_sink uses WIC instead. All are
always present on Windows; the linker strips unused references.

## The warnings target (`r573::warnings`)

`r573_warnings` (alias `r573::warnings`) is an INTERFACE target carrying the
project's full warning policy. It is the ratchet mechanism from the
refactoring plan: it starts linked into nothing, and every module that
migrates to the new structure links it, permanently opting into the policy.
It is never applied via `CMAKE_CXX_FLAGS` or directory-level commands.

Flag set (MSVC, behind a `$<CXX_COMPILER_ID:MSVC>` generator expression):

- `/W4`, never `/Wall`: /Wall enables informational noise (C4820 padding,
  C4514/C4710 inlining, C5045 Spectre) that even the MSVC STL does not pass.
- Curated off-by-default warnings `/w1XXXX`: lossy conversions
  (4242/4254/4311/4826), virtual-function traps (4263/4265), no-effect
  expressions (4545-4549/4555), thread-unsafe local statics (4640),
  string-literal casts relevant to Win32/D3D9 interop (4905/4906),
  unhandled enum in switch (4062) and implicit fallthrough (5262) which
  matter in per-game dispatch code, bogus pragma warning numbers (4619),
  4287/4296/4928, and 4289 promoted to error (`/we4289`).
- Conformance: `/permissive-` plus the `/Zc` switches it does not imply:
  `preprocessor` (conforming preprocessor, matches clang-cl),
  `__cplusplus` (real C++20 value visible to libraries), `enumTypes`
  (correct underlying enum types; enums crossing the game-DLL ABI should
  still declare explicit underlying types since clang-cl emulates the old
  behavior), `templateScope`, `throwingNew`, `externConstexpr`, `inline`.
- `/utf-8` for deterministic source/execution charsets. This one is also
  applied globally to the `renderer` target already: without it MSVC reads
  sources in the machine's active code page, which made local builds on a
  cp932 system emit C4819 for UTF-8 characters in comments while CI's cp1252
  runner read the same files differently.
- `/external:W0`: third-party headers reached through SYSTEM include dirs
  (vcpkg IMPORTED targets, the explicit ffmpeg SYSTEM includes) produce no
  diagnostics.

Warnings-as-errors is not part of the flag set: the `ci` preset turns it on
via `CMAKE_COMPILE_WARNING_AS_ERROR=ON` (CMake >= 3.24), which keeps the
built-in `--compile-no-warning-as-error` escape hatch for local refactoring
while CI stays absolute.

## compile_commands.json

All presets set `CMAKE_EXPORT_COMPILE_COMMANDS=ON` (works because the
generator is Ninja; Visual Studio generators silently ignore it). The
database at `build/compile_commands.json` feeds clang-tidy and the future
lint tooling.
