# Command line

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `afp-speed` | --afp-speed <f> | cli-arg | - | 1 |
| 2 | `animation` | --animation <name> | cli-arg | - | 1 |
| 3 | `animation-label` | --animation-label <l> | cli-arg | - | 1 |
| 4 | `blend-frames` | --blend-frames <N> | cli-arg | - | 1 |
| 5 | `blend-loop` | --blend-loop | cli-arg | - | 1 |
| 6 | `boot-ifses` | --boot-ifses | cli-arg | - | 2 |
| 7 | `cmd-trace` | --cmd-trace <path> | cli-arg | - | 2 |
| 8 | `continuous-loop` | --continuous-loop <N> | cli-arg | - | 1 |
| 9 | `csv-empty-token-tolerance` *(audit)* | comma-list flags given empty tokens (--export-crop, --export-bg, --screenshot-frames, --submonitor-frames) | cli-arg | - | 1 |
| 10 | `deferred-replay` | --deferred-replay | cli-arg | - | 1 |
| 11 | `dump-anim-info` | --dump-anim-info <out.json> | cli-arg | - | 1 |
| 12 | `duplicate-scalar-flag-last-wins` *(audit)* | any scalar flag repeated in one command line | cli-arg | - | **none** |
| 13 | `exit-after-frames` | --exit-after-frames <N> | cli-arg | - | 1 |
| 14 | `export` | --export <path> | cli-arg | - | 1 |
| 15 | `export-bg` | --export-bg <transparent\|none\|R,G,B> | cli-arg | - | 1 |
| 16 | `export-crop` | --export-crop <X,Y,W,H> | cli-arg | - | 1 |
| 17 | `export-dump-frames` | --export-dump-frames <dir> | cli-arg | - | 1 |
| 18 | `export-format` | --export-format <avif\|webm\|webm-av1\|webp\|png\|mp4> | cli-arg | - | 1 |
| 19 | `export-format-alias-tokens` *(audit)* | --export-format <webm-vp9\|vp9\|av1\|webp-anim\|png-seq\|pngseq\|h264\|avc\|mp4-h264> | cli-arg | - | 1 |
| 20 | `export-format-mp4-hevc-alpha` *(audit)* | --export-format <mp4-hevc-alpha\|hevc\|hevc-alpha\|mp4-hevc\|safari> | cli-arg | - | 1 |
| 21 | `export-fps` | --export-fps <N> | cli-arg | - | 1 |
| 22 | `export-keyframe-interval` | --export-keyframe-interval <N> | cli-arg | - | 1 |
| 23 | `export-loop-count` | --export-loop-count <N> | cli-arg | - | 1 |
| 24 | `export-max-frames` | --export-max-frames <N> | cli-arg | - | 1 |
| 25 | `export-no-hw` | --export-no-hw | cli-arg | - | 1 |
| 26 | `export-quality` | --export-quality <0-100> | cli-arg | - | 1 |
| 27 | `export-size` | --export-size <WxH> | cli-arg | - | 1 |
| 28 | `export-sw` | --export-sw | cli-arg | - | 2 |
| 29 | `extract-qpro` | --extract-qpro <dir> | cli-arg | - | 2 |
| 30 | `filter` | --filter | cli-arg | - | 1 |
| 31 | `fps` | --fps <N> | cli-arg | - | 1 |
| 32 | `game-dir` | --game-dir <path> | cli-arg | - | 2 |
| 33 | `goto-label` | --goto-label <l> | cli-arg | - | 2 |
| 34 | `headless` | --headless | cli-arg | - | 1 |
| 35 | `headless-without-game-dir-exits-1` *(audit)* | --headless with no --game-dir | cli-arg | - | 2 |
| 36 | `help-long` | --help | cli-arg | - | 1 |
| 37 | `help-short` | -h | cli-arg | - | **none** |
| 38 | `help-slash` | /? | cli-arg | - | **none** |
| 39 | `hide` | --hide <path> | cli-arg | - | 1 |
| 40 | `hide-sublayer` | --hide-sublayer <path> | cli-arg | - | 1 |
| 41 | `ifs` | --ifs <path> | cli-arg | - | 1 |
| 42 | `ifs2` | --ifs2 <path> | cli-arg | - | 1 |
| 43 | `int-opt-unvalidated-value` *(audit)* | the nine kIntOpts flags given a negative, junk-suffixed, or non-numeric value | cli-arg | - | 2 |
| 44 | `mc-name-type` | --mc-name-type <0\|1> | cli-arg | - | 1 |
| 45 | `missing-value` | any value-taking flag typed as the last argv token | cli-arg | - | **none** |
| 46 | `no-arguments-at-all` *(audit)* | launching with an empty command line | cli-arg | - | **none** |
| 47 | `no-gui` | --no-gui | cli-arg | - | 3 |
| 48 | `no-gui-without-game-dir-exits-1` *(audit)* | --no-gui with no --game-dir | cli-arg | - | 4 |
| 49 | `no-tool-command` | launching with no tool subcommand at all | cli-arg | - | **none** |
| 50 | `profile` | --profile <slug> | cli-arg | - | 1 |
| 51 | `qpro-back-composite` | --qpro-back-composite <value> | cli-arg | - | 2 |
| 52 | `qpro-back-one` | --qpro-back-one <value> | cli-arg | - | 2 |
| 53 | `qpro-body-one` | --qpro-body-one <value> | cli-arg | - | 1 |
| 54 | `qpro-clip-one` | --qpro-clip-one <value> | cli-arg | - | 2 |
| 55 | `qpro-dump` | --qpro-dump <ifs> | cli-arg | - | 2 |
| 56 | `qpro-face-one` | --qpro-face-one <value> | cli-arg | - | 2 |
| 57 | `qpro-fps` | --qpro-fps <N> | cli-arg | - | 1 |
| 58 | `qpro-hair-one` | --qpro-hair-one <value> | cli-arg | - | 2 |
| 59 | `qpro-hand-composite` | --qpro-hand-composite <value> | cli-arg | - | 2 |
| 60 | `qpro-hand-one` | --qpro-hand-one <value> | cli-arg | - | 2 |
| 61 | `qpro-head-composite` | --qpro-head-composite <value> | cli-arg | - | 2 |
| 62 | `qpro-head-one` | --qpro-head-one <value> | cli-arg | - | 2 |
| 63 | `qpro-modifier-flags-inert-alone` *(audit)* | --qpro-parts / --qpro-only / --qpro-fps / --qpro-no-hue-scope without a qpro mode flag | cli-arg | - | 3 |
| 64 | `qpro-no-hue-scope` | --qpro-no-hue-scope | cli-arg | - | 2 |
| 65 | `qpro-oneshot-precedence` *(audit)* | passing more than one qpro one-shot flag at once | cli-arg | - | **none** |
| 66 | `qpro-only` | --qpro-only <value> | cli-arg | - | 2 |
| 67 | `qpro-parts` | --qpro-parts <value> | cli-arg | - | 2 |
| 68 | `render-size` | --render-size <WxH> | cli-arg | - | 1 |
| 69 | `root-loop` | --root-loop <hold\|force> | cli-arg | - | 1 |
| 70 | `scale` | --scale <factor> | cli-arg | - | 1 |
| 71 | `screenshot-frames` | --screenshot-frames <f1,f2,...> | cli-arg | - | 1 |
| 72 | `screenshot-prefix` | --screenshot-prefix <p> | cli-arg | - | 1 |
| 73 | `seek-frame` | --seek-frame <N> | cli-arg | - | 1 |
| 74 | `show-mc-names` | --show-mc-names | cli-arg | - | 2 |
| 75 | `show-sublayer` | --show-sublayer <path> | cli-arg | - | 1 |
| 76 | `size-pair-lenient-parse` *(audit)* | --render-size / --export-size value format | cli-arg | - | 2 |
| 77 | `start-paused` | --start-paused | cli-arg | - | 1 |
| 78 | `submonitor-clip` | --submonitor-clip <path> | cli-arg | - | 1 |
| 79 | `submonitor-dwell-frames` | --submonitor-dwell-frames <N> | cli-arg | - | 1 |
| 80 | `submonitor-fade-frames` | --submonitor-fade-frames <N> | cli-arg | - | 1 |
| 81 | `submonitor-fade-in-label` | --submonitor-fade-in-label <l> | cli-arg | - | 2 |
| 82 | `submonitor-fade-out-label` | --submonitor-fade-out-label <l> | cli-arg | - | 2 |
| 83 | `submonitor-frames` | --submonitor-frames <p1,p2,...> | cli-arg | - | 1 |
| 84 | `submonitor-loop-frames` | --submonitor-loop-frames <N> | cli-arg | - | 1 |
| 85 | `submonitor-slideshow` | --submonitor-slideshow | cli-arg | - | 1 |
| 86 | `submonitor-slideshow-fade` | --submonitor-slideshow-fade | cli-arg | - | 1 |
| 87 | `submonitor-swap-layers` | --submonitor-swap-layers | cli-arg | - | 2 |
| 88 | `swap-after-frames` | --swap-after-frames <N> | cli-arg | - | 1 |
| 89 | `tool-ddr-test` | --ddr-test <in> [arc] [out.png] [frames] | cli-arg | - | 2 |
| 90 | `tool-ddr-test-arc` | --ddr-test positional 2: arc path | cli-arg | - | 2 |
| 91 | `tool-ddr-test-frames` | --ddr-test positional 4: frame count | cli-arg | - | 2 |
| 92 | `tool-ddr-test-out` | --ddr-test positional 3: output PNG path | cli-arg | - | 2 |
| 93 | `tool-extract-arc` | --extract-arc <path> | cli-arg | - | 2 |
| 94 | `tool-extract-customize` | --extract-customize <path> | cli-arg | - | 1 |
| 95 | `tool-extract-qpro-json` | --extract-qpro-json <dll> [out.json] | cli-arg | - | 2 |
| 96 | `tool-extract-qpro-json-out` | --extract-qpro-json positional 2: output JSON path | cli-arg | - | 1 |
| 97 | `tool-qpro-scan` | --qpro-scan <path> | cli-arg | - | 1 |
| 98 | `tool-scene3d-test` | --scene3d-test <in> [out.png] [frames] | cli-arg | - | **none** |
| 99 | `tool-scene3d-test-frames` | --scene3d-test positional 3: frame count | cli-arg | - | **none** |
| 100 | `tool-scene3d-test-out` | --scene3d-test positional 2: output PNG path | cli-arg | - | **none** |
| 101 | `unknown-argument` | any unrecognised token | cli-arg | - | **none** |
| 102 | `value-flag-consumes-following-flag` *(audit)* | any value-taking flag whose next argv token is itself a flag | cli-arg | - | **none** |
| 103 | `variant` | --variant <path>=<bitmap> | cli-arg | - | 1 |

