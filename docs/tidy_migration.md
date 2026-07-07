# clang-tidy whole-tree migration (refactor P9)

How the 51 monolith TUs (flat `src/` + `src/gui/`) were brought under the
tidy gate, and the lessons that migration produced. The end state: the whole
tree (106 compiled TUs) is at ZERO findings under the pinned clang-tidy with
`readability-function-size.LineThreshold` 60, `run_tidy.py` tidies every
tracked `*.cpp` present in `build/compile_commands.json` (build membership is
the catch-all; a standalone source with no database entry is excluded
naturally), and the interim `tools/ci/tidy_scope.json` allow-list is deleted. This doc is the method-and-lessons reference for any
future check enablement or toolchain bump.

## The ratchet method

The same ratchet as the no-comments gate: fix a file (or a check category
across all files), verify, grow the scope file; when the scope covers the
whole tree, flip the gate to catch-all and delete the scope file. Every pass
is verified by build + full ctest + the 3-backend byte-compare regression net
(docs/local_regression.md; sdvx 13 / iidx 13 / ddr 12 outputs), byte-identical
after every pass.

The burn-down is organized by check category, not by file:

- Slice A: mechanical syntax fixits (`--fix`).
- Slice B: `misc-include-cleaner` (`--fix` + manual residue).
- Slice C: `misc-const-correctness` (`--fix`, AnalyzePointers off).
- Slice D: manual categories, per file.
- Slice E: `readability-function-size` / cognitive-complexity - the genuine
  refactor work. Over-complex monolith functions get DECOMPOSED
  (strangler-fig continues), never suppressed.

## Structural config layer

`src/.clang-tidy` is the transitional config for the un-migrated layer; the
migrated module dirs carry re-enable configs so they keep full strictness.
The full design and per-check justification live in docs/gates.md
("Structural suppression"). After introducing the config chain, the scoped
gate (55 files) was re-verified green, and `--dump-config` proves
last-match-wins resolution per layer.

## Baseline

Post-structural-config baseline under clang-tidy 21.1.6: 3113 findings across
the 51 TUs. By category:

| Count | Check | Slice |
|---|---|---|
| 716 | misc-include-cleaner | B: `--fix` + manual residue |
| 632 | readability-implicit-bool-conversion | A: `--fix` |
| 619 | misc-const-correctness | C: `--fix` (AnalyzePointers off) |
| 260 | readability-uppercase-literal-suffix | A: `--fix` |
| 109 | readability-isolate-declaration | A: `--fix` |
| 99 | readability-braces-around-statements | A: `--fix` |
| 95 | modernize-use-designated-initializers | D: manual |
| 80 | misc-use-anonymous-namespace | D: manual (static -> anon ns) |
| 67 | readability-math-missing-parentheses | A: `--fix` |
| 62 | readability-use-std-min-max | A: `--fix` + include check |
| 57 | modernize-use-auto | A: `--fix` |
| 37 | readability-function-cognitive-complexity | E: REAL decomposition |
| 30 | readability-function-size | E: REAL decomposition |
| rest | ~250 across 40 checks | D: manual, per file |

The burn-down took the tree 3113 -> 1883 (slice A) -> ~1200 (literal-suffix
residue, init-variables, qualified-auto, const-correctness) -> 443
(designated-init, anon-namespace, include residue, misc fixits) -> 0. The
slice-D small fixable categories were duplicate-include, concat-namespaces,
static-in-anon-ns, use-auto residue, and member-const.

## Ordering constraint

The `readability-function-size.LineThreshold` 100 -> 60 ratchet (owner
decision, P9) is applied LAST, after slice E: ratcheting mid-campaign would
mix new findings into the burn-down.

## Batch-fix tooling method

- The pip clang-tidy wheel ships no `clang-apply-replacements`, so batch
  fixing runs `--fix` directly, parallelized over DISJOINT TU chunks with
  `--header-filter='^$'` so fixes only ever land in the TU being processed
  (two chunks can never edit the same shared header).
