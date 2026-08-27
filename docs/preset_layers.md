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

What the gate reads is the preset documents themselves, dumped by the renderer
(`573Renderer.exe --preset-dump-defaults <tmp>`): every `sprite.draw` and
`sprite.animate` clip of every built-in, resolved through the document's `assets`
block to the package dir plus the cell or animation name, and every
`hidden_parts` entry. It fails on a dump with no documents or no sprite clips
rather than passing on an empty scan. See docs/gates.md.

## Classification

Verdicts below were each read off a `--gc2d-sheet` render of that layer.

`iidx12-expert-select` draws no 2D layer at all. The `expert` package carries an
`EXPERT_BG` animation, but the executable never registers it (its name has no string
in `bm2dx.exe`, and every registration site and cell table was checked in the
HAPPY SKY sweep, `IIDX/happy_sky_3d_screens.md`); the screen's background is the
additive `ex_sky` model over the breathing clear colour, and every layer the screen
does register (`EXPERT_IN`, `HOWTO_START`, `PLAY_*`, `TIME_COUNT`, `EFFECTER_ON`,
`EXPERT_DECIDE`) is chrome.

`iidx12-mode-select` likewise draws no 2D layer: `MODE_BG`, `X_SKY` and `X_GRID` in the
`mode` package are unregistered art (no string in the executable; the one leftover
handle the settled update would rewind is only ever set to -1), and the layers the
screen registers (`MODE_IN`, `HOWTO_START`, `SEL_WAKU`, `SEL_NOW1..5`, `PLAY_*`,
`M_PANEL`, `M_CHANGE`, `TIME_COUNT`, `EFFECTER_ON`, `MODE_DECIDE`) are chrome. Its
background is the `harfsky` half dome over a white clear.

`iidx12-music-select` draws no 2D layer either, and this one had to be settled
against a report that said it did. The `mselect` package ships a complete
full-screen 2D sky (`MUSIC_BG`, whose parts are `GRID`, `MUSIC_BG`, `X_GRID`,
`X_SKY`; the rendered sheet is a 640x480 blue cloud field with a one-in-two
horizontal scanline stipple), but HAPPY SKY replaced it with the 3D `sky` +
`muring` scene and left the art UNREFERENCED: none of those four names has a
string in `bm2dx.exe`, the only two ways to obtain an animation id both take a
literal name, and the 236-entry table the screen builds holds CELLS
(`LISTB`, `LISTR`, `CA_WAKU`, `HATENA`, `Y_GAUGE`, `WEEKLY`, `YAJI`, the
`SO_*` / `SS_*` / `LIST_*` / `EC_*` / `BPM*` list furniture) and no background at
all. No layer this screen registers sits at priority 30 or 31, so nothing 2D is
behind the models; every one of `MUSIC_IN`, `MUSIC_IN_BEGIN`, `HOWTO_START`,
`PLAY_*`, `TIME_COUNT`, `EFFECTER_ON`, `HOWTO_EFFECT`, `CLEAR_RATE(_NO)`,
`CATEGORY_CHANGE`, `SOUND_INFO(_BEGIN)`, `TIEUP`, `GHOST_TXT` and `MUSIC_DECIDE`
is chrome. `X_MUSIC_LINE` and `X_SEL_WAKU` do appear, but only as parts of
`MUSIC_IN` / `MUSIC_IN_BEGIN` and therefore at that animation's priority 10, in
front of the 3D. The background is the 3D scene plus the clear colour plus the
fog, and nothing else
(`IIDX/happy_sky_re/screens/music_select_verify.md` section 1.6).

