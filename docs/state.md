# r573_state (src/state/)

`App::State` is the state shared between the render thread and the ImGui GUI
thread (plus the render window's WndProc for crop picking). One mutex
serializes everything, and both sides take copies under the lock. `App::Global()`
is the process-wide singleton, but the class is a plain constructible object,
and that is what the unit tests instantiate. The render thread drains the
pending command each frame, the GUI posts commands from widget callbacks, and
the render thread publishes `Status` / `LiveState` / `LoadProgress` back.

The command queue (`fifo_queue.h`) is a mutex-guarded FIFO deque. Posts
never overwrite each other, and the render thread takes one command per
frame in post order.

Known design debts are scheduled for later phases, so leave them alone unless
that phase is the task. The qpro command payloads couple `backend/afp_commands.h`
to qpro_model.h (a later phase moves qpro behind a RenderService seam; the
coupling no longer touches app_state.h).
`ShouldExit` is atomic so both threads can flip it without the mutex. Closing
the GUI window does not tear down the renderer (the GUI is a control panel);
only closing the render window does.

## Command semantics (P13 typed commands)

`App::Command` (`state/commands.h`) is a `std::variant` of app-level
commands. Backend-specific commands travel as
`Cmd::BackendCommand{std::any}`, whose payload is a closed variant the backend
defines. The AFP family's variant is `AfpCmd::Any`
(`backend/afp_commands.h`), and `AfpCmd::Wrap(cmd)` builds the wrapped
`App::Command`.
Posting is `State::PostCommand` and draining is `State::TakeCommand`
(`std::optional`, one per render-loop tick). The dispatcher
(`render_loop_requests.cpp DispatchAppCommand`) is a pair of `std::visit`
visitors, so adding a backend command never edits a
shared state header. `main.cpp WaitForFirstBoot` consumes only
`Cmd::BootGame` before the render loop exists.

App-level commands and the parts of their contracts that are easy to miss:

- `Cmd::LoadContent{path, from_arc}`: hot-swap. `from_arc` means the path is a
  DDR .arc container whose inner .ifs must be decompressed to a temp file
  first, mirroring `IfsEntry::from_arc`.
- `Cmd::BootGame{game_dir, profile_slug, render_width/height}` (0 = "use
  App::Global / settings.ini"; an empty or unrecognized slug = auto-detect
  from the directory). If it arrives after boot it is ignored and logged.
- `Cmd::StartExport{ExportRequest}` / `Cmd::CancelExport`: `ExportRequest`
  mirrors the GUI export panel and cli.md's export options. `format` stays
  an int (MediaSink::Format index) so the struct can be filled without
  pulling in media headers. The AFP-only knobs
  (`loop_count`, `blend_frames`, `blend_loop`) stay in the generic
  request until the P16 state generalization moves them to the AFP UI
  state block.

AFP commands (`AfpCmd::Any`):

- `SwitchAnimation{name, label}` (optional label: after the switch,
  deep-goto that label via `afp_mc_control(stream, 0xF09)`, which is what SDVX
  scene lambdas do for intro+loop backgrounds). `GotoLabel{name}` is the
  live variant that does not switch again.
- `SeekFrame{frame}`: the CAfpViewerScene LEFT/RIGHT seek (`afp_mc_control
  0xF08`), which pauses on seek. It is ignored while an export captures, so a
  backward seek cannot corrupt the loop-wrap counter. `SetPaused{paused}`: stream
  speed 0/1; ignored while exporting.
- `ForceReplay`: destroys and replays the master. It exists because
  afp_play_work_load_bitmap has no "unset", so the only way to revert a slot
  to its authored bitmap is a full re-author. Other slots with a latched
  override are re-applied by ApplyVariantSlots on the next frame.
- `QproStartExtract{out_dir, part_sel, parts, fps, hue_scope}` (hue_scope
  limits the per-effect afp hue filter to the effect bitmap, so a static
  base fill like the gold sword hilt is not hue-shifted; this matches the live
  game and is on by default). `QproStartScan` reads bm2dx.dll's part arrays for the
  selection list.

## VariantSlot / IfsConfig

- `VariantSlot.bitmap` is the name being applied (empty = leave unchanged).
  `default_bitmap` is the restore target for "(default)", filled in at
  slot discovery from the clip path heuristic (clip name == authored bitmap
  name for title.ifs-style slots). `bitmap_override` latches on the first user
  pick because there is no unset path: once touched, the slot is rewritten every
  frame so the timeline's PlaceObject cannot re-apply the authored bitmap.
- The former `CompanionIfs` locale-overlay struct (`<base>_j/_a/_k.ifs`
  inferred by naming rule, exclusive GUI selection) was removed along with the
  locale-overlay feature. Its successor keeps no per-IfsConfig state. The
  packages loaded alongside live in `EngineSession::overlays` (path +
  pkg_id, owned by the engine) and reach the GUI as `Status::overlay_ifs`, a plain
  path list the render thread republishes whenever one is loaded or
  unloaded and on every scene load. The hot-swap handler clears it,
  because `UnloadContent` drops the packages themselves. The bm2dx engine
  fact is kept in docs/boot_and_render_loop.md "Companions".
- `IfsConfig.sublayer_overrides` is separate from `slots`. Slots come from
  ProbeSlots (afplist + bitmap names + a Konami name list). Sublayer
  overrides come from live child enumeration (recursive "parent/child"
  paths); only paths the user touched appear, and everything else keeps its
  authored `_visible`. Thread rule: the render thread reads via
  `GetSublayerOverrides` (copy under lock) and the GUI writes via
  `SetSublayerOverride` (upsert under lock). `configs_` is a `std::deque`
  so that the `IfsConfig&` handed out by `MutConfig` (and the
  pointer from `FindConfig`) stays valid when the other thread's
  emplace_back grows the container. A vector reallocation would leave every
  escaped reference dangling, and that was a real cross-thread use-after-free.
  Stable element addresses do not make cross-thread field access safe:
  MutConfig releases the lock on return, so never iterate an inner vector
  (e.g. `sublayer_overrides`) through `MutConfig` from the render thread
  while the GUI may upsert it. Use the copying accessors.

## Status / label playback

`Status.labels` holds the master clip frame labels (EnumerateLabels after each
switch). `mc_children` holds the child-clip names while the F3-style overlay is on.
`mc_tree` is the lazy sub-layer tree: a node's children are filled only once
the user expands it (its path enters the expanded set).

`active_label` / `label_playback_active` are set whenever GotoLabel runs with a
non-empty label, and cleared on a plain switch, force_replay, hot-swap or unload.
A label plays via deep-goto-play (0xF09) with the clip's authored loop
state, then stops itself at its authored stop(), where the bounded movie-clip
playhead freezes. An export therefore tracks the playhead and stops at the
freeze. It must not drive the continuous-loop flag sequence, which makes
the free-running stream counter climb past total forever.

## RootLoopMode (Hold vs Force) - the full mechanism

Verified in afp-core:

- The "dance" is the real game BG dispatcher's CLayer flag maintenance.
  Without it the master clock stops at end-of-timeline (the time-feed
  routine stops feeding time) and the whole scene freezes,
  children included.
- With the dance the root keeps ticking. At its end the engine
  shallow-seeks: the shallow-seek routine re-runs frame 0's
  PlaceObject, but the existing child at that depth is reused instead of
  recreated (the placed-frame match), so the child
  playhead is preserved and keeps running freely. Children are reset only by
  the deep-wrap (gated by the root LOOP
  flag work+0x24 bit 0x100) or by a renderer ForceReplay (afp_stream_destroy
  + afp_stream_play, a full remount). That remount is the
  o_kazari5 snap.
- Hold (game default) = dance with no ForceReplay. Force = dance plus a
  ForceReplay per master-complete, for one-shot masters (bg_common
  full-clip export) or user loops made on purpose.

The renderer mounts via afplist and cannot read the dispatcher's per-BG
decision at runtime. Under the no-hardcode rule this is therefore an explicit
control that defaults to the game default (Hold). main.cpp seeds it from the
settings key root_loop, which is the authority; see docs/settings.md for the
history of the two bugs.

## LiveOverrides / LiveState provenance

LiveOverrides are per-session scratch and are not persisted:

- continuous-loop mode: -1 explicit off / 0 engine default / 1 on (the SDVX
  CLayer per-tick dance + bit-0 re-set).
- trim_frames: ForceReplay at frame N to preview a trimmed loop.
- bg_color_index: -1 default transparent; 0..4 pick from `kBgPresets`, the IIDX
  debug viewer's F4 cycle (the bm2dx preset array read raw):
  grey/black/red/green/blue. Slot 2 is opaque red 0xFFFF0000.
- filter_enabled (F7): afp-core ord 0x032 with id 0x80000000|1, re-applied on each
  stream switch.
- paused: re-applied on each frame outside export so the engine
  cannot resume on its own.
- show_mc_names + mc_name_type (F3/F6).

LiveState fields and where they come from:

- `cur_pos`/`total_length`/`flags0`: `afp_get_layer_info` words 13/12. This is
  the free-running counter, which runs past total under the continuous-loop dance.
- `mc_cur`/`mc_total`: the bounded movie-clip playhead (work+0x76 via
  afp_mc_set, the same source the IIDX viewer's [TIME] uses) and the whole-clip
  frame count. `have_mc_playhead` is set whenever the read succeeds, whether or
  not a label is active.
- `mc_loop_count`: raw afp loop_count (work+0x104, afp_mc_set 0x1013).
  Diagnostic only. It increments only on a goto or wrap whose target frame
  is 0, so it bumps once just from selecting a frame-0 label (win@0) with
  no playback and never bumps for a non-zero loop label (loop_win@120).
  The debug scene never displays it. The count to use is
  `mc_wrap_count`: backward wraps of the bounded playhead since the clip or
  label last (re)started, reset on label select, stream switch and
  ForceReplay. The label export uses the same definition.
- `mc_w`/`mc_h`: layer_info words 8/9 (work+0x20/0x22), the [SIZE] readout.
- FILE INFO: version DWORDs from afp-utils `afpuloc_get_package_info(pkg,
  sel)` sel 1/3/4, unpacked as major=(v>>16)&0xFFFF, minor=(v>>8)&0xFF,
  patch=v&0xFF. `conv_engine` is the converter engine name (ord 0x03e sel 2,
  shown as "???" when empty). size_bytes/load_time_ms can be derived and are filled.
  The scene's `locale` line reads _LocaleUpdate::GetLocaleT, a
  bm2dx/game runtime concept with no afp/avs equivalent, so the renderer
  does not make one up (the GUI shows n/a).

## LoadProgress and the min-hold

Progress is fine-grained enough for a determinate bar: `textures_expected` is
parsed up front from texturelist.xml, `textures_loaded` is bumped from
AfpD3D9::TexCreate per allocated slot, and `GetLoadProgress` computes the
fraction on the reader side so the bump stays a cheap int++. The `detail` line
lets phases other than texture loading (the boot .ifs scan's climbing file count)
share the overlay; BeginLoad resets it.

`GetLoadProgress` enforces a minimum visible duration (`kLoadMinHoldMs` =
600). After EndLoad it keeps returning active=true with stage
"Finalizing" and the fraction pinned to 1.0 until the hold elapses, so a 30 ms
hot-swap cannot flash past a ~60 Hz GUI poll. EndLoad leaves
stage/target/counters as they are, so during the hold the overlay still shows
"Finalizing <file>" instead of blanks. BeginLoad clears the hold deadline.

The IFS scan runs on a detached thread (a big install used to block boot
for minutes). `ifs_scanning_` is atomic, the GUI shows the climbing count
while scanning, and CLI paths that need the full list call
`WaitForIfsScan`. It polls instead of waiting on a condition variable, since
only rare one-shot CLI automation uses it.

## Crop picking

`CropRect` and pick mode are shared by three threads: the GUI toggles pick mode,
WndProc changes the rect during the drag, and the D3D9 overlay draws it.
Lifecycle: "Pick region" -> crosshair cursor -> WM_LBUTTONDOWN captures ->
WM_MOUSEMOVE live-updates -> WM_LBUTTONUP releases and clears pick mode.
Coordinates are in offscreen-RT space (window.cpp translates window
coordinates to RT), and w==0||h==0 means "no crop". That matches the export crop
sentinel, so the pipeline needs one comparison to test it.

## Misc invariants

- `ActiveBackendId` (BootLifecycle) is seeded once at boot (from
  `Backend::Active()->Id()` in BootFromGameDir) before the GUI reads it.
  It lets GUI TUs key panel visibility off the backend without
  including engine headers. It replaced the old `IsDdrMode` bool in P16,
  and the P17 panel registry keys its per-backend panel sets on it.
- `Status::scene_loaded` is the generic "a scene is mounted and renderable"
  flag, published by whichever backend publishes `Status`. The GUI no longer
  interprets `Status::stream_id` (an AFP diagnostic).
- `SetMasterScale` clamps to 0.1..8.0 (the matrix stays numerically sane, and
  beyond 8x the layer is larger than any plausible viewport). See
  docs/settings.md for what master scale is.
- `SetRenderSize`/`SetRenderFps` ignore non-positive input (a 0x0 window
  would make D3D9 init fail silently).
- ExportState/ExportPhase: the GUI-visible mirror of the export state
  machine (Idle -> Capturing -> Encoding -> Done|Failed). bg colors are
  0..1 linear RGB, and capture does premultiplied source-over before encoding.
  `using_hardware` is published once the encoder exists so the GUI can
  show its HW badge.
