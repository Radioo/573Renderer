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
  goto-label request (backend-agnostic via the RenderLive::Inspect seam,
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

## PrintUsage

The usage text is runtime output (a WIN32-subsystem binary has no stdout
by default, so main shows it in a message box on --help; it stays printf-
discoverable when run from a console). Printed via `std::fputs` - not
printf - because variadic calls are banned by the tidy config.