- `--checks='-*,<list>'` on the command line appends after the config chain,
  so it restricts a fix run to the intended checks while CheckOptions (e.g.
  ShortStatementLines: 2) still apply.
- Sweep tooling: 6 parallel chunks, findings deduped on (file, line, check).
  Chunk lists must be written with LF endings - a CRLF list makes xargs pass
  `file.cpp\r` and clang-tidy silently processes ONLY the last file of each
  chunk (burned by this: an early sweep reported 350 findings instead of
  5060 because 45 of 51 files errored out as "no such file").
- After any `--fix` run: `run_format.py --fix`, rebuild, full ctest, then
  the render-regression net before counting the slice as done.
- Toolchain pins can drift underneath a long campaign: the pip clang-format
  and clang-tidy pins were once found background-upgraded (19.1.7 -> 22.1.5,
  21.1.6 -> 22.1.7 - the documented background-upgrade incident).
  run_format/run_tidy refuse correctly on drift, but direct `clang-tidy.exe`
  sweeps do NOT version-check, so manual verifications can silently run on
  the wrong major. When sweeping manually, check `--version` first.
- clang-tidy counts function "lines" INCLUDING whitespace and comments, so
  text-based size estimates under-count; measure threshold candidates with a
  `--checks='-*,readability-function-size'` sweep, not with an editor.

## Per-check gotchas (deduplicated)

Fixit mechanics:

- `readability-use-std-min-max` fixits introduce `std::min`/`std::max` into
  TUs that include `<windows.h>` without `NOMINMAX` -> C2589 at build. The
  repo idiom was a first-line `#define NOMINMAX` per TU; the define is now
  GLOBAL in CMakeLists (`add_compile_definitions`) - see the include-cleaner
  interaction below. Always expect this after a min-max fix pass.
- Overlapping fixits on one line are skipped by clang-tidy (94 of 260
  uppercase-literal-suffix survived the first pass because another check
  edited the same expression); a second `--fix` pass picks them up.
- Fixits can EXPOSE new findings: isolate-declaration splitting
  `int a, b;` made more `cppcoreguidelines-init-variables` sites visible
  (14 -> 26). Expect small count increases in adjacent checks per pass.

`misc-include-cleaner` has THREE Windows failure modes, all hit in practice:

1. It inserts un-includable Windows SDK internals (`timeapi.h`,
   `synchapi.h`, `windef.h`, `minwinbase.h`, ...) as "direct providers";
   they hard-fail without `windows.h` first. Fix: those headers are all
   in `misc-include-cleaner.IgnoreHeaders` (the list grew from 11 to
   ~35 entries); `windows.h`/`d3d9.h` are the sanctioned providers.
2. Its inserts land at the TOP of the include block, ABOVE any
   `#define NOMINMAX` line, silently re-enabling the min/max macros.
   Fix: `NOMINMAX` is defined GLOBALLY in CMakeLists
   (`add_compile_definitions`), per-file defines removed - verified no
   code relied on the macros (one grep hit was HLSL source in a string).
3. Its inserts land OUTSIDE `extern "C" {}` blocks; FFmpeg headers have
   no C++ guards, so `avio_open` etc. got C++-mangled -> LNK2019. Fix:
   the libav includes moved inside the blocks; expect this for ANY
   C-library include block.

More include-cleaner behavior:

- `IgnoreHeaders` matches the PHYSICAL filename, and the SDK ships
  `WinBase.h`/`WinNls.h`/`WinUser.h`/`WTypesbase.h` camel-case while
  diagnostics print lowercase - lowercase entries silently fail to match.
  Fix: `[Ww]in[Uu]ser`-style character classes.
- CRT extension attribution: `stdio.h` (for `fopen_s`) and `string.h` (for
  `strncpy_s`) both needed IgnoreHeaders entries - include-cleaner attributes
  the `_s` functions to the C headers that `modernize-deprecated-headers`
  bans.
