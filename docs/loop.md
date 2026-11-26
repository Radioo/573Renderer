# r573_loop (src/loop/)

Pure loop-detection primitives for the video export path. The module uses only
the standard library, with no AVS, AFP or D3D9, so it builds and unit-tests
standalone (`loop_tests`, CTest label `ci`). It holds the export loop-detection
strategies as tested pure logic, so the DDR authored-loop and pixel-MAD fallback
detector can be checked on synthetic frame streams as well as on live DDR.

## FrameDiffMad (frame_diff.h)

Mean absolute difference of the G channel between two same-size BGRA buffers,
sampled every 16th pixel. It is a cheap measure (~57k samples at 1280x720) of
how different two frames look.

- Returns 0 for identical buffers, and larger values as the G channels diverge.
- Only the G channel is compared (byte offset +1 in each BGRA quad). G carries
  most of the luma, so it tracks visible change well without reading all three
  colour channels.
- Every 16th pixel is sampled (stride 16 in the pixel index).
- A size mismatch or empty input returns a large sentinel (1e9), so a
  mismatched pair never reads as a loop match.

Consumer: the DDR export loop detector (`backend/afp_capture_drivers.cpp`
`HandleDdrLoopFrame`) uses it in two ways. It is the authored-loop wrap check
(a clean current_frame wrap is ~0; a drifting scene's wrap is far above the
`kAfpCleanMad` = 2.5 threshold, e.g. bg_0001 measures ~14), and it is the
content-fallback deep-minimum detector (the frame-vs-reference diff peaks
mid-cycle and dips back at the visible loop). See docs/export_pipeline.md for
the full loop-detection state machine. Under CLAUDE.md this pixel comparison is
the one sanctioned exception to the "never detect playback state from
pixels" rule, and only inside the DDR content fallback. The authored-loop
primary and the modern path use afp's own state.

## DdrLoopDetector (ddr_loop_detector.h)

The DDR export loop-detection state machine, extracted from the DDR capture driver (originally export_ddr.cpp) so
it runs standalone and can be unit-tested on synthetic streams. It owns all of
its bookkeeping (loop reference frame, held frame, wrap and divergence state,
loops-done count). The engine coupling stays in afp_capture_drivers.cpp's thin
`HandleDdrLoopFrame` adapter.

`Feed(const DdrFeed&, const SubmitFn&) -> DdrResult`, one call per captured
frame:

- `DdrFeed` carries the frame (`bgra`/`w`/`h`), the injected authored playhead
  (`current_frame` from afp_mc_get_param 0x1010, `loop_label_frame` = the
  "loop" label frame or < 0 when afp is unavailable), and the session config
  (`loop_count`, `label_active`). Because the playhead is passed in and the
  detector never calls DdrAfp, the detector can be tested without the game.
- `SubmitFn` is the frame sink (`bool(uint8_t*, int, int)`). It is non-const
  because the real sink (SubmitOneFrame) composites the opaque background into
  the buffer in place. It returns false when the sink has failed, which stops
  the detector for that tick.
- `DdrResult` reports `finished` (the caller latches loop_detected), the
  `reason` (LabelEnded / AfpAuthoredLoop / ContentLoop), diag fields for
  the log line, and `loops_done`.

The four detection paths, in priority order (see docs/export_pipeline.md for
where each afp call comes from):

1. Pre-roll: skip the one-shot intro (current_frame below the "loop" label) so
   capture starts in the looping region.
2. AFP authored loop (the primary path): one authored cycle is counted when
   current_frame wraps back past the reference phase and either the wrapped
   frame pixel-matches the reference (FrameDiffMad <= 2.5) or a start label is
   active. It latches `afp_loop_` so the content fallback stays off and the
   cycle is not counted twice.
3. Label end-on-freeze: for a play-once label (e.g. "out"), current_frame
   stalls with no wrap; 8 consecutive ticks without advance end the export.
4. Content fallback (a sanctioned pixel comparison, only for drifting scenes
   where the root loop is not the visible loop, e.g. bg_0001): the
   frame-vs-reference diff peaks mid-cycle and dips to a deep minimum (< 10%
   of the running peak) at the visible loop after at least 60 frames.

   With afp available, the fallback may end the export only after the root has
   wrapped at least once without matching the reference frame (`root_drifted_`,
   set in the authored-loop path when the wrap MAD exceeds the clean bound).
   That unmatched wrap is the afp-state proof that the visible loop is not the
   root loop. Until it happens, the peak and minimum are still tracked, but a
   deep minimum does not count as a loop and the export waits for the authored
   wrap. The reason is slideshow backgrounds: one returns to its first picture
   exactly (MAD 0.00) when its inner slideshow finishes one pass, before the
   root timeline wraps. DDR WORLD background_0036 (three character slides) hit
   the content minimum at 2249 frames, while its root wraps back to the "loop"
   label at 3506 frames with a clean match, and the game shows the 3506-frame
   cycle. background_0015, an older slideshow, is 3506 frames as well. bg_0001
   is unaffected: its root wraps early without a match, which arms the fallback.
   Without afp (`loop_label_frame < 0`) the fallback behaves as before.

   Afp state cannot replace this fallback. Detecting the loop as the moment
   every movie-clip playhead returns to its reference phase together was
   measured live on bg_0001, and it fails in both directions:
   - Resolving clips by name fails: afp_layer_mc_refer(layer, <package clip name>)
     returns the invalid sentinel for bg_0001's clips (instance paths inside
     bg_root are not the package clip names), so a name-based phase vector is
     empty and the first root wrap looks "clean". The export is cut to
     123 frames instead of the correct 3598.
   - Enumerating every clip overshoots: walking the play-work table (the entry table
     and its allocated slot count, located via `get_index_from_mc_id` and its
     afp-play-work.c assert string; entries validated by struct+348 == mc_id
     with group nibble 4, plus the play-work pool grower, pool "play",
     384-byte entries, 128/block) finds 109
     live movie clips in bg_0001. The exact joint-state period across 109
     independent playheads is far longer than the visible loop (an export
     runs past 9 minutes with no return). The visible loop the detector finds
     is a perceptual match (held diff 0.73, not 0) among the few clips that are
     actually visible, and afp state has no notion of "visible" or "close
     enough". The pixel deep-minimum is the only way to express that signal,
     so it stays.

