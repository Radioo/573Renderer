# Game profiles (src/game_profile.h / src/game_profile.cpp)

The game-profile system encodes what differs between Konami game versions so
the renderer can boot the right ordinals + defaults per game.

## IIDX 9 (9th style) - the older index container

9th style writes chunk 0's length **big-endian** and has no chunk 1, so the
name tables are appended to chunk 0 instead. Everything inside chunk 0 is the
usual little-endian layout at the usual offsets, and nothing is encrypted.
`SysIdx::Parse` picks the container by testing whether `4 + BE_length` equals
the file size, which is exact on all 170 packages and cannot collide with a
little-endian file. Layout and the split name-table form:
`IIDX/ninth_style_index_container.md`.

Its packages live under `data/graph/{anime,game,intro}/` plus `data/graph/mdata`,
with no `sys/` directory and no 3D scenes. Point the tool at the directory that
holds `data/` - for a standard dump that is `<root>/D/C02`.

## IIDX 11 (RED) - binary DirectX .x models

RED needs no decryption anywhere: its 172 `sys/` packages and all 360 tiles are
plain LZSS. What it needs is the **binary** `.X` encoding. A `.x` header's third
field is `txt ` or `bin `, and RED ships 7 of its 9 models as `bin` (DistorteD
ships 1, Resort Anthem none), so `model/red` failed to load entirely while the
newer games looked fine.

Both encodings describe the same object graph, so `XFile::Parse` transcodes the
binary token stream to the text form (`src/formats/xfile_binary.cpp`) and reuses
the existing parser rather than growing a second one. Token table and the
transcoding pitfalls: `IIDX/binary_x_models.md`.

## IIDX 13 (DistorteD) - same backend, Blowfish textures

DistorteD shares the whole IIDX 17 stack below (`scene3d` backend, 640x480, GC
sprite packages) with one difference: its content root is `data/graph/` and its
`sys/` textures are **Blowfish-CBC encrypted while `system.idx` is plaintext** -
the inverse of SIRIUS, where the package is sealed as a unit. The key derives
from the texture's file stem, so the game uses ten keys total. Details and
re-find anchors: `IIDX/distorted_gc_encryption.md`.

The loader decides per texture by decoding: plaintext LZSS first, and if that
does not yield a `GC ` header, Blowfish. The `GC ` magic is the acceptance test,
so a wrong guess cannot be silently accepted. `Lzss::Decompress` rejects a
declared output size larger than the coded input can produce (one control byte
covers eight items, longest match 18 bytes), which is what makes probing safe -
without it a garbage length from encrypted bytes triggers a multi-gigabyte
allocation.

## IIDX 17 (SIRIUS) - the engine-free `scene3d` backend

SIRIUS has no `bm2dx.dll` and no libafp: the AFP engine is statically linked
into `bm2dx.exe`, so there is nothing for a host to load. Its 3D model scenes
need no engine at all, so the `iidx17` profile uses a separate **`scene3d`**
backend that boots without any DLLs, scans for scene directories, and renders
them through `Scene3dHost`.

Its **2D sprite packages are decoded too**, by `Gc2dHost`. A package is one
directory under `data/graph_data/` holding an index (`system.idx`, or
`system.idr` when the package is encrypted) plus up to twelve LZSS-compressed
`GC ` texture tiles (`N.gcz`, or `N.gcr` when encrypted). The content scan lists
both kinds: `[3D scene]` and `[2D package]`. All 349 shipped packages load,
including the 134 encrypted ones.

Encryption is a per-package property detected by probing for `system.idr`; there
is no package list anywhere. The payload is AES-256-CBC with ciphertext stealing
(`src/formats/aes.cpp`), IV = the file's first 16 bytes, key = two constant
32-byte tables XORed together and then XORed with the package's directory name
(zero-padded, not repeated). A decrypted index still spells its texture paths
with the `.gcz` extension, so the loader rewrites it to `.gcr` at open time.

Three render-side rules that the format forces, all covered in the notes repo
doc (`IIDX/sirius_gc_sprite_formats.md`):

- **Point sampling, plus the half-texel offset.** Unused atlas space is filled
  with the transparent key colour (green, alpha bit clear), which is exactly the
  colour key the game hands to `D3DXLoadSurfaceFromMemory`. Anything that
  interpolates across a cell border drags that green into the sprite edge.
- **One D3D texture per tile.** Cells address a virtual atlas that stacks the
  tiles 1024 rows apart, and most packages contain cells that straddle a tile
  boundary, so a cell can need one quad per tile it touches.
- **The record table is an ordering table: draw it BACK TO FRONT.** One animation's
  records are listed front-most first, so drawing in table order makes each
  background paint over everything ahead of it. `GcAnim::Evaluate` reverses the
  flattened draw list, which is equivalent to walking the records and every
  nested group in reverse. Single-layer packages look identical either way, so
  the regression case for this is `title` / `TITLE_TAIKI`.
- **Blend mode comes from the alpha track, not from `flags` alone.** `flags` bit
  `0x0002` means "this record has an alpha track" and bit `0x0010` selects
  subtractive; the additive-vs-normal choice is made from the alpha keyframe's
  two bytes. `GcAnim::SelectBlend` implements the game's exact ladder, and a
  record whose pair is `(0, 100)` draws nothing at all. Without this, cells whose
  artwork has a baked black background (the copyright line in `title`) paint a
  solid black rectangle instead of compositing.
- **`topleft = position - anchor * scale`.** The position is where the record's
  anchor lands on screen; only the anchor is scaled. Scale is a signed percentage
  per axis, so `(100, 200)` means the cell is stored at half height, and
  `(-100, 100)` is a horizontal flip.

A package's alphabetically first animation is not necessarily its content: in
`sys/0200` animation `00` is built entirely from cells that point at blank atlas
padding. Use the **2D package** inspector tab (or `--animation`) to pick another.

The install is split into datecoded revision folders with `.orig` (encrypted)
siblings. `GameRevision::LatestRevisionDir` picks the newest folder whose name
is exactly ten digits, which excludes `.orig` structurally rather than by
suffix matching, and logs the choice.

Adding this backend also moved one check off the AFP runtime: startup content
loading used to gate on `Runtime::Active().IsBooted()`, which is meaningless for
an engine-free backend. `IBackend::ContentReady()` now lets each backend answer
for itself.

## IIDX 18 (Resort Anthem) - by-name libavs

IIDX 18 uses the SAME TXP2 pipeline as IIDX 19 (identical flag word 0x67FDB, no
libafputils, host-driven packages) with two differences worth knowing.

Its `libavs-win32.dll` exports READABLE names (366 exports, no obfuscated
`XC......` prefix), so `DllLoader` resolves by symbol and the generation's
ordinal table is bypassed entirely - only the boot-contract flags
(`boot_takes_split_heaps`, `log_writer_ctx_first`, `log_level_is_u32`) still
apply, which is why the profile reuses `AvsGeneration::Avs2134`.

libavs does NOT prefix every symbol with `avs_`. The compression stream is
exported as `cstream_create` / `cstream_operate` / `cstream_finish` /
`cstream_destroy` while `avs_fs_open` / `avs_boot` / `property_create` keep the
prefix. Our field names carry the `avs_` form, so the four cstream loads use
`DLL_LOAD_AS` with the real export names. Obfuscated builds never noticed
because they resolve by ordinal. When this is wrong every texture fails to
inflate and the whole screen renders as untextured white quads.

AFP is ver2.7.4 (vs 2.9.4 on Lincle). The render-params slot layout is
IDENTICAL through slot 17; 2.7.4 additionally populates slot 19 (+0x4C), which
bm2dx uses as a timeline SOUND callback ("call sound[%s]") and is correctly a
no-op for a renderer.

Packages live under `data/graph_data/sys/*.bin`, not `data/graphic/`. The
content scan finds them either way because it walks the tree for packages.

## P15 split: identity vs engine config

Since P15 the old flat Profile struct is TWO slug-keyed tables:

