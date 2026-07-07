# P6 ownership: EngineSession / GpuContext (strangler status)

The P6 goal: replace the ~50 process-wide inline globals (app_globals.h,
afp_boot_internal.h, afp_d3d9_internal.h) with owned objects created by boot
and passed down explicitly - enabling teardown, re-boot after a failed boot,
and two side-by-side sessions in a test. The mechanism is the plan's strangler
pattern: aggregate first behind shim references (zero call-site churn), then
convert consumers one by one to take the object explicitly, then delete the
shims.

## EngineSession (src/engine_session.h)

One booted Konami engine instance: the three DLL loaders, their resolved proc
tables, and the AFP render context.

- `g_engine` (app_globals.h) is the single instance; the historical names
  `g_avs_dll / g_afp_dll / g_afpu_dll / g_avs / g_afp / g_afpu / g_render_ctx`
  are now inline REFERENCES aliasing its members, so all consumer TUs compile
  and behave identically (verified byte-identical: modern 12-frame dump +
  DDR bg_0009 --ddr-test pre/post).
- Conversion protocol for follow-up slices: when a module is touched, its
  functions taking proc-table/loader parameter lists collapse to
  `EngineSession&` (local `AfpFuncs& afp = es.afp;` aliases keep the body
  diff minimal). New code takes the session, never the shims.
- Signature-collapse step: the 8 lifecycle entry points take the
  session - `Boot(EngineSession&, D3D9State&)`, `Shutdown`, `LoadPackages`,
  `LoadBootIfses`, `UnloadPackages`, `LoadCompanion`, `UnloadCompanion`,
  `UnloadAllCompanions` (~38 call sites collapsed from verbatim
  `(g_afp, g_afpu, g_avs, ...)` triples to `(g_engine, ...)`). Verified
  byte-identical both backends + a companion load/unload run.
- Transport-collapse step: `SwitchAnimation(EngineSession&, name,
  force)`, `ForceReplay(EngineSession&)`, `PlayBitmapAnimation(EngineSession&,
  name)` - 22 more call sites collapsed. PlayBitmapAnimation has NO
  callers (the SDVX Type-2 system-BG path, kept as documented RE capability).
- DELIBERATELY left on raw proc-table params: the single-table `const
  AfpFuncs&` probes and ops (IsMasterComplete, ReadLayerPosition,
  ReadMcPlayhead, ReadLayerAdvanceCounter, EnumerateLabels, GotoLabel,
  SeekFrame, SetStreamPaused, GetRootMcId, EnumerateChildClips,
  DestroyCurrentStream, DestroySceneStreams). One-table read/act surfaces are
  already the minimal fake-drivable seam; wrapping them in the session would
  gain nothing and cost const-correctness.
- NOT in EngineSession: the D3D9 device wrapper - the device's lifetime is not
  the engine's (a hot-swap tears the engine down but keeps the device). It
  lives in GpuContext (`g_gpu.d3d`).

- State-move step: the whole afp_boot_internal.h state block
  lives IN EngineSession - `active_profile`, `afp_booted`, `pkg_id`,
  `stream_id`, `extra_streams`, `anim_name`, `persistent_pkg_ids`,
  `companions` (CompanionRecord moved to engine_session.h) and
  `next_companion_idx`. afp_boot_internal.h is now purely shim references
  over `g_engine`; only the three AfpManager TUs include it. Verified
  byte-identical both backends + a companion load/unload run.

## GpuContext (src/gpu_context.h)

The AfpD3D9 backend's GPU-side state: device alias, shaders, the 256-slot
texture table + per-slot sampler filters, HSV-filter state, and the current
layer transform. `g_gpu` is the single instance; afp_d3d9_internal.h is now
purely shim references over it (the per-member RE knowledge moved to the
member comments in gpu_context.h). Deleting the shims exposed that
render_backend.h's three `extern` escape hatches (`g_fallback_texture`,
`g_texture_width/height`) had NO external users - removed. Verified
byte-identical both backends + a qpro hue-scope run.

The aggregation phase is complete: all three internal-globals headers
(app_globals.h, afp_boot_internal.h, afp_d3d9_internal.h) became pure shim
layers over the two owned objects (`g_engine`, `g_gpu`).

Shim deletion (GPU side): the four AfpD3D9 TUs read
`g_gpu.<member>` directly (361 renames) and afp_d3d9_internal.h carries ONLY
the cross-TU function surface (AfpScreamUnimpl decl, BuildDefaultMatrix,
TransposeMatrix4x4, the scream macro) - its g_* shim block is deleted.
Verified byte-identical both backends + a qpro run.

