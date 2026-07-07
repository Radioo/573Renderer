# Export pipeline (src/export.*, src/export_ddr.cpp, src/video_encoder.*, src/media_sink.*)

Capture-and-encode pipeline driven from the main render thread. The format
vocabulary (MediaSink::Format enum values, tokens, aliases, extensions,
WritesDirectory, MakeOutputPath) is documented in docs/media_formats.md and is
NOT repeated here; this file covers the capture state machine, loop/stop
detection, compositing rules, and the encoder/sink internals.

## 1. Public surface and call flow

Three entry points, called by the main loop between afp_do_update and
EndFrame:

- `Export::HandleStartRequest` (dispatch from main.cpp's request handler, the
  same spot that handles toggle_companion / force_replay / switch_animation;
  kept as free functions so per-session state stays private to export.cpp)
  -> StartSession. How the root timeline is driven depends on RootLoopMode:
  in Force mode (or a label/blend export) it force-replays / drives the root
  so capture begins at frame 0 and loops; in Hold mode (the game default
  for the modern scene-BG path) it leaves the already-mounted stream alone so
  the root plays once and HOLDS while nested children free-run (no root
  re-drive, so a multi-frame child like select_bg's o_kazari5 does not snap
  back each root cycle).
- `Export::OnMainLoopTick` - per-frame tick. Decides whether this main-loop
  frame contributes a sample based on the target fps, snapshots the offscreen
  RT, advances frames_captured, and detects completion (see section 5).
- `Export::HandleCancelRequest` -> CancelSession: discards output, flips
  state back to Idle.

`Export::IsCapturing()` is true between StartSession and FinishAndEncode. The
main loop uses it to drive AFP at the export's target frame rate (dt = 1/fps)
rather than real-time 120 Hz - that way the encoded output plays back at the
animation's authored speed regardless of how slow the encoder is running. It
is also used to skip the 120 Hz frame-pacer loop during export so the encoder
can chew through frames as fast as the CPU allows. `Export::TargetFps()` is
only valid while IsCapturing().

Encoding runs synchronously in the tick path; errors are published into
`App::ExportState::error` and the phase moves to Failed. Encoding goes
through MediaSink/VideoEncoder in-process, never by shelling out to an
external encoder such as avifenc.exe.

## 2. Session state (export_internal.h)

`Export::Session` / the single `g_sess` live in export_internal.h so the DDR
content-loop detector (export_ddr.cpp) shares the same state and the
SubmitOneFrame helper, mirroring the renderer's other `*_internal.h` split
pattern (afp_d3d9_internal.h etc.). Not a public API - only export.cpp and
export_ddr.cpp include it. The Session owns the capture side (D3D9 readback,
bg-colour composite, reference dump for diff tests) and delegates the
FFmpeg / libaom / libvpx / NVENC muxing to MediaSink.

Field semantics worth preserving:

- `max_frames`: 0 = no cap (capture until the natural end); any positive
  value stops at exactly that many frames. Checked AFTER each CaptureFrame so
  the count includes the most-recent frame (max_frames=N produces exactly N
  encoded frames, not N-1 or N+1). When set, the user wants EXACTLY that many
  frames regardless of the natural-end detector - the detector is unreliable
  for animations like bg_bpls5 where AFP marks "complete" early (the
  set_complete bit fires on cycle 1 even though the visible animation keeps
  playing for several more seconds) or where cur/total are in microseconds
  and wrap unpredictably.
- `prev_playheads` / `idle_frames`: playhead-idle auto-stop (afp state, NOT
  pixels; supersedes an earlier bit-identical-frame compare). Each tick,
  UpdateIdleFrames snapshots every readable afp playhead in the
  composition: the master layer cur_pos, the bounded mc playhead, and every
  named child clip's current_frame (EnumerateChildClips). If the snapshot
  equals the previous tick's, idle_frames increments, else resets; an EMPTY
  snapshot (no playhead readable) never counts, so content with no exposed
  state falls back to the master-complete latch and safety caps. Equivalent
  on the canonical content, SDVX hantei.ifs's "perfect" judgement
  animation, which re-submits the SAME final frame indefinitely and never
  sets afp's end-of-animation flag bits - but its PLAYHEADS saturate at the
  authored end, so pixel-static and playhead-idle fire on the same tick
  (both detectors produce the same 289-frame, byte-identical webp). A
  draw-count-based detector does not work because afpu still emits draw
  calls for the static last
  frame - only the pixel CONTENT stays identical. Storage cost: one
  prev-frame BGRA copy, ~8 MB at 1080x1920x4, allocated once per session.
- `start_cooldown`: lets afp_do_update tick a few times before sampling so
  the stream work-struct has populated past frame 0. Always set to
  0 - short clips like exp00 need it off.
- Crop rect (`crop_x/y/w/h`): applied BEFORE the output-size scale, so
  crop_w x crop_h is the encoder's src dimensions and out_width/out_height
  still drive the final scale (defaulting to crop size when zero).
- `format` mirrors App::Request::export_format (the canonical enum, see
  MediaSink::Format). `prefer_hw` asks the sink to try NVENC for the video
  backends (no-op for PNG / VP9 / WebP). `using_hw` is only populated after
  Sink::Open returns, so it can be published to the GUI progress pane.
