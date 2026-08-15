# The legacy preset golden fixture

This file describes the reference recording of the OLD preset host, taken before
milestone M2 of `docs/scene_preset_editor_plan.html` changes anything about it. It
is the acceptance criterion for the new evaluator: M2 part B replays every fixture
and compares the pushes, and M8 re-points the same comparison at the built-in
default documents. A recorded VALUE is never re-recorded to make a comparison pass;
the fixture is only ever re-recorded to ADD to the format, from the old host, and
only when every value it already holds comes back identical.

Status: the asset table and the fixtures are in the tree, and both the coverage
test and the evaluator comparison run in the `ci` label. The recorder is not in the
tree; "The fixture is frozen, and how to re-record it" below is the whole
procedure, including the commit the old host lives at.

## What is recorded

`src/preset/preset_host.cpp` drives exactly two side-effecting modules:
`Scene3dHost` (`src/scene3d/scene3d_host.h`) and `Gc2dHost` (`src/gc2d/gc_host.h`).
Both are namespaces of free functions, so the whole observable behaviour of the old
host is the ordered sequence of calls it makes into those two namespaces. The
fixture is that sequence, per frame, per option choice.

Every call the old host makes is recorded, with every argument in full precision:

| Namespace | Recorded calls |
|-----------|----------------|
| `Scene3dHost` | `LoadWithSetup`, `Unload`, `RenderFrame`, `SetTime`, `SetModelSpeed`, `SetModelAlpha`, `SetModelBlendByName`, `SetModelScale`, `SetModelVisibleByName`, `SetModelTransform`, `SetProjection`, `SetView`, `SetStyle`, `SetLights` |
| `Gc2dHost` | `Load`, `Unload`, `LoadParticles`, `SetSprites`, `SetSpriteFrame`, `SetSpriteScale`, `AdvanceSprites`, `DrawSprites`, `DrawParticles`, `SetFrame` |

`Scene3dHost::GetStatus`, `Gc2dHost::ListSprites` and `Gc2dHost::AnimationLength`
are reads, not pushes, so they are answered from the asset table (below) and not
recorded. That is the complete set of host functions `preset_host.cpp` references;
no other function is stubbed, because an unreferenced stub would be dead code.

## The seam: link-time stubs, no change to the old host

The old `preset_host.cpp` was **not** modified while it was being recorded, and no
interface, sink or virtual seam was introduced into it.
`tests/game/preset_host_stubs.cpp` simply *defines* the `Scene3dHost::` and
`Gc2dHost::` functions listed above, and the recorder linked the real
`src/preset/*.cpp` against those definitions instead of
`src/scene3d/scene3d_host.cpp` and `src/gc2d/gc_host.cpp`. The stubs survive the
recorder: `game_tests` links the NEW host against them for the host tests of
`tests/game/preset_host_tests.cpp`, which is why they also answer
`Gc2dHost::DescribePackage`, `Scene3dHost::DescribeScene` and
`Gc2dHost::SetCanvas` from `asset_lengths.json`.

This was possible because both hosts are namespaces of free functions with a single
global instance each, exactly as the plan (3.9) predicted, and because `game_tests`
already compiles the preset sources without linking `r573_app`. The stubs need no
D3D device, no window and no game files.

Why this and not a recording sink inside `preset_host.cpp`: any seam added to the
old host is a change to the thing being measured. A pure refactor still has to be
believed; a link-time substitution cannot change the app at all, because the app
target does not contain the stub translation unit. The renderer's own build is
untouched by this milestone.

## The asset table

The stubs hold no game data, but the old host reads four values that come from
loaded assets. Only two of them are actual asset lookups:

| Read | Where in `preset_host.cpp` | Why it matters |
|------|---------------------------|----------------|
| `Scene3dHost::GetStatus().max_time` | `ClipFrames` | `NaturalFrames` for every preset with no countdown, and `PhaseTail` |
| `Gc2dHost::AnimationLength(name)` | `PhaseTail` | the longest animated 2D layer visible in the last phase |
| `Gc2dHost::ListSprites().size()` | `Restart` | how many `SetSpriteFrame(i, 0)` calls Restart makes; answered from the last `SetSprites` |
| model names, sprite names, indices | everywhere | come from the `Preset::Scene` tables, not from the assets |

