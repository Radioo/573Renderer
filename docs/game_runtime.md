# Game runtime abstraction (src/game_runtime.{h,cpp})

`Runtime::IGameRuntime` is the interface over the two engine generations the
renderer drives:

- the MODERN avs2 path (`AfpManager` + the `g_afp` proc table), used by
  IIDX, SDVX, and GITADORA;
- the LEGACY DDR path (`DdrAfp`), AFP 2.13.7.

One instance is selected once at boot (`Runtime::SelectRuntime`, called
from the backend constructors in `Backend::CreateActive` - see
docs/backend.md) and reached via `Runtime::Active()`. Before selection it
defaults to the modern runtime. Since P14 `IGameRuntime` is an
AFP-family-internal seam: generic code (boot.cpp, render_loop, the
dispatcher) talks to `Backend::IBackend`, and the AFP family delegates to
the runtime.

## Why it exists

The backend used to be chosen by a scattered `g_ddr_mode` bool - 43 read
sites across 8 files, each an inline `if (g_ddr_mode) DdrAfp::X() else
AfpManager::Y()`. That is binary (no room for a third engine generation) and
every backend-specific decision was interleaved into general code. The plan
(P5) replaces it with construction-time backend selection through one
interface, so the two backends' logic lives in two cohesive classes
(`ModernRuntime`, `DdrRuntime`) and adding a backend is a new class, not a
sweep of 43 sites.

This is the interface the old `render_live.h` Inspect seam explicitly
anticipated: its header called the free-function seam "the natural cut-line
the future refactor widens into a backend interface." That seam has since
been absorbed: `RenderLive::Inspect` was a set of pure one-line forwards to
`Runtime::Active()` and was deleted (P11); callers in render_loop,
render_loop_requests, anim_inspect, and render_live's own publishers now call
`Runtime::Active()` directly, and `Runtime::Label` is the label type. The
`0xFFFFFFFC` no-stream sentinel is likewise never written as a raw literal
anymore: engine code uses `Runtime::kModernNoStream`, and the state/GUI side
uses `App::kNoActiveStream` (state/telemetry.h, static_asserted equal in
render_live.cpp) so the state library needs no engine include.

## Migration (strangler-fig)

The migration is complete: `g_ddr_mode` is fully deleted. The global no
longer exists (nor its
`BootFromGameDir` write); the backend is selected once via
`Runtime::SelectRuntime(profile->legacy_afp)` and every site dispatches through
`Runtime::Active()`. The one `App::State` copy the GUI reads (`SetIsDdrMode` /
`IsDdrMode`) is independent and stays. The strangler sequence that got here:

- render_live (the Inspect seam + PublishLiveState).
- main (IsBooted).
- render_loop backend-DISPATCH sites - active-clip name
  (`ActiveClipName`), scene-loaded test (`HasRenderableScene`), the
  stream-id/layer-id swap (`ActiveClipId`), the DDR frame render
  (`RenderFrame`, a no-op for modern), engine teardown (`Shutdown`), and the
  pre-hot-swap scene unload (`UnloadScene`, a no-op for DDR).
- boot: `BootFromGameDir` reads `profile->legacy_afp` directly and calls
  `SelectRuntime(legacy_ddr)` (plus `SetIsDdrMode` for the GUI copy). The
  `g_ddr_mode = legacy_ddr` write is gone.
- export (backend-kind cache): `StartSession` sets
  `g_sess.ddr = Runtime::Active().IsLegacyDdr()` instead of reading the
  `g_ddr_mode` global, so export.cpp is entirely `g_ddr_mode`-free (its
  now-dead `app_globals.h` include was dropped). The `g_sess.ddr` FIELD
  survives as the per-session backend flag that drives the export's own
  loop-detection flow; retiring the field is the separate strategy slice
  below.
- boot ApplyCliOverrides afp-speed gate: the `--afp-speed` override
  routes through `Runtime::Active().SetGlobalSpeed(afp, speed)` (modern
  calls afp_set_global_speed; DDR no-ops and returns false since AFP 2.13.7
  has no global-speed export, so the caller logs only on a real apply). The
  DDR no-op IS the old `!g_ddr_mode` gate, so no `IsModern()` smell remains.
