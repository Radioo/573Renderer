# The legacy preset golden fixture

This file describes the reference recording of the old preset host, taken before
milestone M2 (docs/preset_document.md lists the milestones) changed anything about it. The
new evaluator is accepted when it reproduces this recording: M2 part B replayed every
fixture against documents the converter produced, and M8 pointed the same comparison
at the built-in default documents of `src/preset/defaults/`. A recorded value is
never re-recorded to make a comparison pass. The fixture is re-recorded only to add
to the format, from the old host, and only when every value it already holds comes
back identical. The recording has changed twice since, and both changes are
documented below with the frames and the proof: the deterministic-math re-record,
and the HAPPY SKY light-channel transformation.

Status: the asset table, the frozen legacy view and the fixtures are in the tree,
and both the coverage test and the evaluator comparison run in the `ci` label. The
recorder is not in the tree. "The fixture is frozen, and how to re-record it" below
gives the whole procedure, including the commit that holds the old host.

M8 deleted the `Preset::Scene` tables, so the comparison no longer has a table to
read the old host's constants from. The three things it needed are frozen next
to the recording in `tests/game/fixtures/legacy_compat.json`, one entry per preset:
the lead model name (`models.front().model`), the `Preset::Countdown` fields
(`start_frames`, `ramp_below`, `speed_base`, `speed_per_frame`, `fade_from`,
`fade_per_frame`) and, per phase, the phase's `start_frame` and the models that
phase materializes hidden while they still pass `Moves()`. A throwaway test case
wrote it from the tables in the same change that deleted them, and it is frozen
like the recording: no reference value changed, and nothing in the recording was
re-recorded. The coverage test checks it against the documents (every entry's phase
starts are the document's marker frames, or the single phase 0 of a document with
no markers, and the lead model is a model that a `model.draw` clip names).

## What is recorded

`src/preset/preset_host.cpp` drives two modules with side effects:
`Scene3dHost` (`src/scene3d/scene3d_host.h`) and `Gc2dHost` (`src/gc2d/gc_host.h`).
Both are namespaces of free functions, so everything the old host does that can be
observed is the ordered sequence of calls it makes into those two namespaces. The
fixture is that sequence, per frame and per option choice.

Every call the old host makes is recorded, with every argument in full precision:

| Namespace | Recorded calls |
|-----------|----------------|
| `Scene3dHost` | `LoadWithSetup`, `Unload`, `RenderFrame`, `SetTime`, `SetModelSpeed`, `SetModelAlpha`, `SetModelBlendByName`, `SetModelScale`, `SetModelVisibleByName`, `SetModelTransform`, `SetProjection`, `SetView`, `SetStyle`, `SetLights` |
| `Gc2dHost` | `Load`, `Unload`, `LoadParticles`, `SetSprites`, `SetSpriteFrame`, `SetSpriteScale`, `AdvanceSprites`, `DrawSprites`, `DrawParticles`, `SetFrame` |

`Scene3dHost::GetStatus`, `Gc2dHost::ListSprites` and `Gc2dHost::AnimationLength`
are reads, not pushes, so they are answered from the asset table (below) and not
recorded. These are all the host functions `preset_host.cpp` references. No other
function is stubbed, because a stub nothing references would be dead code.

## The seam: link-time stubs, no change to the old host

The old `preset_host.cpp` was not modified while it was being recorded, and no
interface, sink or virtual seam was added to it.
`tests/game/preset_host_stubs.cpp` defines the `Scene3dHost::` and `Gc2dHost::`
functions listed above, and the recorder linked the real `src/preset/*.cpp` against
those definitions instead of `src/scene3d/scene3d_host.cpp` and
`src/gc2d/gc_host.cpp`. The stubs outlived the recorder: `game_tests` links the new
host against them for the host tests of `tests/game/preset_host_tests.cpp`, which is
why they also answer `Gc2dHost::DescribePackage`, `Scene3dHost::DescribeScene` and
`Gc2dHost::SetCanvas` from `asset_lengths.json`.

This worked because both hosts are namespaces of free functions with a single
global instance each, and because `game_tests` already
compiles the preset sources without linking `r573_app`. The stubs need no D3D
device, no window and no game files.

A recording sink inside `preset_host.cpp` was not used, because any seam added to
the old host changes the thing being measured. Even a pure refactor would have to
be trusted, while a link-time substitution cannot change the app at all: the app
target does not contain the stub translation unit. This milestone did not touch the
renderer's own build.