`tests/game/fixtures/asset_lengths.json` supplies the first two. The stubs fail
loudly (the recorder aborts with `asset table gap: ...`) when a scene directory or
an animation name is missing from it, so a silent zero cannot reach a fixture.

### Where every number came from

All values were read from the real IIDX 10 and IIDX RED installs with the tools
already in the tree; none is an assumption.

| Key | Value | Source |
|-----|-------|--------|
| `data/graph/texture/music` | 240 | `573Renderer.exe --scene3d-test <game>/data/graph/texture/music out.png 1`, the `[Scene3d] scene 'music': ... 240 ticks` line. `max_time` is the maximum `max_key_time` over the scene's models (`src/scene3d/scene3d.cpp`, `LoadModels`) |
| `data/graph/texture/cube_x` | 60 | same command on `cube_x` |
| `data/graph/texture/ex01` | 480 | same command on `ex01` |
| `data/graph/texture/tranbox` | 120 | same command on `tranbox` |
| `data/graph/texture/samurai` | 60 | same command on `samurai` |
| `data/graph/model/red` | 240 | same command on IIDX RED's `data/graph/model/red`; cross-checked by `docs/export_pipeline.md`, which states the ending tail is `240 / 0.75 = 320` |
| `data/graph/sys/title` `TITLE` | 1736 | `573Renderer.exe --preset-test <game> iidx11-attract out.png 1740`, the `[Gc2d] layer 'TITLE': 1736 frames` line; the same 1736 is quoted in `docs/preset_states.md` for the attract phase boundary |
| `data/graph/sys/title` `TITLE_TAIKI` | 720 | same run, the `[Gc2d] layer 'TITLE_TAIKI': 720 frames` line, reached once the attract preset enters its last phase; `docs/export_pipeline.md` states attract is `1736 + 720 = 2456` |

Only `TITLE_TAIKI` is actually queried by the old host (it is the one animated
layer visible in the attract preset's last phase; the ending's `END_BG1` is not
animated, so `PhaseTail` skips it). `TITLE` is recorded next to it because it is
the source of the attract phase boundary the tables already encode.

## The end rule, per preset

Each run is `PresetHost::Load` -> `SetOption(0, choice)` (only when the preset has
options) -> `Restart` -> `NaturalFrames()` calls to `RenderFrame(1/60)`. The frame
count is therefore whatever `NaturalFrames` (`preset_host.cpp`) returns, which is
one of three rules:

| Rule | Presets | Frames |
|------|---------|--------|
| `countdown.start_frames` | `iidx10-music-select` 1800, `iidx10-music-select-samurai` 1800, `iidx10-card-in` 3600, `iidx10-mode-select` 1200, `iidx10-dan-select` 1200, `iidx10-expert-select` 1800, `iidx10-new-player` 1200, `iidx10-game-over` 180, `iidx11-music-select` 3600, `iidx11-mode-select` 1200, `iidx11-dan-select` 1200, `iidx11-expert-select` 2700, `iidx11-new-player` 1200, `iidx11-card-in` 3600 | as listed |
| last phase start + `PhaseTail` | `iidx11-attract` = 1736 + 720, `iidx11-ending` = 4065 + 320 | 2456, 4385 |
| `ClipFrames` = `max_time / anim_speed` | `iidx10-login` = 240 / 1.0, `iidx11-login` = 240 / 1.0 | 240, 240 |

`SetOption` runs BEFORE `Restart` on purpose: that is the order the export path
uses (the GUI picks a choice, the capture driver restarts), and `Restart` is the
one reset path that deliberately does not clear `g_choices`, `g_transition` or
`g_spin_kick` (plan 2.5). A non-default choice therefore starts frame 0 with its
option transition already in flight, which is exactly what the app does.

