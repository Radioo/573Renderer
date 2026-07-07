# r573_state (src/state/)

`App::State` is the shared state between the render thread and the ImGui GUI
thread (plus the render window's WndProc for crop picking). One mutex
serializes everything; both sides take copies under the lock. `App::Global()`
is the process-wide singleton, but the class is a plain constructible object,
which is what the unit tests instantiate. The render thread reads the pending
`Request` each frame; the GUI writes requests from widget callbacks; the
render thread publishes `Status` / `LiveState` / `LoadProgress` back.

Known design debts scheduled for later phases (do not "fix" casually):
the one-deep request mailbox OVERWRITES a pending request (two posts within
one render-thread poll silently drop the first - P8 replaces it with a typed
command queue); the qpro request fields couple this header to
qpro_extract.h (P8 moves qpro behind a RenderService seam);
`ShouldExit` is atomic so both threads can flip it without the mutex - the
GUI window closing does NOT tear down the renderer (it is a control panel),
only the render window close does.

## Request semantics

One-shot commands, `std::optional` so "no request" is distinguishable from
"apply the same request twice". Field groups and their non-obvious contracts:

- `load_new_ifs` + `ifs_path` (+ `ifs_from_arc` for DDR: path is a .arc
  container whose inner .ifs must be decompressed to a temp file first,
  mirroring `IfsEntry::from_arc`).
- `set_game_dir` + `game_dir` + `render_width/height` (0 = "use App::Global
  / settings.ini" - handlers treat 0 as a sentinel) + `game_profile` (empty
  or unrecognized = auto-detect from the directory).
- qpro: `start_qpro_extract`/`qpro_out_dir`/`qpro_fps`/`qpro_hue_scope`
  (scope the per-effect afp hue filter to the effect bitmap so a static
  base fill like the gold sword hilt is not hue-shifted - matches the live
  game, default on)/`qpro_parts`/`qpro_part_sel`; `start_qpro_scan` reads
  bm2dx.dll's part arrays for the selection list.
- `switch_animation` + `animation_name` (+ optional `animation_label`:
  after the switch, deep-goto that label via `afp_mc_control(stream, 0xF09)`
  - what SDVX scene lambdas do for intro+loop backgrounds). `goto_label` +
  `goto_label_name` is the live variant without re-switching.
- `seek_frame` + `seek_to_frame`: CAfpViewerScene LEFT/RIGHT seek
  (`afp_mc_control 0xF08`), pauses on seek; IGNORED while an export
  captures so a backward seek cannot corrupt the loop-wrap counter.
  `set_paused`/`paused_value`: stream speed 0/1; ignored while exporting.
- `toggle_companion` + `companion_index`: invalid indexes silently ignored.
- `force_replay`: destroy + replay the master. Exists because
  afp_play_work_load_bitmap has no "unset" - the only way to revert a slot
  to its authored bitmap is a full re-author; other slots with a latched
  override get re-applied by ApplyVariants on the next frame.
- Export block: mirrors the GUI export panel and cli.md's export options;
  `export_format` stays an int (MediaSink::Format index) so the struct
  remains trivially populated without pulling media headers.

## VariantSlot / CompanionIfs / IfsConfig

- `VariantSlot.bitmap` = name actively applied (empty = leave unchanged);
  `default_bitmap` = restore target for "(default)", populated at
  slot-discovery from the clip path heuristic (clip name == authored bitmap
  name for title.ifs-style slots); `bitmap_override` LATCHES on first user
  pick because there is no unset path - once touched, keep re-writing every
  frame to beat PlaceObject re-application from the timeline.
- `CompanionIfs`: IIDX locale convention `<base>_j/_a/_k.ifs`. bm2dx keys a
  static per-scene table with these paths (the per-scene locale-path table
  function); the renderer infers them from the naming rule
  instead - deterministic for
  every shipping IFS observed. `pkg_id` is the AFPU package id while
  mounted (needed for UnloadCompanion).
- `IfsConfig.sublayer_overrides` is distinct from `slots`: slots come from
  ProbeSlots (afplist + bitmap names + a Konami name list); sublayer
  overrides come from live child enumeration (recursive "parent/child"
  paths), only user-touched paths appear, everything else keeps its
  authored `_visible`. THREAD RULE: render thread reads via
  `GetSublayerOverrides` (copy under lock), GUI writes via
  `SetSublayerOverride` (upsert under lock). Never iterate the vector
  through `MutConfig` from the render thread - MutConfig releases the lock
  on return, so the GUI's emplace_back can reallocate under the iteration.

## Status / label playback

`Status.labels` = master clip frame labels (EnumerateLabels after each
switch); `mc_children` = child-clip names while the F3-style overlay is on;
`mc_tree` = the LAZY sub-layer tree - a node's children are only filled once
the user expands it (its path enters the expanded set).

`active_label` / `label_playback_active`: set whenever GotoLabel runs with a
non-empty label; cleared on plain switch, force_replay, hot-swap/unload.
A label plays via deep-goto-play (0xF09) with the clip's authored loop
state, then self-stops at its authored stop(): the BOUNDED movie-clip
playhead FREEZES there. An export must therefore track the playhead and
stop at the freeze, NOT drive the continuous-loop flag sequence (which makes
the free-running stream counter climb past total forever).

## RootLoopMode (Hold vs Force) - the full mechanism

Verified in afp-core:

- The "dance" is the real game BG-dispatcher's CLayer flag maintenance.
  WITHOUT it the master clock stops at end-of-timeline (the time-feed
  routine stops feeding time) and the WHOLE scene freezes,
  children included.
- WITH the dance the root keeps ticking; at its end the engine
  shallow-seeks (the shallow-seek routine re-runs frame 0's
  PlaceObject, but the existing child at that depth is REUSED, not
  recreated - the placed-frame match - so the child
  playhead is preserved and it
  free-runs). Only the deep-wrap (gated by the root LOOP
  flag work+0x24 bit 0x100) or a renderer ForceReplay (afp_stream_destroy
  + afp_stream_play full remount) RESETS children - that remount is the
  o_kazari5 snap.
- Hold (game default) = dance + NO ForceReplay. Force = dance +
  ForceReplay per master-complete, for one-shot masters (bg_common
  full-clip export) or deliberate user loops.

The renderer mounts via afplist and cannot read the dispatcher's per-BG
decision at runtime, so per the no-hardcode rule this is an explicit
control defaulting to the game default (Hold). Seeded in main.cpp from
settings root_loop (authority); see docs/settings.md for the
two-bug history.

## LiveOverrides / LiveState provenance

LiveOverrides are per-session scratch (NOT persisted): continuous-loop mode
(-1 explicit off / 0 engine default / 1 on - the SDVX CLayer per-tick
dance + bit-0 re-set), trim_frames (ForceReplay at frame N to preview a
trimmed loop), bg_color_index (-1 default transparent; 0..4 pick from
`kBgPresets` - the IIDX debug viewer's F4 cycle (the bm2dx preset array
read raw): grey/black/red/green/blue, slot 2 is OPAQUE RED 0xFFFF0000),
filter_enabled (F7: afp-core ord 0x032 with id 0x80000000|1, re-applied per
stream switch), paused (re-applied each non-export frame so the engine
cannot quietly resume), show_mc_names + mc_name_type (F3/F6).