## Detail

### 1. --afp-speed <f>

- **id**: `afp-speed`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; value must parse as float in [0, 16]
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::afp_speed = f. Invalid input errors "--afp-speed expects 0..16 (e.g. 0.5 for the 60fps submonitor)" and aborts with rc 1
- **source**: `src/cli/cli.cpp:410`
- **notes**: Undocumented in --help. Special handler registered at cli.cpp:548.
- **tests**: `cli: scale and afp-speed validate as floats`
- **audit correction**: Effect "Options::afp_speed = f" omits two gates, one of which makes the documented accepted value 0 a silent no-op. -> Options::afp_speed = f (0..16 accepted). It is consumed only in ApplyCliOverrides: `if (opts.afp_speed > 0.0F && Runtime::Active().SetGlobalSpeed(g_afp, opts.afp_speed))` logging "afp global speed set to %.3f (--afp-speed)" (boot.cpp:157-159). f == 0 passes validation but does nothing. ApplyCliOverrides has exactly one call site, main.cpp:381, inside the successful-mount branch of MountStartupContent - so the flag is inert without a startup IFS that mounts.

### 2. --animation <name>

- **id**: `animation`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; meaningful only after --ifs loads
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::animation_name = value: switch to this named animation after --ifs load, before any --export starts
- **source**: `src/cli/cli.cpp:309`
- **tests**: `cli: Parse handles flags and string options`

### 3. --animation-label <l>

- **id**: `animation-label`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applied after the --animation switch
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::animation_label = value: deep-goto that label on the master movie clip (e.g. 'loop' to skip an SDVX intro)
- **source**: `src/cli/cli.cpp:310`
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`

### 4. --blend-frames <N>

- **id**: `blend-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; only meaningful with --blend-loop
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_blend_frames = N when N >= 0, else 0: crossfade length for the blended loop seam (struct default 15)
- **source**: `src/cli/cli.cpp:357`
- **notes**: Clamped option.
- **tests**: `cli: clamped int options keep their floors`

### 5. --blend-loop

- **id**: `blend-loop`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_blend_loop = true: smooth-loop export, find the frame closest to the start and crossfade the seam
- **source**: `src/cli/cli.cpp:294`
- **notes**: Pairs with --blend-frames.
- **tests**: `cli: Parse handles the full export option set`

### 6. --boot-ifses

- **id**: `boot-ifses`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::boot_ifses = true
- **source**: `src/cli/cli.cpp:289`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles flags and string options`, `cli: help returns false without error`
- **audit correction**: Effect is the bare "Options::boot_ifses = true" with no consumer, which reads as if the flag were inert. -> Options::boot_ifses = true is forwarded as the load_boot_ifses argument of BootFromGameDir (main.cpp:297) and lands as Env::load_boot_content (src/boot.cpp:90, :121), so the boot sequence also loads the game's boot IFS content instead of booting bare.

### 7. --cmd-trace <path>

- **id**: `cmd-trace`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::cmd_trace_path = value
- **source**: `src/cli/cli.cpp:330`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`, `cli: help returns false without error`
- **audit correction**: Effect is the bare "Options::cmd_trace_path = value" with no consumer named, hiding that the flag both enables render-command capture and writes a file. -> Options::cmd_trace_path = value. A non-empty value arms capture by pointing g_gpu.cmd_list at the frame's RenderCommandList (src/render_loop.cpp:334); WriteCmdTrace then disarms the pointer, formats the list and writes it to that path, logging "--cmd-trace: %zu commands -> %zu bytes to '%s'" on success or "--cmd-trace: FAILED to open '%s'" when the ofstream cannot be opened (src/render_loop.cpp:186-198).

### 8. --continuous-loop <N>

- **id**: `continuous-loop`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::continuous_loop_mode = ParseIntOrZero(value)
- **source**: `src/cli/cli.cpp:337`
- **notes**: Undocumented in --help. Unparsable text silently yields 0.
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`
- **audit correction**: Effect "Options::continuous_loop_mode = ParseIntOrZero(value)" omits the zero gate, the downstream clamp, and the startup-IFS requirement. -> Options::continuous_loop_mode = ParseIntOrZero(value). Applied only when != 0, via MutateLiveOverrides setting LiveOverrides::continuous_loop_mode (boot.cpp:160-164), and the live-controls layer clamps that member to -1..1 (src/state/live_controls.cpp:64), so only -1 and 1 are meaningful and 0 means "leave the live override alone". Reached only through ApplyCliOverrides, i.e. only after a startup IFS mounts successfully (main.cpp:375-386).

### 9. comma-list flags given empty tokens (--export-crop, --export-bg, --screenshot-frames, --submonitor-frames)

- **id**: `csv-empty-token-tolerance` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; the value contains adjacent, leading, or trailing commas
- **effect**: SplitCsv drops empty tokens before counting, so `--export-bg 30,,33,,43,` still yields exactly three ints and succeeds, and `--export-crop ,0,0,100,50,` still yields four and succeeds, while an all-empty value like `--export-crop ,,,` collapses to zero tokens and fails the expected-count check with "--export-crop expects 'X,Y,W,H' with non-negative ints". A completely empty value ('') yields an empty list: --export-crop/--export-bg error, --screenshot-frames/--submonitor-frames succeed as no-ops.
- **source**: `src/cli/cli.cpp:222-233 (SplitCsv, empty tokens skipped at :228), :235-246 (ParseIntList count check at :237)`
- **notes**: main.cpp's ForEachCsvToken (used by --qpro-parts / --qpro-only, src/main.cpp:80-90) does NOT drop empty tokens, so the two CSV dialects in this CLI behave differently.
- **tests**: `cli: Parse handles every string-valued qpro flag`

### 10. --deferred-replay

- **id**: `deferred-replay`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::deferred_replay = true: record each frame's render commands and replay them in one pass at end of frame instead of issuing device calls inline
- **source**: `src/cli/cli.cpp:288`
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`

### 11. --dump-anim-info <out.json>

- **id**: `dump-anim-info`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; intended with --no-gui --game-dir --ifs
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::dump_anim_info = value: after the startup IFS loads, switch to every animation the IFS lists, read master-clip total frames and timeline labels from the afp engine, write JSON, and exit
- **source**: `src/cli/cli.cpp:329`
- **tests**: `cli: Parse handles dump-anim-info`

### 12. any scalar flag repeated in one command line

- **id**: `duplicate-scalar-flag-last-wins` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; the same option name appears two or more times
- **effect**: The per-argument loop keeps no seen-set and every handler is a plain assignment, so the LAST occurrence wins for every bool/string/int/clamped/ranged/special option: `--export-fps 30 --export-fps 60` yields 60; `--export-bg transparent --export-bg 30,33,43` yields the opaque colour. The only exceptions are the six appending options (--variant, --hide, --hide-sublayer, --show-sublayer, --screenshot-frames, --submonitor-frames), whose repeats accumulate.
- **source**: `src/cli/cli.cpp:637-652 (loop), :565 (bool assign), :575 (string assign), :586/:595/:615 (int assigns)`
- **notes**: The inventory marks the six appenders "Repeatable" but never states the last-wins rule for the other ~60 flags, so the two behaviours are indistinguishable from the record set.
- **tests**: none

