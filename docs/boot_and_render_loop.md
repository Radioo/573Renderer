# Boot + render loop: RE facts and design rationale

Knowledge captured from comments in: `src/main.cpp`, `src/boot.{h,cpp}`,
`src/avs_boot.{h,cpp}`, `src/afp_boot.{h,cpp}`, `src/afp_boot_internal.h`,
`src/render_loop.{h,cpp}`, `src/render_seh.{h,cpp}`, `src/render_live.{h,cpp}`,
`src/window.{h,cpp}`. This is the canonical home for the RE facts those
comments carried; the code will eventually carry no comments at all.

---

## 1. AVS boot (AvsManager)

Boot order requirement: AVS must be booted before ANY `avs_fs_*` call.

- Heap: 128 MB `malloc`'d block (the game itself uses `0x8000000` = 128 MB).
- Config property tree: `property_create(7, stack_buf, 0xD18)`, then
  `property_node_create` for:
  - `/config/fs/root/device` (type 0xB, string) = CWD with forward slashes
  - `/config/fs/nr_mountpoint` (type 0x5, int) = 256
  - `/config/fs/nr_filedesc` (type 0x5, int) = 4096
  - `/config/thread/nr_mutex` (type 0x5, int) = 256
  - `/config/log/level` (type 0xB, string) = "misc"
  then `property_search(prop, nullptr, "/config")` and
  `avs_boot(config_node, heap, size, nullptr, log_writer, nullptr)`.
  If `property_create` fails, `avs_boot` is called with a NULL config and
  still works.
- Log writer callback: AVS routes afp/afpu log output AND asserts through the
  registered writer. The renderer mirrors it to a flushed file (`avs_out.log`,
  append across boot phases) because an assert ends in `__debugbreak` and kills
  the process before any console can be read; the file is the only way to see
  the final message.
- `avs_fs_dump_mountpoint` is avs2-core ordinal 0x068 (used as a diagnostic
  when a mount fails).

### Mount patterns

- Standard IFS mount (modern games): mount the directory containing the .ifs
  as `avs_fs_mount("/data", <dir>, "fs", "vf=0,posix=1")`, then mount the IFS
  itself as `avs_fs_mount("/afp/packages", "/data/<name>.ifs", "imagefs",
  nullptr)`. Fallback if that fails: mount the .ifs directly at `/data` as
  imagefs.
- `MountFsRoot` = the "bring the on-disk data root into the VFS" step
  (`"fs"`, `"vf=0,posix=1"`); must precede `MountIfsImage`, whose
  `vfs_ifs_path` must resolve against the VFS (e.g. `/data/graphic/1/common.ifs`
  once `/data` is bound). This matches bm2dx's per-IFS mount pattern.
- Paths handed to AVS are absolute host paths converted to forward slashes.

---

## 2. AFP boot sequence (AfpManager::Boot) and per-profile gates

`SetActiveProfile` must be called BEFORE `Boot`; a null profile means "all
calls enabled" = IIDX 33 default behaviour. The whole boot sequence is derived
from bm2dx (IIDX 33) and then per-game gated where live IAT traces showed the
other games diverge.

### 2.1 Render context (the afp_boot argument)

`render_ctx.Flags() = 0x200`. Callback slots installed (byte offsets into the
context):

