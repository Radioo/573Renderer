# Game runtime abstraction (src/game_runtime.{h,cpp})

`Runtime::IGameRuntime` is the interface over the two engine generations the
renderer drives:

- the modern avs2 path (`AfpManager` + the `g_afp` proc table), used by
  IIDX, SDVX, and GITADORA;
- the legacy DDR path (`DdrAfp`), AFP 2.13.7.

One instance is selected once at boot (`Runtime::SelectRuntime`, called
from the backend constructors in `Backend::CreateActive`, see
docs/backend.md) and reached through `Runtime::Active()`. Before selection it
defaults to the modern runtime. Since P14 `IGameRuntime` is a seam internal to
the AFP family: generic code (boot.cpp, render_loop, the dispatcher) talks to
`Backend::IBackend`, and the AFP family delegates to the runtime.

## Why it exists

The backend used to be chosen by a scattered `g_ddr_mode` bool: 43 read
sites across 8 files, each an inline `if (g_ddr_mode) DdrAfp::X() else
AfpManager::Y()`. That is binary (no room for a third engine generation), and
every backend-specific decision was interleaved into general code. The plan
(P5) replaces it with backend selection at construction time through one
interface, so each backend's logic lives in one cohesive class
(`ModernRuntime`, `DdrRuntime`) and adding a backend means a new class instead
of a sweep of 43 sites.

The old `render_live.h` Inspect seam was written with this interface in mind:
its header called the free-function seam "the natural cut-line the future
refactor widens into a backend interface." That seam has since been absorbed.
`RenderLive::Inspect` was a set of pure one-line forwards to
`Runtime::Active()` and was deleted (P11); callers in render_loop,
render_loop_requests, anim_inspect, and render_live's own publishers now call
`Runtime::Active()` directly, and `Runtime::Label` is the label type. The
`0xFFFFFFFC` no-stream sentinel is also no longer written as a raw literal:
engine code uses `Runtime::kModernNoStream`, and the state/GUI side uses
`App::kNoActiveStream` (state/telemetry.h, static_asserted equal in
render_live.cpp), so the state library needs no engine include.

## Migration (strangler-fig)

The migration is complete and `g_ddr_mode` is deleted. The global no
longer exists (nor does its
`BootFromGameDir` write); the backend is selected once via
`Runtime::SelectRuntime(profile->legacy_afp)` and every site dispatches through
`Runtime::Active()`. The one `App::State` copy the GUI reads (`SetIsDdrMode` /
`IsDdrMode`) is independent and stays. The strangler sequence that got here:

- render_live (the Inspect seam + PublishLiveState).
- main (IsBooted).
- render_loop backend-dispatch sites: active-clip name
  (`ActiveClipName`), scene-loaded test (`HasRenderableScene`), the
  stream-id/layer-id swap (`ActiveClipId`), the DDR frame render
  (`RenderFrame`, a no-op for modern), engine teardown (`Shutdown`), and the
  pre-hot-swap scene unload (`UnloadScene`, a no-op for DDR).
- boot: `BootFromGameDir` reads `profile->legacy_afp` directly and calls
  `SelectRuntime(legacy_ddr)` (plus `SetIsDdrMode` for the GUI copy). The
  `g_ddr_mode = legacy_ddr` write is gone.
- export (backend-kind cache): `StartSession` sets
  `g_sess.ddr = Runtime::Active().IsLegacyDdr()` instead of reading the
  `g_ddr_mode` global, so export.cpp has no `g_ddr_mode` left (its
  now-dead `app_globals.h` include was dropped). The `g_sess.ddr` field
  survives as the per-session backend flag that drives the export's own
  loop-detection flow; retiring the field is the separate strategy slice
  below.
- boot ApplyCliOverrides afp-speed gate: the `--afp-speed` override
  routes through `Runtime::Active().SetGlobalSpeed(afp, speed)` (modern
  calls afp_set_global_speed; DDR does nothing and returns false, since AFP
  2.13.7 has no global-speed export, so the caller logs only when the speed
  was applied). The DDR no-op is the old `!g_ddr_mode` gate, so no
  `IsModern()` check remains.
