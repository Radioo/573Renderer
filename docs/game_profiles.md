# Game profiles (src/game_profile.h / src/game_profile.cpp)

The game-profile system encodes what differs between Konami game versions so
the renderer can boot the right ordinals + defaults per game.

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

Adding a profile:

1. Append a brace-init entry in game_profile.cpp.
2. Set name / slug / dir_substring uniquely (the "Auto" UI option picks it up
   if dir_substring is unique enough).
3. Override any AfpOrdinals field that diverges from IIDX 33; leave the rest
   as struct defaults (which match IIDX 33). Optionally tweak defaults
   (render size, data layout).
4. RE the new game's `afp_set_afp_data` (afp-core ord 0x000) and
   `afpu_render_init` (afp-utils ord 0x070) to derive a new DllOffsetSet
   constant (see below).
5. When adding a NEW ordinal field to AfpOrdinals, ALSO update afp_funcs.h's
   Load() to read from the profile's value rather than a hard-coded literal.

The registry uses C++20 designated initializers ON PURPOSE: field-name typos
surface as compiler errors instead of silently misaligned positional brace
init - which bit this file twice when fields were re-ordered. Designated
initializers must follow declaration order (e.g. SDVX's
`.afpu_set_config_safe_clean_pos` must appear between `.call_afpu_boot` and
`.call_afp_set_flag_setup`).

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

- `name` - human label (e.g. "IIDX 33 (Sparkle Shower)"); `slug` -
  settings.ini token; `dir_substring` - case-insensitive auto-detect hint.
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
- `call_afpu_set_flag_setup` - the bm2dx afpu_set_flag triple (4,4 / 8,8 /
  16,16). SDVX's trace shows only ONE afpu_set_flag call at boot with
  completely different args (0x1, 0x1000); the triple is skipped on SDVX
  entirely until the real SDVX flags are known (branch instead of skip,
  later).
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

## The four shipped profiles

### IIDX 33 (Sparkle Shower) - slug `iidx33`, dir hint "iidx"

Reference target. Default ordinals, 1920x1080, kIidx33Offsets, all gates
default-true - its boot sequence is the renderer's reference; nothing to
override.

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