| offset | callback | notes |
|---|---|---|
| 0x008 | BeginRender | |
| 0x010 | EndRender | |
| 0x018 | SetMaskRegion | |
| 0x020 | SetLayer | |
| 0x028 | DrawPrimitive | |
| 0x030 | SetBlend | |
| 0x038 | SubmitGeometry | the draw_vg path |
| 0x040 | LayerCommand | NOT a "SubmitGeometry fallback". It is the ONLY slot that passes AFPU a D3D device (via case 4's vtable methods), so package-atlas textures bind HERE, not through SubmitGeometry. |
| 0x050 | (left NULL) | per-shape mat2d notification. When non-NULL, afp-core's internal draw_vg flow changes (the alternate vertex-transform helpers activate), which alters vertex transformation. Vertices arrive pre-transformed, so this must stay NULL. |
| 0x058 | SetMatrix | |
| 0x060 | GetScreenSize | |
| 0x068 | GetNearFar | |
| 0x078 | (left NULL) | FindTexture. The game leaves it NULL too. |
| 0x118 | NULL | |
| 0x120 | malloc | |
| 0x128 | realloc | |
| 0x130 | free | |

The same `render_ctx` buffer is 128 KB and is reused as the AFPU render-state
ctx (`AfpD3D9::SetStateCtx(&render_ctx)`): during LayerCommand case 4 the
vtable methods on the AFPU effect object receive this pointer as their 2nd
positional arg, and it is large enough to hold a device-pointer alias at
+0x18000.

### 2.2 The two screen-geometry code paths (slot 12/13 vs ctx +0x60/+0x68)

afp-core's per-frame setup (the `afp_do_sort_render` pre-pass) reads SLOT 12 /
SLOT 13 of afp-core's internal callback table to discover the screen rect and
clip planes. afpu itself uses a DIFFERENT path through the render_ctx
callbacks at +0x60 / +0x68. The slot-12/13 callbacks are afp-core internal.
The renderer's stubs forward to the same `AfpD3D9::GetScreenSize` /
`GetNearFar` (one source of truth). History: the stubs used to hardcode
1280x720, which silently broke every non-1280x720 game; SDVX's portrait
1080x1920 render had AFP set up a 1280-wide draw clip / projection inside the
1080x1920 RT (Rasis squeezed into the upper portion, rest left as clear
colour).

### 2.3 Boot call order (with the reasons behind each gate)

1. `afp_ext_command(9, &ver)` = AFP-Core version string.
2. `afp_boot(&render_ctx)`.
3. `afp_set_verbose`: 1-arg on IIDX; 2-arg `(1, 0x10000)` on SDVX
   (`afp_set_verbose_wide_args` profile flag; the 0x10000 is a verbose-bits
   word observed in the live SDVX trace; IIDX's function ignores R8, so
   passing 0 is safe there).
4. bm2dx boot-time `afp_set_flag` triple, sequenced right after verbose:
   `afp_set_flag(16)`, `afp_set_flag(8)`, `afp_set_flag(65537)` (bit 0 +
   bit 16, unknown semantics). Not observed to be required for rendering,
   but matches bm2dx's log-noise / state. bm2dx's decomp shows single-arg
   calls; the typedef here is `(flags, mask)` and IIDX only reads RCX, so
   `(val, 0)` is used. SDVX 7's live trace shows asymmetric `(val, mask)`
   shapes with unknown semantics, so the SDVX profile skips the whole triple
   (`call_afp_set_flag_setup = false`) to avoid corrupting state.
5. AFPU config property: `/config/render/max_nr_masks` (type 0x4) = 16,
   built with `property_create(7, buf, 0xD18)` like the AVS config.
6. AFPU heap/data struct: a 128-byte table whose layout matches bm2dx's
   .rdata copy:
   - +0x00 CreateTexture, +0x08 DestroyTexture, +0x10 UpdateTextureData,
     +0x18 unused (NULL in bm2dx), +0x20 TextureMalloc, +0x28 TextureRealloc,
     +0x30 TextureFree
   - +0x38 float 10000.0 (far plane?), +0x3C float 1.0 (near plane?),
     +0x40 u32 2 (stream-nr category?) - these exact constants sit in bm2dx's
     .rdata copy; they are quality / depth-plane hints AFPU reads during
     stream init.
7. `afpu_boot(afpu_config, afpu_data)` - profile-gated (`call_afpu_boot`).
   The live SDVX trace shows the game does NOT call afpu_boot from its IAT;
   afp_boot likely bootstraps afpu internally on SDVX, so a second explicit
   call corrupts state. SDVX skips it.
8. get_near_far pre-patch (see section 3, "nearfar slot").
9. `afp_render_init` - profile-gated (`call_afp_render_init`). SDVX's game
   binary never calls it; calling it from the renderer corrupted the heap
   state that later broke SDVX's `afp_set_stream_nr`.
10. `afpu_render_init(&render_ctx)` - same gating logic
    (`call_afpu_render_init`).
11. `AfpD3D9::Init` + `SetStateCtx` + `SetAfpuTexSlotResolver(
    afpuloc_get_texture_data_size)`. afp-utils exports are ordinal-obfuscated
    (names like `XE592acd000042`), so the backend cannot GetProcAddress by
    readable name; it needs the already-resolved pointer. SubmitGeometry uses
    it to convert AFPU tex ids (0x08000000-tagged) into the renderer's
    texture slot ids.
12. `SetAfpuSetScreenRectFnOffset(off.afpu_set_screen_rect_fn)`: BeginRender
    calls afpu's set_screen_rect every frame BY OFFSET into afp-utils. The
    IIDX-default offset lands inside SDVX's matrix-transform helper
    and AVs there; per-profile offsets are mandatory.
13. The afp_set_afp_data block (section 3).
14. `afpu_ext_command(2, &ver)` = AFPU version string.
15. `afpu_set_config` AFTER render_init - profile-gated
    (`call_afpu_set_config`):
    - `afpu_set_config(1, 4096)` (cmd buffer size) is REQUIRED: it sets the
      global that afpu's render-buffer allocator (`afpu_render_get_buf`)
      reads as `buffer_capacity = 24 * <that global>`. Skipping it leaves
      capacity 0 and every vertex write overflows the NULL/empty buffer - on
      SDVX this manifests as an AV inside afpu's matrix-transform inner loop
      writing one stride past the buffer end.
    - `afpu_set_config(2, 10)` = quality level.
    - `afpu_set_config(3, clean_pos)` semantics on SDVX
      (`afpu_render_set_clean_position_local`):
      - value 0: defaults - the test-fn slot returns 0, the cleanup slot is
        a no-op stub
      - value 1: installs the standard clean-position cleanup function
        (bm2dx uses 1)
      - value 2: installs the snap-to-half-pixel cleanup
      - value 0x10: installs another function via the `& 0xF0` branch
      The SDVX live trace shows the GAME never calls afpu_set_config at all;
      the real game's afp setup routine does not call `(3, X)`, so the AFPU
      default 0 is what the real game effectively runs. Also, clean_pos=2 would
      not even trigger for our draws: AFPU's clean-position predicate
      requires geo flag bit 0 (2D-position) set, and the booth-tile draws come
      through with flag 0x02 (XYZ-position), bit 0 clear, so the
      snap-to-half-pixel cleanup
      never fires. SDVX profile passes 0 (`afpu_set_config_safe_clean_pos`);
      the value-1 path on SDVX writes through buffer pointers the renderer
      has not sized.
16. `afpu_set_flag(4,4)`, `(8,8)`, `(16,16)` - profile-gated
    (`call_afpu_set_flag_setup`).
17. `afp_set_stream_nr(4096)` - profile-gated (`call_afp_set_stream_nr`) AND
    SEH-wrapped. Konami's per-game afp-core builds agree on export
    prefix/count but NOT on the ordinal-to-function mapping: IIDX 33's
    ordinal 0x01D is afp_set_stream_nr; SDVX 7 NABLA's 0x01D is something
    else entirely and calling it with 4096 AV'd mid-boot. The SEH frame is
    belt-and-braces for a profile that wrongly enables the call - log and
    survive rather than tank the boot.
18. Test `afp_stream_create()` + `afp_stream_destroy(5, id, 0)` probe -
    profile-gated (`call_afp_stream_create_test`). This is the real "did
    afp+afpu wire up" check, but on SDVX 7 ordinal 0x018 takes 3 args a
    0-arg call site cannot supply, so calling it AVs before the GUI IFS
    picker even appears. The test is diagnostic-only; skipping it is
    correctness-neutral. Success semantics: id != 0xFFFFFFFC and >= 0.
    Destroy type 5 = by-id, immediate=0.

GITADORA special case (`skip_explicit_afp_set_afp_data`, afp-core 2.14.26):
rely on afpu_render_init's INTERNAL afp_set_afp_data (the rebind path, 0x800
flag left SET) and skip both the renderer's 0x800-clear and its explicit
memcpy-path afp_set_afp_data, which otherwise clobbers the per-shape
transform handoff. The slot 12/13 re-patch still runs (it fixes afpu's
installed table).

---

## 3. DLL data-segment patches (VirtualProtect writes into afp-core)

All gated by `apply_iidx_data_segment_patches` (the "emergency off switch"
for un-RE'd games; per-profile offsets normally make it safe).

- **nearfar slot pre-patch**: a function-pointer slot inside afp-core data
  (`DllOffsets::AfpCore::kNearFarSlot`).
  This offset is IIDX-33-SPECIFIC: on SDVX 7's afp-core build the same
  address holds unrelated data, and writing through it corrupts whatever
  lives there (it broke `afp_set_stream_nr` downstream). Offset now comes
  from the profile's DllOffsetSet; SDVX skips the patch entirely.
- **0x800 render-flag clear**: clear bit 0x800 in afp-core's render-flags
  dword (`off.afp_render_flags`) to force `afp_set_afp_data` onto the simple
  memcpy path (as opposed to the rebind path). GITADORA leaves 0x800 SET
  (see above).
- **afp_set_afp_data call**: afp-core export ordinal 0x000. Argument shapes
  differ per game:
  - IIDX 33: reads only RCX; bm2dx's own call site is 1-arg.
    `set_data(afpu_data_ptr, 0, 0, nullptr)`.
  - SDVX 7 (live trace): 4-arg `(table_ptr, 0, 0x320, heap_ctx)`. The 0x320
    is the size of afp-utils' built-in callback table; passing register junk
    in R8 makes SDVX's function corrupt state that `afp_set_stream_nr` later
    trips over. (`afp_set_afp_data_wide_args` profile flag; the renderer
    passes `&render_ctx` as the heap ctx.)
  - The afpu-side data-struct offset (the RCX argument) is per-profile.
- **slot 12/13 re-patch AFTER afp_set_afp_data**: `afp_set_afp_data` installs
  afpu's default callbacks for ALL 35 slots (a memcpy from staging into the
  callback table), which OBLITERATES the earlier nearfar patch.
  So the stubs must be re-installed AFTER it:
  - slot 12 = get_screen_size: afpu's default queries
    the host window size, which is not set up the way SDVX expects; afp's
    `afp_do_sort_render` pre-pass calls it, gets zeros,
    hits an assert + `__debugbreak`, and AVs the renderer.
  - slot 13 = get_near_far: afpu's default returns
    equal near/far, tripping another assert.
  The callback table base is per-profile (`off.afp_callback_table`); a
  diagnostic dump of slots 0..7 (as afpu-relative offsets) runs right after
  set_afp_data.

---

## 4. Boot orchestration (BootFromGameDir) and IFS load path

### 4.1 BootFromGameDir step order

1. Resolve GameProfile: explicit slug (request / settings.ini) beats
   auto-detect from the directory path, beats built-in default (IIDX 33).
   `g_ddr_mode = profile->legacy_afp` routes everything to the DDR
   (legacy AFP 2.13.7) path; it is mirrored into App::State
   (`SetIsDdrMode`) BEFORE the Ready flip so GUI TUs read it correctly.
