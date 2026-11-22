# 32-bit (x86) build

## Why it exists

IIDX 24 (SINOBUZ) and older ship 32-bit engine DLLs (`libavs-win32.dll`,
`libafp-win32.dll`, `libafputils-win32.dll`; PE machine 0x14c). A 64-bit
process CANNOT load a 32-bit DLL: WOW64 isolates at process granularity, so
`LoadLibraryA` on an x86 DLL from an x64 process fails with
`ERROR_BAD_EXE_FORMAT` (193). Verified empirically against
`libafp-win32.dll`, not assumed. There is no in-process workaround; hosting
those DLLs requires a 32-bit renderer process.

The alternative (an out-of-process 32-bit helper driving the engine over
IPC) was rejected: the renderer's whole interaction with afp is in-process
pointer work (callback tables handed to `afp_boot`, per-frame vertex
callbacks, engine struct reads), so a bridge would have to marshal every
callback in both directions per frame.

## Building

`build32.bat` mirrors `build.bat` but calls `vcvarsall x86`, configures the
`dev32` preset (binary dir `build32/`, triplet `x86-windows-static`), and
produces `bin/573Renderer32.exe`. The x64 build is untouched and still
produces `bin/573Renderer.exe`; `OUTPUT_NAME` carries `R573_ARCH_SUFFIX` so
the two live side by side in `bin/`.

All vcpkg dependencies (ffmpeg, imgui, x265, aom, vpx, webp, Catch2,
tl-expected) build for `x86-windows-static` with no manifest changes; the
x265 overlay port already handles `VCPKG_TARGET_ARCHITECTURE STREQUAL
"x86"`.

