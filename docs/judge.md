# Interpolation judge (SDVX 1)

A window for judging clips by eye, one at a time. Each clip is drawn twice side by
side: at the authored rate, and at the display's refresh rate with poses
interpolated between consecutive authored timeline frames. Every clip of every TXP2
package in `data/graphics` gets a verdict (Smooth, Wrong or Unsure) and an optional
note, saved as soon as it is given.

```
bin\573Renderer32.exe --judge "<sdvx1-dir>"
```

It is a tool command (docs/cli.md, "Tool commands"), so it runs before the normal
boot, opens its own window and exits when the window closes. SDVX 1 ships 32-bit
DLLs, so it runs in `573Renderer32.exe` (docs/x86_build.md).

| flag | value | what it does |
|---|---|---|
| `--judge` | `<game-dir>` | the install to judge |
| `--profile` | `<slug>` | the game profile; auto-detected from the folder name when omitted. Only a profile whose engine config carries an interpolation layout can be judged (today `sdvx1`) |
| `--judge-verdicts` | `<file.json>` | where verdicts are kept. Default: `judge_verdicts_<slug>.json` next to the executable |
| `--judge-clip` | `<package>/<clip>` | the clip to open first, for example `bg_space.bin/bg_space`. Default: the first clip without a verdict |
| `--judge-capture` | `<dir>` | verification mode: writes `NNNN_authored.png` and `NNNN_interpolated.png` for every presented frame (and `NNNN_window.png` for the first and the last), then exits. Vsync is off in this mode |
| `--judge-capture-frames` | `<N>` | how many frames `--judge-capture` writes |
| `--judge-display-hz` | `<hz>` | drive the display clock at a fixed rate instead of the real one, so a capture is the same on every machine |
| `--judge-background` | `black`, `grey`, `white`, `red`, `green`, `blue` or `checker` | the backdrop both sides are drawn over (default `black`, the colour the stage was cleared to before this option existed); the Background control and the B key change it live |

## Background

Both stages are cleared to the chosen backdrop before the clip draws (`Judge::FillFor`,
`src/judge/judge_backdrop.cpp`, drawn by `JudgeDevice::BeginStage`). The backdrop is one opaque
colour, or for `checker` a light (`#FFFFFF`) base with darker (`#CCCCCC`) 16-pixel squares in stage
pixels, filled with D3D9 `Clear` rectangles so no shader is involved. The backdrop is opaque and the
clip's blend keeps an opaque target opaque, so the window behind the stage never shows through.
Partly transparent content shows the backdrop through it. A clip that fades in over its own black
layer darkens the backdrop along with it; that darkening comes from the clip
(`logo.bin/beatnation` does this).

## The time model

Logic and display are separate, the same way the game hook runs the game.

- Logic advances in the game's own fixed step, `AfpConfig::fixed_step_seconds`
  (1/60 s for SDVX 1, the constant `dt` its frame loop hands to `afp_render`, see the
  notes on SDVX 1's frame pacing). One step is the call the game makes per frame,
  libafp's `afp_render(step)` export (`DdrAfp::Advance`; for a build without that
  export it runs the export's body: `afp_render_init`, `afp_render_all(step)`,
  `afp_render_finish`). Both layers live in the same afp, so the one call advances
  them identically.
- Display presents with vsync on, so the window runs at the monitor's refresh.
  Each presented frame adds the real elapsed time, times the speed factor, to
  `Judge::Clock`. Whole steps run the advance, and the remainder is the "time since
  the last advance" that each draw uses. A frame longer than four steps (a hitch, a
  window drag) runs four steps and drops the rest instead of fast-forwarding.
- Slow motion scales only the logic clock (1x, 0.5x, 0.25x, 0.1x, 0.05x). The
  window keeps presenting at the display rate, so the interpolated side keeps
  showing in-between poses while the authored side holds each frame longer.
- Pause stops the clock and keeps the remainder, so the interpolated side
  freezes on the in-between pose it was showing. Step pauses and moves time by one render at the
  game hook's default rate, half a logic step (1/120 s for SDVX 1, `Judge::kRendersPerLogicStep`).
  The authored side changes on every other Step and the interpolated side on every Step, so each
  in-between pose can be inspected on its own. Restart destroys and recreates both layers. With
  Loop off, playback pauses on the root clip's last frame, which is read from afp
  (`afp_mc_get_param` codes 0x1010 / 0x1011 on the root MC), never from pixels.