## The asset table

The stubs hold no game data, but the old host reads four values that come from
loaded assets. Only two of them are actual asset lookups:

| Read | Where in `preset_host.cpp` | Why it matters |
|------|---------------------------|----------------|
| `Scene3dHost::GetStatus().max_time` | `ClipFrames` | `NaturalFrames` for every preset with no countdown, and `PhaseTail` |
| `Gc2dHost::AnimationLength(name)` | `PhaseTail` | the longest animated 2D layer visible in the last phase |
| `Gc2dHost::ListSprites().size()` | `Restart` | how many `SetSpriteFrame(i, 0)` calls Restart makes; answered from the last `SetSprites` |
| model names, sprite names, indices | everywhere | come from the `Preset::Scene` tables, not from the assets |

`tests/game/fixtures/asset_lengths.json` supplies the first two. When a scene
directory or an animation name is missing from it, the stubs fail (the recorder
aborts with `asset table gap: ...`), so a silent zero cannot reach a fixture.

### Where every number came from

Every value was read from the real IIDX 10 and IIDX RED installs with tools
already in the tree.

| Key | Value | Source |
|-----|-------|--------|
| `data/graph/texture/music` | 240 | `573Renderer.exe --scene3d-test <game>/data/graph/texture/music out.png 1`, the `[Scene3d] scene 'music': ... 240 ticks` line. `max_time` was the maximum `max_key_time` over the scene's models when this was recorded; since 2026-08-19 it is the least common multiple of every key track's last key time (`Scene3d::LoopTicks`), which is the same 240 here and for every other converted dir (`src/scene3d/scene3d.cpp`, `LoadModels`) |
| `data/graph/texture/cube_x` | 60 | same command on `cube_x` |
| `data/graph/texture/ex01` | 480 | same command on `ex01` |
| `data/graph/texture/tranbox` | 120 | same command on `tranbox` |
| `data/graph/texture/samurai` | 60 | same command on `samurai` |
| `data/graph/model/red` | 240 | same command on IIDX RED's `data/graph/model/red`; cross-checked by `docs/export_pipeline.md`, which states the ending tail is `240 / 0.75 = 320` |
| `data/graph/sys/title` `TITLE` | 1736 | `573Renderer.exe --preset-test <game> iidx11-attract out.png 1740`, the `[Gc2d] layer 'TITLE': 1736 frames` line; the same 1736 is quoted in `docs/preset_states.md` for the attract phase boundary |
| `data/graph/sys/title` `TITLE_TAIKI` | 720 | same run, the `[Gc2d] layer 'TITLE_TAIKI': 720 frames` line, reached once the attract preset enters its last phase; `docs/export_pipeline.md` states attract is `1736 + 720 = 2456` |

| `data/graph/model/ex_bg` | 1000 | `--scene3d-test` on HAPPY SKY's `ex_bg`, the `max ticks` line (HAPPY SKY, not in the golden replay) |
| `data/graph/model/mode_bg` | 1500 | same, `mode_bg` |
| `data/graph/model/sky` | 600 | same, `sky`: the loop LCM of sky.xz 30, muring.xz 300 and the unbound muyaji.xz 120 |
| `data/graph/model/extra_st` | 640 | same, `extra_st` |
| `data/graph/model/dan` | 3000 | same, `dan`: the loop LCM of 600 (sea), 1000 (sky) and 1500 (light_bg) |
| `data/graph/sys/title` `LOGO_IN` | 120 | HAPPY SKY's `title` package (`--gc2d-sheet`), used by `iidx12-attract` |
| `data/graph/sys/card` `CARD_BG` | 120 | HAPPY SKY's and RED's `card` package, identical on both |
| `data/graph/sys/dan_e` `DAN_BG` | 600 | HAPPY SKY's `dan_e` package |

The key is the directory, and `data/graph/sys/title` exists in both installs with
different animation lengths: RED's `TITLE` is 1736 and `TITLE_TAIKI` 720 (above),
while HAPPY SKY's are 422 and 480. The fixture keeps RED's numbers because the
golden replay reads them. The HAPPY SKY documents carry an explicit `length`, and
`tests/game/preset_defaults_2d_tests.cpp` builds its own `TitleLengths()` with 422,
so nothing reads the wrong game's value today. Key the table by build before
feeding this fixture to an iidx12 document.