### 13. --exit-after-frames <N>

- **id**: `exit-after-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::exit_after_frames = ParseIntOrZero(value): exit cleanly after N frames; 0 runs forever until the window is closed / ESC
- **source**: `src/cli/cli.cpp:339`
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`

### 14. --export <path>

- **id**: `export`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; export begins only after the startup IFS loads
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_path = value: start export after the startup IFS loads, then exit once encoding finishes
- **source**: `src/cli/cli.cpp:327`
- **notes**: Gates the whole --export-* family in practice.
- **tests**: `cli: Parse handles the full export option set`

### 15. --export-bg <transparent\|none\|R,G,B>

- **id**: `export-bg`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: "transparent" or "none" sets Options::export_bg_transparent = true; otherwise three comma-separated ints set export_bg_transparent = false and export_bg_r/g/b = value/255. Bad input errors "--export-bg expects 'transparent' or 'R,G,B' with 0..255 ints" and aborts with rc 1
- **source**: `src/cli/cli.cpp:473`
- **notes**: Special handler registered at cli.cpp:553. The undocumented "none" alias is accepted alongside "transparent"; the 0..255 bound is not actually range-checked.
- **tests**: `cli: export-bg parses transparent and rgb`

### 16. --export-crop <X,Y,W,H>

- **id**: `export-crop`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; must be exactly four comma-separated non-negative ints
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_crop_x/y/w/h set from the four values. Wrong count, unparsable, or negative errors "--export-crop expects 'X,Y,W,H' with non-negative ints" and aborts with rc 1
- **source**: `src/cli/cli.cpp:458`
- **notes**: Undocumented in --help. Special handler registered at cli.cpp:552.
- **tests**: `cli: export-crop needs four non-negative ints`

### 17. --export-dump-frames <dir>

- **id**: `export-dump-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; needs an active export
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_dump_frames_dir = value: dump pre-encode BGRA frames to that directory for codec-diff regression tests
- **source**: `src/cli/cli.cpp:328`
- **tests**: `cli: Parse handles the full export option set`

### 18. --export-format <avif\|webm\|webm-av1\|webp\|png\|mp4>

- **id**: `export-format`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; MediaSink::ParseToken must accept the token
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_format = MediaSink::ToIndex(parsed format). Unrecognised token errors "--export-format expects 'avif', 'webm' (=webm-vp9), 'webm-av1', 'webp', 'png', or 'mp4'" and aborts with rc 1
- **source**: `src/cli/cli.cpp:444`
- **notes**: Special handler registered at cli.cpp:551. One entry covers all six accepted tokens since they share one handler; the accepted set is owned by MediaSink::ParseToken.
- **tests**: `cli: export-format resolves tokens and aliases`
- **audit correction**: Wrong accepted-token set. The control string and the note claim six tokens "owned by MediaSink::ParseToken", but ParseToken accepts 20 distinct tokens across kTable, kAliases and a "safari" special case, and one of them selects a format (MP4_HEVC_Alpha, export_format index 6) that no documented token can reach. The note that "one entry covers all six accepted tokens" is therefore incorrect. -> control: "--export-format <token>" where token is one of avif \| webm-vp9 \| webm-av1 \| webp \| png \| mp4 \| mp4-hevc-alpha (canonical, media_format.cpp:22-70) or the aliases webm, vp9, av1, webp-anim, png-seq, pngseq, h264, avc, mp4-h264, hevc, hevc-alpha, mp4-hevc (media_format.cpp:78-89) or the literal "safari" (media_format.cpp:154). Matching is exact and case-sensitive. Note that the documented "webm" is an alias and "webm-vp9" is the canonical token.

### 19. --export-format <webm-vp9\|vp9\|av1\|webp-anim\|png-seq\|pngseq\|h264\|avc\|mp4-h264>

- **id**: `export-format-alias-tokens` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Nine further accepted spellings beyond the six documented names: "webm-vp9" (the canonical token, while the documented "webm" is itself only an alias) and "vp9" -> WebM_VP9; "av1" -> WebM_AV1; "webp-anim" -> WebP_Anim; "png-seq"/"pngseq" -> PNG_Sequence; "h264"/"avc"/"mp4-h264" -> MP4_H264. All set Options::export_format to the same index as their documented counterpart.
- **source**: `src/media/media_format.cpp:78-89 (kAliases), :27 ("webm-vp9" canonical token); dispatched from src/cli/cli.cpp:448`
- **notes**: Matching is exact and case-sensitive, so --export-format AVIF or WEBM errors out even though the lowercase form is valid. The inventory records none of this.
- **tests**: `cli: export-format resolves tokens and aliases`

### 20. --export-format <mp4-hevc-alpha\|hevc\|hevc-alpha\|mp4-hevc\|safari>

- **id**: `export-format-mp4-hevc-alpha` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; MediaSink::ParseToken compares the token case-sensitively against kTable then kAliases then the literal "safari"
- **disabled when**: a tool subcommand flag is present
- **effect**: Resolves to MediaSink::Format::MP4_HEVC_Alpha, so Options::export_format = 6 and the export writes an MP4 HEVC-with-alpha file (muxer "mp4", extension ".mp4", label "MP4 HEVC alpha (video, alpha, Safari)"). This is a SEVENTH output format that is reachable ONLY through these five undocumented tokens - none of the six names printed by --help or by the --export-format error string select it.
- **source**: `src/media/media_format.cpp:64 (kTable row), :86-88 (hevc/hevc-alpha/mp4-hevc aliases), :154 ("safari" special case); dispatched from src/cli/cli.cpp:448`
- **notes**: The existing export-format entry folds "all six accepted tokens" into one record and asserts the accepted set is owned by ParseToken, but ParseToken accepts 20 tokens and one whole extra format. A user reading the inventory cannot discover MP4 HEVC alpha at all.
- **tests**: `cli: export-format resolves tokens and aliases`

### 21. --export-fps <N>

- **id**: `export-fps`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applies to an --export run
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_fps = ParseIntOrZero(value), default 60
- **source**: `src/cli/cli.cpp:340`
- **notes**: No range validation; garbage input becomes 0.
- **tests**: `cli: Parse handles the full export option set`

### 22. --export-keyframe-interval <N>

- **id**: `export-keyframe-interval`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; video formats only, ignored by PNG and WebP
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_keyframe_interval = ParseIntOrZero(value); 0 (default) means one keyframe per second
- **source**: `src/cli/cli.cpp:342`
- **tests**: `cli: Parse handles the full export option set`

### 23. --export-loop-count <N>

- **id**: `export-loop-count`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applies to an --export run
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_loop_count = N when N >= 1, else falls back to 1: capture N loops of the animation
- **source**: `src/cli/cli.cpp:353`
- **notes**: Clamped option; note the help text's "0 (default) = capture until animation ends" cannot be expressed because 0 is rewritten to 1.
- **tests**: `cli: clamped int options keep their floors`

### 24. --export-max-frames <N>

- **id**: `export-max-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applies to an --export run
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_max_frames = N when N >= 0, else falls back to 0: stop the export after N captured frames
- **source**: `src/cli/cli.cpp:349`
- **notes**: Clamped option: negative or unparsable input silently becomes the fallback 0.
- **tests**: `cli: clamped int options keep their floors`

### 25. --export-no-hw

- **id**: `export-no-hw`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_prefer_hardware = false: disable NVENC, fall back to software libaom (affects avif + webm-av1; no-op for webm-vp9)
- **source**: `src/cli/cli.cpp:300`
- **tests**: `cli: Parse handles the full export option set`

### 26. --export-quality <0-100>

- **id**: `export-quality`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applies to an --export run
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_quality = ParseIntOrZero(value), default 60, mapped downstream to codec CRF
- **source**: `src/cli/cli.cpp:341`
- **notes**: The 0-100 bound in the help text is not enforced by the parser.
- **tests**: `cli: Parse handles the full export option set`

### 27. --export-size <WxH>

- **id**: `export-size`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; both components must be non-negative
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_width/export_height set (scale before encode; 0x0 = native). Malformed input errors "--export-size expects 'WxH' with non-negative ints" and aborts with rc 1
- **source**: `src/cli/cli.cpp:384`
- **notes**: Special handler registered at cli.cpp:546.
- **tests**: `cli: Parse handles the full export option set`

### 28. --export-sw

- **id**: `export-sw`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::export_prefer_hardware = false, identical to --export-no-hw
- **source**: `src/cli/cli.cpp:301`
- **notes**: Undocumented alias: not present in the kUsage --help text.
- **tests**: `cli: Parse accepts the export software alias`, `cli: help returns false without error`

### 29. --extract-qpro <dir>