The two sides are two afp layers of the same stream, created and played back to
back. Interpolation is enabled per layer, because the hooks act only on the layer ids
registered with `AfpInterpHooks::SetLayerInterpolated`, so the left layer is drawn
exactly as the game draws it and the right one gets the substituted poses. Each
side is drawn into its own 720x1280 render target (the stage size of the profile)
and shown in the window scaled to fit.

## The interpolation mechanism

The detours are in `src/judge_hooks/afp_interp_hooks.cpp`, on top of
`src/judge/interp_tracks.cpp` and `src/judge/afp_pose.cpp` (pure, unit tested). The
mechanism is the game hook's, applied to the same `libafp-win32.dll` loaded in this
process. The RE behind it
(libafp 2.10.3's layer and display-object structs, the advance and draw paths,
what breaks object identity) is in the notes on SDVX 1's libafp internals.

Five functions are detoured, all `cdecl`, found by the assert strings libafp keeps:

| role | anchor | what the detour does |
|---|---|---|
| per-layer advance | `"afp_advance_play_data"`, log `"render %s"` | snapshots every live object before and after the call, counts the frame steps the call took, and updates the layer's tracks |
| per-layer draw | `"afp_draw_frame_play_data"`, log `"display %s"`; the only caller of it is the display core | writes `lerp(from, to, alpha)` into each tracked object, calls the original, writes the live values back |
| display-object constructor | memsets 188 bytes; 16 callers | forgets the track of that address in every layer (and its pre-advance snapshot) |
| goto | `"AfpClip_GotoFrame"` role: every goto path ends here | reads the clip's current frame (+140) before the call; during an advance, marks the clip as jumped unless the target is exactly the next frame (`LayerTracks::MarkGoto`) |
| direct frame set | the frame write that applies only one frame's tags | same as goto |

Per object the pose is the 2D local matrix, the anchor, the 3D local matrix when the
object has one, the colour transform and the head camera node. `alpha` is
`(layer accumulator + time since the advance * |speed|) * layer fps`, clamped to
[0, 1]. That is the usual fixed-step blend, with one authored frame of latency.

A track interpolates from the pose before an advance to the pose after it only when
that advance took exactly one frame step, the object existed in both snapshots,
nothing about its identity changed (character, depth, 2D or 3D, camera present), it
is not under script or API control (flags A bit `0x40000`) and its parent clip did
not jump. An advance with no frame step keeps the pair (30 fps content takes a step
on every second 1/60 advance). In every other case the object snaps. At draw time, a
live pose that differs from the authored pose the track ends on (because the game or
a script moved it after the advance) is drawn as it is.

Each sample also carries the object's created frame (+142) and lifetime end
(+144). `AfpInterpHooks::SetAdvanceObserver` hands every interpolated layer's
before and after samples and the frame-step count to a callback after each
advance. The re-placement survey below uses it.

`AfpInterp::LayerTracks` keeps one track map per interpolated layer and drops the
tracks of objects that are no longer in the tree after an advance.

Three behaviours differ from the first version of the game hook and are covered by
unit tests:

- The constructor detour also forgets the object's pre-advance snapshot. Without
  that, an object freed and constructed again at the same address inside one
  advance (same character, same depth: a remove and a place again) blended from the
  old instance's pose.
- When one end of a decomposed matrix blend has a zero x scale, its rotation and
  skew are undefined (`atan2(0, 0)`), so the other end's angles are used. Without
  that, a clip that scales up from zero while rotated spins in from angle 0.
- An advance with no frame step keeps a pair only for objects that are not under
  script control.

### Objects that are replaced instead of moved

Much SDVX 1 content changes from one authored frame to the next by removing an
object and placing a new one, so after an advance some objects are gone and some
are new. A new object has no pose before the advance, so it is drawn as it is.
Pairing a new object with a removed one would be right only where the content means
the same object, and the survey below shows that case does not occur.

What libafp does (libafp 2.10.3, by the assert strings):

