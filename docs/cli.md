# r573_cli (src/cli/)

`Cli::Parse` turns argv into a plain `Cli::Options` struct and applies
nothing; main.cpp applies the options at startup. It returns false on `--help`
(usage printed, no error) and on any parse error (message in `err`). The full
option reference for users is the `--help` text itself (`kUsage` in cli.cpp).
This doc covers the design and the RE context behind the less obvious options.

## Parser structure

The parser is table-driven: option tables of member pointers (`kBoolOpts`,
`kStringOpts`, `kIntOpts`, `kClampedIntOpts`, `kRangedIntOpts`) plus one
small handler function for each option that needs real validation
(`kSpecialOpts`). Adding an option takes one table row, or one row and one
handler, and no function grows another else-if arm. Numbers are parsed with
`std::from_chars` using atoi-compatible semantics: leading digits parse,
garbage yields 0 where the old parser was forgiving, and options with explicit
validation (`--fps`, `--scale`, ...) reject it instead.

These clamps are kept from the original parser: `--export-max-frames`
< 0 becomes 0 (no cap), `--export-loop-count` floors at 1, `--blend-frames`
floors at 0, `--seek-frame` < 0 becomes -1 (no seek), and any nonzero
`--mc-name-type` becomes 1. `--screenshot-frames` parses permissively
("1,30,,0,120" keeps 1/30/120), and `--submonitor-frames` skips blank
tokens so trailing commas are harmless. Paths containing commas are not
supported; Konami's subbg_*.png never contain one.

## Tool commands (src/cli/tool_command.cpp)

`Cli::ParseToolCommand` runs before `Cli::Parse` and before main creates a
window. If it returns a kind other than `None`, `ToolCommands::Run` executes it
and the process exits with its return code (`src/main.cpp`, the block right
after `CollectCliArgs`). The flags are matched in a fixed priority order, so a
command line that names two of them runs whichever comes first in that order.
Positionals always stop at the first token that starts with `-`, so a flag is
never taken as an output path or an id: `--preset-export-json iidx11 <id> --force`
writes `<id>.json`, not a file called `--force`.

