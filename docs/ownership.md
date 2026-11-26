# P6 ownership: EngineSession / GpuContext (strangler status)

The P6 goal is to replace the ~50 process-wide inline globals (app_globals.h,
afp_boot_internal.h, afp_d3d9_internal.h) with owned objects that boot creates
and passes down explicitly. That makes teardown possible, allows a re-boot
after a failed boot, and lets a test run two sessions side by side. The method
is the plan's strangler pattern: first aggregate behind shim references (no
call-site churn), then convert consumers one by one to take the object
explicitly, then delete the shims.

## EngineSession (src/engine_session.h)

One booted Konami engine instance: the three DLL loaders, their resolved proc
tables, and the AFP render context.

- `g_engine` (app_globals.h) is the single instance. The historical names
  `g_avs_dll / g_afp_dll / g_afpu_dll / g_avs / g_afp / g_afpu / g_render_ctx`
  are now inline references aliasing its members, so all consumer TUs compile
  and behave the same (verified byte-identical: modern 12-frame dump +
  DDR bg_0009 --ddr-test before and after).
- Conversion protocol for follow-up slices: when a module is touched, its
  functions that take proc-table/loader parameter lists collapse to
  `EngineSession&` (local `AfpFuncs& afp = es.afp;` aliases keep the body
  diff minimal). New code takes the session, never the shims.
- Signature-collapse step: the 8 lifecycle entry points take the
  session: `Boot(EngineSession&, D3D9State&)`, `Shutdown`, `LoadPackages`,
  `LoadBootIfses`, `UnloadPackages`, `LoadCompanion`, `UnloadCompanion`,
  `UnloadAllCompanions` (~38 call sites collapsed from verbatim
  `(g_afp, g_afpu, g_avs, ...)` triples to `(g_engine, ...)`). Verified
  byte-identical on both backends plus a companion load/unload run.
- Transport-collapse step: `SwitchAnimation(EngineSession&, name,
  force)`, `ForceReplay(EngineSession&)`, `PlayBitmapAnimation(EngineSession&,
  name)`, 22 more call sites collapsed. PlayBitmapAnimation has no
  callers (it is the SDVX Type-2 system-BG path, kept as a documented RE
  capability).
- Deliberately left on raw proc-table params: the single-table `const
  AfpFuncs&` probes and ops (IsMasterComplete, ReadLayerPosition,
  ReadMcPlayhead, ReadLayerAdvanceCounter, EnumerateLabels, GotoLabel,
  SeekFrame, SetStreamPaused, GetRootMcId, EnumerateChildClips,
  DestroyCurrentStream, DestroySceneStreams). A surface that reads or acts on
  one table is already the smallest seam a fake can drive; wrapping it in the
  session would gain nothing and cost const-correctness.
- The D3D9 device wrapper is not in EngineSession, because the device's
  lifetime is not the engine's (a hot-swap tears the engine down but keeps the
  device). It lives in GpuContext (`g_gpu.d3d`).

- State-move step: the whole afp_boot_internal.h state block
  lives in EngineSession: `active_profile`, `afp_booted`, `pkg_id`,
  `stream_id`, `extra_streams`, `anim_name`, `persistent_pkg_ids`,
  `companions` (CompanionRecord moved to engine_session.h) and
  `next_companion_idx`. afp_boot_internal.h is now only shim references
  over `g_engine`, and only the three AfpManager TUs include it. Verified
  byte-identical on both backends plus a companion load/unload run.

## GpuContext (src/gpu_context.h)

The AfpD3D9 backend's GPU-side state: device alias, shaders, the 256-slot
texture table + per-slot sampler filters, HSV-filter state, and the current
layer transform. `g_gpu` is the single instance; afp_d3d9_internal.h is now
only shim references over it (the per-member RE knowledge moved to the
member comments in gpu_context.h). Deleting the shims showed that
render_backend.h's three `extern` escape hatches (`g_fallback_texture`,
`g_texture_width/height`) had no external users, so they were removed.
Verified byte-identical on both backends plus a qpro hue-scope run.