All paths share the one-frame-delayed submit: each frame is held for a tick, so
at the loop point the duplicate is dropped and exactly one clean cycle is
emitted.

Characterization tests (`tests/loop/ddr_loop_detector_tests.cpp`) lock each
path on scripted `(frame, current_frame)` streams with a fake sink, so CI
catches a regression from the extraction or any later change to this code
without needing a live DDR export.

## StepModernLoop (modern_loop.h)

The modern (avs2) export stop decision, extracted from export.cpp's
OnMainLoopTick. It is a pure function, `StepModernLoop(const ModernTick&,
uint32_t& mc_prev_cur, int& loops_done, int& label_seen) -> ModernDecision`,
so it can be tested on synthetic tick streams. The mutable state
(mc_prev_cur / loops_done / label_seen) stays in the export Session and is
passed by reference, because the surrounding safety caps also read it. Only
the decision logic moved.

`ModernTick` carries the afp-state reads for the tick (`have_pos` / `cur_pos`
/ `total_len` from afp_get_layer_info w[12]/w[13]; `mc_valid` / `mc_cur` =
the bounded mc playhead work+0x76 via afp_mc_set 0x1010; `is_master_complete`
= the fallback signal, which the caller reads lazily only when there is no
cur/total and no label, as before) plus config (`idle_frames`,
maintained per tick by UpdateIdleFrames from the composition's afp
playheads, `label_active`, `loop_count`, `hold_mode`).

`ModernDecision` returns `wrapped` (this tick's RT holds fresh loop-start
content, so the loop-closing frame is captured and not dropped),
`naturally_done` (the stop signal), and `master_oneshot`.

The stop logic uses afp state only, never pixels (CLAUDE.md):

- Each tick, a backward step of the bounded mc playhead (mc_cur < previous) is
  exactly one loop. mc_prev_cur starts at the kNoPrevCur sentinel, so the
  launch goto's own backward jump seeds the baseline instead of counting.
- A label export stops after loop_count backward wraps. A play-once label
  like "win" chains into a sustained "loop_win" that moves the playhead back
  and forth forever (the IIDX 33 afp-core gotoAndPlay frame-action handler
  writes work+0x76=120), so the wrap is the only authored loop
  signal.
- A full-clip export stops on whichever fires first: loop_count wraps
  (catches clips that loop at a non-zero frame, where the free cur never
  reaches N*total), the legacy cur >= loop_count*total (a one-shot that the
  continuous-loop dance keeps animating via the stream counter without
  wrapping the root), or master_oneshot (output frozen at total). In HOLD
  mode the free cur is ignored (the continuous-loop dance climbs it
  continuously and it would be counted twice), and only the wrap count and
  oneshot bound the export.
- master_oneshot means the master reached total and every readable playhead in
  the composition has been idle for 8 ticks (idle_frames >= 8). A cur plateau
  alone is not enough, because a looping master can briefly hold cur at the
  wrap while its children's playheads keep advancing.
- When no cur/total is available, the IsMasterComplete latch is the fallback.

Known limitation (unchanged): a bg with a free-running ornament whose period
exceeds one root cycle (select_bg's o_kazari5) will not loop seamlessly at
loop_count root cycles. Its period cannot be detected automatically, so the
user sets --export-max-frames to the ornament's period.

## Blend loop seam (blend_loop.h)

The "Blend loop seam" synthesis, extracted from export.cpp's
BlendComposeAndSubmit. This is the one exception the user opts into to the
"never detect playback state from pixels" rule (see CLAUDE.md): for a
background where afp exposes no authored loop, the user asks the export to
buffer a run of frames and synthesise a seamless loop from them. It is split
into two pure functions so the seam maths can be unit-tested on synthetic frame
streams. The MediaSink open and submit I/O stays in export.cpp.

`PlanBlendLoop(FrameBuffer& buf, int blend_frames) -> BlendPlan`:

- Trims a trailing run of bit-identical (frozen) frames in place. Otherwise a
  one-shot that settled static would pollute the loop search and the
  crossfade tail.
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
  the real start (`buf[index]`), so the seam dissolves instead of snapping.

Characterization tests (`tests/loop/blend_loop_tests.cpp`) lock the loop-length
choice, the trailing-static trim, the crossfade clamp, and the blend direction.

## Module boundary

r573_loop holds `FrameDiffMad`, the full DDR loop detector (`DdrLoopDetector`),
the modern stop decision (`StepModernLoop`), and the blend loop seam synthesis
(`PlanBlendLoop` / `ComposeBlendFrame`), so both loop-detection paths and the
opt-in seam maths are tested pure logic. export.cpp/OnMainLoopTick keeps the
capture and finish flow (CaptureFrame, the loop-closing-frame capture,
FinishAndEncode, the safety caps), which branches on `g_sess.ddr` between the
DDR delayed-submit path and the modern path. Moving that flow behind a
single strategy would retire `g_sess.ddr` (and export's last `g_ddr_mode`
read).