The old `CMakeLists.txt` hard failure ("This project must be built as
64-bit") is replaced by a pointer-size switch that sets `R573_ARCH_X64` and
`R573_ARCH_SUFFIX`.

`573Renderer32.exe` exports through `bin/573Encoder.exe`, which only the
x64 build (`build.bat`) produces, so a working 32-bit setup needs both
builds in the same `bin/`. See "Encoding runs in a 64-bit child" below.

## Configuring dev32 from an IDE (CLion): needs an x86 toolchain

`base32` declares `"architecture": {"value": "x86", "strategy": "external"}`.
`external` means CMake does NOT set the architecture up - the CALLER must.
It is the only valid strategy under the Ninja generator, because Ninja has
no platform concept: with MSVC the target architecture comes entirely from
the `INCLUDE` / `LIB` / `PATH` environment variables.

`build32.bat` supplies that by calling `vcvarsall.bat x86` (vs `x64` in
`build.bat`), and CI does it with `ilammy/msvc-dev-cmd` `arch: x86`. An IDE
that configures the preset from its own default (amd64) environment gets a
half-x86 setup: the x86 compiler with x64 libraries. It fails as

```
...\lib\x64\MSVCRTD.lib : warning LNK4272: library machine type 'x64'
                          conflicts with target machine type 'x86'
unresolved external symbol _mainCRTStartup / __RTC_InitBase / __RTC_Shutdown
```

The unresolved symbols are the x86 CRT entry points (leading-underscore
cdecl decoration); they are missing because the x64 CRT was linked against
an x86 object.

Fix in CLion: Settings -> Build, Execution, Deployment -> Toolchains -> add
a Visual Studio toolchain with **Architecture: x86**, then point the
`dev32` CMake profile at it. Leave `dev` on the default toolchain.

This is an environment mismatch, not a preset defect - do not "fix" it by
pinning compiler paths or LIB directories in the preset, which would hard
code an MSVC version. Note the dangerous sibling of this failure: with a
fully x64 environment the configure SUCCEEDS and silently produces an x64
binary named `573Renderer32.exe`, which is why CI verifies the PE machine
field (0x14c) rather than merely that the file exists.

## The engine ABI seam (src/support/engine_abi.h)

Two things about the engine interface change with pointer width. Both are
routed through one header so there is a single place to reason about them.

### Calling convention: `AFP_CB`

The DDR/legacy render callbacks were declared `__fastcall`. On x64 that
annotation is a NO-OP (there is one convention), so it was never load
bearing and was never validated. On x86 `__fastcall` is a genuinely
different ABI (first two integer args in ECX/EDX, callee cleans the stack)
while the 32-bit engine uses `__cdecl` - confirmed by decompiling
`libafp-win32.dll`'s `afp_boot` and `libafputils-win32.dll`'s `afpu_boot`,
both of which IDA types as `__cdecl`. A mismatch is not a compile error; it
is a stack imbalance on the first callback.

`AFP_CB` expands to `__fastcall` on `_WIN64` and `__cdecl` otherwise, so
x64 codegen is bit-identical to before and x86 gets the convention the
32-bit engine actually uses.

### Pointer-slot layout: `kEngineSlot` / `SlotOffset`

Structs SHARED with the engine (the `afp_boot` render-params table and the
`afpu_boot` config table) are arrays of function pointers. They were built
with hardcoded x64 byte offsets (`0x08, 0x10, 0x18, ...`). On x86 those
tables are 4-byte strided, so every callback would land in the wrong slot
and the tail of the struct would be past its end.

`BuildStructs` now indexes by SLOT NUMBER through `Support::SlotOffset`,
which multiplies by `sizeof(void*)`. On x64 slot N still resolves to the
same byte offset as before (slot 1 = 0x08, slot 14 = 0x70, slot 36 =
0x120), so this is a no-op for every existing profile. The named
`kSlot*` constants replace the magic offsets.

Two related details in the same structs:

- The render-params header field was written as a `uint64_t` 0x200. It is a
  flags DWORD; the write is now `uint32_t`. On x64 the result is identical
  because the array is memset to zero first.
- The afpu config's near/far pair sat at x64 `0x38`/`0x3C`, i.e. two floats
  packed into the slot following seven pointer slots. They are now written
  as `float[2]` at `SlotOffset(kSlotAfpuNear)`, which lands at 0x38/0x3C on
  x64 and 0x1C/0x20 on x86.

## Engine struct reads: prefer the engine's own API

The DDR path used to enumerate a package's clips by walking afputils
internals from a hardcoded data-segment offset: `afpu_base + 0x48450` for
the package table, an `* 8` stride to index it, then `rec + 0x14` (count),
`rec + 0x50` (clip array), and `arr + i * 40 + 16` / `+ 32` for each clip's
name and stream id. Every one of those is an x64-only number: the strides
assume 8-byte pointers and each field offset shifts once a pointer earlier
in the record shrinks.

That whole walk is now replaced by `afpu_get_afp_info_from_index_at_package`,
a real export present in BOTH the DDR World x64 build and the IIDX 24 x86
build. Enumeration calls it with index 0, 1, 2, ... and stops when it
returns negative (the engine logs "afp index error" past the end and
returns -2). No offsets, no strides, no version coupling.

Its out-parameter is `DdrAfpInfo` = `{u32, u32, void*, const char* name,
u32 stream_id}`, recovered by decompiling the function in both builds. The
x64 build writes dwords at +0/+4, qwords at +8/+16, dword at +24; the x86
build writes five consecutive dwords. Declaring it as a plain C++ struct
makes the compiler produce exactly those layouts on each arch (name at +16
and stream_id at +24 on x64, +12 and +16 on x86), which is why the old
`info + 16` / `info + 24` byte offsets could be deleted rather than
arch-gated. `afpu_get_afp_info_at_package` takes the same struct.

Verified no DDR World regression: `common_background_v3.arc` enumerates the
same six clips with identical names, stream ids and sizes as the offset
walk produced, and still renders.

The two remaining `GetModuleHandleA("libafp-win64.dll")` calls (diagnostic
paths only) now use the `DllLoader`-held module handle that boot already
has, so no DLL name is hardcoded outside the profile table.

## afp requires 16-byte-aligned allocations (the x86 killer)

The DDR/legacy allocator callbacks (`Cb_Alloc` / `Cb_Realloc` / `Cb_Free`,
handed to afp in the render-params table) used `malloc` / `realloc` /
`free`. afp allocates its internal structures THROUGH those callbacks, and
at least its shape structures require 16-byte alignment: libafp's
`afp_shape_init` asserts on `(ptr & 0xF) == 0` before memsetting the
struct, and a failed AVS assert is FATAL - it kills the process.

On x64 this never fires because MSVC's `malloc` guarantees 16-byte
alignment. **On x86 `malloc` guarantees only 8**, so roughly half the
shape allocations land 8-mod-16 and the first one to do so aborts the
process. This is a pure-x86 failure with no x64 symptom whatsoever.

The callbacks now use `_aligned_malloc` / `_aligned_realloc` /
`_aligned_free` with a 16-byte alignment. All three must change together:
memory from `_aligned_malloc` MUST be released with `_aligned_free`, and
since afp routes every allocation through this one triple, the pairing
stays consistent.

How to re-find the requirement on a new build: search libafp for the
`afp-shape.c` assert string, data-xref it, and look for the
`(ptr & 0xF)` test. Note DDR World's 2.13.7 does not even carry the
`afp_shape_init` symbol string, so its absence in one build says nothing
about another.

Verified no DDR World regression: `common_background_v3.arc` still
enumerates its six clips and produces a byte-identical 4332-byte render
after the allocator change.

(`.clang-tidy` gained `corecrt_malloc.h` in the include-cleaner
`IgnoreHeaders` list: `_aligned_*` is declared in that MSVC-internal
header, which arrives via `<cstdlib>`, and the list already covers exactly
this class of CRT/Windows internal header.)

## IIDX 24 (SINOBUZ) status

Profile `iidx24`, dir hint "sinobuz", backend `afp_ddr` (its
libafp-win32 2.13.1t8 export set matches DDR World's libafp-win64 2.13.7
apart from `afp_ctrl` / `afp_render_dump`, neither of which the renderer
uses), 1280x720, `avs_generation = Avs2161`.

WORKING END TO END. The 32-bit renderer loads all three win32 DLLs, AVS
boots (after the 2.16.1 ordinal-map fix, see docs/game_profiles.md),
afp_boot / afpu_boot succeed, the render loop runs at a stable 60 fps, and
the directory scan finds 2467 IFS. `data/graphic/gmframe24.ifs` mounts
through avs imagefs, its package is read by `afpu_ngp_read_data`, and all
33 clips enumerate through the engine API with correct names, stream ids
and sizes (`1p_frame`, `center_frame_sp`, `x_lane_bg_dp`, `x_lightning`,
...). Both `1p_frame` (the 1P lane frame with the ninja motif and the
animated lightning) and `center_frame_sp` render correctly at 1280x720.

Two bugs stood between "boots" and "renders", both x86-only:

1. The avs 2.16.1 ordinal map (see docs/game_profiles.md) - AVS boot
   segfaulted because `property_create` resolved to `node_refdata`.
2. The 16-byte allocation alignment above - the FATAL
   `afp-shape.c:524 afp_shape_init` assert. Note the misleading symptom:
   the log made it look like clip enumeration returned nothing, because
   the process was aborting inside afp before the enumeration result was
   ever logged. Enumeration itself was correct all along.

Residual non-fatal warnings in `avs_out.log` of the form
`W:afpu-package: no image[playm_gauge_normal_1p] in shape[1p_frame_shape64]`
are the game's own: those images live in other IFS the real game has
mounted concurrently, and afp degrades gracefully. They are not an error
in the renderer.

## IIDX 20 (tricoro) - a THIRD engine generation

Profile `iidx20`, dir hint "tricoro", backend `afp_ddr`, 1280x720,
`avs_generation = Avs2158`. WORKING: 24 clips enumerate and both
`1p_frame` (lane, turntable, groove gauge) and `center_frame_sp` render.

tricoro is not just "older 32-bit" - it is a different generation of BOTH
libraries, and needed four separate fixes. Each was found by RE, and each
is detected from the DLL's own exports rather than hardcoded per profile.

### 1. avs 2.15.8 ordinal map

360 exports (vs 390 / 392) with prefix `XCd229cc` - note the prefix is a
per-build SEED, not a library identity: this same prefix is used by modern
afp-core elsewhere. Ordinals shifted again and non-uniformly, so
`kAvsOrdinals2158` is a third table. Recovered by the assert-string method
described in docs/game_profiles.md and cross-checked against the repo's
existing `idc573/avs/2.15.08.idc`.

### 2. avs_boot takes SEVEN args (split heaps)

2.15.8: `avs_boot(config, std_heap, std_sz, avs_heap, avs_sz, log_writer,
log_ctx)` - TWO separate heaps. The 6-arg form used by 2.16+/2.17 puts the
log writer where 2.15.8 expects `avs_sz`. Gated by
`AvsOrdinals::boot_takes_split_heaps`, which also makes the renderer
allocate the second heap.

### 3. AFP 2.10.5t7 has the PRE-UNIFICATION render API

`afp_do_render(dt,type,id)` / `afp_do_display(type,id)` /
`afp_id_is_valid(type,id)` do not exist yet. The originals are the
per-target `afp_render_all(dt)` / `afp_display_layer(id)` /
`afp_layer_is_valid(id)`. Likewise afputils: `afpu_ngp_read(name, path)`
(no flags arg) and `afpu_create_stream_all(pkg)` (1 arg).
`AfpDdrFuncs`/`AfpuDdrFuncs` resolve BOTH spellings and dispatch through
`RenderAll` / `DisplayLayer` / `LayerValid` / `ReadPackage` /
`CreateStreamsForPackage`; `HasSplitRenderApi()` is the generation probe.

### 4. Both engines want a CALLER-SUPPLIED heap

`afp_boot(heap, heap_size, render_params)` - 3 args, where the first two
initialise afp's own pool (the callee aligns the base, memsets it to -1
and registers it). Same for `afpu_boot(config, heap, heap_size)`, whose
heap init logs "afpu heap already setting". The 2.13.x 1-arg/2-arg forms
pass render_params where the heap belongs, so the engine memsets wild
memory. 64 MB each; note the afpu render callbacks arrive via the
separate `afpu_set_render_params`, not through afpu_boot.

