# Scene preset parameters

Every value a scene preset transcribes from the game is a named parameter whose
default IS the game's value, read out of the constexpr table at load. Nothing is
typed twice: the schema declares what a parameter is, the table supplies what it
is worth.

## The four objects

```
Preset::Scene        constexpr, .rdata, never mutated, parsed by the layer gate
       |
       |  Materialize(scene, tweaks)
       v
Preset::Effective    mutable copy, what every per-frame reader reads
       ^
       |  a sparse id to value override set, authored by the UI
Preset::TweakSet
```

Setting a parameter, clearing one, resetting a group and resetting everything are
the same operation: edit the sparse tweak set, then re-materialize. There is no
second code path that can drift, and reset needs no stored copy of the original.

## Declarations and instances

A declaration is written once, in `preset_schema.cpp`. Instances are derived from
the loaded preset, so a scene with four models gets four copies of every model
row. No count is ever written down as a literal.

Ids are name keyed, so a tweak survives a table edit that reorders layers:

```
sprite_split_priority
camera.fov_y
light[0].diffuse
model[core].alpha
model[core].motion.spin_per_frame
sprite[BG_SKY].scroll_x
option[course].choice[10TH DAN].camera_eye
```

## Apply kinds

`Live` means an existing per-frame reader picks the new value up next frame.
`Rebind` means one push calls a host setter after the edit. Which one a row is
follows from where the value is read, not from preference.

## Ranges never quantize

`Range::step` is a UI hint for drag speed and slider granularity. It is never
applied to the stored value, because snapping would make the game's own value
unrepresentable: IIDX's `camera.fov_y` of 1.0471976 does not sit on a 0.001 grid.
A test asserts `ClampValue(desc, fallback) == fallback` for every parameter
instance of every shipped preset in both builds. When that test fails it means
the RANGE is wrong, never the preset.

That test has already caught two real ones: `countdown.speed_per_frame` is
negative on IIDX 10's game over screen, and the camera's near plane is authored
as 0 on every IIDX RED screen.

## What is not a parameter, and why

| Thing | Reason |
|---|---|
| `scene_dir`, `model`, `package_dir`, `sprite`, `hidden_parts` | Layer identity is what `check_preset_layers.py` parses out of source and matches against a background verdict. Editable identity would route around the gate and put unvetted layers, or chrome, on screen. |
| Render width and height | The window and device are sized before init and there is no device reset hook. Per-game resolutions are fixed. |
| Which models load | A different set needs a reload. `visible` covers the hide-only case over the vetted set. |
| Number of lights | Fixed by the table; the effective copy is sized at materialize. |
| Per-model animation loop range | The 3D clock wraps globally at the scene's max time. Needs engine work, not a schema row. |
| Tick rate, 3D and 2D | No preset produces a value other than 60, and moving one without the other desynchronises the clocks. They move together or not at all. |

## States are chosen, not edited

`Preset::Option` is the game's own per-mode or per-phase machine. Which choice is
active is per-session state, set by the workspace's GAME STATES band and never
serialized. What IS a parameter is the shape of that machine: each choice's
position, each choice's camera eye, the transition length and its per-frame step
(the game's own step was a hardcoded 4, which made the length read in quarter
frames), and the spin kick.

A choice can move the model, the camera, or both. IIDX RED's class course select
is the camera case: all seventeen courses share one model pose and differ only in
where the camera settles.

## Tweak files

```bash
573Renderer.exe --preset-test <game-dir> <preset-id> out.png 120 0 --preset-tweaks tweaks.txt
```

One override per line, `id = x y z i`, blank lines and `#` comments ignored:

```
camera.fov_y = 2.6 0 0 0
model[core].alpha = 1.0 0 0 0
sprite[BG_SKY].scroll_x = 2.0 0 0 0
```

Overrides never load by themselves. A preset with a tweak file sitting next to it
applies zero overrides, so `--preset-test` exit 8 keeps meaning "this preset is
frozen" rather than "the user tweaked it to a standstill".

The plan called for JSON here. This ships a line format instead, because the file
is a flat id to numbers map, the repo has no JSON parser today, and adding one
would pull a dependency into the two test targets as well as the renderer. If the
format needs nesting or string values, that trade goes the other way and the
dependency is the right call.