LiveState fields and where they come from:

- `cur_pos`/`total_length`/`flags0`: `afp_get_layer_info` words 13/12 - the
  FREE-RUNNING counter, runs past total under the continuous-loop dance.
- `mc_cur`/`mc_total`: the BOUNDED movie-clip playhead (work+0x76 via
  afp_mc_set, the same source the IIDX viewer's [TIME] uses) and whole-clip
  frame count. `have_mc_playhead` is set whenever the read succeeds, not
  gated on a label.
- `mc_loop_count`: raw afp loop_count (work+0x104, afp_mc_set 0x1013).
  DIAGNOSTIC ONLY: it increments solely on a goto/wrap whose TARGET frame
  is 0, so it bumps once just from selecting a frame-0 label (win@0) with
  no playback and never bumps for a non-zero loop label (loop_win@120).
  The debug scene never displays it. The meaningful tally is
  `mc_wrap_count`: backward wraps of the bounded playhead since the clip /
  label last (re)started, reset on label select, stream switch, and
  ForceReplay - the same definition the label export uses.
- `mc_w`/`mc_h`: layer_info words 8/9 (work+0x20/0x22), the [SIZE] readout.
- FILE INFO: version DWORDs from afp-utils `afpuloc_get_package_info(pkg,
  sel)` sel 1/3/4, unpacked major=(v>>16)&0xFFFF, minor=(v>>8)&0xFF,
  patch=v&0xFF; `conv_engine` = converter engine name (ord 0x03e sel 2,
  empty shown as "???"). size_bytes/load_time_ms are derivable and filled.
  The scene's `locale` line reads _LocaleUpdate::GetLocaleT - a
  bm2dx/game runtime concept with no afp/avs equivalent, so it is
  deliberately NOT invented here (GUI shows n/a).

## LoadProgress and the min-hold

Progress is granular enough for a determinate bar: `textures_expected` is
parsed up front from texturelist.xml, `textures_loaded` is bumped from
AfpD3D9::TexCreate per allocated slot, and `GetLoadProgress` computes the
fraction reader-side so the bump stays a cheap int++. The `detail` line
lets non-texture phases (the boot .ifs scan's climbing file count) share
the overlay; BeginLoad resets it.

`GetLoadProgress` enforces a minimum visible duration (`kLoadMinHoldMs` =
600): after EndLoad, it keeps returning active=true with stage
"Finalizing" and fraction pinned to 1.0 until the hold elapses, so a 30 ms
hot-swap cannot flash past a ~60 Hz GUI poll. EndLoad deliberately does
NOT clear stage/target/counters - the hold window still shows a coherent
"Finalizing <file>" instead of blanks. BeginLoad clears the hold deadline.

The IFS scan runs on a DETACHED thread (a big install used to block boot
for minutes): `ifs_scanning_` is atomic, the GUI shows the climbing count
while scanning, and CLI paths that need the full list call
`WaitForIfsScan` (polled, not condition-var'd - only rare one-shot CLI
automation uses it).

## Crop picking

`CropRect` + pick mode are shared by three threads (GUI toggles pick mode,
WndProc mutates the rect during drag, the D3D9 overlay draws it).
Lifecycle: "Pick region" -> crosshair cursor -> WM_LBUTTONDOWN captures ->
WM_MOUSEMOVE live-updates -> WM_LBUTTONUP releases AND clears pick mode.
Coordinates are in OFFSCREEN-RT space (window-to-RT translation happens in
window.cpp), and w==0||h==0 means "no crop", matching the export crop
sentinel so the pipeline's test is one comparison.

## Misc invariants

- `IsDdrMode` is boot-seeded ONCE (from the profile in BootFromGameDir)
  before the GUI reads it - it exists so GUI TUs can branch on
  capabilities without including app_globals.h (which drags windows.h /
  d3d9 through render_backend.h). It dies with g_ddr_mode in P5.
- `SetMasterScale` clamps to 0.1..8.0 (matrix stays numerically sane;
  beyond 8x the layer exceeds any plausible viewport). See
  docs/settings.md for what master scale is.
- `SetRenderSize`/`SetRenderFps` ignore non-positive input (a 0x0 window
  would tank D3D9 init silently).
- ExportState/ExportPhase: the GUI-visible mirror of the export state
  machine (Idle -> Capturing -> Encoding -> Done|Failed); bg colors are
  0..1 linear RGB, capture does premultiplied source-over before encoding;
  `using_hardware` is published once the encoder exists so the GUI can
  show its HW badge.