The old host queries only `TITLE_TAIKI` (it is the one animated layer visible in
the attract preset's last phase; the ending's `END_BG1` is not animated, so
`PhaseTail` skips it). `TITLE` is recorded next to it because the attract phase
boundary the tables already encode comes from it.

## The end rule, per preset

Each run is `PresetHost::Load` -> `SetOption(0, choice)` (only when the preset has
options) -> `Restart` -> `NaturalFrames()` calls to `RenderFrame(1/60)`. The frame
count is whatever `NaturalFrames` (`preset_host.cpp`) returns, which follows one of
three rules:

| Rule | Presets | Frames |
|------|---------|--------|
| `countdown.start_frames` | `iidx10-music-select` 1800, `iidx10-music-select-samurai` 1800, `iidx10-card-in` 3600, `iidx10-mode-select` 1200, `iidx10-dan-select` 1200, `iidx10-expert-select` 1800, `iidx10-new-player` 1200, `iidx10-game-over` 180, `iidx11-music-select` 3600, `iidx11-mode-select` 1200, `iidx11-dan-select` 1200, `iidx11-expert-select` 2700, `iidx11-new-player` 1200, `iidx11-card-in` 3600 | as listed |
| last phase start + `PhaseTail` | `iidx11-attract` = 1736 + 720, `iidx11-ending` = 4065 + 320 | 2456, 4385 |
| `ClipFrames` = `max_time / anim_speed` | `iidx10-login` = 240 / 1.0, `iidx11-login` = 240 / 1.0 | 240, 240 |

`SetOption` runs before `Restart` on purpose. That is the order the export path
uses (the GUI picks a choice, then the capture driver restarts), and `Restart` is the
one reset path that leaves `g_choices`, `g_transition` and `g_spin_kick` alone by
design (plan 2.5). A non-default choice therefore starts frame 0 with its option
transition already running, as it does in the app.

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
- `hashes` has `frames` entries, one per `RenderFrame` call: the FNV-1a
  64-bit hash of that frame's push list, 16 lowercase hex digits. Frame `i` covers
  the whole `RenderFrame(dt)` call, so it holds the draws for frame `i` followed by
  the `Advance()` that prepares frame `i + 1`.
- `hashes_no_transform` has the same `frames` entries taken over the same push
  list with every `Scene3dHost::SetModelTransform` line removed
  (`PresetGolden::WithoutModelTransforms`, the single definition that both the
  recorder and the comparison call). It covers the hidden-model gap described below:
  on a frame whose transforms cannot be reproduced, everything else on that frame
  still has a hash to compare against. The rule works per line and does not depend
  on the preset (every transform of every model on every frame of every preset), so
  the recorder holds no preset knowledge.
- `detail` carries the full push list for the frames a person or a failing test
  needs to read: every 64th frame, every phase start frame (the preset's markers) and
  the frame right after each phase start. That stride and marker rule lived in the
  recorder and is frozen into the committed files; the reader does not work it out
  again.

A push is canonicalised to one line of text, `Namespace::Call(arg, arg, ...)`.
Floats use `std::to_chars`'s shortest round-tripping form, so the text records the
float bits exactly and the formatting does not depend on the platform (nothing in it
is locale dependent); strings are quoted with `'`; `std::array<float, 3>` prints as
`[x y z]`; the compound arguments (`proj[...]`, `light[...]`, `model[...]`,
`sprite[...]`, `timing[...]`, `parts[...]`, `cell[...]`) are spelled out in
`tests/game/preset_host_stubs.cpp`. `model[...]` gained a leading instance name in
front of the mesh name when model tracks became instances
(docs/preset_document.md). That did not touch the committed fixtures, because the
golden reader only checks that the recorded `setup` line is present and replays the
per-frame hashes, which carry no `ModelSetup` at all. The hash is taken over that
canonical text, joined by newlines, so it is stable across runs, compilers and
platforms: nothing in it is a pointer, an address, an unordered container order or
a locale-dependent number.

Determinism was checked by recording twice into a copy and diffing: the two were
byte identical.

Platform-independent formatting does not make the values platform independent,
and for one release they were not. Because the text is exact to the last bit, a
value produced by `sinf` or `cosf` carried whatever last bit the machine's CRT
returned, and Microsoft's CRT does not return the same bit on every machine (see the
proof table in `docs/support.md`). The recording and the evaluator now both call
`Support::Sinf` / `Support::Cosf`, which are deterministic by construction, so the
fixture depends on the document and not on the machine that ran it.

