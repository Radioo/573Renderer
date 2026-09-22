# r573_afp_host (modern avs/afp host code)

The library the IFS editor's preview host is built on: everything needed to
boot a modern avs2-core / afp-core / afp-utils build, mount content, load
packages, drive an animation and draw it through the D3D9 callbacks, with no
GUI, `App` state or window code. The renderer's `r573_app` links it.

| Area | Files |
|---|---|
| AVS boot, mounts, property XML | `avs_boot.cpp`, `avs_xml.cpp` |
| AFP boot and callbacks table | `afp_boot.cpp`, `backend/afp_profiles.cpp`, `game_profile.cpp` |
| Packages and companions | `afp_packages.cpp`, `afp_package_id.cpp`, `ifs_inspect.cpp` |
| Animation, labels, playhead | `afp_anim.cpp`, `mc_control.cpp`, `render_seh.cpp` |
| D3D9 draw path | `afp_d3d9.cpp`, `afp_d3d9_callbacks.cpp`, `afp_d3d9_commands.cpp`, `afp_d3d9_textures.cpp`, `render_executor.cpp`, `render_backend.cpp`, `warp_device.cpp`, `backend/afp_shaders.cpp` |

It links `r573_support`, `r573_formats`, `r573_state` (for the IFS catalog
types `ifs_inspect.h` uses), `r573_render` and `d3d9`.

## The standalone link gate

`local_dll_tests` links the library with `/WHOLEARCHIVE`, so every object in
it is linked even when no test calls it. A file that starts depending on
`r573_app` (the GUI, `App::Global()`, export, the backends) fails that link,
and `tools/checks.sh` builds every target, so the dependency is caught before
it lands.

## Process globals that remain

The library still reads two process-wide objects, which is enough for a host
process that owns one engine:

- `g_engine` (`app_globals.h`): read by the accessors in `afp_anim.cpp`
  (`IsBooted`, `StreamId`, `PackageId`, `AnimName`,
  `LastCompanionMountPoint`, and the `const AfpFuncs&` probes that act on the
  current stream), `AfpManager::SetActiveConfig` and the stream sweep in
  `UnloadPackages`. Moving them to `EngineSession&` touches about 150 call
  sites across the backends, runtimes, qpro and tests; do it when two
  sessions in one process are needed.
- `g_gpu` (`gpu_context.h`): the texture and draw callbacks afp-core and
  afp-utils call are plain function pointers, so they reach the GPU context
  through this object.

## Shared frames (`shared_frame.h`)

`SharedFrame::Create` makes a D3D9Ex render target texture of the viewport
size with a shared handle, which another process opens (D3D9Ex
`CreateTexture` with the handle; D3D11 `OpenSharedResource` should also work but is untested). It needs a
D3D9Ex device; `D3D9State::Init` creates one whenever `Direct3DCreate9Ex`
works. A resize is a new `Create`, which hands out a new handle.

`SharedFrame::Copy` copies the offscreen render target into the texture and
returns only once the GPU has run the copy, so the reader never sees a
half-drawn frame. It waits by copying one pixel of the shared texture into a
lockable 1x1 render target and locking it. A `D3DQUERYTYPE_EVENT` query was
tried first: on the development machine its `GetData` kept returning
`S_FALSE` for five seconds after the copy, with or without a scene or a
`Present` in between. Why is not known.

Test: `tests/local/shared_frame_tests.cpp` (label `local`, needs a D3D9Ex
adapter, no game data). Without the wait, the second device read zeros.

## IFS bytes from memory (`AvsManager::MountMemoryIfs`)

avs2-core's `ramfs` driver does not store files: a mount maps memory the
host owns and shows it as one read-only file, `image.bin`. So an IFS held in
memory mounts in two steps, and `imagefs` does the rest as it does for a file
on disk:

1. Register the driver once per boot: `avs_fs_addfs(avs_filesys_ramfs())`
   (boot registers `imagefs`, `mirrorfs`, `cryptfs` and `nvram2`, not
   `ramfs`; registering again only bumps a reference count).
2. `avs_fs_mount(<ramfs point>, "", "ramfs", "base=0x<addr>,size=<len>,mode=ro")`.
   fsroot is ignored but must not be null; numbers parse in base 0; `size`
   is compared as a signed int, so at most `0x7FFFFFFF` bytes.
3. `avs_fs_mount(<mountpoint>, "<ramfs point>/image.bin", "imagefs", nullptr)`.

`UnmountMemoryIfs` unmounts `imagefs` first, because it keeps its source file
open and that makes the `ramfs` mount busy, then `ramfs`. The bytes in
`MemoryIfs` must outlive both mounts. Mounting on a path that is still
mounted fails, so a reload unmounts before it mounts the new bytes. The
`ramfs` getter is only in the 2.17 ordinal table (`XCgsqzn0000159`); other
builds leave `avs_filesys_ramfs` null and `MountMemoryIfs` returns false.

Finders in avs2-core: every filesystem driver descriptor holds the magic
`0xA94BEE7C`; the ramfs one sits next to `vfs-driver-ramfs.c`, and its mount
option parser names `base`, `size` and `mode`.

Test: `tests/local/memory_ifs_mount_tests.cpp` (`local_dll`) mounts
`data/graphic/02005.ifs` from memory, reads its `magic` file through
avs2-core, unmounts, then mounts a copy our IFS writer rebuilt with a
different `magic` and reads the new bytes.

## Loading and reloading a package from memory

`AfpManager::LoadPackageFromMemory(es, ifs, package, animation)` mounts the
bytes with `MountMemoryIfs`, reads the package with
`afpu_ngp_read_local(package, mountpoint, 0)` (which copies every file into
afp-utils' heap), queues the texture list's atlas filters, unmounts again (so
the bytes may be freed right away), opens the package's streams and plays the
animation.

`ReloadPackageFromMemory` keeps the playhead: it reads the root clip's current
frame, runs `UnloadPackages` (companions, then the scene streams, then
`afpu_package_control(6, pkg_id)`, then the scene textures), loads the new
bytes under the same package name and seeks back to that frame. The order
matters because afp-utils looks a package up by name before reading it: a
package that is still registered is linked again instead of read, so a reload
that skipped the destroy would keep showing the old content.

Package ids carry a generation counter, so an id kept from before a reload no
longer resolves. Whether afp-core drops global export and class names when
the data is destroyed has not been traced; a reload that renames exports may
leave stale names behind.

Test: `tests/local/afp_reload_tests.cpp` (`local_dll`) boots IIDX 33's engine
on a hidden window, loads `graphic/1/title.ifs` from memory, then reloads a
copy rebuilt with `AfpAnimation` and `Ifs::Write` whose `loop` label moved
and was renamed, and back, three times. It checks the labels and the playhead
through afp-core and that the loaded package count stays the same.

## Loading the engine DLLs (`engine_dlls.h`)

`EngineDlls::DiscoverDllDir` finds the directory holding the config's avs,
afp and afp-utils DLLs (`modules/`, `contents/modules/`, then the install
root). `EngineDlls::Load` loads them into an `EngineSession` and resolves the
avs ordinal table for the config's avs generation. Both the renderer's AFP
family backend and the preview host use them.