| package | layer | kind | verdict | what the render showed |
|---|---|---|---|---|
| data/graph/sys/dan_e | DAN_BG | animation | background | two `BG_SEA` quads, no chrome: the sea-blue caustic plate. Sampled every 15 frames over all 600 (`--gc2d-sheet <install>/data/graph/sys/dan_e screenshots/sheet_dan_e_dense 40`), it is a zoom-in only - 3840x2880 at x -1500 and 2340 on frame 0, an additive flash pair around frame 15, and from frame 30 on it is fixed at 640x480 at x 0 and x 640, byte for byte, to frame 599. The draft's "scrolls horizontally" is REFUTED: the second quad sits entirely off screen and never moves. Registered with mode 16, which `sub_492840` reads as hold-last (`mode & 1` loops, else `frame < length` draws, else `mode & 0x10` pins `length - 1`) |
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
| data/graph/sys/card | CARD_BG | animation | background | RED: the cabinet illustration on a blue or red field with flavour text. HAPPY SKY: the same 120-frame idea in navy, `sheet_card/anim_CARD_BG_f{0,39,79,119}.png` - a sky band across the top with a cyan rule, ghosted `1P` / `2P` numerals, two sweeping guide rails, and a small wireframe cabinet whose four `M_KAKKO` crop brackets fold open over the 120 frames. `draws.txt` shows the whole plate is ONE 640x480 `CARD_BG` cell plus `KYOTAI`, `ECBOX_GLOW` x2, `M_KAKKO` x4, `LINESIRO` and `T_REMAIN`, so the marketing block and the `2DX VERSION:12` mark are painted INTO the texture and no `hidden_parts` entry can reach them |
| data/graph/sys/card | T_REMAIN | cell | chrome | the TIME REMAIN chip, a separate `(502,45) 85x11` draw inside both RED's and HAPPY SKY's CARD_BG |
| data/graph/sys/card | PLAYER1 | animation | chrome | the 1P player panel |
| data/graph/sys/card | PLAYER2 | animation | chrome | the 2P player panel |
| data/graph/sys/card | P1_CARD_IN | animation | chrome | the 1P insert-card prompt |
| data/graph/sys/card | P2_CARD_IN | animation | chrome | the 2P insert-card prompt |
| data/graph/sys/card | 1P_START_IN | animation | chrome | the 1P press-start prompt |
| data/graph/sys/card | 2P_START_IN | animation | chrome | the 2P press-start prompt |
| data/graph/sys/title | TITLE | animation | background | RED: the 1736 frame boot sequence, the BEATMANIA 2DX header rule, the genre words fading in and out, and the RED logo settling into the red band. Nests TITLE_TAIKI as a child for its own tail. HAPPY SKY: a different 422-frame film, sampled every 18 frames (`--gc2d-sheet <install>/data/graph/sys/title screenshots/iidx12/sheet_title_dense 24`) - a letterboxed sunset strip under an orange `ORE_LINE` rule, a blue horizon flaring open, cloud footage panning with `UP_YAJI` chevrons and `LENS_F` flares, and a full-bleed sky over a dusk cityscape from frame 366. The `HAPPY?` build, the tagline and the advert block are separate draws and are SHOWN: they are the attract screen's own content (see "The attract shows the game's title content" below) |
| data/graph/sys/title | LOGO_IN | animation | background | HAPPY SKY's 120-frame logo reveal, `anim_LOGO_IN_f{0,59,119}.png`: `TITLE_BG` cloud plate at `(0,-139)` with a white `FLASH` panel filling y 281..480 under a `BG2_1` cyan rule, a `LENS_BG` flare and an `X_SPINDOT` sparkle sweep. Everything printed on it (the wordmark group, the advert block, the version mark) is a separate draw and is SHOWN: the attract keeps its title content |
| data/graph/sys/title | TITLE_TAIKI | animation | background | RED: the standby logo loop the game swaps to once TITLE ends, RED wordmark on the red band. HAPPY SKY: the 480-frame standby loop, same plate as LOGO_IN with three additive `COMET` streaks drifting across it. It is NOT a full-screen sky: `TITLE_BG` covers y 0..341 and the opaque white `FLASH` panel is the bottom 42 percent, on which the game prints the logo, the advert block and the version mark, all of which the preset keeps |
| data/graph/sys/title | LOGIN | animation | background | the RED logo reveal, with a header and footer bar baked in |
| data/graph/sys/title | OP_BG_U | cell | chrome | the BEATMANIA 2DX header bar and its red rule |
| data/graph/sys/title | OP_BG_D | cell | chrome | the IIDXRED BOOT_ footer bar and its red rule |
| data/graph/sys/title | LOGO_BASE | cell | background | the additive 432x107 glow plate the HAPPY SKY wordmark sits on, drawn at `(185,167)` |
| data/graph/sys/title | L_HAPPY1 | cell | background | the upper half of the HAPPY wordmark, `(187,168) 285x35` |
| data/graph/sys/title | L_HAPPY2 | cell | background | the lower half of the HAPPY wordmark, `(187,204) 266x68` |
| data/graph/sys/title | L_SKY1 | cell | background | the upper half of the SKY wordmark, `(449,199) 166x35` |
| data/graph/sys/title | L_SKY2 | cell | background | the lower half of the SKY wordmark, `(393,234) 202x33` |
| data/graph/sys/title | L_BM2DX | cell | background | the `beatmania IIDX 12` line under the wordmark, `(210,239) 174x28` |
| data/graph/sys/title | L_KONAMI | cell | background | the `(C)1999 2005 KONAMI` line, `(531,188) 83x9` |
| data/graph/sys/title | HAPPY1 | cell | background | one of the eight additive HAPPY? letter plates the intro film flickers up around frames 18 to 36 |
| data/graph/sys/title | HAPPY2 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | HAPPY3 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | HAPPY4 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | HAPPY5 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | HAPPY6 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | HAPPY7 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | HAPPY8 | cell | background | HAPPY? letter plate |
| data/graph/sys/title | C_COPY | cell | background | the `Just Got Splash Beats !` strapline the intro film brings up at frame 384 |
| data/graph/sys/title | C_COPY_BOKE | cell | background | the blurred additive twin of the same strapline |
| data/graph/sys/title | ORE_2DX | cell | background | the orange `IIDX VERSION:12` mark on the intro film's rule, `(61,175) 92x5` |
| data/graph/sys/title | 2DXVER12 | cell | background | the `IIDX VERSION:12` mark in the standby plate's bottom right, `(537,330) 67x9` |
| data/graph/sys/title | PLUS | cell | background | the four corner ticks that bracket the version mark |
| data/graph/sys/title | PHOT1 | cell | background | one of four 40x26 product photos in the bottom-left advert block |
| data/graph/sys/title | PHOT2 | cell | background | advert block product photo |
| data/graph/sys/title | PHOT3 | cell | background | advert block product photo |
| data/graph/sys/title | PHOT4 | cell | background | advert block product photo |
| data/graph/sys/title | EF_TXTMINI1 | cell | background | the advert block's `THE ULTIMATE SYSTEM ...` micro-line |
| data/graph/sys/title | EF_TXTMINI2 | cell | background | the advert block's second micro-line |
| data/graph/sys/title | HI_ENE_SOUND | cell | background | the `HIGH-ENERGY SOUND` strapline under the advert block |
| data/graph/sys/title | KOME | cell | background | the asterisk that closes the `HIGH-ENERGY SOUND` line |
| data/graph/sys/title | KURO_BAR | cell | background | the underline drawn under each of the four advert photos |
| data/graph/sys/title | KURO_BARMINI | cell | background | the short tail rule at the end of the advert photo strip |
| data/graph/sys/title | MINI_YAJI | cell | background | the two small arrow heads that lead the advert block |
| data/graph/sys/title | MERU_KI | cell | background | one of the two rating marks beside the advert headline |
| data/graph/sys/title | MARU_KUU | cell | background | the second rating mark beside the advert headline |
| data/graph/sys/title | TX_H | cell | background | one glyph of the `HAPPY?` headline the advert block spells out |
| data/graph/sys/title | TX_A | cell | background | `HAPPY?` headline glyph |
| data/graph/sys/title | TX_P | cell | background | `HAPPY?` headline glyph, drawn twice |
| data/graph/sys/title | TX_Y | cell | background | `HAPPY?` headline glyph |
| data/graph/sys/title | TX_HATENA | cell | background | the question mark that ends the `HAPPY?` headline |
| data/graph/sys/title | X_COIN_BRINK | animation | chrome | the INSERT COIN / PRESS START blinker nested in TITLE_TAIKI. It draws nothing in the renderer because the game fills it at run time from the substitution callback `sub_4386D0` (cells `COIN` / `START` chosen on `sub_43E1D0()`), and it is hidden so the prompt cannot appear if that callback is ever implemented |
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
| data/graph/sys/ending | ENDING_BG | animation | chrome | HAPPY SKY's staff-roll plate. `parts.txt` gives it one part, its own `END_BG1` cell, and `screenshots/iidx12/sheet_ending/cell_END_BG1.png` shows the 640x480 cloud field with CONGRATULATIONS burnt into the top right, IIDX VERSION:12 into the bottom right, and the HAPPY! wordmark, its strapline block and HIGH-ENERGY SOUND into the top left. There is one cell and one draw pair, so those captions cannot be lifted off the artwork |
| data/graph/sys/ending | TITLE_LOGO | animation | chrome | HAPPY SKY's title build and the 2DXSTAFF caption it fades in over frames 120..179 |

