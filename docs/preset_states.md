# Scene preset states

A game screen is usually a SEQUENCE, not a pose. The screen fades in, warps in,
settles, and often changes again on input or on a timer. A preset that carries
only the settled pose is not the screen; it is the last frame of it.

Every state a preset exposes is recorded here with where it comes from in the
game, and `tools/ci/check_preset_states.py` fails the build in both directions:
a documented state the preset does not expose, or a state in the preset with no
row here.

A "state" in a preset document is a MARKER on the frame axis (a phase) or an
OPTION CHOICE label. The gate reads them out of the documents the renderer dumps
(`573Renderer.exe --preset-dump-defaults <tmp>`), so a marker is required for
every documented state, including spans that carry no clip at all, such as the
attract screen's two "models hidden" spans. A row belongs to the table it sits in
and is keyed by preset id alone, with no id or prefix rule of any kind: an id in
the first column that no dumped document carries fails the gate, so a typo is a
build failure rather than a silently skipped row. See docs/gates.md.

This exists because `iidx11-attract` shipped as the settled attract loop alone,
while the screen actually plays a 291-frame warp-in first, with the models hidden
before it. The reverse-engineering brief listed all of it. The preset format at
the time could only hold one pose, so the format quietly decided what the truth
was.

## Phases versus options

A **phase** advances on its own, keyed on the preset's frame counter, because the
game advances it on its own. A preset with phases plays its sequence.

An **option** is chosen, because the game chooses it: a selected mode, a selected
course, a player count.

