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

## Ramps: a phase can move a value per frame

A phase holds constants. A fly-in is a curve, so a phase can also carry `Ramp`
entries: a parameter id, a from and to value, a length in frames, and the curve
the game uses. Two curves cover every fly-in found so far, and both are the shape
the game's own arithmetic has:

- `Linear` for an accumulator whose per-frame step changes linearly. Mode select's
  wind-up is `A -= max(60-2n, 4) * k`, so the STEP is linear in n even though the
  angle is quadratic; ramping `motion.spin_per_frame` reproduces it exactly.
- `Sine` for `value = from + (to - from) * sin(rate * t degrees)`. Music select's
  z and expert select's position are both this, which is why music select
  overshoots: `sin` passes 1.0 at frame 30 and comes back down before the clamp.

A ramp writes straight into the effective copy each frame, so it costs no
re-materialize, and the parameter still reports the game's value as its default.

## Emitters: particles the screen blits per frame

Some screens draw particles through `sub_438480`, a per-frame blit QUEUE rather
than a registered layer, which is why they cannot be a `SpriteLayer`. A phase can
carry `Emitter` entries instead, and the host keeps the same pool of live
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

The three spawn triggers are the game's three: `PhaseStart` for `if (v1 == 0)`,
`EveryFrame` for phase 13's unconditional 32 per frame, and `Beat` for phase 14's
128 on `since == 0` with an odd beat index.

### Swapping layers without making identity editable

The expert outro re-registers `DECIDE_BG` and `COURSE_DECIDE` over the same
animation ids the screen was already using, which is a layer SWAP. Layer identity
is deliberately not a parameter, because identity is exactly what the layer gate
parses out of source and matches against a background verdict.

The swap therefore needs no new mechanism: the scene declares both layers, so the
gate vets both, and the phases toggle `sprite[NAME].visible`. The observable
result is the game's, and identity stays uneditable at runtime.

`DECIDE_BG` is excluded even so. Its only backdrop cell, `EXDECIDE`, has YOUR
SELECT COURSE printed into the same cell as the hexagon field, so the caption
cannot be separated from the art. A layer whose chrome cannot be removed does not
go in a preset.