- boot MountAndLoadIfs load branches: the whole per-backend load body
  moved into `Runtime::Active().LoadScene(mount_path, ifs_path)`, the
  counterpart of the already converted `UnloadScene`, which completes the
  load/render/unload/shutdown lifecycle on the runtime. `MountAndLoadIfs`
  keeps only the shared arc-staging setup and then tail-calls `LoadScene`. The
  modern override runs the full MountIfs / texturelist-filter / LoadPackages /
  dictionary / companion machinery, and the DDR override mounts and reads the
  package via DdrAfp. The move was verbatim (indentation reflowed by
  clang-format); game_runtime.cpp gained the loader deps (avs_boot /
  ifs_inspect / render_backend / app_state / log). The DDR override exposes
  the package's top-level clips (e.g. common_shutter's 00_cleared /
  shutter_clear) as the selectable "layer" list, which is DDR's counterpart of
  the modern afplist animations, so the GUI Layers panel + switch_animation can
  pick any clip instead of only clip[0]. Variant slots stay empty (no per-clip
  bitmap overrides on DDR). Its frame labels come from DdrAfp's by-name
  resolve of the four authored DDR background labels (in/loop/out/end,
  afp_mc_get_param 0x1012), the DDR counterpart of the modern master-clip
  label enumeration.
- render_loop ProbeSlots variant scan: the once-per-second re-probe
  for named variant clips became `Runtime::Active().ReprobeVariantSlots(
  stream_id)` (modern drives the afp_mc_* probe on the active IFS config; DDR
  does nothing, since its backgrounds expose no named variant clips). The DDR
  no-op is the old `!g_ddr_mode` gate, and the render loop keeps only the
  throttle.
- render_loop submonitor bind: resolved by deleting the read instead of
  converting it, because the `!g_ddr_mode` there was a redundant defensive
  guard. The bind reads `AfpManager::StreamId()` and gates on
  `sid != 0xFFFFFFFC`; in DDR that is the sentinel (g_afp null, no modern
  stream), so the bind already skips itself and the per-frame cycler does
  nothing (`sm_base_mc < 0`). Removing the guard does not change behaviour
  (proven by the StreamId note in the render loop), and the 106-ref
  submonitor feature stays where it is, with no controller extraction needed
  just to drop the read.
- render_loop switch-animation arm: the request pump had a DDR arm
  (`&& g_ddr_mode` -> `DdrAfp::SwitchClip` + republish DDR status/labels) and a
  separate modern arm (`AfpManager::SwitchAnimation`/`ForceReplay` + optional
  label goto + republish). Both collapsed into one arm,
  `Runtime::Active().SwitchAnimation(name, label)`; each backend does its own
  switch + `App::Status` republish in its runtime override (verbatim move).
  This is the P8 request-dispatcher pattern applied to one request, and it
  shrank render_loop.cpp 1252 -> 1186.
- render_loop trim/loop ForceReplay: the modern master-loop replay (the
  last read) became `Runtime::Active().MaybeRedriveRootLoop(stream_id,
  loop_cooldown, frames_since_switch, trim_frames) -> RootRedrive`. The runtime
  owns the whole decision (Hold never replays; Force / loop-master re-drive a
  one-shot master, never during export) plus the ForceReplay and `App::Status`
  republish. It returns a small `RootRedrive{replayed, new_stream_id}` that the
  render loop applies to its loop statics. Since P12 the continuous-loop
  flag-dance latch is a ModernRuntime member, so the trim-path replay resets it
  internally instead of returning a `reset_flag_dance` bit. DDR does nothing.
  That no-op is the old `!g_ddr_mode` guard, which mattered here: unlike the
  submonitor, it operates on the DDR-layer-id `stream_id` local, so it does
  not skip itself on the sentinel. With this read gone, the global was deleted.
  render_loop.cpp 1186 -> 1145.

