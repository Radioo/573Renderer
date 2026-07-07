# Engine binding: Konami DLL interfaces

This document captures the reverse-engineered knowledge behind the renderer's DLL binding layer
(function-pointer tables, export resolution, raw data offsets, MovieClip control, AVS binary XML,
logging). It is the doc-of-record for the comments that used to live in:
`src/afp_funcs.h`, `src/afpu_funcs.h`, `src/avs_funcs.h`, `src/afp_ddr_funcs.h`,
`src/dll_offsets.h`, `src/app_globals.h`, `src/com_ptr.h`, `src/dll_loader.{h,cpp}`,
`src/mc_control.{h,cpp}`, `src/avs_xml.{h,cpp}`, `src/log.{h,cpp}`.

Target DLL versions (facts below are version-specific unless noted):

| DLL | Version | Game | Export prefix |
|---|---|---|---|
| avs2-core.dll | 2.17.00 | IIDX 33 / SDVX 7 era | `XCgsqzn` |
| afp-core.dll | AFP 2.14.18 | IIDX 33 Sparkle Shower | `XCd229cc` |
| afp-utils.dll | AFPU 1.2.19 | IIDX 33 Sparkle Shower | `XE592acd` |
| libafp-win64.dll | AFP 2.13.7 | DDR World (MDX) | readable names |
| libafputils-win64.dll | AFPU (2.13.7-era) | DDR World (MDX) | readable names |
| libavs-win64.dll | AVS 2.15.8-era | DDR World (MDX) | `XCnbrep7` |

Ordinals and enum values are taken from each DLL's export table and IDA analysis
of the shipped versions (afp-core 2.14.18, afp-utils 1.2.19, avs2-core 2.17.00).
Calling convention everywhere: x64 Microsoft (default __cdecl on x64 = __fastcall).

## 1. DLL export naming schemes and DllLoader

Two export-naming schemes exist across Konami DLLs:

- Mangled (avs2-core, modern afp-core / afp-utils, DDR's libavs-win64): every export is
  `<prefix><NNNNNN>` where the shared prefix identifies the library and the 6-char suffix is the
  zero-padded hex ordinal. Examples: `XCgsqzn000129` shape for avs_boot on avs2-core,
  `XCnbrep7000129` on DDR's libavs-win64, `XCd229cc000046` = afp-core ordinal 0x46.
  DllLoader auto-detects the prefix from the first export name and resolves by
  `prefix + %06x ordinal`.
- Readable (DDR World's libafp-win64 / libafputils-win64, AFP 2.13.7): plain names like
  `afp_boot`, `afp_do_render`. Resolved by name; the ordinal argument is ignored.

Detection rule: a DLL is treated as mangled iff its first export name ends in exactly 6 hex
digits (readable AFP names like `afp_boot` do not). `DllLoader::IsByName()` reports which mode
was detected.

The export COUNT is cached as a coarse version fingerprint: different game/build versions of
afp-core.dll have different export counts, so a count mismatch is an early tell that offsets and
ordinals may have moved.

Design rationale: DLL load failures are routed through LOG() rather than printf because the
renderer is a WIN32-subsystem app, so stdout is dropped on the floor. Before that, DLL load
failures (the most likely cause of "renderer crashed at boot" reports) were invisible in the log
file. The `DLL_LOAD(loader, field, ord)` macro passes the field name as the human-readable name
for resolve-failure warnings.

### The proc tables are the complete engine surface

Every engine call resolves through one of the typed structs `AfpFuncs`,
`AfpuFuncs`, `AvsFuncs` (modern) or `DdrAfp`'s accessors (legacy). Each
struct's `Load(DllLoader&)` populates its function-pointer fields once at
boot via `DLL_LOAD`. There are NO ad-hoc `DllLoader::GetFunc(ordinal)`
resolutions anywhere else in the codebase; the P4 seam pass folded the
last stragglers (afpu 0x062
afpu_afp_get_info_in_package, 0x046 afpu_image_lookup, 0x04c
afpu_image_to_stream_args; afp 0x021 afp_stream_play_bitmap_by_name, 0x022
afp_image_stream_create, 0x032 afp_set_filter; avs 0x05c/0x05d/0x05e
opendir/readdir/closedir, 0x068 dump_mountpoint) into their structs. This
is the seam a test uses: populate the structs with C-linkage fakes and the
whole manager layer runs without a Konami DLL.

The `DllLoader` itself is still passed to `AfpManager::Boot` (only) because
boot needs `DllLoader::Module()` - the loaded base ADDRESS - for the
VirtualProtect data-segment patches (section 6), which are raw memory
writes, not function calls. That is a base-pointer need, distinct from the
proc-table surface, so a fake cannot drive the boot-time patch path (it
mutates real DLL memory) but can drive everything downstream.

## 2. afp-core (modern, AFP 2.14.18) - ordinals and semantics

Ordinal table as loaded by `AfpFuncs::Load` (afp-core 2.14.18 export table):

| Ordinal | Function | Signature notes |
|---|---|---|
| 0x000 | afp_set_afp_data | `(data, a2, a3, ctx)` - see below |
| 0x001 | afp_get_afp_data | `()` -> void* |
| 0x002 | afp_boot | `(render_context)` |
| 0x003 | afp_shutdown | `()` |
| 0x005 | afp_set_flag | `(flags, mask)` |
| 0x008 | afp_set_verbose | `(verbose, flags)` - see below |
| 0x00a | afp_set_global_speed | `(float speed)` |
| 0x00b | afp_set_bg_color | `(stream_id, rgb)` |
| 0x00d | afp_do_render | `()` |
| 0x00e | afp_do_update | `(float delta_time, int type, int flags)` |
| 0x00f | afp_render_init | `()` |
| 0x010 | afp_render_destroy | `()` |
| 0x011 | afp_do_sort_render | `(int type, unsigned filter)` |
| 0x013 | afp_set_create_level | `(int level)` |
| 0x014 | afp_get_create_level | `()` |
| 0x015 | afp_stream_control | `(int cmd, stream_id)` - cmd 6 = "refer" an mc work |
| 0x018 | afp_stream_create | `()` -> stream_id |
| 0x019 | afp_stream_set_data | `(stream_id, afp_data)` |
| 0x01a | afp_stream_get_work | `(stream_id)` -> work ptr |
| 0x01d | afp_set_stream_nr | `(int max_streams)` |
| 0x01e | afp_stream_play | `(data_id, data_ptr, unk1, unk2)` |
| 0x01f | afp_stream_get_name | `(stream_id)` -> const char* |
| 0x020 | afp_stream_destroy | `(type, selector, schedule)` - see below |
| 0x02a | afp_stream_set_speed | `(stream_id, float speed)` |
| 0x02c | afp_stream_set_matrix | `(stream_id, float* mat2d)` |
| 0x02d | afp_stream_get_matrix | `(stream_id, float* mat2d)` |
| 0x02e | afp_stream_set_translate | `(stream_id, float x, float y)` |
| 0x037 | afp_set_flag_mask | `(stream_id, flags, mask)` - see below |
| 0x043 | afp_system_dump_layer_info | `(min_priority)` - see below |
| 0x044 | afp_data_get_info | `(data, info_out)` |
| 0x045 | afp_data_get_stream_info | `(data, index, info_out)` |
| 0x046 | afp_get_layer_info | `(layer_id, info_buf[60])` - see below |
| 0x047 | afp_get_data_id_by_name | `(name)` |
| 0x04b | afp_get_layers_by_nr | `(category, out_buf, max_out)` - see below |
| 0x066 | afp_mc_get_id_by_path | `(stream_id, path)` -> mc_id |
| 0x069 | afp_mc_get_relative_id | `(mc_id, direction)` - dir 6 = next sibling; full enum in section 7 |
| 0x06e | afp_mc_attach_stream | `(mc_id, data_id)` - see below |
| 0x071 | afp_mc_control | varargs op dispatcher - see below |
| 0x072 | afp_mc_set | varargs - label getters live HERE - see below |
| 0x073 | afp_mc_get | varargs property setter - see below |
| 0x079 | afp_mc_enumerate_children | `(mc_id, buf, buf_size, flags, out_ptr)` - see below |
| 0x086 | afp_ext_command | `(cmd, arg)` |
| 0x087 | afp_play_work_load_bitmap | `(mc_id, bitmap_name, attach)` - NAMED package bitmap |
| 0x088 | afp_play_work_load_image | `(mc_id, image_info, attach)` - RAW image - see below |

Additional typedefs that exist but are not currently loaded: afp_set_priority_offset
`(stream_id, offset)`, afp_get_flag `()`.

Load-gate (functions considered critical): afp_boot, afp_shutdown, afp_render_init,
afp_do_update, afp_do_sort_render, afp_stream_create, afp_stream_play.

### afp_set_afp_data (0x000) - per-game arg count

The 4-arg form matches what soundvoltex.dll calls in the SDVX 7 live trace:
`(callback_table, 0, 0x320, heap_ctx)`. IIDX 33 uses fewer args (the function reads only what it
needs from RCX), but x64 fastcall lets the binding declare the extras safely - they travel in
RDX / R8 / R9 and IIDX's function just does not read them. Callers pass 0 / nullptr for the
extras when not on SDVX.

### afp_set_verbose (0x008) - per-game arg count

2-arg form per the SDVX 7 live trace: `(verbose=1, flags=0x10000)`. IIDX likely reads only RCX;
passing flags=0 keeps IIDX behaviour while letting the SDVX caller supply the real value.

### afp_stream_destroy (0x020)

`afp_stream_destroy(type, selector, schedule)` - verified against afp-core `XCd229cc000020`.

- `type` picks what to destroy: 1 = every stream; 5 = one stream by id; 7/8 = streams matching a
  byte at stream+36.
- `selector` is the stream id for type=5, otherwise the byte filter.
- `schedule`: 0 = destroy immediately; 1 = set the destroy-later flag (bit 0x2000 at stream+8) so
  AFP cleans it up on the next tick.

Note: afp_stream_destroy leaves MovieClip children behind; the proper release path is
afpu_package_control cmd 6 (see section 3).

### afp_get_layers_by_nr (0x04b)

`(category, out_buf, max_out)`: iterates Table A, filters by
`stream->+14 == category`, writes stream_ids into `out_buf[0..min(count,max_out))`, returns the
TOTAL match count. It does NOT walk Table B (bitmap streams). Useful for counting live Table A
streams to verify destroy-sweeps work as expected.

### afp_get_layer_info (0x046)

`afp_get_layer_info(layer_id, info_buf[60])` - afp-core `XCd229cc000046`. Fills a
60-byte info struct. Accepts a LAYER id (0x1xxx / 0x2xxx), NOT a stream id (0x3xxx) - the internal
id lookup rejects stream tags. Relevant fields:

- `info+4` (u32): flags dword. Top-nibble bits observed empirically on IIDX 33 afp-core 2.14.18 by
  running exp00.ifs (a one-shot 10-tick animation):
  - `0x10000000` = "stream currently in a layer slot" - set on every live layer from the moment
    afp_stream_play returns until destroy.
  - `0x20000000` = "stream has finished its scripted timeline" latch. Set the moment the frame
    tick would otherwise advance past the last authored frame. Never cleared by afp-core; a fresh
    afp_stream_play on a replacement layer starts with it back at 0. This is the bit polled to
    drive "auto-loop on end of animation".
  - `0x40000000` = "timeline wrapped to frame 0" latch (per the decompilation of the afp-core
    frame-advance routine).
    Only set for content that natively loops (e.g. authored with a loop instruction). One-shots
    like exp00 never set it because they stop rather than wrap. Check
    `0x20000000 | 0x40000000` together to cover both cases.
- `info+14` (u16): source stream id (0x3xxx).
- `info+20` (u32): frame period.
- `info+52` (u32): current frame tick.
- `info+56` (u32): monotonic frame-change counter.

### afp_system_dump_layer_info (0x043)

`(min_priority)`: logs all Table A streams with priority >= arg. Priority = the byte at
stream+36 = create_level at play time.

### afp_mc_attach_stream (0x06e)

`afp_mc_attach_stream(mc_id, data_id)` - afp-core `XCd229cc00006e`. Mounts a clip stream
(`data_id` = `(u32)info[3]` from afpu_afp_get_info_in_package) onto a movie clip as a child
instance. bm2dx's CLayer item-clip mount uses this to put the qpro item's hand
clip onto the avatar's qp_hand_l_neutral layer, so the item's per-frame HSL (the rainbow) plays
in the composite.

### afp_mc_get (0x073) - varargs MovieClip property setter

`afp_mc_get(mc_id, prop_id, arg)`. The variadic 3rd arg fits in r8 (x64) and is interpreted
per-op:

- `0x1003` = set_xscale_yscale: arg = pointer to `{float x, float y}`. Verified in the afp-core
  scale-property handler - writes to MC work +272 (_xscale) and +276 (_yscale), then flags
  `+32 |= 0x40000` and `+36 |= 0x8` for re-render.
- `0x1007` = visible: arg = int (0/1).
- `0x101E` = invalidate: arg = int (1 to force re-evaluation of the MC's transform after a
  property change like _xscale).

The binding types the arg as `intptr_t` so callers can pass either an integer (cast in) or a
pointer to a stack-local struct - the dispatch reads r8 the same way regardless.

### afp_mc_set (0x072) - where the LABEL getters live

`afp_mc_set(mc_id, code, ...)` - afp-core `XCd229cc000072`. Despite the "set" name this is where
the label getters live (the label codes in afp_mc_get are stubbed to return -2; these are real).
Codes:

- `0x101F` = label count: `afp_mc_set(mc, 0x101F, int* out_count)`.
- `0x1020` = label name + frame: `afp_mc_set(mc, 0x1020, int index, const char** out_name,
  int* out_frame)` -> 0 on success.

The mc work must be "refered" first via `afp_stream_control(6, mc_id)` or it returns -4.
Verified identical in SDVX 7 and IIDX 33 afp-core; the IIDX AFP debug viewer (CAfpViewerScene)
uses exactly this.

### afp_mc_control (0x071) - op dispatcher, label and frame forms

`afp_mc_control(mc_id, op, ...)` - afp-core `XCd229cc000071`, varargs
(`__int64(__int64 mc_id, unsigned int op, ...)`). Op codes:

| Op | Meaning | 3rd arg |
|---|---|---|
| 0xF02 | goto + play (by frame) | int frame |
| 0xF03 | goto_play_label | const char* label |
| 0xF04 | goto + stop (by frame) | int frame |
| 0xF05 | goto_stop_label | const char* label |
| 0xF08 | deep goto + play (by frame, recursive root + children) | int frame |
| 0xF09 | deep_goto_play_label (recursive) | const char* label |
| 0xF0A | deep goto + stop (by frame, recursive) | int frame |
| 0xF0B | deep_goto_stop_label (recursive) | const char* label |

bm2dx uses 0xF09 deep_goto_play_label to kick the fcombo animation from its "in" label - see
`BM2D::CMovieClip::SetFrameLabel` in bm2dx.

Frame-op internals (verified): for the frame ops the export reads the 3rd vararg as a QWORD and
truncates to u32 for the target frame. Case 0xF08 -> the frame-clamp helper
(called as `(work, (unsigned int)frame)`) clamps frame >= total to total-1, then the deep-goto
worker performs the deep goto. On x64 the 3rd
arg sits in r8 for BOTH `int` and `const char*`, so reinterpret-casting the loaded 0x071 pointer
to an `(int mc_id, uint32_t op, int frame)` signature is ABI-sound - the handler masks to 32
bits. This is exactly how the binding creates `afp_mc_control_frame`: the SAME 0x071 pointer,
reinterpret_cast in Load, NOT a second GetFunc.

The renderer's SEEK uses op 0xF08: the CAfpViewerScene LEFT/RIGHT seek wraps the bm2dx seek
dispatcher -> SetFrame with a3=1, a4=1 -> op 0xF08. The frame is treated UNSIGNED by afp (a
negative wraps huge and then clamps to total-1), so callers must low-clamp to 0 themselves. See
the bm2dx AFP debug viewer's SEEK/STEP handling.

### afp_mc_enumerate_children (0x079)

`afp_mc_enumerate_children(mc_id, buf, buf_size, flags, out_ptr)` - afp-core `XCd229cc000079`
(worker: the child-walk routine, verified in IIDX 33 afp-core). Walks the named child movie
clips of mc_id, writes their names into `buf`, and publishes a pointer to the populated header
into `*out_ptr`. Header layout (aligned to 4 inside buf):

- u16 @ +0: WRITTEN count (names actually stored).
- u16 @ +2: TOTAL count (children seen; may exceed written if buf ran short).
- char* @ +8 + 8*i: pointer to child i's name string (8-byte stride).

`flags`: bit0 = recurse into nested clips; bit1 = emit FULL "parent/child" paths (each name then
resolvable by afp_mc_get_id_by_path). The CAfpViewerScene F3 overlay passes flags=3.

Returns: 0 ok; 0xFFFFFFFD (-3) null/too-small buf; 0xFFFFFFFC (-4) bad mc; 0xFFFFFFFB (-5)
truncated-but-valid (written < total). Callers should accept {0, -5}. See
the bm2dx AFP debug viewer's F3 DISP MC handling.

### afp_play_work_load_image (0x088) vs load_bitmap (0x087)

`afp_play_work_load_image(mc_id, image_info, attach)` - afp-core `XCd229cc000088`.
Binds a RAW image descriptor to the movie clip's bitmap, whereas 0x087
(afp_play_work_load_bitmap) looks up a NAMED package bitmap in the IFS dictionary. SDVX's
submonitor bg uses 0x088 to attach a loose, runtime-decoded subbg_*.png frame to the
subbg_usr/bg_usr placeholder layers (loose pngs are NOT in the package dictionary, so 0x087
cannot reach them).

The two ops converge on the SAME final bind: the 0x088 worker builds a 16-byte
descriptor from image_info via the descriptor builder, then the bind-store
routine stores it in the work's afp property (23,81) and redraws.

`image_info` is a 32-byte struct afp reads as:

| Offset | Type | Meaning |
|---|---|---|
| +0x00 | u32 | texid (afp tex_ref; the renderer backend's legacy resolve maps tex_ref -> `g_textures[(tex_ref & 0xFFFF) + 1]`, so pass slot-1) |
| +0x04 | u16 | width |
| +0x08 | u16 | height |
| +0x0C | f32 | u0 |
| +0x10 | f32 | u1 |
| +0x14 | f32 | v0 |
| +0x18 | f32 | v1 (UVs are 0..1 fractional) |

`attach != 0` makes afp validate that the mc has a non-zero authored size. NOTE on the attach
value: the afp_funcs.h note said "we pass 1 to mirror the game", but the actual mc_control
helpers (BindImageToMc / BindClipImages, mirroring soundvoltex.dll SdvxSubmoniBg_Load's
bind loop) pass attach=0 "per the game". The shipping code path uses attach=0; treat the
"pass 1" note as superseded unless re-verified.

**Re-pointing a clip's bitmap to a COMPANION package's image** (the qpro body-equip pattern):
`SwapClipBitmapFromCompanion(pkg_id, stream_id, clip_name)` reproduces bm2dx's body-piece
binder. It resolves the piece bitmap from the companion package by name (afpu image lookup ord
0x046 into a 64-byte image_info, then image-to-stream-args ord 0x04c into a 40-byte descriptor)
and re-points the whole clip sibling chain: find the clip via afp_mc_get_id_by_path, then for
each sibling (walk with afp_mc_get_relative_id(mc, 6)) call afp_play_work_load_image(mc,
descriptor, 0) and invalidate with afp_mc_get(mc, 0x101E, 1). This is how the game equips a
body: each torso/limb clip (qp_body_f/b, qp_arm_*, qp_leg_*) adopts the companion bitmap's own
size and anchor, so pieces of any size (including empty placeholders and oversized pieces) bind
correctly without touching a shared atlas. bm2dx's own binder walks the same chain and calls
the same ords; the descriptor source is the companion package's afp data id.

### afp_set_flag_mask (0x037)

`afp_set_flag_mask(stream_id, flags, mask)`: ORs `flags & mask` into the stream's flag byte at
stream+8. bm2dx uses two bits:

- bit 0x001 = "timeline advancing" (goes from 0 to 1 to kick animation).
- bit 0x200 = "main stream" (set on the master at stream_play time).

## 3. afp-utils (AFPU 1.2.19) - ordinals and semantics

Ordinal table as loaded by `AfpuFuncs::Load` (afp-utils 1.2.19 export table):

| Ordinal | Function |
|---|---|
| 0x000 | afpu_boot `(config, afp_data)` |
| 0x001 | afpu_shutdown |
| 0x003 | afpu_set_flag `(flags, mask)` |
| 0x005 | afpu_set_config `(type, ...)` |
| 0x01a | afpu_ngp_mounttable_load |
| 0x01b | afpu_ngp_mounttable_load_from_property `(prop)` |
| 0x01c | afpu_ngp_packages_exist |
| 0x01e | afpu_package_get_count |
| 0x01f | afpu_package_get_name_by_index `(index)` |
| 0x020 | afpu_package_find_by_name `(name)` |
| 0x022 | afpu_ngp_read_local `(name, path, flags)` |
| 0x023 | afpu_ngp_read_by_mounttable |
| 0x026 | afpu_ngp_detect_format |
| 0x034 | afpu_get_loaded_package_count |
| 0x036 | afpu_package_control `(cmd, pkg_id, reserved)` - see below |
| 0x037 | afpuloc_get_package_id `(name)` |
| 0x038 | afpuloc_get_first_package_id |
| 0x03a | afpu_package_open_streams `(pkg_id)` |
| 0x03b | afpuloc_package_has_animation `(pkg_id)` |
| 0x03e | afpuloc_get_version_string `(pkg_id, sel)` - see below |
| 0x03f | afpuloc_get_package_info `(pkg_id, sel)` - see below |
| 0x040 | afpuloc_get_texture_info_by_id `(tex_id, info_out)` |
| 0x042 | afpuloc_get_texture_data_size `(tex_id)` |
| 0x043 | afpu_image_find `(name)` |
| 0x045 | afpu_image_get_info `(name, info_out)` |
| 0x068 | afpu_package_dump (debug) |
| 0x070 | afpu_render_init `(render_context)` |
| 0x071 | afpu_render_reset |
| 0x072 | afpu_render_info |
| 0x074 | afpu_render_flush |
| 0x079 | afpu_texture_get_bpp `(format_id)` |
| 0x07e | afpu_ext_command `(cmd, arg)` |
| 0x07f | afpu_get_context |

Load-gate: afpu_boot, afpu_shutdown, afpu_render_init, afpu_ngp_read_local,
afpu_package_open_streams.

### afpuloc_get_package_info (0x03f)

`afpuloc_get_package_info(pkg_id, sel)` - afp-utils `XE592acd00003f`. Returns a
packed version DWORD for the loaded package. `sel`: 1 = converter, 3 = package, 4 = afp (each a
u32 read from the package info at +120 / +124 / +128 respectively). The DWORD packs the version
as major = bits 16-31, minor = bits 8-15, patch = bits 0-7 (bm2dx's CAfpViewerScene prints
`%d.%d.%d` = HIWORD, BYTE1, (u8) - verified in the bm2dx version-print routine). Returns 0 when the
package id fails the resolver gate (which requires the
`(id & 0x78000000) == 0x30000000` tag that the renderer's g_pkg_id carries). See
the bm2dx AFP debug viewer's [FILE INFO] panel.

### afpuloc_get_version_string (0x03e)

`afpuloc_get_version_string(pkg_id, sel)` - afp-utils `XE592acd00003e`. Returns a
const char* engine-name string for the package (a table-stored char* via the string-table
lookup).
`sel` = 2 returns the converter engine name (null when the engine byte at info+42 is 0xFF).
bm2dx pairs this with the converter version triplet: `converter %d.%d.%d(%s)` with "???" as the
null fallback.

### afpu_package_control (0x036)

`afpu_package_control(cmd, pkg_id, reserved)` - per-package lifecycle op. Decoded from afp-utils
`XE592acd000036`; matches the call pattern of the bm2dx scene-teardown routine.

- cmd == 6 with pkg_id = some id: unload that specific package.
- cmd == 6 with pkg_id = 0x78000000: unload EVERY loaded package (sentinel value).
- other cmds filter a locale-priority table (not used by the renderer).

This is what properly releases MovieClip children left behind by afp_stream_destroy.

## 4. avs2-core (2.17.00) - ordinals and the property (binary XML) API

Ordinal table as loaded by `AvsFuncs::Load` (avs2-core 2.17.00 export table; the five property
ordinals marked * were additionally confirmed via afp-utils.dll's import table against the same
shipped avs2-core.dll):

| Ordinal | Function |
|---|---|
| 0x02f | avs_gheap_allocate `(flags, size, tag)` - see below |
| 0x031 | avs_gheap_free `(ptr)` |
| 0x048 | avs_fs_addfs `(filesys)` |
| 0x04b | avs_fs_mount `(mountpoint, fsroot, fstype, options)` |
| 0x04c | avs_fs_umount `(mountpoint)` |
| 0x04e | avs_fs_open `(path, flags, mode)` |
| 0x04f | avs_fs_lseek `(desc, offset, whence)` |
| 0x051 | avs_fs_read `(desc, buf, size)` |
| 0x055 | avs_fs_close `(desc)` |
| 0x062 | avs_fs_fstat `(desc, stat_buf)` |
| 0x090 | property_create `(flags, buf, size)` |
| 0x091 | property_destroy `(prop)` |
| 0x094 | property_insert_read * (drives the parse) |
| 0x0a1 | property_search `(prop, node, path)` |
| 0x0a2 | property_node_create `(prop, parent, type, name, ...)` |
| 0x0a6 | property_node_traversal * (walk tree) |
| 0x0a7 | property_node_name * (node name -> buf) |
| 0x0af | property_node_refer `(prop, node, path, type, data, size)` |
| 0x0b0 | property_read_query_memsize * (size query, short) |
| 0x0b1 | property_read_query_memsize_long * (size query, long) |
| 0x0b2 | property_psmap_import `(prop, node, psmap)` |
| 0x129 | avs_boot `(config_node, heap_buffer, heap_size, log_callback, log_userdata, extra)` |
| 0x12a | avs_shutdown |
| 0x12d | avs_is_active |
| 0x158 | avs_filesys_imagefs |
| 0x170 | log_boot `(config)` |
| 0x17b | log_body_warning `(tag, fmt, ...)` |
| 0x17c | log_body_info `(tag, fmt, ...)` |
| 0x17d | log_body_misc `(tag, fmt, ...)` |

Load-gate: avs_boot, avs_shutdown, avs_fs_mount, avs_fs_addfs, avs_filesys_imagefs,
property_create.

### avs_fs_mount options arg

The 4th arg is the AVS mount-options string (e.g. `"vf=0,posix=1"`). The AVS API treats it as
`void*` internally but every caller in the renderer passes a `const char*` literal; the binding
uses the precise type so C++20's stricter const-correctness accepts string literals without
casts.

### avs_gheap_allocate - 3 args, not 2 (burned by this)

`avs_gheap_allocate(0, size, 0)` - signature verified via the decompilation of the bm2dx
allocation wrapper: THREE
args. The first and third are flags/tags the normal bm2dx caller leaves zero (internally AVS
probably treats them as an allocation strategy hint and a debug/heap-tracking tag; zero =
"default + untagged"). A 2-arg typedef is silently wrong on x64: with fastcall, `size`
lands in rcx (the first-arg slot) instead of rdx, and `alignment` in rdx instead of r8,
corrupting the call.

### Binary-XML read chain (the pattern afp-utils itself uses)

This is the exact chain afp-utils' own xml loader (the `afpu-ngp` xml loader)
performs. All signatures verified against the raw disassembly.

1. Open the file: `fd = avs_fs_open(path, 1, 420)`.
2. Query how many bytes the property tree will need:
   `memsize = property_read_query_memsize(avs_fs_read, fd, &node_count, &reserved)`.
   Re-seek fd to 0 (the query consumed the stream). If memsize <= 0 or node_count > 0xFFFF, fall
   through to the `_long` variant (which takes an extra 40-byte / 10-DWORD scratch buffer) that
   handles files too big for the short path; re-seek again after it.
3. Allocate `memsize` bytes as the property-tree heap.
4. `tree = property_create(flags, heap, memsize)` - flags 0x11 on the short path, 0x1011 on the
   long path (bit 0x1000 differentiates them internally). See section 8 for the flag meanings.
5. `property_insert_read(tree, NULL /*parent*/, avs_fs_read, fd)` drives the parse; AVS calls the
   reader repeatedly until the stream is exhausted. Returns > 0 on success.
6. `avs_fs_close(fd)`. The tree now owns a fully materialized DOM.

The reader is a classic fd-style callback: `int reader(int fd, void* buf, int size)` - exactly
avs_fs_read's shape. Any compatible reader can be plugged in if the data is not on the AVS
filesystem.

property_insert_read register-level signature (verified against the afp-utils afpu-ngp xml
loader): rcx = tree pointer, rdx = 0 (root node), r8 = reader function
pointer, r9d = fd zero-extended (the ctx is treated as an int by AVS and passed directly as the
reader's first arg). The reader is called repeatedly with `(ctx, buf, size)` until it returns
<= 0 (EOF / error). Return: > 0 success, <= 0 failure.

### property_node_traversal directions - two conflicting notes

Two different direction mappings were documented in the code and MUST be reconciled against
the avs2-core E_PROPERTY_TRAVERSE enum when next touched:

- avs_funcs.h claimed: 0 = first
  child, 1 = next sibling, 2 = parent, 3 = previous sibling.
- avs_xml.h (this is the one exercised by working
  code - TRAVERSE_NEXT_MATCH = 7 is used successfully by the shipping XML walker):
  0 = PARENT, 1 = FIRST_CHILD, 2 = FIRST_ATTR, 3 = FIRST_SIBLING, 4 = NEXT_SIBLING,
  5 = PREV_SIBLING, 6 = LAST_SIBLING, 7 = NEXT_MATCH (next sibling with the same tag name),
  8 = LAST_MATCH.

Treat the avs_xml.h enum as authoritative (it is verified by use); the avs_funcs.h comment is
likely a stale paraphrase.

## 5. DDR World (AFP 2.13.7) - libafp-win64 / libafputils-win64

These tables are the 2.13.7 equivalent of the modern afp/afpu tables, NOT a substitute: they
drive DDR's own DLLs, resolved BY NAME (readable exports; DllLoader auto-detects this scheme).
Signatures were RE'd from gamemdx's API usage plus libafp/libafputils decomp.

Load-gate (afp): afp_boot, afp_do_render, afp_do_display. afp_mc_op is deliberately OPTIONAL
(like afp_layer_set_attribute) - seek fails soft if a future libafp build drops the export, so
it is kept OUT of the return gate. Load-gate (afpu): afpu_boot, afpu_set_afp_render_params,
afpu_set_render_params.

### Core loop

- `afp_boot(render_params)`: render_params is the named callback table (AfpDdrRender).
- `afp_do_render(dt, type=3, id=0)` advances everything; `afp_do_display(type=1, id=0)`
  depth-sorts and draws.
- `afp_stream_do_create(data_id, a2, a3)` makes a stream from loaded package data.

### afp_system_set_attribute - TWO args (burned by this)

`afp_system_set_attribute(mask, value)`: the global system-flags word becomes
`value | (flags & ~mask)`. It takes
TWO args - calling it with one leaves `value` as a garbage register, polluting the global system
flags. Those flags are read by afp_layer_create_with_property, the draw routine,
and the shape creators; bit 1 gates the set_mask stencil clip.

The afputils twin has the same trap: `afpu_system_set_attributes(mask, value)`: the afputils
global becomes `value | (*g & ~mask)` (libafputils). One-arg calls leave `value` garbage,
polluting the afputils global that governs shape/stream interpretation.

### Layers

- `afp_layer_create_with_property(stream_id, path, 0, 0) -> layer_id`.
  Creates a visible layer instance of a stream; the LAYER is what afp_do_render/afp_do_display
  actually iterate. `afp_id_is_valid(5, id)` checks a layer id. `afp_layer_set_priority(layer,
  prio)` sets draw order.
- `afp_layer_set_attribute(layer_id, mask, value)` (flags live at layer+0;
  default flags from create = 0x1000001F): clears the `mask` bits in the layer flag word and
  sets the `value` bits. gamemdx's BM2D layer-attach calls
  `(layer, 0x200, 0x200)` on backgrounds; bit 0x200 gates a per-node transform step in the
  advance (afp_advance_play_data: `if (node_flags & 0x210)` calls the per-node transform
  routine).
- `afp_layer_mc_refer(layer_id, "/") -> root mc id`, for per-clip control.
- `afp_layer_stop(layer_id)` = afp_movie_work_free: frees the layer's
  play-data and RE-INITIALIZES it to frame 0 with the SAME stream (calls the layer-init
  routine). Effectively a rewind-to-frame-0 that keeps the texture streams - used to
  loop a background's authored section. The real game loops e.g. background_0009 over subpos
  10..132 and never free-runs into the frame ~316 "swipe" outro.
- `afp_layer_play(layer_id, rate)`: sets the layer's play rate (layer+44)
  and clears the stopped flag. afp_layer_stop leaves rate = 0 (frozen), so after a stop-rewind
  call `afp_layer_play(layer, 1.0f)` to resume normal 1x playback.
- `afp_layer_get_info(layer_id, out_info, 0)`: fills out_info (>= 60 bytes). Fields used:
  `out_info[1]` (+4) = layer flags - bit 0x40000000 is set on the frame the ROOT timeline wraps
  back to frame 0 (afp_advance_play_data). `out_info[6]` = frame divisor,
  `out_info[13]` = current sub-frame position.

### afp_mc_get_param - MovieClip queries

`afp_mc_get_param(mc_id, code, ...)`: queries a movie-clip property by code; the code indexes a
per-code dispatch table by `code - 0x1000`. mc_id comes from afp_layer_mc_refer. Loop-relevant codes (RE'd
from their embedded `pw_get_*` error-string names):

| Code | Internal name | Meaning |
|---|---|---|
| 0x1010 | pw_get_curent_frame | current timeline frame - the drift-free clock |
| 0x1011 | pw_get_total_frame | the layer "wrapmod" sub-frame divisor - NOT the authored loop length |
| 0x1012 | pw_get_label_frame | args: `const char* name, int* out` - frame of a named label |
| 0x1013 | pw_get_loop_count | builtin loop counter; stays 0 for DDR idle loops, which are authored as a gotoAndPlay back to "loop" rather than via afp's own loop |
| 0x101F | pw_get_label_nr | label count |
| 0x1020 | pw_get_label_name | by index -> frame + a NUMERIC name token (not text) |

The authored VISIBLE loop length is no single field: it is (current_frame wrap point minus the
"loop" label frame), read live during playback. The movie clip is still the right object to read
the timeline from - afp_layer_get_info reads the LAYER (a container whose integer frame stays
0), not the clip. Because 0x1020 returns a numeric token rather than text, query-by-name via
0x1012 is the reliable path for known label names.

### afp_mc_op - MovieClip op dispatcher

`afp_mc_op(mc_id, op, ...)`. Op 0xF08 (decimal 3848) = afp_mc_deep_goto_play(frame): seek +
resume play, recursing into child clips (DDR's bg_root has children). The frame is the FIRST
vararg (read as a 32-bit int) and is clamped to [0, total-1] internally. This is the SAME
op-code family the modern afp-core path drives via afp_mc_control (section 2, ord 0x071); it
CONTRADICTS an earlier "no goto-label API" assumption - the dispatcher is
present, just renamed (modern afp-core mangles it as afp_mc_control / ordinal 0x071). The
binding creates a typed frame overload by reinterpreting the same pointer (on x64 the frame
lands in r8 for both the varargs and typed forms and the handler masks it to a DWORD - identical
to the modern afp_mc_control / afp_mc_control_frame pair). Label ops take a `const char*`
instead.

### afp_stream_get_info - version word trap

`afp_stream_get_info(stream_id, out_info)`: `out_info+2` (u16) is the AFP DATA FORMAT VERSION
word (= `*(stream_data + 12)`; it reads 512 = 0x200 for EVERY clip - the raw parser
gates it with `if (ver < 256) "afp data version ... not supported"`), NOT a
frame count. Only `out_info+16` / `out_info+18` (u16 width/height) are usable from this struct.
There is NO timeline-length field on it: afp's frame count is
`afp_mc_get_param(mc, 0x1011)` (pw_get_total_frame), and the authored VISIBLE loop is the
current_frame (0x1010) wrap versus the "loop" label (0x1012).

### libafputils specifics

- `afpu_do_create_stream_all(a1, a2)`: turns loaded package(s) into streams (the content-load
  entry; exact args to be refined when wiring more content).
- `afpu_get_afp_info_at_package(out_info, data_id, clip_name)`: finds the afp clip in the
  package whose name == clip_name (strcmp via avs `XCnbrep70000e4`) and fills out_info - path at
  +16, stream_id at +24 (out+24 = clip+32, out+16 = clip+16). CRITICAL: the THIRD arg (r8,
  clip_name) is real - Hex-Rays shows only 2 args, but the wrapper does `mov r15, r8` and the
  walk compares each clip name against it; passing garbage there faults. Mirrors the gamemdx
  BM2D layer-attach, which forwards its own path arg through r8.
- `afpu_get_texture_bind_id(afp_tex_id) -> bind slot index` (the order the create_texture
  callback returned ids in). draw_primitive's `params[2]` is an afp texture id that MUST be run
  through this to index the renderer's texture array.

## 6. Raw data offsets (dll_offsets.h)

Version-specific module-relative offsets discovered via IDA. afp-core.dll = AFP 2.14.18 (IIDX 33
Sparkle Shower); afp-utils.dll = AFPU 1.2.19. When DLL versions change, update these in
dll_offsets.h (single source of truth). Each constant is a data-segment RVA (relative to the
DLL image base); when a DLL version changes, re-derive each from the role described below.

afp-core.dll:

| Constant | RVA | Meaning |
|---|---|---|
| kCallbackTable | 0xE0E08 | callback table (35 qwords, installed by afp_set_afp_data) |
| kNearFarSlot | 0xE0E70 | slot 13, get_near_far callback (kCallbackTable + 0x68) |
| kRenderFlags | 0xE1134 | render flags dword (bit 0x800 controls the afp_set_afp_data path) |

afp-utils.dll:

| Constant | RVA | Meaning |
|---|---|---|
| kDataStruct | 0x281F0 | dispatch data struct (35 qwords: [0] = flags, [1..34] = dispatch fns) |
| kCmdBufUsed | 0x28810 | command buffer "used" byte count |
| kCmdBufSize | 0x287FC | command buffer capacity |
| kRenderContext | 0x28880 | stored render context pointer (set by afpu_render_init) |
| kSetScreenRectFn | 0x18550 | set_screen_rect internal function |

## 7. MovieClip control helpers (mc_control)

Thin wrappers over afp_mc_get / afp_mc_get_relative_id / afp_play_work_load_bitmap that mirror
bm2dx's pattern: resolve a named clip, then iterate its sibling chain applying the operation to
every sibling (safety cap 64 per chain). The stream_id is the one returned from
afp_stream_play; clip paths are relative to the stream root (e.g. "coin", "paseli",
"m_paseli").

Standard SWF MovieClip property IDs used by the variant system:

- `0x1007` = _visible.
- `0x101E` = invalidate / refresh-dirty bit.

afp_mc_get_relative_id direction values (names follow afp-core's internal traversal):

| Dir | Meaning |
|---|---|
| 0 | containing timeline (nearest ancestor "clip") |
| 1 | first descendant (DFS first child) |
| 2 | first in tree (root then first descendant) |
| 3 | next in DFS pre-order |
| 4 | prev in DFS pre-order |
| 5 | last in tree (root then last descendant) |
| 6 | next SAME-NAME sibling |
| 7 | prev SAME-NAME sibling |

bm2dx equivalences (IIDX 33 bm2dx.dll):

- SetClipVisible (set _visible on clip + all same-name siblings, then invalidate) matches the
  bm2dx clip-visibility setter.
- SetClipBitmap (swap displayed bitmap via 0x087 on clip + siblings, then invalidate) matches
  the bm2dx clip-bitmap swap routine.

Image binding (the SDVX submonitor path):

- BindImageToMc builds the 32-byte image-info struct read by afp ord 0x088 (the afp-core
  descriptor builder): +0 texid (u32), +4 w (u16), +8 h (u16), +12 u0, +16 u1, +20 v0, +24 v1
  (floats). texid = slot-1 so SubmitGeometry's legacy resolve (`(tex_ref & 0xFFFF) + 1`) maps it
  back to `g_textures[slot]`. Full-frame UVs (0..1). It calls afp_play_work_load_image with
  attach = 0, matching the game, then fires invalidate (0x101E).
- BindClipImages mirrors SdvxSubmoniBg_Load's bind loop EXACTLY (soundvoltex.dll): bind
  frame[0]'s image to the resolved holder AND every
  same-name sibling (dir 6), attach = 0. The slideshow cross-fade path instead uses
  BindImageToMc per sibling (driven from the render loop).
- ResolveSiblings exists because SDVX's r3_fade cross-fade clip "bg_usr" has TWO same-name
  siblings - the depth-2 base and the depth-3 overlay - that the slideshow cycles frames
  through.

EnumerateClips: an EMPTY path passed to afp_mc_get_id_by_path resolves to the stream's root
clip. The walker steps to "first in tree" (dir 2) and then advances with "next in DFS pre-order"
(dir 3), with a max_clips safety cap.

## 8. AVS binary XML (avs_xml)

IFS files ship their manifests (texturelist.xml, afplist.xml, etc.) as AVS-binary-XML: a
compact, typed binary format that starts with the magic bytes 0xA0 0x42 and is NOT plain text.
History note: an earlier parser in ifs_inspect.cpp did a brittle string-search over the raw
bytes and silently returned zero matches whenever the file was binary-encoded - which is every
shipping IFS. The AvsXml module instead drives AVS's own property API, mirroring afp-utils' own
xml loader; all constants and call shapes are verified against afp-utils' xml loader and its
layoutlist.xml walker.

property_create flags (values straight from the afp-utils afpu-ngp xml loader):

- 0x0011 = short-path read (READ | DUPNODE).
- 0x1011 = long-path read (READ | DUPNODE | "LONG"). The `_long` memsize query is used whenever
  the short path refuses (memsize <= 0 or node count > 0xFFFF); the wrapper replicates the same
  control-flow graph.

E_PROPERTY_TRAVERSE values (avs2-core; verified by working code):
0 PARENT, 1 FIRST_CHILD, 2 FIRST_ATTR, 3 FIRST_SIBLING, 4 NEXT_SIBLING, 5 PREV_SIBLING,
6 LAST_SIBLING, 7 NEXT_MATCH (next sibling with the same tag name), 8 LAST_MATCH.
FindFirst + NextMatch(TRAVERSE_NEXT_MATCH) gives a forward-only "for each match" iterator
without tracking the parent.

E_PROPERTY_TYPE values used:

- TYPE_ATTR = 46 = 0x2E: the type passed to property_node_refer to read an XML attribute.
- TYPE_STR = 0x0B.
- TYPE_4U16 = 0x27 (four u16s from a child node, e.g. texture rects).

Attribute-read convention: the path is the attribute name with a TRAILING '@' (e.g. "name@").
Verified against the afp-utils layoutlist.xml walker:
`XCgsqzn00000af(0, v8, "name@", 46, v18, 64)` which is
`property_node_refer(tree=0, node=v8, "name@", 46, buf, 64)` - note the tree argument may be
NULL for node-relative reads. property_search usage `(tree, starting_node=null, path)` with a
slash-separated tag chain including the root element (e.g. "texturelist/texture") matches
the afp-utils layoutlist.xml walker, which does
`property_search(tree, null, "layoutlist/group")`.

File-open parameters: `avs_fs_open(path, 1, 420)` (flags 1, mode 420 = octal 0644). An fd <= 0
is normal for optional files - callers treat an empty PropertyTree as "file absent, move on".
AVS auto-detects binary vs plain-text XML in property_insert_read.

Lifetime rules (PropertyTree): the handle owns BOTH the parsed tree and the heap buffer it lives
in. Teardown order matters: property_destroy the tree FIRST (it may touch the heap memory it was
built into), THEN avs_gheap_free the heap. Both calls tolerate null. The handle is move-only -
copying would double-free the heap.

## 9. Process globals, COM guard, logging

app_globals.h holds process-wide singletons (one DLL set, one set of resolved function tables,
one D3D9 device, one AFP render context), defined `inline` so main.cpp, boot.cpp and
render_loop.cpp all see the SAME instance - these used to be file-statics in main.cpp before it
was split for the 1000-line file limit.

`g_ddr_mode`: true when the active profile uses the legacy AFP 2.13.7 (DDR World) render path
(DdrAfp) instead of the modern afp-core path (AfpManager). Set by BootFromGameDir from
`profile->legacy_afp`. The boot and render loop read this to route IFS-load and per-frame render
to the right backend; in DDR mode the modern g_afp / g_afpu tables stay null so the modern
per-frame code blocks self-skip on their null checks.

ComPtr (com_ptr.h) is a minimal COM release guard for D3D9 interfaces, not a full CComPtr.
Contract worth keeping: `operator&` Resets (releases) before handing out the address (for
overwrite-style APIs), while `GetAddressOf()` does NOT reset - use it only when the pointer is
known to be null.

Logging (log.{h,cpp}): Log::Init allocates a console, redirects stdout/stderr to it, and opens
`renderer.log` in the current working directory (so the log path is CWD-relative). Both the file
and stdout are set unbuffered so output survives crashes without fflush calls. LOG(tag, fmt,
...) prefixes `[tag] `; LOG_ONCE fires once per call site.

## 10. Open items / discrepancies

- Traversal direction mapping: avs_funcs.h and avs_xml.h documented two conflicting
  E_PROPERTY_TRAVERSE mappings (section 4). The avs_xml.h enum is exercised by working code and
  should be treated as correct; re-verify against the avs2-core enum and delete the stale
  variant.
- afp_play_work_load_image attach flag: afp_funcs.h said "pass 1 to mirror the game" while the
  shipping mc_control helpers pass attach = 0 "per the game" (mirroring soundvoltex.dll
  SdvxSubmoniBg_Load). The code path in use is attach = 0; re-verify against the live game if the
  submonitor bind ever misbehaves.
- afp_stream_do_create / afpu_do_create_stream_all (DDR): argument shapes were only partially
  RE'd ("refined when wiring content") - treat trailing args as unknown until re-verified.