- boot MountAndLoadIfs load branches: the whole per-backend load body
  moved into `Runtime::Active().LoadScene(mount_path, ifs_path)` (the pair of
  the already-converted `UnloadScene`, completing the load/render/unload/
  shutdown lifecycle on the runtime). `MountAndLoadIfs` keeps only the shared
  arc-staging setup then tail-calls `LoadScene`; the modern override runs the
  full MountIfs / texturelist-filter / LoadPackages / dictionary / companion
  machinery, the DDR override mounts + reads the package via DdrAfp. Verbatim
  move (indentation reflowed by clang-format); game_runtime.cpp gained the
  loader deps (avs_boot / ifs_inspect / render_backend / app_state / log). The
  DDR override exposes the package's TOP-LEVEL CLIPS (e.g. common_shutter's
  00_cleared / shutter_clear) as the selectable "layer" list - DDR's analogue
  of the modern afplist animations - so the GUI Layers panel + switch_animation
  can pick any clip, not just clip[0]; variant slots stay empty (no per-clip
  bitmap overrides on DDR). Its frame labels come from DdrAfp's by-name resolve
  of the four authored DDR background labels (in/loop/out/end, afp_mc_get_param
  0x1012), the DDR analogue of the modern master-clip label enumeration.
- render_loop ProbeSlots variant scan: the once-per-second re-probe
  for named variant clips became `Runtime::Active().ReprobeVariantSlots(
  stream_id)` (modern drives the afp_mc_* probe on the active IFS config; DDR
  no-ops since its backgrounds expose no named variant clips). The DDR no-op IS
  the old `!g_ddr_mode` gate; the render loop keeps only the throttle.
- render_loop submonitor bind: resolved by DELETION, not conversion - the
  `!g_ddr_mode` there was a redundant defensive guard. The bind reads
  `AfpManager::StreamId()` and gates on `sid != 0xFFFFFFFC`; in DDR that is the
  sentinel (g_afp null, no modern stream), so the bind already self-skips and
  the per-frame cycler no-ops (`sm_base_mc < 0`). Removing the guard is
  behaviour-identical (proven by the StreamId note in the render loop) and the
  106-ref submonitor feature stays where it is - no controller extraction needed
  just to drop the read.
- render_loop switch-animation arm: the request pump had a DDR arm
  (`&& g_ddr_mode` -> `DdrAfp::SwitchClip` + republish DDR status/labels) and a
  separate modern arm (`AfpManager::SwitchAnimation`/`ForceReplay` + optional
  label goto + republish). Both collapsed into one arm
  `Runtime::Active().SwitchAnimation(name, label)`; each backend does its own
  switch + `App::Status` republish in its runtime override (verbatim move).
  This is the P8 request-dispatcher pattern applied to one request; it shrank
  render_loop.cpp 1252 -> 1186.
- render_loop trim/loop ForceReplay: the modern master-loop replay (the
  last read) became `Runtime::Active().MaybeRedriveRootLoop(stream_id,
  loop_cooldown, frames_since_switch, trim_frames) -> RootRedrive`. The runtime
  owns the whole decision (Hold never replays; Force / loop-master re-drive a
  one-shot master, never during export) plus the ForceReplay and `App::Status`
  republish; it returns a small `RootRedrive{replayed, new_stream_id}` the
  render loop applies to its loop statics. Since P12 the continuous-loop
  flag-dance latch is a ModernRuntime member, so the trim-path replay resets it
  internally instead of returning a `reset_flag_dance` bit. DDR no-ops -
  that no-op is the old `!g_ddr_mode` guard, which was load-bearing here (unlike
  the submonitor it operates on the DDR-layer-id `stream_id` local, so it does
  NOT self-skip on the sentinel). With this read gone, the global was deleted.
  render_loop.cpp 1186 -> 1145.

The full P7 root-loop-policy decomposition (folding the flag sequence + wrap
tracker + the loop statics into one controller) is still worthwhile for
render_loop readability, but it is no longer on the `g_ddr_mode` critical path -
`MaybeRedriveRootLoop` already carries the backend split.

## The export loop-detection strategy (now backend capture drivers)

The `g_sess.ddr` flow field is GONE (P18): the capture/finish flow is a
backend-provided strategy, `Export::ICaptureDriver` (src/export_capture.h)
with `AfpModernCaptureDriver` / `AfpDdrCaptureDriver`
(src/backend/afp_capture_drivers.*), reached via
`Backend::Active()->ExportDriver()`. `BeginCapture` is the old per-backend
playback setup (label goto / seek / ForceReplay / continuous-loop override +
unpause), `TickCapture` the old per-tick capture flow (modern: idle tracking
+ `StepModernLoop` decisions + safety caps; DDR: the loop-detector feed with
its authored-loop PRIMARY signal, the one sanctioned pixel-MAD content
FALLBACK, end-on-freeze latch and one-frame-delayed submit), and
`EndCapture` the continuous-loop restore + pause-defend reset. The loop
DECISIONS stay unit-tested in `r573_loop`; the flow was moved VERBATIM and
verified with the smoke set this doc always demanded (live DDR
authored-loop export, modern label export, blend-loop export, plus the
pixel net). export.cpp is now backend-free AND engine-free: it owns the
session, sink, crop/bg/dump, blend compose, and finish, which is what made
the hosted `export_capture_tests` (fake frame source through
`SubmitOneFrame`) possible.

