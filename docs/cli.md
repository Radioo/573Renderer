# r573_cli (src/cli/)

`Cli::Parse` turns argv into a plain `Cli::Options` struct and applies
nothing - main.cpp applies at startup. Returns false on `--help` (usage
printed, no error) and on any parse error (message in `err`). The complete
user-facing option reference lives in the `--help` text itself
(`kUsage` in cli.cpp); this doc carries the design and the RE context
behind the less obvious options.

## Parser structure

Table-driven: option tables of member pointers (`kBoolOpts`,
`kStringOpts`, `kIntOpts`, `kClampedIntOpts`, `kRangedIntOpts`) plus one
small handler function per option with real validation (`kSpecialOpts`).
Adding an option is one table row (or one row + one handler), and no
function ever grows another else-if arm. Numeric parsing is
`std::from_chars` with atoi-compatible semantics: leading digits parse,
garbage yields 0 where the old parser was forgiving, and explicitly
validated options (`--fps`, `--scale`, ...) reject instead.

Clamp behaviors preserved from the original parser: `--export-max-frames`
< 0 becomes 0 (no cap), `--export-loop-count` floors at 1, `--blend-frames`
floors at 0, `--seek-frame` < 0 becomes -1 (no seek), `--mc-name-type`
any nonzero becomes 1. `--screenshot-frames` parses permissively
("1,30,,0,120" keeps 1/30/120) and `--submonitor-frames` skips blank
tokens so trailing commas are harmless (paths with commas are unsupported;
Konami's subbg_*.png never contain one).

## Tool commands (src/cli/tool_command.cpp)

`Cli::ParseToolCommand` runs BEFORE `Cli::Parse` and before main creates a
window: if it returns a kind other than `None`, `ToolCommands::Run` executes it
and the process exits with its return code (`src/main.cpp`, the block right
after `CollectCliArgs`). The flags are matched in a fixed priority order, so a
command line naming two of them runs the first one in that order. Positionals stop
at the first token starting with `-`, everywhere: a flag is never swallowed as an
output path or an id, so `--preset-export-json iidx11 <id> --force` still writes
`<id>.json` rather than a file called `--force`.

| flag | arguments | what it does |
|---|---|---|
| `--preset-dump-defaults` | `<out-dir>` | writes every built-in preset document as `<out-dir>/<build>/<id>.json`. No game data, no window, no D3D9 device. This is what the preset CI gates read (docs/gates.md). Exits 2 with no directory named, rather than writing a build tree into the working directory. |
| `--preset-export-json` | `<build> <preset-id> [out.json]` | writes one document (built-in or user) as canonical JSON. Keyed by build plus id, so no game directory is involved. |
| `--preset-validate` | `<file.json>` | prints every validation problem and exits 1 if any of them is an error, 2 if the file cannot be parsed at all, 0 otherwise. |
| `--preset-test` | `<game-dir> [preset-id] [out.png] [frames]` | loads a preset, renders `frames` frames, writes a PNG, and exits 8 if nothing in the screen moves. |
| `--preset-export` | `<game-dir> [preset-id] [out] [frames]` | the same load, driven through the export pipeline (`--export-bg` applies). |
| `--preset-json` | `<file.json>` | with `--preset-test` / `--preset-export`: load that document instead of a registry id. The positional preset id is then omitted. |
| `--force` | | load a document that has validation errors anyway; the evaluator skips the offending clips. Without it, `--preset-json` refuses a document with errors. |
| `--preset-option` | `<option-id>=<choice>` | selects an option; repeatable, one per option. The choice is a label (`mode=EXPERT`) or an index (`mode=3`). It replaced the positional option-index argument, which could only ever reach the first option. |

`--preset-tweaks` is gone with the tweak file. Naming it makes the process print
what to do instead (export the JSON, edit it, run it with `--preset-json`) and
exit 2, rather than silently ignoring the flag.

`--preset-validate` and `--preset-json` check a file on its own, so they do NOT
apply the "a user id may not equal a built-in id" rule: that rule exists so an id
resolves to one document in a listing, and it is applied by the registry when the
file sits in `presets/<build>/`. Exporting a built-in, editing it and running it
with `--preset-json` therefore works without renaming it.

Preset ids are resolved through `Preset::Doc::Registry` (built-ins plus
`presets/<build>/*.json` next to the exe, docs/preset_document.md). With no id,
`--preset-test` takes the first document of the fingerprinted build. The export
profile comes from the document's `build` through
`GameFingerprint::ProfileSlugFor`, not from a literal.

## RE context per option

- `--animation-label`: mirrors SDVX scene lambdas calling
  `afp_mc_control(.., 0xF09 deep_goto_play_label, label)` for backgrounds
  with intro+loop structure (bg_bpls5 jumps to "loop" so it starts with the
  BPL5 monitors lit instead of playing the intro).
- `--scale`: the GUI Master-scale row; SDVX-I-IV 720x1280 select_bg
  variants need 1.5 to fill 1080x1920 (ratio's field of truth: BG entry
  payload+28 in soundvoltex.dll).
- `--afp-speed`: `afp_set_global_speed` (afp-core ord 0x00a). Content is
  authored at 120fps; the SDVX submonitor runs 60fps, so its export passes
  0.5 or everything animates 2x too fast.
- `--seek-frame`: CAfpViewerScene LEFT/RIGHT seek = `afp_mc_control 0xF08`;
  pauses on seek like the debug scene. `--goto-label` posts the live
  goto-label request (backend-agnostic via `Runtime::Active().GotoLabel`,
  modern 0xF09 / DDR afp_mc_op).
- `--filter` = debug viewer F7 (afp-core set-filter ord 0x032, id
  0x80000000|1); `--show-mc-names` = F3 DISP MC; `--mc-name-type` = F6.
- `--root-loop`: see docs/settings.md (same Hold/Force mechanism; the CLI
  value overrides settings.ini, tri-state with -1 = unset).
- `--export-fps`: default 60; 120 is possible but needs a 120 Hz display
  to present each frame (browsers vsync-cap at refresh rate).
- `--export-keyframe-interval`: frames between video keyframes (default 0 =
  one per second). Larger = fewer keyframes = smaller file; a value >= the
  captured frame count yields a single keyframe (smallest file, slower to
  seek). Applies only to the codec formats (`MediaSink::UsesKeyframeInterval`
  = everything except PNG and WebP); ignored otherwise. Maps to
  `Params::keyframe_interval` -> `AVCodecContext::gop_size`/`keyint_min`.
- `--export-format`: index into MediaSink::Format (docs/media_formats.md);
  bare "webm" maps to WebM-VP9 for backwards compatibility.
- `--export-dump-frames`: writes every pre-encode frame as PNG so encoder
  artifacts can be diffed against exact pre-encode pixels.
- Submonitor options: drive the SDVX submonitor slideshow/pan renders.
  `--submonitor-frames` binds loose subbg images to the placeholder clip's
  child layers via afp ord 0x088; `--submonitor-slideshow` is the oversized
  r3_fade two-layer dissolve (base + alpha-fading overlay, advanced at afp
  loop boundaries); `--submonitor-slideshow-fade` is the NORMAL (<=1080p)
  mode - the game's real mechanism per SdvxSubmoniBg_Load: frame0 bound to
  the CENTERED subbg_usr holder, transitions driven by the subbg_0001
  template's fade_in/fade_out labels (authored: fast in/out at 0-20 and
  840-860, smooth pair at 120-240 and 960-1080; hold from fade_in end 240
  to fade_out 960 = 720 frames, hence the dwell/fade defaults 720/120).