| preset | state | source |
|---|---|---|
| iidx11-attract | Boot animation, models hidden | title state 0 runs `sub_43E4F0(TITLE clip frame)`, which hides slots 4 to 7 for every frame outside 502 to 792 |
| iidx11-attract | Warp in, rotating and zooming | clip frames 502 to 792: position (0,0,-0.15), yaw 1 deg per frame off the RAW clip frame, z tilt 45 rad (85 for r_side), projection fov ramping 22.546017 to 25.110188 at aspect 1.3333334, which is the zoom, plus 16 particles a frame |
| iidx11-attract | Boot animation runs on, models hidden again | clip frames 793 to 901 are outside `sub_43E4F0`'s window, so it hides the slots again while TITLE keeps playing. The attract loop does NOT start at 793 |
| iidx11-attract | Attract loop, settled to the right | `sub_43CD90` state 0 flips to state 2 at clip frame 902, and from there `sub_43E740(1.0, counter)` runs: position (0.105,0,0), yaw counter * 0.125 deg (r_side counter * 0.5 deg), z 45 rad and -10 rad, aspect 1.7708334 |
| iidx11-attract | Standby logo, TITLE_TAIKI looping | `sub_43E010` destroys TITLE once its playhead reaches its 1736 frame length and registers TITLE_TAIKI looping in its place. The 3D keeps running unchanged |
| iidx11-dan-select | CLASS 7 | `flt_4D9018[0]`, camera eye target (0, 0.6, -0.01) eased to 0.9045085 |
| iidx11-dan-select | CLASS 6 | `flt_4D9018[1]` |
| iidx11-dan-select | CLASS 5 | `flt_4D9018[2]`, the DP entry index |
| iidx11-dan-select | CLASS 4 | `flt_4D9018[3]` |
| iidx11-dan-select | CLASS 3 | `flt_4D9018[4]` |
| iidx11-dan-select | CLASS 2 | `flt_4D9018[5]` |
| iidx11-dan-select | CLASS 1 | `flt_4D9018[6]` |
| iidx11-dan-select | 1ST DAN | `flt_4D9018[7]` |
| iidx11-dan-select | 2ND DAN | `flt_4D9018[8]` |
| iidx11-dan-select | 3RD DAN | `flt_4D9018[9]` |
| iidx11-dan-select | 4TH DAN | `flt_4D9018[10]` |
| iidx11-dan-select | 5TH DAN | `flt_4D9018[11]` |
| iidx11-dan-select | 6TH DAN | `flt_4D9018[12]` |
| iidx11-dan-select | 7TH DAN | `flt_4D9018[13]` |
| iidx11-dan-select | 8TH DAN | `flt_4D9018[14]` |
| iidx11-dan-select | 9TH DAN | `flt_4D9018[15]`, requires `sub_434820() >= 1` |
| iidx11-dan-select | 10TH DAN | `flt_4D9018[16]`, the only index that accelerates the spin |
| iidx11-music-select | Fly in, punching past the resting point | `sub_41C620` frames 1 to 34: z = sin(3n deg) * 0.75 - 1.0, which peaks at -0.25 at n=30 and recedes to -0.2756, and a sine-eased yaw of 270 degrees |
| iidx11-music-select | Settled | n >= 35, the clamp at `min(n, 35)` |
| iidx11-music-select | Normal | `dword_1515BB0` clear |
| iidx11-music-select | ATTACK | `dword_1515BB0` set at 0x418838: position (-0.05,-0.01,-0.855), yaw target -540 instead of 270, and shield and flame shearing at 3x the core's yaw rate |
| iidx11-mode-select | Fly in, fast wind up | frames 0 to 14, accumulator `A -= max(60-2n,4) * 0.2`, so the per-frame yaw falls linearly from 0.12 to 0.064 rad |
| iidx11-mode-select | Fly in, five times the accumulate | frames 15 to 27, `A -= max(60-2n,4)`, yaw rate 0.30 down to 0.06 rad |
| iidx11-mode-select | Fly in, spin at its floor | frames 28 to 39, the spin term is clamped at 4, a flat 0.04 rad per frame |
| iidx11-mode-select | Interactive, unwinding | frame 40 on, `A += 0.8`, so yaw unwinds at -0.008 rad per frame forever |
| iidx11-expert-select | Fly in from the eye plane | `sub_433C30` frames 0 to 41: sin(90 * trunc(f) / 38 deg) driving x 0.2 to -0.196, y 0 to -0.006 and z -1.0 to -0.7 |
| iidx11-expert-select | Settled | f >= 42, the clamp at `min(f, 42)` |
| iidx11-expert-select | Outro hold, snapped to centre | `dword_15C0F84 == 1`: position hard-snaps to (0, 0, -0.7), x and y jump to zero, rotation keeps running |
| iidx11-ending | Black, models hidden | phase 0 of `sub_42F030`, frames from 0 (`off_4D9F94` timeline) |
| iidx11-ending | Fade in, camera pulling back | phase 1 of `sub_42F030`, frames from 71 (`off_4D9F94` timeline) |
| iidx11-ending | Burst, the spin-up whip | phase 2 of `sub_42F030`, frames from 440 (`off_4D9F94` timeline) |
| iidx11-ending | Beat pulse | phase 3 of `sub_42F030`, frames from 814 (`off_4D9F94` timeline) |
| iidx11-ending | Core and flame counter-rotate | phase 4 of `sub_42F030`, frames from 1185 (`off_4D9F94` timeline) |
| iidx11-ending | Second beat run | phase 5 of `sub_42F030`, frames from 1557 (`off_4D9F94` timeline) |
| iidx11-ending | Beat 80, the flame unwinds | still phase 5, but the `dword_15C07C0 < 80` branch flips: slot 6's z rotation stops being the constant 45 rad and becomes `(frame - 1930) * 1 deg`. Beat 80 lands at frame 1929 and the counter is published at the end of that frame, so the branch first runs on frame 1930 with z exactly 0 |
| iidx11-ending | Camera pushes through | phase 6 of `sub_42F030`, frames from 2300 (`off_4D9F94` timeline) |
| iidx11-ending | Snap back, triple speed | phase 7 of `sub_42F030`, frames from 2475 (`off_4D9F94` timeline) |
| iidx11-ending | Fade out, close in | phase 8 of `sub_42F030`, frames from 2486 (`off_4D9F94` timeline) |
| iidx11-ending | Cyan burst | phase 9 of `sub_42F030`, frames from 2672 (`off_4D9F94` timeline) |
| iidx11-ending | Blue burst | phase 10 of `sub_42F030`, frames from 2765 (`off_4D9F94` timeline) |
| iidx11-ending | Nine degree spin | phase 11 of `sub_42F030`, frames from 2858 (`off_4D9F94` timeline) |
| iidx11-ending | Reverse twelve | phase 12 of `sub_42F030`, frames from 2904 (`off_4D9F94` timeline) |
| iidx11-ending | Sixteen degree spin, fading up | phase 13 of `sub_42F030`, frames from 2950 (`off_4D9F94` timeline) |
| iidx11-ending | Logo hold | phase 14 of `sub_42F030`, frames from 3034 (`off_4D9F94` timeline) |
| iidx11-ending | Drift out | phase 15 of `sub_42F030`, frames from 3787 (`off_4D9F94` timeline) |
| iidx11-ending | Final push in | phase 16 of `sub_42F030`, frames from 4065 (`off_4D9F94` timeline) |
| iidx11-expert-select | Outro fade | `sub_4331B0` ramps a full-screen quad 100 to 0 over 31 frames while the models fade out |
| iidx12-expert-select | Course select | HAPPY SKY `sub_434270` state 0 for its 2760-frame timer (which only starts counting at frame 60, so the game state runs 2820 frames; the preset carries the timer length): `ex_sky` additive over the breathing clear colour of `sub_432C10` (dark blue `(0, 48, 96)` at frame 0 to dark red `(96, 0, 0)` at 2700 and back, linear per channel, C integer division). Nothing else changes with the cursor, the course, the category or the side, and the decide phase (model hidden, black clear, 180 frames) is not background, so it is not carried. Read from the disassembly, see `IIDX/happy_sky_3d_screens.md` |
| iidx12-mode-select | Fly in | HAPPY SKY `sub_41B6B0` state 0, per frame `sub_41A1C0`: `t = min(f * 0.025252523, 1)`, `at` slides linearly from (0, 0.4340612, 0) to (0, 0.4340612, 0.9008834) and `eye` from (0, 0.3, 0) to (0, 0.0302638, -0.2755064) on `sin(t)`; the preset carries a key per frame (0..40) so the integer-frame values are exact. Frame 0 is the game's own degenerate LookAt (eye, at and up collinear) |
| iidx12-mode-select | Settled | from frame 40 (`t` clamped to 1, `sin(1) = 0.84147`, so the eye settles at (0, 0.073023, -0.231832)); `sub_41B0C0` re-asserts the same camera every frame; nothing in the 3D depends on the mode cursor, so there is no option. The preset carries the 1201-frame timer, which the game only decrements once `MODE_IN`'s playhead reaches 66, so the game screen runs about 1267 frames. The 66-frame `MODE_IN` chrome and the `MODE_DECIDE` exit are not background. See `IIDX/happy_sky_3d_screens.md` |
| iidx12-music-select | Song list | HAPPY SKY `sub_41F240` writes the whole 3D scene ONCE at init and `sub_4206C0` never touches a slot, a camera, a projection or a light again, so there is no phase to carry: the models only advance their baked clips (`sky` 30 ticks, `muring` 300, `extra_st` 640). The marker names the one span the preset holds. The game's own screen runs a 1800..6000 frame operator timer that only starts counting at frame 60; the preset carries 1800. The 60-frame frozen intro, `MUSIC_IN` and the `MUSIC_DECIDE` exit are chrome. See `IIDX/happy_sky_3d_screens.md` |
| iidx12-music-select | NORMAL | the extra-stage flag `dword_18564CC` clear. `sub_41E120` shows slot 0 `sky/sky.xz` opaque alpha 1 speed 1 and slot 9 `sky/muring.xz` additive alpha 0.2 speed 1; `sub_41DF50` sets eye (-0.074308, 0.031367, -0.6388999), at (1.559, 0, 1.12) and up `D3DXMatrixRotationZ(10 deg)` applied to (0,1,0) = (-0.17364818, 0.98480775, 0), a 10 degree camera roll; `sub_41CD80` clears to `0xFFFFFFFF` once and turns fog ON, white, start 55.0, end 62.4, density 0.5 |
| iidx12-music-select | EXTRA | the flag set (`sub_441E90`, or the 5th-stage input force). The `sub_41F240` branch shows slot 10 `extra_st/extra_bg.xz` additive alpha 1 at DOUBLE clip rate and slots 0 and 9 stay hidden; the camera becomes eye (-0.2,-0.2,-0.2), at (1, 0.78, 1), up (0, 100, 0), byte for byte the expert-select camera; `sub_41CD80` clears to `0x00000000` and turns fog OFF; and `sub_41E1F0` rewrites the clear colour every frame with the strobe and ramp the `render.clear_cycle` clip carries. The 2D half of the screen is identical on both choices, so nothing else changes |
| iidx12-dan-select | Enter | HAPPY SKY `sub_42DFA0` writes the six slots and the entry camera eye (0, 0.06, -0.45), at (0, 0.16, 0), up (0, 1, 0), and `sub_42E730` refuses to write a camera target until the screen frame counter reaches 21, so this span holds that pose exactly. `DAN_BG` is registered on the first update frame. The preset carries the 1260-frame TIME REMAIN (`sub_42DFA0`: `(sub_43DAB0() ? 0x1284 : 0) + 0x4EC`, so 1260 normally and 6000 in event mode); no consumer was found that ends the screen when it hits 0, so the length is a display length, not a timeout |
| iidx12-dan-select | Camera follows the grade | from frame 21 `sub_42E730` writes the per-cursor target and `sub_42D3E0` eases eye.y, eye.z and at.y toward it every frame. eye.x, at.x and at.z are never lerped: the target table's eye.x of 0.9 is written and has no reader, so the camera moves in Y and Z only |
| iidx12-dan-select | CLASS 7 to 1 | cursor 0..6. `sub_42D3E0` holds the shared scale target at 1.0, so `dan_sky` and `dan_sky2` sit at scale 1 and y 0; `sub_42D510` runs the slot 8 alpha down at 0.1 per frame to 0, so `dan_light_bg` is invisible; the camera target is eye (0, 0.076, -0.2), at (0, 0.18, 0) at rate 0.05, and no particles spawn |
| iidx12-dan-select | 1ST to 8TH DAN | cursor 7..14. The shared scale target flips to 3.5 and the derived position y to `(1 - 3.5) * 0.033333335 = -0.083333336`; the camera target becomes eye (0, 0.55, -0.45), at (0, 0, 0), still at rate 0.05; slot 8 stays dark and no particles spawn |
| iidx12-dan-select | 9TH and 10TH DAN | cursor 15..16, reachable only while `sub_449FA0() >= 1`. The scale stays 3.5 but the camera lerp rate doubles to 0.15 and the target drops to eye (0, -0.05, -0.76), at (0, -0.6, 0), putting the eye under the sea planes; `sub_42D510` ramps slot 8 up at +0.005 per frame to 0.6, lighting the `dan_light_bg` curtain; and `sub_42DBF0(27)` starts raining `AWA1` bubbles from the `system` package at priority 27 once the screen frame counter passes 59. The choice is ONE preset state covering both grades, and the emitter it carries is the **9TH DAN record: 3 bubbles per burst**. 10TH DAN's own record is the same `AWA1` art at **5 bubbles per burst**; that count is documented here but is NOT carried by the preset, because the document's `grade` option has no separate 10TH DAN choice to hang it on. A preset that splits the choice in two would add a second emitter clip with `count` 5 |
| iidx12-ending | Title plate blooms in | HAPPY SKY `sub_430440` frames 0..199. The 3D is empty: `sub_42FB10` hid slot 0 at init and `sub_42FE30`'s `if (dword_185A2BC >= 200) sub_495450(0, 1)` has not fired, so the screen is the white clear with the `ENDING_BG` cloud plate over it (a 120-frame bloom-in, then held on frame 119 by the `0x10` flag). The plate is CHROME here, see docs/preset_layers.md, so the preset carries the marker and not the layer: frames 0..199 render as the white clear alone |
| iidx12-ending | Sky and movie tiles | frame 200. `sub_42FE30` shows slot 0 `sky/sky.xz` at alpha 0.5, draw mode 2, pose (0,0,0)/(0,0,0)/(1,1,1) as `sub_42FB10` left it, and `sub_430440` starts calling `sub_4300D0` to submit the 3 x 3 movie tile grid. The dome's own clock has been running since frame 0 (the scene loop advances `obj+164 += obj+156` regardless of the visibility flag), so it appears at tick 200. The two `>= 200` tests are one update apart in the game because `sub_4300D0` runs before the frame counter's `++` and `sub_42FE30` after it; the preset starts both at 200 |
| iidx12-ending | Plate gone | frame 300. `sub_430440` blits `ENDING_BG` with the per-call alpha pair `(clamp(300 - f, 0, 100), 100 - that)`, so the plate cross-fades out linearly over frames 200..300 and from 300 the screen is the sky dome, the nine tiles and the credit roll. The preset does not carry the plate, so this marker records where the game's cross-fade lands rather than a change the preset makes |
| iidx12-ending | Speed ramp and burst | frame 4833 (`dword_185A544 > 179`, first true at `27 * 179`). `sub_42FE30` starts adding 0.025 to slot 0's anim speed every frame, reaching 7.975 on the last update, and `sub_4300D0`'s burst counter `flt_185A580` starts climbing 2.0 a frame, pushing quad `i` in +Z once the counter passes `10 * i`. The tiles fly AWAY from the camera and white out in the fog around z 80 |
| iidx12-ending | Fade | frame 5077. `cmp dword_185A544, 0BDh / jge` is unconditional, so the exit latch fires the frame after the line index reaches 189, and `sub_40E720` ramps a full-screen curtain 100 to 0 over frames 5077..5107 at 2D priority 4 before the update returns 1 on frame 5111. The curtain is chrome, so the preset carries the marker and keeps rendering the screen to its 5112th frame |
| iidx12-ending | Seed 1 | the lattice jitter `sub_42F410` draws 16 values from the CRT `rand()` seeded from `timeGetTime` at stage init, so the game's own jitter is NOT reproducible and a preset has to name a seed. This choice runs the MSVC generator from 1, which is also the CRT's own default seed |
| iidx12-ending | Seed 573 | the same 16 draws from seed 573. Only the seed changes: the draw order, the twelve displaced points and the +/- half-amplitude step are fixed by `sub_42F410` |
| iidx12-ending | Seed 12345 | the same 16 draws from seed 12345, offered so the editor and `--preset-option lattice=...` can show that the grid shape is a preset parameter and not part of the screen |
| iidx12-attract | Intro film | HAPPY SKY `sub_438970` state 0. `sub_438780` registers nothing and the update's entry block runs `sub_40E030(list, pkg, "TITLE", 16, 0, 0, 15)` on the first frame, mode 16 being play-once-then-hold-the-last-frame (`sub_492840` tests `mode & 1` for loop, then `frame < length`, then `mode & 0x10` to pin `length - 1`). `TITLE` is 422 frames, and the state only flips when `sub_40E300 >= sub_40E2C0`, i.e. one frame AFTER the playhead passes the end, so the clip owns document frames 0..422 |
| iidx12-attract | Logo reveal | frame 423. The entry block unregisters the previous layer and registers `LOGO_IN` mode 16 at the same (0,0) and priority 15. `LOGO_IN` is 120 frames and the same late handover applies, so it owns 423..543. The preset gives each clip `time: restart` because the game registers a NEW layer with `record+8 = 0` rather than continuing the old playhead |
| iidx12-attract | Standby loop | frame 544. `TITLE_TAIKI` is registered mode **1**, the loop mode, and nothing ends it: this is the attract standby screen. 480 frames a cycle |
| iidx12-attract | Attract times out | frame 2400 on `dword_1895D5C`, which is reset once per title session by the latched `sub_437D30` and every frame by the operator branch. From here `sub_438970` calls `sub_40E720(100 * (2400 - c) / 60 + 100, 8)`, whose first argument is `100 - alpha` (`sub_40E720` passes the pair `(v2, 100 - v2)` where every other call site of that family passes `(100 - alpha, alpha)`), so a black curtain fades IN over frames 2400..2460 at 2D priority 8 and the update returns -1 on frame 2461. The curtain is chrome, so the preset carries the marker and keeps drawing the standby loop to its 2461st frame |
| iidx12-card-in | Card hall | HAPPY SKY master state 2/3. `sub_42B3D0` clears the layer list and registers `sub_40E0D0(list, pkg, "CARD_BG", pkg, "CARD_IN", 16, 0, 0, 31)`, so the plate is mode 16 at priority 31, the only 2D priority the frame driver draws BEHIND the model scene (`sub_447E60` runs `sub_48FE50(30, 31)` before `sub_496730` and `sub_48FE50(0, 29)` after). The attached cell is the CARD IN caption and is chrome, so the preset carries the animation alone. Nothing else changes for the whole screen: every other registration is a per-player prompt at priority 19..28. The 3600 is the screen's own timer (`sub_43DAB0() ? 6000 : 3600`), and in event mode the 6000 is never decremented, so the digits sit frozen at 99 |
| iidx10-mode-select | BEGINNER | the per-mode cube placement the screen lerps to |
| iidx10-mode-select | LIGHT7 | per-mode cube placement |
| iidx10-mode-select | 7KEYS | per-mode cube placement |
| iidx10-mode-select | EXPERT | per-mode cube placement |
| iidx10-mode-select | CLASS COURSE | per-mode cube placement |
| iidx10-mode-select | FREE | per-mode cube placement |