`dt` is a fixed `1.0f / 60.0f` on every call, matching `preset_test.cpp`'s
`kFrameSeconds` and the export tick.

## The fixture format

One file per (preset, choice) in `tests/game/fixtures/golden/`, named
`<preset-id>.json` when the preset has no options and `<preset-id>.<choice>.json`
when it does. 40 files, 2.7 MB in total, largest 628 KB
(`iidx11-ending.json`).

```
{
 "preset": "iidx11-attract",
 "build": "iidx11",
 "choice": null,
 "frames": 2456,
 "setup": [ "Scene3dHost::LoadWithSetup('data/graph/model/red', 1, 60, ...)", ... ],
 "hashes": [ "77c3f1e2a9b40d51", ... ],
 "hashes_no_transform": [ "3d0b2ecf535295a4", ... ],
 "detail": { "0": [ "Gc2dHost::DrawSprites(30, 2147483647)", ... ], "64": [ ... ] }
}
```

- `setup` is every push made by `Load` + `SetOption` + `Restart`, before frame 0.
- `hashes` has exactly `frames` entries, one per `RenderFrame` call: the FNV-1a
  64-bit hash of that frame's push list, 16 lowercase hex digits. Frame `i` covers
  the whole `RenderFrame(dt)` call, so it holds the draws for frame `i` followed by
  the `Advance()` that prepares frame `i + 1`.
- `hashes_no_transform` has the same `frames` entries taken over the same push
  list with every `Scene3dHost::SetModelTransform` line removed
  (`PresetGolden::WithoutModelTransforms`, the one definition both the recorder and
  the comparison call). It is what closes the hidden-model hole below: on a frame
  whose transforms cannot be reproduced, everything else on that frame still has a
  hash to be compared against. The rule is per line and preset independent - every
  transform of every model on every frame of every preset - so the recorder holds
  no preset knowledge.
