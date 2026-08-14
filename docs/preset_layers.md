# Scene preset layers: what a preset may contain

A scene preset exists for ONE purpose: to capture a game screen's **background
animation on its own**, with none of the game's chrome. It is not a replica of
the screen. If a title, a timer, an information bar, a song list, a difficulty
row, a player label or an instruction line is visible in a preset render, the
preset is wrong.

There is deliberately no "show UI" toggle. A preset carries background layers
and nothing else, so there is no state in which chrome can appear.

## The rule for deciding

A layer is **background** if it is artwork: it would still belong on screen with
every other layer removed and no player present.

A layer is **chrome** if it exists to tell the player something: titles, labels,
instructions, timers, counters, lists, frames, buttons, player state, prompts.

KONAMI's own naming is a hint and NOT evidence. On IIDX RED, `MUSIC_IN` sounds
like a background and is in fact the entire music-select frame; `EXPERT_IN` is
the course-select frame. The `_BG` suffix is usually artwork and the `_IN`
suffix is usually the frame, but the suffix has never been checked against the
format and must never be the reason a layer is classified.

## The procedure (mandatory before adding any layer)

1. Render the whole package to one PNG per layer:

```bash
573Renderer.exe --gc2d-sheet "<iidx-red-dir>/data/graph/sys/mselect" screenshots/sheet_red_mselect 4
```

The last argument is how many frames to SAMPLE per animation, spread across its
length, written as `anim_NAME_f<frame>.png`. It is not one frame, because a layer
can be clean early and bring chrome in later: IIDX RED's `COURSE_DECIDE` is a
plain blue flash at frame 30 and has SELECT KEY MODE and both option strips by
frame 119. A single sample classified it as clean art, and the caption only
turned up in a preset render afterwards.

2. **Look at every candidate image, at every sample.** Not the name, the image.
3. Add a row to the table below with the verdict and one line on what the image
   showed.
4. Only then add the layer to the preset.

`tools/ci/check_preset_layers.py` fails the build if a preset uses a layer that
has no row here, or that has a `chrome` verdict. It also requires every
`hidden_parts` entry to have a `chrome` row. The gate cannot tell artwork from
chrome; it can only guarantee that every layer in a preset was written down as
looked-at, which is exactly the step that was skipped when music select shipped
with its whole UI frame in it.

## Classification

Verdicts below were each read off a `--gc2d-sheet` render of that layer.