## The first change to the recording: deterministic math

The fixture was re-recorded once, on 2026-08-18, for the deterministic-math change
and for nothing else. The rule at the top of this file allows this case: the
recording changed because the old host's own arithmetic changed in the worktree, in
the same way the evaluator's did, and not to make a comparison pass.

The procedure below was followed as written, with the deterministic functions
applied to the old host's six transcendental call sites in the worktree
(`OrbitPosition`, the `Curve::Sine` ramp, `RingPhase` and the two scatter angles in
`SpawnParticles`, all in `src/preset/preset_host.cpp` at `61d22b7`). It was run
twice, and the first run is the evidence:

1. With the worktree's `preset_host.cpp` left as `61d22b7` has it, the rebuilt
   recorder reproduced all 40 committed fixtures byte for byte. That proves the
   rebuilt stubs and recorder are the ones that made the committed files, so any
   later difference can only come from the arithmetic.
2. With `Support::Sinf` / `Support::Cosf` patched in, 38 of the 40 fixtures came
   back byte identical and 2 changed, by 5 frame hashes in total:

| Fixture | Frames | Changed `hashes` |
|---|---|---|
| `iidx10-dan-select.json` | 1200 | 121, 732, 919, 1153 |
| `iidx10-new-player.json` | 1200 | 121 |

`preset`, `build`, `choice`, `frames`, `setup` and every `detail` push list are
unchanged, and so is every entry of `hashes_no_transform`. That confirms
independently that the difference is only in `SetModelTransform` lines, the only
pushes that carry an orbit position. These are the two presets that orbit a model;
nothing else in the 40 calls `sinf` or `cosf` on a value that reaches a push. Three
of the four dan-select frames (732, 919, 1153) were predicted before the re-record
by sweeping the preset's 1211 orbit angles for a printed difference between this
machine's CRT and the correctly rounded result. Frame 121 is the remaining case:
there the vendored implementation, not the CRT, is one last bit off the correctly
rounded value.

## The second change to the recording: the HAPPY SKY light channels

HAPPY SKY milestone M8 (docs/game_profiles.md, "Preset options", not the preset editor's M8
named at the top of this file) is the planned re-record that "Growing the push
vocabulary without touching the fixture" below refers to. It changed two things at
once, and both are text inside the single `light[...]` token:

1. `LightText` (in `tests/game/preset_push_text.cpp` and in
   `tests/game/preset_host_stubs.cpp`, which must stay identical) now prints
   `ambient` as a fourth vector.
2. The converted documents' lights changed from `specular` white / `ambient` black
   to `specular` black / `ambient` white, because that is what the game sets: on
   both IIDX 10 and RED the light array is 8 x 104-byte `D3DLIGHT8`, the boot init
   `memset`s each entry (leaving `Specular` at 0), writes `Type = 3`, and then
   calls diffuse (+4), ambient (+36) and direction (+64). No instruction in either
   executable references the specular field, and the title update writes ambient
   white on lights 0 and 1 every frame. The proof is in
   `IIDX/red_3d_screens.md` and `IIDX/tenth_style_music_select.md`.

So every recorded `light[[dir][diffuse][1 1 1]]` became
`light[[dir][diffuse][0 0 0][1 1 1]]`. Nothing else in the recording changed.

This was a transformation of the recording, not a re-recording. The old host is
not in the tree, and patching its light table in the 61d22b7 worktree would have
meant editing the reference so the comparison passes. The recording was transformed
instead, and a throwaway test proved the transformation frame by frame
(`tests/game/preset_golden_relight.cpp`, run once in this change and then deleted,
like the `legacy_compat.json` extractor). For all 40 fixtures and all 62401 frames
it replayed the changed documents, mapped each produced call back to the old
three-vector light text, and required with `REQUIRE` that the result reproduce the
recorded `hashes`, `hashes_no_transform` and every `detail` push list byte for byte:
125408 assertions, all passing. Only then did it write the new hashes from the
four-vector text. Any difference outside the light token would have failed that
check, so what landed is the recording with one token rewritten.

The proof has a limit. The inverse map drops the ambient vector, so it shows that
nothing except the light token changed, but it cannot show that the ambient value is
right: any ambient would have passed, and the new hashes were then written from what
the evaluator emitted. The evidence for "ambient white" is the RE above and
`preset_eval_tests.cpp`, not the recording. From now on the recording does guard it:
flipping `StandardLights()` ambient back to black fails six fixtures.