## Screens whose remaining states are not yet exposed

Recording these is the point: an empty column here would hide the gap.

| preset | state still missing | why |
|---|---|---|
| (none) | | every state found so far is built |

### Where the title screen's 2D sits relative to its 3D

`sub_40D940` registers the whole title screen at priority **15**:
`sub_43CB00(pkg, 15, ...)` for TITLE, `sub_43CD90(pkg, 0, 15)` for the per-frame
pass, which is the priority `sub_43E010` hands to TITLE_TAIKI and case 5 hands to
LOGIN. The warp particles are the exception at 31, straight from
`sub_438480(..., 31, 60, 0)`.

Priority 15 is BELOW `sprite_split_priority`, so those layers draw after the 3D
pass and the logo occludes the emblem. Getting this wrong is very visible: at 31
the emblem draws over the RED wordmark and, because slots 4 to 6 are additive,
it washes the logo out instead of passing behind it. Four independent priorities
bracket the 3D pass and all agree the split belongs between 24 and 31: title 2D
at 15 and the ending's bursts at 24 draw in front of the models, while the title
warp particles at 31 and the ending backdrop at 31 draw behind them.

### The state HAPPY SKY's attract has that the preset does not carry

`sub_438780`'s tail is `if ( sub_43E1D0() != 0 && sub_43DF50() == 0 ) dword_1895D48 = 1;`,
so when a credit is already in the machine the title screen is ENTERED in state 1
and the 422-frame `TITLE` film never plays: the screen opens on `LOGO_IN` and falls
straight through to the standby loop. `sub_43F4D0` uses the same pair of tests to
skip the whole boot-logo chain.

