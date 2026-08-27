# r573_settings (src/settings/)

Persistent user preferences: a `settings.ini` of `key=value` lines next to
`renderer.exe` (the renderer is a portable tool, so config lives next to the
binary the user launched, never %APPDATA%; `DefaultIniPath` resolves via
`GetModuleFileNameA` so the working directory is irrelevant). Missing,
unreadable, or malformed files always degrade to a default `Config` - first
run is "empty input", never an error. Not thread-safe; main thread only.

`LoadFrom` / `SaveAtomicTo` take an explicit ini path, which is what the
unit tests inject; `Load` / `SaveAtomic` are the production conveniences
bound to `DefaultIniPath`.

## Atomicity

Saves write a sibling `.tmp` then rename it over the real file
(`std::filesystem::rename`, which on Windows is `MoveFileExW` +
`MOVEFILE_REPLACE_EXISTING`). A crash mid-write leaves the old file intact -
a reader sees either the old values or the new ones, never a torn file. The
pre-refactor code additionally passed `MOVEFILE_WRITE_THROUGH` (forced
metadata flush); `std::filesystem::rename` does not, which slightly widens
the crash window without affecting the never-torn guarantee - accepted in
exchange for dropping the direct Win32 dependency from the save path.

## Keys

| Key | Meaning |
|---|---|
| `game_dir` | Last game directory picked; pre-fills the Setup screen. |
| `loop_master` | "Loop master animation" toggle. A preference (survives IFS swaps and restarts), not per-file state. Accepts `0/false/no` as off; any other value (including empty) is on, since the key's presence implies intent. |
| `root_loop` | `hold` or `force` (also the usual bool spellings; anything not an explicit force token stays hold so older INIs keep the game default). See below. |
| `render_width/height` | Native game resolution (SDVX portrait 1080x1920 default; IIDX 25+/DDR A 1920x1080; IIDX 17-24 1280x720; jubeat 1024x768). Guarded: non-positive or garbage values keep defaults so a hand-edited `render_width=foo` never propagates a 0x0 window into D3D9 init. |
| `render_fps` | Live render / animation-tick rate; dt = 1/fps keeps animation speed real-time at any rate. Default 120 (the renderer's historical internal tick). |
| `game_profile` | GameProfile slug; empty = auto-detect from the game directory at boot. The profile registry is the source of truth for valid slugs. |
| `master_scale` | Uniform master-stream scale applied via `afp_stream_set_matrix` on stream (re)creation. Previews SDVX's 1.5x upscale of older 720x1280 IFSes; the field of truth for that ratio is the BG entry's payload+28 in soundvoltex.dll (see SDVX system-BG notes in the parent RE repo). Guarded > 0. |

Unknown keys are silently ignored so older binaries tolerate newer files.

## root_loop: Hold vs Force

Stored as a plain bool so Settings stays free of any app_state dependency
(the enum translation lives at the consumers). `hold` (default,
game-faithful): mount once and let the continuous-loop dance keep the
master CLOCK running so the root's own tick loops it via shallow-seek,
REUSING persistent nested children while they free-run. `force`: re-drive
the root via ForceReplay + the dance, for one-shot masters (bg_common) or
explicit user loops. History: the earlier "Hold froze / still snapped"
symptom was two distinct bugs - (1) Hold did not apply the dance so
the master clock stopped at end-of-timeline and the whole scene froze;
(2) the loop_master ForceReplay arm was not gated on Hold, so a full
remount reset every nested child to frame 0 (the o_kazari5 snap). Now
Hold = dance + NO ForceReplay, verified by root-phase-aligned frames
differing across cycles.

## App::SaveCurrentSettings

The single canonical "persist everything now" entry point, declared in
`app_state.h` and defined in `app_state.cpp` (it reads App::Global; the
pre-refactor layout declared it in settings.h, which inverted the layering).
Every widget callback that mutates a persisted preference calls it. It
snapshots the FULL config on purpose: the original per-callsite saves had a
bug where the loop-master toggle wrote only `game_dir + loop_master`,
silently clobbering `render_width`, `render_height`, and `game_profile` on
disk every time it was clicked. One helper that rereads the entire state
makes that class of bug impossible, and new persisted fields only need
app_state + this helper, not every call site.

## 16:9 stretch (`stretch_16_9`, `stretch_filter`)

A 4:3 game on a 16:9 cabinet monitor is displayed stretched, not pillarboxed.
`stretch_16_9` reproduces that: the render stays at the 4:3 size the game asks
for and only the PRESENTED picture is widened.

`Stretch::Present` (`src/render/stretch.h`) derives the presented size: it keeps
the height and widens to 16:9, rounding the width UP TO AN EVEN NUMBER because
yuv420p video encoding rejects odd dimensions. 640x480 becomes 854x480. The
option only applies to landscape 4:3 (`w * 3 == h * 4`), so 1280x720, 1080x1920
and the 520x704 qpro size are untouched and the setup screen hides the control
for them.

The pipeline is unchanged up to present time:

- `offscreen_rt` stays at the render size, so everything the game draws and the
  asset-extraction readbacks still work in native pixels;
- a `present_rt` at the widened size exists only while stretching, and
  `EndFrame` does `offscreen -> present` with the chosen filter, then a plain
  copy `present -> backbuffer`;
- the swap chain and the render window are created at the presented size, so the
  preview window itself is 16:9.

Exports read `ReadPresentBGRA`, which returns `present_rt` when stretching and
falls back to the offscreen when not, so a capture is exactly the picture on
screen.

**The crop rect therefore lives in PRESENTED pixels, not render pixels.** It is
applied to the frame `ReadPresentBGRA` hands back, so every other place that
touches it has to agree: `AppWindow::SetRenderRtSize` is fed
`D3D9State::GetPresentSize`, `Stretch::ClientToFrame` maps a click in the window
onto that frame, `Stretch::FrameToTarget` maps the rect back onto the backbuffer
for the on-screen overlay, and the export modal's output-resolution row derives
its native size from `Stretch::Present` as well. Mapping picks into the RENDER
size instead is what made a stretched 4:3 crop land on the wrong region: a click
at the right edge of an 854-wide window resolved to x=640, so the exported crop
was shifted left and about 75% of the intended width. The DDR loop detector shares that readback for the same reason: what it
compares is what it encodes. `ReadOffscreenBGRA` stays native-resolution and is
still what qpro extraction and the DDR test harness use.

`stretch_filter` indexes `Stretch::Filter` (0 Nearest, 1 Linear, 2 Gaussian,
3 Pyramidal) and maps to the `D3DTEXTUREFILTERTYPE` handed to `StretchRect`.
Nearest and Linear are always available; Gaussian and Pyramidal are offered only
when `D3DCAPS9::StretchRectFilterCaps` reports them, and the setup combo greys
out the rest with a tooltip saying why. The index is parsed with a bounded
non-negative check rather than the positive-only helper the other integers use,
because 0 is a legal value (Nearest).
