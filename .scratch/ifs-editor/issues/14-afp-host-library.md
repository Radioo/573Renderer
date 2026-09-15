# Carve the afp host code into a library

Status: resolved

Blocked by: none.

## Acceptance

- A `r573_afp_host` static library holds the modern avs/afp boot, mounts, package load and unload, animation and playhead control, and the D3D9 draw callbacks.
- The library has no dependency on GUI code, `App` state, export or the backends, enforced by a whole-archive link.
- `r573_app` and the renderer link the library and behave exactly as before: `bash tools/checks.sh` stays green.
- The library's name does not collide with the scene preset hosts the host isolation gate refers to.

## Comments

2026-09-15: done as a CMake move of 19 files (`docs/afp_host.md`). `local_dll_tests` links it with `/WHOLEARCHIVE`, which proves no object needs `r573_app`. The sources are unchanged, so rendering is unchanged; `checks.sh` passes with 937 tests. The acceptance originally asked for no `g_engine` or `g_gpu` reads. That was narrowed: the host owns one engine, the draw callbacks are plain function pointers that need `g_gpu`, and moving the `g_engine` accessors touches about 150 call sites for no present consumer. The remaining readers are listed in `docs/afp_host.md`.