2. Discover DLL dir. Candidates in order: `<game>/modules`,
   `<game>/contents/modules`, `<game>` itself (portable layouts). All three
   of avs/afp/afpu DLLs must be present for a candidate to qualify (a
   partial hit would just fail later). Returned WITH trailing separator for
   string-concat loading.
3. Load + resolve DLLs. `SetDllDirectoryA(modules_dir)` first so dependent
   imports resolve: DDR's libafp imports libavs + d3dx9_43; modern afp-core
   imports avs2-core. avs is loaded first regardless, but d3dx9_43 may live
   in the modules dir. DDR profile: the modern (ordinal) afp/afpu func
   tables do NOT apply to legacy AFP 2.13.7 - `g_afp`/`g_afpu` are left
   null INTENTIONALLY (DdrAfp::Boot resolves its own by-name tables); the
   null tables are what make the modern per-frame blocks in the render loop
   self-skip.
4. AVS boot (must precede any avs_fs call).
5. Render window + D3D9 - created here (not up front) so the setup screen
   has no black 1920x1080 window behind it before a dir is picked. Sizes
   floored at 1 to protect D3D9::Init. DDR override: DDR's afp emits
   screen-space vertices authored at 1280x720 (the bg fill is the quad
   (0,0)-(1280,720)), so the window is pinned to 1280x720 for a 1:1 fill;
   arbitrary-resolution DDR is a follow-up. After init, the offscreen RT
   size is published to the window module (`AppWindow::SetRenderRtSize`)
   for crop-pick mouse translation.
6. AFP boot: modern `AfpManager::Boot` OR `DdrAfp::Boot` (DDR). DDR then
   gets `DdrAfp::SetTimeScale(profile->time_scale)` - the game-speed
   calibration for the still-unresolved ~6% rate divergence
   (see GameProfile::time_scale).
7. Optional persistent boot IFSes (`--boot-ifses`, off by default):
   LoadBootIfses appends `graphic/1/<file>.ifs` to the data root, so it is
   passed `<game>/data` (parent of `graphic/`), falling back to `<game>` for
   non-standard layouts. These mirror bm2dx's priority-0 boot mounts
   (common.ifs, gameparts.ifs, graph.ifs, ...); packages loaded here are
   tracked and never touched by UnloadPackages - they persist until
   Shutdown.
8. Persist settings + flip BootState to Ready BEFORE scanning for IFSes.
   The scan walks the entire install (~40k files on a full SDVX dir; IIDX
   6000+ IFSes) and on a cold disk cache used to block the boot for minutes
   of "Booting...". It feeds only the GUI browser, so it runs on a detached
   background thread. ScanGameDir is pure filesystem (no AVS/AFP calls) so
   it is safe off the render thread; App::Global is a leak-forever
   singleton, so the detached thread outliving BootFromGameDir is fine
   UNDER the first-boot-terminal model (there is never a second scan, and
   process exit just kills the detached thread mid-walk with no cleanup
   needed). If re-boot is ever added, this detach must become a tracked,
   joinable thread. CLI paths that need the list call `WaitForIfsScan`.

DDR .arc staging (`WriteTempIfs`): a decompressed inner .ifs is written to
`%TEMP%/573renderer_ifs_<pid>/<basename>` before the path-based imagefs
mount. The dir is namespaced by process id so two renderer instances (e.g. a
CLI qpro extract launched while the GUI is open) cannot stage the same inner
basename onto each other's file mid-mount. Within one process the dir is
reused across loads (MountAndLoadIfs is single-threaded on the render
thread), so it does not litter a file per load.

Failure handling (audit, P4 slice): every failure that is DETECTABLE
without knowing a DLL's return-code contract is checked and routes through
the shared `FailBoot(state, msg)` helper (EndLoad + SetBootError banner +
SetBootState(Failed) + return false):

- DLL directory not found.
- DLL load or function-table resolve failure (`LoadAllDlls` checks every
  `DllLoader::Load` and every `*Funcs::Load`; `AfpFuncs::Load` returns false
  if `afp_boot` itself is null, so `afp.afp_boot` can never reach its call
  site null).
- AVS boot failure.
- Render window / D3D9 init failure.
- DDR (legacy) AFP boot failure (`DdrAfp::Boot` returns bool and is checked).

What is deliberately NOT treated as failure: the modern `afp_boot` /
`afpu_boot` / `afpu_render_init` INT return codes are logged but the boot
continues. This is intentional - the real success signal is the
`afp_stream_create` probe later, and the return-code contract differs across
the per-game afp-core builds. Turning a nonzero return into a hard failure
would need the return contract RE'd per game AND live game verification
across all four families; until then a blind check risks refusing to boot a
game whose afp_boot returns nonzero-but-fine. `AfpManager::Boot` returns true
unconditionally today for the same reason (best-effort past the guarded
prerequisites).

Known limitation (still open, needs live game RE): on failure the
partially-initialized subsystems are NOT torn down; a re-pick of a different
dir would hit "already loaded" errors, so the FIRST SUCCESSFUL BOOT IS
TERMINAL. Retiring this needs a teardown path (FreeLibrary + AVS/AFP/D3D9
shutdown in the right order) AND proof that the Konami engine even SUPPORTS
re-boot in-process (avs_boot / afp_boot are not obviously re-entrant). That
is a live-testing task, not a blind refactor, and is deferred rather than
shipped untested.

Settings written at boot: all tracked fields (game_dir, loop_master, render
size, fps, profile slug) are written together so a partial set does not
silently drop fields on rewrite. Render size is read back from the live
D3D state (not the request) so the persisted value is the one that survived
the floor and actually reached the device.

### 4.2 ScanGameDir details

- Collects `*.ifs` case-insensitively (Windows filesystems do not preserve
  extension case reliably). `name` = path relative to game root (GUI key),
  `full_path` = absolute for AVS. Sorted by name for a deterministic tree.
- DDR profile (`scan_arc_containers`): the renderable .ifs are LZ77-wrapped
  ONE-PER-.arc; each .arc's TOC is parsed head-only (cheap) and the inner
  .ifs surfaced as a `from_arc` entry (name = inner IFS logical path,
  full_path = the .arc). Arcs holding no .ifs (textures/models/data) are
  skipped; exactly one .ifs per bm2d arc.
- Progress: throttled to ~10 Hz TIME-based, not every-Nth-entry, because
  cold-drive readdir latency is wildly uneven (a fixed count updates in
  bursts) and per-entry mutex locking is avoided. Tracks the first two path
  components (e.g. "data/music") so the user can watch the walk move
  through the big subtrees. On Windows the `is_directory` check at depth
  <= 1 costs no extra syscalls (file type is cached from the directory
  read).

### 4.3 MountAndLoadIfs (the hot-swap load path)

- Runs on the render thread by necessity: AFP/AFPU call chains have
  implicit thread affinity with the thread that calls afp_do_update /
  afp_do_sort_render. Progress is published for the GUI instead of
  offloading.
- DDR arc path: decompress the .arc's inner .ifs to a temp file
  (`<TEMP>/573renderer_ifs/<name>`, one dir reused, file overwritten per
  load) so the existing path-based avs_fs imagefs mount works unchanged.
  The .arc path (not the temp file) is published as the active identity so
  the browser highlight matches the clicked entry.