`iidx12-attract` does NOT carry that variant, and it is a marker-less omission
rather than a missing state row on purpose. Carrying it would need a `coin` option
whose two choices place the SAME clips on DIFFERENT frames (`LOGO_IN` at 0 instead
of 423) and shorten the document by 423 frames, and a document has one frame axis
and one length. The zero-credit attract path is the one the cabinet shows when
nobody is standing at it, which is what a background preset is for.

Two more registrations on that screen are chrome and are not carried: `LOGIN`
(mode 0, priority 14, the login handshake once a card is accepted) and `VEFX`
(mode 0, priority 14, registered once `sub_43E1D0()` reports a credit and then
hand-looped over frames 60..179 every frame by
`if ( sub_40E300(...) >= 180 ) sub_40E2E0(..., 60 )`). The two ticker strips
`CARD_NG` and `1CREDIT_DP`, both at priority 14, are chrome as well.

### One card preset covers four screens

The `card` package is loaded by four screens, and all four register the SAME
`CARD_BG` animation at mode 16, position (0,0) and priority 31. They differ only in
the caption CELL attached to it through `sub_40E0D0` and in the per-player prompts
stacked on top, all of which are chrome:

| master state | init | attached caption cell | screen |
|---|---|---|---|
| 2 / 3 | `sub_42B3D0` | `CARD_IN` | card in / reception |
| 6 / 7 | `sub_435F00` | `SANKASYA` | new player invited |
| 20 / 21 | `sub_429A20` | `CARD_OUT` | card out, save and eject |
| 4/5, 8/9 | `sub_417D80` | (package `name`, `NAME_BG`) | name entry |