### 5. draw_primitive changed signature (the last crash)

The render_params PUBLIC layout is otherwise IDENTICAL between 2.10.5t7
and 2.13.1t8 (2.13 remaps internally into a compacted table, but the
caller-facing offsets match, verified slot by slot). The one exception:

- 2.10.5t7: `draw_primitive(vtx, count, prim_type, attr, a5, a6, c0[4], c1[4], ctx)` - 9 args
- 2.13.1t8: `draw_primitive(vtx, count, params, ctx)` - 4 args

Our callback reads arg 3 as a `params` POINTER; on 2.10 that slot holds an
integer primitive type, so dereferencing it segfaulted on the first frame.
`Cb_DrawPrimitiveLegacy` repacks the 9 args into the 2.13 params block and
forwards. The block layout is `[0]=type [1]=flags [2]=tex [3]=? [4..7]=c0
[8..11]=c1`; that c0/c1 pair is exactly 2.10's two colour args. `a5` was
confirmed to be the texture id EMPIRICALLY (it logs values like 0x9808001,
the AFP texture-id shape, while a6 is always 0) rather than assumed.

Also learned from that mapping pass: `set_mask` takes SIX args in BOTH
builds - the 7-arg description elsewhere counted the format string's
duplicate print of arg1 (once as %d, once as %s through a type-name table).