- Hot-swap/self-test options (`--swap-after-frames`, `--ifs2`,
  `--exit-after-frames`, `--screenshot-frames`) exist for --no-gui
  regression runs of the unload/load path.
- qpro options are the CLI face of the IIDX qpro extractor (see the qpro
  docs in the parent RE repo); `--qpro-only` takes part labels whose layer
  suffix is ignored, other parts stay in the manifest but are skipped.
- `--dump-anim-info <out.json>`: one-shot metadata dump handled by
  `AnimInspect::Run` (src/anim_inspect.cpp) right after the startup
  `--ifs` mounts, before the render loop would start; the process exits
  with the dump's status. For every animation the IFS's afplist.xml
  declares it switches the master stream (`Runtime::SwitchAnimation`,
  which is synchronous - the GUI reads labels the same way right after a
  switch), ticks `afp_do_update` twice so the clip tree settles, then
  reads the master clip's total frame count (`afp_mc_set 0x1011`) and
  timeline labels (`0x101F` count + `0x1020` name/frame) from the game's
  own afp engine. Output JSON: `{"ifs": ..., "anims": [{"name", "ok",
  "total_frames", "labels": [{"name", "frame"}]}]}`; an entry that fails
  to switch (afplist names that are not playable animations) gets
  `"ok": false` and no fields. This replaced the SDVX metadata
  extractor's bemaniutils dependency (afputils `list --include-frames`
  and the SWF `end`-label read): the values come from the game's libafp
  instead of a reimplementation, e.g. the submonitor pan anims report
  total 4800 and the collab scrolls carry their `end` label. Typical
  invocation: `--no-gui --game-dir <dir> --profile sdvx7 --ifs
  <submon.ifs> --dump-anim-info out.json`.

## PrintUsage

The usage text is runtime output (a WIN32-subsystem binary has no stdout
by default, so main shows it in a message box on --help; it stays printf-
discoverable when run from a console). Printed via `std::fputs` - not
printf - because variadic calls are banned by the tidy config.
