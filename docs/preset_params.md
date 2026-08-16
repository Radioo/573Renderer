# Scene preset parameters

> RETIRED SURFACE. The "Screen parameters" pane and the tweak-set override model this
> document describes are gone. A preset is a `Preset::Doc::Document` and every value in it is
> edited in the timeline editor: the clip properties modal and the Inspector "Clip" tab build
> their forms from `FieldsFor(command type)`, and the document-level keys (name, notes, fps,
> length, rng_seed, render, camera, lights) are edited in the Document properties modal. See
> docs/preset_document.md for the schema and docs/gui.md 3.5 for the editor. What survives of
> this file is the parameter MEANINGS and the unit notes below, kept until they have all been
> carried into docs/preset_document.md.

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

## The Beat and noise group

Four scopes exist purely so the timing a screen derives arithmetically is editable
rather than hard-coded, and they are all populated by IIDX RED's ending:

```
rng.seed            the seed for Preset::Ran3, the game's own subtractive generator
beat.rate           beats counted per beat.span frames, 0 disables the beat
beat.span
beat.offset_a       the frame each of the two beat grids counts from
beat.offset_b
pulse.grid          which grid the model scale pulse fires on
pulse.scale_odd     scale on the frame an odd beat lands, decaying to 1 over
pulse.scale_even    pulse.frames, and the same for even beats
pulse.frames
jitter.from_frame   the scene frame per-frame position jitter starts on
jitter.span         width of the draw, 0 disables it, the offset is draw - span/2
jitter.scale        world units per step of the draw
```

`pulse` and `jitter` are phase-scoped in practice: a phase sets them through the
same `ParamOverride` list it uses for everything else, exactly as `intro` works,
so the ending's four pulsing phases and two shaking phases are table data and the
user can still retune all of it live. The current beat index, the frames since it
changed, the pulse being applied and the offset drawn this frame all show in the
preset panel, so the arithmetic is visible rather than inferred.

`rng.seed` is a parameter because the GAME has no reproducible seed to copy: IIDX
RED reseeds from `timeGetTime()` when a stage starts and gameplay then spends an
unknown number of draws before the ending runs. Every other input to the ending is
derived exactly; this one is genuinely a per-play accident, so it is exposed with a
default rather than guessed. See `docs/preset_states.md` for the full account.

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

## The tweak file is retired

A preset's user-editable form is now the JSON document (docs/preset_document.md):
`--preset-export-json <build> <id> <out.json>`, edit, `--preset-json <file>`.
`--preset-tweaks` no longer exists; naming it makes the process print that
sentence and exit 2 instead of silently ignoring the flag, and
`PresetHost::LoadTweaks` went with it. The line format it used to take
(`id = x y z i`, one override per line) is described in the git history of this
file; nothing reads it any more.

The live parameter overrides in the Parameters pane are unaffected: they are a
session-only override set on top of the loaded document
(`PresetHost::SetParam` / `ResetParam`), never a file.

## Ids must be unique, and a repeated layer proves it

A screen may place the same layer twice. IIDX 10's music select draws `BG_SKY`
twice, half a screen apart, to make the scrolling sky band. Keying ids on the
name alone gave both copies the same id, which silently merged two independent
layers into one row and produced a real ImGui ID conflict on screen.

A repeated name now takes an occurrence suffix, so the second copy is
`sprite[BG_SKY#2]` and both are separately addressable. The first occurrence is
never suffixed, so ids stay readable and stable for the common case.

Two tests hold this: every parameter id is unique within every preset of both
builds, and IIDX 10 music select specifically must expose two `BG_SKY` sets where
a tweak on one moves only that copy.

Each instance also carries its own GROUP, naming the layer it belongs to
("Model core", "2D layer BG_SKY#2"), so a row is never an anonymous "Visible" or
"X px" with no way to tell which layer it drives. Group reset matches on the
instance group, not the schema's static one.