- Extracting a helper that spells a previously-inferred parameter type makes
  that type's header a direct dependency (e.g. a helper taking
  `App::IfsConfig&` needs `state/ifs_catalog.h` spelled in the TU).
- `unknwnbase.h` is IUnknown's canonical definer; `unknwn.h` is a wrapper
  the checker refuses to credit (entry added for the WARP pixel-golden work,
  same mechanism).

Manual categories:

- `misc-use-anonymous-namespace` (+ internal-linkage) is scriptable: remove
  `static`, wrap each function in its own `namespace {}` block (adjacent
  blocks merge; clang-format normalizes). Reliable only because the tree is
  formatted + comment-free so braces sit at column 0.
- `readability-named-parameter`: `[[maybe_unused]] type name` on every
  previously-unnamed parameter (comments are banned so the `/*name*/` idiom
  is not available); MSVC /W4 honors the attribute - the build stays at zero
  warnings.
- `modernize-use-designated-initializers` is hand-converted with each
  struct's REAL field order (done for Label, DdrClip, BootIfs, AfpLabel,
  RootRedrive, Candidate, Issue, LayerJob, Layer, ImageSlot, ModuleSpan, the
  render_seh mods struct).
- Narrowing conversions: explicit casts. Multi-level pointer conversions:
  `static_cast` at the FFI argument sites (`T_PROPERTY_NODE*` IS `void**` -
  the avs property tree is untyped).
- Nested conditionals -> if/else chains, `std::min({a,b,c})`, `std::clamp`;
  `bugprone-incorrect-roundings` -> `std::lround(f)`.
- `bugprone-exception-escape`: try/catch in dtors and move-assignment;
  `Log::Write` is NOEXCEPT (it is printf/fwrite underneath) so catch-handlers
  may LOG. The boot scan-thread lambda became a named noexcept
  `ScanThreadBody` - the check attributes the capture-copy string ctor to a
  noexcept lambda. Related: `QproDll::Read` returns its `RecordScan` via
  OUT-PARAM, not by value, because `unordered_set`'s non-noexcept move trips
  the check on the implicit move ctor.
- `cppcoreguidelines-avoid-const-or-ref-data-members`: pass a callback
  per-call instead of storing it as a reference member (hit by the boot
  scan-progress throttle).
- `misc-misplaced-const` rejects `AliasType const x` when the alias is a
  POINTER type - this includes `HWND` (a pointer typedef) and function-
  pointer aliases. Declare such locals non-const.
- WIC `WritePixels` takes a non-const pointer it never writes through; its
  `const_cast` put `cppcoreguidelines-pro-type-const-cast` on the
  transitional subtraction list rather than contorting the call site.
- Dead diagnostics found along the way are DELETED, not suppressed: dead
  stubs, unused RE crop constants (their values live in docs/qpro.md).

Analyzer behavior under decomposition:

- Finer-grained functions let clang-analyzer complete PATHS it previously
  bailed on, surfacing LATENT findings after a pure-motion decomposition:
  a dead-store at a loop tail (`o += 3/2` where `o` re-inits each
  iteration), a `bugprone-branch-clone` created by removing those dead
  stores (the pos3/pos2 branches became identical -> merged), and a
  dead-store on a pane-clamp `deficit` in the GUI. Expect new findings after
  decomposing, even when behavior is byte-identical.
- `clang-analyzer-optin.core.EnumCastOutOfRange` fires on the D3D9 SDK's own
  `D3DTS_WORLD` macro (`=(D3DTRANSFORMSTATETYPE)256`) in unchanged code - an
  opt-in check firing on valid SDK usage; subtracted at the ROOT config and
  documented in docs/gates.md.

## Function decomposition lessons (slice E)

Slice E decomposed 41 over-threshold functions across 30 TUs, then the
LineThreshold 100 -> 60 ratchet surfaced 31 more across 22 TUs; all are
decomposed, never suppressed. Structural lessons:

- Cognitive complexity is dominated by nested ifs inside loops: extracting
  leaf helpers while leaving an action fan-out inline can leave the score
  unchanged (RunRenderLoop stayed at 68 until the per-action posts moved
  out). Big orchestrators shrink in LAYERS, not one pass (WinMain took three
  extraction rounds; RunRenderLoop needed a second round after the ratchet).
- Anonymous-namespace placement is load-bearing for PUBLIC functions: helpers
  for a public function go in an anon-ns that CLOSES right before it, not
  around it - over-wrapping made `DdrAfp::RenderFrame` internal-linkage ->
  LNK2019. Similarly, inserting a helper batch by text marker can land AFTER
  the anon-ns close (the close is a bare `}` that matches markers) -> stray
  brace C2059; move the close down past the new helpers.
- When a helper is extracted from a guarded call site, the guard and its LOG
  stay in the CALLER, or the log doubles (the auto-swap plan resolve
  originally kept both).
- Signatures over `ParameterThreshold` are fixed by dropping parameters
  recoverable from an existing argument (a params[] array) or by introducing
  a shared context struct at the call sites (`QproExtract::CompositeShare`
  replaced a repeated sid/comp_base/main_base trio). The one UNFIXABLE case,
  afp-core's fixed 9-parameter TexUpload callback ABI, is a transitional
  config exception (`ParameterThreshold: 9`; module configs re-pin 8).
- Shared mutable locals need explicit design when split: helper structs carry
  POINTERS to the outer buffers when later iterations must see mid-loop
  mutations (qpro `JobEnv`); function-local statics either move WITH their
  block (single call site) or become anon-ns file-scope `s_*` variables when
  several helpers share them (the qpro GUI options).
- Loop-counter semantics survive extraction only by care: a mountpoint index
  must still increment when the file EXISTS but the mount FAILS, else later
  numbering shifts (LoadOneBootIfs returns true for any existing file); a
  parse-fail path must still bump its progress counter from the caller
  (arc_extract). Inverted short-circuits with side effects
  (`!=sid || --wait<=0` -> `==sid && --wait>0`) must keep the decrement on
  the same path.
- Save/restore nesting can be part of a boot contract: LoadPackages keeps
  the OUTER create-level save/restore bracketing both master-play helpers;
  the per-iteration save/restore stays inside the loop helper. Sequenced
  data-segment patch helpers must run in order, not be reordered.

The decomposition record (which helper carries what), by area:

- video_encoder Create: per-format `Impl::Open*` methods + shared
  FinishColorStream + MuxerName/FmtLabel. window WndProc: HandleCropPick
  (every message path verified identical). media_sink WriteFrame: RAII
  WicPngTarget + OpenPngTarget. afp_d3d9_textures TexUpload: FormatBpp +
  ConvertRows. afp_anim IsMasterComplete: MaybeDumpLayerInfo.
- qpro_dll Read: ScanRecords/KeepMinRuns/BinByCategory. customize_extract
  Run: CollectJobs/ProcessJob. boot ScanGameDir: HasExt/AppendArcIfsEntry/
  TopDirTracker/ScanProgressThrottle. boot BootFromGameDir:
  ResolveBootProfile/CreateRenderWindowAndDevice/SaveBootSettings (second
  round: BootAfpLayer/LoadPersistentIfses/FinishBootAndStartScan).
- render_backend Init: CreateDeviceForWindow/CreateRenderTargets/
  CreateAfpVertexShader/CreateAfpPixelShaders (the runtime-assembled shader
  strings moved verbatim; proven intact by the byte-compare net through the
  real device; later CreateAfpPixelShaders split again into anon-ns shader
  constants + ResolveD3dxCompiler + CompilePs2b). DrawCropOverlay: AppendRect
  builder (both 24-vertex tables are 4 rects with identical winding,
  hand-verified vertex-for-vertex) + SetOverlayRenderStates.
- render_live PublishLiveState: FillLiveState wrap-tally +
  RefreshSubLayerTree + RefreshMcNameList + PublishStatusExtras
  (function-local statics moved with their blocks, single call site).