The full P7 root-loop-policy decomposition (folding the flag sequence, the
wrap tracker and the loop statics into one controller) would still help
render_loop readability, but it is no longer needed for `g_ddr_mode`:
`MaybeRedriveRootLoop` already carries the backend split.

## The export loop-detection strategy (now backend capture drivers)

The `g_sess.ddr` flow field is gone (P18). The capture/finish flow is a
strategy the backend provides, `Export::ICaptureDriver` (src/export_capture.h)
with `AfpModernCaptureDriver` / `AfpDdrCaptureDriver`
(src/backend/afp_capture_drivers.*), reached via
`Backend::Active()->ExportDriver()`. `BeginCapture` is the old per-backend
playback setup (label goto / seek / ForceReplay / continuous-loop override +
unpause). `TickCapture` is the old per-tick capture flow (modern: idle
tracking + `StepModernLoop` decisions + safety caps; DDR: the loop-detector
feed with its authored-loop primary signal, the one sanctioned pixel-MAD
content fallback, the end-on-freeze latch and the one-frame-delayed submit).
`EndCapture` is the continuous-loop restore + pause-defend reset. The loop
decisions stay unit-tested in `r573_loop`. The flow was moved verbatim and
verified with this doc's standard smoke set (live DDR authored-loop export,
modern label export, blend-loop export, plus the pixel net). export.cpp is now
free of both backend and engine code: it owns the session, sink,
crop/bg/dump, blend compose, and finish, which made the hosted
`export_capture_tests` (fake frame source through `SubmitOneFrame`) possible.

The two runtimes gained the backend-lifecycle + per-frame methods (`IsBooted`,
`ActiveClipName`, `HasRenderableScene`, `RenderFrame`, `ReprobeVariantSlots`,
`MaybeRedriveRootLoop`, `LoadScene`, `UnloadScene`, `Shutdown`) alongside the
Inspect surface.

P12 added the last per-frame and request-handler seams, so generic code makes
no raw `g_afp` calls:

- `RenderFrame(dt, modern_stream_id, frame_count)`: modern runs the
  SEH-guarded `afp_do_sort_render` (plus the one-time afpu world-mat log);
  DDR runs its own frame render. This fixed the old asymmetry where modern
  rendered via a direct `g_afp.afp_do_sort_render` call in RenderOneFrame
  while DDR rendered inside the runtime.
- `ApplyContinuousLoop(modern_stream_id, mode)`: the SDVX continuous-loop
  flag-dance (0x200/0x1/0x1000 sequence + per-frame bit-0 re-set). The
  done-for latch is a ModernRuntime member; MaybeRedriveRootLoop's trim path
  resets it internally.
- `ApplyMasterScale(modern_stream_id)`: root-mc xy-scale via afp_mc_get
  0x1003/0x101E, cache keyed on (stream, scale).
- `ApplyVariantSlots` / `ApplySublayerOverrides`: the per-frame re-apply
  passes (moved out of boot.cpp; see docs/boot_and_render_loop.md 4.4).
- `ForceReplayMaster()`: the request handler that previously hard-wired
  AfpManager in render_loop_requests.cpp.
- `LoadAlongside(ifs_path)` / `UnloadAlongside(ifs_path)`: mount (or drop) an
  extra afpu package on top of the loaded one, then `ForceReplay` so the
  bind-on-play bindings resolve against it (docs/boot_and_render_loop.md 5).
  Modern wraps `AfpManager::LoadOverlay` / `UnloadOverlay` and republishes
  `Status::overlay_ifs`; DDR logs and does nothing, because libafp has no
  afpu packages to overlay. These replace the old `ToggleCompanion`, which
  took an index into a per-IFS locale list; the pair is keyed by path, so any
  number of unrelated IFSes can be stacked.