So `iidx12-card-in` is the background of all three `card` screens, not just the
reception one, and card out and new player invited get no preset of their own -
they would be the same picture three times. Their timers differ (card in 3600,
new player invited 1200, card out none) and the preset carries card in's.

Name entry is the same hall composition recoloured teal in a different package,
with a `NAME ENTRY` title and a TIME REMAIN chip painted INTO the plate rather than
drawn separately, so it cannot be cleaned up and is not carried either.

## Ramps: a clip can move a value per frame

A clip holds constants. A fly-in is a curve, so a clip also carries tween keys: a
parameter, a value per key, the key frames, and the ease the game uses. Two eases
cover every fly-in found so far, and both are the shape the game's own arithmetic
has:

- `linear` for an accumulator whose per-frame step changes linearly. Mode select's
  wind-up is `A -= max(60-2n, 4) * k`, so the STEP is linear in n even though the
  angle is quadratic; tweening `spin_per_frame` reproduces it exactly.
- `sine_deg` for `value = from + (to - from) * sin(rate * t degrees)`. Music
  select's z and expert select's position are both this, which is why music select
  overshoots: `sin` passes 1.0 at frame 30 and comes back down before the clamp.

The keys are resolved into the frame the evaluator hands the host, so a ramped
value costs nothing beyond the resolve and the parameter still reports the game's
value as its default (docs/preset_document.md).