- `detail` carries the full push list for the frames a human or a failing test
  needs to read: every 64th frame, every phase start frame (the plan's markers) and
  the frame right after each phase start. That stride and marker rule lived in the
  recorder and is frozen into the committed files; the reader does not re-derive it.

A push is canonicalised to one line of text, `Namespace::Call(arg, arg, ...)`.
Floats use `std::to_chars`'s shortest round-tripping form, so the text is exact and
platform independent; strings are quoted with `'`; `std::array<float, 3>` prints as
`[x y z]`; the compound arguments (`proj[...]`, `light[...]`, `model[...]`,
`sprite[...]`, `timing[...]`, `parts[...]`, `cell[...]`) are spelled out in
`tests/game/preset_host_stubs.cpp`. The hash is over that canonical text, joined by
newlines, so it is stable across runs, compilers and platforms: nothing in it is a
pointer, an address, an unordered container order or a locale-dependent number.

Determinism was verified by recording twice into a copy and diffing: byte identical.

## The fixture is frozen, and how to re-record it

The recorder (`tests/game/preset_golden_record.cpp`, target `preset_golden_record`)
exists only to drive the OLD `PresetHost` against the link-time stubs. M2 part C
replaced that host with the evaluator, so there is no legacy runtime left in the
tree to record and the recorder is not in it: keeping an executable that records the
NEW host into the file the new host is checked against would turn the reference into
a mirror, and keeping a source file nothing builds would be dead code. The procedure
below IS the recorder's home; it is rebuilt from the last commit that still has the
old host whenever the fixture format has to change, and never to "fix" a difference.

The old host is commit `61d22b7` ("Add preset document structures and validation"),
the parent of the M2 part C change on `feature/fixes`. That tree has
`src/preset/preset_host.cpp` as the legacy state machine and the `Preset::Scene`
tables, but not the stubs, the format reader or the recorder, which are all part of
M2. So:

1. `git worktree add <scratch>/m2-legacy 61d22b7 --detach` somewhere OUTSIDE the
   repo (a scratch directory, never a tracked path).
2. Copy today's `tests/game/preset_golden_format.h/.cpp` into the worktree
   unchanged, so the canonical push text, the frame hash and
   `WithoutModelTransforms` have ONE definition on both sides.
3. Write `tests/game/preset_host_stubs.h/.cpp` in the worktree against the OLD
   host signatures: `Gc2dHost::Load(dir)`, `LoadParticles(dir)`,
   `DrawParticles(cells)` and `Scene3dHost::LoadWithSetup` with the argument text
   this file documents. Today's tracked stubs have moved on with the engine
   (`LoadAsset`, `LoadUnion`, `DrawParticles(asset, cells)`, `SetCanvas`,
   `SetModelTime`), so they record different text and cannot be used as they are.
4. Write `tests/game/preset_golden_record.cpp`: for every scene of `ForBuild` and
   every choice, `PresetHost::Load({}, scene)` (an EMPTY game dir, which is what
   makes the recorded asset paths relative), `SetOption(0, choice)` when the preset
   has options, `Restart()`, take the pushes so far as `setup`, then
   `NaturalFrames()` calls to `RenderFrame(1/60)`, hashing each frame's push list
   twice (plain, and through `WithoutModelTransforms`) and storing the full list on
   the detail frames: every 64th frame, every `phase.start_frame` and every
   `phase.start_frame + 1`, and NOTHING else (a preset with no phases has detail
   only on the multiples of 64, which is how the committed files look). Write with
   `nlohmann::ordered_json` and `dump(1)` plus a trailing newline, keys in the order
   of the block above.
5. Add a `preset_golden_record` target to the worktree's `CMakeLists.txt` with
   `preset_host.cpp`, `scene_presets*.cpp`, `preset_params/schema/effective/rng`,
   the three test files, `r573_support`, `r573_formats` and `nlohmann_json`.
   Configure it with the main checkout's vcpkg toolchain and
   `-DVCPKG_MANIFEST_MODE=OFF -DVCPKG_INSTALLED_DIR=<main>/build/vcpkg_installed`,
   because a worktree has no `vendor/vcpkg` submodule.
6. Run `build/preset_golden_record.exe <fixtures-dir>` with a scratch fixtures
   directory holding a copy of `asset_lengths.json` and an empty `golden/`. It
   needs no game install, no GPU and no window.
7. Diff the result against the committed fixtures field by field. `preset`,
   `build`, `choice`, `frames`, `setup`, `hashes` and `detail` MUST be identical:
   they were, for all 40 fixtures, 62401 frame hashes and 10016 detail push lines,
   when `hashes_no_transform` was added. If any of them differs, STOP: the
   difference is in the recorder or the stubs, not in the game, and overwriting the
   fixture would destroy the reference.
8. Copy the files into `tests/game/fixtures/golden/` and remove the worktree with
   `git worktree remove`, so no worktree metadata or build output can reach
   `git status`.

## Tests

`tests/game/preset_golden_tests.cpp`, in `game_tests`:

- `the legacy golden fixture set covers every built-in preset` (`[golden]`, `ci`):
  18 presets, 40 fixtures, each one's `preset`, `build` and `choice` match the
  table it came from, `hashes.size() == frames`, `setup` is not empty, `detail`
  holds frame 0, and `frames` equals the end rule above. The expectation is
  re-derived from the `Preset::Scene` tables and `asset_lengths.json`, not from the
  recorder, so a recorder that silently produced 0 frames cannot pass.
- `the golden frame hash is stable and order sensitive` (`[golden]`, `ci`): the
  hash is reproducible, changes when two pushes swap order, and an empty frame does
  not collide with a non-empty one.

The coverage test also requires `hashes_no_transform` to have one entry per frame,
so a fixture recorded before that field existed cannot pass. The comparison was
checked against a mutated fixture (one `hashes_no_transform` entry of
`iidx11-attract` corrupted at frame 300, an EXCLUDED frame that the old comparison
skipped entirely) and it failed at frame 300, which is the proof that the 610 frames
are now really compared.

### The comparison test

`the new evaluator reproduces the legacy golden recording` (`[golden]`, `ci`) is
the acceptance criterion. For every one of the 40 (preset, choice) pairs it
converts the `Preset::Scene` with `Preset::FromScene`, asserts the document
validates with no error and that its `length` equals the recorded frame count,
then runs `Preset::Eval::Evaluator::RenderFrame(1/60)` once per recorded frame
and compares:

- the frame's push list against the fixture's full list on every detail frame, and
- the FNV-1a hash of that list against `hashes[frame]` on every other frame.

It reports the first differing frame, the first frame whose full list differs, and
both texts. `tests/game/preset_push_text.cpp` formats a `Push` into exactly the
text `preset_host_stubs.cpp` records, so the two sides are directly comparable;
`tests/game/preset_legacy_view.cpp` applies the tolerated list below and nothing
else.

## How the comparison applies the tolerated list

The fixture holds a hash per frame and a full push list only on detail frames, so
a difference can only be handled by changing the NEW side: a push the old host
made and the evaluator does not emit can never be hashed away. The evaluator
therefore emits the old host's push sequence and flags the pushes the old host
never made, and the comparison drops exactly those.

1. **`Push::legacy`.** Every push the evaluator produces says whether the old host
   made it. The pushes flagged `legacy = false` are the ones plan 6.3 item 1
   names: the per-frame `SetModelAlpha` of every visible model, the per-frame
   `SetView` and `SetProjection`, the camera vectors a `camera.tween` writes (the
   old host's `ApplyRamps` wrote the effective copy and pushed nothing), the model
   `scale` a tween writes, and `SetModelTime` and `SetSpriteFrame`, which have no
   old-host counterpart at all. The comparison drops them. What stays legacy-true
   is what the old host really pushed: the rebind block, `ApplyChoiceCamera`'s
   `SetView`, `ApplyIntro`'s `SetProjection` (a `camera.tween` writing `fov_y`),
   and the countdown's `anim_speed` and `blend_mode` (a `model.tween` writing
   them).
2. **`Push::model_moves`.** `SetModelTransform` for a model that fails the old
   host's `Moves()` gate (`ApplyMotion`) is dropped, which is plan 6.3 item 1's
   other half. The evaluator computes the same gate from the document: a non-zero
   resolved `spin_per_frame`, an orbit, a `spin_kick`, active jitter, or being the
   first model track of a document that has options.
3. **`Push::legacy_vec_b`.** Plan 6.3 item 3, the one spin step. Every
   `SetModelTransform` carries both rotations: the evaluator's own (a clip's first
   frame carries no spin) and the old host's (`AdvancePhase` zeroed the spins and
   `ApplyMotion` added a step in the same `Advance`, so the phase's first frame
   already carries one). The comparison formats the legacy one. It is a second
   accumulator rather than an addition after the fact because float addition is
   not associative and two of the presets integrate a RAMPED rate. Phase 0 is bit
   exact in both, and `preset_eval_tests.cpp` pins the difference on IIDX RED mode
   select at frames 0, 14, 15, 27, 28 and 40.
