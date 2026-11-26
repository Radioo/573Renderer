# Backend seam (src/backend/)

`Backend::IBackend` is the generic engine surface. Everything above it
(boot.cpp orchestration, the render loop, the command dispatcher, main.cpp)
is backend-neutral. Everything below it (DLL loading, AVS filesystem, IFS
mounting, AFP scenes, per-frame engine housekeeping) is a backend
implementation detail. A future backend with no game DLLs, no AVS and no IFS
(a pre-IFS IIDX TXP2 parser and renderer, for example) can implement
`IBackend` directly, and nothing above the seam has to change.

## The interface (src/backend/backend.h)

- `Id()`: stable backend identifier string (`afp_modern` / `afp_ddr`).
- `Boot(BootEnv)`: everything from DLL discovery to a ready engine.
  `BootEnv` carries the game dir, the resolved profile, the CLI options
  pointer (for backend-owned CLI features), and `load_boot_content`.
- `Shutdown()`: engine and filesystem teardown (the app keeps D3D9 device and
  log shutdown).
- `StartContentScan()`: starts the backend's content discovery (AFP family:
  the background `*.ifs` / `*.arc` walk).
- `LoadContent(path, from_arc)` / `UnloadContent()`: content lifecycle (AFP
  family: arc staging plus `Runtime::LoadScene` / `UnloadScene`).
- `AdvanceFrame(dt, frame_count, exporting)`: per-frame engine update and
  housekeeping. The render loop computes dt (capture-locked during export)
  and calls this once per tick.
- `RenderScene(dt, frame_count)`: called between the app's
  `BeginFrame`/`EndFrame`; draws the scene.
- `FillAutopilotInputs(in)`: fills the engine-derived CLI-autopilot inputs
  (clip liveness, name matches, scene renderability). The loop fills the
  generic ones (frame, label_applied, export_finished).
- `BindSubmonitor()`: the CLI submonitor bind action. It is a modern-AFP
  feature; other backends do nothing because the CLI flags are absent.
- `HandleCommand(any)`: backend-specific command dispatch. The payload is
  the backend's own closed variant (`AfpCmd::Any` for the AFP family, see
  docs/state.md "Command semantics").
- `ExportDriver()`: the backend's `Export::ICaptureDriver` (P18), which is the
  export capture strategy (begin playback, per-tick capture and stop decisions,
  teardown). See docs/export_pipeline.md and docs/game_runtime.md "export
  loop-detection strategy".

`Backend::CreateActive(profile)` constructs the backend once per boot by
looking up `profile.backend_id` in the `kBackends` registry table. An unknown
id fails the boot with a clear error. `Backend::Active()` is the
process-wide accessor. The backend cannot be switched while running: there is
one boot per process (user decision; the GUI Setup screen is gone once Ready).

## The AFP family (afp_family_backend.*)

`AfpFamilyBackend` holds everything the modern and DDR paths share. P14 moved
it verbatim out of boot.cpp and render_loop.cpp:

- `Boot`: resolve the family engine config
  (`AfpProfiles::For(profile->slug)`, see docs/game_profiles.md "P15
  split") -> `DiscoverDllDir` (modules/, contents/modules/, root) ->
  `LoadAllDlls` (ordinal resolve for modern; DDR defers afp/afpu resolve to
  `DdrAfp::Boot`) -> `AvsManager::Boot` -> virtual `BootEngine()`.
- `StartContentScan`: the recursive `.ifs` walk, publishing progress at most
  every 100 ms, plus `.arc` TOC peeking when the profile sets
  `scan_arc_containers`.
- `LoadContent`: DDR `.arc` staging (`WriteTempIfs` to the process temp
  dir), then `Runtime::Active().LoadScene`.
- `AdvanceFrame` (afp_family_frame.cpp): `afp_do_update` under SEH,
  submonitor cyclers, the variant-slot reprobe every 120 frames, loop
  housekeeping (the continuous-loop dance, `PublishLiveState` and the
  root-loop redrive; the loop statics are members now), then the per-frame
  variant, sublayer and master-scale applies. The order is exactly the old
  render_loop AdvanceFrame order. `PublishLiveState` stays inside
  housekeeping because it also applies the F7 filter and pause-defend to
  the engine, so it is more than telemetry.
- `RenderScene`: recomputes the active clip id
  (`ActiveClipId(AfpManager::StreamId())`, identical to the old
  post-housekeeping stream id) and calls `Runtime::RenderFrame`.
- The submonitor bind and cycle machinery and its state (slots, base and
  overlay mc, dissolve and fade cyclers seeded from the CLI at Boot).
- `HandleCommand`: `std::any_cast<AfpCmd::Any>` plus a visitor (the P13
  dispatcher, moved here).

`AfpModernBackend` adds `AfpManager::Boot` and persistent boot-IFS loading.
`AfpDdrBackend` adds `DdrAfp::Boot` and `SetTimeScale`. Each constructor
calls `Runtime::SelectRuntime`, so `IGameRuntime` is now a seam internal to
the AFP family. Generic code reaches the engine only through
`Backend::Active()`, with three known exceptions that remain until the P18
export seam: `boot.cpp ApplyCliOverrides` passing `g_afp` to `SetGlobalSpeed`,
`render_loop.cpp`'s inline animation-label arm, and
`Export::OnMainLoopTick(g_engine, ...)`.

## Boot flow after P14 (boot.cpp, ~190 lines)

```
BootFromGameDir(game_dir, ..., profile_slug, cli):
  ResolveBootProfile          identity table + AutoDetect, unchanged
  Backend::CreateActive       ctor selects the runtime
  SetIsDdrMode                GUI copy, dies in P16
  CreateRenderWindowAndDevice if wanted (DDR 1280x720 override until P15)
  Backend::Active()->Boot     DLLs + AVS + engine + persistent content
  SaveBootSettings, Ready
  Backend::Active()->StartContentScan
```

The order differs from the pre-P14 flow in one place, on purpose: the render
window and device are now created before DLL loading and AVS boot (they used
to sit between AVS boot and AFP boot). Neither depends on the other, and the
output was verified byte-identical on the 3-game pixel net.

`MountAndLoadIfs` in boot.h remains as a one-line forwarder to
`Backend::Active()->LoadContent` because the qpro extractor calls it from
16 sites (the deferred qpro RenderService seam owns that cleanup).

## What a pre-IFS backend implements

A TXP2/`.bin` backend (IIDX 18 era) would implement `IBackend` directly:
`Boot` parses no DLLs and mounts no AVS, `StartContentScan` enumerates its
own container files, `LoadContent` parses a package natively, and
`AdvanceFrame`/`RenderScene` drive its own animation model. None of the
AFP family files are involved. Above the seam, the only change is a
registry line (post-P15: a profile row with its backend id).