- main WinMain: Utf8Args/ParseCliOrReport/SeedStateFromSettings/
  PostInitialBootRequest/WaitForFirstBoot/WantsQproCliMode/RunQproOneShot/
  RunQproCliMode/ParseQproPartsCsv/ParseQproOnlyCsv/ForEachCsvToken/
  ResolveStartupIfs+NormalizeSlashes+IfsEntryMatches/RaiseGuiWindow/
  InstallGpuStateHooks (second round: ShutdownEngineStack/RunQproCliAndExit/
  MountStartupContent/CollectCliArgs/StartGuiIfWanted).
- arc_extract Run: CollectArcs/ExtractOneArc/WriteArcEntries. afp_ddr_test
  Run: BootDdrTestStack rc-chain, LoadArcAsIfs, SelfDiffTracker (the
  sanctioned DDR_SELFDIFF diagnostic).
- export StartSession: InitSessionFromRequest, ApplyLabelSuffixToOutput,
  StartDdrPlayback, StartModernPlayback with the shared
  ForceContinuousLoopOverride, LogSessionStart (the static-char-buffer
  lambdas became plain stack buffers). export OnMainLoopTick: TickDdrCapture,
  MaybeDumpTick, HitSafetyCap (the three caps merged into one bool-returning
  gate; second round: BuildModernTick - the dump/log sites read tick.*
  fields). export SubmitOneFrame: OpenSinkForFirstFrame.
- afp_packages LoadPackages: LogPackageDirs, ReadNgpPackage (hint -> title ->
  fcombo00 -> sub_customize_bg001 fallback chain), BuildMasterCandidates
  (hint + hardcoded four + afplist.xml gather), TryPlayMasterAnimation (the
  stream_play loop with per-iteration create-level save/restore inside),
  TryPlayBitmapFallback.
- afp_boot Boot (~300 lines): FillRenderContext / RunAfpCoreBoot /
  RunAfpuBoot / PatchNearFarSlot / RunRenderInits / BindD3D9AndDataSegment /
  ConfigureAfpu / SetStreamNrGuarded / ProbeStreamCreateGuarded (each
  __try/__except probe its own guarded helper; BindD3D9AndDataSegment split
  again into ClearRenderFlag0x800 / CallSetAfpData / VerifyCallbackTable /
  RepatchScreenStubs, run strictly in sequence). afp LoadCompanion:
  MountCompanionImage/QueueCompanionAtlasFilters. PlayBitmapAnimation:
  BuildBitmapStreamArgs/CreateBitmapStream. InstallDefaultRenderStates:
  blend+stencil / stages+samplers halves. SubmitGeometry (afp_d3d9):
  SubmitPreflightOk/LogSubmitGeoProbe/RejectBadVertCount/
  RecordAndDispatchDraw. LoadBootIfses: LoadOneBootIfs.
- afp_ddr_boot LoadIfs: FindPkgClipTable + PkgClipName/PkgClipStreamId
  accessors (dedupe the three base+0x48450 table walks) +
  EnumeratePackageClips / ApplyRootClipOverride / CreateRootLayer /
  CreateExtraLayersDiag. afp_ddr_boot RenderFrame: LogSubPosDiag /
  DumpLoopDiag+DumpLoopProbeLabels / DisplayFrame (all DDR_* debug-env
  diagnostics extracted). SwitchClip: CreateClipLayer. DDR Boot:
  ApplyDdrAfpAttributes/ApplyDdrAfpuAttributes.
- afp_d3d9_callbacks SubmitGeometry: ResolveBoundTexture (returns the slot,
  out-params the dims), LogSeamVertProbe / LogQproDrawProbe diagnostics,
  ResolveFilterState (hsl/add + the two shader constants).