Shim deletion (engine state): afp_boot_internal.h is
GONE (the first of the plan's "three internal-globals headers deleted").
Inside the 11 EngineSession-taking functions the state reads went to
`es.<member>` (108 sites - these paths are now genuinely session-scoped, the
prerequisite for two side-by-side sessions); the accessors and const-AfpFuncs
helpers read `g_engine.<member>` explicitly (37 sites - their session
threading comes with their own conversion). The gate scripts also gained
missing-file guards (a deleted-but-tracked file crashed run_format /
check_file_length; all five now skip non-existent paths).

## Upward-dependency inversion

The render backend no longer reads App::Global() anywhere (was: TexCreate's
progress bump + DrawCropOverlay's crop state; two more TUs had dead app_state
includes, now removed). GpuContext carries two injected hooks -
`on_texture_created` (load progress) and `query_crop_overlay` (pick mode +
RT-space rect) - wired to App::Global() by main.cpp at startup. Headless
one-shots (--ddr-test) leave them null and the backend behaves as if the
state were default (identical to the old defaults path). `g_d3d` also moved
into GpuContext (`g_gpu.d3d`; the shim reference stays in app_globals.h).

## Still to convert

- The texture-table RE spike is complete (the game's mechanism is known):
  bm2dx uses a mutex-guarded
  FIXED 1024-slot table, ids = 1024+slot, round-robin first-free allocation
  with a rotating cursor (freed slots RECYCLE), destroy zeroes the slot,
  overflow logs loudly and returns -1 (no growth, no crash). The renderer's
  redesign target is therefore: 1024 slots, round-robin reuse, loud -1
  overflow; the persistent-boundary hack becomes optional once slots recycle.
  Implementation is deliberately partial: capacity + loud overflow are in
  place (kTexSlotCount =
  1024 in gpu_context.h, replacing the scattered 256 literals; the 5 texture
  arrays resized; TexCreate logs a "slot table FULL" line + returns -1 on
  exhaustion, matching bm2dx's own log-and-fail). Verified byte-identical both
  backends + a qpro run (peak slot 5, zero FULL messages). The game's
  ROUND-ROBIN slot reuse is deliberately NOT adopted: the renderer's public
  NextSlot/SetNextSlot + contiguous base+offset addressing (40 consumers, e.g.
  qpro `TexUpload(base + atlas, ...)`) is a different model that round-robin
  would break; switching it is a separate, higher-risk slice (qpro-regression
  surface) not a capacity bump. Documented rather than half-shipped.
- Consumer conversion (modules take EngineSession& / GpuContext& explicitly),
  then shim deletion. Export's public surface converted (the P8 ExportSession
  seam, deferred from P6): `Export::OnMainLoopTick(EngineSession&, D3D9State&)`
  and `HandleStartRequest(req, EngineSession&, D3D9State&)` - the internal
  helpers keep their table params via `es.afp`/`es.afpu`/`es.afpu_dll`
  forwarding, so the change is signature-only (byte-verified 12/12 SDVX export
  frames across the stash-dance). export.h no longer pulls the loose
  afp_funcs/afpu_funcs/dll_loader headers - engine_session.h is the seam.

## Exit criteria trace (from the plan)

- Two EngineSessions constructible in a test: needs the proc-table headers +
  DllLoader to link into a test exe (r573_support already does; the funcs
  headers are header-only structs) - reachable once the shims are gone from
  the session's own header chain.
- Three internal-globals headers deleted: after both aggregates + consumer
  conversion.
- Failed boot retryable: still additionally blocked on the P4-deferred
  re-boot RE (whether avs_boot/afp_boot are re-entrant at all); ownership is
  the prerequisite structure, not the proof.

## Export Session ownership (P8)

`Export::Session` (export_internal.h) is no longer a file-shared mutable
global. The single owned instance lives in an anonymous namespace inside
export.cpp; the only way to reach it is `Export::ActiveSession()`. Every
internal export function now takes `Session&` (const where it only reads):
the session threads Publish / Init / StartDdrPlayback / StartModernPlayback /
ForceContinuousLoopOverride / Start-Cancel-FailSession / OpenSinkForFirstFrame
/ SubmitOneFrame / CaptureFrame / BlendComposeAndSubmit / FinishAndEncode /
TickDdrCapture / MaybeDumpTick / HitSafetyCap / BuildModernTick and
export_ddr.cpp's HandleDdrLoopFrame (whose DdrLoopDetector submit lambda now
CAPTURES the session instead of reading the global - SubmitFn is a
std::function, so the capture is free). The five public entry points
(OnMainLoopTick, HandleStartRequest, HandleCancelRequest, IsCapturing,
TargetFps) fetch ActiveSession() and pass it down, so a test can drive the
whole capture/finish state machine on its own Session instance without
touching the app-global one. Verified: 3-backend byte-compare net (the
SDVX/IIDX scenarios run the modern export path end-to-end) + a live DDR
export smoke through the loop-detection path (authored-loop detect -> capture
-> encode).

## P8 exit criteria (audited: met)

- "New modern-afp game = data entry + registry line": audited - zero
  game-specific control flow exists outside the game_profile.cpp kProfiles
  table (the only per-game names elsewhere are profile-FLAG reads like
  apply_iidx_data_segment_patches, CLI help text, and GUI tooltip copy; the
  SDVX 1.5x scale button is a labeled convenience over the generic master
  scale, not a branch). Proven in practice by GITADORA DELTA landing as a
  pure profile entry.
- "GUI panels hold zero business logic": audited - panels read App state,
  draw, and post App::Requests; their only computations call the TESTED
  r573_media_format lib (DeriveExportStem / MakeOutputPath). ImGui usage is
  100% confined to src/gui/ and gated (tools/ci/check_gui_isolation.py).
- "File-length baseline empty": met - the baseline mechanism is deleted and a
  hard 1000-line limit applies.
- Export strategies: one Format enum (media/media_format.h), per-format
  encoder Open functions, MediaSink as the Sink strategy, ExportSession
  ownership threaded (above).

Deferred deliberately (each is a seam with NO consumer today; cutting them
now would be speculative generality):
- CaptureSource seam: the capture already funnels through ONE call
  (D3D9State::ReadOffscreenBGRA in Export::CaptureFrame); introduce the
  injection point together with the first export-loop test that needs a fake
  frame source (blocked on the export.cpp link web: AfpManager / RenderLive /
  Runtime symbols).
- qpro RenderService seam: revisit with the multi-package "Scene designer"
  work.
- Formal per-panel view-model structs: panels are already thin; add VMs when
  a GUI test harness exists to consume them.