The two runtimes gained the backend-lifecycle + per-frame methods (`IsBooted`,
`ActiveClipName`, `HasRenderableScene`, `RenderFrame`, `ReprobeVariantSlots`,
`MaybeRedriveRootLoop`, `LoadScene`, `UnloadScene`, `Shutdown`) alongside the
Inspect surface.

P12 added the last per-frame and request-handler seams, so generic code makes
zero raw `g_afp` calls:

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
- `ForceReplayMaster()` / `ToggleCompanion(index)`: the request handlers that
  previously hard-wired AfpManager in render_loop_requests.cpp.

Every DDR override of these is a no-op equal to the old implicit behavior
(sentinel self-skip or null-fn-pointer skip on the unresolved DDR `g_afp`
table, or an empty slots/companions vector).

File layout: `game_runtime.h` (interface) + `game_runtime_internal.h` (the
two class declarations + ModernRuntime's latch/cache members) +
`game_runtime_modern.cpp` / `game_runtime_ddr.cpp` (implementations) +
`game_runtime.cpp` (instances + SelectRuntime/Active).
`LoadScene` /
`UnloadScene` are the load/unload pair; adding `LoadScene` is what let
`MountAndLoadIfs` drop its `g_ddr_mode` branch (it is now shared arc-staging
plus a `Runtime::Active().LoadScene(...)` tail-call). Note `HasRenderableScene`
differs from `HaveActiveClip` on DDR: it additionally requires
`DdrAfp::IsBooted()`, matching the render loop's export-gate check exactly
(behavior preserved, not unified).

## Method surface and the modern-param convention

The methods carry the modern-specific parameters (`modern_stream_id`, the
`AfpFuncs&`) that the modern reads need; `DdrRuntime` ignores them and reads
`DdrAfp::LayerId()` / its own accessors instead. Passing them through keeps
the call shape at every site identical to the pre-interface code, which is
what makes the conversion behavior-preserving.

Key methods (see game_runtime.h for the full contract):

- `ActiveClipId(modern_stream_id)`: modern returns the stream id, DDR returns
  the layer id (the modern stream id is the `kModernNoStream` = 0xFFFFFFFC
  sentinel in DDR mode). This is the backend-stable id the wrap-tally
  re-baseline and pause-defend cache key off.
- `HaveActiveClip` / `ReadPlayhead` / `ReadSize` / `ReadRawLayerInfo` /
  `ReadComplete` / `EnumerateLabels` / `SetPaused` / `SeekFrame` /
  `GotoLabel` / `SetGlobalSpeed` / `SwitchAnimation`: the live
  [TIME]/[SIZE]/labels/transport readout plus the mutation and request-handler
  ops. `SwitchAnimation(name, label)` handles a GUI switch-clip request end to
  end - each backend switches and republishes App::Status in its own override
  (modern re-authors the master / ForceReplay + optional label goto; DDR
  re-points the layer via DdrAfp::SwitchClip, ignoring the modern label).
  Per-backend behavior (e.g. DDR has no
  free-running counter so `ReadRawLayerInfo` returns false; DDR seamless
  loops have no complete latch so `ReadComplete` returns false; DDR has no
  global-speed control so `SetGlobalSpeed` no-ops and returns false) is
  documented on each override and carried over verbatim from the old Inspect
  branches - see docs/boot_and_render_loop.md section 8 for the afp-call
  provenance.
- `SupportsLiveExtras()`: true only for modern. Gates the three modern-only
  features DDR's afp 2.13.7 does not export - the FILTER apply (afp-core ord
  0x032), the bulk MC-name child enumeration (ord 0x079), and the FILE-INFO
  version triplet (afp-utils afpuloc_*).
- `IsLegacyDdr()`: true only for DDR. The honest backend-identity signal,
  distinct from `SupportsLiveExtras` (which reports feature availability). The
  export session caches it into `g_sess.ddr` so export.cpp needs no
  `g_ddr_mode` read; prefer it over `!SupportsLiveExtras()` for a DDR test.

## Testability

`IGameRuntime` is the seam a future `FakeGameRuntime` implements to drive
component tests (loop/trim policy, export loop counting, clip-switch
sequencing) with scripted playheads and no Konami DLL. That fake and its
tests land once the policy code that would consume them (render_loop's loop
policy, the export loop counter) is extracted to take an `IGameRuntime&`
rather than reaching for globals - a later P5 / P7 slice. `ModernRuntime`
and `DdrRuntime` themselves forward to DLL-backed code, so they are
build-verified and smoke-tested live, not unit-tested.