- The frame-tag walk (`"afp_set_play_data_frame_core"`) places a record (tag 127)
  only when its lifetime end is greater than the target frame, and passes
  `(clip, record, frame, lifetime)` on. The lifetime is the record's end frame, or
  the separate lifetime record when there is one (`"no lifetime data
  level=%d depth=%d flag=%02x id=%d"` when it is missing).
- The place function (`"ap2_set_display_object 3"`) writes `frame` to the new
  object's created frame (+142) and `lifetime` to its lifetime end (+144), u16
  each. A record without the move flag at a depth that is still occupied by an
  object created on an earlier frame is ignored.
- The remove tag (128, `"afp_remove_display_object"`) removes the object at its
  depth only when the object was created before the current frame; it moves it to
  depth 12288 and marks it dead, and the free happens two advances later.
- Nothing else in the draw or the advance reads the lifetime end. The only
  reader of +144 is `afp_pw_get_life_frame` (`"afp-access.c"`), which returns
  `lifetime - created`. Objects are removed only by the remove tag.

A "continuation by re-placement" would therefore be a removed object and a new
object with the same parent, the same depth and the same character, where the
removed object's lifetime end equals the new object's created frame (one span ends
on the frame the next one starts). `judge_succession_survey_tests` (`local_dll`,
`R573_SDVX1_DIR`) runs every one of the 1323 clips for 240 advances through the
real libafp with the hooks and sorts every new object an advance produced:

| what the new object is | objects | clips |
|---|---|---|
| same parent, depth and character, the removed one's span ends where it starts | 0 | 0 |
| same parent, depth and character, spans not contiguous (a clip that looped and replayed its creation frame) | 960 | 64 |
| same parent and character, other depth, spans contiguous | 7336 | 282 |
| same parent and character, other depth, spans not contiguous | 1076 | 140 |
| other character, starting on the frame a removed sibling's span ended (a flipbook: one drawing hands over to the next) | 36627 | 413 |
| child of an object created in the same advance (the contents of a new clip instance) | 47962 | 311 |
| no removed sibling at all | 29752 | 713 |

There were 1725357 pose changes of persisting objects over the same advances, and
no new object came from an advance that was not a single frame step.

The same-slot continuation does not occur in this content, so `LayerTracks` pairs
nothing. The two large groups are not continuations either:

- Handovers between characters are flipbooks: the gameplay tunnel clip `bg_all` of
  the `gmbg_*` packages replaces the drawing at depth 17 with a different shape at
  depth 16 two frames later, then 15, then 14, each at the same position. The
  objects are different drawings, so there is no pose to blend between them. 64
  clips change in this way and no other.
- Same-character handovers at another depth come from rows of copies: in
  `appeal_select.bin` a row of 25 identical tiles at depths 2 to 26, all ending on
  frame 120, is replaced by a new row at depths 27 onward starting on frame 120.
  Nothing records which old tile continues which new one. Only geometry (the
  tile at x 629.8 and the one at x 630.0) suggests it, and the engine never
  guesses from positions. These snap once per span boundary.

### A goto to the next frame is a step

A goto that moves a clip from frame `f` to `f + 1` replays nothing. The goto
function walks the tags of the frames strictly between the two with `apply = 0`
(there are none) and then frame `f + 1` with `apply = 1`. That is the same
`ApplyFrameTags(clip, f + 1, f + 1, 1)` that the inline forward step of the per-clip
advance runs. A deep goto then sends every child with a timeline to
`(clip frame - child created) % child frame count`, which is again the next frame
for a child that was playing in step. So `MarkGoto(clip, from, to)` marks a jump
only when `to != from + 1`. A wrap to frame 0, a skip of two or more frames, a goto
backward and a goto to the same frame all still snap the clip's children.

The SDVX 1 title screen relies on this. Its `title` layer (`title.bin`, clip `title`)
is paused, and the game moves it every logic frame with `afp_mc_op(root, 0xF08,
frame)` (deep goto and play), usually one frame further each time, looping 74 to
264. `judge_title_study_tests` (`local_dll`, `R573_SDVX1_DIR`) plays the clip
normally and then drives it the game's way. Around frame 104 a one-frame deep goto
keeps 116 objects, of which 40 change pose, removes 13 and creates 22; the next
two frames keep 138 / 132 with 44 / 43 changing. Treated as a step, 40 to 44
objects blend per frame. If every clip the deep goto visits were marked as jumped,
none would. None of the new objects continues a removed one in the same slot (same
parent, depth and character). The ones that look like continuations (for example
a clip at depth 2 with span 0..30 handing over to the same character at depth 3
with span 30..60 at the same position) are span handovers at another depth,
covered above. In the judge itself the clip advances by plain playback, and the
interpolated side changes on every display frame while the authored side holds
each frame for two.