## Emitters: particles the screen blits per frame

Some screens draw particles through `sub_438480`, a per-frame blit QUEUE rather
than a registered layer, which is why they cannot be a `sprite.animate` clip. An
`emitter` clip carries them instead, and the host keeps the same pool of live
particles the game's queue does, ageing and freeing them on the same schedule.

The attract warp's ring is fully deterministic and is reproduced exactly: 16
`PTC_ORAN` cells at 22.5 degree steps, on a radius growing 10 to 630 px by
integer division over 290 frames, with the whole ring rotating by
`trunc(sin(32f deg) * 360)` degrees. The cell comes from `data/graph/sys/system`,
a DIFFERENT package from the screen's own, exactly as the game does it, so the
2D host keeps a second package purely for particle cells.

### The ending's beat, pulses, jitter and particles

The 18 phases carry each phase's yaw rate, camera eye, alpha ramp and particle
work, read out of `sub_42F030`'s switch. The beat pulses and the position jitter
used to be listed here as not reproduced. They are reproduced now, exactly, and
this is where the mechanism is written down. `docs/preset_document.md` lists the
rhythm commands and their parameters, `IIDX/red_3d_screens.md` in the notes repo carries the full RE trail.

**The beat grid is arithmetic, not audio.** `sub_430300` returns
`155 * (frame - 70) / 3600` and `sub_430330` returns `155 * (frame - 59) / 3600`,
both plain C integer division on the ending's own frame counter. That is 155 BPM
at 60 frames a second, on two grids eleven frames apart. `sub_42F030` compares
each against the value it stored last frame: unchanged increments a
frames-since counter, changed resets it to zero. Nothing external feeds it, so
the whole thing is a pure function of the frame number. The scene carries it as
`beat.rate` 155, `beat.span` 3600, `beat.offset_a` 70, `beat.offset_b` 59.