4. **The countdown arithmetic.** The old host computed the timer ramp as
   `speed_base + elapsed * speed_per_frame` once per frame; the document expresses
   the same straight line as a two-key linear tween, `A + (B - A) * k`. The two
   are the same function of the frame and differ only in float rounding, by at
   most two ulp. No key placement or evaluation order removes that: the values are
   not representable as the same sequence of float operations. The comparison
   therefore recomputes the old host's value from the `Preset::Countdown` of the
   scene it is replaying, requires the evaluator's value to agree within 1e-6 (the
   tolerance plan 3.9 states) and then uses the old value, so every other push on
   those frames is still compared exactly. The same substitution supplies the old
   host's `SetModelAlpha` on a ramped frame, which is the one alpha push the
   document cannot tell apart from a fade ramp's; the evaluator's own resolved
   alpha for that model is held to the same 1e-6 agreement before the substitution
   happens, so the fade ramp is compared and not merely replaced. The tolerance
   applies to those two values on ramped frames and to nothing else. The absolute
   values are pinned
   independently by `preset_eval_tests.cpp` at frames 1199, 1200, 1201, 1500 and
   1799 of IIDX 10 music select and 0, 1 and 179 of game over.

The drift check is skipped on the LAST recorded frame of a preset, where the old
host advanced once more into a frame that does not exist: at `start_frames` the
timer has run out and the ramp value is one step past the last drawn frame, while
the document's tween holds its last key. Plan 2.6 describes that frame as the one
the countdown never reaches.