## IIDX 19 (Lincle) - the TXP2 package path

IIDX 19 ships NO libafputils, so the host has to do what afputils did. It does
NOT reimplement anything afp or avs already provide - every engine operation
goes through the game's own DLLs. See `IIDX/lincle_bmafp_package.md` in the
notes repo for the RE this mirrors (`BMAFP::C_MY_AFP_PACKAGE::package_read`).

Working today, verified against real Lincle packages:

- `avs 2.13.4` boots (its own ordinal table, 7-arg split-heap `avs_boot`,
  ctx-first log writer, u32 `log/level`).
- `afp 2.9.4` boots with the caller-supplied heap; `DdrAfp::Boot` now tolerates
  a missing afp-utils when the afp exposes `afp_stream_create_call`.
- `src/formats/txp2.{h,cpp}` parses the package: big-endian header, the packed
  section run, the AFP stream table, the texture table and the atlas cells with
  their name table. Unit-tested, and validated on real files where
  `texture.offset + texture.size` lands exactly on the file size and the cell
  count matches the cell-name count.
- Packages are opened as PLAIN AVS FILES (`avs_fs_open`), not an IFS. The
  loader mounts the package's own directory at `/pkg` so any `.bin` anywhere in
  the tree can be selected from the UI file list.
- Texture payloads are decompressed by **AVS's own cstream INFLATE**, reached
  through four extra avs ordinals, exactly as `BMAFP::avslz_decode_mem` does.
  The blob is `{BE u32 uncompressed, BE u32 compressed, data}` and a zero
  compressed size means a straight copy.
- `afp_check_src` is applied per stream from the appended byte-order block
  (whose offset equals the core size) BEFORE `afp_stream_create_call`, without
  which afp rejects the blob with "This is not afp data."
- Streams, layers and MC refs are all created through the engine, and afp
  reports real timelines back (42 / 25 / 20 frames for `0200.bin`).
- Textures are uploaded to D3D and `get_bitmap_info` (render-params slot 15 =
  byte 0x3C) is answered from the parsed cells, with half-pixel UVs.