The changes in full:

| Part | Files | Entries |
|---|---|---|
| `setup` | all 40 | the `LoadWithSetup` line and both `SetLights` lines in each |
| `detail` | none | no detail push list contains a light: `SetLights` is only emitted by `EmitRebind` |
| `hashes` / `hashes_no_transform` | 6 | 57 entries, all on rebind frames |

| Fixture | `hashes` frames | `hashes_no_transform` frames |
|---|---|---|
| `iidx11-attract.json` | 501, 901, 1735 | 501, 792, 901, 1735 |
| `iidx11-ending.json` | 70, 439, 813, 1184, 1556, 1929, 2299, 2474, 2485, 2671, 2764, 2857, 2903, 2949, 3033, 3786, 4064 | the same 17 |
| `iidx11-expert-select.json` | 41, 399, 549 | the same 3 |
| `iidx11-mode-select.json` | 14, 27, 39 | the same 3 |
| `iidx11-music-select.0.json` | 34 | 34 |
| `iidx11-music-select.1.json` | 34 | 34 |

The other 34 fixtures have no clip that starts or ends after frame 0, so they never
rebind inside the recorded range and no frame of theirs carries a `SetLights` push.

One entry was left as it was on purpose: `iidx11-attract.json` `hashes[792]`.
`Adapter::Excluded` keys on `PhaseAt(record_frame + 1)`, so the excluded record
frames are 0..500 and 792..900 (610 of them, which `HiddenModelFrames` asserts), and
record frame 792 is the first of the second span. On an excluded frame the recorded
`hashes` entry covers `SetModelTransform` lines for models the evaluator does not
emit at all (see "The one difference that is not reconcilable"), which is why the
comparison reads `hashes_no_transform` there instead. That entry cannot be derived
and is never read, so it still carries the pre-M8 light text, and this note records
it. The other two attract rebind frames, 501 and 901, are not excluded and were
rewritten normally.

## The fixture is frozen, and how to re-record it

The recorder (`tests/game/preset_golden_record.cpp`, target `preset_golden_record`)
exists only to drive the old `PresetHost` against the link-time stubs. M2 part C
replaced that host with the evaluator, so there is no legacy runtime left in the
tree to record, and the recorder is not in the tree either. An executable that
recorded the new host into the file the new host is checked against would make the
reference check itself, and a source file nothing builds would be dead code. The
recorder lives in the procedure below. It is rebuilt from the last commit that still
has the old host whenever the fixture format has to change, and never to "fix" a
difference.

The old host is commit `61d22b7` ("Add preset document structures and validation"),
the parent of the M2 part C change on `feature/fixes`. That tree has
`src/preset/preset_host.cpp` as the legacy state machine and the `Preset::Scene`
tables, but not the stubs, the format reader or the recorder, which are all part of
M2. So:

1. `git worktree add <scratch>/m2-legacy 61d22b7 --detach` somewhere outside the
   repo (a scratch directory, never a tracked path).
2. Copy today's `tests/game/preset_golden_format.h/.cpp` into the worktree
   unchanged, so the canonical push text, the frame hash and
   `WithoutModelTransforms` have one definition on both sides. Copy today's
   `src/support/math/float_trig.h/.cpp` in as well, add it to the worktree's
   `r573_support` sources, and replace every `std::sin` / `std::cos` in the
   worktree's `src/preset/preset_host.cpp` with `Support::Sinf` / `Support::Cosf`
   (six call sites). Otherwise the recording uses the recording machine's CRT
   and the fixture becomes machine dependent again.
3. Write `tests/game/preset_host_stubs.h/.cpp` in the worktree against the old
   host signatures: `Gc2dHost::Load(dir)`, `LoadParticles(dir)`,
   `DrawParticles(cells)` and `Scene3dHost::LoadWithSetup` with the argument text
   this file documents. Today's tracked stubs have moved on with the engine
   (`LoadAsset`, `LoadUnion`, `DrawParticles(asset, cells)`, `SetCanvas`,
   `SetModelTime`), so they record different text and cannot be used as they are.