- DDR load branch: umount `/afp/packages` + `/data` (hot-swap), then
  DdrAfp::LoadIfs. The package's top-level CLIPS (e.g. common_shutter's
  00_cleared / shutter_clear) are DDR's equivalent of the modern afplist
  animations and are exposed as the selectable layer list. Variant slots
  stay empty (DDR has no per-clip bitmap overrides). Frame labels are
  populated immediately from DdrAfp's by-name resolve of the four authored
  DDR background labels (in/loop/out/end via afp_mc_get_param 0x1012),
  because the DDR branch returns before the modern label enumeration.
- Modern path: package name = IFS basename minus ".ifs" ("title",
  "fcombo00"); AFPU uses this string as its internal package key. For
  arc-backed loads it is the inner .ifs name.
- Texture pre-count: texturelist.xml is read right after mount so the
  progress bar can be determinate; 0 (missing/unparseable) falls back to an
  indeterminate bar.
- Per-atlas sampler filters: read from texturelist.xml in the SAME forward
  order AFPU iterates the file, queued into AfpD3D9. TexCreate consumes one
  queue entry per call and stamps it on the new slot so SubmitGeometry can
  flip MAGFILTER/MINFILTER per texture (instead of the global
  LINEAR/ANISOTROPIC). The queue MUST be cleared before a fresh package
  opens - stale leftovers skew the indices and the wrong atlas's filter
  lands on the wrong slots. (This is the SDVX x=540 seam fix; BG atlases
  are NEAREST, UI atlases LINEAR.)
