# r573_loop (src/loop/)

Pure loop-detection primitives for the video export path. Stdlib-only, no
AVS/AFP/D3D9, so it builds and unit-tests standalone (`loop_tests`, CTest
label `ci`). This module holds the export loop-detection STRATEGIES as tested
pure logic, so the renderer's subtlest code (the DDR authored-loop +
pixel-MAD fallback detector) is verifiable on synthetic frame streams instead
of only smoke-tested on live DDR.

## FrameDiffMad (frame_diff.h)

Mean absolute difference of the G channel between two same-size BGRA buffers,
sampled every 16th pixel. A cheap (~57k samples at 1280x720), robust proxy
for "how different do these two frames look."

- Returns 0 for identical buffers; larger as the G channels diverge.
- Only the G channel is compared (byte offset +1 in each BGRA quad) - a
  deliberate speed/robustness trade: G carries most luma, so it tracks
  visible change well without touching all three colour channels.
- Every 16th pixel is sampled (stride 16 in the pixel index).
- Size mismatch or empty input returns a large sentinel (1e9) so a
  mismatched pair never reads as a loop match.

Consumer: the DDR export loop detector (`export_ddr.cpp`
`HandleDdrLoopFrame`) uses it two ways - as the authored-loop wrap check
(a clean current_frame wrap is ~0; a drifting scene's wrap is >> the
`kAfpCleanMad` = 2.5 threshold, e.g. bg_0001 measures ~14) and as the
content-fallback deep-minimum detector (the frame-vs-reference diff peaks
mid-cycle and dips back at the visible loop). See docs/export_pipeline.md for
the full loop-detection state machine. Per CLAUDE.md this pixel comparison is
the ONE sanctioned exception to the "never detect playback state from
pixels" rule, and only inside the DDR content fallback - the authored-loop
primary and the modern path use afp's own state.

## DdrLoopDetector (ddr_loop_detector.h)

The DDR export loop-detection state machine, extracted from export_ddr.cpp so
it runs standalone and is unit-tested on synthetic streams. It owns all its
own bookkeeping (loop reference frame, held frame, wrap/divergence state,
loops-done count); the engine coupling stays in export_ddr.cpp's thin
`HandleDdrLoopFrame` adapter.

`Feed(const DdrFeed&, const SubmitFn&) -> DdrResult`, one call per captured
frame:

- `DdrFeed` carries the frame (`bgra`/`w`/`h`), the INJECTED authored playhead
  (`current_frame` from afp_mc_get_param 0x1010, `loop_label_frame` = the
  "loop" label frame or < 0 when afp is unavailable), and the session config
  (`loop_count`, `label_active`). Injecting the playhead (rather than calling
  DdrAfp) is what makes the detector testable without the game.
- `SubmitFn` is the frame sink (`bool(uint8_t*, int, int)`). It is non-const
  because the real sink (SubmitOneFrame) composites the opaque background into
  the buffer in place; it returns false when the sink has failed, which stops
  the detector for that tick.
- `DdrResult` reports `finished` (the caller latches loop_detected), the
  `reason` (LabelEnded / AfpAuthoredLoop / ContentLoop) and diag fields for
  the log line, and `loops_done`.

The four detection paths, in priority order (see docs/export_pipeline.md for
the afp-call provenance):

1. Pre-roll: skip the one-shot intro (current_frame below the "loop" label) so
   capture starts in the looping region.
2. AFP authored loop (PRIMARY): when current_frame wraps back past the
   reference phase and the wrapped frame pixel-matches the reference
   (FrameDiffMad <= 2.5) OR a start label is active, that is one authored
   cycle. Latches `afp_loop_` so the content fallback stays off (no
   double-count).
3. Label end-on-freeze: for a play-once label (e.g. "out"), current_frame
   stalls with no wrap; 8 consecutive no-advance ticks end the export.
4. Content fallback (a sanctioned pixel comparison, only for drifting scenes
   where the root loop is not the visible loop, e.g. bg_0001): the
   frame-vs-reference diff peaks mid-cycle and dips to a deep minimum (< 10%
   of the running peak) at the visible loop after at least 60 frames.

   This fallback cannot be replaced by afp state. The obvious afp-state
   replacement - detecting the loop as the joint return of every movie-clip
   playhead to its reference phase - fails in both directions, measured live
   on bg_0001:
   - Named resolution fails: afp_layer_mc_refer(layer, <package clip name>)
     returns the invalid sentinel for bg_0001's clips (instance paths inside
     bg_root are not the package clip names), so a name-based phase vector is
     empty and the first root wrap looks "clean" - the export truncates to
     123 frames vs the correct 3598.
   - Full enumeration overshoots: walking the play-work table (the entry table
     and its allocated slot count, located via `get_index_from_mc_id` and its
     afp-play-work.c assert string; entries validated by struct+348 == mc_id
     with group nibble 4, plus the play-work pool grower - pool "play",
     384-byte entries, 128/block) finds 109
     LIVE movie clips in bg_0001. The exact joint-state period across 109
     independent playheads is far longer than the visible loop (an export
     runs past 9 minutes with no return), because the visible loop the
     detector finds is a PERCEPTUAL match (held diff 0.73, not 0) among the
     handful of clips that are actually visible - afp state has no notion of
     "visible" or "close enough". The pixel deep-minimum IS the only
     expressible form of that signal, so it stays, deliberately.

All paths share the one-frame-delayed submit: each frame is held a tick, so
at the loop point the duplicate is dropped and exactly one clean cycle is
emitted.

Characterization tests (`tests/loop/ddr_loop_detector_tests.cpp`) lock each
path on scripted `(frame, current_frame)` streams with a fake sink, so the
extraction and any later change to this delicate code are regression-caught
in CI rather than only on live DDR exports.