- `saved_clear_color` / `d3d_ptr`: D3D9 clear-color override state (see
  section 3).
- `dump_frames_dir`: per-frame BGRA dump dir for ground-truth diff tests.
- `sink`: owned for the whole session (Open -> SubmitFrame per capture ->
  Finish). MediaSink hides whether the backend is a VideoEncoder (AVIF /
  WebM / WebP / MP4) or a per-frame PNG writer (PNG_Sequence).

DDR-specific fields are described in section 7; label/blend fields in
sections 4 and 8.

## 3. Compositing rules (the premultiplied-alpha model)

### Clear colour is pinned transparent

The offscreen RT's clear colour is pinned to FULLY TRANSPARENT
(ARGB 0x00000000) for the whole capture session, INDEPENDENTLY of the
user-chosen bg. Rationale: AFP's blending pipeline mixes premultiplied
alpha-blended draws with additive / screen / multiply modes. Clearing to an
opaque colour "bakes" the background into D3D9's blend math differently for
each mode; clearing to transparent lets us read back a BGRA buffer whose
interpretation is uniform across all modes (premult rgb + real alpha), and
the user's opaque bg is composited in software AFTER readback with a single
formula that matches what a browser would produce. The user-visible D3D9
window is unchanged: the backbuffer is XRGB, so StretchRect drops alpha and
displays the RGB as-is. The saved clear colour is restored when the session
ends.

### Opaque-bg composite formula

AFP writes PREMULTIPLIED RGB into the RT (D3D9's SRCALPHA/INVSRCALPHA blend
multiplies src.rgb by src.a before summing). Additive-mode draws meanwhile
leave raw RGB additions with alpha unchanged - e.g. the bright burst centre
in fcombo00 ends up stored as RGB=(255,255,255), A=0.

The "premultiplied source-over" composite
`display = src_rgb + bg * (1 - src_a)` is physically correct for both kinds
of content:

- premult alpha-blended pixel (src_rgb=128, a=128 at authored rgb=255,
  a=128/255): display = 128 + bg * 127/255 - the same result D3D9 would
  produce if the BG had been cleared to bg instead of transparent.
- additive pixel (src_rgb=100, a=0, from AFP's ADD blend):
  display = 100 + bg (clamped to 255) - additive content brightens the bg as
  intended.

The captured file therefore looks identical to what D3D9 would render with
the user's chosen bg colour as the clear colour - no codec-side compositing
required. Implementation uses integer math with +127 rounding per channel
and clamps to 255; output alpha is forced to 255.

## 4. Start modes (StartSession)

Session parameters are copied from App::Request with defaults: fps 60 if
unset, quality 60 if outside 0..100, loop_count 1 minimum, blend_frames 15
default. All loop/blend/content-detector bookkeeping is reset explicitly
because g_sess is reused across sessions.

The SELECTED frame label (the one the user last clicked in the Labels list)
is snapshotted ONCE at StartSession for both backends, so a label toggled
mid-capture cannot retroactively change a running export. When a label is
active, it is auto-appended to the output filename before the extension
(foo.webp -> foo_<label>.webp, appended if no extension) so per-label exports
do not clobber each other.

### Modern (non-DDR) start paths

- LABEL export: a labelled segment (e.g. "win") plays its intro via
  deep-goto-play (afp_mc_control 0xF09) then CHAINS into a sustained loop
  label ("loop_win") via an authored gotoAndPlay; the bounded mc playhead
  work+0x76 wraps BACKWARD at each loop boundary. StartSession re-APPLIES the
  label (rather than ForceReplay to frame 0) so capture starts at the label's
  first frame; OnMainLoopTick counts backward wraps and stops after
  loop_count of them. The continuous-loop flag sequence is deliberately NOT
  driven: it makes the free-running stream counter climb past total forever
  (the runaway-cur / never-ending-export bug). If the user has the persistent
  "Continuous loop" override ON, it is forced OFF for the session (the render
  loop's per-frame bit-0 re-set would interfere with the clip's authored
  loop) and restored on finish.
- BLEND export: ForceReplay to frame 0 for one clean pass to compose from,
  never the continuous-loop dance (blend buffers a single pass and finds its
  own loop).
- FORCE root mode: taken when RootLoopMode == Force OR the user already had
  the live continuous-loop override ON (an explicit "keep this looping"
  request that must win). ForceReplay to frame 0 + drive the continuous-loop
  dance so a one-shot master (e.g. bg_common) actually loops past one cycle.
- HOLD root mode (the game default): do NOT ForceReplay (a full remount
  resets every nested child to frame 0 = the o_kazari5 snap), but DO drive
  the continuous-loop dance. The dance is the real game BG dispatcher's
  CLayer-flag maintenance that keeps the master CLOCK running so the root's
  own tick loops it (shallow-seek, REUSING persistent nested children).
  WITHOUT the dance the master clock stops at end-of-timeline and the ENTIRE
  scene freezes, children included (passive Hold produces byte-identical
  frames; the dance makes root-phase-aligned frames DIFFER, proving the
  child free-runs). Before capturing, the layer is
  REWOUND to frame 0 via SeekFrame(0) = afp_mc_deep_goto_play(0) (op 0xF08),
  which gently rewinds the root master AND deep-syncs nested children to
  frame 0 WITHOUT the afp_stream destroy+recreate that ForceReplay does - a
  clean frame-0 start with no remount snap. A no-label export must START at
  the beginning of the timeline, never at the drifted preview playhead.
  Length is bounded by master_oneshot / loops_done / max_frames / the hold
  safety cap (section 5).
- "Continuous loop count" semantics: loop_count means "give me N loops", but
  a one-shot master (e.g. select_bg_vi's bg_common, which freezes after one
  master cycle) only keeps looping while the BG-dispatcher continuous-loop
  flag sequence is active, so a modern (non-blend) loop export drives that
  itself - saving the user's live setting and restoring it when the session
  ends (`saved_continuous_loop` / `forced_continuous_loop`).

### DDR start paths (legacy AFP 2.13.7)

- DDR LABEL export: RE-SEEK to the selected label so capture STARTS there,
  not at the drifted playhead. afp_mc_op 0xF08 (deep_goto_play) positions and
  resumes. The pre-roll threshold (ddr_loop_label) is pinned to the selected
  label's frame so HandleDdrLoopFrame begins capturing right there instead of
  auto-skipping the intro to the "loop" label; it then stops after loop_count
  authored current_frame wraps from that start (the wrap alone counts when
  label_active, since a non-"loop" start won't pixel-match the "loop" wrap
  reference).
- DDR no-label export: rewind to a DETERMINISTIC start so capture never
  begins at the drifted preview phase (user directive). The natural start of
  a DDR loop is the "loop" label - the exact frame the authored loop wraps
  BACK to - NOT frame 0, which is the one-shot intro the detector's pre-roll
  deliberately skips. The GENTLE SeekFrame (afp_mc_op 0xF08 deep_goto_play:
  seek + resume, recurses to children) is used, NOT afp_layer_stop: the old
  objection ("do not rewind to frame 0 -> seam") was specifically about
  afp_layer_stop's teardown+reinit-to-0; a deep_goto_play to the "loop" label
  lands on EXACTLY the state the loop naturally returns to, so there is no
  seam. A scene with no "loop" label (ClipLabelFrame returns -1) is not an
  intro+loop: its visible loop is found by the content detector, and "the
  beginning of the layer" IS frame 0, so it rewinds there (still the gentle
  0xF08 seek).