- **Geometry renders.** Most Lincle content never queries `get_bitmap_info`; it
  goes through the SHAPE path instead. `src/formats/txp2.cpp` parses the 0x2000
  section (shape bodies with their positions, uvs, colours, bitmap refs and
  16-byte primitive nodes), `src/afp_ddr_geo.cpp` resolves each bitmap ref
  against the atlas cell tables and rewrites the per-cell UVs into atlas space
  exactly as `package_read` does, and `src/afp_ddr_render_shape.cpp` answers
  slots 0x40 / 0x44 / 0x20. `0200.bin` reports "43 shapes, 43 primitives, 0
  unresolved bitmap refs" and draws the Lincle angel; `0414.bin` (96 shapes)
  draws its song background.

Two decisions worth recording:

- The renderer emits one triangle list per primitive node instead of
  accumulating into bm2dx's 5120-vertex / 512-index batch. The batch size is
  not observable in the output, only in how many draw calls reach D3D.
- The shape id keeps the game's exact `SystemShapeID` packing (bit0 clear,
  bits 1..7 package slot, bits 8.. geometry index + 1) so ids round-trip
  through afp unchanged.

Package RELOAD (picking a second file from the tree) crashed inside libafp until
the lifetime was fixed. `afp_stream_create_call` does NOT copy the afp blob - it
references it in place, which is exactly why `afp_check_src` fixes the bytes up
in place. Replacing the loaded package therefore freed a buffer afp was still
reading, and afp faulted on a dangling pointer.

`BMAFP::C_MY_AFP_PACKAGE::package_free` shows the required order, and
`LoadTxp2` now mirrors it: destroy the LAYERS first
(`afp_layer_is_valid(l) >= 0` then `afp_layer_destroy(l)`, after `play(0)` and
`set_attribute(1, 0)`), then every stream via `afp_stream_destroy_call` (which
returns 0 on success), then release the host's D3D textures, and only then free
the package buffer. Two supporting fixes: the texture-slot allocator now reuses
released slots instead of only ever bumping a counter (a long session would
otherwise exhaust the 4096-slot table and silently stop texturing), and a
process-wide crash reporter (`src/support/crash_report.cpp`) logs the faulting
module and offset for any access violation, which is what identified this one.

A resolution bug surfaced while testing this: `CreateRenderWindowAndDevice`
used to force 1280x720 for every legacy-AFP profile, which silently overrode
both the profile default and `--render-size`. The profile's
`default_render_w/h` is now authoritative unless `--render-size` is passed on
that run, which is what makes Lincle boot at its real 640x480.

## SDVX 1 (BOOTH) - the TXP2 path on a newer engine

Profile `sdvx1`, dir hint "booth", backend `afp_ddr`, 720x1280 portrait,
`avs_generation = Avs2158`, no afp-utils (docs/game_profiles.md has the
profile and where the render size comes from). WORKING: TXP2 packages from
`data/graphics/*.bin` load, every clip enumerates, and they render
(`bg_space.bin` / `bg_space`, `logo.bin`, `game_bg_loop.bin`,
`cmn_game_hud.bin`). Render-regress case `sdvx1`.

### Where it sits between the known generations

| | IIDX 19 (Lincle) | SDVX 1 (BOOTH) | IIDX 20 (tricoro) |
|---|---|---|---|
| libafp | 2.9.4, 152 exports | 2.10.3, 159 exports | 2.10.5t7 |
| libavs | 2.13.4 | 2.15.8 | 2.15.8 (same file) |
| afp-utils | none | none | present |
| packages | TXP2 `.bin` | TXP2 `.bin` | IFS |

SDVX 1's libafp is a strict superset of Lincle's: it adds
`afp_layer_create_with_property`, `afp_layer_mc_refer_by_level_depth_id`,
`afp_mc_mc_listf`, `afp_set_render_params`, `afp_stream_symbol_get_param`,
`afp_stream_symbol_mc_listf` and `afp_system_get_attribute`, and removes
nothing. Against IIDX 24's 2.13.1t8 it is the pre-unification split API
(`afp_render_all` / `afp_display_layer` / `afp_layer_is_valid`, no
`afp_do_*`, no `afp_id_is_valid`), so `HasSplitRenderApi()` picks the
caller-supplied-heap `afp_boot` and the 9-argument `draw_primitive`, both
detected from the exports with no profile flag. The game's own bring-up
(the only caller of the `afp_boot` import in `soundvoltex.dll`) is
`afp_boot(malloc(6 MB), 6 MB, render_params)`, `afp_system_set_attribute(16,
16)`, `afp_set_stream_max_nr(2048)`, `afp_set_policy(1)`.

