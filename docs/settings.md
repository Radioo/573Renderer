# r573_settings (src/settings/)

Persistent user preferences, stored as a `settings.ini` of `key=value` lines
next to `renderer.exe`. The renderer is a portable tool, so its config lives
next to the binary the user launched and never in %APPDATA%. `DefaultIniPath`
resolves the path through `GetModuleFileNameA`, so the working directory does
not matter. A missing, unreadable or malformed file always falls back to a
default `Config`: the first run is treated as empty input, not as an error.
The module is not thread-safe and runs on the main thread only.

`LoadFrom` / `SaveAtomicTo` take an explicit ini path, which is how the unit
tests inject one; `Load` / `SaveAtomic` are the production versions bound to
`DefaultIniPath`.

## Atomicity

A save writes a sibling `.tmp` file and renames it over the real one
(`std::filesystem::rename`, which on Windows is `MoveFileExW` +
`MOVEFILE_REPLACE_EXISTING`). A crash mid-write leaves the old file intact, so
a reader sees either the old values or the new ones and never a torn file. The
pre-refactor code also passed `MOVEFILE_WRITE_THROUGH` (a forced metadata
flush). `std::filesystem::rename` does not, which slightly widens the crash
window but keeps the no-torn-file guarantee. We accepted that in exchange for
removing the direct Win32 dependency from the save path.

## Keys

| Key | Meaning |
|---|---|
| `game_dir` | Last game directory picked; pre-fills the Setup screen. |
| `loop_master` | "Loop master animation" toggle. It is a preference that survives IFS swaps and restarts, not per-file state. `0/false/no` mean off; any other value (including empty) means on, since the key's presence implies intent. |
| `root_loop` | `hold` or `force` (the usual bool spellings work too). Anything that is not an explicit force token stays hold, so older INIs keep the game default. See below. |
| `render_width/height` | Native game resolution (SDVX portrait 1080x1920 default; IIDX 25+/DDR A 1920x1080; IIDX 17-24 1280x720; jubeat 1024x768). Non-positive or garbage values keep the defaults, so a hand-edited `render_width=foo` can never pass a 0x0 window to D3D9 init. |
| `render_fps` | Live render and animation-tick rate. dt = 1/fps, so animation runs in real time at any rate. Default 120 (the renderer's historical internal tick). |
| `game_profile` | GameProfile slug; empty means auto-detect from the game directory at boot. The profile registry is the source of truth for valid slugs. |
| `master_scale` | Uniform master-stream scale, applied through `afp_stream_set_matrix` whenever the stream is created or recreated. It previews SDVX's 1.5x upscale of older 720x1280 IFSes; soundvoltex.dll stores the real ratio at the BG entry's payload+28 (see the SDVX system-BG notes in the parent RE repo). Must be > 0. |

Unknown keys are ignored without a warning, so older binaries can read newer files.

## root_loop: Hold vs Force

The setting is stored as a plain bool so Settings has no dependency on
app_state; the consumers translate it to the enum. `hold` is the default and
matches the game: the root is mounted once, and the continuous-loop dance keeps
the master clock running so the root's own tick loops it with a shallow seek,
reusing the persistent nested children while they free-run. `force` re-drives
the root with ForceReplay plus the dance, for one-shot masters (bg_common) or
loops the user asked for. The earlier "Hold froze / still snapped" symptom was
two separate bugs. First, Hold did not apply the dance, so the master clock
stopped at the end of the timeline and the whole scene froze. Second, the
loop_master ForceReplay branch did not check for Hold, so a full remount reset
every nested child to frame 0 (the o_kazari5 snap). Hold now means the dance
without ForceReplay, verified by frames at the same root phase differing
across cycles.

## App::SaveCurrentSettings

This is the one entry point that persists everything. It is declared in
`app_state.h` and defined in `app_state.cpp` because it reads App::Global (the
pre-refactor layout declared it in settings.h, which inverted the layering).
Every widget callback that changes a persisted preference calls it. It saves
the whole config on purpose. With the original per-callsite saves, the
loop-master toggle wrote only `game_dir + loop_master` and overwrote
`render_width`, `render_height` and `game_profile` on disk every time it was
clicked. A single helper that rereads the entire state rules out that kind of
bug, and a new persisted field only needs changes in app_state and this
helper instead of every call site.

## 16:9 stretch (`stretch_16_9`, `stretch_filter`)

A 4:3 game on a 16:9 cabinet monitor is shown stretched, without pillarboxes.
`stretch_16_9` reproduces that: the render stays at the 4:3 size the game asks
for, and only the presented picture is widened.

`Stretch::Present` (`src/render/stretch.h`) works out the presented size. It
keeps the height and widens to 16:9, rounding the width up to an even number
because yuv420p video encoding rejects odd dimensions. 640x480 becomes 854x480.
The option only applies to landscape 4:3 (`w * 3 == h * 4`), so 1280x720,
1080x1920 and the 520x704 qpro size are untouched, and the setup screen hides
the control for them.

Up to present time the pipeline does not change:

- `offscreen_rt` stays at the render size, so everything the game draws and the
  asset-extraction readbacks still work in native pixels;
- a `present_rt` at the widened size exists only while stretching, and
  `EndFrame` does `offscreen -> present` with the chosen filter, then a plain
  copy `present -> backbuffer`;
- the swap chain and the render window are created at the presented size, so the
  preview window itself is 16:9.

Exports read `ReadPresentBGRA`, which returns `present_rt` when stretching and
the offscreen target otherwise, so a capture is exactly the picture on screen.

That is why the crop rect is in presented pixels, not render pixels. It is
applied to the frame `ReadPresentBGRA` returns, so every other place that
touches it has to use the same space: `AppWindow::SetRenderRtSize` is fed
`D3D9State::GetPresentSize`, `Stretch::ClientToFrame` maps a click in the window
onto that frame, `Stretch::FrameToTarget` maps the rect back onto the backbuffer
for the on-screen overlay, and the export modal's output-resolution row takes
its native size from `Stretch::Present` too. Mapping clicks into the render
size made a stretched 4:3 crop land on the wrong region: a click at the right
edge of an 854-wide window resolved to x=640, so the exported crop was shifted
left and about 75% of the intended width. The DDR loop detector uses the same
readback for the same reason, so what it compares is what it encodes.
`ReadOffscreenBGRA` stays at native resolution, and qpro extraction and the DDR
test harness still use it.

`stretch_filter` indexes `Stretch::Filter` (0 Nearest, 1 Linear, 2 Gaussian,
3 Pyramidal) and maps to the `D3DTEXTUREFILTERTYPE` passed to `StretchRect`.
Nearest and Linear are always available. Gaussian and Pyramidal are offered only
when `D3DCAPS9::StretchRectFilterCaps` reports them; the setup combo greys out
the others and its tooltip says why. The index is parsed with a bounded
non-negative check instead of the positive-only helper the other integers use,
because 0 (Nearest) is a legal value.