Every DDR override of these is a no-op that matches the old implicit behavior
(sentinel self-skip or null-fn-pointer skip on the unresolved DDR `g_afp`
table, or an empty slots vector).

File layout: `game_runtime.h` (interface) + `game_runtime_internal.h` (the
two class declarations + ModernRuntime's latch/cache members) +
`game_runtime_modern.cpp` / `game_runtime_ddr.cpp` (implementations) +
`game_runtime.cpp` (instances + SelectRuntime/Active).
`LoadScene` / `UnloadScene` are the load/unload pair; adding `LoadScene` let
`MountAndLoadIfs` drop its `g_ddr_mode` branch (it is now shared arc-staging
plus a `Runtime::Active().LoadScene(...)` tail-call). `HasRenderableScene`
differs from `HaveActiveClip` on DDR: it also requires `DdrAfp::IsBooted()`,
matching the render loop's export-gate check exactly (the behavior was kept as
it was, not unified).

## Method surface and the modern-param convention

The methods carry the modern-specific parameters (`modern_stream_id`, the
`AfpFuncs&`) that the modern reads need; `DdrRuntime` ignores them and reads
`DdrAfp::LayerId()` / its own accessors instead. Passing them through keeps
the call shape at every site the same as the pre-interface code, which is
what kept the conversion behavior-preserving.

Key methods (see game_runtime.h for the full contract):

- `ActiveClipId(modern_stream_id)`: modern returns the stream id, DDR returns
  the layer id (the modern stream id is the `kModernNoStream` = 0xFFFFFFFC
  sentinel in DDR mode). This is the backend-stable id that the wrap-tally
  re-baseline and the pause-defend cache key off.
- `HaveActiveClip` / `ReadPlayhead` / `ReadSize` / `ReadRawLayerInfo` /
  `ReadComplete` / `EnumerateLabels` / `SetPaused` / `SeekFrame` /
  `GotoLabel` / `SetGlobalSpeed` / `SwitchAnimation`: the live
  [TIME]/[SIZE]/labels/transport readout plus the mutation and request-handler
  ops. `SwitchAnimation(name, label)` handles a GUI switch-clip request end to
  end; each backend switches and republishes App::Status in its own override
  (modern re-authors the master / ForceReplay + optional label goto; DDR
  re-points the layer via DdrAfp::SwitchClip and ignores the modern label).
  Per-backend behavior (e.g. DDR has no free-running counter, so
  `ReadRawLayerInfo` returns false; DDR seamless loops have no complete latch,
  so `ReadComplete` returns false; DDR has no global-speed control, so
  `SetGlobalSpeed` does nothing and returns false) is documented on each
  override and carried over verbatim from the old Inspect branches. See
  docs/boot_and_render_loop.md section 8 for where each afp call comes from.
- `SupportsLiveExtras()`: true only for modern. It gates the three modern-only
  features that DDR's afp 2.13.7 does not export: the filter apply (afp-core
  ord 0x032), the bulk MC-name child enumeration (ord 0x079), and the
  file-info version triplet (afp-utils afpuloc_*).
- `IsLegacyDdr()`: true only for DDR. It is the direct backend-identity
  signal, separate from `SupportsLiveExtras` (which reports feature
  availability). The export session caches it into `g_sess.ddr` so export.cpp
  needs no `g_ddr_mode` read; prefer it over `!SupportsLiveExtras()` for a DDR
  test.

## Testability

`IGameRuntime` is the seam a future `FakeGameRuntime` implements to drive
component tests (loop/trim policy, export loop counting, clip-switch
sequencing) with scripted playheads and no Konami DLL. That fake and its
tests land once the policy code that would use them (render_loop's loop
policy, the export loop counter) is extracted to take an `IGameRuntime&`
instead of reaching for globals, in a later P5 / P7 slice. `ModernRuntime`
and `DdrRuntime` themselves forward to DLL-backed code, so they are
build-verified and smoke-tested live, not unit-tested.