- `GameProfile::Profile` (src/game_profile.{h,cpp}) is pure IDENTITY:
  `{name, slug, dir_substring, backend_id, game_dll, default_render_w/h}`.
  It is what selection (AutoDetect / BySlug / the Setup combo), the backend
  registry, and the window-size default consume. It contains ZERO
  engine-ABI data, so a future non-AFP backend adds identity rows without
  touching anything AFP.
- `AfpProfiles::AfpConfig` (src/backend/afp_profiles.{h,cpp}) is the AFP
  family's engine config: DLL names, `DllOffsetSet`, the boot-call gate
  bools, `scan_arc_containers`, `time_scale`. `AfpFamilyBackend::Boot`
  resolves it by slug (`AfpProfiles::For`); a modern/DDR profile without a
  config row fails the boot with a clear error. `ActiveOffsets` /
  `SetActiveOffsets` / `kFallbackIidxOffsets` moved into this namespace
  too, and `EngineSession::active_profile` became `active_cfg`
  (`AfpManager::SetActiveConfig`).
- `legacy_afp` is DELETED: the backend choice is `Profile::backend_id`
  ("afp_modern" / "afp_ddr"), resolved by the `Backend::CreateActive`
  registry table. The old `Profile::afp` AfpOrdinals member and
  `GameProfile::kSkip` were removed outright: nothing ever read them (the
  real ordinals live in afp_funcs.h's DLL_LOAD lines; the reference table
  stays in docs/engine_binding.md).

"New modern-AFP game" is now: one identity row in game_profile.cpp + one
config row in afp_profiles.cpp. "New backend" is: identity rows with a new
backend_id + a registry entry + that backend's own config table. The field
documentation below (ordinals, offsets, boot gates) describes the AfpConfig
fields unless it names identity fields.

## Why it exists

IIDX 33's afp-core.dll and SDVX 7 NABLA's afp-core.dll both ship the same
obfuscated-export-name scheme (`XCd229cc<hex>`, 126 exports each) but they are
DIFFERENT BUILDS with different bodies and (potentially) a different
ordinal-to-function mapping. Calling IIDX's ordinal 0x01d
(afp_set_stream_nr) against the wrong build AVs because the slot holds a
different function. So the renderer tracks per-game ordinal overrides plus a
handful of other knobs (default render resolution, data-dir layout, boot-call
gates, DLL data-segment offsets).

## Lifecycle

1. User picks a directory in the Setup screen; a profile is auto-detected
   from the directory name (case-insensitive substring match against each
   profile's `dir_substring`).
2. User can override the auto-pick via the GUI dropdown.
3. Persists in settings.ini under `game_profile=<slug>` TOGETHER WITH
   `game_dir` - the two form a pairing. On the next launch the saved slug is
   honoured ONLY when booting the same `game_dir` it was saved with
   (`Settings::SameGameDir` - case-insensitive, separator- and
   trailing-slash-tolerant; unit-tested). A launch with a different
   `--game-dir` logs that the saved pairing is stale and falls back to
   auto-detect. Rationale: applying one game's DLL data-segment offsets to
   another game's modules crashes deep inside afp (observed: saved iidx33 +
   `--game-dir` SDVX segfaulted in afp_stream_play).
4. BootFromGameDir consults the active profile when filling AfpFuncs /
   AfpuFuncs / AvsFuncs ordinals and when picking defaults.

Resolution is strict - there is NO silent default profile:

- An explicit slug (CLI `--profile`, GUI combo, valid settings pairing) that
  `BySlug` does not know fails the boot with the known-slug list.
- No slug + `AutoDetect` miss fails the boot and asks for `--profile` or a
  GUI pick. The GUI "Auto" label warns when the current dir has no match.
- A failed boot with no GUI to retry from (`--no-gui` / `--headless`) exits
  1 with the boot error instead of idling in the request loop.

`GameProfile::Default()` was removed with this change - the old
auto-detect-miss fallback to IIDX 33 was exactly the wrong-offsets crash
above. Registry order drives the GUI dropdown display order; most-likely
profiles go first. `AutoDetect` and `BySlug` return nullptr on no match. The
registry is constructed at static init and never empty.

Adding a modern-AFP profile:

1. Append an identity row in game_profile.cpp (name / slug / dir_substring
   unique enough for the "Auto" UI option; backend_id "afp_modern"; the
   game_dll used for the presence probe; default render size).
2. Append a matching AfpConfig row (same slug) in
   src/backend/afp_profiles.cpp: gate bools + offsets.
3. RE the new game's `afp_set_afp_data` (afp-core ord 0x000) and
   `afpu_render_init` (afp-utils ord 0x070) to derive a new DllOffsetSet
   constant (see below).
4. If the new build moves an ordinal, update afp_funcs.h's DLL_LOAD line
   (the ordinal reference table is docs/engine_binding.md).

The registry uses C++20 designated initializers ON PURPOSE: field-name typos
surface as compiler errors instead of silently misaligned positional brace
init - which bit this file twice when fields were re-ordered. Designated
initializers must follow declaration order (e.g. SDVX's
`.afpu_set_config_safe_clean_pos` must appear between `.call_afpu_boot` and
`.call_afp_set_flag_setup`).

## AvsGeneration - the avs2 ordinal map is NOT stable across versions

`AvsFuncs::Load` resolves avs2 by mangled ordinal (`<prefix><6 hex>`), and
that ordinal-to-function map CHANGES between avs generations. It is not a
uniform shift, so it cannot be derived by adding an offset.

`AfpConfig::avs_generation` selects the table; `kAvsOrdinals217` (the
default) covers avs2 2.16.3 and 2.17.x, and `kAvsOrdinals2161` covers the
2.16.1 build IIDX 24 ships. `LoadAllDlls` picks one and logs which.

Concrete evidence, IIDX 24's libavs-win32 2.16.1 (390 exports) vs the
2.16.3 / 2.17.x builds (392 exports):

| block | delta | example |
|---|---|---|
| VFS (`avs_fs_*`) | -0x15 | mount 0x04b -> 0x036 |
| property (`property_*`) | -0x15 | create 0x090 -> 0x07b |
| log (`log_body_*`) | -0x12 | info 0x17c -> 0x16a |
| boot (`avs_boot`/`_shutdown`) | -0x0f | boot 0x129 -> 0x11a |
| `avs_is_active` | -0x11 | 0x12d -> 0x11c |
| `avs_filesys_imagefs` | -0x0b | 0x158 -> 0x14d |
| gheap | REORGANISED | allocate 0x02f -> 0x183, free 0x031 -> 0x178 |

Four different deltas plus a wholesale gheap move: the heap API was
restructured between 2.16.1 and 2.16.3, so `avs_gheap_allocate` is not
merely shifted. Using the 2.17 map against 2.16.1 makes
`property_create` (0x090) land on `node_refdata`, and the renderer
SEGFAULTS inside AVS boot. That was the IIDX 24 boot crash.

How to re-derive for a new avs build (the method that produced the table
above): these libraries keep their assert/log strings, so locate the source
file names (`property-api.c`, `vfs-api-mount.c`, `avs-boot.c`,
`heap-api-gheap.c`) and the function-name literals (`node_create`,
`mount: fstype==NULL`, `kill application`), data-xref back to the
referencing function, then walk callers until you reach an export named
`<prefix><6 hex>` and read off its suffix. Cross-check the shape (arg
count, callees) against the known-good build. Disambiguating tips found
this round: `avs_boot` vs its mode-2 sibling both log the same banner - the
real `avs_boot` writes boot-mode 1 into the state byte that `avs_is_active`
reads; `avs_gheap_allocate` is the one whose NULL-pointer path allocates
and non-NULL path reallocates (its wrapper logs `realloc(%p,%u)=%p`), NOT
the 1-arg `alloc(size)` export next to it.

### The avs BOOT CONTRACT also changes, not just the ordinals

Two things beyond the ordinal map differ per avs generation. Both are flags on
`AvsOrdinals` and both were found only by RE, because both fail SILENTLY or as
a bare segfault:

**1. `boot_takes_split_heaps` - avs_boot arity.** 2.13.4 and 2.15.8 take SEVEN
args `(config, heap_std, sz_std, heap_avs, sz_avs, log_writer, log_ctx)` with
TWO separate heaps; 2.16+ take SIX. Calling the 6-arg shape against a 7-arg
build puts the log writer where the build expects `sz_avs`. Proof: in the
2.13.4 avs_boot, arg2 asserts `"heap_std is NULL."` and arg4 asserts
`"heap_avs is NULL."`, arg4 is 16-byte aligned then carved down through
desc/thread/fs/net_private, and args 6/7 are passed straight to log_boot.

**2. `log_writer_ctx_first` - the log-callback argument ORDER.** avs calls the
host log writer through one small dispatcher. In 2.13.4 it is:

```
if ( g_log_writer != NULL )
    g_log_writer(g_log_writer_ctx, buf, len);
```

i.e. **context FIRST**, whereas 2.15.8+ pass `(buf, len, ctx)`. With the newer
signature installed on 2.13.4 the writer receives `chars = ctx` and
`nchars = <the buffer pointer>`, so the very first log line segfaults inside
the host's `fwrite`. The symptom is brutal to diagnose because the crash
happens BEFORE any avs output exists - `avs_out.log` is never even created, so
it looks like avs died with no explanation. `src/avs_boot.cpp` keeps both
`avs_log_writer` and `avs_log_writer_ctx_first` around one shared emitter and
picks by flag.

How to re-find the writer order on a new build: from `log_boot`, note which
globals it stores the two trailing avs_boot args into, then xref the writer
global; its single call site is a 3-line dispatcher and the argument order is
read straight off it.

**3. `log_level_is_u32` - the /config/log/level NODE TYPE.** avs_boot parses
its config through `property_psmap_import` against a built-in psmap. The psmap
is an array of 16-byte entries `{type, ?, offset, len, path*, default}`; dump
it at the address avs_boot passes as the psmap argument and read the type of
each entry. In 2.13.4 `log/level` has psmap type **0x07 (u32)** with default 4,
so the string node newer builds accept is rejected with
`W:psmap: failed to read 'log/level'` followed by the FATAL
`F:boot: property_psmap_import() failed.` The renderer creates the node as
u32 4 (= misc) on that generation and as the string "misc" elsewhere. Neighbour
entries in the same psmap confirm the decoding: `log/use_netsci` is type 0x03
(u8) and `desc/nr_desc` is type 0x05 (u16) with default 808, which matches the
`nr_desc=808` the build then logs.

## kSkip sentinel

`GameProfile::kSkip` (= -1) as an ordinal means "this game's afp-core does
NOT expose this function - don't resolve, don't call". For known-divergent
functions where a build removed the call entirely or moved the responsibility
elsewhere.

## DllOffsetSet - per-game DLL data-segment offsets

Background: `dll_offsets.h` holds the original IIDX-33-derived constants
(kCallbackTable = 0xE0E08 etc.). Different game builds of afp-core /
afp-utils have those globals at DIFFERENT addresses - verified by RE-ing both
IIDX 33's and SDVX 7 NABLA's afp-core `afp_set_afp_data` (ord 0x000)
decompiles plus SDVX's afp-utils `afpu_render_init` (ord 0x070). IIDX writing
through SDVX's offsets corrupted unrelated bytes in SDVX's afp-core .data
section - the actual root cause of the `afp_set_stream_nr` AV. Routing every poke through the active GameProfile ensures
only addresses known to belong to the right global are touched.

Field meanings:

- `afp_callback_table` (afp-core): 35 qwords installed by afp_set_afp_data.
  The renderer reads it to log slot bindings; harmless on its own but the
  IIDX address is junk on other games.
- `afp_render_flags` (afp-core): bit 0x800 controls afp_set_afp_data's
  memcpy-vs-rebind path. The renderer clears it before calling (unless
  skip_explicit_afp_set_afp_data, see below).
- `afp_nearfar_slot` (afp-core): slot 13 of the callback table - the
  get_near_far callback, overwritten with the renderer's stub. Always equals
  afp_callback_table + 0x68.
- `afpu_data_struct` (afp-utils): afp-utils' built-in callback table (35
  qwords starting with a flags dword). The renderer hands a pointer to this
  to afp_set_afp_data so afp-core knows the dispatch table.
- `afpu_render_context` (afp-utils): the slot afp-utils stores the render-ctx
  pointer in once afpu_render_init has run. AfpHook polls it to detect when
  afpu is ready to be hooked (IIDX-only module; SDVX uses BootTrace instead).
- `afpu_set_screen_rect_fn` (afp-utils): a small standalone function taking
  an int[4] rect and storing it into afpu's screen-dim globals; the
  renderer's BeginRender invokes it every frame via
  `DllOffsets::At<void(int*)>(afpu_base, .afpu_set_screen_rect_fn)`.
  CRITICAL: this offset points at actual FUNCTION BYTES that must match the
  (a1) -> store-4-ints + flag-bit signature. Calling IIDX's offset
  on SDVX lands inside SDVX's matrix-transform helper, which
  interprets the rect as vertex data and writes off the end of buffers - the
  root cause of the per-frame afp_do_sort_render AV.

`kFallbackIidxOffsets` (extern, same values as IIDX 33) is the last-resort
fallback for code paths that need offsets but somehow run before a profile is
set; IIDX 33 because it is the renderer's reference target.

### Diagnostic-only offsets (optional)

A second group of `DllOffsetSet` fields exists purely for log-line
diagnostics; none is load-bearing:

| field | reads | consumer |
|---|---|---|
| afp_table_b_count | afp-core word (IIDX 0xE1142) | afp_packages post-sweep log |
| afpu_shapes_a / _b | afp-utils ints (IIDX 0x288AC / 0x288B0) | afp_d3d9 EndRender frame<10 counter log |
| afpu_drawn | afp-utils int (IIDX 0x289E4) | same |
| afpu_world_mat_type | afp-utils byte (IIDX 0x2B8C5) | render_loop pre-fault dump |
| afpu_world_mat | afp-utils float[] (IIDX 0x2B880) | same |

A field of 0 means "not RE'd for this game". Each read site is gated on its
offset being non-zero, so the diagnostic degrades to disabled rather than
reading a wrong global. Only IIDX 33 and the fallback populate these; SDVX 7
and GITADORA DELTA leave them 0 (designated-init) until confirmed. This
replaced IIDX-33 hex literals that were previously inlined at the three read
sites and executed on every game, so a non-IIDX title read whatever those
afp-core/afp-utils addresses happen to hold in its build.