## Two games share `data/graph/sys/ending`

The verdict table is keyed by package directory plus layer name, and IIDX RED and
IIDX 12 HAPPY SKY both ship a package at `data/graph/sys/ending`. RED's `END_BG1`
is the machine-room plate the RED ending preset carries. HAPPY SKY's cell of the
same name is a different picture with captions burnt into it, and it is reached
through the `ENDING_BG` animation, which is why the chrome verdict is recorded
against the animation name. `iidx12-ending` therefore carries no 2D layer at all.

The cost is recorded rather than hidden: the game's first 300 frames are the
plate blooming in and cross-fading out, and without it `iidx12-ending` renders
those frames as the bare white clear. The `Title plate blooms in` and `Plate gone`
markers in docs/preset_states.md say what the game does there. If the captions are
ever judged acceptable, the layer is one `sprite.animate` clip of `ENDING_BG` at
priority 27 with `hold_last` playback and an alpha tween from 1 to 0 over frames
200 to 300, and the verdict row above has to change with it.

## `data/graph/sys/title` and `data/graph/sys/card` are shared too

RED and HAPPY SKY also ship a `title` and a `card` package at the same paths, and
`TITLE`, `TITLE_TAIKI`, `CARD_BG` and `T_REMAIN` exist in both with different art.
Unlike `ending` the verdict is the SAME on both games, so one row per key carries
both descriptions rather than a second row silently overwriting the first. Only
HAPPY SKY has `LOGO_IN`, and only RED has `OP_BG_U` / `OP_BG_D`.