### Pause interaction

The live stream is forced to RUN at capture start. The CAfpViewerScene-style
PLAY/PAUSE control (LiveOverrides::paused) sets the stream speed to 0, which
freezes the bounded playhead (the per-stream tick function in afp-core returns
early when speed*dt <= 0). If the user left the preview paused and
started an export, the capture would record a frozen frame. The per-frame
pause-defend is already gated off while exporting, but the stream may ALREADY
be at speed 0 at capture start, so it is resumed once in StartSession. DDR
uses afp_layer_play(layer, 0) for pause (NOT a stream speed), so it gets the
symmetric resume via DdrAfp::SetPaused(false). On session end (finish, cancel
or fail), RenderLive::ResetPauseDefend() re-arms the defend: the capture
forced speed 1.0 and the defend's cache is stale, so clearing it lets the
user's sticky paused override re-assert.

## 5. Stop conditions (modern path, OnMainLoopTick)

The master stream's position is snapshotted BEFORE capture (afp_do_update has
already run upstream, so `cur` is the live cursor for the frame currently in
the RT). Stopping at cur >= total happens BEFORE capturing because on the
saturation tick AFP renders the same pixels as the previous tick (the
cur=total bump is the engine's "we just reached the end" marker, not a fresh
authored frame); capturing it would produce a duplicate at the tail - exactly
the bug where frames 299 and 300 came out identical. Reading w[12]/w[13] via
ReadLayerPosition matches what the live-state widget shows ("cur N/N"). When
the layer-info path is unavailable (older AFP-core or a layer type whose info
struct doesn't populate w[12]/w[13]), IsMasterComplete is the fallback, with
the OLD "exit BEFORE capture" semantics: loses 1-2 frames but never emits
duplicates.

### The universal mc-playhead wrap counter

A play-once label (e.g. arena.ifs 1p_judge "win") does NOT self-stop: it
plays its intro then CHAINS into a sustained loop label ("loop_win" at frame
120) via an AUTHORED gotoAndPlay frame-action. In IIDX 33 afp-core, the
gotoAndPlay frame-action handler writes *(u16*)(work+0x76) = 120, a BACKWARD jump
from ~239; the same action at the loop_win out-point re-seeks 120, so the
playhead OSCILLATES in [120, segment_end] forever. The ONLY afp-state loop
signal is the BOUNDED mc playhead work+0x76 (afp_mc_set code 0x1010, read by
ReadMcPlayhead) DECREASING between two consecutive post-do_update reads -
each backward wrap is exactly one loop. The clip plays each segment forward
(cur strictly increases), so cur only ever decreases at the authored loop
boundary: every detected decrease is authoritative, no over-count.

Backward wraps are counted for EVERY export (label AND full-clip).
mc_prev_cur starts at a 0xFFFFFFFF sentinel so the launch goto / ForceReplay
backward jump SEEDS the baseline on the first tick rather than being
miscounted as a loop. Full-clip use: a clip whose frame-0 sequence loops at a
NON-zero label (e.g. 1p_judge -> loop_win at 120) never satisfies the
free-running cur >= N*total (w[13] only climbs on a wrap-to-frame-0), so
without the wrap count it runs forever.

A per-tick playhead trace is available via env var EXPORT_TICK_DUMP=1
(mirrors RENDERER_LAYER_INFO_DUMP): logs the bounded mc playhead, free
cur/total and the wrap tally each tick so a loop-seam off-by-one can be read
straight from the log. Off in production.

### master_oneshot

Decides whether the master is a one-shot that cannot satisfy loop_count > 1.
A one-shot reaches `total` and FREEZES (rendered output stops changing). A
looping master - a natural root-loop, or a one-shot kept looping by the
continuous-loop flag sequence - keeps `cur` climbing cumulatively past `total`
AND keeps the output animating. So the signal is "cur reached total AND the
COMPOSITION's playheads have gone idle" (idle_frames >= 8), NOT a bare cur
plateau: a
looping master can briefly hold cur at the loop wrap while still animating,
and keying off cur alone wrongly cuts multi-loop captures short there. When
master_oneshot fires with loop_count > 1, a log explains that loop_count > 1
needs a looping bg (try Continuous loop).

### The combined stop expression

- Label export: stop when loops_done >= loop_count (label_done).
- Full-clip export with position info: stop on whichever fires first:
  - loops_done >= loop_count (mc-playhead wraps, the debug-scene-faithful
    signal; catches clips that loop at a non-zero frame), OR
  - cur >= loop_count * total (legacy fallback for a one-shot the
    continuous-loop dance keeps animating via the stream counter without
    wrapping the root MC; gated OFF in hold mode - with the dance the FREE
    cur climbs continuously and would double-count against loops_done), OR
  - master_oneshot.
- No position info: AfpManager::IsMasterComplete fallback.

loop_count counts MASTER CYCLES, and is fps-independent: the master advances
at the asset's native rate (often 120fps), so at a 60fps export cur climbs
~2/frame and one cycle is ~total/2 export frames - same SPEED, half the frame
count. One "loop" is the master timeline, the same length the UI reports
(e.g. 1200 for bg_common).

HOLD mode note: under the dance the root LOOPS through its own tick
(shallow-seek, reusing persistent children), so its bounded mc playhead
WRAPS and loops_done counts real root cycles (bg_common's root cur wraps
under the dance). But a bg with a long
free-running ornament whose period EXCEEDS one root cycle (select_bg's
o_kazari5) will not loop SEAMLESSLY at loop_count root cycles - the ornament
is mid-rotation at the seam. Its period is not auto-detectable: the ornament
is an ANONYMOUS embedded PlaceObject child (afp_mc_get_id_by_path returns -4,
afp_mc_enumerate_children returns 0 named children, afp_mc_get stubs the
frame/total codes), so the public afp API exposes no child playhead to bound
on (a child-cycle terminator would require an afp_hook-confirmed child
work+0x78 walk or a new ordinal).
For a seamless ornamented loop the user sets --export-max-frames to the
ornament's period. A pixel "is it done" comparison is never substituted.

### The loop-closing frame rule

When the stop is a backward WRAP of the bounded mc playhead (mc_wrapped_now),
THIS tick's RT holds fresh loop-start content (the authored gotoAndPlay
jumped the playhead back to the loop frame), NOT a duplicate - it MUST be
captured so the encoded loop spans exactly one full period. Otherwise the
loop is one frame short and SKIPS a frame at the seam: bg_room is a
1200-frame master driven at the 60fps export dt (2 master frames/tick), so a
clean loop = frames {0,2,...,1198} (600 frames). Dropping the wrap frame
captures {2,...,1198} (599) - missing the 0/1200 frame, so the video jumps
1198 -> 2 on loop. Capturing it yields {2,...,1198,0} (600), which plays
1198 -> 0 -> (loop) 2, perfectly seamless. A one-shot SATURATION stop
(master_oneshot: cur frozen at total, pixels identical to the previous frame)
did NOT wrap, so it is still DROPPED - that drop exists precisely to avoid a
tail duplicate. Label exports keep their own bounded-wrap accounting.