Access is through `GameProfile::ActiveOffsets()` (returns the active
profile's set, or the IIDX fallback before boot). Boot publishes the set via
`SetActiveOffsets(off)` right where it resolves the load-bearing offsets, so
no raw offset literal lives outside `game_profile.cpp`.

### Offset values and provenance

IIDX 33 (`kIidx33Offsets`; from the original dll_offsets.h IDA RE):

| field                   | value   | note                                  |
|-------------------------|---------|---------------------------------------|
| afp_callback_table      | 0xE0E08 |                                        |
| afp_render_flags        | 0xE1134 |                                        |
| afp_nearfar_slot        | 0xE0E70 | = table + 0x68 (slot 13)               |
| afpu_data_struct        | 0x281F0 |                                        |
| afpu_render_context     | 0x28880 |                                        |
| afpu_set_screen_rect_fn | 0x18550 | the set-screen-rect function body, in IIDX afpu 1.2.19 |

IIDX 26 Rootage (`kIidx26Offsets`; afp-core 2.14.11 / afp-utils 1.2.12 -
seven and five point releases BEFORE IIDX 33's 2.14.18 / 1.2.19, same
XCd229cc / XE592acd export schemes but a completely different data-segment
layout. Derived by decompiling afp_set_afp_data (afp-core ord 0x000: the
callback table is the destination of its 35-qword copy loop, also passed
to the rebind helper in the & 0x800 branch; the render-flags dword is the
& 0x800 gate itself, at table + 0x32C exactly like IIDX 33) and
afpu_render_init (afp-utils ord 0x070: stores its argument - the render
context - into one global at function entry and passes the data-struct
global to afp-core's afp_set_afp_data at the end). The set-screen-rect
function was found by its body shape, NOT by data-struct+0x630 (that
offset holds max_nr_nodes in 1.2.12 - the struct layout shifted): search
afp-utils for the `or byte ptr [rip+X], 1` idiom (80 0D ?? ?? ?? ?? 01)
and keep the hit whose function takes a pointer arg and stores 4 ints
(one 16-byte SSE store in this build) into rect globals, ORs 1 into a
flag byte and zeroes a counter. Cross-check: the function's address sits
in the afpu data struct at slot +0x70, the SAME slot IIDX 33's set-rect
function occupies in ITS data struct):

| field                   | value   | note                                     |
|-------------------------|---------|-------------------------------------------|
| afp_callback_table      | 0x189988 | in afp_set_afp_data                       |
| afp_render_flags        | 0x189CB4 | = table + 0x32C, the & 0x800 gate         |
| afp_nearfar_slot        | 0x1899F0 | = table + 0x68                            |
| afpu_data_struct        | 0x431A0  | in afpu_render_init                       |
| afpu_render_context     | 0x43850  | in afpu_render_init                       |
| afpu_set_screen_rect_fn | 0x30CB0  | body-shape + data-struct slot +0x70 match |

SDVX 7 NABLA (`kSdvx7Offsets`; derived by comparing IIDX afp-core's
afp_set_afp_data against SDVX 7's - the function structures are identical so
the globals it writes map 1:1; SDVX afp-utils' afpu_render_init gives the
data-struct + render-context offsets directly):

| field                   | value   | IDA name / note                        |
|-------------------------|---------|----------------------------------------|
| afp_callback_table      | 0xED008 | in afp_set_afp_data                     |
| afp_render_flags        | 0xED334 |                                         |
| afp_nearfar_slot        | 0xED070 | = table + 0x68                          |
| afpu_data_struct        | 0x2B2C0 | in afpu_render_init                     |
| afpu_render_context     | 0x2B958 | in afpu_render_init                     |
| afpu_set_screen_rect_fn | 0x199B0 | the set-screen-rect function body, in SDVX afpu 1.2.26; IIDX's offset lands inside SDVX's matrix-transform helper - the per-frame AV root cause |

GITADORA DELTA (`kGitadoraDeltaOffsets`; afp-core/afp-utils are the same
modern XCd229cc / XE592acd family as IIDX/SDVX, only the data-segment
addresses differ. Derived by decompiling afp_set_afp_data (afp-core ord
0x000) and afpu_render_init (afp-utils ord 0x070); set_draw_rect found via
the "draw_primitive called before set_draw_rect" assert string + flag xref):

| field                   | value   | IDA name / note                          |
|-------------------------|---------|-------------------------------------------|
| afp_callback_table      | 0xEE048 | in afp_set_afp_data                        |
| afp_render_flags        | 0xEE374 | the & 0x800 path gate                       |
| afp_nearfar_slot        | 0xEE0B0 | = table + 0x68                             |
| afpu_data_struct        | 0x2A2D0 | in afpu_render_init                        |
| afpu_render_context     | 0x2A8A8 | in afpu_render_init                        |
| afpu_set_screen_rect_fn | 0x15720 | afpu set_draw_rect                          |

jubeat T44 (`kT44Offsets`; afp-core 2.14.19 / afp-utils 1.2.20 - one point
release after IIDX 33's 2.14.18 / 1.2.19, same XCd229cc / XE592acd export
scheme. Derived by decompiling afp_set_afp_data (afp-core ord 0x000) and
afpu_render_init (afp-utils ord 0x070); every data-segment global matches
IIDX 33 exactly, only the set-screen-rect FUNCTION moved. Found by taking
IIDX's known set-rect body (store int[4] into the rect globals at
data-struct+0x630, OR 1 into the flag byte, zero the counter) and locating
the identical body in the T44 build via a data xref to the first rect
global):

| field                   | value   | note                                     |
|-------------------------|---------|-------------------------------------------|
| afp_callback_table      | 0xE0E08 | same as IIDX 33                            |
| afp_render_flags        | 0xE1134 | same as IIDX 33                            |
| afp_nearfar_slot        | 0xE0E70 | = table + 0x68                             |
| afpu_data_struct        | 0x281F0 | same as IIDX 33                            |
| afpu_render_context     | 0x28880 | same as IIDX 33                            |
| afpu_set_screen_rect_fn | 0x18810 | IIDX 33 has 0x18550; body is identical     |

## AfpOrdinals - the afp-core ordinal map

Defaults match IIDX 33 (Sparkle Shower), the first / primary RE target. Any
profile matching IIDX's ordinals uses the defaults; divergent profiles
override individual fields. avs2-core / afp-utils share their ordinals across
IIDX and SDVX in current testing - only afp-core was expected to diverge; if
that ever changes, mirror the struct for the other two DLLs. (Live trace +
IDA RE later confirmed SDVX 7's ordinal map matches IIDX 33's exactly; the
real divergence was the data-segment addresses. GITADORA's map is also
confirmed identical via gdxg's imports.)

Default (IIDX 33) map:

| function                    | ord   | function                    | ord   |
|-----------------------------|-------|-----------------------------|-------|
| afp_set_afp_data            | 0x000 | afp_stream_get_name         | 0x01f |
| afp_get_afp_data            | 0x001 | afp_stream_destroy          | 0x020 |
| afp_boot                    | 0x002 | afp_stream_set_speed        | 0x02a |
| afp_shutdown                | 0x003 | afp_stream_set_matrix       | 0x02c |
| afp_set_flag                | 0x005 | afp_stream_get_matrix       | 0x02d |
| afp_set_verbose             | 0x008 | afp_stream_set_translate    | 0x02e |
| afp_set_global_speed        | 0x00a | afp_set_flag_mask           | 0x037 |
| afp_set_bg_color            | 0x00b | afp_system_dump_layer_info  | 0x043 |
| afp_do_render               | 0x00d | afp_data_get_info           | 0x044 |
| afp_do_update               | 0x00e | afp_data_get_stream_info    | 0x045 |
| afp_render_init             | 0x00f | afp_get_layer_info          | 0x046 |
| afp_render_destroy          | 0x010 | afp_get_data_id_by_name     | 0x047 |
| afp_do_sort_render          | 0x011 | afp_get_layers_by_nr        | 0x04b |
| afp_set_create_level        | 0x013 | afp_mc_get_id_by_path       | 0x066 |
| afp_get_create_level        | 0x014 | afp_mc_get_relative_id      | 0x069 |
| afp_stream_control          | 0x015 | afp_mc_control              | 0x071 |
| afp_stream_create           | 0x018 | afp_mc_get                  | 0x073 |
| afp_stream_set_data         | 0x019 | afp_ext_command             | 0x086 |
| afp_stream_get_work         | 0x01a | afp_play_work_load_bitmap   | 0x087 |
| afp_set_stream_nr           | 0x01d | afp_stream_play             | 0x01e |

## Profile fields

- `name` - human label, and ONLY that: it is read in exactly one place, the
  Setup screen's profile dropdown. It states the VERSION RANGE the profile
  supports rather than one codename (e.g. "IIDX 21-24", "IIDX 27+"), because
  a profile covers every version that shares its ordinals/offsets, not just
  the one it was RE'd against. Renaming it is cosmetic - the dropdown stores
  the SLUG, so a saved selection survives a relabel or a reorder.
- `slug` - settings.ini token, `--profile` argument, and the key the
  AfpProfiles config table and the qpro gating use. Stable: do not rename.
- `dir_substring` - case-insensitive auto-detect hint.
- `avs_dll` / `afp_dll` / `afpu_dll` - DLL filenames the game ships. IIDX and
  SDVX use the avs2-core.dll / afp-core.dll / afp-utils.dll trio; DDR World
  ships lib*-win64 names. DllLoader auto-detects each DLL's obfuscated export
  prefix, so only the filenames differ - ordinal maps are resolved against
  whatever is loaded.
- `default_render_w/h` - the resolution the game ships at; seeds the Setup
  screen W/H inputs when the user picks the profile without a saved value.
- `offsets` - the DllOffsetSet above.

### Boot-time call gates

Some afp-core entry points exist on IIDX but AV on SDVX even though the
export resolves to the "same" function (the global state machine inside
afp-core differs between builds). Each flag gates one call site in
afp_boot.cpp; false = skip.

- `call_afp_set_stream_nr` - gates the afp_set_stream_nr call.
- `call_afp_stream_create_test` - the post-init diagnostic probe:
  afp_stream_create() then destroy. DEFAULT OFF for all games. The real
  afp_stream_create (afp-core 2.14.x; IIDX 33 2.14.18 EA) takes
  THREE args and passes the first to afp_stream_data_check_valid,
  which dereferences it to read the CWS/FWS/afp magic bytes. The
  renderer's typedef is 0-arg, so the probe hands whatever is in RCX to that
  deref. Null/invalid-safe junk makes the function early-return 0xFFFFFFFD (the
  "benign sentinel" IIDX 33 used to hit); a bad non-null pointer AVs in the magic
  read. So IIDX 33's "benign" behaviour was register-value LUCK, not safety - a
  codegen change to the code before the call flipped IIDX 33 (2.14.18) from
  sentinel to crash (0xC0000005 at boot). There is no valid data to pass at boot
  (no IFS mounted), so the probe cannot be made correct and is skipped; real
  streams come from the package load path. The probe is also SEH-guarded in
  afp_boot.cpp as a backstop if a profile opts back in on a proven-0-arg afp-core.
  To re-find afp_stream_create when the DLL changes: the renderer resolves it by
  the mangled name `<prefix>%06x` of its "ordinal" (a NAME suffix, = export
  ordinal - 1), so 0x018 -> name `...000018`; decompile that export and look for
  the `afp_stream_data_check_valid` / "This is not afp data." call to confirm the
  3-arg signature.
- `call_afp_render_init` - afp-core ordinal 0x00f. SDVX 7's live boot trace
  shows the GAME never calls it directly; on IIDX 33 it is part of the
  bm2dx-derived sequence the renderer copied. (History: the renderer's
  intermediate afp_render_init/afpu_render_init calls between afp_boot and
  afp_set_stream_nr were once believed to corrupt heap state that SDVX's
  afp_set_stream_nr tripped over; later
  superseded, see the SDVX profile notes below.)
- `call_afpu_render_init` - afp-utils ordinal 0x00f, same situation.
- `call_afpu_set_config` - afp-utils ordinal 0x005. Live SDVX trace shows the
  SDVX game does not call it from its main thread (see the SDVX notes for the
  later, corrected picture).
- `call_afpu_set_flag_setup` - gates the afpu_set_flag boot calls. The exact
  (flags, mask) pairs fired come from `afpu_set_flag_calls` (see below);
  the default list is the bm2dx-33-derived triple (4,4 / 8,8 / 16,16).
  SDVX's trace shows only ONE afpu_set_flag call at boot with
  completely different args (0x1, 0x1000); the triple is skipped on SDVX
  entirely until the real SDVX flags are known (branch instead of skip,
  later).
- `afpu_set_flag_calls` / `afp_set_flag_calls` - the exact per-profile
  (flags, mask) pair lists the gated set-flag setup fires, in order. Both
  DLLs implement the same semantics, verified by decompiling afp-core
  export 0x005 and afp-utils export 0x003 on IIDX 26 and IIDX 33:
  `new = mask | (old & ~flags)` - the first argument SELECTS the bits to
  modify, the second gives their new values. So (16, 16) SETS bit 16,
  (16, 0) CLEARS it, and a game's mirrored `mov edx, N; mov ecx, edx`
  call sites mean "set bit N" while `(N, 0)` means "clear bit N". The
  defaults preserve the renderer's historical bm2dx-33-derived behaviour:
  afp (16,0 / 8,0 / 65537,0) and afpu (4,4 / 8,8 / 16,16). A profile whose
  game demonstrably passes different pairs overrides the list with the
  game's exact calls (read them off the disasm of the boot function's
  call sites - the decompiler often hides the second argument, so check
  the edx/ecx setup instructions).
- `call_afpu_boot` - afp-utils ordinal 0x000. Live SDVX trace showed
  soundvoltex.dll's IAT does NOT call afpu_boot directly; best hypothesis was
  that SDVX's afp_boot internally bootstraps the afp-utils side, making an
  explicit afpu_boot a second-init that corrupts state. Profile-gated so IIDX
  (where afpu_boot is required) keeps its behaviour. (Later corrected for
  SDVX - see profile notes.)
- `afpu_set_config_safe_clean_pos` - afpu_set_config(3, X) value override.
  bm2dx passes 1, which installs the vertex-cleanup routine
  as the cleanup callback. SDVX needs 0: its
  afpu_render_set_clean_position_local path with X=1 routes through a
  cleanup that writes through buffer pointers the renderer has not sized
  correctly, AVing mid-frame. With X=0 the callback stays a no-op stub
  and the crucial case-1 buffer-size config still gets applied.
- `call_afp_set_flag_setup` - afp_set_flag(flag, mask) shape: bm2dx-style
  decomp shows single-arg afp_set_flag(16) (mask = junk/0). SDVX 7's live
  trace shows the mirrored-mask form afp_set_flag(0x10, 0x10) /
  afp_set_flag(0x8, 0x8) for the first two calls and asymmetric (0x10001, 0)
  for the third. The (flag, mask) semantics clearly differ from what the
  bm2dx-derived renderer assumes, so the whole triple is skipped on SDVX.
- `apply_iidx_data_segment_patches` - gates three operations in afp_boot.cpp:
  (a) VirtualProtect + write kNearFarSlot to install
  StubGetNearFar; (b) VirtualProtect + write kRenderFlags
  to clear the 0x800 flag bit; (c) call
  afp_set_afp_data(afpu_data_ptr = the afp-utils data-struct offset) - a pointer derived
  from an IIDX-specific offset. On a different build those offsets are
  UNKNOWN data: the writes corrupt random memory and the pointer is invalid.
  This gating was the root cause fix for the afp_set_stream_nr AV after every
  other gate was off. The "iidx" in the name is now a historical misnomer
  (offsets are per-profile); a rename to `apply_data_segment_patches` was
  suggested as a follow-up.
- `afp_set_afp_data_wide_args` - per-call argument shape from the live SDVX
  trace. The renderer historically called with the bm2dx 1-arg shape; SDVX
  expects the 4-arg form `afp_set_afp_data(callback_table, 0, 0x320,
  heap_ctx)` and reads R8/R9 (register junk corrupts behaviour). 0x320 is the
  size of the afp-utils built-in callback table.
- `afp_set_verbose_wide_args` - same idea: SDVX form is
  `afp_set_verbose(1, 0x10000)` (0x10000 = a verbosity-flags word) vs bm2dx's
  1-arg call.
- `scan_arc_containers` - DDR World: renderable .ifs are LZ77-wrapped inside
  .arc containers (data/arc/bm2d/*.arc). When set, the boot-time scan also
  walks .arc files, parses their TOC, and surfaces the inner .ifs in the
  browser (skipping arcs holding no .ifs). Off for IIDX/SDVX (loose .ifs).
- `legacy_afp` - DDR World runs legacy AFP 2.13.7 (readable exports:
  afp_boot / afp_do_render / afp_do_display) - a different API generation
  than the modern afp-core (afp_set_afp_data / afp_do_update /
  afp_do_sort_render). When set, BootFromGameDir routes boot / IFS-load /
  render through DdrAfp instead of AfpManager, LoadAllDlls skips the modern
  (ordinal) afp/afpu func resolve (DdrAfp resolves its own by-name tables),
  and the render window is pinned to the DDR-native 1280x720. See
  the DDR AFP 2.13.7 boot sequence.
- `time_scale` - per-frame afp advance time scale (multiplies the dt fed to
  afp_do_render). 1.0 = native afp speed, the right default. History and
  caution: the DDR render's per-frame advance vs the real game differs PER
  background (background_0001 resolves to a 3607-frame loop in the renderer
  vs the real game's 3404; background_0009 matches at 1.0). A 1.0596 value was
  tried to make background_0001's export duration match the real game, but a
  GLOBAL speed fudge shifts the animation PHASE of EVERY bg - on a bg with a
  sharp wipe (background_0009's line bundles, ~3 s) it put the render ~11
  frames deeper into the wipe than the real game, visibly truncating the
  left/right shapes. A uniform fudge cannot fix a per-bg timing difference,
  so this stays 1.0; background_0001 then loops at its true 3607 frames
  (~3.4 s longer than the real game, but seamless and artifact-free). The real
  root cause (the renderer's per-frame advance diverging from the real game) is
  still UNRESOLVED. The
  content-based loop detector finds whatever period the native-speed render
  actually has.
- `skip_explicit_afp_set_afp_data` - skip the renderer's EXPLICIT
  afp_set_afp_data call + the 0x800 render-flag clear (both inside the
  data-segment-patch block). Background: the IIDX-derived boot clears
  afp-core's 0x800 flag then calls afp_set_afp_data itself, forcing the
  "memcpy" callback-table install. But afpu_render_init ALREADY calls
  afp_set_afp_data internally (with 0x800 still set -> the "rebind" path), so
  the renderer's extra call OVERWRITES that table with the memcpy variant. On
  GITADORA's afp-core 2.14.26 the two installs are NOT equivalent: the memcpy
  one leaves the per-shape transform handoff wrong (shapes come out
  X-scale = 0). gdxg never calls afp_set_afp_data nor clears 0x800 (RE
  evidence: gdxg's boot function and the internal init function it delegates
  to) - it relies solely on afpu_render_init's internal rebind. True = match the game: skip both, but keep the slot 12/13
  (screen-size / near-far) re-patch.

## Registry order (load-bearing)

`kProfiles` is listed OLDEST-FIRST within the IIDX family (iidx09, iidx11,
iidx13, iidx17, iidx18, iidx19, iidx20, iidx24, iidx26, iidx33), then the
other games. That order is not cosmetic:

- `AutoDetect` returns the FIRST profile whose `dir_substring` appears in the
  path. So when one hint is a SUBSTRING of another, the more specific one
  must be listed first, or it can never be reached. The live instance is
  iidx33's broad `"iidx"` versus iidx11's `"iidxred"`; listing newest-first
  would make every IIDX dump auto-detect as IIDX 27+.
- Oldest-first satisfies that automatically, because the broad `"iidx"` hint
  belongs to the newest profile - the ordering that reads most naturally in
  the dropdown is also the correct one.
- `tests/game/game_profile_tests.cpp` machine-checks both halves: one case
  asserts no earlier hint is a substring of a later one (so any future
  shadowing fails CI, not just the iidx pair), another pins the IIDX order.

Nothing persists a profile INDEX - the Setup dropdown maps its selection
through `slug` in both directions - so the list can be reordered freely as
long as the shadowing rule holds.

## The shipped profiles

### IIDX 27+ (RE'd on Sparkle Shower) - slug `iidx33`, dir hint "iidx"

Reference target. Default ordinals, 1920x1080, kIidx33Offsets, all gates
default-true - its boot sequence is the renderer's reference; nothing to
override.

### IIDX 25-26 (RE'd on Rootage) - slug `iidx26`, dir hint "rootage"

DLLs: avs2-core 2.17.0 / afp-core 2.14.11 / afp-utils 1.2.12 (2018-era, vs
IIDX 33's avs2 2.17.4 / afp-core 2.14.18 / afp-utils 1.2.19). Same
XCd229cc / XE592acd / XCgsqzn export schemes and the SAME export counts
(126 / 123 / 392). The identity row sits BEFORE iidx33 in the registry ON
PURPOSE: AutoDetect returns the first dir_substring match, and every IIDX
dir matches iidx33's broad "iidx" hint - "rootage" must win first or the
Rootage dir boots with IIDX 27+ offsets (the original load-crash this
profile fixes). That is an instance of the general registry rule below. 1280x720 (Rootage-era cabinets are 720p; FHD IIDX arrived
with the Lightning Model era), kIidx26Offsets.

Ordinal maps: verified IDENTICAL to IIDX 33 for every export the renderer
resolves, via a pairwise decompile comparison of all 46 used afp-core
exports and all 36 used afp-utils exports across both builds (multi-agent
sweep; 82/82 same-function verdicts, no low-confidence). avs2-core 2.17.0's
suffix map likewise matches the avs_funcs.h ordinals - spot-verified by
decompiling the 26 build's exports for avs_boot ("avs-boot.c"),
property_create / property_node_create ("property-api.c", "node_create"),
and avs_fs_mount ("vfs-api-mount.c") at the same suffixes, plus a full
export-table diff (392 names in both).

Version drift found by the sweep (none affects the renderer's call
surface): afp-core 2.14.11 lacks ext commands 13-17, mc_control mode range
tops at 0x1039 vs 0x103D, and its set_flag refresh-trigger mask is 0x4011
vs 0x14011 (bit 0x10000 does not exist yet - see the flag-call list note
below). afp-utils 1.2.12's set_config has cases 1-8 only (no 9/10), and
its per-slot render array is 48 bytes vs 24.

The entire afp bring-up lives in ONE bm2dx function - find it via the xref
to the afp_boot import (afp-core name suffix 000002); every gate below is
read straight off that decompile/disasm. The sequence: afp_boot(ctx) with
a STATIC render-context blob (flags dword 0x200, callbacks at +0x008..
+0x068, allocator trio at +0x118..+0x130 - layout identical to
FillRenderContext's), afp_set_stream_nr(2048), afp_set_verbose(1) 1-arg,
afp_set_flag(0x10, 0x10), afp_set_flag(8, 8) - MIRRORED args, i.e. SET
those bits, and NO third 65537 call - afpu_boot(0, data) 2-arg with a NULL
config node, afpu_render_init(cfg), the afpu memory-hook install (afpu
suffix 000006, skipped by the renderer as on T44), D3D setup,
afpu_set_config(1, 4096), then afpu_set_flag(4, 0) - note the xor edx
CLEAR - afpu_set_flag(8, 8), afpu_set_flag(16, 16). bm2dx 26 never
imports afp_set_afp_data (0x000) nor afp_render_init (0x00f) at all, and
never calls afpu_set_config types 2/3.

Gate set and provenance:

- `call_afp_set_stream_nr = true` - game calls afp_set_stream_nr(2048).
- `call_afp_stream_create_test = false` - diagnostic probe; skip for safety.
- `call_afp_render_init = false` - bm2dx 26 does not import afp-core 0x00f.
- `call_afpu_render_init = true` - game calls afpu_render_init.
- `call_afpu_set_config = true` - game calls (1, 4096). The renderer's
  extra (2, 10) hits 1.2.12's case 2 (max_nr_masks resize, identical to
  IIDX 33's) and (3, 0) hits case 3, where value 0 installs a NULL
  cleanup callback (values 1/2 install real cleanup routines) - so the
  safe_clean_pos override below makes case 3 a no-op, matching the game
  never calling it.
- `call_afpu_set_flag_setup = true` with
  `afpu_set_flag_calls = {(4,0), (8,8), (16,16)}` - the game's exact
  pairs; the first call CLEARS afpu bit 4 where the default list sets it.
- `call_afpu_boot = true` - game calls afpu_boot(NULL, data). The
  renderer passes its max_nr_masks=16 property instead; 1.2.12's
  afpu_boot runs the same property_psmap_import path (2 psmap fields
  fewer than 1.2.19, none of them ours).
- `afpu_set_config_safe_clean_pos = true` - pass (3, 0), see above.
- `call_afp_set_flag_setup = true` with
  `afp_set_flag_calls = {(16,16), (8,8)}` - the game's exact mirrored
  pairs. NO 65537: afp-core 2.14.11's refresh-trigger mask is 0x4011
  (bit 0x10000 arrived by 2.14.18), so the bm2dx-33 third call addresses
  a flag bit that does not exist in this build.
- `apply_iidx_data_segment_patches = true` - kIidx26Offsets are correct;
  needed for the poke + slot re-patch.
- `afp_set_afp_data_wide_args = false` - afp_set_afp_data is 1-arg in
  2.14.11 (seen directly in its decompile).
- `afp_set_verbose_wide_args = false` - game calls afp_set_verbose(1) 1-arg.
- `scan_arc_containers = false` - loose .ifs (modern layout, DLLs and
  data/ in the game root; no modules/ subdir).
- `skip_explicit_afp_set_afp_data = true` - like gdxg/T44 the game relies
  solely on afpu_render_init's internal rebind-path call (0x800 left set).

### SDVX 7 (NABLA) - slug `sdvx7`, dir hint "sdvx"

Ordinal map confirmed == IIDX 33 (live trace + IDA RE); the actual divergence
is data-segment addresses (kSdvx7Offsets). 1080x1920 portrait default.
Gate values, with the debugging history that produced them:

- `call_afp_set_stream_nr = true` - re-enabled: the live trace shows
  afp_set_stream_nr(4096) works fine when called directly after afp_boot;
  the prior crashes were caused by intermediate calls (now gated) corrupting
  heap state.
- `call_afp_stream_create_test = false` - SDVX 7's afp_stream_create takes
  3-4 args; the renderer's 0-arg probe AVs on register junk. Skip until the
  typedef is fixed.
- `call_afp_render_init = true` and `call_afpu_render_init = true` -
  re-enabled. The earlier rationale ("the SDVX game doesn't call this from
  its IAT") was a red herring - the actual crash cause was IIDX-tuned
  data-segment offsets corrupting random bytes. With per-game DllOffsetSet
  routing the pokes through correct SDVX addresses, these are safe again.
  SDVX afpu_render_init (ord 0x070) actually INTERNALLY calls
  afp_set_afp_data, so without it the callback-table install path skips the
  proper "init first dword from render_ctx" step that fills
  the data-struct's first dword.
- `call_afpu_set_config = true` - RE-ENABLED with the safe value override.
  SDVX afpu's vertex-buffer allocator (afpu_render_get_buf)
  computes its capacity as `24 * <a config-set dword>`, where
  that dword is set by afpu_set_config(1, value). Skipping the call
  leaves capacity 0, so every vertex write overflows and AVs
  inside the matrix-transform helper. The earlier hypothesis that case 3 (=1) corrupted things
  was correct but only PARTIALLY: case 1 is REQUIRED, case 3 is optional and
  replaceable - hence keep the call and pass
  `afpu_set_config_safe_clean_pos = true` to neutralise case 3 (0 = safe
  default cleanup callback).
- `call_afpu_set_flag_setup = false` - live trace shows totally different
  afpu_set_flag args on SDVX (0x1, 0x1000); a true shape difference, not a
  corruption mismatch. Kept skipped until the trace is re-captured with
  corrected ordinals.
- `call_afpu_boot = true` - re-enabled: afpu_boot at SDVX ord 0x000
  (export XE592acd000000) DOES exist and does the standard "import
  config, populate flags, init internal state" work. The earlier "not
  exported" finding came from a wrong-ordinal probe in boot_trace.cpp (it was
  probing 0x002).
- `call_afp_set_flag_setup = false` - SDVX uses a (val, mask) shape not fully
  understood yet; skip rather than risk wrong values.
- `apply_iidx_data_segment_patches = true` - originally gated off when the
  renderer used IIDX hardcoded addresses on SDVX; now that DllOffsetSet
  routes every poke through kSdvx7Offsets the block is re-enabled and writes
  to SDVX-correct locations.
- `afp_set_afp_data_wide_args = true` (4-arg SDVX shape) and
  `afp_set_verbose_wide_args = true` (2-arg shape).

(Header history note: before the ordinal map was confirmed, the SDVX entry
was documented as "PLACEHOLDER ordinals equal to IIDX's, marked TODO" whose
value was (a) auto-detect recognition and (b) gating known-AV calls. That
phase is over - the map is confirmed equal - but the note explains older
comments elsewhere.)

### DDR World (MDX) - slug `ddrworld`, dir hint "mdx"

DLLs: libavs-win64.dll / libafp-win64.dll / libafputils-win64.dll (legacy AFP
2.13.7 + avs 2.16.3 + libafputils). The afp ordinal map is UNUSED for DDR -
`legacy_afp = true` routes the whole boot/load/render through DdrAfp, which
resolves its own readable-export func tables. `scan_arc_containers = true`
surfaces the inner .ifs of each data/arc/**.arc (LZ77-wrapped one-per-.arc
under data/arc/bm2d/). Render 1280x720 - DDR backgrounds are authored at
1280x720 and the DDR path pins the window to it (the verts afp emits are
screen-space pixels). offsets = kIidx33Offsets (irrelevant on the legacy
path). time_scale = 1.0 (see the field doc above for the full 1.0596
calibration post-mortem).

### GITADORA DELTA - slug `gitadora`, dir hint "delta"

dir_substring "delta" (a game dir like <drive>:\GD\delta matches; --profile
overrides). Default DLL names (avs2-core / afp-core / afp-utils) are correct.
Ordinal map confirmed == IIDX/SDVX (via gdxg imports). Main screen is
authored 4K landscape: 3840x2160. offsets = kGitadoraDeltaOffsets.

Gates traced from gdxg's own boot function
(from the GITADORA DELTA boot sequence). GITADORA == IIDX defaults EXCEPT
three fields; per-gate provenance:

- `call_afp_set_stream_nr = true` - game calls afp_set_stream_nr(2048).
- `call_afp_stream_create_test = false` - diagnostic probe; skip for safety.
- `call_afp_render_init = false` - the game never calls afp-core 0x00f
  (skipping did not affect the vertex-collapse issue).
- `call_afpu_render_init = true` - game calls afpu_render_init.
- `call_afpu_set_config = true` - game calls (1, 4096) and (2, 10).
- `call_afpu_set_flag_setup = true` - game calls afpu 0x003 with 4/8/16.
- `call_afpu_boot = true` - game calls afpu_boot(config, data) 2-arg.
- `afpu_set_config_safe_clean_pos = true` - game never calls set_config(3),
  so pass 0.
- `call_afp_set_flag_setup = true` - game calls 16/8/65537 (the bm2dx
  triple).
- `apply_iidx_data_segment_patches = true` - DELTA offsets are correct;
  needed for the poke + slot re-patch.
- `afp_set_afp_data_wide_args = false` - afpu_render_init's internal call is
  1-arg.
- `afp_set_verbose_wide_args = false` - game calls afp_set_verbose(1) 1-arg.
- `legacy_afp` / `scan_arc_containers` stay false (modern path, loose .ifs).
- `skip_explicit_afp_set_afp_data = true` - match gdxg: rely on
  afpu_render_init's internal afp_set_afp_data (rebind path, 0x800 left set);
  skip the renderer's memcpy-path call.

### jubeat (T44) - slug `t44`, dir hint "t44"

Game DLL jubeat2019.dll; default DLL names (avs2-core / afp-core / afp-utils)
are correct. afp-core 2.14.19 / afp-utils 1.2.20. Native render is 1080x1920
PORTRAIT at 60 Hz - both hardcoded in the game's InitD3D routine (find it via
the "InitD3D" log-tag string): the default mode constant packs 1080/1920, the
fullscreen mode enumeration filters adapter modes on RefreshRate == 60, and
FullScreen_RefreshRateInHz is set to 60. Renderable content is loose .ifs
under data/graphics (per-scene t44_*.ifs plus common/marker and common/font).

The entire afp bring-up lives in ONE game function - find it via the xref to
the afp_boot import (afp-core name suffix 000002); every gate below is read
straight off that decompile. The sequence: afp_boot(cfg),
afp_set_stream_nr(2048), afp_set_verbose(1), afp_set_flag 16/8/65537, build a
property with /config/render/max_nr_masks = 16, afpu_boot(config_node, data)
2-arg, afpu_render_init(cfg), afpu memory-hook install (afpu name suffix
000006 - optional allocator callbacks, defaults are fine so the renderer
skips it), D3D shader/texture setup, afpu_set_config(1, 4096),
afpu_set_flag 4/8/16, afpu_set_config(2, 10).

Gate set == GITADORA DELTA's exactly; per-gate provenance:

- `call_afp_set_stream_nr = true` - game calls afp_set_stream_nr(2048).
- `call_afp_stream_create_test = false` - diagnostic probe; skip for safety.
- `call_afp_render_init = false` - the boot function never calls afp-core
  0x00f; the game touches it only inside a device-reset helper pair
  (afp_render_init -> per-stream afp_do_update -> afp_render_destroy).
- `call_afpu_render_init = true` - game calls afpu_render_init(cfg).
- `call_afpu_set_config = true` - game calls (1, 4096) and (2, 10), the same
  values the renderer passes.
- `call_afpu_set_flag_setup = true` - game calls afpu 0x003 with 4/8/16.
- `call_afpu_boot = true` - game calls afpu_boot(config, data) 2-arg with
  /config/render/max_nr_masks = 16, exactly the property the renderer builds.
- `afpu_set_config_safe_clean_pos = true` - game never calls set_config(3),
  so pass 0.
- `call_afp_set_flag_setup = true` - game calls 16/8/65537 (the bm2dx
  triple).
- `apply_iidx_data_segment_patches = true` - T44 offsets are correct; needed
  for the poke + slot re-patch.
- `afp_set_afp_data_wide_args = false` - afpu_render_init's internal
  afp_set_afp_data call is 1-arg.
- `afp_set_verbose_wide_args = false` - game calls afp_set_verbose(1) 1-arg.
- `legacy_afp` / `scan_arc_containers` stay false (modern path, loose .ifs).
- `skip_explicit_afp_set_afp_data = true` - the game never even imports
  afp-core 0x000; like gdxg it relies solely on afpu_render_init's internal
  rebind-path call.

## Scene presets (`src/preset/`) and build fingerprints (`src/game_fingerprint.h`)

A game profile says which backend and resolution a game needs. A **scene preset**
goes one level further: it is the recipe for reproducing ONE game screen exactly
as the game draws it, for one identified build.

`GameFingerprint::Identify` walks the game directory (three levels deep) looking
for each known build's key file, and matches on size plus CRC32. An exact match
gives the build id; a size-only match is reported as a patched copy and still
resolves, because hooks and patch tools rewrite bytes in the executable. This is
independent of the folder name, which the directory-substring detection in
`game_profile.cpp` relies on and which users rename freely.

`Preset::Scene` carries what the data files cannot: which model of which scene
directory is visible, its blend mode, tint alpha, animation speed and transform,
the fixed camera and projection, the directional lights, the shading style, an
optional 2D sprite layer, and an optional frame countdown. `PresetHost::Load`
resolves the relative paths against the game root, drives `Scene3dHost` through
`LoadWithSetup` and `Gc2dHost` for the 2D layer, and `PresetHost::Advance` ticks
the countdown once per rendered frame.

Two shading styles exist because the fixed-function setup is per engine build:

- `TextureOnly` is the asset browser's own choice - unlit, `COLOROP =
  SELECTARG1(TEXTURE)`, cull none. It is what the scene viewer has always done
  and it is NOT a claim about any game's state.
- `LitMaterial` is IIDX 10's actual D3D8 state block, decoded from the binary:
  `LIGHTING` on with `AMBIENT` 0, `COLOROP = MODULATE(TEXTURE, DIFFUSE)`, a
  `D3DMATERIAL9` per material subset, cull CCW, `ZFUNC LESSEQUAL`, `MINFILTER`
  point. Full table and the per-screen values:
  `IIDX/tenth_style_music_select.md` in the notes repo.

Run one headless:

```bash
573Renderer.exe --preset-test <iidx10-install-dir> iidx10-music-select out.png 900
```

The build is identified from the directory, the preset id selects the screen
(omit it for the build's first preset), and the frame count drives the countdown
so the last-ten-seconds speed-up can be captured.

Each animated sprite layer carries a `GcAnim::Timing`: the game's playback mode
(loop, hold the last frame, or hide once the timeline ends) plus an optional
`[loop_start, loop_end)` range for the screens that rewind a playhead themselves.
`GcAnim::ResolveFrame` maps the layer's raw counter through it, so a one-shot
that the game freezes stays frozen instead of snapping back to its first frame.
The values are per screen and come out of the binary, never a guess: see the
"Playback modes" section of `IIDX/tenth_style_music_select.md`.

A preset may omit the 3D layer entirely, in which case `PresetHost` skips the 3D
host and only the sprite layers and the countdown run. No IIDX 10 screen needs
that yet: the game's model slots are global state that survives a screen change,
so a screen whose own code never mentions a model can still be showing one it
inherited. Which model a screen shows is a property of the PATH INTO it, and the
preset has to carry the state the previous screen left behind - see
`IIDX/tenth_style_card_in.md`. Registered IIDX 10 screens:

| id | content | natural length |
|---|---|---|
| `iidx10-music-select` | `music_bg` 3D (opaque, rotated) + 6 sprite layers | 1800 frames |
| `iidx10-music-select-samurai` | `samurai` 3D, unrotated + the same 6 layers | 1800 frames |
| `iidx10-card-in` | `music_bg` 3D (blend 3, alpha 0.5) + 7 sprite layers | 3600 frames |
| `iidx10-login` | `music_bg` 3D (alpha 0.8) + `LOGIN` | model timeline |
| `iidx10-mode-select` | `cube_x` 3D, spinning, placed per selected mode + 3 sprite layers | 1200 frames |
| `iidx10-dan-select` | `cube_x` 3D, orbiting + 2 sprite layers | 1200 frames |
| `iidx10-expert-select` | `ex01` 3D, entry ramp + end ramp + 2 sprite layers | 1800 frames |
| `iidx10-new-player` | `tran_box` 3D, orbiting + 3 sprite layers | 1200 frames |
| `iidx10-game-over` | `music_bg` 3D fading out + `GAMEOVER` | 180 frames |

The play screen also renders `music_bg`, for songs that have no movie, but it
draws it with blend mode 4 - reverse subtract, destination minus source - so
the model only darkens whatever the play HUD puts behind it. On its own it is
black by construction, so there is no preset for it.

That list is the complete set of IIDX 10 screens that render a model: it comes
from every `SetVisible(slot, non-zero)` call in the binary, which is the only way
a model becomes visible. The derivation is in `IIDX/tenth_style_3d_screens.md`.

Per-frame behaviours the game recomputes are carried as data rather than baked
into a screenshot-matching constant. `Preset::ModelMotion` holds the orbit +
fly-in the class-course and new-player screens apply to their slot transform, plus
a per-axis `spin_per_frame` with an optional decaying `spin_kick` - that is how
mode select's cube keeps turning and how it lurches when the selection changes.
`Preset::Intro` is a speed ramp over the first N frames (expert select spins its
model backwards for 22 frames before settling). All of them are transcriptions of
the game's own formulas.

### Preset options

A preset can expose `Preset::Option`s: a named list of choices the viewer can
switch between, each supplying a model position. They exist because the game
itself moves the model in response to the player - mode select places its cube
somewhere different for every entry in the mode menu - so a single fixed
placement would only ever be one sixth of that screen. `PresetHost::SetOption`
runs the game's own transition when the choice changes (mode select lerps over 25
frames and kicks the spin in the direction of the turntable move), the GUI draws
one combo per option in the Screens tab, and the CLI takes the choice index as
the last argument:

```bash
573Renderer.exe --preset-test <iidx10-install-dir> iidx10-mode-select out.png 120 3
```

Layers can also mark `ui_parts`: names of cells or nested child animations INSIDE
an animation that belong to the screen's chrome rather than its background. The
"Show UI layers" toggle hides them, which is how mode select's backdrop renders
without the `MODE SELECT` title, the marquee and the `INFORMATION` bar that share
its one `MODE_BG_LOOP` animation.

In the GUI the same presets appear as a **Screens** tab in the inspector,
which is visible whenever the loaded directory fingerprints to a build that has
presets. It lists the build's screens, loads one on click, and exposes the
countdown as a slider so the end-of-timer ramp can be scrubbed.