4. Write `tests/game/preset_golden_record.cpp`: for every scene of `ForBuild` and
   every choice, `PresetHost::Load({}, scene)` (an empty game dir, which makes the
   recorded asset paths relative), `SetOption(0, choice)` when the preset
   has options, `Restart()`, take the pushes so far as `setup`, then
   `NaturalFrames()` calls to `RenderFrame(1/60)`, hashing each frame's push list
   twice (plain, and through `WithoutModelTransforms`) and storing the full list on
   the detail frames: every 64th frame, every `phase.start_frame` and every
   `phase.start_frame + 1`, and no others (a preset with no phases has detail
   only on the multiples of 64, as in the committed files). Write with
   `nlohmann::ordered_json` and `dump(1)` plus a trailing newline, keys in the order
   of the block above.
5. Add a `preset_golden_record` target to the worktree's `CMakeLists.txt` with
   `preset_host.cpp`, `scene_presets*.cpp`, `preset_params/schema/effective/rng`
   (all of which exist at 61d22b7 and were deleted by M8),
   the three test files, `r573_support`, `r573_formats` and `nlohmann_json`.
   Configure it with the main checkout's vcpkg toolchain and
   `-DVCPKG_MANIFEST_MODE=OFF -DVCPKG_INSTALLED_DIR=<main>/build/vcpkg_installed`,
   because a worktree has no `vendor/vcpkg` submodule.
6. Run `build/preset_golden_record.exe <fixtures-dir>` with a scratch fixtures
   directory holding a copy of `asset_lengths.json` and an empty `golden/`. It
   needs no game install, no GPU and no window.
7. Diff the result against the committed fixtures field by field, twice. Record
   once with the worktree's `preset_host.cpp` as `61d22b7` has it: all 40 files
   must come back byte identical, which proves the rebuilt recorder and stubs are
   the ones that made the committed files. Then record again with step 2's
   deterministic math patched in and diff that. In either run, `preset`, `build`,
   `choice`, `frames` and `setup` must be identical; they were for all 40
   fixtures, 62401 frame hashes and 10016 detail push lines when
   `hashes_no_transform` was added. If one of them differs, stop. The difference is
   in the recorder or the stubs, not in the game, and overwriting the fixture would
   destroy the reference.
8. Copy the files into `tests/game/fixtures/golden/` and remove the worktree with
   `git worktree remove`, so no worktree metadata or build output can reach
   `git status`.

## Tests

`tests/game/preset_defaults_golden_tests.cpp`, in `game_tests`:

- `the frozen legacy view covers every built-in preset` (`[golden]`, `ci`): the 18
  `legacy_compat.json` entries name a lead model each built-in really draws, and
  their phase starts are the document's markers.
- `the legacy golden fixture set covers every built-in preset` (`[golden]`, `ci`):
  18 presets, 40 fixtures, each one's `preset`, `build` and `choice` match the
  document it came from, `hashes.size() == frames`, `setup` is not empty, `detail`
  holds frame 0, and `frames` equals the end rule above. The expected count is
  computed again from the frozen countdown of `legacy_compat.json` and, for the two
  logins, from the document's own lead `anim_speed` against `asset_lengths.json`,
  not taken from the recorder, so a recorder that silently produced 0 frames cannot
  pass.
- `the golden frame hash is stable and order sensitive` (`[golden]`, `ci`): the
  hash is reproducible, changes when two pushes swap order, and an empty frame does
  not collide with a non-empty one.

The coverage test also requires `hashes_no_transform` to have one entry per frame,
so a fixture recorded before that field existed cannot pass. The comparison was
run against a mutated fixture (one `hashes_no_transform` entry of `iidx11-attract`
corrupted at frame 300, an excluded frame that the old comparison skipped entirely),
and it failed at frame 300. That shows the 610 frames are now really compared.

### The comparison test

`the default documents reproduce the legacy golden recording` (`[golden]`, `ci`)
is the acceptance test. For each of the 40 (preset, choice) pairs it
takes the built-in document from `Preset::Doc::BuiltIns()`, asserts the document
validates with no error and that its `length` equals the recorded frame count,
then runs `Preset::Eval::Evaluator::RenderFrame(1/60)` once per recorded frame
and compares:

- the frame's push list against the fixture's full list on every detail frame, and
- the FNV-1a hash of that list against `hashes[frame]` on every other frame.

It reports the first differing frame, the first frame whose full list differs, and
both texts. `tests/game/preset_push_text.cpp` formats a `Push` into the same text
`preset_host_stubs.cpp` records, so the two sides compare directly.
`tests/game/preset_legacy_view.cpp` applies the tolerated list below and nothing
else.

## How the comparison applies the tolerated list