- **id**: `extract-qpro`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::extract_qpro_dir = value
- **source**: `src/cli/cli.cpp:312`
- **notes**: Undocumented in --help. Distinct from the --extract-qpro-json tool subcommand: ParseToolCommand compares tokens for exact equality, so this flag does not trigger the tool path.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: ParseToolCommand qpro-json output defaults unless a non-flag follows`
- **audit correction**: Effect is only "Options::extract_qpro_dir = value", omitting that this flag is one of the twelve gates in WantsQproCliMode and therefore turns the whole process into a one-shot that exits before the render loop. -> Options::extract_qpro_dir = value. Being non-empty makes WantsQproCliMode return true (main.cpp:139-145), so after boot main.cpp:429 calls RunQproCliAndExit: RunQproCliMode sets the hue scope, finds no one-shot flag, fills QproExtract::Options{game_dir, out_dir = this value, optional fps/parts/only} and runs the batch QproExtract::Run, logging "ERROR: %s" on failure; then the engine stack is shut down, the GUI thread stopped, and the process returns rc 0 unconditionally (main.cpp:183-194, :331-337).

### 30. --filter

- **id**: `filter`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::filter_enabled = true: enable the AFP layer filter at startup (debug viewer F7, afp-core set-filter 0x032)
- **source**: `src/cli/cli.cpp:291`
- **tests**: `cli: Parse handles flags and string options`

### 31. --fps <N>

- **id**: `fps`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; N must be 1..1000
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::render_fps = N (live render / animation-tick rate). Out-of-range sets err "--fps out of range (1..1000), got '<v>'" and aborts parsing with exit rc 1
- **source**: `src/cli/cli.cpp:362`
- **notes**: Ranged option; the out-of-range rejection is a distinct observable outcome of the same flag.
- **tests**: `cli: fps and qpro-fps enforce ranges`

### 32. --game-dir <path>

- **id**: `game-dir`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::game_dir = value; main.cpp prefers it over settings.ini's game_dir as initial_dir and boots directly, skipping the GUI Setup screen
- **source**: `src/cli/cli.cpp:305`
- **tests**: `cli: Parse handles flags and string options`, `cli: Parse rejects a missing value`

### 33. --goto-label <l>

- **id**: `goto-label`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::goto_label = value
- **source**: `src/cli/cli.cpp:311`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles flags and string options`, `cli: help returns false without error`
- **audit correction**: Effect is the bare "Options::goto_label = value" with no consumer, so the flag looks dead; it actually drives a one-shot afp command and gates the CLI autopilot's readiness. -> Options::goto_label = value. A non-empty value logs "--goto-label='%s' -> will goto after IFS load" (render_loop.cpp:86-87) and sets AutopilotConfig::want_goto_label (render_loop.cpp:299); once the clip is live the autopilot fires post_goto_label exactly once and the loop posts AfpCmd::GotoLabel{name = cli.goto_label} (render_loop.cpp:262-264, src/loop/cli_autopilot.cpp:37-39). Downstream readiness gates on label_playback_active && active_label == goto_label (render_loop.cpp:101-103), so a label that never becomes active stalls the autopilot's ready condition.

### 34. --headless

- **id**: `headless`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::headless = true (skip window creation entirely; run only the init sequence and exit). Also pre-scanned in main.cpp ParseCliOrReport to suppress the error MessageBox
- **source**: `src/cli/cli.cpp:286`
- **tests**: `cli: Parse handles flags and string options`

### 35. --headless with no --game-dir

- **id**: `headless-without-game-dir-exits-1` *(audit)*
- **input**: cli-arg
- **precondition**: --headless set and Options::game_dir empty, so main.cpp never posts the initial BootGame command
- **disabled when**: --game-dir is supplied (a boot request is posted and the normal boot path runs)
- **effect**: WaitForFirstBoot takes its first no-boot-command iteration, logs "--headless without --game-dir: nothing to do, exiting.", calls Log::Shutdown() and returns with exit_rc = 1, so the process exits with rc 1 almost immediately. A settings.ini game_dir does NOT rescue this: the boot request at main.cpp:370-371 is posted only when cli.game_dir is non-empty.
- **source**: `src/main.cpp:308-315 (message and exit_rc = 1), :370-371 (boot posted only for cli.game_dir), :420`
- **notes**: The headless entry describes only "skip window creation entirely; run only the init sequence and exit" and never mentions this hard --game-dir dependency or the non-zero rc.
- **tests**: `cli: Parse handles flags and string options`, `cli: Parse rejects a missing value`

### 36. --help