The game's package reader is the same code as Lincle's
`BMAFP::C_MY_AFP_PACKAGE::package_read` (find it as the only caller of the
`afp_stream_create_call` import; it checks the `TXP2` magic). It opens
`/data/graphics/<name>_j.bin` (or `_k` / `_a` by region) and falls back to
`<name>.bin`. Every SDVX 1 package has flags `0x67fdb`, so its textures are
avslz compressed (`0x40000`) and the byte-order block (`0x20000`) is present.

Two things had to change, both avs 2.15.8 facts rather than SDVX ones
(docs/game_profiles.md, avs boot contract items 4 and 5):

1. `avs_fs_open` read-only is flags 1 on 2.15.8, not 0. With 0 the open
   failed (`avs_fs_open failed for /pkg/bg_space.bin`) even though the mount
   succeeded.
2. The 2.15.8 table had no cstream ordinals, so the compressed textures could
   not be inflated.

### The `.ifs` files in `data/graphics`

`game_hud.ifs`, `game_lane.ifs`, `card_end_j.ifs` and the rest are IFS images
holding afp-utils packages (`afp/afplist.xml`, `afp/bsi/...`). No consumer
found in this build (searched: no `.ifs` or `imagefs` string in any module
except libavs itself; the imagefs getter export, ordinal 330, has no import in
`soundvoltex.dll`; the one `avs_fs_mount` import is used only to mount
`/e_drive`), and the build ships no afp-utils to read them with. The renderer
loads them only through the afp-utils path, so on `sdvx1` they fail to load.

### The first clip of a package could not report its playhead

`--dump-anim-info` reported `"ok": false` for the first clip of every TXP2
package (SDVX 1 and IIDX 19 alike). `RootMc()` caches the root MC id per
layer id. `SwitchClip` back to a clip whose layer already exists calls
`afp_layer_stop` then `afp_layer_play`, and `afp_layer_stop` instantiates a
NEW root MC: `afp_layer_mc_refer` returned `0x18050000` before the stop and
`0x18060000` after it, while `afp_layer_play(0)` and `afp_layer_play(1)` left
it unchanged. The cached id was then dead (`W:afp-access: afp_mc_get_param
afp_mc_id[0x18050000] is invalid.`). `SwitchClip` now drops the cache on that
path. Test: `tests/local/ddr_txp2_live_tests.cpp` (`ddr_local_tests`, label
`local_dll`, 32-bit only, `R573_SDVX1_DIR`) loads `bg_space.bin` and requires a
readable playhead for all four clips; it failed on the first clip before the
fix.

### A package's bytes outlive its streams until the next advance

Loading SDVX 1 packages one after another (the judge's clip list, or picking files
in the renderer) crashed inside libafp on the fifth package with an access
violation in the object constructor. `afp_stream_destroy_call` does not destroy
anything:

```c
int afp_stream_destroy_call(int id) {
    if (!stream_lookup(id)) return -3;
    stream_mark_destroy(id, 1, 1);      /* sets the entry's pending bit */
    afp_flags |= 8;
    return 0;
}
```

and `afp_render_init` runs the real destruction (`if (afp_flags & 8) destroy_pending();`).
Until then the stream is alive and still referenced by the "current render stream"
global that the last advance or display left behind. The next `afp_layer_create`
constructs the new layer's root object, whose constructor
(memsets 188 bytes) asks that current stream for its data to read the AS
version flag (`"afp_play_work_check_as_version"`), and the data points into the
afp blob of the package the renderer had already freed.

`UnloadTxp2Package` now moves the package's bytes into `g_retired_cores` instead
of freeing them, and `DdrAfp::Advance` / the renderer's per-frame advance clear
that list after `afp_render_init` has run. Test: `ddr_reload_local_tests`
(`tests/local/ddr_txp2_reload_live_tests.cpp`, `local_dll`, `R573_SDVX1_DIR`)
loads every SDVX 1 package in turn, playing each for 30 frames; it crashed on
`caution.bin` before the change.

### The interpolation judge

`--judge` runs SDVX 1 clips through the same engine with libafp's per-layer advance
and draw detoured, to draw poses interpolated between authored frames next to the
authored rate. It needs no change to the DDR backend beyond `DdrAfp::LoadTxp2Streams`
(a package without the root layer `LoadTxp2` creates), `DdrAfp::Advance` (the game's
`afp_render` call) and `DdrAfp::DestroyLayer`. docs/judge.md.

### How a frame is produced (afp does the transforms)

The renderer never walks afp's display objects. Per render-loop tick:

1. `render_loop.cpp` `AdvanceFrame` takes `dt` as `1 / render_fps` (the
   renderer setting, 120 by default) or `1 / export fps` while exporting, and
   `RenderOneFrame` hands it to `AfpFamilyBackend::RenderScene` ->
   `DdrRuntime::RenderFrame` -> `DdrAfp::RenderFrame(dt)`.
2. `AdvanceOnce` runs `afp_render_init()`, `afp_render_all(dt * time_scale)`,
   `afp_render_finish()`. That is exactly the body of libafp's own
   `afp_render(dt)`, which is what SDVX 1 calls once per frame with a fixed
   1/60, except that `afp_render` skips all three when `dt <= 0`.
3. Inside `afp_render_all`, each layer scales `dt` by its speed and steps in
   whole-frame chunks of `1 / stream fps`, carrying the remainder in the
   layer's time accumulators, and advances its MC tree once per chunk. A clip
   authored at 60 fps and driven at dt = 1/120 therefore changes pose every
   second tick and holds it in between: afp never produces an in-between pose.
4. `DisplayFrame` calls `afp_display_layer(layer)`. afp walks the display list
   itself, composes each placement's world matrix down the clip chain and calls
   the render-params callbacks: `load_matrix` (2x3, `Cb_LoadMatrix`) or
   `load_matrix44` (`Cb_LoadMatrix44`) with the FINAL composed matrix, then
   `draw_shape` (`Cb_DrawShape`, shape id plus colour) or `draw_primitive`
   (`Cb_DrawPrimitiveLegacy`). The callbacks only set D3D state and draw.

Clips are enumerated from the TXP2 file itself (`Txp2::Parse`'s AFP stream
table, then one `afp_stream_create_call` per entry in `LoadTxp2Package`);
nothing comes from afp-utils, which this generation does not have.

## Still x64-only

The MODERN afp path (`afp_boot.cpp`'s render context / `afpu_data`,
`render_executor.cpp`'s vtable indices, the `uint64_t info[8]` package-info
reads in `afp_packages.cpp` / `afp_anim.cpp` / `qpro_*.cpp`) keeps its
8-byte stride assumptions. It COMPILES on x86 but would not work there.
That is fine today: every modern-afp game (IIDX 26+, SDVX, GITADORA,
jubeat) ships 64-bit DLLs. If a 32-bit modern-afp title ever appears, those
sites need the same slot treatment as `BuildStructs` got, all together.

`render_seh.cpp`'s fault register capture is arch-gated: x64 fills all 16
GPRs, x86 fills the 8 it has and zeroes the rest (`kRegNames` still prints
x64 mnemonics, so the last 8 lines read as zeros on x86).

## Encoding runs in a 64-bit child (573Encoder.exe)

RTX 50 cards (Blackwell) and newer do not run 32-bit CUDA programs; NVIDIA's
driver refuses them on that architecture while RTX 40 and older still accept
them. ffmpeg's `h264_nvenc` / `av1_nvenc` open their session through CUDA
whenever they get plain system-memory frames: `nvenc_setup_device` in
`libavcodec/nvenc.c` only takes its Direct3D 11 branch when it is handed
D3D11 or CUDA frames or a hardware device context, and otherwise calls
`cuInit` and `cuDeviceGetCount`. The renderer feeds BGRA from system memory,
so inside `573Renderer32.exe` the NVENC probe failed on an RTX 5080 and the
export panel said "h264_nvenc unavailable" while the same build worked on an
RTX 4070. The decision record is ADR 0008.

The 32-bit build therefore never encodes in process. `ExportEncoder`
(`src/encode/export_encoder.h`) is the one place that chooses: with
`R573_REMOTE_ENCODER` defined (CMake defines it on `r573_encode_client` for
the 32-bit build, and `r573_app` links that library only there) its `Sink`
is `EncodeClient::Sink` and its `HardwareAvailable` asks the child; on x64 it
is `MediaSink::Sink` and `VideoEncoder::HardwareAvailable` as before. The
export session (`export_internal.h`), the export panel's hardware check and
the Q-pro writers (`qpro_detail.cpp`, `qpro_walk.cpp`) all go through it.

`EncodeClient::Host` (`src/encode/encode_client.*`) starts
`573Encoder.exe` from the renderer's own directory the first time anything
needs it and keeps it for the life of the renderer, so the Q-pro extractor's
hundreds of still AVIFs do not each pay a process start. Calls are serialised
by a mutex. If the child has exited or a call fails, the connection is
dropped and the next call starts a fresh child; a sink that was open on the
dead child gets "no session" from the new one and fails its export with that
message. Hardware probe verdicts are cached per format in the `Host`, failed
ones included, because the export panel asks every frame. The cache has its
own lock, separate from the one that serialises calls, so the panel's
per-frame lookup never waits behind a long encode or flush.