The fixture holds a hash per frame and a full push list only on detail frames, so
a difference can only be handled on the new side: a push the old host made and the
evaluator does not emit can never be hashed away. The evaluator therefore emits the
old host's push sequence and flags the pushes the old host never made, and the
comparison drops those and only those.

1. **`Push::legacy`.** Every push the evaluator produces records whether the old
   host made it. The pushes flagged `legacy = false` are the ones plan 6.3 item 1
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
   already carries one). The comparison formats the legacy one. It is kept as a
   second accumulator, instead of being added afterwards, because float addition is
   not associative and two of the presets integrate a ramped rate. Phase 0 is bit
   exact in both, and `preset_eval_tests.cpp` pins the difference on IIDX RED mode
   select at frames 0, 14, 15, 27, 28 and 40.
4. **The countdown arithmetic.** The old host computed the timer ramp as
   `speed_base + elapsed * speed_per_frame` once per frame; the document expresses
   the same straight line as a two-key linear tween, `A + (B - A) * k`. Both
   are the same function of the frame and differ only in float rounding, by at
   most two ulp. No key placement or evaluation order removes that, because the two
   cannot be written as the same sequence of float operations. The comparison
   therefore recomputes the old host's value from the frozen countdown of
   `legacy_compat.json`, requires the evaluator's value to agree within 1e-6 (the
   tolerance plan 3.9 states) and then uses the old value, so every other push on
   those frames is still compared exactly. The same substitution supplies the old
   host's `SetModelAlpha` on a ramped frame, which is the one alpha push the
   document cannot tell apart from a fade ramp's. The evaluator's own resolved
   alpha for that model must agree within the same 1e-6 before the substitution
   happens, so the fade ramp is still compared, not just replaced. The tolerance
   applies to those two values on ramped frames and to nothing else. The absolute
   values are pinned separately by `preset_eval_tests.cpp` at frames 1199, 1200, 1201, 1500 and
   1799 of IIDX 10 music select and 0, 1 and 179 of game over.

The drift check is skipped on the last recorded frame of a preset, where the old
host advanced once more into a frame that does not exist: at `start_frames` the
timer has run out and the ramp value is one step past the last drawn frame, while
the document's tween holds its last key. Plan 2.6 describes that frame as the one
the countdown never reaches.

## The one difference that is not reconcilable

One kind of difference cannot be reproduced. Its frames are compared without the
pushes that carry it, instead of tolerating the difference inside them, and it is
listed here so it stays visible.

**Transforms of hidden models, `iidx11-attract` frames 0..501 and 793..901.**
`ApplyMotion` never looked at visibility: a model that passes `Moves()` gets a
`SetModelTransform` every frame even while `SetModelVisibleByName` says it is
hidden. The attract screen's two hidden spans do this, because `kAttractHidden`
leaves `motion.spin_per_frame` at the warp rate. In the document a hidden model is
one with no `model.draw` clip (plan 3.6), so it has no pose, no rate and nothing to
push. Reproducing those pushes would need the "invisible model with a pose" concept
that the format leaves out on purpose. They have no visual effect, because the
model is not drawn.

The comparison marks a frame excluded when the frame's advance lands in a phase
whose `hidden_movers` entry in `legacy_compat.json` is not empty. That entry is the
frozen answer to whether the phase "materializes a model that is hidden AND passes
`Moves()`". Only the two attract spans qualify: the ending's hidden phase sets the
spin to zero, so it is compared against the primary hash like everything else. 610
of the attract preset's 2456 frames are excluded this way. The count is asserted per
preset, so the exclusion cannot quietly grow: the test requires 610 excluded frames
for `iidx11-attract` and zero for the other 17.

On an excluded frame the comparison uses `hashes_no_transform` instead of `hashes`,
against the evaluator's own push list run through the same
`WithoutModelTransforms`. Every other push of those 610 frames (the two
`DrawSprites` ranges, the particle `DrawParticles` lists, `AdvanceSprites` and the
per layer `SetSpriteScale`) is therefore compared on all of them, not only on the
13 detail frames inside the two spans, and only the model transforms of that frame
go uncompared. When the fixture has a full push list for an excluded frame, the list
is still compared line by line after dropping the legacy `SetModelTransform` lines
of the hidden models only, so the visible models' transforms are compared there too.

Every frame of every fixture also checks its detail list against
`hashes_no_transform`, which keeps the second hash in step with the list it
summarises.