- **id**: `help-long`
- **input**: cli-arg
- **precondition**: no tool subcommand present earlier in argv (main.cpp:400 short-circuits before Cli::Parse)
- **disabled when**: a tool subcommand flag is present; ToolCommands::Run executes and the process returns before Cli::Parse is reached
- **effect**: Cli::PrintUsage() writes kUsage to stdout, Parse returns false with empty err, main.cpp ParseCliOrReport exits the process with rc 0
- **source**: `src/cli/cli.cpp:639`
- **notes**: Scanned first in the per-argument loop, so it wins over any other option that appears later in argv.
- **tests**: `cli: help returns false without error`
- **audit correction**: The note "Scanned first in the per-argument loop, so it wins over any other option that appears later in argv" is misleading in two provable ways: an EARLIER erroring option aborts Parse before --help is reached, and an earlier value-taking flag swallows --help as its value. -> --help wins only over later options in an otherwise valid command line. `--bogus --help` returns the error "Unknown argument: --bogus" with rc 1 and prints no usage (cli.cpp:643-651 runs before the next iteration's help check); `--fps 0 --help` returns the range error; and `--ifs --help` sets startup_ifs = "--help" via NextArg (cli.cpp:190) so no usage is printed and the process boots normally. The same three qualifiers apply to help-short and help-slash.

### 37. -h

- **id**: `help-short`
- **input**: cli-arg
- **precondition**: no tool subcommand present earlier in argv
- **disabled when**: a tool subcommand flag is present
- **effect**: Identical to --help: PrintUsage() then Parse returns false, process exits rc 0
- **source**: `src/cli/cli.cpp:639`
- **notes**: Separate accepted token in the same comparison as --help.
- **tests**: none

### 38. /?

- **id**: `help-slash`
- **input**: cli-arg
- **precondition**: no tool subcommand present earlier in argv
- **disabled when**: a tool subcommand flag is present
- **effect**: Identical to --help: PrintUsage() then Parse returns false, process exits rc 0
- **source**: `src/cli/cli.cpp:639`
- **notes**: Windows-style help token accepted alongside --help/-h.
- **tests**: none

### 39. --hide <path>

- **id**: `hide`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Appends Options::SlotOverride{path, visible = false, bitmap = ""} to slot_overrides, making that clip slot invisible
- **source**: `src/cli/cli.cpp:523`
- **notes**: Special handler registered at cli.cpp:557. Repeatable.
- **tests**: `cli: variant and hide build slot overrides`
- **audit correction**: Same missing precondition: the override is applied only through ApplyCliOverrides after a successful startup-IFS mount. -> Adds SlotOverride{path, visible = false, bitmap = ""}; applied at boot.cpp:173-190 only when ApplyCliOverrides runs (main.cpp:381, successful MountAndLoadIfs) and only when ActiveIfs() is non-empty. bitmap_override lands as false because the bitmap is empty, so only visibility changes.

### 40. --hide-sublayer <path>

- **id**: `hide-sublayer`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Appends Options::SubLayerOverride{path, visible = false} to sublayer_overrides: force a named child sub-clip and its same-name siblings invisible
- **source**: `src/cli/cli.cpp:530`
- **notes**: Special handler registered at cli.cpp:558. Repeatable.
- **tests**: `cli: sublayer overrides record visibility`
- **audit correction**: Same missing precondition: applied only via ApplyCliOverrides after a successful startup-IFS mount. -> Adds SubLayerOverride{path, visible = false}; applied by `App::Global().SetSublayerOverride(active, ov.path, ov.visible)` at boot.cpp:191-192, reached only from main.cpp:381 after MountAndLoadIfs succeeds and only when ActiveIfs() is non-empty.

### 41. --ifs <path>

- **id**: `ifs`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::startup_ifs = value: load this IFS at startup and skip the GUI picker (main.cpp MountAndLoadIfs then ApplyCliOverrides)
- **source**: `src/cli/cli.cpp:307`
- **tests**: `cli: Parse handles flags and string options`

### 42. --ifs2 <path>

- **id**: `ifs2`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; only used when --swap-after-frames is non-zero
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::swap_ifs = value: second IFS loaded at the hot-swap point
- **source**: `src/cli/cli.cpp:308`
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`

### 43. the nine kIntOpts flags given a negative, junk-suffixed, or non-numeric value

- **id**: `int-opt-unvalidated-value` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; the flag is one of --continuous-loop, --swap-after-frames, --exit-after-frames, --export-fps, --export-quality, --export-keyframe-interval, --submonitor-loop-frames, --submonitor-dwell-frames, --submonitor-fade-frames
- **disabled when**: never; unlike kClampedIntOpts and kRangedIntOpts this family performs no validation at all
- **effect**: ParseIntOrZero ignores the std::from_chars error code and the unconsumed tail, so `--export-fps -30` stores -30, `--exit-after-frames 12abc` stores 12, `--export-quality abc` stores 0 (silently discarding the struct default 60), and `--export-fps +60` stores 0 because from_chars rejects a leading '+'. No error is ever raised and parsing continues.
- **source**: `src/cli/cli.cpp:194-198 (ParseIntOrZero), :581-589 (kIntOpts assignment at :586)`
- **notes**: The inventory notes "garbage becomes 0" on three of the nine entries and never mentions that negatives are accepted, that the leading '+' fails, or that a numeric prefix is silently truncated. --mc-name-type (cli.cpp:440) and --screenshot-frames (cli.cpp:496) share ParseIntOrZero and the same leniency.
- **tests**: `cli: clamped int options keep their floors`, `cli: screenshot-frames parses permissively`

### 44. --mc-name-type <0\|1>

- **id**: `mc-name-type`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; help says it needs --show-mc-names
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::mc_name_type = 1 when the value parses non-zero, else 0 (F6 MC NAME TYPE: 0 = at clip position, 1 = fixed column)
- **source**: `src/cli/cli.cpp:437`
- **notes**: Special handler registered at cli.cpp:550. Never errors: any unparsable value collapses to 0.
- **tests**: `cli: clamped int options keep their floors`

### 45. any value-taking flag typed as the last argv token

- **id**: `missing-value`
- **input**: cli-arg
- **precondition**: the flag is the final token so c.i+1 >= argc
- **effect**: NextArg sets err to "Missing value for <flag>", Parse returns false; main.cpp logs, MessageBoxA on GUI runs, exits rc 1
- **source**: `src/cli/cli.cpp:185`
- **notes**: One entry covering every string/int/special option's missing-value path.
- **tests**: none

### 46. launching with an empty command line

- **id**: `no-arguments-at-all` *(audit)*
- **input**: cli-arg
- **precondition**: argc == 1 (only argv[0])
- **effect**: ParseToolCommand returns ToolKind::None, the Parse loop body never executes and Parse returns true with a fully default Options; main.cpp then takes initial_dir from settings.ini's game_dir, starts the GUI thread, posts NO boot request (cli.game_dir is empty) and waits in WaitForFirstBoot for the GUI Setup screen to post one. This is the only path into the GUI Setup screen.
- **source**: `src/cli/cli.cpp:637 (loop starts at c.i = 1), :653; src/main.cpp:413-419`
- **notes**: The inventory records the analogous no-tool-command gate but never the no-flags-at-all baseline, even though it is the default launch every GUI user takes.
- **tests**: none

### 47. --no-gui

- **id**: `no-gui`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::no_gui = true (render window without the ImGui control panel). Also pre-scanned in main.cpp:233 so argument errors log instead of opening a MessageBox
- **source**: `src/cli/cli.cpp:287`
- **tests**: `cli: Parse handles flags and string options`, `cli: ParseToolCommand keeps the historical priority order and value rule`, `cli: ParseToolCommand qpro-json output defaults unless a non-flag follows`

### 48. --no-gui with no --game-dir

- **id**: `no-gui-without-game-dir-exits-1` *(audit)*
- **input**: cli-arg
- **precondition**: --no-gui set (StartGuiIfWanted returns false, so have_gui is false) and Options::game_dir empty
- **disabled when**: --game-dir is supplied; or the GUI thread is running, which can post its own boot request
- **effect**: Same WaitForFirstBoot branch fires on the !have_gui side, logging "no GUI and no --game-dir: nothing to boot, exiting.", then Log::Shutdown() and exit_rc = 1. The same branch also catches a normal GUI run where GuiThread::Start failed.
- **source**: `src/main.cpp:308-315 (the !have_gui alternative message), :354-359 (StartGuiIfWanted)`
- **notes**: Also unrecorded: after a boot request that FAILS, main.cpp:301-307 exits rc 1 when there is no GUI to retry from, whereas a GUI run stays alive to retry.
- **tests**: `cli: Parse handles flags and string options`, `cli: Parse rejects a missing value`, `cli: ParseToolCommand keeps the historical priority order and value rule`, `cli: ParseToolCommand qpro-json output defaults unless a non-flag follows`

### 49. launching with no tool subcommand at all

- **id**: `no-tool-command`
- **input**: cli-arg
- **precondition**: none of the six tool flags appear with a following value token
- **effect**: ParseToolCommand returns a default ToolCommand with ToolKind::None; ToolCommands::Run's None/default branch returns 0 and main.cpp continues into InstallProcessDiagnostics + Cli::Parse + GUI boot instead of exiting
- **source**: `src/tool_commands.cpp:116`
- **notes**: This is the gate that makes every option flag above reachable; recorded once rather than repeated per flag.
- **tests**: none

### 50. --profile <slug>

- **id**: `profile`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::game_profile = value: select a GameProfile (iidx33, sdvx7, ...), overriding settings.ini
- **source**: `src/cli/cli.cpp:306`
- **tests**: `cli: Parse handles flags and string options`

### 51. --qpro-back-composite <value>

- **id**: `qpro-back-composite`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_back_composite = value
- **source**: `src/cli/cli.cpp:325`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 52. --qpro-back-one <value>

- **id**: `qpro-back-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_back_one = value
- **source**: `src/cli/cli.cpp:317`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 53. --qpro-body-one <value>

- **id**: `qpro-body-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_body_one = value
- **source**: `src/cli/cli.cpp:316`
- **notes**: Undocumented in --help. One of the seven per-category single-part selectors.
- **tests**: `cli: Parse handles every string-valued qpro flag`

### 54. --qpro-clip-one <value>

- **id**: `qpro-clip-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_clip_one = value
- **source**: `src/cli/cli.cpp:322`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`
- **audit correction**: Control and effect omit the value's internal format; "<value>" hides that the argument is a colon-separated pair. -> control: "--qpro-clip-one <ifs>[:<clip>]". The value is split at the LAST ':' (rfind), giving ifs = text before it and clip = text after it; with no ':' the whole value is the ifs and clip is empty. It then calls QproExtract::ClipOne(engine, d3d, game_dir, ifs, clip) as a one-shot and the process exits rc 0 (main.cpp:154-159).

### 55. --qpro-dump <ifs>

- **id**: `qpro-dump`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_dump_ifs = value
- **source**: `src/cli/cli.cpp:315`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`
- **audit correction**: Effect "Options::qpro_dump_ifs = value" omits the path resolution rule and the one-shot behaviour. -> Options::qpro_dump_ifs = value. In RunQproOneShot (lowest precedence of the eleven one-shot flags) a RELATIVE value is resolved to <game_dir>/data/graphic/<value> while an absolute path is used as-is, then QproExtract::DumpIfs(engine, path) runs and RunQproCliAndExit exits the process with rc 0 (main.cpp:172-176).

### 56. --qpro-face-one <value>

- **id**: `qpro-face-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_face_one = value
- **source**: `src/cli/cli.cpp:321`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 57. --qpro-fps <N>

- **id**: `qpro-fps`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; N must be 1..240
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_fps = N. Out-of-range sets err "--qpro-fps out of range (1..240), got '<v>'" and aborts parsing with exit rc 1
- **source**: `src/cli/cli.cpp:363`
- **notes**: Undocumented in --help. Ranged option.
- **tests**: `cli: fps and qpro-fps enforce ranges`
- **audit correction**: Effect "Options::qpro_fps = N" omits both gates on the value: it is applied only when > 0 and only inside the batch qpro CLI path. -> Options::qpro_fps = N after the 1..240 range check. It is consumed only at main.cpp:189 (`if (cli.qpro_fps > 0) o.fps = cli.qpro_fps;`) inside RunQproCliMode, which runs only when WantsQproCliMode is true, and it is skipped entirely when a one-shot/composite flag wins at main.cpp:185. Passing it alone changes nothing.

### 58. --qpro-hair-one <value>

- **id**: `qpro-hair-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_hair_one = value
- **source**: `src/cli/cli.cpp:320`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 59. --qpro-hand-composite <value>

- **id**: `qpro-hand-composite`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_hand_composite = value
- **source**: `src/cli/cli.cpp:323`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 60. --qpro-hand-one <value>

- **id**: `qpro-hand-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_hand_one = value
- **source**: `src/cli/cli.cpp:319`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 61. --qpro-head-composite <value>

- **id**: `qpro-head-composite`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_head_composite = value
- **source**: `src/cli/cli.cpp:324`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 62. --qpro-head-one <value>

- **id**: `qpro-head-one`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_head_one = value
- **source**: `src/cli/cli.cpp:318`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`

### 63. --qpro-parts / --qpro-only / --qpro-fps / --qpro-no-hue-scope without a qpro mode flag

- **id**: `qpro-modifier-flags-inert-alone` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; none of the twelve flags listed in WantsQproCliMode is set
- **disabled when**: always inert in this combination - WantsQproCliMode returns false so RunQproCliMode is never called
- **effect**: These four are modifiers, not modes: WantsQproCliMode tests only extract_qpro_dir, qpro_dump_ifs and the nine one-shot/composite strings, so passing them alone parses successfully and then does nothing at all - the process continues into the normal GUI/render path. They take effect only inside RunQproCliMode: SetHueScopeEnabled(!qpro_no_hue_scope), fps override only when qpro_fps > 0, ParseQproPartsCsv, ParseQproOnlyCsv - and the last two apply only on the batch --extract-qpro path, since RunQproOneShot returns before them.
- **source**: `src/main.cpp:139-145 (WantsQproCliMode), :183-194 (RunQproCliMode; :185 early return, :189 fps gate, :190-191 csv gates)`
- **notes**: Four inventory entries (qpro-parts, qpro-only, qpro-fps, qpro-no-hue-scope) claim an unconditional effect and list disabled_when as only "a tool subcommand flag is present".
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: Parse handles the qpro hue-scope switch`, `cli: fps and qpro-fps enforce ranges`

### 64. --qpro-no-hue-scope

- **id**: `qpro-no-hue-scope`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_no_hue_scope = true
- **source**: `src/cli/cli.cpp:293`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles the qpro hue-scope switch`, `cli: help returns false without error`
- **audit correction**: Effect is the bare "Options::qpro_no_hue_scope = true" with no consumer named. -> Options::qpro_no_hue_scope = true, consumed only at main.cpp:184 as QproExtract::SetHueScopeEnabled(!cli.qpro_no_hue_scope) at the top of RunQproCliMode. It therefore affects both the one-shot and batch qpro paths, and does nothing at all unless some other flag puts the process into qpro CLI mode.

### 65. passing more than one qpro one-shot flag at once

- **id**: `qpro-oneshot-precedence` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; two or more of --qpro-back-composite, --qpro-hand-composite, --qpro-head-composite, --qpro-clip-one, --qpro-back-one, --qpro-head-one, --qpro-hand-one, --qpro-hair-one, --qpro-face-one, --qpro-body-one, --qpro-dump are non-empty; a game dir must have booted
- **disabled when**: never - no error or warning is emitted for the ignored flags
- **effect**: RunQproOneShot is a single if/else-if chain, so exactly ONE flag runs and the rest are silently discarded, in this fixed precedence regardless of argv order: back-composite > hand-composite > head-composite > clip-one > back-one > head-one > hand-one > hair-one > face-one > body-one > qpro-dump. Whichever wins renders its one-shot and RunQproCliAndExit shuts the engine stack down and returns rc 0 without ever entering the render loop. If none is set, RunQproOneShot returns false and the batch QproExtract::Run path (--extract-qpro) executes instead.
- **source**: `src/main.cpp:147-181 (RunQproOneShot), :183-194 (RunQproCliMode), :331-337 (RunQproCliAndExit), :429 (gate)`
- **notes**: The inventory has all eleven flags but records each effect as a bare "Options::qpro_x = value", so this mutual exclusion and its precedence order are entirely absent - and so is the fact that any one of them turns the process into a one-shot that exits before the render loop.
- **tests**: none

### 66. --qpro-only <value>

- **id**: `qpro-only`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_only = value
- **source**: `src/cli/cli.cpp:314`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`
- **audit correction**: Effect "Options::qpro_only = value" omits the token grammar and the resolution/log behaviour. -> Options::qpro_only = value, a comma list of <category-prefix>_<index> tokens (prefix matched against QproDll::Prefix, index the decimal digits after the first '_'). Each resolved token sets part_sel.sel[cat][idx] = 1; a token whose prefix or index does not resolve logs "--qpro-only: '%s' not resolved (ignored)"; the parser then logs "--qpro-only: %d part(s) selected". Applied only at main.cpp:191 on the --extract-qpro batch path (main.cpp:113-137).

### 67. --qpro-parts <value>

- **id**: `qpro-parts`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::qpro_parts = value
- **source**: `src/cli/cli.cpp:313`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: help returns false without error`
- **audit correction**: Effect "Options::qpro_parts = value" omits the value grammar, the reset-then-set semantics, the ignore-with-log behaviour for unknown tokens, and the fact that it only applies on the --extract-qpro batch path. -> Options::qpro_parts = value, a comma list drawn from head\|hand\|hair\|face\|body\|back. ParseQproPartsCsv first clears all six selectors to false and then enables the named ones, so passing the flag narrows the extract; an unrecognised non-empty token logs "--qpro-parts: unknown category '%s' (ignored)" and is skipped. Applied only at main.cpp:190, inside RunQproCliMode and only after RunQproOneShot declines (main.cpp:92-111).

### 68. --render-size <WxH>

- **id**: `render-size`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; value must parse as WxH with both in 64..8192
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::render_width/render_height set (window + offscreen RT native resolution). Malformed input errors "--render-size expects 'WxH' with positive ints"; out-of-range errors "--render-size out of range (need 64..8192 each)"; both abort with exit rc 1
- **source**: `src/cli/cli.cpp:366`
- **notes**: Special handler registered at cli.cpp:545. Presence also drives main.cpp's size_explicit flag on the BootGame command.
- **tests**: `cli: render-size validates format and range`

### 69. --root-loop <hold\|force>

- **id**: `root-loop`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; value must be exactly "hold" or "force"
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::root_loop_mode = 0 for "hold" (mount once, root plays once then HOLDs while nested children free-run) or 1 for "force" (ForceReplay + continuous-loop flag sequence). Anything else errors "--root-loop expects 'hold' or 'force'" and aborts with rc 1
- **source**: `src/cli/cli.cpp:422`
- **notes**: Special handler registered at cli.cpp:549. Struct default is -1 meaning "use settings.ini".
- **tests**: `cli: root-loop accepts hold and force only`
- **audit correction**: Effect claims the mode is set, without the gate that ApplyCliOverrides only runs after a successful startup-IFS mount, so --root-loop is silently ignored without --ifs. -> Correct as far as parsing goes (root_loop_mode 0/1, default -1), but the application step is `if (opts.root_loop_mode == 0) SetRootLoopMode(Hold) else if (== 1) SetRootLoopMode(Force)` in ApplyCliOverrides (boot.cpp:165-168). ApplyCliOverrides is called from exactly one place, main.cpp:381, inside `if (MountAndLoadIfs(startup_ifs, ...))`. Without --ifs, or when the mount fails ("startup IFS mount failed ... CLI overrides skipped"), the flag has no effect and settings.ini's root_loop_force stands.

### 70. --scale <factor>

- **id**: `scale`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; factor must parse as float in (0, 16]
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::master_scale = factor: master scale applied to the AFP stream before export starts. Invalid input errors "--scale expects a positive factor (e.g. 1.5)" and aborts with rc 1
- **source**: `src/cli/cli.cpp:398`
- **notes**: Special handler registered at cli.cpp:547.
- **tests**: `cli: scale and afp-speed validate as floats`

### 71. --screenshot-frames <f1,f2,...>

- **id**: `screenshot-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Each comma token > 0 is appended to Options::screenshot_frames; the backbuffer is dumped as PNG at each listed 1-indexed frame to <--screenshot-prefix><frame>.png
- **source**: `src/cli/cli.cpp:492`
- **notes**: Special handler registered at cli.cpp:554. Never errors: non-positive or unparsable tokens are silently dropped. Repeating the flag appends more frames.
- **tests**: `cli: screenshot-frames parses permissively`

### 72. --screenshot-prefix <p>

- **id**: `screenshot-prefix`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; only observable together with --screenshot-frames
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::screenshot_prefix = value (default "screenshots/auto_f"); output files become <prefix><frame>.png
- **source**: `src/cli/cli.cpp:326`
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`

### 73. --seek-frame <N>

- **id**: `seek-frame`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applied after --ifs (and --animation) load
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::seek_frame = N when N >= 0, else -1 (no seek): seek the master clip to absolute frame N and play (CAfpViewerScene TIME seek, afp_mc_control 0xF08), pausing on seek
- **source**: `src/cli/cli.cpp:358`
- **notes**: Clamped option with a -1 fallback meaning "no seek".
- **tests**: `cli: clamped int options keep their floors`

### 74. --show-mc-names

- **id**: `show-mc-names`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::show_mc_names = true: enable the MC-name overlay (debug viewer F3), enumerating the master's child clips into the GUI list
- **source**: `src/cli/cli.cpp:292`
- **notes**: Gates --mc-name-type per the help text.
- **tests**: `cli: Parse handles flags and string options`, `cli: clamped int options keep their floors`

### 75. --show-sublayer <path>

- **id**: `show-sublayer`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Appends Options::SubLayerOverride{path, visible = true} to sublayer_overrides: force a named child sub-clip visible
- **source**: `src/cli/cli.cpp:537`
- **notes**: Special handler registered at cli.cpp:559. Repeatable.
- **tests**: `cli: sublayer overrides record visibility`
- **audit correction**: Same missing precondition: applied only via ApplyCliOverrides after a successful startup-IFS mount. -> Adds SubLayerOverride{path, visible = true}; applied by SetSublayerOverride at boot.cpp:191-192, reached only from main.cpp:381 after a successful startup IFS mount with a non-empty ActiveIfs().

### 76. --render-size / --export-size value format

- **id**: `size-pair-lenient-parse` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: ParseSizePair splits at the FIRST lowercase 'x' and lets from_chars ignore trailing text, so "1920x1080junk" and "1920x1080x2" are ACCEPTED as 1920x1080, while "1920X1080" (capital X) fails with "--render-size expects 'WxH' with positive ints" and "x1080" fails because the left side is empty. "1920x-5" parses to h = -5 and is then rejected by the <= 0 check (--render-size) or the < 0 check (--export-size).
- **source**: `src/cli/cli.cpp:206-220 (ParseSizePair), :371 (--render-size validation), :389 (--export-size validation)`
- **notes**: Both existing entries state only the range rules, so the case-sensitivity of the separator and the accepted trailing junk are unrecorded.
- **tests**: `cli: Parse handles the full export option set`, `cli: render-size validates format and range`

### 77. --start-paused

- **id**: `start-paused`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::start_paused = true: live preview starts paused (stream speed 0, the debug viewer RETURN+SHIFT pause)
- **source**: `src/cli/cli.cpp:290`
- **tests**: `cli: Parse handles flags and string options`

### 78. --submonitor-clip <path>

- **id**: `submonitor-clip`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; only meaningful with --submonitor-frames
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_clip = value, replacing the default "subbg_usr/bg_usr" placeholder clip target
- **source**: `src/cli/cli.cpp:331`
- **tests**: `cli: submonitor options parse`

### 79. --submonitor-dwell-frames <N>

- **id**: `submonitor-dwell-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; submonitor slideshow paths
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_dwell_frames = ParseIntOrZero(value), default 720: hold length per frame
- **source**: `src/cli/cli.cpp:344`
- **tests**: `cli: submonitor options parse`

### 80. --submonitor-fade-frames <N>

- **id**: `submonitor-fade-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; submonitor slideshow paths
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_fade_frames = ParseIntOrZero(value), default 120: fade in/out length
- **source**: `src/cli/cli.cpp:345`
- **tests**: `cli: Parse handles the submonitor option set`

### 81. --submonitor-fade-in-label <l>

- **id**: `submonitor-fade-in-label`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; used by the slideshow fade path
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_fade_in_label = value, replacing the default "fade_in"
- **source**: `src/cli/cli.cpp:332`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles the submonitor option set`, `cli: help returns false without error`

### 82. --submonitor-fade-out-label <l>

- **id**: `submonitor-fade-out-label`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; used by the slideshow fade path
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_fade_out_label = value, replacing the default "fade_out"
- **source**: `src/cli/cli.cpp:333`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles the submonitor option set`, `cli: help returns false without error`

### 83. --submonitor-frames <p1,p2,...>

- **id**: `submonitor-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; applied after the --animation switch, before --export
- **disabled when**: a tool subcommand flag is present
- **effect**: Each comma token is appended to Options::submonitor_frames: loose image files bound to the startup IFS's submonitor placeholder clip, frame i to the i-th child layer via afp ord 0x088
- **source**: `src/cli/cli.cpp:502`
- **notes**: Special handler registered at cli.cpp:555. Repeatable and additive.
- **tests**: `cli: submonitor options parse`

### 84. --submonitor-loop-frames <N>

- **id**: `submonitor-loop-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist; required by --submonitor-slideshow
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_loop_frames = ParseIntOrZero(value)
- **source**: `src/cli/cli.cpp:343`
- **notes**: Undocumented as its own entry in --help, though --submonitor-slideshow references it.
- **tests**: `cli: Parse handles the submonitor option set`

### 85. --submonitor-slideshow

- **id**: `submonitor-slideshow`
- **input**: cli-arg
- **precondition**: no tool subcommand present; help text says it needs --submonitor-loop-frames
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_slideshow = true: cross-fade CYCLE mode (oversized r3_fade 2-layer dissolve)
- **source**: `src/cli/cli.cpp:295`
- **tests**: `cli: Parse handles the submonitor option set`

### 86. --submonitor-slideshow-fade

- **id**: `submonitor-slideshow-fade`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_slideshow_fade = true: NORMAL slideshow mode (centered subbg_usr + subbg_0001 fade_in/fade_out labels, no pan)
- **source**: `src/cli/cli.cpp:297`
- **tests**: `cli: submonitor options parse`

### 87. --submonitor-swap-layers

- **id**: `submonitor-swap-layers`
- **input**: cli-arg
- **precondition**: no tool subcommand present
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::submonitor_swap_layers = true
- **source**: `src/cli/cli.cpp:296`
- **notes**: Undocumented: not present in the kUsage --help text.
- **tests**: `cli: Parse handles the submonitor option set`, `cli: help returns false without error`
- **audit correction**: Effect is the bare "Options::submonitor_swap_layers = true" with no consumer named. -> Options::submonitor_swap_layers = true swaps which of the two resolved submonitor movie-clip ids is treated as the base and which as the overlay: sm_base_mc_ = ids[1] and sm_overlay_mc_ = ids[0] instead of the default ids[0]/ids[1] (src/backend/afp_family_frame.cpp:106-107), inverting the cross-fade layer order used by the slideshow modes.

### 88. --swap-after-frames <N>

- **id**: `swap-after-frames`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist
- **disabled when**: a tool subcommand flag is present
- **effect**: Options::swap_after_frames = ParseIntOrZero(value): after N frames hot-swap to --ifs2; 0 means never
- **source**: `src/cli/cli.cpp:338`
- **tests**: `cli: Parse handles the remaining playback and diagnostic flags`

### 89. --ddr-test <in> [arc] [out.png] [frames]

- **id**: `tool-ddr-test`
- **input**: cli-arg
- **precondition**: the token must be followed by at least one more token; no --scene3d-test earlier match
- **disabled when**: --scene3d-test with a value is also present (it is matched first); or the flag is the last token in argv
- **effect**: ParseToolCommand returns ToolKind::DdrTest; ToolCommands::Run calls RunDdrTest -> DdrTest::Run(in_path, arc_path, out_path, frames) and the process exits with that rc
- **source**: `src/cli/tool_command.cpp:67`
- **notes**: Dispatched via src/tool_commands.cpp:22 and :106.
- **tests**: `cli: ParseToolCommand keeps the historical priority order and value rule`, `cli: ParseToolCommand recognizes ddr-test with defaults and explicit args`

### 90. --ddr-test positional 2: arc path

- **id**: `tool-ddr-test-arc`
- **input**: cli-arg
- **precondition**: a token exists at i+2
- **disabled when**: absent, in which case arc_path is the empty string
- **effect**: ToolCommand::arc_path = that token, passed as DdrTest::Run's second argument
- **source**: `src/cli/tool_command.cpp:30`
- **notes**: Unlike the scene3d parser this does not skip tokens beginning with '-', so a following flag would be consumed as the arc path.
- **tests**: `cli: ParseToolCommand keeps the historical priority order and value rule`, `cli: ParseToolCommand recognizes ddr-test with defaults and explicit args`

### 91. --ddr-test positional 4: frame count

- **id**: `tool-ddr-test-frames`
- **input**: cli-arg
- **precondition**: a token exists at i+4
- **disabled when**: absent, in which case frames stays at the struct default 120
- **effect**: ToolCommand::frames = ParseIntAtoiLike(token), passed as DdrTest::Run's fourth argument
- **source**: `src/cli/tool_command.cpp:32`
- **notes**: Unparsable text silently yields 0.
- **tests**: `cli: ParseToolCommand keeps the historical priority order and value rule`, `cli: ParseToolCommand recognizes ddr-test with defaults and explicit args`

### 92. --ddr-test positional 3: output PNG path

- **id**: `tool-ddr-test-out`
- **input**: cli-arg
- **precondition**: a token exists at i+3
- **disabled when**: absent, in which case out_path defaults to "ddr_out.png"
- **effect**: ToolCommand::out_path = that token, passed as DdrTest::Run's third argument
- **source**: `src/cli/tool_command.cpp:31`
- **tests**: `cli: ParseToolCommand keeps the historical priority order and value rule`, `cli: ParseToolCommand recognizes ddr-test with defaults and explicit args`

### 93. --extract-arc <path>

- **id**: `tool-extract-arc`
- **input**: cli-arg
- **precondition**: the token must be followed by at least one more token; no --scene3d-test or --ddr-test matched earlier
- **disabled when**: --scene3d-test or --ddr-test with a value is also present; or the flag is the last token in argv
- **effect**: ParseToolCommand returns ToolKind::ExtractArc; RunExtractArc calls ArcExtract::Start(in_path), then polls ArcExtract::IsRunning()/GetStatus() every 100 ms logging "progress: done/total arcs, N files written", and returns 0 on empty status.error else 2
- **source**: `src/cli/tool_command.cpp:69`
- **notes**: Handler at src/tool_commands.cpp:26; dispatched at src/tool_commands.cpp:108. Only in_path is consumed, no further positionals.
- **tests**: `cli: ParseToolCommand handles the single-path tools`, `cli: ParseToolCommand keeps the historical priority order and value rule`

### 94. --extract-customize <path>

- **id**: `tool-extract-customize`
- **input**: cli-arg
- **precondition**: the token must be followed by at least one more token; no earlier tool subcommand matched
- **disabled when**: --scene3d-test, --ddr-test, or --extract-arc with a value is also present; or the flag is the last token in argv
- **effect**: ParseToolCommand returns ToolKind::ExtractCustomize; RunExtractCustomize calls CustomizeExtract::Start(in_path), polls IsRunning()/GetStatus() every 100 ms logging "progress: done/total images", and returns 0 on empty status.error else 2
- **source**: `src/cli/tool_command.cpp:71`
- **notes**: Handler at src/tool_commands.cpp:42; dispatched at src/tool_commands.cpp:110.
- **tests**: `cli: ParseToolCommand handles the single-path tools`

### 95. --extract-qpro-json <dll> [out.json]

- **id**: `tool-extract-qpro-json`
- **input**: cli-arg
- **precondition**: the token must be followed by at least one more token; no earlier tool subcommand matched
- **disabled when**: any of --scene3d-test/--ddr-test/--extract-arc/--extract-customize with a value is also present; or the flag is the last token in argv
- **effect**: RunExtractQproJson calls QproDll::Read(in_path); on failure logs "Qpro ERROR" and returns 2, otherwise writes QproDll::ToJson output to out_path (binary+trunc; returns 2 if the file cannot be opened), logs a per-category part count and a "wrote N parts -> out (from dll)" summary, returns 0
- **source**: `src/cli/tool_command.cpp:73`
- **notes**: Handler at src/tool_commands.cpp:57; dispatched at src/tool_commands.cpp:112. Distinct from the --extract-qpro string option in Cli::Parse.
- **tests**: `cli: Parse handles every string-valued qpro flag`, `cli: ParseToolCommand qpro-json output defaults unless a non-flag follows`

### 96. --extract-qpro-json positional 2: output JSON path

- **id**: `tool-extract-qpro-json-out`
- **input**: cli-arg
- **precondition**: a token exists at i+2, is non-empty, and does not begin with '-'
- **disabled when**: absent, empty, or starts with '-', in which case out_path defaults to "2dx_qpro.json"
- **effect**: ToolCommand::out_path = that token; the JSON is written there by RunExtractQproJson
- **source**: `src/cli/tool_command.cpp:47`
- **tests**: `cli: ParseToolCommand qpro-json output defaults unless a non-flag follows`

### 97. --qpro-scan <path>

- **id**: `tool-qpro-scan`
- **input**: cli-arg
- **precondition**: the token must be followed by at least one more token; no earlier tool subcommand matched (this is the last one checked)
- **disabled when**: any other tool subcommand with a value is present; or the flag is the last token in argv
- **effect**: RunQproScan calls QproExtract::RunScan(in_path) then GetScanResult(); on non-empty error logs "QproScan ERROR" and returns 2, otherwise logs per-category counts, a per-date-group part count, and a "total N parts across M date group(s)" line, returns 0
- **source**: `src/cli/tool_command.cpp:75`
- **notes**: Handler at src/tool_commands.cpp:79; dispatched at src/tool_commands.cpp:114.
- **tests**: `cli: ParseToolCommand handles the single-path tools`

### 98. --scene3d-test <in> [out.png] [frames]

- **id**: `tool-scene3d-test`
- **input**: cli-arg
- **precondition**: the token must be followed by at least one more token (FindFlagWithValue requires i+1 < args.size()); highest priority of all tool subcommands
- **disabled when**: never once matched; the flag is ignored (falls through to Cli::Parse, which then errors "Unknown argument") when it is the last token in argv
- **effect**: ParseToolCommand returns ToolKind::Scene3dTest and main.cpp:400 calls ToolCommands::Run, which invokes Scene3dTest::Run(in_path, out_path, frames), then Log::Shutdown() and returns that rc without ever creating a window or running Cli::Parse
- **source**: `src/cli/tool_command.cpp:65`
- **notes**: Dispatched at src/tool_commands.cpp:104. Checked first, so it wins over any other tool subcommand present in the same command line.
- **tests**: none

### 99. --scene3d-test positional 3: frame count

- **id**: `tool-scene3d-test-frames`
- **input**: cli-arg
- **precondition**: a token exists at i+3 and does not begin with '-'
- **disabled when**: absent or starts with '-', in which case frames stays at the struct default 120
- **effect**: ToolCommand::frames = ParseIntAtoiLike(token), passed as the third argument to Scene3dTest::Run
- **source**: `src/cli/tool_command.cpp:58`
- **notes**: Unparsable text silently yields 0.
- **tests**: none

### 100. --scene3d-test positional 2: output PNG path

- **id**: `tool-scene3d-test-out`
- **input**: cli-arg
- **precondition**: a token exists at i+2 and does not begin with '-'
- **disabled when**: absent or starts with '-', in which case out_path defaults to "scene3d_out.png"
- **effect**: ToolCommand::out_path = that token, passed as the second argument to Scene3dTest::Run
- **source**: `src/cli/tool_command.cpp:57`
- **tests**: none
- **audit correction**: disabled_when is incomplete: an EMPTY token at i+2 does not fall back to the default, unlike the --extract-qpro-json parser. -> ParseScene3dTest tests only `args[i + 2][0] != '-'`, and indexing an empty std::string yields '\0', so an empty token at i+2 sets out_path = "" (passed straight to Scene3dTest::Run) rather than "scene3d_out.png". Only an ABSENT token or one starting with '-' reaches the default (src/cli/tool_command.cpp:57). Contrast ParseExtractQproJson at :47, which explicitly guards !args[i + 2].empty(). The same missing empty-guard applies to the frames positional at :58 and to every ParseDdrTest positional at :30-32.

### 101. any unrecognised token

- **id**: `unknown-argument`
- **input**: cli-arg
- **precondition**: token matched none of kBoolOpts/kStringOpts/kIntOpts/kClampedIntOpts/kRangedIntOpts/kSpecialOpts
- **effect**: err set to "Unknown argument: <token>", Parse returns false; main.cpp logs it, shows a MessageBoxA "573Renderer: argument error" when a GUI was wanted, exits rc 1
- **source**: `src/cli/cli.cpp:649`
- **notes**: One entry for the whole class of unknown tokens; also catches bare positional paths since there are no positionals in Cli::Parse.
- **tests**: none

### 102. any value-taking flag whose next argv token is itself a flag

- **id**: `value-flag-consumes-following-flag` *(audit)*
- **input**: cli-arg
- **precondition**: no tool subcommand present; at least one token follows the flag (so the missing-value path is not taken)
- **disabled when**: never; NextArg has no look-ahead guard of any kind
- **effect**: NextArg takes args[++c.i] unconditionally, so the following flag is swallowed as the value and is never parsed: `--ifs --no-gui` sets Options::startup_ifs = "--no-gui" and leaves no_gui false; `--ifs --help` sets startup_ifs = "--help" and no usage is printed; `--export-format --headless` produces the misleading error "--export-format expects 'avif' ... got '--headless'". Silent for all string options, an error for validated ones.
- **source**: `src/cli/cli.cpp:185-192 (NextArg), consumed by src/cli/cli.cpp:575, :585, :593, :602 and every Handle* special`
- **notes**: Distinct from the missing-value entry, which only covers the flag being the LAST argv token. Note the contrast with tool_command.cpp, where ParseScene3dTest and ParseExtractQproJson do guard against a leading '-'.
- **tests**: none

### 103. --variant <path>=<bitmap>

- **id**: `variant`
- **input**: cli-arg
- **precondition**: no tool subcommand present; a following value token must exist and contain '='
- **disabled when**: a tool subcommand flag is present
- **effect**: Appends Options::SlotOverride{path, visible = true, bitmap} to slot_overrides: swap the clip slot's bitmap and make it visible. Missing '=' errors "--variant expects <path>=<bitmap>" and aborts with rc 1
- **source**: `src/cli/cli.cpp:510`
- **notes**: Special handler registered at cli.cpp:556. Repeatable; each occurrence appends one override.
- **tests**: `cli: variant and hide build slot overrides`
- **audit correction**: Effect omits the hard precondition that slot overrides are applied only after a startup IFS mounts, and omits that they are written into the ACTIVE ifs config, creating the slot when it does not exist. -> Parsing is as recorded, but application happens in ApplyCliOverrides (boot.cpp:170-190), whose only call site is main.cpp:381 inside the successful-mount branch: it returns early if App::Global().ActiveIfs() is empty, then for each override finds the matching slot in MutConfig(active) or PUSHES A NEW ONE with default_bitmap = path, setting bitmap, visible and bitmap_override = !bitmap.empty(). Without a startup IFS that mounts, --variant does nothing.