- After LoadPackages: the "master" animation (first matching name in the
  package's afplist.xml) is the initial playing stream; labels are
  enumerated via afp_mc_set 0x101F/0x1020. `ifs_size_bytes` measures the
  mounted file (for DDR that is the temp inner .ifs; note the .arc case in
  the debug scene measures the .arc); `load_time_ms` = wall clock at load
  finish. Both are constants the render thread copies into LiveState.

### 4.3b IFS manifest inspection + variant discovery (IfsInspect)

BuildIfsConfig re-derives bitmap_names / anim_names from the mounted IFS's
texturelist.xml (two-level atlas -> image walk) + afplist.xml (flat) on EVERY
load - they are purely derived, so a reload must rebuild from scratch or the
GUI lists show each name N times; slots are left intact (the user's variant
config outlives a reload, ProbeSlots dedups). Slot discovery is THREE-TIER
because a named variant clip (coin / e_amu / m_paseli) is a CHILD of the master
animation's timeline, not an afplist root, so afplist alone misses it: probe 1
= every afplist name, probe 2 = every bitmap name (authors often name a clip
after the bitmap it shows), probe 3 = a conservative well-known-Konami-name
list (not enumerable without parsing afp bytecode). Any name that resolves via
afp_mc_get_id_by_path becomes a controllable slot; default_bitmap seeds to the
clip path (the "(default)" heuristic).

### 4.4 Per-frame re-apply passes (ApplyVariants / ApplySubLayerVisibility)

- ApplyVariants runs once per frame - matching bm2dx's own per-frame
  variant pattern. Slot validity is probed lazily via
  `afp_mc_get_id_by_path`. A bitmap is only pushed when the user explicitly
  touched the slot (`bitmap_override` latches on first interaction):
  unconditionally re-writing `slot.path` every frame would defeat
  animations that cycle through multiple bitmaps via PlaceObject.
- ApplySubLayerVisibility re-asserts each frame so a PlaceObject-authored
  re-show cannot override the user's toggle (same rationale as the
  license_usr re-assert and BG 26's per-mount denylist hide). Thread-safety
  rule: it must copy the overrides under the State lock
  (GetSublayerOverrides) - iterating the live `cfg.sublayer_overrides`
  is a use-after-free risk because the GUI thread can emplace_back
  concurrently and a vector reallocation would invalidate the walk;
  MutConfig releases its lock on return, so it is the wrong tool for a
  cross-thread vector.

### 4.5 CLI override application (ApplyCliOverrides)

- Runs after boot + startup IFS mount (it needs the IFS mounted), symmetric
  with how --continuous-loop / --scale layer over settings.
- `--afp-speed`: global afp playback-speed multiplier
  (`afp_set_global_speed`). The SDVX submonitor runs at 60 fps while the
  afp data is authored at 120 fps, so the submonitor export passes 0.5 to
  halve the advance. Applied once; persists through afp_do_update during
  export.
- `--root-loop <hold|force>`: -1 = unset (keep settings). Hold is the
  game default: mount once, the root holds while children free-run.
- CAfpViewerScene parity toggles (--paused / --filter / --show-mc-names)
  land in the sticky LiveOverrides; `--seek-frame` is a one-shot handled in
  the render loop, not here.
- `--variant` upsert: `default_bitmap` defaults to the clip path
  (heuristic); `bitmap_override` latches whenever a concrete bitmap is
  supplied, matching the GUI.

---

## 5. AfpManager API semantics (afp_boot.h contracts)

These are the engine-behaviour facts encoded in the header comments:

- **PlayBitmapAnimation** (SDVX Type 2 system BGs, e.g. bg_iseki): routes
  `afpu_image_lookup` (afp-utils ord 0x46) + `afpu_image_to_stream_args`
  (afp-utils ord 0x4C) + `afp_image_stream_create` (afp-core ord 0x22),
  mirroring soundvoltex.dll's bitmap-animation play routine.
  SwitchAnimation by contrast
  goes through `afp_stream_play` (afp-core ord 0x1E).
- **SwitchAnimation**: destroy current stream (`afp_stream_destroy` type=5)
  + fresh `afp_stream_play`; package stays resident so atlases are not
  re-uploaded. No-ops when the name already matches unless `force`.
- **BIND-ON-PLAY** (the ForceReplay rationale): `afp_stream_play` walks the
  timeline and binds every PlaceObject bitmap reference to a specific
  texture AT PLAY TIME; the bindings live in the stream struct and are NOT
  re-resolved on later frames. So loading/unloading a companion after the
  stream is running does not change pixels until a destroy+replay. Real
  bm2dx sidesteps this by mounting the locale companion BEFORE
  afp_stream_play.
- **Companions**: IIDX locale companions (`<base>_{j,a,k}.ifs`) share
  bitmap NAMES with the base (title.ifs::coin vs title_j.ifs::coin - same
  name, different pixels); AFPU's lookup is last-loaded-wins, so a freshly
  loaded companion package overlays the base's textures. That is also why
  the GUI enforces EXCLUSIVE companion selection - keeping two loaded buys
  nothing. Unload cascade: `afpu_package_control(6)` + `avs_fs_umount` of
  the per-companion mountpoint (`/afp_companion_N`, monotonic counter so
  two companions never collide even after an unload).
- **DestroyCurrentStream**: afp_stream_destroy type-5 cascades the whole
  master tree including every child clip attached via
  `afp_mc_attach_stream`. The composite extractor must call this BEFORE
  UnloadCompanion - unloading a companion whose item clip is still attached
  to the avatar faults afp.
- **ReadLayerPosition**: `afp_get_layer_info` word[13] = cur,
  word[12] = total. Export stops at cur >= total so the recorded sequence
  matches the authored frame count (the older IsMasterComplete check fires
  1-2 frames before saturation and lost the tail).
- **ReadMcPlayhead**: the BOUNDED movie-clip playhead from the root mc via
  afp_mc_set codes 0x1010 (cur) / 0x1011 (total) / 0x1013 (loop_count) -
  the same source as the IIDX AFP debug viewer's [TIME] readout. Root mc is
  refered first (`afp_mc_get_id_by_path(stream,"")` +
  `afp_stream_control(6, mc)`). Unlike the free-running layer counter
  (which climbs past total under the continuous-loop dance), this value
  FREEZES when the clip self-stops at its authored stop(); `total` is the
  WHOLE-CLIP frame count (afp-core's whole-clip frame-count function), so a
  label stopping mid-timeline reads cur < total.
- **ReadLayerAdvanceCounter**: `afp_get_layer_info` word[14]
  (= layerobj+60). The per-stream tick function
  increments it ONLY on a tick where the ROOT movie clip's current_frame
  actually changed - afp's OWN "did playback advance" signal, independent
  of export/native fps ratio. A play-once label hitting its stop() freezes
  the playhead, so the counter stops incrementing; that freeze is the
  drift-free "labelled segment finished" signal the export uses.
- **EnumerateLabels**: afp_mc_set(mc, 0x101F, &count) then
  (mc, 0x1020, i, &name, &frame), after refer via afp_stream_control(6,mc).
  Frame labels are the debug scene's generalisation of the bg_bpls5 "loop"
  mechanism.
- **GotoLabel**: deep/recursive goto+play by label = afp_mc_control op
  0xF09.
- **SeekFrame**: mirrors CAfpViewerScene LEFT/RIGHT seek, which wraps bm2dx's
  seek helper -> SetFrame a3=1,a4=1
  -> afp_mc_control op 0xF08
  (deep_goto_play with an INT frame). The 0xF08 path's refer-CHECK returns
  -4 if the root mc is not refered first. `frame` is low-clamped to 0
  because afp treats it as unsigned and only clamps the high end to
  total-1.
- **SetStreamPaused**: the debug scene's RETURN+SHIFT toggle = set stream
  playback SPEED (afp ord 0x02A) to 0.0 (frozen) or 1.0 (running), then
  stamp stream flag bit0 via afp_set_flag_mask (ord 0x037, mask=1 val=1).
  Both act on the stream/layer work, NOT the per-clip stop bit. bm2dx:
  the pause routine (speed 0) and the resume routine (speed 1);
  the only differentiator is the speed float - the flag value is 1 in both.
- **EnumerateChildClips**: `afp_mc_enumerate_children` is afp-core ord
  0x079; flags=3 = recursive + fullpath ("parent/child" paths). Child
  screen position = afp_mc_set(child, 0x1008, &xy), the same
  world-translate the scene's F3 overlay reads via bm2dx's
  world-translate readers. Per-child position reads can FAULT on
  dynamically re-created nested clips, so the names-only Sub-layers list
  passes want_positions=false. Child playheads (0x1010/0x1011 on the child
  mc) reveal whether a nested child free-runs across a root wrap or snaps
  back (the o_kazari5 diagnostic).
- **DestroySceneStreams** exists because `afpu_shutdown` ASSERTS
  (`__debugbreak`, exit 0x80000003) if any scene stream is still alive at
  shutdown - so both the hot-swap path and Shutdown destroy the primary +
  extras and then run the type=8 priority=0 sweep (full teardown mechanics:
  docs/d3d9_backend.md "scene teardown"). Persistent create_level=1 boot
  IFSes are untouched by the sweep.
- Sentinels: no-stream = `0xFFFFFFFC`; `pkg == 0xFFFFFFFE` = invalid
  package.

---

## 6. Render loop orchestration (RunRenderLoop)

### 6.1 Pacing

- Fixed-timestep QPC pacer at the user's chosen fps (default 120,
  clamped 1..1000); dt = 1/fps. Sleep down to a half-millisecond
  (`kBusyWaitTicks` = QPC/2000), then YieldProcessor spin. If the loop
  falls more than 4 frames behind, the anchor resets (no catch-up debt).
- Export mode: the pacer is BYPASSED (loop runs as fast as encoder + GPU
  readback allow) and dt is overridden to 1/export_fps so AFP advances one
  OUTPUT frame per tick - otherwise the captured video would play back at
  the encoder's speed, not the authored speed. `next_frame_tick` is
  re-anchored to "now" each export tick so paced mode resumes without a
  huge accumulated pacing debt.
- FPS diagnostics: measured fps published ~4 Hz; animation speed =
  fps * dt, so a measured rate persistently above target means AFP sees too
  much accumulated dt.

### 6.2 CLI autopilot one-shot gates (ordering contracts)

Each `--flag` has a fired-once bool; the gating chain enforces
"switch animation, then bind submonitor, then apply label, then export":

- `--animation`: posts switch_animation once a clip is live AND the current
  name differs. Backend-agnostic liveness via
  `RenderLive::Inspect::HaveActiveClip` (DDR's modern StreamId is the
  0xFFFFFFFC sentinel; without the seam the switch never posted in DDR).
  If the name already matches, the flag still flips (so the export gate
  does not wait forever) and any `--animation-label` is applied via the
  seam, recording label_playback_active so the export tracks the bounded
  playhead.
- `--submonitor-frames`: one-shot bind after the requested animation is
  live. Decodes loose PNGs to texture slots and binds them to the
  placeholder clip's child layers via afp ord 0x088
  (McControl::BindClipImages), exactly as SDVX's CSubmoniLayerComponent
  does. Must complete BEFORE the export fires (the export gate waits on
  `submonitor_bound`). Modern-only - the submonitor bg is an SDVX afp-core
  scene, never DDR.
- `--seek-frame` / `--goto-label`: post once a clip is live and any
  requested animation is in place; routed through the normal Request path
  so behaviour is identical to the GUI (seek pauses + re-baselines the wrap
  counter).
- `--export`: fires once ifs_loaded && anim_ready && label_ready &&
  submonitor_bound. ifs_loaded differs per backend (modern: stream id
  valid; DDR: DdrAfp::IsBooted && LayerId != 0). label_ready requires
  Status to REFLECT the applied label (both --goto-label and --export post
  on IFS-load and the request slot is single, so this sequences
  "label, then export" instead of racing). Exit when ExportPhase reaches
  Done or Failed.
- `--swap-after-frames` + `--ifs2`: posts the identical hot-swap request
  the GUI would, at the configured frame; bare names are resolved against
  the scanned IFS list; `.arc` suffix implies from_arc.
- `--exit-after-frames`: clean self-test exit.
- `--screenshot-frames`: 1-indexed (frame being rendered = frame_count+1 at
  the queue point, since frame_count increments post-EndFrame); queued dumps
  are consumed by EndFrame.

### 6.3 Request handling notes

- Hot-swap: BeginLoad is flipped on BEFORE the unload so the GUI overlay
  appears instantly even though the render thread is about to stall. The
  DDR unload happens inside the DDR `LoadScene` override (the modern unload
  would call into null g_afp/g_afpu tables in DDR mode). Label state is
  cleared so a play-once label cannot leak into the new IFS's first export.
- switch_animation with the SAME name = user wants a replay;
  SwitchAnimation short-circuits that case (to avoid blanking on
  double-clicks), so it is routed through ForceReplay instead (which clears
  the tracked name to bypass the short-circuit).
- Companion toggle: exclusive selection + BeginLoad overlay + ForceReplay
  afterwards so new bindings resolve (see section 5, BIND-ON-PLAY). An
  empty AnimName means a bitmap-only package - nothing to replay.
- qpro extract/scan requests run synchronously ON the render thread
  (thread affinity again; mounts ~2300 part IFSes; GUI thread stays
  responsive showing the overlay). Body assembly needs a >= 520x704 RT.

### 6.4 DDR mode interplay (LOAD-BEARING)

In DDR mode `AfpManager::StreamId()` is the 0xFFFFFFFC sentinel (g_afp is
null), so the loop substitutes `DdrAfp::LayerId()` as the loop-bookkeeping
id. That resets frames_since_switch on a DDR hot-swap and feeds the correct
active id into PublishLiveState (which lights up the whole DDR live
readout). Every MODERN-only per-frame block still self-skips in DDR because
it ALSO gates on `AfpManager::IsBooted()` (false in DDR) or a null
`g_afp.afp_*` member - EXCEPT the two ForceReplay blocks and the variant
probe, which needed explicit `!g_ddr_mode` guards once stream_id stopped
being the sentinel:
- ProbeSlots drives modern afp_mc_* (null in DDR); DDR backgrounds expose
  no named variant clips anyway.
- trim_frames / loop_master ForceReplay are modern CLayer-flag +
  afp_stream_destroy/play mechanics that do not exist on DDR; DDR
  loop/export uses its own export-side authored-loop detector.

DDR render: `DdrAfp::RenderFrame(dt)` is self-contained per-frame
(render_init / do_render / render_finish / do_display).

### 6.5 The continuous-loop flag sequence (SDVX BG dispatcher parity)

This mirrors the in-game BG dispatcher's CLayer-flag maintenance on the
master stream. Applied once per stream-switch (tracked by
`flag_dance_done_for` + mode seen), via `afp_set_flag_mask(stream, mask,
value)`:

ON (mode 1), exact order:
1. `afp_set_flag_mask(sid, 0x200, 0x0)` - clear bit 9 (main-playable)
2. `afp_set_flag_mask(sid, 0x1,   0x0)` - clear bit 0 (auto-render)
3. `afp_set_flag_mask(sid, 0x1000,0x1000)` - set bit 12 (CLayer flag)
4. `afp_set_flag_mask(sid, 0x1,   0x1)` - set bit 0 back

This puts the master into continuous-loop mode where `cur` advances past
`total_length`. Explicit OFF (mode -1): `afp_set_flag_mask(sid, 0x1000, 0)`
(bit 0 stays set). Additionally a PER-FRAME idempotent
`afp_set_flag_mask(sid, 0x1, 0x1)` mirrors CLayer's per-tick Update and
defends against the engine clearing the flag.

Effective-mode resolution for the live preview: the explicit live control
(-1/0/1) always wins; a left-alone control (0) maps to ON in BOTH root-loop
modes, because the dance is needed in Hold too: it keeps the master CLOCK
running so the root's own tick loops it via shallow-seek (which REUSES
persistent nested children) instead of freezing at the last frame. A held
master with no dance freezes the ENTIRE scene, nested o_kazari5-style
children included - verified by frame comparison: passive Hold produces
byte-identical frames; with the dance, root-phase-aligned frames DIFFER,
proving the child free-runs. The modes differ only in whether they ALSO
ForceReplay:
- Hold (game default): dance yes, ForceReplay NEVER. A ForceReplay
  full-remounts the stream (afp_stream_destroy + afp_stream_play), which
  resets EVERY nested child to frame 0 - the o_kazari5 snap. In Hold the
  engine's own shallow-seek loops the root while persistent children
  free-run.