## The one difference that is not reconcilable

One class of difference cannot be reproduced, so its frames are compared without
the pushes that carry it rather than tolerated inside them. It is listed here so it
is visible rather than silently absorbed.

**Transforms of hidden models, `iidx11-attract` frames 0..501 and 793..901.**
`ApplyMotion` never looked at visibility: a model that passes `Moves()` gets a
`SetModelTransform` every frame even while `SetModelVisibleByName` says it is
hidden. The attract screen's two hidden spans do exactly that, because
`kAttractHidden` leaves `motion.spin_per_frame` at the warp rate. In the document
a hidden model is the ABSENCE of a `model.draw` clip (plan 3.6), so it has no
pose, no rate and nothing to push; the pushes cannot be reproduced without
reintroducing the "invisible model with a pose" concept the format deliberately
drops. They have no visual effect, because the model is not drawn.

The comparison marks a frame excluded when the phase the frame's advance lands in
materializes a model that is hidden AND passes `Moves()`. That is only the two
attract spans: the ending's hidden phase sets the spin to zero, so it is compared
against the primary hash like everything else. 610 of the attract preset's 2456
frames are excluded this way. The count is asserted per preset, so the exclusion
cannot quietly grow: the test requires 610 excluded frames for `iidx11-attract` and
zero for the other 17.

On an excluded frame the comparison uses `hashes_no_transform` instead of `hashes`,
against the evaluator's own push list run through the same
`WithoutModelTransforms`. So every OTHER push of those 610 frames - the two
`DrawSprites` ranges, the particle `DrawParticles` lists, `AdvanceSprites` and the
per layer `SetSpriteScale` - is compared on every one of them, not only on the 13
detail frames inside the two spans, and what is given up is exactly the model
transforms of that frame and nothing else. When the fixture has a full push list for
an excluded frame, the list is still compared line by line after dropping the legacy
`SetModelTransform` lines of exactly the hidden models, so the VISIBLE models
transforms are compared there too.

Every frame of every fixture also cross-checks its detail list against
`hashes_no_transform`, which is what stops the second hash from drifting away from
the list it is supposed to summarise.

Two further legacy behaviours are reproduced by the evaluator rather than
tolerated, because the document can express them:

- The old host re-pushed the lead model's speed, alpha and blend a second time at
  the end of every rebind (`Rematerialize` ends in `SetCountdown`, which pushes
  while the timer is at or above `ramp_below`, and every preset with phases has a
  timer above it or no timer at all). The evaluator emits the same three pushes at
  a boundary. Part C, which pushes everything every frame, drops them.
- `ApplyIntro` kept writing the lead speed and the projection for as long as the
  PHASE lasted, not for `intro.frames`, and re-anchored to every phase start. The
  converter therefore gives the intro tween the phase's span, not the intro's
  length, and lets the last key hold. Plan 3.6 says "ending at intro.frames",
  which would stop the pushes at that frame; the recording shows they continue.

## Where the code is

| What | File |
|------|------|
| Fixture format, canonical push text, frame hash | `tests/game/preset_golden_format.h/.cpp` |
| Link-time `Scene3dHost` / `Gc2dHost` stubs and the asset table reader | `tests/game/preset_host_stubs.h/.cpp` |
| Coverage test and the evaluator comparison | `tests/game/preset_golden_tests.cpp` |
| Push text identical to the stubs' | `tests/game/preset_push_text.h/.cpp` |
| The tolerated list, applied | `tests/game/preset_legacy_view.h/.cpp` |
| Asset lengths | `tests/game/fixtures/asset_lengths.json` |
| Fixtures | `tests/game/fixtures/golden/*.json` |