Each sink is one numbered session. `Open` creates a Boost.Interprocess
`windows_shared_memory` section named `Local\r573_encode_<pid>_<session>`
sized for one source frame (`src/encode/frame_section.*`); the child opens it
by name. `SubmitFrame` copies the frame into the section and sends `Frame`;
the child encodes straight from the mapping. The copy uses the sink's own
frame size, never the mapping's, because Windows can round a mapping up to a
page. `windows_shared_memory` is the native Windows mapping, which disappears
when its last handle closes, so a crash on either side leaves nothing behind.
A `Section` is built in place and passed around by `unique_ptr`, never moved:
Boost's `windows_shared_memory` move constructor swaps with a `mode_t`
member it has not initialised, which clang-tidy's analyzer reports as an
uninitialised read.

Control messages are FlatBuffers (`src/encode/encode_host.fbs`) over the same
length-prefixed named pipe as the preview host (`PreviewChannel`, ADR 0007):
`Open` (every `MediaSink::Params` field plus the section name), `Frame`,
`FinishOutput`, `Cancel` and `Probe`, answered by `Opened`
(whether hardware is in use), `Done`, `Probed` or `Failure` (the sink's
`LastError()` text). The finish request is not called `Finish`: flatc
generates a builder class per table, and a table named `Finish` collides
with the builder's own `Finish()` method. Replies have no timeout: a slow
AV1 flush is legitimate, and a dead child closes the pipe, which ends the
wait at once.

`EncodeHost::Session` (`src/encode/host/encode_session.*`) holds one
`MediaSink::Sink` and its section per open session. When the renderer closes
the pipe, the child returns from `main`, and every sink still open is
cancelled by its destructor, which removes partial output.

Tests (`tests/encode/encode_host_tests.cpp`, `ci` label, x64 only because
the child is a 64-bit build): a PNG sequence with a scaled output size and a
VP9 WebM through the real child, a failing `Open` coming back as the sink's
error while the child keeps serving, a missing `573Encoder.exe` named in the
error, and the session answering a probe and refusing an unknown session.
Replacing the child's frame handling with a bare `Done` fails the first case.

## CI

`CMakePresets.json` gains `dev32` / `ci32`. `build-renderer.yml`'s `windows`
job is a MATRIX over `x64` and `x86`: each leg picks its own MSVC dev-cmd
arch, vcpkg triplet, preset, build dir and output name, then builds, runs
the full ctest suite, and uploads its own artefact (`renderer-win64-*` /
`renderer-win32-*`). All 187 tests pass on x86, including the ffmpeg
encoder and D3D pixel-golden suites.

Two details worth keeping:

- The vcpkg binary-archive cache key includes the triplet, so the x64 and
  x86 dependency sets do not evict each other.
- "Verify build output" checks the PE machine field of the produced binary
  (0x8664 for x64, 0x14c for x86), not just its size. A preset or dev-cmd
  arch mix-up would otherwise produce a working-looking build of the wrong
  architecture, which is exactly the failure this whole effort exists to
  avoid.

clang-tidy runs on the x64 leg only (`run_tidy` matrix flag). The tree is
arch-independent apart from the few `#ifdef _WIN64` sites, so running it
twice would just double the slowest CI step.

Each leg stages its artefact into a flat `artefact/` folder (the exe and its
PDB, nothing else), so the zip opens to the files with no `bin/` or build
folders. The x64 leg also checks that `bin/573Encoder.exe` is a 64-bit
executable and stages it next to `573Renderer.exe`. The x86 leg cannot build
it, so the `bundle-win32` job, after both legs, downloads the two artefacts,
copies `573Encoder.exe` and its PDB into the x86 set and re-uploads
`renderer-win32-*` in place (`overwrite: true`); the 32-bit zip then exports
on its own. `preview_host.exe` is only the editor's helper: it reaches the
editor job through the Actions cache, never as an artefact (docs/gates.md).

Releasing is a SEPARATE `release` job gated on `needs: [bundle-win32,
editor]` plus the `v*` tag. It packs both renderers and the one
`573Encoder.exe` they share into `573Renderer.zip`, and the editor into
`573Editor.zip` (docs/ci.md, "Release zips"). Doing it inside the matrix
would have two jobs racing to create the same release.