## Presets that are not conversions are outside the fixture

The fixture is the recording of the 18 documents converted from the legacy
tables of IIDX 10 and RED. Built-ins for later builds (HAPPY SKY's `iidx12-*`
documents, authored from the sweep in `IIDX/happy_sky_3d_screens.md`, never from
a legacy table) have no legacy counterpart to compare against, so
`AllDocuments()` in `tests/game/preset_defaults_golden_tests.cpp` keeps only the
`iidx10` and `iidx11` builds and every count in that file stays at 18 documents,
40 fixtures. `preset_defaults_tests.cpp` and `--preset-test` on the install check
those documents instead. One consequence: `iidx12_mode_defaults.cpp` computes its
camera keys with `std::sin` in double. That is fine for a document whose numbers are
only compared against the same computation, but it is not `Support::Sinf`
(docs/support.md), so before a HAPPY SKY document is added to a bit-compared
fixture, that computation has to move onto the deterministic functions.

## Growing the push vocabulary without touching the fixture

A milestone that adds a push kind or a light channel must leave all 40 recorded
files byte-identical, and there are two ways to do that. The HAPPY SKY M1 change
used both, and they are the rule for every change after it.

1. **A new push kind is emitted with `legacy = false`.** `PushCall::SetClearColor`
   (M1) and `PushCall::SetFog` (M4) are emitted once per frame by
   `EmitUnconditional`, like `SetModelTime` and `SetSpriteFrame`, and the
   comparison drops them with the rest of the `legacy = false` list
   (`Adapter::Filter`, `preset_legacy_view.cpp`). No recorded line changes, on any
   frame, for any preset. Emitting the push only when a document sets a non-default
   value was considered and rejected: the emitted push list would then depend on a
   value instead of on the document's shape, a preset that tweens back to the
   default would silently stop pushing, and the renderer would carry that fragility
   for good just to protect a test fixture.
2. **A new field on an existing push is not added to the canonical text.**
   `LightPush::ambient` travels in `SetLights`, which the old host really made, so
   its text is compared line by line. `LightText` in `tests/game/preset_push_text.cpp`
   therefore printed only direction, diffuse and specular from M1 to M7. Printing
   ambient would have appended `[0 0 0]` to every `Scene3dHost::SetLights` line of
   every fixture and forced a re-record for a value that was black in all 18
   converted documents. In exchange, a difference in ambient alone was invisible to
   the golden comparison; `preset_eval_tests.cpp` covered it instead, by asserting
   the ambient that a document light and a `light.set` clip put on the push. M8 made
   ambient non-black in every converted preset, which is the planned re-record this
   rule expected, and the text gained the channel then. "The second change to the
   recording: the HAPPY SKY light channels" above describes what changed and how it
   was proven.

The evaluator reproduces two more legacy behaviours instead of tolerating them,
because the document can express them:

- The old host re-pushed the lead model's speed, alpha and blend a second time at
  the end of every rebind (`Rematerialize` ends in `SetCountdown`, which pushes
  while the timer is at or above `ramp_below`, and every preset with phases has a
  timer above it or no timer at all). The evaluator emits the same three pushes at
  a boundary. Part C, which pushes everything every frame, drops them.
- `ApplyIntro` kept writing the lead speed and the projection for as long as the
  phase lasted, not for `intro.frames`, and re-anchored to every phase start. The
  converter therefore gives the intro tween the phase's span, not the intro's
  length, and lets the last key hold. Plan 3.6 says "ending at intro.frames",
  which would stop the pushes at that frame; the recording shows they continue.

## Where the code is

| What | File |
|------|------|
| Fixture format, canonical push text, frame hash | `tests/game/preset_golden_format.h/.cpp` |
| Link-time `Scene3dHost` / `Gc2dHost` stubs and the asset table reader | `tests/game/preset_host_stubs.h/.cpp` |
| Coverage test and the evaluator comparison | `tests/game/preset_defaults_golden_tests.cpp` |
| Push text identical to the stubs' | `tests/game/preset_push_text.h/.cpp` |
| The tolerated list, applied, and the frozen legacy view reader | `tests/game/preset_legacy_view.h/.cpp` |
| Asset lengths | `tests/game/fixtures/asset_lengths.json` |
| The frozen legacy view of the deleted tables | `tests/game/fixtures/legacy_compat.json` |
| Fixtures | `tests/game/fixtures/golden/*.json` |
