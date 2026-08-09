# r573_state (src/state/)

`App::State` is the shared state between the render thread and the ImGui GUI
thread (plus the render window's WndProc for crop picking). One mutex
serializes everything; both sides take copies under the lock. `App::Global()`
is the process-wide singleton, but the class is a plain constructible object,
which is what the unit tests instantiate. The render thread drains the
pending command each frame; the GUI posts commands from widget callbacks;
the render thread publishes `Status` / `LiveState` / `LoadProgress` back.

The command queue (`fifo_queue.h`) is a mutex-guarded FIFO deque: posts
never overwrite each other, and the render thread takes one command per
frame in post order.

Known design debts scheduled for later phases (do not "fix" casually):
the qpro command payloads couple `backend/afp_commands.h` to qpro_model.h (a
later phase moves qpro behind a RenderService seam; the coupling no longer
touches app_state.h);
`ShouldExit` is atomic so both threads can flip it without the mutex - the
GUI window closing does NOT tear down the renderer (it is a control panel),
only the render window close does.

## Command semantics (P13 typed commands)

`App::Command` (`state/commands.h`) is a `std::variant` of app-level
commands; backend-specific commands travel as
`Cmd::BackendCommand{std::any}` whose payload is a backend-defined closed
variant. The AFP family's variant is `AfpCmd::Any`
(`backend/afp_commands.h`); `AfpCmd::Wrap(cmd)` builds the wrapped
`App::Command`.
Posting is `State::PostCommand`, draining is `State::TakeCommand`
(`std::optional`, one per render-loop tick). The dispatcher
(`render_loop_requests.cpp DispatchAppCommand`) is a pair of `std::visit`
visitors, not an if/else chain; adding a backend command never edits a
shared state header. `main.cpp WaitForFirstBoot` consumes only
`Cmd::BootGame` before the render loop exists.

App-level commands and their non-obvious contracts:

- `Cmd::LoadContent{path, from_arc}`: hot-swap. `from_arc` = the path is a
  DDR .arc container whose inner .ifs must be decompressed to a temp file
  first, mirroring `IfsEntry::from_arc`.
- `Cmd::BootGame{game_dir, profile_slug, render_width/height}` (0 = "use
  App::Global / settings.ini"; empty or unrecognized slug = auto-detect
  from the directory). Ignored with a log if it arrives after boot.
- `Cmd::StartExport{ExportRequest}` / `Cmd::CancelExport`: `ExportRequest`
  mirrors the GUI export panel and cli.md's export options; `format` stays
  an int (MediaSink::Format index) so the struct remains trivially
  populated without pulling media headers. The AFP-only knobs
  (`loop_count`, `blend_frames`, `blend_loop`) remain in the generic
  request until the P16 state generalization moves them to the AFP UI
  state block.

AFP commands (`AfpCmd::Any`):

- `SwitchAnimation{name, label}` (optional label: after the switch,
  deep-goto that label via `afp_mc_control(stream, 0xF09)` - what SDVX
  scene lambdas do for intro+loop backgrounds). `GotoLabel{name}` is the
  live variant without re-switching.
- `SeekFrame{frame}`: CAfpViewerScene LEFT/RIGHT seek (`afp_mc_control
  0xF08`), pauses on seek; IGNORED while an export captures so a backward
  seek cannot corrupt the loop-wrap counter. `SetPaused{paused}`: stream
  speed 0/1; ignored while exporting.
- `ForceReplay`: destroy + replay the master. Exists because
  afp_play_work_load_bitmap has no "unset" - the only way to revert a slot
  to its authored bitmap is a full re-author; other slots with a latched
  override get re-applied by ApplyVariantSlots on the next frame.
- `QproStartExtract{out_dir, part_sel, parts, fps, hue_scope}` (hue_scope
  scopes the per-effect afp hue filter to the effect bitmap so a static
  base fill like the gold sword hilt is not hue-shifted - matches the live
  game, default on); `QproStartScan` reads bm2dx.dll's part arrays for the
  selection list.

## VariantSlot / IfsConfig

- `VariantSlot.bitmap` = name actively applied (empty = leave unchanged);
  `default_bitmap` = restore target for "(default)", populated at
  slot-discovery from the clip path heuristic (clip name == authored bitmap
  name for title.ifs-style slots); `bitmap_override` LATCHES on first user
  pick because there is no unset path - once touched, keep re-writing every
  frame to beat PlaceObject re-application from the timeline.
- The former `CompanionIfs` locale-overlay struct (`<base>_j/_a/_k.ifs`
  inferred by naming rule, exclusive GUI selection) was removed with the
  locale-overlay feature; the engine-level companion-package machinery it
  used lives on for qpro (docs/qpro.md). The bm2dx engine fact survives in
  docs/boot_and_render_loop.md "Companions".
- `IfsConfig.sublayer_overrides` is distinct from `slots`: slots come from
  ProbeSlots (afplist + bitmap names + a Konami name list); sublayer
  overrides come from live child enumeration (recursive "parent/child"
  paths), only user-touched paths appear, everything else keeps its
  authored `_visible`. THREAD RULE: render thread reads via
  `GetSublayerOverrides` (copy under lock), GUI writes via
  `SetSublayerOverride` (upsert under lock). `configs_` is a `std::deque`
  precisely so that the `IfsConfig&` handed out by `MutConfig` (and the
  pointer from `FindConfig`) stays valid when the other thread's
  emplace_back grows the container - a vector reallocation would dangle
  every escaped reference, which was a real cross-thread use-after-free.
  Stable element addresses do NOT license cross-thread field access:
  MutConfig releases the lock on return, so never iterate an inner vector
  (e.g. `sublayer_overrides`) through `MutConfig` from the render thread
  while the GUI may upsert it - use the copying accessors.

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

- `ActiveBackendId` (BootLifecycle) is boot-seeded ONCE (from
  `Backend::Active()->Id()` in BootFromGameDir) before the GUI reads it -
  it exists so GUI TUs can key panel visibility off the backend without
  including engine headers. It replaced the old `IsDdrMode` bool in P16;
  the P17 panel registry keys its per-backend panel sets on it.
- `Status::scene_loaded` is the generic "a scene is mounted and renderable"
  flag published by whichever backend publishes `Status`; the GUI never
  interprets `Status::stream_id` (an AFP diagnostic) anymore.
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