## StepModernLoop (modern_loop.h)

The modern (avs2) export stop decision, extracted from export.cpp's
OnMainLoopTick. A pure function - `StepModernLoop(const ModernTick&,
uint32_t& mc_prev_cur, int& loops_done, int& label_seen) -> ModernDecision` -
so it is testable on synthetic tick streams. The mutable state
(mc_prev_cur / loops_done / label_seen) stays in the export Session and is
passed by reference, because the surrounding safety caps also read it; only
the DECISION logic moved.

`ModernTick` carries the afp-state reads for the tick (`have_pos` / `cur_pos`
/ `total_len` from afp_get_layer_info w[12]/w[13]; `mc_valid` / `mc_cur` =
the bounded mc playhead work+0x76 via afp_mc_set 0x1010; `is_master_complete`
= the fallback signal, which the caller reads lazily only when there is no
cur/total and no label, exactly as before) plus config (`idle_frames`
maintained per tick by UpdateIdleFrames from the composition's afp
playheads, `label_active`, `loop_count`, `hold_mode`).

`ModernDecision` returns `wrapped` (this tick's RT holds fresh loop-start
content, so the loop-closing frame is captured not dropped), `naturally_done`
(the stop signal), and `master_oneshot`.

The stop logic, afp-state only (NEVER pixels, per CLAUDE.md):

- Each tick, a BACKWARD step of the bounded mc playhead (mc_cur < previous) is
  exactly one loop; mc_prev_cur starts at the kNoPrevCur sentinel so the
  launch goto's own backward jump seeds the baseline instead of counting.
- A LABEL export stops after loop_count backward wraps (a play-once label
  like "win" chains into a sustained "loop_win" that oscillates the playhead
  forever - the IIDX 33 afp-core gotoAndPlay frame-action handler
  writes work+0x76=120; the wrap is the only authored loop
  signal).
- A full-clip export stops on whichever fires first: loop_count wraps
  (catches clips that loop at a non-zero frame, where the free cur never
  reaches N*total), the legacy cur >= loop_count*total (a one-shot the
  continuous-loop dance keeps animating via the stream counter without
  wrapping the root), or master_oneshot (output frozen at total). Under HOLD
  mode the free cur is ignored (the dance climbs it continuously and would
  double-count) and only the wrap count / oneshot bound the export.
- master_oneshot = the master reached total AND every readable playhead in
  the composition has been idle for 8 ticks (idle_frames >= 8), NOT a bare
  cur plateau: a looping master can briefly hold cur at the wrap while its
  children's playheads keep advancing.
- When no cur/total is available, the IsMasterComplete latch is the fallback.

Known limitation (unchanged): a bg with a free-running ornament whose period
exceeds one root cycle (select_bg's o_kazari5) will not loop seamlessly at
loop_count root cycles; its period is not auto-detectable, so the user sets
--export-max-frames to the ornament's period.

## Blend loop seam (blend_loop.h)

The "Blend loop seam" synthesis, extracted from export.cpp's
BlendComposeAndSubmit. This is the ONE user-opted-in exception to the
"never detect playback state from pixels" rule (see CLAUDE.md): for a
background afp exposes no authored loop for, the user asks the export to buffer
a run of frames and synthesise a seamless loop from them. Splitting it into two
pure functions lets the seam maths be unit-tested on synthetic frame streams;
the MediaSink open/submit I/O stays in export.cpp.

`PlanBlendLoop(FrameBuffer& buf, int blend_frames) -> BlendPlan`:

- Trims a trailing run of bit-identical (frozen) frames in place - a one-shot
  that settled static would otherwise pollute the loop search and the crossfade
  tail.
- Picks the loop length whose frame best matches frame 0 (smallest wrap) over a
  sparse RGBA MAD sample (every 16th byte). The search floor is
  `min(N-1, max(30, blend_frames+1))` and every candidate leaves room for the
  crossfade tail (`length + crossfade <= N`).
- Returns the chosen `loop_length`, the (possibly clamped) `crossfade` count,
  and the winning `best_mad`. `loop_length < 2` means the buffer was too short
  to compose.

`ComposeBlendFrame(const FrameBuffer& buf, const BlendPlan& plan, int index, std::span<uint8_t> out)`:

- Writes output frame `index` of the synthesised loop into `out`.
- For `index >= crossfade` it passes the body frame `buf[index]` through
  unchanged.
- For the first `crossfade` frames it eases (smoothstep `t = x*x*(3-2x)`,
  `x = (index+0.5)/crossfade`) from the natural continuation of the end
  (`buf[loop_length+index]`, the smooth successor of the last body frame) toward
  the real start (`buf[index]`), so the seam DISSOLVES instead of snapping.

Characterization tests (`tests/loop/blend_loop_tests.cpp`) lock the loop-length
choice, the trailing-static trim, the crossfade clamp, and the blend direction.

## Module boundary

r573_loop holds `FrameDiffMad`, the full DDR loop detector (`DdrLoopDetector`),
the modern stop decision (`StepModernLoop`), and the blend loop seam synthesis
(`PlanBlendLoop` / `ComposeBlendFrame`) - both loop-detection paths and the
opt-in seam maths are tested pure logic. export.cpp/OnMainLoopTick retains the
capture + finish FLOW (CaptureFrame, the loop-closing-frame capture,
FinishAndEncode, the safety caps), which branches on `g_sess.ddr` between the
DDR delayed-submit path and the modern path. Unifying that flow behind a
single strategy would retire `g_sess.ddr` (and export's last `g_ddr_mode`
read).