### The profile carries the layout

`AfpConfig::interp_layout` points at `kLibafp2103Interp` in
`src/backend/afp_profiles.cpp`: the five hook sites (module-relative address, the
bytes of the prologue the detour overwrites) and the field offsets the hooks read.
Before installing anything, `AfpInterp::PrologueMismatch` compares every prologue
with the loaded module. If the bytes differ, the hooks are not installed, the
window says "Interpolation is off: the prologue of <function> does not match this
profile's libafp build" in red, and the right side shows the clip without
interpolation. The detours are made with safetyhook (vcpkg port `safetyhook`).
They are compiled as a separate C++23 library, `r573_judge_hooks`, because the
library's headers use `std::expected`; the rest of the renderer stays C++20.

## Clips and verdicts

The package list holds every `.bin` under `data/graphics` (subfolders included)
that is a TXP2 package with at least one afp stream. It is read on a background
thread, showing `N / M packages` and the current file. Each package's clip names
come from its afp stream table; only the header and the core are read, not the
textures (`Judge::ReadClipNames`). A clip is keyed by `<package>/<clip>`, where the
package path is relative to `data/graphics` with forward slashes, so the keys stay
valid when the order of the list changes.

The verdict file:

```json
{
  "verdicts": {
    "bg_space.bin/bg_space": {
      "matrix_lerp": "decompose",
      "note": "ring jitters at the loop point",
      "verdict": "wrong"
    }
  },
  "version": 1
}
```

`verdict` is `smooth`, `wrong` or `unsure` (absent while only a note was written),
and `matrix_lerp` is the mode that was active when the verdict was given. Every
change is written at once, to `<file>.tmp`, which is then renamed over the file. A
file that does not parse, or holds an unknown verdict or mode, is refused: the
window says so and saves nothing, so a damaged file is never overwritten.

## Controls

| input | does |
|---|---|
| Space | play / pause |
| Right or `.` | pause and step one render (half a logic frame) |
| R | restart the clip |
| L | loop on / off |
| `[` / `]` | slower / faster |
| M | matrix lerp mode decompose / linear |
| B | next background (black, grey, white, red, green, blue, checker) |
| Up, K / Down, J | previous / next clip |
| N | next clip without a verdict |
| 1 / 2 / 3 | Smooth / Wrong / Unsure |

Keys do nothing while the note field has focus. With "Next unjudged clip after a
verdict" on (the default), giving a verdict moves to the next clip that has none,
so the fast way through is to type a note if there is one and then press the
verdict key. The matrix lerp mode applies to the right side at once: decompose
blends scale, rotation and skew the way the engine decomposes a matrix
(`AfpObj_SetMatrix`), and linear blends the six numbers.

## Tests

- `judge_tests` (`ci`): the pose math, the track rules, frame steps and alpha, the
  prologue check, the verdict file, the clip catalog and the clock.
- `gui_tests` `[judge]` (`ci`): the progress line and verdict badges, and the
  buttons, keys, rows and note reaching the judge; `contrast_tests` covers the
  verdict colours.
- `judge_succession_survey_tests` (`local_dll`, 32-bit, `R573_SDVX1_DIR`): the
  re-placement survey above, printed as `[replace]` lines.
- `judge_title_study_tests` (`local_dll`, 32-bit, `R573_SDVX1_DIR`): the title
  screen's one-frame deep goto, printed as `[title]` lines.
- `judge_local_tests` (`local_dll`, 32-bit, `R573_SDVX1_DIR`): boots SDVX 1's own
  libafp with the hooks, opens `bg_space.bin` / `bg_space`, and over ten logic steps
  requires the authored side to be identical at 0 and at half a step after the
  advance while the interpolated side differs at every one.