| flag | arguments | what it does |
|---|---|---|
| `--preset-dump-defaults` | `<out-dir>` | writes every built-in preset document as `<out-dir>/<build>/<id>.json`. No game data, no window, no D3D9 device. The preset CI gates read this output (docs/gates.md). With no directory named it exits 2 instead of writing a build tree into the working directory. |
| `--preset-export-json` | `<build> <preset-id> [out.json]` | writes one document (built-in or user) as canonical JSON. It is keyed by build and id, so no game directory is involved. |
| `--preset-validate` | `<file.json>` | prints every validation problem. Exits 1 if any of them is an error, 2 if the file cannot be parsed at all, 0 otherwise. |
| `--preset-test` | `<game-dir> [preset-id] [out.png] [frames]` | loads a preset, renders `frames` frames, writes a PNG, and exits 8 if nothing in the screen moves. |
| `--preset-export` | `<game-dir> [preset-id] [out] [frames]` | the same load, driven through the export pipeline (`--export-bg` applies). |
| `--preset-json` | `<file.json>` | with `--preset-test` / `--preset-export`: load that document instead of a registry id. Leave out the positional preset id. |
| `--force` | | load a document that has validation errors anyway; the evaluator skips the clips at fault. Without it, `--preset-json` refuses a document with errors. |
| `--txp2-dump` | `<game-dir> <package.bin> <out-dir>` | writes every texture in a TXP2 package as `<name>.png`, every named cell as `cell_<name>.png` cropped out of its texture, and, when the package carries afp streams, an `inputs.json` naming each animation's input surface. It boots only the engine DLLs and AVS (AVS owns the `avslz` inflate the texture payloads need), with no window and no D3D9 device. That lets it read packages an AFP render cannot: the IIDX 18/19 `lane%03d.bin` covers hold one texture, one cell and no animation, and `LoadTxp2Package` rejects them because they produce no streams. |
| `--judge` | `<game-dir>` | opens the interpolation judge: every clip of every TXP2 package of the install, authored next to interpolated, with a verdict file. Its options (`--profile`, `--judge-verdicts`, `--judge-clip`, `--judge-capture`, `--judge-capture-frames`, `--judge-display-hz`, `--judge-background`) and the tool itself are described in docs/judge.md. |
| `--gc2d-sheet` | `<package-dir> <out-dir> [samples]` | renders every animation and cell of a GC 2D package (IIDX 14 to 17, `system.idx` + `*.gcz`). Each animation is sampled `samples` times across its length, so a `samples` value at or above the animation's length writes every frame. |
| `--gc2d-at` | `<x>,<y>` | with `--gc2d-sheet`: where to place the sprite in the 640x480 window. The default `0,0` puts its registration point at the top left corner. That suits a full-screen background like a play frame, and clips a centred effect to its lower-right quadrant. `320,240` centres it. |
| `--gc2d-alpha` | | with `--gc2d-sheet`: write straight-alpha PNGs instead of the opaque back buffer. See below. |
| `--gc2d-parts` | `<package-dir>` | with `--gc2d-sheet` and `--iidx-playfield`: the package the value cells come from (IIDX 17's `gameparts`). |
| `--preset-option` | `<option-id>=<choice>` | selects an option; repeat it, once per option. The choice is a label (`mode=EXPERT`) or an index (`mode=3`). It replaced the positional option-index argument, which could only reach the first option. |

The motion check takes its baseline at the first rendered frame that has any
visible model, not at frame 0. A screen whose 3D appears later (HAPPY SKY's staff
roll shows the sky dome from frame 200) would otherwise be compared against an
empty pose list and reported as having no 3D at all. If no model is visible during
the whole run, the log says so and gives the number of frames rendered, instead of
claiming the preset contains no model.

`--gc2d-sheet` with `--gc2d-parts` and `--iidx-playfield` fills a GC play frame's
values the way the game does. A GC animation carries element records (record type
3) whose id names a value instead of a cell. The game registers a callback that
the engine calls with that id and the position it computed, and draws the score,
the groove gauge and the other values there. The sheet reads the same records, so
the positions come from the art and nothing is hardcoded per frame. It writes them
to `elements.txt` whether or not the values are drawn.

`--gc2d-alpha` recovers the alpha that the GC 2D renderer never writes. The sheet
normally saves the back buffer, which is opaque, so a sprite arrives composited
over black and a browser or an editor has nothing to key on. With the flag each
shot is drawn twice, once over black and once over white, and the two are
unmixed per pixel:

    coverage = max over rgb of 255 - clamp(over_white - over_black, 0, 255)
    straight = over_black * 255 / coverage

This needs no knowledge of the blend mode. A normal-blend sprite gives the same
coverage on every channel, because compositing `src` over white adds
`255 * (1 - a)`. An additive sprite saturates the white pass, so the expression
reduces to `max(r, g, b)` over black, which is the additive coverage the modern
AFP backend's own pixel shader computes. It was checked against two files already
on the server that were made independently of this renderer: SIRIUS's `items`
`EXP0` comes out 85x103 over 9 visible frames, the size and frame count of
`exp_0_legacy.webp`, and its `TURN_00_0000` cell matches `turn_0_legacy.png`.

`--preset-tweaks` was removed along with the tweak file. If it is passed, the
process prints what to do instead (export the JSON, edit it, run it with
`--preset-json`) and exits 2, so the flag is never silently ignored.

`--preset-validate` and `--preset-json` check a file on its own, so they do not
apply the rule that a user id may not equal a built-in id. That rule keeps each id
pointing at one document in a listing, and the registry applies it when the file
sits in `presets/<build>/`. You can therefore export a built-in, edit it and run
it with `--preset-json` without renaming it.

Preset ids are resolved through `Preset::Doc::Registry` (built-ins plus
`presets/<build>/*.json` next to the exe, docs/preset_document.md). With no id,
`--preset-test` takes the first document of the fingerprinted build. The export
profile comes from the document's `build` through
`GameFingerprint::ProfileSlugFor`, not from a literal.

## RE context per option

- `--animation-label`: mirrors the SDVX scene lambdas that call
  `afp_mc_control(.., 0xF09 deep_goto_play_label, label)` for backgrounds
  with an intro and a loop (bg_bpls5 jumps to "loop" so it starts with the
  BPL5 monitors lit instead of playing the intro).
- `--load-alongside <path>`: the command-line version of Browse's "Load alongside"
  context menu (docs/gui.md 3.1). Each path resolves through the same
  catalog lookup as `--ifs` (bare name or full path) and is mounted as an
  extra afpu package after the `--ifs` load, in the order given. A later
  package shadows an earlier one on a name clash, because afpu's bitmap lookup
  takes the last one loaded. .arc entries are skipped with a log line, since
  only afpu packages can be overlaid. The overlays are applied before
  `ApplyCliOverrides`, so an `--animation` switch binds against overlays that
  are already mounted (section 5 BIND-ON-PLAY in docs/boot_and_render_loop.md).
  The option does nothing on the DDR backend, which has no afpu packages.
- `--render-size`: when omitted, the render size is the last one saved in settings.ini
  (`main.cpp` `SeedStateAndStartGui`), not the profile's `default_render_w/h`, so an SDVX
  launch after a DDR session comes up 1280x720 landscape. Pass it on every scripted launch,
  including throwaway diagnostics: SDVX main screen = `1080x1920` portrait, SDVX sub monitor
  content = `1920x1080` landscape (it runs at 60fps, see `--afp-speed`). If a dump is too big,
  capture fewer frames or downscale only the offline analysis copy. Never shrink the render.
- `--scale`: the GUI Master-scale row. SDVX-I-IV 720x1280 select_bg
  variants need 1.5 to fill 1080x1920 (the ratio's source of truth is BG entry
  payload+28 in soundvoltex.dll).
- `--afp-speed`: `afp_set_global_speed` (afp-core ord 0x00a). Content is
  authored at 120fps and the SDVX submonitor runs at 60fps, so its export passes
  0.5, or everything animates twice as fast as it should.
- `--seek-frame`: CAfpViewerScene LEFT/RIGHT seek = `afp_mc_control 0xF08`.
  It pauses on seek like the debug scene. `--goto-label` posts the live
  goto-label request, which works on either backend through
  `Runtime::Active().GotoLabel` (modern 0xF09 / DDR afp_mc_op).
- `--filter` = debug viewer F7 (afp-core set-filter ord 0x032, id
  0x80000000|1); `--show-mc-names` = F3 DISP MC; `--mc-name-type` = F6.
- `--stretch <0|1>`: overrides settings.ini's `stretch_16_9` for this run
  (tri-state, -1 = unset, the same shape as `--root-loop`). A 4:3 game is
  presented widened on a 16:9 cabinet monitor (docs/settings.md). That is
  right for looking at a screen and wrong for pulling an asset out of one. The
  web UI that uses IIDX 9-19 artwork stores it at native 640x480 and
  applies the same widening in CSS, so extraction passes 0 and keeps the
  horizontal resolution that an 854x480 present would throw away. Like the
  other startup settings, it is written back into settings.ini on boot.
- `--load-bitmaps <path>` / `--iidx-playfield <spec>`: the IIDX 18/19 play
  frame draws its chrome and leaves every live value as an authored
  placeholder clip whose colour transform has alpha 0, so the game never shows
  it. The game hides the clip, reads its position, and draws the figure itself
  from `gameparts.bin` (`_sco00`.. for score and max combo, `_bpm00`..,
  `_bpm_m00`.., `_par00`.. for the percent, `_gauge_normal_1p` for the bar).
  `--load-bitmaps` mounts that second package as a bitmap source and
  `--iidx-playfield` reproduces the drawer. The layout constants come from
  `sub_1001F2E0` and the gauge from `sub_10058340`.

  The spec also carries `line_x`, `line_y`, `line_clip` and `glow_y`, which draw
  the red judgment bar and the cyan gradient above it. Those are neither in the
  frame's art nor a placeholder clip: the play loop draws them every frame from a
  table in the DLL, so a frame rendered on its own shows a bare lane field.
  `line_clip` masks the bar to its first visible row; this is how IIDX 19 trims a
  6 row cell to the 4 rows it shows. The numbers differ per version, so the caller
  supplies them, not the renderer.
- `DDR_TRACE_PRIM=1`: logs every legacy `draw_primitive` with its decoded
  vertex layout, the position bounding box, the texture id and the modulate
  colour, plus every `set_mask`. Use it to diagnose a sprite that reaches the
  engine but never reaches the screen: the box shows whether the quad is
  degenerate, and the mask lines show whether the draw happened inside a
  mask-write phase.
- `--root-loop`: see docs/settings.md (same Hold/Force mechanism; the CLI
  value overrides settings.ini, tri-state with -1 = unset).
- `--export-fps`: default 60. 120 works, but the viewer needs a 120 Hz display
  to see each frame (browsers cap playback at the refresh rate).
- `--export-keyframe-interval`: frames between video keyframes (default 0 =
  one per second). A larger value means fewer keyframes and a smaller file; a
  value >= the captured frame count gives a single keyframe (smallest file,
  slower to seek). It applies only to the codec formats
  (`MediaSink::UsesKeyframeInterval` = everything except PNG and WebP) and is
  ignored otherwise. Maps to `Params::keyframe_interval` ->
  `AVCodecContext::gop_size`/`keyint_min`.
- `--export-format`: index into MediaSink::Format (docs/media_formats.md).
  A bare "webm" maps to WebM-VP9 for backwards compatibility.
- `--export-dump-frames`: writes every frame as PNG before encoding, so
  encoder artifacts can be diffed against the exact pre-encode pixels.
- Submonitor options drive the SDVX submonitor slideshow and pan renders.
  `--submonitor-frames` binds loose subbg images to the placeholder clip's
  child layers through afp ord 0x088. `--submonitor-slideshow` is the oversized
  r3_fade two-layer dissolve (a base plus an overlay fading in alpha, advanced
  at afp loop boundaries). `--submonitor-slideshow-fade` is the normal (<=1080p)
  mode, which follows the game's own mechanism in SdvxSubmoniBg_Load: frame0 is
  bound to the centred subbg_usr holder, and transitions are driven by the
  subbg_0001 template's fade_in/fade_out labels. As authored, those are a fast
  in/out at 0-20 and 840-860 and a smooth pair at 120-240 and 960-1080. The hold
  runs from the end of fade_in at 240 to fade_out at 960, 720 frames, which is
  where the dwell/fade defaults of 720/120 come from.
- The hot-swap and self-test options (`--swap-after-frames`, `--ifs2`,
  `--exit-after-frames`, `--screenshot-frames`) are for --no-gui
  regression runs of the unload/load path.
- Reproduce a boot or render fault with `--no-gui`, never `--headless`.
  `--headless` creates no window and no D3D9 device and runs only the init
  sequence, so the AFP draw path, and everything that depends on it, never
  runs. Pass `--profile` explicitly on reproducible runs so the result
  does not depend on a saved slug or on auto-detection.
- 573Renderer.exe is a WIN32-subsystem binary, so a shell does not wait for
  it: PowerShell's `& .\573Renderer.exe ...` returns at once while the render
  thread keeps going. Pipe the call to `Out-Null` (or use
  `Start-Process -Wait`) when a script needs the run to finish first. The log
  is `renderer.log` in the current working directory, not next to the exe.
- The qpro options are the command-line version of the IIDX qpro extractor.
  `--qpro-only` takes part labels and ignores their layer suffix; other parts
  stay in the manifest but are skipped.
- `--dump-anim-info <out.json>`: a one-shot metadata dump handled by
  `AnimInspect::Run` (src/anim_inspect.cpp) right after the startup
  `--ifs` mounts, before the render loop would start. The process exits
  with the dump's status. For every animation the IFS's afplist.xml
  declares, it switches the master stream (`Runtime::SwitchAnimation`,
  which is synchronous; the GUI reads labels the same way right after a
  switch), ticks `afp_do_update` twice so the clip tree settles, then
  reads the master clip's total frame count (`afp_mc_set 0x1011`) and
  timeline labels (`0x101F` count + `0x1020` name/frame) from the game's
  own afp engine. Output JSON: `{"ifs": ..., "anims": [{"name", "ok",
  "total_frames", "labels": [{"name", "frame"}]}]}`. An entry that fails
  to switch (afplist names that are not playable animations) gets
  `"ok": false` and no other fields. The SDVX metadata extractor used to
  read these values with an outside afp parser (a `list --include-frames`
  listing and a read of the SWF `end` label). Now they come from the game's
  libafp: for example, the submonitor pan anims report a total of 4800 and
  the collab scrolls carry their `end` label. A typical invocation:
  `--no-gui --game-dir <dir> --profile sdvx7 --ifs <submon.ifs>
  --dump-anim-info out.json`.

## PrintUsage

The usage text is runtime output. A WIN32-subsystem binary has no stdout by
default, so main shows the text in a message box on --help; run from a console,
it still goes to stdout. It is printed with `std::fputs` instead of printf,
because the tidy config bans variadic calls.