### Safety caps and the playhead-idle stop

- max_frames: checked AFTER CaptureFrame (exact-count semantics, see
  section 2); always wins over every other detector.
- HOLD safety cap: 5400 frames (90 s at 60fps). Only reached if a master
  neither wraps (loops_done stays 0) nor goes static (master_oneshot) and no
  max_frames was set - e.g. a one-shot the dance keeps ticking without ever
  wrapping its bounded playhead. Exists so an unbounded hold capture cannot
  run forever without falling back to a pixel "is it done" detector.
- Label safety cap: 3600 ticks (60 s at 60fps). A looping label wraps and
  label_done fires; but a label that resolves to a plain run-to-stop never
  wraps (its playhead freezes at the authored stop()), and the public mc API
  exposes no stop flag to tell the two apart - this is the explicit bound for
  that underivable case.
- Playhead-idle auto-stop: threshold 90 consecutive ticks in which NO afp
  playhead in the composition advanced (master layer cur, mc playhead, every
  named child's current_frame). Long enough that a brief authored hold does
  not false-fire; short enough not to waste disk and encode time on a long
  static tail. Skipped for label exports and when max_frames is set. This
  supersedes both a bit-identical-pixel counter and a draw-count heuristic
  (the draw-count heuristic never fires because afpu keeps emitting draw
  calls for a static final frame - the SDVX hantei case); playheads are the
  afp ground truth per the repo rule, and on hantei both signals fire on the
  same tick. The corner the pixel test covered that this one deliberately
  trades away: a clip whose playheads keep advancing over authored-static
  frames keeps capturing until a wrap or cap (captures MORE, never
  truncates).
- DDR safety cap: 18000 frames (5 min at 60fps); the loop detector normally
  fires long before.
- GUI progress is published every 4 frames.

## 6. SubmitOneFrame (shared submit path)

Order per frame: opaque-bg composite (if bg not transparent) -> crop (clamped
against the actual w/h so a stale crop cannot overflow; minimum 1x1) ->
optional reference dump -> blend-mode buffering OR sink open-on-first-frame +
SubmitFrame. It lives at Export scope (not file-local) because the DDR
delayed-submit path in export_ddr.cpp shares it. A sink error calls
FailSession; callers must check g_sess.active afterwards.

The reference dump (DumpBgraReference) writes a tight BGRA blob with a
12-byte header (magic "BGRA" + u32 width + u32 height) to
`<dump_dir>/frame_%06d.bgra`; tests/diff_bgra_vs_avif.py reads these against
the AVIF decode to measure per-channel codec error.

Blend mode buffers the post-crop RGBA frame instead of encoding (cap: 3000
frames, ~50 s at 60fps, bounding RAM at enc_w*enc_h*4 per frame; past the cap
buffering stops and the normal stop logic finalises what is in hand).

## 7. DDR loop detection (export_ddr.cpp)

Split out of export.cpp for the 1000-line limit and to isolate the
DDR-specific (partly pixel-based) logic. This is the ONE place the renderer
still pixel-compares to find a loop, and only as the FALLBACK; the modern
export path never uses this file and never pixel-compares.

Why DDR needs its own detector: the modern loop-end signals
(AfpManager::IsMasterComplete / ReadLayerPosition) read the null modern g_afp
table under DDR, and no single afp field gives the visible loop length - the
scene repeats when its slowest sub-clips (e.g. the drifting clouds) realign,
which is SHORTER than bg_root's own wrap modulus (afp_layer_get_info out[6])
and unrelated to the per-clip frame counts.

PRIMARY signal - afp authored loop: the
root clip's current_frame (afp_mc_get_param 0x1010) wraps back to its "loop"
label every authored cycle. When the frame at that wrap matches the loop
reference, the cut is exact (pixel-perfect for bg_0009 / bg_0028).
ddr_loop_label encoding: -2 = not yet queried, -1 = unavailable (fall back to
the content detector), >= 0 = the "loop" label frame. A wrap is recognised as
current_frame dropping by more than 4 below the previous read
(cf < prev_cf - 4). The wrap counts as a loop when the frame-0 diff is clean
(FrameDiffMad <= 2.5, constant kAfpCleanMad; a clean wrap measures ~0, a
drifting scene's wrap is far above - bg_0001 measures ~14) OR unconditionally
when the user selected a start label (a non-"loop" start such as "in" won't
pixel-match the "loop" wrap reference, but the current_frame wrap IS still
exactly one authored loop). Once a clean wrap is taken, ddr_afp_loop latches
true, which disables the content detector so the two cannot double-count the
same boundary. If the wrap's pixels differ (children drift, e.g. bg_0001),
the wrap flag is re-armed and the content detector finds the true visible
loop.

Pre-roll: before capturing, frames with current_frame below the "loop" label
are skipped (the one-shot intro). If already cycling (the usual case after
the user previewed), this passes through immediately. ddr_cf_start records
the loop reference phase.

END-ON-FREEZE (labels only): a non-looping DDR label (e.g. common_shutter's
"out") plays ONCE then STOPS - current_frame stalls and no wrap ever comes,
so loop_count would never be satisfied. Consecutive no-advance ticks are
counted (ddr_cf_static); at threshold 8 (kStaticEnd - larger than any brief
mid-animation authored hold, small enough to end promptly) the clip has
ended: submit the held frame and finish. A looping bg never stalls this long
(current_frame advances every tick), so this only fires for play-once labels.

FALLBACK content detector: FrameDiffMad = mean absolute difference of the G
channel between two same-size BGRA buffers, sampled every 16th pixel (cheap,
~57k samples at 1280x720, a robust proxy for visual difference). The
frame-vs-frame0 diff peaks mid-cycle and dips back at the visible loop.
State: loop_max_mad tracks the running peak; divergence is declared once the
peak exceeds 4.0; a minimum of 60 frames (kMinLoopFrames) must pass to ignore
the trivial start; the loop is detected when the HELD frame's diff was below
10 percent of the peak (deep = loop_max_mad * 0.10 - only a true return is
this low) and the current diff is rising again - i.e. the held frame was the
minimum (== the reference), one visible loop. afp playback is deterministic
per integer counter tick, so the return is a clean deep dip
(bg_0001 returns at frame 3607, diff ~0.4/255, vs > 7 mid-loop). On an
intermediate loop boundary (multi-loop capture) the detector state is fully
reset so the next return is found fresh.

Both paths share the ONE-FRAME-DELAYED SUBMIT: each frame is held one tick
before being submitted so that, at the loop point, the duplicate can be
dropped and exactly loop_count clean cycles are emitted. At the FINAL
boundary the held (last unique) frame is submitted and the current duplicate
dropped; at an intermediate boundary the current frame (the next loop's first
frame) is kept.

## 8. Blend ("smooth loop seam") compose

For backgrounds with no clean authored loop (e.g. the select_bg one-shots) -
an explicit user-opted-in pixel-based synthesis (the only sanctioned pixel
comparison outside the DDR fallback). The captured cycle is buffered rather
than streamed; at finish BlendComposeAndSubmit:

1. Trims a trailing run of bit-identical (frozen) frames - a one-shot that
   settled static would otherwise pollute the loop search and crossfade tail.
2. Ranks loop-length candidates by mean abs diff over a sparse RGBA sample
   (every 4th pixel, i.e. byte stride 16) against frame 0. Candidate range:
   L from min(N-1, max(30, K+1)) to N-1 with L+K <= N, where K =
   blend_frames; the L with the smallest diff wins. If no candidate fits,
   L = N and K is clamped to N - L.
3. Crossfades the first K output frames from the natural continuation
   (buf[L+i], the smooth successor of the last body frame) toward the real
   start (buf[i]) with a smoothstep ease (t = x*x*(3-2x), x sampled at
   i+0.5 over K), so the wrap dissolves instead of snapping.
4. Opens the sink and submits exactly L frames; frames_captured is set to L
   (the actual encoded count).

## 9. VideoEncoder internals (src/video_encoder.*)

Move-only RAII (Encoder) so Session structs that own one stay safe under
copy-assign / reset patterns. Consumes tight-packed BGRA, converts via
sws_scale (including optional downsample - useful for browser playback where
1080p60 software decode runs past real-time even on fast boxes), produces
output at user-chosen fps + quality, and tags colour metadata as BT.709
full-range sRGB (primaries BT709, TRC IEC61966-2-1 = sRGB, colorspace BT709,
range JPEG = 0..255; the sws context is fed the ITU709 coefficient table with
full-range flags on both sides). AvErr re-implements av_err2str because the C
compound-literal macro does not compile as C++.

### Format characteristics (why each exists)

- AVIF: AV1 dual-stream (colour + alpha auxiliary). Image format with the
  best browser <img> compatibility for transparency. Colour stream can go
  through NVENC (av1_nvenc) on capable GPUs - ~50x encode speedup over
  libaom-av1, with yuv420p chroma subsampling; the alpha stream stays on
  libaom (tiny bitstream, software is already instant). KNOWN to drop frames
  at high fps because (a) the dual AV1 streams can desync on rounded keyframe
  intervals and (b) HEIF's frame-duration timing is rounded to ~15 ms by some
  decoders. Best for static images; pick WebP_Anim or WebM for video.
- WebP_Anim: VP8 single stream, yuva420p, wrapped in animated WebP. Plays in
  <img> tags on every modern browser (Chrome/Edge/Firefox/Safari, no decoder
  bugs at high fps). ~2-3x larger files than AVIF/AV1 but much smoother
  playback for animated content. libwebp software only - there is no hardware
  path for VP8 (but WebP is very fast in software anyway).
- WebM_VP9: VP9 single stream, yuva420p (alpha packed into the same stream -
  no auxiliary-image dance). Native <video> transparency in
  Chrome/Edge/Firefox. No NVENC path (NVIDIA does not encode VP9), always
  libvpx software. Much smoother <video> playback for long clips than
  <img>-driven AVIF because <video> bypasses Blink's 10 ms animated-image
  frame-duration clamp.
- WebM_AV1: AV1 single stream in a WebM (matroska) container.
  NVENC-accelerated (same av1_nvenc path as AVIF's colour stream), plays as
  <video>. OPAQUE ONLY: ffmpeg's matroska muxer has no AV1-alpha
  BlockAdditional path (that is a VP8/VP9-only mechanism), and
  Chrome/Firefox have limited / inconsistent alpha support for AV1-in-WebM.
- MP4_H264: H.264 in MP4, yuv420p, OPAQUE ONLY (H.264 has no alpha). The most
  universally compatible output - <video> everywhere, every desktop player /
  editor. NVENC via h264_nvenc, which is present on far more GPUs than
  av1_nvenc (any NVENC-capable card, not just Ada+); falls back to libx264
  (also fast). Muxed +faststart for web streaming.
- MP4_HEVC_Alpha: HEVC in MP4, yuva420p, WITH transparency. The alpha plane
  is carried as an x265 auxiliary alpha layer (x265 built with ENABLE_ALPHA -
  the repo's x265 vcpkg overlay port - driven by ffmpeg's libx265 wrapper,
  which turns on x265's --alpha automatically once it sees an alpha-bearing
  pixel format). Tagged hvc1 (codec_tag AND stream codecpar codec_tag) so
  QuickTime / Safari accept it - hvc1, not hev1. This is the
  transparent-<video> path for WebKit, which does not play VP9/AV1 alpha.
  Software only (no NVENC alpha exists for HEVC). Needs ffmpeg >= 7.1 +
  x265 >= 4.0 built with ENABLE_ALPHA. +faststart.

### Muxer selection

AVIF -> "avif" muxer (HEIF-derived); WebP_Anim -> "webp" (libwebp's own
container); WebM_VP9 / WebM_AV1 -> "webm" (matroska subset matching Blink's
<video>-tag parser); MP4_H264 / MP4_HEVC_Alpha -> "mp4" (ISO BMFF).

### Quality mapping

One user quality int 0..100 (higher = better), remapped per codec via
`crf = hi - q * (hi - lo) / 100`, clamped to [lo, hi]:

| Encoder      | knob            | lo | hi | notes                                             |
|--------------|-----------------|----|----|---------------------------------------------------|
| libaom-av1   | crf             | 4  | 40 | b=0 (pure CQ), usage=good (inter-frame enabled)   |
| av1_nvenc    | qp (rc=constqp) | 10 | 40 | cq/CQP is the CRF equivalent; range roughly 0..51 |
| libvpx-vp9   | crf             | 4  | 50 | CRF range 0..63; b=0                              |
| h264_nvenc   | qp (rc=constqp) | 18 | 38 | qp range ~0..51                                   |
| libx264      | crf             | 16 | 34 | CRF 0 lossless..51 worst, ~23 visual default; q=60 -> ~23 |
| libx265      | crf via x265-params | 16 | 34 | no direct crf AVOption; same window as x264; q=60 -> ~23 |
| libwebp_anim | quality         | direct 0..100 | | no inversion or clamping needed        |

CQP (constqp) is chosen over VBR on the NVENC paths because it is simpler for
short clips.

### Per-encoder settings worth keeping

- libaom: cpu-used=4, row-mt=1, tile-columns/rows = log2 tiling by dimension
  (>= 1024 -> 2, >= 512 -> 1, else 0), threads=16, frame+slice threading,
  gop_size = keyint_min = fps (keyframe every second).
- av1_nvenc: pix_fmt FORCED to yuv420p (NVENC AV1 does not accept yuv444p;
  nv12 would also work but yuv420p keeps the sws pipeline simpler). preset
  p5, tune hq (NVENC presets p1 fastest .. p7 slowest/best; p4 balanced; p5
  or p6 is the sweet spot for short burst encodes where quality matters).
  GOP fix: NVENC AV1 rejects "gop_length <= num_b_frames + 1"; stills encode
  at fps=1 which made gop_size=1 and tripped that check ("Gop Length should
  be greater than number of B frames + 1"), so every still fell back to
  libaom. B-frames buy nothing for a single-frame still or short loops, so
  max_b_frames=0 and gop floored at 2 - keeps the GPU path for stills too.
- libwebp_anim: compression_level=4 (0 fastest .. 6 best; 4 is libwebp's
  default). loop=0 (infinite) set explicitly on the ENCODER so future libwebp
  updates cannot surprise-break it - AND on the MUXER: the webp muxer has its
  own `loop` option defaulting to 1 (play once) that OVERRIDES whatever the
  encoder wrote into the bitstream; without the muxer option
  Chrome/Firefox/Safari all see the muxer's "1" in the WebP ANIM chunk and
  play the file once. Setting BOTH layers is the only way to get infinite
  browser playback.
- libvpx-vp9: deadline=good, cpu-used=2 (NOTE: libvpx's cpu-used is inverted
  vs libaom - lower is faster for libvpx), row-mt=1, tile-columns=2,
  gop = fps.
- h264_nvenc: preset p5, tune hq, profile high (8-bit 4:2:0), gop = fps.
- libx264: preset medium, profile high, gop = fps.
- libx265: preset medium, x265-params "crf=N:log-level=none" (log-level=none
  silences x265's per-run banner on stderr), gop = fps.

### Container/stream plumbing

- Stream time_base is {1, fps*1000} on every path for jitter-free frame
  pacing; avg/r_frame_rate = {fps, 1}; frame PTS = the 0-based capture index.
- MP4 formats (H264 and HEVC_Alpha) round output dimensions DOWN to even
  (w &= ~1) because yuv420p H.264/HEVC requires even dimensions - so an odd
  crop cannot hard-fail libx264 / h264_nvenc.
- AVIF path: dual streams; alpha is extracted by copying byte 3 of each BGRA
  pixel into a source-sized gray8 buffer, then sws-scaled (gray8 -> gray8) to
  output size and encoded by a second libaom instance.
- NVENC fallback: for AVIF colour, WebM_AV1 and MP4_H264, NVENC is tried
  first when prefer_hardware is set; on open failure (driver / ffmpeg build
  missing NVENC, GPU too old) the software encoder (libaom / libx264) is used
  and the reason logged. MP4_H264's "no encoder at all" error strings guide
  the user to enable HW accel or rebuild ffmpeg with x264/openh264.

### HardwareAvailable probe

Probes a named NVENC encoder by actually OPENING it at a 256x256 yuv420p test
config: present in the ffmpeg build AND a compatible GPU/driver opens it.
256x256 clears every current card's minimum frame size (NVENC AV1 needs
128x128 on Ada; H.264 NVENC's minimum is smaller still). ffmpeg logging is
silenced (AV_LOG_QUIET) during the probe - the return code is the source of
truth - so a failed probe does not spam the console. Results are cached in
function-local statics: NVENC init costs ~hundreds of ms (spins up a CUDA
context + NVENC session), the verdict cannot change without an app restart
(GPU/driver are not hot-swapped), and static-init is thread-safe since C++11
(N2660). MP4_H264 queries the h264_nvenc probe; the AV1 formats (and the
default) query av1_nvenc. Formats with no hardware path (WebP/VP9) still
report based on the default probe - the GUI gates those out separately via
format_can_use_hw.

## 10. MediaSink::Sink internals (src/media_sink.*)

The unified "give me frames, I'll write them to disk" surface the Export
panel feeds BGRA into; hides video-style outputs (ffmpeg-driven) vs image
sequences (per-frame PNG into a folder) behind one Sink so callers never
branch on format. Adding a new output format is a one-line enum change plus a
branch in the switch tables; CLI, dropdowns and the export pipeline pick it
up for free. The Sink mirrors the original VideoEncoder::Encoder surface
(Open/SubmitFrame/Finish/Cancel) so call sites could switch with no shape
changes - only the type name and Open() (was Create()). Params parallels
VideoEncoder::Params: out dims 0 = no scale; video sinks downscale via
sws_scale, image-sequence sinks IGNORE out dims and always write
src dimensions; quality is ignored by PNG_Sequence; output_path may be a
single file or a directory depending on WritesDirectory(format) (see
docs/media_formats.md).

Impl holds exactly one of a VideoEncoder::Encoder (video) or a PngSeq (image
sequence), chosen at Open by WritesDirectory.

PNG-sequence backend (WIC):

- One PNG per captured frame, frame_NNNNNN.png (6-digit zero-pad so a
  directory listing sorts in capture order). Zlib compression is baked into
  WIC's PNG codec - no extra config. Pixel format 32bppBGRA.
- CoInitializeEx is called once at session start and released at session end,
  NOT per frame - per-frame COM init would pay a noticeable cost on 600-frame
  captures. (COM init is reference-counted.)
- DeleteFrames best-effort deletes every frame_*.png on Cancel (and would on
  a re-Open of an existing directory) so a previous failed run does not leave
  stale frames mixed in; Cancel also removes the directory itself.
- PNG write errors are individually non-fatal but the first is propagated;
  the caller decides whether to bail or retry (the Export panel bails on the
  first error).

Lifetime rules:

- The destructor guards a null impl_: after a move, unique_ptr<Impl> leaves
  the source pointer-null and Cancel() would deref it, so both the dtor and
  every method null-check.
- Open re-attaches a fresh Impl if the Sink was moved-from, so a
  session-owning struct can safely do `sink = {}; sink.Open(...)` between
  runs. Double-Open is an error.
- On video Cancel, the VideoEncoder owns the partial file's lifetime;
  Export's RemovePartialOutput still runs at the caller's discretion, so the
  sink does not double-delete.
- SubmitFrame's frame_index is the 0-based capture index (PNG filename + the
  video encoder's PTS). After Finish(), SubmitFrame is invalid.
- UsingHardware: PNG always reports false; video forwards
  VideoEncoder::UsingHardware (true only while opened).

## Appendix: native folder dialog (src/native_dialog.*)

Small Win32 wrapper kept here so its rationale is not lost:

- Uses IFileOpenDialog (the modern COM picker) instead of legacy
  SHBrowseForFolder: Explorer-style UI and clean long/unicode path handling,
  which matters for deep game install layouts.
- Callable from any thread; CoInitializeEx per call with
  COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE - the dialog thread must
  be STA because the shell COM objects are not thread-safe.
  RPC_E_CHANGED_MODE is treated as "already initialised" success.
- FOS_PICKFOLDERS turns the file dialog into a folder picker;
  FOS_FORCEFILESYSTEM keeps the user from picking shell-virtual locations
  (Recycle Bin, Libraries, This PC) that cannot be converted to a real
  filesystem path.
- The initial folder is pre-selected via SHCreateItemFromParsingName, which
  validates existence - if the saved path was deleted the dialog silently
  falls back to the OS default starting location ("This PC").
- Show() blocks until pick/cancel but pumps messages for the parent window,
  so the GUI stays responsive under the modal dialog. `parent` should be the
  triggering window's HWND so the dialog modally centres over it; nullptr
  parents to the desktop.
- Strings: the codebase stays UTF-8 std::string throughout; conversion to/
  from UTF-16 happens exactly at the API boundary. IFileOpenDialog returns
  wide paths in CoTaskMemAlloc buffers, converted and freed immediately.
  Returns empty string on cancel or error.