**The pulse is a decay back to one.** Phases 3, 4 and 14 read grid A, phase 5
reads grid B. Each writes `scale = from - (from - 1) * since / 8` while `since`
is under 8 and a flat 1.0 after. Phases 3, 4 and 5 use `from = 1.1` on every
beat; phase 14 uses 1.5 when the beat index is odd and 1.2 when it is even,
which is what gives the logo section its limp. The counter the phase reads is
the one published at the END of the previous frame, so the host updates the beat
after applying the pulse, not before.

**The jitter is one draw per frame, applied to x and y together.** Phase 1 past
frame 430 sets both models' position to `(rand % 200 - 100) * 0.0001` on both
axes; phase 2 past frame 723 does the same with `rand % 100 - 50`. It is a
position SET, not an offset, and both models get the same value.

**The generator is reproduced exactly.** `sub_48D950` / `sub_48D9C0` /
`sub_48DA10` are Knuth's subtractive generator: 55 entry table, lags 24 and 31,
modulus 1000000000, seeded by `ma[55] = seed` and 54 steps of
`ma[(21k) % 55] = mk; mk = mj - mk; mj = ma[(21k) % 55]`, then three refill
sweeps with the index parked at 55 so the first draw triggers a fourth. That is
`Preset::Ran3`, transcribed from the disassembly rather than from a textbook, and
a test pins it.