- afp_ddr_render Cb_DrawPrimitive: LogPixColDiag, DrawGateAllows (the
  DDR_ONLY/MIN/MAX env gates), DecodeVtxLayout struct, BuildVertices with
  MulARGB + DrawBBox out-struct, ApplyLineAlpha, MapPrimType, LogDrawDump
  (afp_type/flags/afp_tex/col dropped from its signature - all live in
  params[]), LogTrackBars. Cb_TexUpload: PutArgb + DecodeTexRect per-format
  switch.
- render_loop RunRenderLoop: anon-ns stage helpers ResolveAutoSwapPath +
  NormalizeSlashes + FindIfsBySwapName; LogAutopilotPlans;
  GatherAutopilotInputs; ExecuteAutopilotActions (every act.* post incl.
  BindSubmonitor and PostCliExportRequest); LogAutopilotExit;
  CallAfpUpdateGuarded (logged_update_fault static); TickSubmonitorCyclers;
  ApplyLoopHousekeeping (the 5 loop statics + flag sequence + PublishLiveState +
  MaybeRedriveRootLoop); ApplyMasterScale; RenderOneFrame; PublishFpsStats;
  PaceFrame + QpcNow; WriteCmdTrace. Second round (ratchet): setup preamble
  ResolveRenderFps / ResolveAutoSwapPlan / MakeAutopilot / ArmCommandTaps;
  per-frame mid-section AdvanceFrame (SubmonitorBinding groups the sm slots +
  cyclers; FrameTickResult carries dt/exporting back); tail
  ShutdownAfterLoop. BindSubmonitor: DecodeSubmonitorFrames + one helper per
  arm (Fade/Slideshow/Static). The result is a ~120-line orchestrator:
  setup, then per-frame tick = autopilot -> dispatch -> update -> cyclers ->
  housekeeping -> variants/scale -> render -> fps -> pace.
- render_loop_requests DispatchAppRequest: flat if/else dispatcher over
  anon-ns handlers (HandleHotSwap / HandleQproExtract / HandleForceReplay /
  HandleGotoLabel / HandleToggleCompanion, itself split UnloadAllCompanions +
  LoadCompanionAt + ReplayMasterForBindings).
- qpro_back RenderBackComposite: SetupBackCompositeSlots (BackCompCtx) /
  AttachQproBgStream / HideAllButQproBg / ReadBackTotal / ReadBackHead /
  WriteStillBack / CaptureAnimatedBack / CleanupBackComposite.
  RenderBackRealtime: GrabBackFrame/WriteBackRealtimeStill/
  CaptureBackRealtimeFrames.
- qpro_composite RenderItemComposite (cognitive 139, the worst in the tree):
  SetupItemCompositeSlots (ItemCompCtx carries mainTl) / MountItemClip /
  ProbeItemTotal / ResolveEmitCount (the freeze-probe) / ApplyJobHueScope /
  JobEnv + RenderLayerJob per job / CaptureJobFrames / WriteJobOutputs.
  RenderHandComposite: MountHandItemClip / HideNonHandLayers /
  ApplyHandHueScope / ProbeHandTotal / CaptureHandFrames / WriteHandOutputs.
- qpro_composite_debug HeadComposite: MountHeadItemClipLogged /
  HideNonHeadLayers / LogHeadClipState / RenderHeadDumpSweep /
  LogHeadChildren. HandComposite: HideNonHandLayersAll,
  MountHandItemClipLogged, RenderHandProbeFrames.
- qpro_extract RunBodyPass: BodyCanvasOk / MountBodyMain / BodyPassCtx +
  ExtractOneBody. Run: ResolveOutRoot / BeginRunStatus / FailRunStatus /
  FinishRunStatus / CountSelectedParts / MeasureBackNativeFps / CatMount +
  MountCategorySweep / EmitPlaceholder / SweepCtx + SweepOneItem +
  RunCategorySweep / WritePartsJson / WriteVideosJson / UnmountSweepPackages.
  ClipOne: ApplyClipHueScope. WritePngBGRA + LoadExternalImageSlot: WIC RAII
  structs WicPngTarget/WicDecodeTarget (deleted copy/move + Release dtor,
  replacing goto-style cleanup lambdas).