## The attract shows the game's title content

`iidx12-attract` is the one HAPPY SKY preset that deliberately shows what this
document otherwise calls chrome. The attract screen IS its title: the HAPPY SKY
wordmark and its glow plate, the `beatmania IIDX 12` line, the `(C)1999 2005 KONAMI`
line, the eight `HAPPY?` letter plates the intro film flickers up, the
`Just Got Splash Beats !` strapline, the `IIDX VERSION:12` marks, the bottom-left
advert block (product photos, micro-lines, `HIGH-ENERGY SOUND`, rating marks, the
`HAPPY?` headline glyphs, underlines and arrows) are the screen, the same way RED's
attract preset keeps the RED logo settling into its band. The user decided on
2026-08-19 that the attract must show pretty much everything the game draws there
except the SYSTEM texts, so those 38 parts are classified background above and the
preset hides exactly one part: `X_COIN_BRINK`, the INSERT COIN / PRESS START
blinker. The other system texts the game draws on the title are not registered
animations at all and are not carried either: the `CARD_NG` and `1CREDIT_DP`
scroller strips (per-frame blits that depend on the cabinet's card reader and
credit configuration), the debug text overlay, the operator branch, and the
`COIN` / `START` / `VEFX` cells the substitution callback fills in at run time
(the renderer has no callback, so those placeholders draw nothing).

With everything else kept, `FLASH` (the opaque white 640x200 panel filling
y 281..480 of `LOGO_IN` and `TITLE_TAIKI`) is simply the plate the advert block and
the version mark are printed on, exactly as the game shows it; the kept art under
it is `TITLE_BG` and `SKY1`..`SKY7` (the sky footage), `KURO_BG` (the letterbox
bars), `ORE_LINE`, `ORE_YAJI`, `UP_YAJI`, `YAJI_OKU`, `WHITE_RING`, `START_BG`,
`LENS_BG`, `LENS_F`, the `X_SPINDOT` sparkle sweep, `COMET` and `BG2_1`.