What the game does NOT hand us is the seed. `sub_43FE40`, the stage init, calls
`sub_48D950(timeGetTime())`, and gameplay then consumes an unknown number of
draws before the ending starts. The ending's own init `sub_42E770` does not
reseed. So the real cabinet scatters differently on every play, by design. The
seed is therefore a parameter, `rng.seed`, defaulting to 1: pick one and the
entire timeline replays identically, which is what a capture needs.

**Particle order matters and is preserved.** Every draw is spent in the game's
order, so a given seed lands on the game's own values: two draws per particle for
the 480 bursts (x, then y, life fixed at 45) and three for the 32 and 128 counts
(x, y, then `rand % 48 + 48` for the life). Within a frame the spawn happens
before the jitter draw, as in the switch.

**A particle is a lerp, not a dot.** `sub_438540` places it at
`centre + age * (target - centre) / life` with alpha `100 - 100 * age / life` at
200 percent scale, and `sub_438420` frees it once age reaches life. Particles
spawned on frame N first draw on frame N + 1, because the pass that draws them
already ran. The host keeps live particles with those integer semantics.

The three spawn triggers are the game's three: `clip_start` for `if (v1 == 0)`,
`every_frame` for phase 13's unconditional 32 per frame, and `beat` for phase 14's
128 on `since == 0` with an odd beat index.

### Swapping layers without making identity editable

The expert outro re-registers `DECIDE_BG` and `COURSE_DECIDE` over the same
animation ids the screen was already using, which is a layer SWAP. Layer identity
is deliberately not a parameter, because identity is exactly what the layer gate
parses out of source and matches against a background verdict.

The swap therefore needs no new mechanism: the document declares both layers, so
the gate vets both, and `param.override` clips toggle `sprite[NAME].visible`. The
observable
result is the game's, and identity stays uneditable at runtime.

`DECIDE_BG` is excluded even so. Its only backdrop cell, `EXDECIDE`, has YOUR
SELECT COURSE printed into the same cell as the hexagon field, so the caption
cannot be separated from the art. A layer whose chrome cannot be removed does not
go in a preset.