- Force: dance yes, plus re-drives one-shot masters (bg_common) that the
  user wants hard-looped for a full-clip export.

The FADE slideshow forces the dance OFF (-1): it drives subbg_usr's alpha
via GotoLabel(fade_in/out), and the dance would PIN the alpha at the held
last-keyframe state and fight the gotos (same reason the label-playback
export path disables it).

### 6.6 Loop / trim / replay bookkeeping

- `loop_cooldown` = 10 frames (~83 ms at 120 Hz) hysteresis after a replay:
  the fresh stream's frame counter briefly reads "past end" before
  afp_do_update settles, and 10 frames is enough for it to advance past
  frame 0 so "complete" is not immediately re-detected.
- CRITICAL: after every ForceReplay the local `stream_id` must be refreshed
  so the rest of the frame (variant apply, master-scale apply, render
  dispatch) uses the NEW id. Otherwise the scale-apply block sees the old
  id, decides "nothing changed", and the new stream renders one frame with
  an identity transform (the reported "ghost frame" at cur == total).
  `flag_dance_done_for` is also reset so the dance re-applies on the new
  stream.
- After the loop_master auto-restart, frames_since_switch is reset to 0 so
  the live-state wrap tracker treats it as a fresh start; without this, afp
  slot REUSE on replay can keep the same stream_id and the playhead's jump
  to 0 gets miscounted as an authored loop wrap on every engine
  auto-restart.
- Auto-restart is disabled during export: capturing the actual continuation
  (master holds + sub-clips animate) is the point, and AFP's set_complete
  flag can fire as early as the end of the intro, so a ForceReplay would
  produce a stuttery loop in the exported clip.

### 6.7 Master scale (op 0x1003 / 0x101E)