The aggregation phase is complete: all three internal-globals headers
(app_globals.h, afp_boot_internal.h, afp_d3d9_internal.h) became pure shim
layers over the two owned objects (`g_engine`, `g_gpu`).

Shim deletion (GPU side): the four AfpD3D9 TUs read
`g_gpu.<member>` directly (361 renames), and afp_d3d9_internal.h carries only
the cross-TU function surface (AfpScreamUnimpl decl, BuildDefaultMatrix,
TransposeMatrix4x4, the scream macro); its g_* shim block is deleted.
Verified byte-identical on both backends plus a qpro run.

Shim deletion (engine state): afp_boot_internal.h is
gone (the first of the plan's "three internal-globals headers deleted").
Inside the 11 functions that take an EngineSession, the state reads went to
`es.<member>` (108 sites; these paths are now actually session-scoped, which
two side-by-side sessions need). The accessors and const-AfpFuncs helpers read
`g_engine.<member>` explicitly (37 sites; they get session threading with
their own conversion). The gate scripts also gained missing-file guards (a
deleted but still tracked file crashed run_format / check_file_length; all
five now skip paths that do not exist).

## Upward-dependency inversion

The render backend no longer reads App::Global() anywhere (it used to, for
TexCreate's progress bump and DrawCropOverlay's crop state; two more TUs had
dead app_state includes, now removed). GpuContext carries two injected hooks,
`on_texture_created` (load progress) and `query_crop_overlay` (pick mode +
RT-space rect), which main.cpp wires to App::Global() at startup. Headless
one-shots (--ddr-test) leave them null, and the backend behaves as if the
state were default (the same as the old defaults path). `g_d3d` also moved
into GpuContext (`g_gpu.d3d`; the shim reference stays in app_globals.h).

## Still to convert

- The texture-table RE spike is complete, so the game's mechanism is known.
  bm2dx uses a mutex-guarded fixed 1024-slot table, ids = 1024+slot,
  round-robin first-free allocation with a rotating cursor (freed slots are
  recycled); destroy zeroes the slot, and overflow logs loudly and returns -1
  (no growth, no crash). The renderer's redesign target is therefore 1024
  slots, round-robin reuse and a loud -1 on overflow; the persistent-boundary
  hack becomes optional once slots recycle. The implementation is partial on
  purpose. Capacity and loud overflow are in place (kTexSlotCount =
  1024 in gpu_context.h, replacing the scattered 256 literals; the 5 texture
  arrays resized; TexCreate logs a "slot table FULL" line and returns -1 on
  exhaustion, matching bm2dx's own log-and-fail). Verified byte-identical on
  both backends plus a qpro run (peak slot 5, no FULL messages). The game's
  round-robin slot reuse is deliberately not adopted: the renderer's public
  NextSlot/SetNextSlot and contiguous base+offset addressing (40 consumers,
  e.g. qpro `TexUpload(base + atlas, ...)`) is a different model that
  round-robin would break. Switching it is a separate, riskier slice (it
  touches the qpro regression surface), not a capacity bump, so it is
  documented here instead of half shipped.
- Consumer conversion (modules take EngineSession& / GpuContext& explicitly),
  then shim deletion. Export's surface has since moved past the stage of
  taking an EngineSession: P18 removed the engine from export.h entirely
  (`OnMainLoopTick(D3D9State&)` / `HandleStartRequest(req, D3D9State&)`).
  The engine access lives in the backend capture drivers, which use the
  family globals like the rest of the AFP family.

## Exit criteria trace (from the plan)

- Two EngineSessions constructible in a test: needs the proc-table headers +
  DllLoader to link into a test exe (r573_support already does; the funcs
  headers are header-only structs). Reachable once the shims are gone from
  the session's own header chain.
- Three internal-globals headers deleted: after both aggregates and the
  consumer conversion.
- Failed boot retryable: still also blocked on the P4-deferred re-boot RE
  (whether avs_boot/afp_boot are re-entrant at all). Ownership provides the
  structure for it but does not prove it works.

## Export Session ownership (P8)

`Export::Session` (export_internal.h) is no longer a file-shared mutable
global. The single owned instance lives in an anonymous namespace inside
export.cpp, and the only way to reach it is `Export::ActiveSession()`. Every
internal export function now takes `Session&` (const where it only reads).
The session is threaded through Publish / Init / StartDdrPlayback /
StartModernPlayback / ForceContinuousLoopOverride / Start-Cancel-FailSession /
OpenSinkForFirstFrame / SubmitOneFrame / CaptureFrame / BlendComposeAndSubmit /
FinishAndEncode / TickDdrCapture / MaybeDumpTick / HitSafetyCap /
BuildModernTick and the DDR capture driver's HandleDdrLoopFrame (whose
DdrLoopDetector submit lambda now captures the session instead of reading the
global; SubmitFn is a std::function, so the capture costs nothing). The five
public entry points (OnMainLoopTick, HandleStartRequest, HandleCancelRequest,
IsCapturing, TargetFps) fetch ActiveSession() and pass it down, so a test can
drive the whole capture/finish state machine on its own Session instance
without touching the app-global one. Verified: 3-backend byte-compare net (the
SDVX/IIDX scenarios run the modern export path end-to-end) + a live DDR
export smoke through the loop-detection path (authored-loop detect -> capture
-> encode).

## P8 exit criteria (audited: met)

- "New modern-afp game = data entry + registry line": audited. No
  game-specific control flow exists outside the game_profile.cpp kProfiles
  table (the only per-game names elsewhere are profile flag reads like
  apply_iidx_data_segment_patches, CLI help text, and GUI tooltip copy; the
  SDVX 1.5x scale button is a labeled convenience over the generic master
  scale, not a branch). GITADORA DELTA landing as a pure profile entry proved
  it in practice.
- "GUI panels hold zero business logic": audited. Panels read App state,
  draw, and post App::Commands; their only computations call the tested
  r573_media_format lib (DeriveExportStem / MakeOutputPath). ImGui usage is
  confined to src/gui/ and gated (tools/ci/check_gui_isolation.py).
- "File-length baseline empty": met. The baseline mechanism is deleted and a
  hard 1000-line limit applies.
- Export strategies: one Format enum (media/media_format.h), per-format
  encoder Open functions, MediaSink as the Sink strategy, ExportSession
  ownership threaded (above).

## P11-P12 (backend abstraction program, in progress)

P11 (mechanical enablers): the `tools/checks.sh` aggregate gate was created;
`RenderLive::Inspect` (9 pure forwards) collapsed into direct
`Runtime::Active()` calls; the raw `0xFFFFFFFC` literal was retired behind
`Runtime::kModernNoStream` (engine) / `App::kNoActiveStream` (state+gui).
Verified byte-identical on the 3-game pixel net.

P12 (runtime symmetry): the five engine bypasses moved behind `IGameRuntime`:
the modern `afp_do_sort_render` (now `ModernRuntime::RenderFrame`, which fixes
the modern/DDR render asymmetry), the continuous-loop flag-dance
(`ApplyContinuousLoop`, the latch is a runtime member), `ApplyMasterScale`,
`ApplyVariantSlots`/`ApplySublayerOverrides` (out of boot.cpp), and the
`force_replay` request handler (`ForceReplayMaster`, out of
render_loop_requests.cpp; the `toggle_companion` handler moved the same way
and was later removed with the locale-overlay feature).
Every DDR override is the old implicit no-op (sentinel self-skip, null fn
pointer, or empty vector), verified byte-identical. game_runtime.cpp was split
into game_runtime.cpp (selection) + game_runtime_internal.h (class decls) +
game_runtime_modern.cpp + game_runtime_ddr.cpp. The remaining known direct
AfpManager reads in generic code (`GatherAutopilotInputs`,
`AdvanceFrame`'s `StreamId()`, `CallAfpUpdateGuarded`) are scheduled to move
with the CLI-autopilot split in the backend-seam phase (P14), where they
become AFP-family code instead of adding odd interface methods.

P13 (typed commands): the flat ~50-field `App::Request` is gone, replaced by
the `App::Command` variant + `Cmd::BackendCommand{std::any}` + `AfpCmd::Any`
(see docs/state.md "Command semantics"). The dispatcher is a `std::visit`
visitor pair, and `app_state.h` no longer includes qpro_model.h (that coupling
now lives only in `backend/afp_commands.h`; P14 did the relocation).
Export start carries a generic `App::ExportRequest`. Verified byte-identical.

P14 (backend seam lift): `Backend::IBackend` (src/backend/) is the generic
engine surface. `AfpFamilyBackend` + `AfpModernBackend`/`AfpDdrBackend`
absorbed DLL discovery/loading, AVS boot, the engine boot fork, persistent
boot IFSes, the content scan thread, arc staging + content load, the whole
per-frame AdvanceFrame housekeeping (loop statics are members now), scene
render, the submonitor machinery, the autopilot engine inputs, and AFP
command dispatch. boot.cpp shrank to profile resolve + window/device +
settings + orchestration. render_loop.cpp is backend-neutral except for
`Export::OnMainLoopTick(g_engine, g_d3d)` (P18 cuts it) and the inline
`--animation-label` arm. `IGameRuntime` was demoted to family-internal. The
window+device creation moved ahead of DLL/AVS boot (documented in
docs/backend.md). Verified byte-identical + DLL suite. The
`GatherAutopilotInputs`/`AdvanceFrame`/`CallAfpUpdateGuarded` residue listed
under P12 is now resolved (all family code).

P15 (profile split): `GameProfile::Profile` is identity-only (name, slug,
dir_substring, backend_id, game_dll, default render size). The AFP engine
config (DLL names, offsets, boot-gate bools, scan_arc_containers,
time_scale) moved to the slug-keyed `AfpProfiles::AfpConfig` table
(src/backend/afp_profiles.*), which the family backend resolves at Boot.
`legacy_afp` was deleted (backend_id + registry table); the never-read
`Profile::afp` AfpOrdinals member and `kSkip` were deleted;
`EngineSession::active_profile` was renamed `active_cfg`
(`AfpManager::SetActiveConfig`); `ActiveOffsets` moved to `AfpProfiles`.
dll_contract_tests now assert backend_id. Verified byte-identical + DLL
suite.

P16 slice 1 (capability surface): `IsDdrMode`/`SetIsDdrMode` were deleted.
`BootLifecycle::ActiveBackendId` (seeded from `Backend::Active()->Id()`) is
the GUI's backend signal, and `Status::scene_loaded` is the generic
scene-mounted flag (the GUI no longer interprets `Status::stream_id`).
Verified byte-identical. The rest of the original P16 (moving
IfsConfig/LiveState/labels/mc_tree/AFP export knobs into a
`BackendUiState` block so generic `Status` is fully backend-neutral, and
the IfsCatalog -> ContentCatalog rename) is deferred on purpose until
after the P17 panel registry. P17 only needs the backend id + scene flag,
and the state-block move is a large mechanical migration that users will not
see any benefit from until the first non-AFP backend lands.

P17 (panel registry): the GUI tab bar and right-pane stack are
registry-driven per backend (src/gui/panel_registry.*, see docs/gui.md
2.0). Intended visible differences (renders byte-identical, verified): the
qpro tab only for iidx33; DDR loses the loop/scale rows that did nothing, the
variants editor, and the sub-layers section; the Setup view's arc/customize
extractors show only for ddrworld and the qpro render preset only for
iidx33.

P18 (export capture seam): `Export::ICaptureDriver`
(BeginCapture/TickCapture/EndCapture) with the two AFP drivers in
src/backend/afp_capture_drivers.*; `Session.ddr` deleted; export_ddr.cpp
absorbed into the DDR driver. `export.h` lost `EngineSession&` and
export.cpp lost every AFP and D3D9 link dependency (the offscreen readback
lives in the drivers). That landed the long-deferred CaptureSource seam
together with its first consumer: the hosted `export_capture_tests` ci suite
drives SubmitOneFrame/FinishAndEncode (blend compose + crop) from a synthetic
frame source with a null-backend stub. Verified: pixel net byte-identical,
live DDR authored-loop export (bg_0009, 1373 frames), modern label export
(select_bg_vi hologram 'loop', 299 frames), blend-loop export smoke.

P19 (GPU hygiene + closure): the three AFP shaders left `D3D9State` (they are
now compiled by `Render::CompileAfpShaders` into GpuContext from
`AfpD3D9::Init`, see docs/d3d9_backend.md); `AfpRenderContext` moved to
src/backend/afp_render_context.h; `D3D9State::Init` is engine-agnostic.
Grep-zero audit result: raw `0xFFFFFFFC` exists only at its two constant
definitions; `RenderLive::Inspect`, `IsDdrMode`, `legacy_afp` (as a profile
field), and `App::Request` are gone. Exactly two documented `g_afp` reads
remain in generic TUs, both AFP-family CLI features routed through
`Runtime::Active()`: render_loop's inline `--animation-label` arm and
ApplyCliOverrides' `--afp-speed`. They move with a future autopilot/CLI
ownership pass. The physical relocation of the pre-P14 AFP TUs (afp_boot,
afp_anim, afp_packages, afp_d3d9*, afp_ddr_*, qpro_*, mc_control,
ifs_inspect, engine_session/app_globals/avs_* and the game_runtime files)
under src/backend/ subdirectories is deliberately not done. The ownership
boundary is enforced by the `Backend::IBackend` seam and this audit, not by
folders, and the move would churn 40+ files plus every src/ path in these
docs without changing behavior. If it is ever done, do it as a standalone
mechanical commit with a full doc-path sweep.

## The qpro slice (src/qpro/ is shim-free)

The qpro cluster no longer reads any process global. Every function in
`src/qpro/` that touches the engine takes `EngineSession& es` (and
`D3D9State& d3d` where it renders) as its leading parameters, through the
whole public surface: `Run`, the six `*One` debug drivers, `ClipOne`,
`DumpIfs`, the three `*Composite` dumpers, and the `Render*Composite` /
`ProbeBackNativeFps` / `RenderBackRealtime` entry points. The globals are
now supplied only at the two boundaries that own them, `main.cpp`'s qpro
CLI one-shots and `AfpFamilyBackend`'s qpro command, both passing
`g_engine, g_gpu.d3d`. No file under `src/qpro/` includes app_globals.h
any more, so the cluster can be constructed against a test session.

The conversion was safe to do mechanically because `g_afp` is a reference to
`g_engine.afp` (app_globals.h), so `g_afp` -> `es.afp` is a rename with no
change in meaning. It started by deleting the app_globals.h include, which
turns every remaining site into a compile error so the compiler lists the
work. Then two steps were repeated until nothing changed: "add the parameter
to any function whose body now names `es`/`d3d`" and "pass it at every call
site" (three rounds; 33 functions, plus the header declarations). Two kinds
of site were invisible to the automation and had to be fixed by hand:
multi-line signatures, and functions that only needed `d3d` once a callee had
gained it.

Verification: the 20-file qpro byte-compare (QPRO_LIMIT=2 over all six
categories at 520x704) is byte-identical before and after, and so are the
14-case render-regression net, 272 tests, and a clean whole-tree tidy.

The remaining shim readers are the afp_d3d9_* TUs (g_gpu, the largest single
block), the DDR TUs, boot/render_loop and the GUI. Those are separate slices.

Deferred on purpose (each is a seam with no consumer today, so cutting it now
would be speculative generality):
- The P16b state-block migration (IfsConfig / LiveState / AFP telemetry
  fields into a `BackendUiState` the AFP panels fetch): deferred until the
  first non-AFP backend lands. Its real needs should shape the split, and
  today every reader of that state is an AFP-registered panel that the P17
  registry already gates.
- qpro RenderService seam (an object owning the mount/slot lifetime instead
  of free functions taking `es`): the parameter threading above had to come
  first; revisit it with the multi-package "Scene designer" work.
- Formal per-panel view-model structs: panels are already thin; add VMs when
  a GUI test harness exists to consume them.