- qpro_walk CaptureClipFrames: ClipCapture struct + CollectClipFrames /
  ResolveClipCropRect / CropClipFrames. CaptureLayerViaWalk: HandShiftGuard
  hoisted to anon-ns file scope.
- core: avs_xml LoadFromFile (AvsXmlExportsOk/QueryPropertyMemsize),
  hsl_adjust AdjustArgb (RgbToHsl/HslToRgb - identical float expressions,
  r573_formats unit tests stay green), game_runtime LoadScene
  (ApplySceneAtlasFilters/PublishSceneStatus), video_encoder SubmitFrame
  (ExtractAlphaPlane), render_executor DrawCmd Execute (MapDrawPrimType/
  BindDrawTexture/ApplyDrawFilterState/RestoreDrawFilterState).
- GUI: gui_export_panel DrawFpsQualityFrameCap (DrawFpsQualitySliders /
  DrawKeyframeIntervalControl / DrawFrameLimitControls / DrawLoopControls);
  DrawOutputResolution (ResPreset + MatchPresetIdx + ApplyPresetSelection +
  DrawResolutionPresetCombo keeping the shown_idx/last_out statics /
  DrawResolutionInputs / DrawScaleButtons). gui_live_controls
  RenderOverridePanel (DrawLoopTrimControls / DrawBackgroundCycler /
  DrawFilterMcNameControls / DrawLiveStateSection + DrawMcPlayheadLines +
  DrawActiveLabelLine / DrawFileInfoSection / DrawMcNamesList). gui_panels
  RenderVariantSlot (DrawVariantBitmapCombo / DrawVariantBitmapInput);
  RenderIfsInfoPane (DrawIfsSummaryCard / CompanionLocaleName +
  DrawCompanionRow + DrawLocaleOverlayCard). gui_qpro_panel DrawPartScanList
  (DrawScanButton / DrawPartGroup); RenderQproTabBody (DrawQproIntro /
  DrawQproOptions / DrawCategoryChecks / DrawExtractButton /
  DrawExtractStatus; the function-local statics scope_hue, qpro_fps, ex_*
  moved to anon-ns `s_*` so the option helpers share them).
  DrawBackgroundAndHw (DrawHwAccelTooltip), RenderSeekControls
  (PostSeekPaused/DrawPausedCheckbox), loading-overlay Render
  (DrawProgressDetailLine), RenderLayersPanel (DrawLoopMasterControls/
  DrawMasterScaleControls/DrawLayerList), RenderRendererTabBody
  (ClampPaneWidths/RenderRightPane), setup DrawRenderResolution
  (DrawRenderPresetCombo, statics move with it).

## Verification evidence

- Every pass: build + 171 ctest + the 3-backend byte-compare net,
  byte-identical.
- Render-path decompositions are additionally exercised by the net through
  the exact code paths: the boot path (afp_packages) runs for SDVX and IIDX;
  Cb_DrawPrimitive + Cb_TexUpload run on EVERY DDR frame; StartSession +
  OnMainLoopTick run end-to-end for the SDVX/IIDX webp exports.
- The qpro extractor is smoke-tested functionally after its restructure:
  a QPRO_LIMIT=2 hand+back extraction produces hand_N_l/r + back_N + both
  jsons, and a head-only run with `--qpro-only head_66,head_67` produces
  head_68/head_69 files - the game-id -> canonical asset-id (+2 over 65)
  shift survives intact.

## Final state

- Whole-tree sweep (106 compiled TUs, all checks) = ZERO findings at
  LineThreshold 60 (baseline 3113 -> 0).
- `run_tidy.py` tidies every tracked `*.cpp` in
  `build/compile_commands.json`; `tidy_scope.json` is deleted.
- `.clang-tidy` LineThreshold is 60; the ratchet's 31 surfaced functions
  were decomposed in verified batches (each: build + 171 ctest + targeted
  tidy + the 3-backend net).