| package | layer | kind | verdict | what the render showed |
|---|---|---|---|---|
| data/graph/sys/mselect | MU10_BG | cell | background | dark blue tech panel backdrop, top and bottom bands |
| data/graph/sys/mselect | BG_SKY | cell | background | the scrolling sky band, drawn twice half a screen apart |
| data/graph/sys/mselect | MU_BG01 | animation | chrome | an empty black box outline, the info panel border |
| data/graph/sys/mselect | MUSIC_BG_LOOP | animation | chrome | MUSIC SELECT title, tagline, TIME REMAIN, INFORMATION, EFFECTOR OFF |
| data/graph/sys/mselect | MUSIC_IN | animation | chrome | the whole RED music-select frame: title, INFORMATION, OPERATION, SELECT KEY MODE, DIFFICULTY |
| data/graph/sys/mselect | OPT_ICON_1P | animation | chrome | the per-player option icon strip |
| data/graph/sys/mselect | OPT_ICON_2P | animation | chrome | the per-player option icon strip |
| data/graph/sys/mselect | DIFF_BAR | cell | chrome | the difficulty bar graphic |
| data/graph/sys/mselect | DIFFICUL | cell | chrome | the word DIFFICULTY |
| data/graph/sys/mode | MODE_BG_LOOP | animation | background | the big dial, the grid and the tech panels, with chrome baked in |
| data/graph/sys/mode | FRAME | animation | chrome | the screen border, baked into MODE_BG_LOOP |
| data/graph/sys/mode | FRAME_GLOW | animation | chrome | the border glow |
| data/graph/sys/mode | FRAME_GLOW2 | animation | chrome | the second border glow |
| data/graph/sys/mode | CTXT | animation | chrome | the caption text line |
| data/graph/sys/mode | MODE_T | cell | chrome | the MODE SELECT title |
| data/graph/sys/mode | SETSUMEI | cell | chrome | the Japanese instruction line |
| data/graph/sys/mode | T_REMAIN | cell | chrome | the TIME REMAIN label |
| data/graph/sys/mode | INFOWAKU | cell | chrome | the INFORMATION bar |
| data/graph/sys/mode | FRAME4 | cell | chrome | a frame corner piece |
| data/graph/sys/mode | FRAME6 | cell | chrome | a frame corner piece |
| data/graph/sys/mode | M_KAKKO | cell | chrome | the bracket around the mode name |
| data/graph/sys/mode | MODE_NAME | animation | chrome | the selected mode's name |
| data/graph/sys/mode | WHICH | animation | chrome | the left/right selection arrows |
| data/graph/sys/card | CARD_BG | animation | background | the cabinet illustration on a blue or red field with flavour text |
| data/graph/sys/card | T_REMAIN | cell | chrome | the TIME REMAIN chip, baked into RED's CARD_BG |
| data/graph/sys/card | PLAYER1 | animation | chrome | the 1P player panel |
| data/graph/sys/card | PLAYER2 | animation | chrome | the 2P player panel |
| data/graph/sys/card | P1_CARD_IN | animation | chrome | the 1P insert-card prompt |
| data/graph/sys/card | P2_CARD_IN | animation | chrome | the 2P insert-card prompt |
| data/graph/sys/card | 1P_START_IN | animation | chrome | the 1P press-start prompt |
| data/graph/sys/card | 2P_START_IN | animation | chrome | the 2P press-start prompt |
| data/graph/sys/title | TITLE | animation | background | the 1736 frame boot sequence: the BEATMANIA 2DX header rule, the genre words fading in and out, and the RED logo settling into the red band. Nests TITLE_TAIKI as a child for its own tail |
| data/graph/sys/title | TITLE_TAIKI | animation | background | the standby logo loop the game swaps to once TITLE ends, RED wordmark on the red band |
| data/graph/sys/title | LOGIN | animation | background | the RED logo reveal, with a header and footer bar baked in |
| data/graph/sys/title | OP_BG_U | cell | chrome | the BEATMANIA 2DX header bar and its red rule |
| data/graph/sys/title | OP_BG_D | cell | chrome | the IIDXRED BOOT_ footer bar and its red rule |
| data/graph/sys/title_10th | LOGIN | animation | background | the 10th style logo mark |
| data/graph/sys/title_10th | BE2DX10 | cell | chrome | the BEATMANIA 2DX 10TH STYLE title line |
| data/graph/sys/title_10th | TXT | cell | chrome | the tagline under the logo |
| data/graph/sys/dan_e | BG | animation | background | RED's red hexagon honeycomb field |
| data/graph/sys/dan_e | DAN_BG | animation | background | IIDX 10's blue gear-and-ring backdrop |
| data/graph/sys/dan_e | DAN_IN | animation | chrome | the class course frame |
| data/graph/sys/dan_e | DAN_NAME | animation | chrome | the course name plate |
| data/graph/sys/expert | EX_BG | animation | background | IIDX 10's blue circuit-trace backdrop |
| data/graph/sys/expert | EXPERT_BG | animation | background | RED's red concentric-ring backdrop |
| data/graph/sys/expert | EXPERT_IN | animation | chrome | the COURSE SELECT frame |
| data/graph/sys/expert | COURSE_DECIDE | animation | background | the blue flash the outro swaps in; clean early, but it brings in the key-mode and option chrome later in its timeline |
| data/graph/sys/expert | SKM | cell | chrome | the SELECT KEY MODE caption, drawn from inside COURSE_DECIDE |
| data/graph/sys/expert | OPTION_1P | animation | chrome | the 1P option strip COURSE_DECIDE nests |
| data/graph/sys/expert | OPTION_2P | animation | chrome | the 2P option strip COURSE_DECIDE nests |
| data/graph/sys/expert | DECIDE_BG | animation | chrome | the outro hexagon field, but its only backdrop cell EXDECIDE has YOUR SELECT COURSE printed into it, so the caption cannot be separated from the art |
| data/graph/sys/gameover | GAMEOVER | animation | chrome | the GAME OVER banner |
| data/graph/sys/ending | END_BG1 | cell | background | the ending's machine-room artwork |
