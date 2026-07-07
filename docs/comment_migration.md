# Comment-knowledge migration (refactor P3)

The method: every source file's comment content is accounted for before its
comments are stripped - unique RE facts and load-bearing design rationale
move into `docs/`, disposable narration just dies. During the campaign each
file carried a status (`captured` = knowledge extracted to the named doc,
comments stripped at module migration; `stripped` = comment-free and under
the no-comments gate), and until a file was stripped its SOURCE remained the
tie-breaker if doc and code disagreed. The migration is complete: every
tracked file with a known comment syntax is comment-free, and `docs/` is the
single source of truth. This doc is the factual map of which source file's
comment knowledge lives in which doc, plus the discrepancies the audit
surfaced.

## Source-to-doc knowledge map

The flat-monolith TUs (from src/render_executor.{h,cpp} down in this table)
were audited from scratch during P9 rather than incrementally.

| File | Doc |
|---|---|
| src/formats/* | docs/formats.md |
| src/media/* | docs/media_formats.md |
| src/cli/* | docs/cli.md |
| src/settings/* | docs/settings.md |
| src/state/* | docs/state.md |
| src/support/* (log, dll_loader, com_ptr, module_handle, expected, env) | docs/support.md |
| src/loop/* (frame_diff, ddr_loop_detector; export loop logic) | docs/loop.md |
| CMakeLists.txt, CMakePresets.json, build.bat, workflows | docs/build.md, docs/ci.md |
| src/app_globals.h | docs/ownership.md (shim rationale) |
| src/afp_boot_internal.h | docs/boot_and_render_loop.md (the header itself was later deleted; its state moved into EngineSession - docs/ownership.md) |
| src/dll_offsets.h | docs/engine_binding.md section 6 (offset table + IDA symbols + versions) |
| src/afpu_funcs.h | docs/engine_binding.md (ordinal table + the 0x036/0x03e/0x03f decoded semantics) |
| src/avs_funcs.h | docs/engine_binding.md (property-tree read chain, gheap 3-arg fix, traversal enum, ordinals) |
| src/afp_funcs.h | docs/engine_binding.md (flags latches, mc_get/set/control codes, load_image struct, enumerate_children header) |
| src/afp_ddr_funcs.h | docs/engine_binding.md (2.13.7 named exports, pw_get_* codes, layer stop/play/info, stream_get_info gotcha) |
| src/mc_control.{h,cpp} | docs/engine_binding.md section 7 (direction enum, bm2dx equivalences, image-bind internals) |
| src/avs_xml.{h,cpp} | docs/engine_binding.md section 8 (binary-XML chain, flags, ATTR reads, PropertyTree lifetime) |
| src/window.{h,cpp} | docs/boot_and_render_loop.md (crop-pick WndProc semantics, mouse macros, RT-space mapping) |
| src/avs_boot.{h,cpp} | docs/boot_and_render_loop.md section 1 (heap, config tree, log writer, mount patterns) |
| src/render_seh.{h,cpp} | docs/boot_and_render_loop.md section 7 (SEH policy, FaultReport, LogFault, scream rate-limit) |
| src/main.cpp | docs/boot_and_render_loop.md (argv, tool modes, boot request flow, --ifs resolution, foreground fix) + docs/cli.md |
| src/boot.{h,cpp} | docs/boot_and_render_loop.md sections 4-5 (boot sequence, scan, DLL discovery, variant/sublayer per-frame asserts, CLI overrides) |
| src/render_live.{h,cpp} | docs/boot_and_render_loop.md section 8 (CAfpViewerScene parity map, Inspect seam, wrap-tally rules, lazy tree) |
| src/render_loop.{h,cpp} | docs/boot_and_render_loop.md section 6 (loop cadence, autopilot, cyclers, flag sequence, scale dispatch, shutdown) + docs/export_pipeline.md (export dt) |
| src/afp_boot.{h,cpp} | docs/boot_and_render_loop.md sections 2-3+5 (boot call order, gates, patches, API contracts) + docs/game_profiles.md (per-gate rationale) + docs/d3d9_backend.md (scene teardown) |
| src/render_executor.{h,cpp} | docs/command_stream.md + docs/d3d9_backend.md (blend/mask/draw/HSL/additive sequences) |
| src/gpu_context.h | docs/d3d9_backend.md (texture table, sampler filters, HSV state, matrix) + docs/ownership.md |
| src/engine_session.h | docs/ownership.md (the P6 aggregate) |
| src/render_backend.{h,cpp} | docs/d3d9_backend.md (offscreen RT, no-MSAA, YIQ blade shader, additive-coverage shader, crop overlay, atlas-filter queue, render-ctx layout) |
| src/game_runtime.{h,cpp} | docs/game_runtime.md (IGameRuntime seam, per-backend method surface, LoadScene split, DDR clip-list design) |
| src/afp_packages.cpp | docs/d3d9_backend.md (scene teardown, master-pick heuristics, persistent boot IFSes, type=8 sweep) |
| src/afp_d3d9_commands.cpp | docs/d3d9_backend.md (PushMatrix transpose, ortho, SetShapeMatrix notify) + docs/command_stream.md (LayerCommand dispatch) |
| src/afp_d3d9_callbacks.cpp | docs/d3d9_backend.md (HSV sentinel, tex-ref encoding, BG-3 x=540 seam trade study, UV inset) + docs/command_stream.md (id-192 determinism, deferred capture) |
| src/afp_d3d9.cpp | docs/d3d9_backend.md (InstallDefaultRenderStates, alpha-MAX blend, game-verified sampler state, per-frame reset, afpu_render_info counters) |
| src/afp_d3d9_textures.cpp | docs/d3d9_backend.md (format-id table, TexCreate slot alloc, atlas-filter drain, LoadExternalImageSlot) + docs/ownership.md (1024-slot overflow) |
| src/afp_anim.cpp | docs/engine_binding.md + docs/boot_and_render_loop.md sec 5 (mc-playhead codes, labels, seek/pause, companions) + docs/d3d9_backend.md (mc_set handler addrs) + docs/export_pipeline.md (0xE0000000 latches, hantei no-signal) |
| src/ifs_inspect.cpp | docs/boot_and_render_loop.md sec 4.3b (manifest parse, 3-tier variant discovery) + docs/d3d9_backend.md (texturelist NEAREST/x=540) |
| src/arc_extract.cpp | docs/ddr.md (.arc format, two-pass extraction) + docs/formats.md - the source comments were mostly progress narration |
| src/customize_extract.cpp | docs/ddr.md (category mapping, lossless PNG minify chunk policy) |
| src/qpro_dll.cpp | docs/qpro.md (PE part-array parse, Elpis head1/2 numeric-suffix classify) |
| src/qpro_scan.cpp | docs/qpro.md (part scan, IFS modified-date group key) |
| src/qpro_walk.cpp | docs/qpro.md (clip-walk pipeline, design-canvas crop, afp_fps, straight-alpha, hue-scope, occlusion pairing) |
| src/qpro_internal.h | docs/qpro.md (shared helpers, avatar-layer list, animated-part trio) |
| src/qpro_extract.cpp | docs/qpro.md (Run/*One orchestration, hue-scope region1/2, mask_type probe, LayerJob category map, freeze-play, qpro_videos.json trio, native-fps probe) |
| src/qpro_composite.cpp | docs/qpro.md (mount-once/MOUNTED mode, freeze-play statue, shared-canvas crop bounds, clip-attach vs texture-swap, animated-vs-static, the bm2dx afp_mc_attach_stream item-clip mount) |
| src/qpro_back.cpp | docs/qpro.md (native-fps standalone probe, dt=(1/fps)(native/60) scaling, one-shot-vs-looper afp-state loop, effect-bg matrix-stack-strand diag) |
| src/qpro_composite_debug.cpp | docs/qpro.md (HandComposite/HeadComposite diagnostics) |
| src/export.cpp | docs/export_pipeline.md (session machine, clear-transparent, DDR/label seek, Root Force/Hold, continuous-loop dance, static-frame detect, blend loop) |
| src/export_internal.h | docs/export_pipeline.md (Session fields, static-frames, DDR content-loop bg_0001@3607, root-hold) |
| src/export_ddr.cpp | docs/loop.md + docs/export_pipeline.md (DdrLoopDetector bridge) |
| src/afp_ddr_render.cpp | docs/ddr.md (render-params slot map, set_mask scissor, HSL filter #31416d, blend mode 8, draw_primitive fmt, DXT decode) |
| src/afp_ddr_boot.cpp | docs/ddr.md (the gamemdx boot function, 0x819 per-node MASK bit, pp-pool mode-3, package-table clip enum, layer_create) |
| src/afp_ddr.{h,test.cpp,render.h,test.h} | docs/ddr.md (DDR API surface, --ddr-test smoke tool) |
| src/gui/* (20 files) | docs/gui.md (panel/view-model design, the variant "(default)" no-un-override behavior of `afp_play_work_load_bitmap`, live-controls seek/playhead) - mostly self-documenting ImGui widget layout; RE refs are cross-links to engine docs |
| src/tool_commands.{h,cpp} | docs/cli.md + docs/boot_and_render_loop.md (self-contained tool modes) |
| src/game_profile.{h,cpp} | docs/game_profiles.md (per-game ordinal overrides, kSkip gate, per-game DLL data-segment offsets, profile registry + detection) |
| src/video_encoder.{cpp,h}, src/video_encoder_codecs.{cpp,h} | docs/media_formats.md "Encoder implementation" (AVIF dual-stream, per-format stream/muxer choice, WebP loop=0 both layers, MP4 faststart, H.264 even-dims, NVENC probe cache) + docs/export_pipeline.md |
| src/media_sink.{cpp,h} | docs/media_formats.md "PNG-sequence backend" (WIC frame_NNNNNN.png, CoInitialize once/session, moved-from guard, best-effort cleanup) + docs/export_pipeline.md |
| src/native_dialog.{cpp,h} | docs/media_formats.md "Native file dialog" (STA + per-call CoInitializeEx rationale) + docs/export_pipeline.md (appendix) |
| src/render_loop_requests.{cpp,h} | docs/boot_and_render_loop.md (request dispatch: animation switch/label goto/seek/pause, exclusive companion selection, qpro batch main-thread + >=520x704 RT, CAfpViewerScene key map) |
| src/afp_d3d9_internal.h, src/arc_extract.h, src/customize_extract.h, src/export.h, src/ifs_inspect.h, src/qpro_{dll,extract,model,scan,walk}.h | declaration headers for already-stripped .cpp; facts in the docs above |

## Whole-repo warnings coverage (P9)

`r573::warnings` is linked into the `renderer` executable too (it was the
last target without it). Driving the monolith warning-clean produced one new
module - `src/support/env.{h,cpp}` (`Support::EnvVar/EnvFlag/EnvInt`, doc
docs/support.md "Env") - which replaced ~30 raw `getenv` debug-knob reads in
the DDR/qpro paths with a `GetEnvironmentVariableA`-backed helper (no C4996,
and `<windows.h>` is in include-cleaner's ignore list). Dead statics
(ApplyMat2d/ApplyOrthoIdentity, the qpro MaskProbe + HideAllMotionClips
diagnostics), unused params, and dead locals were removed; `fopen`->`fopen_s`,
`sscanf`->`sscanf_s`, `strncpy`->`strncpy_s`. A pre-existing include-cleaner
gap in `tests/state/app_state_tests.cpp` (the P8 state-split moved BootState/
IfsConfig into sub-headers the test never included directly) was fixed in the
same pass. Verification for the pass: all no-comments-gated files, the full
test suite, whole-tree clang-tidy, and the 3-backend byte-compare regression
stayed green.

## Gate end state (P9 whole-repo flip)

Every tracked file with a known comment syntax is comment-free. The
no-comments gate flipped from an allowlist (`no_comments_scope.json`, grown
per migrated file through P8, now removed) to catch-all-minus-exempt: it
checks all tracked source/tooling files and forbids comments in every
one. The only exemptions live in `tools/ci/no_comments_exempt.json`:
`.idea/*` (IDE config) and `vcpkg-overlays/*` (vendored third-party
portfiles). New
source files are caught by default - there is no per-file opt-in to grow.

## Open questions and discrepancies found by the audit

Discrepancies between comments, or between comments and code, preserved in
the docs instead of silently resolved. Each needs an RE pass or a decision:

1. AVS property-traversal enum: avs_funcs.h and avs_xml.h documented
   different value maps from the same claimed source. The avs_xml.h map is
   verified by working code (NEXT_MATCH=7) and marked authoritative in
   docs/engine_binding.md; re-check the avs_funcs.h copy on the next IDA
   pass.
2. afp_play_work_load_image attach flag: the afp_funcs.h comments said "pass
   1 to mirror the game"; mc_control.cpp ships attach=0 "per the game"
   (soundvoltex.dll SdvxSubmoniBg_Load mirror). Shipping behavior is 0.
3. Modern SeekFrame failure: the render_live.h comments claimed it always
   returns true; afp_boot.h documented the refer-check failure path. The
   mechanism (refer result -4) is documented; align the two claims on the
   next RE pass.
4. Atlas-filter contradiction: early comments said the game honors
   texturelist per-atlas filters (the x=540 seam fix); the later real game
   sampler probe in afp_d3d9.cpp concludes the strings are a red herring
   and the consumer is disabled. Docs carry the probe as current truth.
   Resolve inside the P6 texture-table RE spike before changing behavior.
5. TexUpload format 0x1F: the comment said rgb565 (2 bpp) but the decode is
   4-bit ARGB nibbles scaled by 17 (argb4444). Verify against the
   afp-utils format table and fix label or decode.
6. export.h claimed the exporter shells out to avifenc.exe - stale, flagged
   historical in docs/export_pipeline.md.
7. game_profile.cpp carried a literal drive-letter install path in a
   comment and an explicit rename TODO; both died at migration (the path is
   generalized in docs/game_profiles.md).
8. DDR layer struct: "+40" vs "+0x40" for the force-advance byte in two
   files - possible decimal/hex notation collision; reconcile on the next
   libafp pass (docs/ddr.md flags it).
9. afp_boot.cpp afpu_data tail constants are tentative ("far plane (?)"),
   and the bm2dx afp_set_flag(16/8/65537) triple has unknown semantics; both
   documented as unverified.
10. Pixel-based legacy detectors: the static-frame export stop was
    subsequently migrated to playhead-idle; the DDR content-loop fallback
    was investigated for migration and proven unmigratable, so it is a
    sanctioned exception alongside the blend-loop seam (evidence in
    docs/loop.md); DDR_SELFDIFF remains a diagnostic.

## Standing rule

The migration is complete: the whole repo is comment-free and the gate is
catch-all. The rule runs forward, not as a backlog - any NEW source
file must land comment-free, with its unique RE facts and load-bearing
design rationale written into the relevant `docs/` file in the SAME change.
The gate catches a stray comment automatically; the discipline is that the
knowledge is not lost when the comment is removed. The captured-draft
tie-breaker era is over - `docs/` is the single source of truth.