Mirrors the game's own dispatch path: soundvoltex.dll's master-scale
dispatch routine invoked via CLayer vtable slot 26. On the root mc
(`afp_mc_get_id_by_path(stream, "")`):
- op 0x1003 writes the float pair (_xscale/_yscale) into the MC work struct
  at +272/+276 and flags `+32 |= 0x40000` / `+36 |= 0x8` to trigger an AFP
  transform re-bake (verified in afp-core's transform re-bake consumer).
- op 0x101E invalidates the cached transform so AFP picks the new value up
  next render.
Applied only on stream change or scale change.

KNOWN ISSUE (open): applying scale via this call produces a visible
centerline seam on BG 3 (mirrored booth) at scale = 1.5, while the game
makes the SAME call without the seam; root cause under investigation. Do
NOT switch to a smaller scene RT + StretchRect upscale workaround - the
game renders directly into a single 1080x1920 RT (verified), and the
CLAUDE.md "never take the easy way out" rule applies.

### 6.8 Render dispatch + fault diagnostics

- The SDVX vertex-transform AV: `afp_do_sort_render` AVs at frame 0 inside
  afpu's matrix-transform routine on misboots. The renderer survives the AV
  but no draws land (blank framebuffer). IIDX renders fine because its afpu
  (1.2.19) takes a different path than SDVX's (1.2.26).
- Pre-state dump (once): the world matrix TYPE byte in the afpu play struct
  controls which case branch the matrix-transform routine takes.
  0..4 = matrix variants; 5 = "calc world mat type error in 3d 3d" (afpu
  logs and returns without doing anything - observed when the 4x4-matrix
  setter writes the matrix); values > 5 fall through into the crash region.
  The 4x4 matrix data (or the 6-float 2D matrix) sits in the afpu play struct.
- Fault logging: first hit only; PC and target resolved module-relative
  (afp-core / afp-utils, 2 MB range check) for direct IDA lookup, plus the
  full RAX..R15 dump with any register that looks like an afp/afpu address
  annotated as module+offset.
- Export tick placement: `Export::OnMainLoopTick` runs AFTER AFP has
  rendered into the offscreen RT and BEFORE EndFrame's StretchRect, because
  the StretchRect drops the alpha channel onto the XRGB backbuffer -
  reading the offscreen at this point captures proper RGBA with
  transparency.

### 6.9 Submonitor slideshow mechanics (SDVX submonitor parity)

- CROSS-FADE mode (`--submonitor-slideshow`): `r3_fade` is a 2-layer
  self-looping afp crossfade - depth-2 base at alpha 1 with a slow
  Ken-Burns pan, depth-3 overlay alpha 0 -> 1 over the back ~43% of the
  loop; it shows only ONE pair of frames per loop. The game plays an
  N-frame slideshow by REBINDING the two layers each loop: base <- the
  frame the overlay just faded to, overlay <- the next frame. Because the
  overlay never pans and alpha/tx reset at the loop's 'loop'@0 label, the
  hand-off is pixel-seamless. Loop detection uses afp's OWN bounded
  playhead (never pixels): the per-frame playhead delta is accumulated
  wrap-safe into a cumulative counter and cycle = frames /
  submonitor_loop_frames - correct whether cur free-runs past total (Hold
  dance) or wraps (bare gotoAndPlay). A backward playhead jump means afp
  wrapped to 'loop'@0; the frames up to the wrap are counted, then
  accumulation resumes. A full slideshow loop = N afp loops, so the
  orchestrator captures N * submonitor_loop_frames output frames for a
  seamless mp4. The rebind runs AFTER afp_do_update so it lands on this
  frame's render dispatch.
- NORMAL/FADE mode (`--submonitor-slideshow-fade`): the game's NORMAL
  slideshow uses ONE centered non-panning `subbg_usr` holder whose alpha is
  driven by the subbg_0001 fade_in/fade_out labels. One layer cannot
  cross-dissolve, so each frame fades in, dwells, fades out, then the next
  binds. Driven purely by the OUTPUT frame count (deterministic:
  period = fade + dwell + fade) so a capture of N periods loops perfectly
  (transparent dip to transparent dip). Also runs after afp_do_update.
- `license_usr`: the subbg_0001 template has a depth-3 offset "license_usr"
  holder for collab copyright art. With no license bitmap bound it renders
  as a missing-texture placeholder, while in-game an unbound holder shows
  nothing - so it is hidden, and RE-HIDDEN every frame in fade mode because
  a GotoLabel re-seek can re-place (re-show) it.

### 6.10 Shutdown order

Signal `ShouldExit`, join the GUI thread FIRST (it owns its own D3D9 device
and Win32 window, which must go before the DLLs are freed), then DdrAfp or
AfpManager shutdown, AvsManager shutdown, D3D shutdown.

---

## 7. SEH policy (render_seh)

- Structural constraint: the `__try/__except` wrappers MUST live in their
  own TU as plain C-style functions - `__try` cannot sit in a function that
  also needs C++ object unwinding, which RunRenderLoop does.
- Exact wrapped call shapes: `afp_do_update(dt, 3, 0)` and
  `afp_do_sort_render(1, 0)`.
- What is captured on a fault: exception code, faulting PC (both
  ExceptionAddress and ContextRecord->Rip are read; Rip wins - they should
  match for synchronous exceptions but the OS may adjust one), the access
  op (ExceptionInformation[0]: 0 = read, 1 = write, 8 = DEP), the target
  address (ExceptionInformation[1]), and RAX..R15 - all so the fault site
  can be matched against IDA disassembly with live register values.
- The capture is RETURNED BY VALUE as a `RenderSeh::FaultReport` (a plain
  POD: faulted flag, code, pc, target, op, regs[16]). `SafeCallUpdate` and
  `SafeCallSortRender` return it directly; no fault state lives in file
  statics. FaultReport is trivially destructible, so it is legal to
  hold as a local inside the `__try/__except` function (the C2712
  object-unwinding restriction is about non-trivial destructors, which it
  does not have). The filter (`CaptureFault`) writes into a pointer to that
  local. Because the report is returned, the caller decides what to do: the
  render loop logs a detailed one-time dump, the qpro batch paths discard it
  (they only need the fault swallowed).
- The detailed dump lives in `RenderSeh::LogFault(what, frame, report)`, not
  in the render loop: it maps the pc and each of the 16 registers back to
  afp-core / afp-utils module offsets (the register mapping turns a value
  that looks like an afpu address into a module-relative offset for IDA).
  Keeping the dump out of RunRenderLoop is part of the P4 SEH consolidation,
  which also eliminated the
  `LastFaultPc()/Target()/Op()/Regs()/RegNames()` accessor surface; no such
  accessors exist.
- Policy on swallowed render faults: swallowing is survival, not
  acceptance. A caught AV in afp_do_update / afp_do_sort_render means an
  afp callback dereferenced something the renderer mishandled (historical
  example: the SetLayer "no HSV filter" sentinel); the unwind skips afp's
  own cleanup and SILENTLY CORRUPTS render state. So the handler screams
  loudly in the log ("REVERSE the faulting path and fix it ASAP"),
  rate-limited to the first 3 hits then every 600th, with the PC mapped to
  module+offset (573Renderer.exe / afp-core / afp-utils, 8 MB range).
- `SafeEnumChildNames`: afp's bulk child-enumerate FAULTS when a scene is
  mid-recreating dynamic nested clips (e.g. sel_all's per-second timer
  digits), so the always-on names-only Sub-layers feed routes through the
  guarded version. Flags semantics: bit0 = recurse, bit1 = fullpath.
  flags=0 (direct children, relative names) is the per-level building block
  that does NOT fault; flags=3 (recursive fullpath) is the faulting one.
  Result-header layout (afp-core's child-enumerate implementation): u16
  written at +0, u16 total at +2, then a table of `char*` at +8 + 8*i.
  Return codes 0 and
  0xFFFFFFFB are both success-shaped. ALL reads (including the name string
  copies) sit inside __try because a dangling name pointer can fault too.
- `SafeGetIdByPath`: resolving a clip path can fault if the path names a
  clip mid-recreation (used by the lazy tree builder stepping into expanded
  nodes).

---

## 8. Live-state readout (render_live) - CAfpViewerScene parity

Decomp source: IIDX 33 bm2dx's in-view debug screen routine +
afp-core / afp-utils. Feature-to-afp-call map:

- SEEK: afp_mc_control 0xF08 (deep goto + play, absolute INT frame). The GUI
  pairs every seek with paused = true, matching the scene's pause-on-seek
  behaviour; the request handler mirrors that into the sticky pause override
  and applies it immediately.
- PAUSE: stream speed 0/1 + afp_set_flag_mask(stream, 1, 1).
- FILTER: afp-core ord 0x032 (`afp_set_filter`), called as
  `afp_set_filter(stream, &blob, 48)` where the blob's first u32 =
  `0x80000000 | enable`. The scene's CLayer slot-32 wrapper writes the
  filter id to CLayer+552 before the call.
  Ord 0x032 is not a bound AfpFuncs member (one-off); it is resolved via
  the DLL loader on demand. Re-applied once per stream switch and on
  toggle.
- F3 MC list: afp_mc_enumerate_children (ord 0x079) + per-child 0x1008
  position.
- FILE INFO: afp-utils `afpuloc_get_package_info(pkg, sel)` with sel 1 =
  converter version, 3 = package version, 4 = afp version;
  `afpuloc_get_version_string(pkg, 2)` = converter engine name. The
  package id carries the 0x30000000 tag the resolver gate requires. Version
  unpack matches the scene's printf: major = bits 16-31, minor = bits 8-15,
  patch = bits 0-7 (HIWORD.BYTE1.byte0).

All live controls are gated to the LIVE preview only - none run while an
export is capturing (a seek would corrupt the loop-wrap counter, a pause
would freeze the capture; the export pins its own stream state). The
export-end path calls ResetPauseDefend because the export forced speed 1.0
at start and the defend's cache is stale afterwards; clearing it lets the
sticky paused override re-apply once exporting clears.

Backend seam (Inspect::): each helper branches ONCE on g_ddr_mode - the
only place g_ddr_mode appears for the readout/controls. Deliberately free
functions, not a vtable: this is the natural cut-line a future avs/afp
version-separation refactor widens into a backend interface, but it is not
that refactor yet. DDR-side facts:

- DDR's afp 2.13.7 exports NO afp_set_filter, NO bulk child-enumerate, NO
  afpuloc_* version exports (RE-confirmed), so FILTER / MC-name enum /
  FILE-INFO stay modern-only and the GUI widgets self-hide. For FILTER
  specifically there is also nothing to push on DDR: its only runtime filter
  path is the host render callback, which the renderer already drives
  per-element from the asset's BAKED filters.
- DDR playhead: afp_mc_get_param 0x1010 cur / 0x1011 total; 0x1013 stays 0
  for DDR's gotoAndPlay idle loop, so the surfaced loop tally is the
  caller-computed wrap count.
- DDR [SIZE]: afp_stream_get_info +16/+18 - NOT out+2 (that is the AP2
  version field = 512).
- DDR pause: `afp_layer_play(layer, paused ? 0 : 1)` - NEVER afp_layer_stop
  (that is a teardown).
- DDR seek: afp_mc_op 0xF08 deep_goto_play. DDR GotoLabel resolves
  name -> frame via afp_mc_get_param 0x1012 then uses 0xF08 (0xF09
  by-label resolves to the same thing internally).
- DDR master-complete: always false - seamless authored loops have no
  one-shot finished latch; the wrap tally is the signal. Modern
  IsMasterComplete keys off the 0xE0000000 flag group (finished 0x20000000
  / wrapped 0x40000000 / set_complete 0x80000000 latches).
- Modern [SIZE]: afp_get_layer_info u16[8]/u16[9] (= work+0x20/0x22).
- Modern raw layer info: word[13] = free-running cur, word[12] = total,
  word[1] = flags0. No DDR analogue, so have_layer_info stays false there
  and the raw cur / flags0 GUI lines self-hide.

Wrap tally rules: the [TIME] loop count = backward playhead wraps (cur
decreased) since the clip / label / stream last (re)started. Re-baseline
(seed, do not count) on: stream/layer switch (keyed on the backend-stable
active id, so a DDR hot-swap re-baselines), label (re)select, fresh start
(frames_since_switch == 0), or a user SEEK (the one-shot NotifySeek flag,
consumed inside PublishLiveState) - a backward seek must not be miscounted
as a loop.

Headless verifiability: the [FILE INFO] version triplet is logged ONCE per
package, and the sub-layer tree's top-level set is logged once per
(stream, count) change - so both are checkable from renderer.log without a
GUI and without log spam per refresh.

Sub-layer tree: rebuilt ~8 Hz (every 15 ticks at 120 fps) so an expand or a
newly-placed dynamic clip (e.g. content_usr appearing at content_in@30)
shows up promptly; only the root
level plus user-expanded branches are walked, each with the non-recursive
flags=0 enumerate that does not fault, and each expanded node resolved via
the SEH-guarded path->id. Names are copied OUT of the shared static buffer
before recursion refills it. The F3 overlay's positioned list (recursive +
positions, the faultable kind) is built only while the overlay is ON,
refreshed every ~1 s (120 ticks), and on a faulted enumerate the previous
list is KEPT rather than cleared. Depth cap 8.

---

## 9. Window / WndProc contracts (window.cpp)

- Exact-size guarantee: the window is positioned at (0,0) with the exact
  AdjustWindowRect outer size instead of CW_USEDEFAULT. CW_USEDEFAULT lets
  the WM pick position AND size, auto-shrinking any dimension exceeding the
  work area - a portrait 1080x1920 client on a 1080p landscape monitor got
  clipped to ~1920x1040 outer / ~1916x1006 client, and Present() then
  stretched the 1080x1920 backbuffer into the squashed client (visible
  distortion). Pinning to (0,0) with the exact outer size forces Windows to
  honour it even when the bottom extends below the desktop; the title bar
  stays reachable.
- Post-create re-assert: SetWindowPos with SWP_NOZORDER | SWP_NOACTIVATE |
  SWP_NOSENDCHANGING. Some WM hooks / accessibility tools intercept
  WM_GETMINMAXINFO and shrink oversized windows; SWP_NOSENDCHANGING is
  critical or the same intercept fires again. If the client STILL does not
  match (heavily themed setups / unaccounted DPI scaling), a warning is
  logged so distortion can be correlated with the mismatch instead of being
  chased through D3D9.
- Mouse macros: GET_X_LPARAM / GET_Y_LPARAM (windowsx.h) are required -
  bare LOWORD/HIWORD sign-extend wrong for negative coords on
  multi-monitor setups.
- Crop pick mode: WndProc intercepts mouse events ONLY while the GUI has
  armed pick mode (otherwise the render window would steal every click).
  Drag state lives in client-pixel space and is converted to RT space only
  when published (single consistent representation for overlay + export).
  The client -> RT mapping is a pure ratio with no letterboxing because
  EndFrame's StretchRect maps the whole RT onto the whole client area.
  Points are clamped to the client rect (mouse capture lets the cursor
  leave the window mid-drag). ESC cancels pick mode first, closes the app
  only when pick mode is off. Mouse-up auto-exits pick mode (border colour
  amber -> cyan as commit feedback) and a degenerate zero-area rect is
  cleared (a 0-sized crop would silently break export). WM_CAPTURECHANGED
  (alt-tab, menus) drops the drag so no stale half-rect lingers. The RT
  size is pushed in once after D3D init (SetRenderRtSize) so WndProc never
  reaches into the render backend.

---

## 10. Process / thread architecture notes (main.cpp)

- CLI parsing uses CommandLineToArgvW (lpCmdLine lacks argv[0] and has
  quoting issues), converted to UTF-8.
- Self-contained CLI fast paths that bypass the normal boot entirely:
  `--ddr-test` (only touches DDR's libavs/libafp/libafputils),
  `--extract-arc`, `--extract-customize`, `--extract-qpro-json` (pure PE
  parse of bm2dx.dll), `--qpro-scan`. Their pollers treat total == 0 as
  "still scanning".
- The argument-error MessageBox is suppressed under --no-gui/--headless
  (a blocking modal would hang the orchestrator, which launches with
  --no-gui and waits on exit); the raw args are scanned because Cli::Parse
  has already failed at that point.
- The GUI thread starts FIRST, before any game DLL is touched, so the
  Setup screen appears immediately. It owns its own Win32 window + D3D9Ex
  device via the SYSTEM d3d9.dll, so it has no dependency on the game
  DLLs; communication is strictly through App::State.
- Single boot code path: the CLI auto-boot posts the same set_game_dir
  request the GUI's Load button posts. Boot failures loop back to the
  picker; ShouldExit before boot exits cleanly.
- qpro batch extraction runs on the main thread because it owns the
  AFP/AVS/D3D9 affinity post-boot.
- `--ifs` resolution: absolute path used verbatim; otherwise resolved
  against the scanned IFS list (relative-path match first, then bare
  filename against the last path segment; backslashes normalised). The
  list is consulted rather than plain exists() because dev setups with a
  stale `data/` next to the worktree would otherwise grab the wrong file.
  An absolute path ending in `.arc` implies from_arc.
- Post-boot foreground fix: BootFromGameDir's render window (a separate
  top-level window, SW_SHOW) grabs the foreground and sits on top of the
  ImGui control window - at the GITADORA 4K default it covers it entirely,
  and Windows routes mouse input to the foreground window, so clicks on
  the IFS tree never reached ImGui. The control window is raised back
  (ShowWindow + BringWindowToTop + SetForegroundWindow) as the interactive
  surface.
- `AfpManager` module state is split across afp_boot.cpp / afp_packages.cpp
  / afp_anim.cpp purely for the 1000-line limit; the shared state lives as
  inline variables in afp_boot_internal.h (exactly one instance across
  TUs).
