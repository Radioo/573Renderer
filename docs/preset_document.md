# Scene preset documents

A scene preset document is the owning, JSON round-trippable timeline that replaces
the `constexpr Preset::Scene` tables: tracks of clips on a frame axis, plus the
document-wide render, camera, light, asset, option and marker blocks. This file is
the single source of truth for the schema, the command catalog, the canonical
serialization rules and the validation rules.

Status: milestones M1 to M3 of `docs/scene_preset_editor_plan.html` are in the
tree. Documents now drive the app end to end: the host evaluates a document
(`preset_host.cpp` on `src/preset/eval/`), the 18 shipped screens ARE documents
built in code (`src/preset/defaults/`, below), and the Screens panel and the
preset CLI resolve them through the registry. What is left of the old world is the
`Preset::Scene` tables plus `preset_convert.cpp`, kept only so the tests can prove
the defaults still equal the converter output; nothing in the app reads a table
any more. The timeline editor (M4 onward) is not here yet.

The legacy behaviour of the old host is a committed fixture, so the evaluator was
built against a frozen reference instead of against a host that had to survive the
rewrite. See `docs/preset_golden.md` for what was recorded, the link-time stub seam
it used, the asset lengths it needed and the differences the new evaluator is
allowed to have.

## Where the code is

| What | File | Entry points |
|------|------|--------------|
| Enums and their JSON name tables | `src/preset/doc/preset_enum_names.h/.cpp` | `kCommandTypeNames`, `kEaseNames`, ..., `IndexForName`, `NameForIndex` |
| One struct per command, the `Command` variant, the command traits | `src/preset/doc/preset_commands.h` | `Command`, `ParamValue`, `TypeOf`, `TraitsFor`, `IsEvent`, `kCommandTraits` |
| Document, Track, Clip, Key, Gate, OptionSpec, Marker, Asset, render/camera/light blocks | `src/preset/doc/preset_document.h` | `Document`, `kSchemaId`, `kSchemaVersion`, `HasTarget` |
| One `FieldDesc` per command parameter | `src/preset/doc/preset_fields.h/.cpp` | `FieldsFor`, `KeyFieldsFor`, `FindField`, `DefaultCommand` |
| JSON load and save | `src/preset/doc/preset_json.h/.cpp` | `Load`, `Save`, `ParseError`, `Loaded` |
| Validation | `src/preset/doc/preset_validate.h/.cpp` | `Validate`, `Problem`, `Severity` |
| Key sampling and the eases | `src/preset/eval/eval_tween.h/.cpp` | `SampleKeys`, `EaseFactor`, `BlendValues`, `TweenValue` |
| Per kind clip application | `src/preset/eval/eval_models`, `eval_sprites`, `eval_camera_lights`, `eval_scene` | `ApplyModelDraw`, `ApplySpriteAnimate`, `ApplyCameraSet`, `ReadTarget`, `WriteTarget`, `BeatIndex`, `PulseFactor` |
| The stateful part | `src/preset/eval/eval_particles.h/.cpp`, `eval_state.h` | `SpawnParticles`, `AgeParticles`, `DrawJitter`, `EvalState` |
| The frame the evaluator resolves and what it pushes | `src/preset/eval/frame_state.h`, `eval_push.h` | `FrameState`, `ModelSlot`, `SpriteSlot`, `Push`, `PushCall` |
| The evaluator | `src/preset/eval/preset_evaluator.h/.cpp` | `Evaluator::Load`, `Reset`, `Seek`, `SetOption`, `RenderFrame`, `Resolve` |
| The one-shot converter from the old tables | `src/preset/preset_convert.h/.cpp` | `FromScene` |
| Asset lengths the evaluator and the converter need | `src/preset/preset_asset_lengths.h` | `AssetLengths::MaxTime`, `AnimationLength` |
| Tests | `tests/game/preset_json_tests.cpp`, `preset_validate_tests.cpp`, `eval_tween_tests.cpp`, `preset_eval_tests.cpp`, `preset_convert_tests.cpp`, `preset_defaults_tests.cpp`, `preset_registry_tests.cpp`, `preset_golden_tests.cpp` | fixtures in `tests/game/fixtures/` |

Everything lives in `namespace Preset::Doc`. The nested namespace is deliberate:
the old table structs (`Preset::ParamOverride`, `Preset::ModelMotion`,
`Preset::Camera`, `Preset::Option`, ...) keep their names in `namespace Preset`
until the last milestone deletes them, and the converter of M2 has to include both
headers at once.

JSON is `nlohmann-json` (vcpkg port `nlohmann-json`, header only), used through
`nlohmann::ordered_json` so key order is what the writer wrote. It is a header-only
dependency of the preset JSON layer, so only the targets that compile
`preset_json.cpp` need it.

## Top level

| Key | Type | Meaning |
|-----|------|---------|
| `schema` | string | Always `573renderer/scene-preset`. A different value is a load error. |
| `version` | int | Schema version, currently 1. A higher version is a load error; there are no older versions to migrate from yet, so any other value is refused as well. |
| `id` | string | Stable identifier. Unique within a build. |
| `name` | string | Human label. |
| `build` | string | Game fingerprint id (`iidx10`, `iidx11`, `src/game_fingerprint.cpp`). |
| `fps` | int | Frames per second the frame axis is defined in. Default 60. |
| `length` | int or `"auto"` | Document length in frames. `"auto"` (parsed as an absent `Document::length`) means the evaluator derives it; every converted document writes a number. |
| `render` | object | `width`, `height`, `opaque`, `shading`, `sprite_split_priority`. Written in full. |
| `camera` | object | `eye`, `at`, `up`, `fov_y` (rad), `near_z`, `far_z`, `aspect` (positive number or `"auto"`). Written in full. |
| `lights` | array | Each entry `direction`, `diffuse`, `specular`. Written in full. |
| `assets` | object | `"<asset id>": { "kind": "scene3d" or "package2d", "dir": "<game relative dir>" }`. Order is preserved. |
| `options` | array | The only storage of selectable options, see "Options" below. |
| `rng_seed` | int | Seed of the shared `Ran3` stream at frame 0. |
| `markers` | array | `{ "frame": int, "label": string }`, sorted by frame, one per frame. A marker names the state the screen is in from that frame to the next marker, including spans with no clips at all. |
| `tracks` | array | Ordered tracks; track order is evaluation order. |
| `notes` | string | Free text. Omitted when empty. |

Numbers are stored as `double` (the evaluator narrows to `float` at use) and
written by nlohmann's writer, so parse, serialize, parse is lossless. Vec3 is
`[x, y, z]`, colours are `[r, g, b]`, 2D points are `[x, y]`, angles are radians
in the file.

## Units and time

- The frame is the clock: integer, starting at 0, axis `[0, length)`.
- Seconds are display only (`frame / fps`).
- Clip-relative time: a tween key `at`, an emitter `reach_frames` and a sprite
  `offset` are all counted from the clip start.
- Angles are radians for model rotation, spin and camera `fov_y`, and degrees for
  the fields the game itself expresses in degrees (emitter ring angles, the
  `sine_deg` rate). Every `FieldDesc` carries its unit.
- 2D coordinates are pixels on the document canvas (`render.width` by
  `render.height`); the sprite scale pivot is `(x + render.width/2, y +
  render.height/2)`, which is what `Gc2dHost::SetCanvas(width, height)` makes true
  (it replaced the fixed 640x480 canvas and the 320/240 pivot literals in M2).
- 3D clip time is ticks at 60 ticks per second. `render.ticks_per_second` is
  deliberately NOT a document key: every `anim_speed` in the game tables is
  expressed against 60 ticks per second, so a per-document rate would only rescale
  every speed and add a second time unit.

## Tracks, clips, keys and gates

A track is `id`, `name`, `kind` (`sprite`, `model`, `camera`, `light`, `fx`,
`scene`), `target`, the `muted` / `solo` / `locked` flags, an optional `color`, and
its `clips`. `name` may be left out of the file: it then takes the track's
`target`, or the track's kind name on the kinds that have no target. The writer
always emits it, so a document that omits it is rewritten with the resolved name.
`target` is the instance the track's clips act on: a model name for a
model track, a document-unique sprite instance name for a sprite track (two
placements of one animation are two targets, `BG_SKY` and `BG_SKY_2`). Only sprite
and model tracks carry a target (`HasTarget`).

A clip is `id`, `type` (a command type name), `start` (inclusive), `end`
(exclusive, `null` for open-ended), an optional `when` gate, an optional `label`,
`muted`, `params` and `keys`.

There is no `option` track kind: the options track the editor draws is a view of
`document.options`, and it is never serialized.

Gates have three forms, and one gate names exactly one option:

```json
{ "option": "mode", "choice": "EXPERT" }
{ "option": "mode", "choices": ["EXPERT", "CLASS COURSE"] }
{ "option": "mode", "not": "EXPERT" }
```

Keys are `at` (frames from the clip start), `ease`, `rate_deg` (only with
`sine_deg`), `cp` (only with `bezier`), and `values`, an object of parameter id to
value. Key values are typed by the same field descriptors the params use, and the
tweenable set of a `model.tween` is the `model.draw` catalog, of a `camera.tween`
the `camera.set` catalog (`KeyFieldsFor`).

### Overlap families

`TraitsFor(type)` gives each command its track kind and its family. A family with
a value other than `Family::None` is a PRIMARY: `sprite.draw` and
`sprite.animate` share the sprite family, `model.draw`, `camera.set`, `light.set`
(per `index`), `render.settings`, `rhythm.beat` and `rng.seed` are their own.
Everything else is a modifier and may overlap freely. Two primaries of one family
may not overlap on one target unless their gates are mutually exclusive. Two
gates are mutually exclusive when they name the same option and no choice
satisfies both: two positive gates whose choice sets are disjoint, or a `not`
gate and a positive gate whose whole choice set is the negated one. Two `not`
gates are never treated as exclusive, since that would depend on the option
having exactly the two negated choices.

## Command catalog

"T" marks a tweenable parameter (one that may appear in `keys[].values`). Every
parameter is optional in the file and takes the catalog default when absent; the
default is the member initializer of the command struct, and `DefaultCommand`
hands the JSON writer the same value so "equal to the default" is decided in one
place. Ranges come from the rows of `preset_schema.cpp` where one exists; a soft
range is a UI hint that validation never enforces, a hard range is an error.

### sprite.draw (sprite track, primary)

| Param | Kind | Default | T |
|-------|------|---------|---|
| `asset` | package2d asset id | required | |
| `cell` | string | required | |
| `x`, `y` | float px, soft -4096..4096 | 0 | T |
| `alpha` | float 0..1 | 1 | T |
| `scale` | float soft 0.05..16 | 1 | T |
| `blend` | enum `normal` / `additive` / `subtract` / `replace` | `normal` | |
| `priority` | int 0..64 | 0 | |

### sprite.animate (sprite track, primary)

| Param | Kind | Default | T |
|-------|------|---------|---|
| `asset`, `animation` | asset id, string | required | |
| `x`, `y`, `alpha`, `scale` | as sprite.draw | | T |
| `priority` | int 0..64 | 0 | |
| `playback` | enum `loop` / `hold_last` / `hide_after_end` | `loop` | |
| `loop_start`, `loop_end` | int frames | 0 | |
| `offset` | int frames | 0 | |
| `clock` | `null` / `"continue"` / `"restart"` | `null` (auto) | |
| `speed` | float 0..8 soft, animation frames per document frame | 1 | T |
| `hidden_parts` | string list | [] | |

`sprite.animate` has NO `blend`: `GcAnim::Evaluate` picks every node's blend from
the package's alpha pair rule (`SelectBlend`), which is the game's mechanism. A
`blend` key on an animate clip is a validation warning and is ignored.

`clock` auto means: continue when the clip starts on the `end` of a clip of the
same target on the same track, restart otherwise.

### sprite.scroll (sprite track, modifier)

| Param | Kind | Default | T |
|-------|------|---------|---|
| `scroll_x` | float px/frame, -64..64 | 0 | T |
| `scroll_wrap` | float px | 0 | |
| `scroll_offset` | float px | 0 | |

The layer it scrolls is the track's `target`; the clip must overlap a sprite clip
on the same track.

The scroll displacement is `fmod(scroll_offset + clock * scroll_x, scroll_wrap)`
on the sprite instance's own clock (`Gc2d::ScrollOffset`, `src/gc2d/gc_sprite.cpp`),
and the sprite is drawn at `x` minus that displacement. `scroll_offset` is
therefore the scroll POSITION of the frame where the instance's clock is 0: it is
a px position, not a clock, and it is added to the wrapped displacement rather
than to the clock. Because the displacement is a pure function of the clock, an
instance whose clock carries over from a previous clip (`SetSprites` carries the
clock by `target`, `src/gc2d/gc_host.cpp`) continues along the same line and the
offset is not re-applied: a scroll clip that abuts another on the same target
scrolls on, exactly as the sprite frame does. With `scroll_wrap` at or below 0
there is no scroll and the displacement is 0, offset included. The converted
defaults all leave `scroll_offset` at 0, so the golden recording is unaffected;
the field replaces the Screens panel's live "scroll offset" slider.

### emitter (fx track, modifier)

| Param | Kind | Default | T |
|-------|------|---------|---|
| `asset`, `cell` | asset id, string | required | |
| `spawn` | enum `clip_start` / `every_frame` / `beat` | `clip_start` | |
| `count` | int | 0 | |
| `angle_step_deg`, `phase_rate_deg`, `phase_amplitude_deg` | float deg | 0 | |
| `radius_from`, `radius_to` | int px | 0 | |
| `reach_frames` | int frames | 0 | |
| `center` | [x, y] px | absent, meaning the canvas centre | T |
| `priority` | int 0..64 | 0 | |
| `blend` | enum, same table as sprite `blend` | `additive` | |
| `scale_percent` | int percent | 100 | |
| `scatter` | `{ "span": [x, y], "offset": [x, y] }` or null | null | |
| `beat_grid`, `beat_odd` | enum `a` / `b`, int 0..1 | `a`, 0 | |
| `life`, `life_base`, `life_span` | int frames | 0 | |

The ring phase is a function of the ABSOLUTE document frame, not the clip-relative
one, because the game rotates the whole ring on its own frame counter. Life is
clamped to at least 1 by the evaluator.

### model.draw (model track, primary)

| Param | Kind | Default | T |
|-------|------|---------|---|
| `asset` | scene3d asset id | required | |
| `model` | string | empty, meaning the track target | |
| `blend_mode` | enum `opaque` / `opaque_1` / `alpha` / `additive` / `subtract` | `opaque` | T (hold) |
| `alpha` | float 0..1 | 1 | T |
| `anim_speed` | float ticks/frame, 0..8 soft | 1 | T |
| `position` | vec3 | 0 | T |
| `rotation` | vec3 rad | 0 | T |
| `scale` | vec3 | 1 | T |
| `spin_per_frame` | vec3 rad/frame, -0.5..0.5 | 0 | T |
| `clip_time` | `"continue"` / `"restart"` / int ticks | `"continue"` | |

`opaque` and `opaque_1` render identically (`ApplyBlendMode` treats mode 1 as
opaque); the second name exists only so the int survives a round trip.

`spin_per_frame` is the RATE. The spin itself is a per-model accumulator in the
evaluator's stateful step, zeroed when a draw primary with a different `start`
becomes the winner, so a keyed or choice-changed rate is simply integrated.

### model.tween (model track, modifier)

No params; its whole content is `keys`. Tweenable values are the `model.draw`
fields marked T. A tween writes the base pose, and the accumulated spin is added on
top of the rotation afterwards.

### model.motion (model track, modifier)

| Param | Kind | Default |
|-------|------|---------|
| `orbit` | `{ "radius", "rate_rad_per_frame", "center": [x, y], "z_start", "z_per_frame", "z_min" }` or null | null |
| `spin_kick` | float 0..64 | 0 |
| `spin_kick_decay` | float 0..8 | 0 |
| `pulse` | `{ "grid": "a" or "b", "scale_odd", "scale_even", "frames" }` or null | null |

`spin_kick` is armed once at the clip start and the evaluator applies
`max(1, spin_kick)`, so the default 0 means "no kick". `spin_kick_decay` is the
ONLY decay, for the motion kick and for the option transition kick alike.

### camera.set (camera track, primary) and camera.tween (camera track, modifier)

| Param | Kind | Default | T |
|-------|------|---------|---|
| `eye`, `at`, `up` | vec3 | absent, meaning the document camera | T |
| `fov_y` | float rad, SOFT 0..64 | absent | T |
| `near_z`, `far_z` | float | absent | T |
| `aspect` | positive float or `"auto"` | absent | |

The soft `fov_y` range is a deliberate difference from `preset_schema.cpp`, whose
`camera.fov_y` row is a hard 0.05..3.05: the RED intro passes raw fov numbers of
22.5 to 25.1 through what becomes a `camera.tween`, and a hard range would either
clamp them or reject the converted attract document. `aspect` is not tweenable at
all, so an `aspect` key is rejected as a non-tweenable value.

`camera.tween` has no params; its tweenable set is the `camera.set` table.

### light.set (light track, primary per `index`)

`index` (int), `direction` (vec3, T), `diffuse` (colour, T), `specular` (colour,
T), `enabled` (bool, default true). Absent values fall back to the document
`lights`.

`enabled` reaches the device: it rides the `SetLights` push (`Preset::Eval::LightPush`)
into `Scene3d::Light`, and `Renderer::ApplyLights` (`src/scene3d/scene3d_render.cpp`)
calls `LightEnable(i, FALSE)` for a disabled light instead of `SetLight` plus
`LightEnable(i, TRUE)`, so a disabled light does not light the scene. The index
still counts: `active_lights_` stays the size of the pushed list, so the next
`SetLights` disables every slot this one used. No converted default disables a
light, so the pushed vocabulary for enabled lights is unchanged and the golden
fixtures still match.

### param.override (scene track, modifier)

`id` (a schema parameter id, required) and `value` (bool, number, enum name or
three numbers; any other JSON shape is a load error). A whole number is read as a
number, not as an int, so `31` and `31.0` mean the same value and both are
written back as `31.0`. Choice `values` use the same reader. The escape hatch for
anything the typed commands do not name. The id grammar is the one used
everywhere in the document:

```
model[<target>].{blend_mode, alpha, anim_speed, position, rotation, scale, spin_per_frame}
sprite[<target>].{x, y, alpha, scale, blend, priority}
camera.{eye, at, up, fov_y, near_z, far_z, aspect}
light[<index>].{direction, diffuse, specular}
shading
sprite_split_priority
```

### render.settings (scene track, primary)

`shading` (enum `texture_only` / `lit_material`) and `sprite_split_priority`
(int 0..64). Both absent by default, meaning "no override".

### rng.seed, rhythm.beat, rhythm.jitter (scene track)

- `rng.seed`: `seed` (int). An EVENT clip: no `end`, duration 0, fires on the frame
  equal to `start`, reseeds the stream and clears the live particle pool. Its own
  primary family, so it may run beside a beat clip.
- `rhythm.beat`: `rate`, `span`, `offset_a`, `offset_b` (ints, absolute document
  frames). The beat index is `(rate * (frame - offset)) / span` with C++ integer
  division truncating toward zero for `frame < offset`, which the evaluator
  reproduces exactly. A primary.
- `rhythm.jitter`: `span` (int), `scale` (float), `models` (string list, empty
  meaning every drawn model), `mode` (`set` or `add`, default `set`). A modifier
  that draws EXACTLY ONE random value per active frame, `j = (draw % span -
  span/2) * scale`. `set` REPLACES the resolved position with `{j, j, 0}`,
  discarding orbit, choice and base position: that is the game's own mechanism
  (the ending's `sub_42F030` sets both models' position rather than offsetting it,
  see `docs/preset_states.md`), so it is the default and the only value the
  converter emits. `add` offsets the resolved position instead and exists for user
  presets that shake a placed model.

### option.select (scene track, event)

`option` and `choice`. Triggers the runtime option transition on the frame equal
to `start`, during playback and export alike. No converted preset has one.

It runs through the SAME code as clicking the choice on the options track
(`Evaluator::SetOption`), so an `option.select` at frame N is exactly a click made
while the playhead sits ON N. `Evaluator::AdvanceFrame` drops the transition
counter by `step` FIRST (that drop belongs to the frame that is ending), then
resolves frame N, sees the event clip in `FrameState::selects` and calls
`SetOption`, which captures the start values, arms the signed kick and loads the
counter with `transition.frames`; frame N is then re-resolved so `when` gates on
the new choice already match on it, and the choice values are blended with the
full counter, so frame N shows the OLD pose and frame N+1 is the first blended
one. That is the order a click produces, and it is the order of the frame walk
below: the counter drop belongs to the previous frame, the select and the blend to
this one. Because the change is skipped when the option already holds that choice,
an event clip fires once and only on its own frame, and because `Seek` replays the
frames from a checkpoint, it fires again every time the playhead crosses it,
including the loop wrap (which is a `Seek(0)`).
`tests/game/preset_option_tests.cpp` asserts the clip and the click produce the
same choices, counter and resolved position on every frame of the blend, and that
seeking backwards past the clip restores the previous choice.

### Options (top level, not clips)

```json
"options": [
  { "id": "mode", "label": "Selected mode", "default_choice": 0,
    "transition": { "frames": 100, "step": 4, "ease": "linear", "spin_kick": 15 },
    "choices": [
      { "label": "BEGINNER", "values": { "model[cube_x].position": [1.0, 0.2, 1.4] } }
    ] }
]
```

`transition.ease` is restricted to `hold`, `linear`, `ease_in`, `ease_out` and
`ease_in_out`: the transition object has no place for `rate_deg` or `cp`, so
`sine_deg` and `bezier` are validation errors. Choice `values` keys use the
parameter id grammar above and nothing else, and choice labels are unique within
an option because gates, `--preset-option` and the states gate address them.

`frames` and `step` are the game's counter, not a duration in frames: the count
starts at `frames` and drops by `step` every frame, so the blend lasts
`ceil(frames / step)` frames and the blend factor is `1 - count / frames`. The
game's own step was a hardcoded 4 against a count of 100, which is why the length
reads in quarter frames (`docs/preset_params.md`, "States are chosen, not
edited"); the document makes the step authorable per option instead of leaving it
a runtime-only parameter, and every converted default carries the game's 4.
`spin_kick` is the game's per-choice-change kick, re-armed to `max(1, spin_kick)`
on every change and negated when the choice index decreases; it decays by the
model's `model.motion.spin_kick_decay`, the only decay (above).

## Tween semantics and the ease formulas

Interpolation is component-wise linear on the raw numbers: Euler radians lerp per
axis with no wrapping and no shortest arc, vec3 per component, scalars directly.
Enum values only `hold`.

For a segment from key A (at `a`) to key B (at `b`) at clip-relative frame `t`,
with `seg_len = b - a`, `t' = (t - a) / seg_len` clamped to 0..1 and
`value = A + (B - A) * k`:

| Ease | k |
|------|---|
| `hold` | 0 (B applies from key B on) |
| `linear` | `t'` |
| `sine_deg` | `sin(min(t - a, seg_len) * rate_deg)`, not a function of `t'` |
| `ease_in` | `t' * t'` |
| `ease_out` | `1 - (1 - t') * (1 - t')` |
| `ease_in_out` | `2 * t' * t'` for `t' < 0.5`, else `1 - 2 * (1 - t') * (1 - t')` |
| `bezier` | CSS `cubic-bezier(x1, y1, x2, y2)` with control points `(0,0), (x1,y1), (x2,y2), (1,1)`: `k = y(s)` where `s` solves `x(s) = t'` by exactly 8 Newton iterations from `s = t'` followed by 8 bisection steps when the Newton result leaves 0..1, all in `float` |

`sine_deg` is the game's own unnormalised sine ramp: the value REACHED at key B and
held afterwards is `A + (B - A) * sin(seg_len * rate_deg)`, which equals B only
when `seg_len * rate_deg` is 90 degrees. RED music select's 35 frame fly-in at 3
degrees per frame therefore rises past its key at frame 30 and settles at
`sin(105 deg) = 0.9659` of the way. Every other ease reaches B exactly.
`hold`, `linear` and `sine_deg` are the three the game itself has; where they come
from and how to re-find them after a build changes is the "Ramps: a phase can move
a value per frame" section of `docs/preset_states.md`, which derives `linear` from
mode select's `A -= max(60-2n, 4) * k` wind-up and `sine_deg` from music select's
`sin(rate * t deg)` fly-in (its per-screen table row names the routine to look at).
`ease_in`, `ease_out`, `ease_in_out` and `bezier` are editor conveniences no
converted default uses; the Tween tab prints the reached fraction beside a
`sine_deg` key so the overshoot is visible rather than surprising.

Before, between and after keys: a value named by key 0 holds key 0's value before
key 0; a value FIRST named at a later key `k` interpolates, on every frame from the
clip start to `k`, from the value already resolved for that target at THAT frame to
key `k`'s value, using key 0's ease; a value present in any key holds the value it
reached at the last key that names it; a key that omits a value another key set
does not touch it.

Key JSON per ease kind:

```json
{ "at": 0, "ease": "linear", "values": { "alpha": 0.0 } }
{ "at": 0, "ease": "sine_deg", "rate_deg": 3.0, "values": { "position": [-0.1, 0.0, -1.0] } }
{ "at": 0, "ease": "bezier", "cp": [0.42, 0.0, 0.58, 1.0], "values": { "scale": [1.0, 1.0, 1.0] } }
{ "at": 35, "values": { "position": [-0.1, 0.0, -0.25] } }
```

`eval_tween.cpp` implements this table, and the curve editor draws its curves by
sampling `SampleKeys` rather than re-deriving the formulas, so the picture and the
render cannot drift apart.

## Editing keys and the add-transition rule

The pure key edits live in `src/editor/tween_edits.h/.cpp` and are what both the
timeline diamonds, the Tween tab and the curve editor call:

| Edit | Rule |
|------|------|
| `AddKeyAt(document, clip, at)` | `at` is clip-relative and must lie in `[0, duration]`; anything else is refused. The new key COPIES the previous key's ease, `rate_deg` and `cp`, and captures, for every value id any key of the clip already names, the value the clip RESOLVES at that frame. Adding a key therefore changes nothing that is drawn: on a linear segment the captured value is on the line and the two halves reproduce it exactly. A key already sitting on `at` is returned unchanged. |
| `MoveKey(document, clip, index, at)` | Clamped to `[previous.at + 1, next.at - 1]`, and to `[0, duration]` at the ends, so keys never reorder and never leave the clip. |
| `DeleteKey`, `SetKeyValue`, `UnsetKeyValue` | A value may only be set for a field that `KeyFieldsFor(type)` marks tweenable, which is what makes "non-tweenable value in a key" impossible to author from the UI. |
| `SetKeyEase` | Sets the ease and carries ONLY the extra field the kind needs: switching to `sine_deg` fills a default `rate_deg` of 3, switching to `bezier` fills `cp` `[0.42, 0, 0.58, 1]`, and switching away drops them. That is what keeps the "rate_deg without sine_deg / cp without bezier" validation errors unreachable from the editor. |
| `SetKeyRate`, `SetKeyBezier` | Refused unless the key's ease is `sine_deg` / `bezier`; `cp[0]` and `cp[2]` are clamped to 0..1 as the CSS cubic-bezier definition requires. |

`ResolvedFieldValue(document, clip, field, frame)` is the capture used by both
`AddKeyAt` and the Tween tab's "+ add" button. It resolves the frame through
`ResolveFrame` and reads the field through the same `ReadTarget` id grammar the
option choice values use (`model[<target>].position`, `camera.eye`, ...), falling
back to the command's own field value when the target grammar does not name it.

`AddTransition(document, a, b, n)` is the NLE-style cross transition of the
timeline's "Add transition to next clip" (and "Add transition between selected"
for two selected draw clips of one target). `a` and `b` are `model.draw` clips on
tracks with the same `target`, `b` starts at or after `a.end`, and `n` defaults to
30 frames:

- Abutting (`a.end == b.start`): the inserted `model.tween` spans
  `[a.end - n, b.start + n)`.
- With a gap: it spans the whole gap plus `n` frames into `b`, `[a.end, b.start + n)`,
  and a COPY of `a`'s draw clip is inserted on `a`'s own track covering
  `[a.end, b.start)`. A tween never makes a model visible (every model starts
  hidden and only `model.draw` shows it), so without that copy the transition
  would play on a model nobody can see. The copy is an ordinary clip the user can
  delete to keep the gap.
- Key 0 (`at 0`) is `a`'s RESOLVED pose at its last frame `a.end - 1`
  (`position`, `rotation`, `scale`, `alpha`, `anim_speed`), key 1 (`at duration`)
  is `b`'s params, and the ease is `linear`, so the mid frame is the midpoint.
- The tween goes on a model track of the same target that sits BELOW `a`'s draw
  track and holds only `model.tween` clips; if there is none, one is created and
  inserted directly below the draw track, because track order is evaluation order
  and a modifier above its draw track would be overwritten by the primary.

## Canonical serialization

`Save` writes exactly one form of a document, and `Load(Save(doc))` is the
identity for every document that validates. (An event clip carrying an `end` is
the one shape that does not survive, because the writer drops that key and
validation already calls it an error.) The rules:

- Keys are written in a fixed order. Document: `schema`, `version`, `id`, `name`,
  `build`, `fps`, `length`, `render`, `camera`, `lights`, `assets`, `options`,
  `rng_seed`, `markers`, `tracks`, `notes`. Track: `id`, `name`, `kind`, `target`,
  `muted`, `solo`, `locked`, `color`, `clips`. Clip: `id`, `type`, `start`, `end`,
  `when`, `label`, `muted`, `params`, `keys`. Key: `at`, `ease`, `rate_deg`, `cp`,
  `values`. Option: `id`, `label`, `default_choice`, `transition`, `choices`.
  Marker: `frame`, `label`. Asset: `kind`, `dir`.
- `render`, `camera` and every `lights` entry are written IN FULL.
- A clip always writes `id`, `type`, `start` and `end` (`null` when open-ended).
  EVENT clips (`rng.seed`, `option.select`) write no `end` at all.
- A track always writes `id`, `name` and `kind`, `name` included when it equals
  the target it would default to, so the file never depends on the fallback.
- Everything else is omitted at its default: `muted` / `solo` / `locked` false,
  `color` and `when` null, `label` and `notes` empty, `keys` empty, `target` on
  track kinds without one, an empty `params` object, and inside `params` every
  value equal to the catalog default.
- `params` are written in catalog order (`FieldsFor`). Key `values` are written in
  the order they were stored, which for a loaded document is file order.
- Unknown keys are preserved and written back after the known keys of the object
  they came from. They are kept at the document, track, clip and `params` level
  (`Document::extra`, `Track::extra`, `Clip::extra`, `Clip::params_extra`), stored
  as the compact dump of the value and re-parsed on save so the indentation is
  regenerated rather than remembered.
- Whitespace is nlohmann's two-space dump, one key per line (so vectors expand to
  one number per line), plus a trailing newline.

Two rules the plan's printed examples imply rather than state, decided here:

- `ease` is written on every key except the last key of a clip, where the default
  `linear` is omitted because the last key's ease is never read. A non-default
  ease IS written on the last key, so that editing it and saving does not lose
  it; that keeps `Load(Save(doc))` total instead of dropping a value the writer
  considers meaningless. Both worked examples in the plan end on a `linear` key
  and so print no `ease` there.
- `params` is omitted entirely when it would be empty, which is why the
  `camera.tween` and `model.tween` clips carry only `keys`.

Number formatting is nlohmann's: a `double` always carries a decimal point, so a
value of 45 in a float field is written `45.0`, while int fields (`fps`, `length`,
`start`, `end`, `priority`, `count`, ...) are written without one.

## Validation

`Validate(document, builtin_ids)` returns a list of `Problem{severity, path,
message}` where `path` is the clip id, track id or option id the problem belongs
to, or the document id for document-wide problems. `builtin_ids` is the list of
built-in ids of the same build; it is a `std::span` argument because the built-in
documents only arrive in M3. Validation never needs a game directory: the
asset-dependent checks of the plan (a missing dir, an unknown model, cell or
animation name, a chrome layer verdict) come with the `AssetIndex` in a later
milestone.

Errors:

- `schema` is not `573renderer/scene-preset`, or `version` is not 1.
- `fps` or `length` is not positive.
- The document `id` equals a built-in id of the same build.
- Duplicate track id, clip id, asset id or option id.
- `markers` not sorted by frame, or two markers on one frame.
- A command on a track kind that does not admit it (`TraitsFor(type).kind`):
  `sprite.*` on sprite, `model.*` on model, `camera.*` on camera, `light.set` on
  light, `emitter` on fx, and `param.override`, `render.settings`, `rhythm.*`,
  `rng.seed` and `option.select` only on scene.
- `start` before frame 0; `end` at or before `start` for a span clip; an `end` on
  an event clip that differs from its `start`.
- A key `at` before the clip start (a negative `at`).
- `rate_deg` on a key that is not `sine_deg`, or `cp` on a key that is not
  `bezier`.
- A key value that is not a parameter of the clip's command, or one that is not
  tweenable.
- A hard range violation on an int, float, vec2 or vec3 parameter. Soft ranges are
  never enforced.
- An enum parameter holding a value with no name (only reachable from code; the
  JSON layer rejects an unknown name at load).
- A required parameter left empty (`asset`, `cell`, `animation`, `param.override`
  `id`, `option.select` `option` and `choice`).
- An `asset` parameter naming an id that is not in the `assets` table.
- Two primaries of one family overlapping: per target across every track for the
  sprite and `model.draw` families, per track (and per `index` for lights) for the
  rest, unless the two clips' gates are mutually exclusive.
- A second track holding primary clips for a target another track already draws.
  Reported once per pair of tracks and target, however many clips each holds, so
  a target drawn twice on two tracks is one message rather than four.
- A `when` naming an unknown option, or an unknown choice of a known option.
- A `transition.ease` outside hold / linear / ease_in / ease_out / ease_in_out, a
  `default_choice` outside the choice list, duplicate choice labels within one
  option, or a choice value key outside the parameter id grammar.
- A `param.override` id outside the parameter id grammar.
- A `sprite.scroll` clip with no sprite clip under it on the same track.

Warnings:

- A `model.tween` or `model.motion` clip with no `model.draw` of the same target
  under any part of it: a tween never makes a model visible, so the clip changes
  nothing. `camera.tween` is exempt, since it modifies the document camera.
- A modifier track for a target that sits ABOVE that target's draw track in
  `tracks` order, and is therefore evaluated before it.
- A `blend` key on a `sprite.animate` clip.
- A key `at` past the clip's duration (`end - start`, or `length - start` for an
  open-ended clip): the clip ends before the key is reached, so the tween is cut
  off. This is a warning and not an error because it is a real authoring state
  and because the converted IIDX RED ending needs it: its 60-frame alpha ramps at
  frame 2858 sit in a phase that is 46 frames long, and the old host simply stops
  applying the ramp when the phase ends (`ApplyRamps` runs for the current phase
  only). Clamping the key would change the interpolation and with it the value on
  every frame of the ramp: `preset_eval_tests.cpp` pins the core alpha of that
  phase at frames 2858, 2859, 2880 and 2903 to `1 - elapsed / 60`, and at 2904 to
  the next phase's own value. The golden fixture cannot cover this one, because
  the old host never pushed a per frame alpha at all.

Blocking versus loadable: only a JSON syntax error, a wrong `schema` string, a
version this build cannot read, a missing required key, a wrong JSON type and an
unknown enum NAME prevent a document from loading. Everything above loads with the
problems listed, so the editor can show them and the evaluator can skip the
offending clip.

`ParseError` carries `line` and `column` for syntax errors (computed from the byte
offset nlohmann reports) and `path` plus a message for everything else, where
`path` is the clip id when the failure was inside a clip.

## Evaluation order and determinism

`Preset::Eval::Evaluator` (`src/preset/eval/preset_evaluator.h`) is the one thing
that turns a document into per-frame work. It is pure with respect to the
document, the option choices and one explicit `EvalState`; nothing else carries
state between frames.

`Load(document, lengths)` binds a document and the asset length table (per
scene3d asset its `max_time`, per package2d animation its frame count) and
resets. `RenderFrame(dt)` returns the frame's ordered `Push` list: the draws for
the current frame, then the state change that prepares the next one, in the order
the section below lists. `Seek(n)` restores the nearest checkpoint and replays
`Advance` up to `n`.

`Seek` clamps to `[0, length - 1]`, the frames that exist: `Seek(-5)` lands on 0
and a seek past the end lands on the last frame. `length` is the document's when it
has one; a document whose `length` is `"auto"` (an absent `Document::length`) has
it DERIVED, by `Evaluator::DerivedLength`, as the largest finite clip `end`, and
when no clip has one, as the longest content tail: per clip, `start` plus the
`sprite.animate` animation's frame count, or plus `max_time / anim_speed` for a
`model.draw`, both read from the asset length table. That is the rule the
converter writes out as a number (`Converter::NaturalLength`), so an auto document
and its converted twin clamp to the same frame. `PresetHost::Seek` clamps the same
way before it posts the command, from the published `Status::length`, and leaves
the upper clamp to the evaluator when the document has no length of its own.

### The order inside one RenderFrame

1. Draw the current frame: `DrawSprites(split, INT_MAX)`, the particles at or
   above the split, `RenderFrame(dt)`, `DrawSprites(INT_MIN, split - 1)`, the
   particles below the split, `AdvanceSprites(dt)`.
2. Advance the model 3D ticks by the `anim_speed` bound for the frame being left
   and the sprite clocks by their `speed`, drop the option transition counter by
   its `step` (that drop belongs to the frame being left), then resolve the next
   frame from the document: tracks in order, muted tracks and clips skipped,
   non-solo tracks skipped while any track is solo, clips whose `when` gate does
   not match the selected choice skipped, the primary of a family first and its
   modifiers after it in clip order. An `option.select` event clip on the resolved
   frame fires here, after the drop, and the frame is resolved again so its gates
   see the new choice.
3. Reset the per model spin accumulator of every model whose winning `model.draw`
   primary now has a different `start`, and arm the kick multiplier of every model
   whose `model.motion` clip starts on this frame to `max(1, spin_kick)`.
4. On a frame where the set of active PRIMARY clips changes (a clip of a primary
   family starts or ends), re-push the whole bound state: style, view, projection,
   lights, then per model alpha, speed, blend, scale and visibility, then the
   sprite placements. This block is resolved WITHOUT the frame's tweens and clip
   keys, because it stands for the material the document's parameters define, not
   the value a tween has reached on this frame.
5. Age the particle pool, push one `SetSpriteScale` per visible sprite in draw
   order, spawn the emitters active on this frame from the beat state of the
   PREVIOUS frame, and draw the single `rhythm.jitter` random value.
6. Motion: apply the choice values (blended from the captured start values while a
   transition is in flight, by the counter step 2 already dropped), push the choice
   camera when the selected choice names `camera.eye`,
   then per model decay the kick multiplier toward 1, integrate the resolved
   `spin_per_frame` times that multiplier into the accumulator, and push the
   transform. Position priority is jitter, then orbit, then the resolved
   (choice-blended) position.
7. Pulse: while a `model.motion` pulse is active and a `rhythm.beat` clip gives a
   positive rate, push every model's scale multiplied by the pulse factor read
   from the beat state of the PREVIOUS frame.
8. Push the material a tween wrote this frame: `anim_speed`, `blend_mode`,
   `alpha`, model `scale`, `fov_y` and the camera vectors, in the order the tracks
   and clips wrote them.
9. Advance the two beat grids for the frame just resolved, then push the values
   that do not depend on the old host's code paths at all: alpha per visible
   model, view, projection, `SetModelTime` per model and `SetSpriteFrame` per
   sprite instance.
10. Apply the `rng.seed` events of this frame (reseed and clear the pool).

### EvalState and checkpoints

`EvalState` (`src/preset/eval/eval_state.h`) is exactly: the frame, the `Ran3`
stream and its seed, the particle pool, the two beat grid indices with their age,
the last jitter draw, the pulse factor, per model the spin accumulator, the
legacy-parity accumulator, the kick multiplier, the 3D tick and the winning draw
and motion clip starts, per sprite instance the clock and its winning clip start,
the option transition counter, the captured transition start values and the
selected choices. Nothing else survives a frame.

A checkpoint is a copy of `EvalState` every 256 frames. `Load` discards every
checkpoint, so replacing a document always re-simulates from frame 0: an edit
anywhere before the playhead changes the RNG stream and the particle pool that a
later checkpoint holds. `preset_eval_tests.cpp` pins both halves: `Seek(700)`
equals 700 `RenderFrame` calls for every member of `EvalState`, and a document
whose emitter moved to frame 10 gives the same state through `Load` plus
`Seek(700)` as a fresh run.

The 3D tick follows the host's own rule (`scene3d_host.cpp`, `RenderFrame`): it
adds `anim_speed` per frame and RESETS TO ZERO on the first frame it exceeds
`max_time`, so it is not a modulo. The IIDX RED attract core reaches exactly
240 ticks at frame 320, is zero at 321, and is `0.75 * (502 - 321)` at frame 502.

### The legacy-parity accumulator

`ModelRuntime` carries a second spin accumulator next to its own. Both integrate
the same per-frame rate; the difference is that the evaluator's accumulator is
zeroed on the first frame of a new draw clip and adds nothing on that frame, while
the legacy one adds a step there as well. It exists so the golden comparison can
be bit exact: float addition is not associative, so adding the missing step to the
finished sum would not reproduce the old host's value for a ramped rate.
`docs/preset_golden.md` describes what the comparison does with it.

## The running host

`PresetHost` (`src/preset/preset_host.h`) is the only thing that turns the
evaluator's `Push` list into engine calls. It is thin on purpose: it owns an
`Evaluator`, the assets a document names, a command queue, and one published
snapshot. It has no countdown, no phases, no `Materialize`.

### Loading

`Load(game_dir, scene, progress)` is the temporary bridge for the compiled
`Preset::Scene` tables: it converts with `Preset::FromScene` and evaluates the
document. `LoadDocument(game_dir, document, progress)` is the real entry point and
is what the registry will call in M3.

Loading runs in two passes, because both the asset length table and the document's
own length come from the assets:

1. Bind the document with an EMPTY `AssetLengths` and resolve frame 0. Frame 0
   does not depend on any length, and the resolved `FrameState` is what the
   `Scene3dHost::Setup` is built from: style, camera, lights, and every model a
   `model.draw` clip names with its frame-0 material.
2. Load the assets, build the `AssetIndex` from what the hosts report, derive
   `AssetLengths` from that index, and bind the document again with it. The
   converter is re-run on the `Scene` path so `length` uses the real numbers.

`progress` is a `ProgressFn(stage, fraction)`. The Screens panel wraps the call in
`BeginLoad` / `UpdateLoadStage` / `EndLoad`, so the loading overlay names the asset
being loaded and climbs a determinate bar; passing no callback is silent, which is
what the CLI does.

### The published snapshot

Everything the GUI reads is a copy taken under one mutex at the end of every
rendered frame:

| Published | Carries |
|-----------|---------|
| `Status` | id, name, frame, length, fps, playing, validation error count, the countdown-shaped `countdown` / `countdown_start` pair (`length - frame` and `length`), the lead model's speed / alpha / blend, beat index and age, pulse factor, last jitter draw, live particle count, the selected choice per option |
| `AssetIndex` | per asset id: kind, dir, loaded, the scene3d model names, the package2d cell names, and the animation names with their frame counts |
| `FrameState` | the frame the evaluator last resolved, which the parameter list reads |

`AssetIndex` (`src/preset/asset_index.h`) is the ONLY source of asset names on the
GUI side. Nothing in the editor asks a host for a name, which is what lets the
editor stay off the render thread. The M2 host test asserts the index is published
after `ReplaceDocument` loads an asset.

### Commands and threading

GUI-thread calls do not touch the evaluator. `Seek`, `SetPaused`, `SetLoop`,
`SetOption` and `ReplaceDocument` push a command onto a mutex-guarded queue that
`RenderFrame` drains at the frame boundary, before it draws. The timeline editor
never calls any of them itself: it posts the matching `PresetCmd` through
`App::State`, and `Backend::ApplyPresetCommand` (`src/backend/preset_command_apply.cpp`)
is the one place that turns a queued payload into these calls on the render thread
(docs/gui.md 3.5, gate `check_host_isolation.py`). `PresetCmd::PreviewLayer` takes the
same route and reaches `PresetHost::RequestPreview`. `Restart()` is `Seek(0)`; there is
no second clock left to desynchronise, because the old "time remain" slider and the
`SetCountdown` entry point behind it are gone with the Parameters pane.

`SetOption` re-pushes the whole rebind list the way `Seek` does, because a choice
change can flip a `when` gate and therefore a model's VISIBILITY, blend, scale and
base speed - and those five are rebind-only pushes (`EmitRebind`), not part of the
per-frame `EmitUnconditional` set. Without the re-push the choice took effect in the
evaluator but the model stayed hidden (or visible) on screen until the playhead
happened to cross a clip boundary, which for an open-ended clip could be never.
`preset_host_tests.cpp` pins it with a `model.draw` gated on the second choice.

`ReplaceDocument(document, progress)` swaps the immutable `shared_ptr<const Document>` and
RE-SIMULATES: it reloads the assets when the asset set or the render size changed,
binds the new document (which discards every checkpoint), re-applies the selected
choices, and replays to the frame the playhead was on. There is no diffing, because
an edit anywhere before the playhead changes the RNG stream and the particle pool
that a later checkpoint holds.

The optional `progress` is the same `ProgressFn` `Load` takes, carried on the
queued command and owned by it (the queue outlives the call, so the callback must
own what it captures), and it is invoked from the render thread by the same
`LoadAssets` the load path uses: one report per 3D scene set and per 2D package,
naming the asset id. A swap that keeps the asset set and the render size reloads
nothing and reports nothing.

`Load` and `Unload` still run synchronously on the calling thread: the preset library
panel that picks a screen is not part of the editor and keeps calling them directly
until M7 replaces it.

### Time ownership

`EvalState` is the only clock. The host calls `Scene3dHost::RenderFrame(0)`, never
calls `Gc2dHost::AdvanceSprites`, and pushes `SetModelTime` per model and
`SetSpriteFrame` per sprite instance on EVERY frame, so a host clock cannot drift
from the evaluator's between seeks.

Live playback accumulates wall time and delivers WHOLE document frames at the
document's `fps`: a 120 fps display renders each document frame twice instead of
running the screen twice as fast. A render frame that advances no document frame
re-applies the last state push list, so the engine state is identical on both. The
frame after `length - 1` depends on the LOOP toggle (`SetLoop`, default on): with the
toggle on it is frame 0 again, as a `Seek(0)` that re-simulates; with it off playback
PAUSES on `length - 1` and stays there, which is the rule the frame axis is defined by
(3.2) and what export captures. The toggle is document-independent editor state, so it
lives on the host and is reported back in `PresetHost::Status::loop`, not in the
document. `preset_host_tests.cpp` pins both directions, and the M2 test renders 120 host
frames for 60 document frames and asserts the pushed host tick equals the `EvalState`
tick on every one of them.

### The engine gaps this closed

| Gap | Before | Now |
|-----|--------|-----|
| 2D canvas | `gc_render.cpp` scaled every node by `width / 640` and `height / 480`, and `gc_host.cpp` scaled a sprite about `(x + 320, y + 240)` | `Gc2dHost::SetCanvas(width, height)` from the document's `render` block. `Gc2d::ScaleFactors` and `Gc2d::PivotFor` (`src/gc2d/gc_sprite.h`) are the single definition, shared by the renderer and by the tests. All 18 converted defaults keep 640x480 |
| One 2D package | one package plus one particle package | packages are keyed by ASSET ID (`Gc2dHost::LoadAsset`), sprite placements and particle draws name their asset, and `DrawSprites` draws each consecutive same-asset run with that package's own renderer, so draw order is preserved across packages |
| Sprite identity | placements carried their clock over by ANIMATION name, so two instances of one animation fought over it | a placement carries `target`, the document's unique instance name, and the clock carries over by that |
| Animation alpha | `sprite.alpha` reached static cells only, so a `sprite.animate` alpha did nothing | `Gc2d::AppendNodes` fades every node it appended, cell or animation. Blend stays per node from the package |
| One model dir | only `models.front().scene_dir` loaded | `Scene3dHost::LoadUnion(dirs, setup)` merges the union of the dirs a document's assets name. `Scene3d::Merge` (`src/scene3d/scene3d_merge.h`) offsets the second scene's tile indices into the merged tile list, unions the bounds, keeps the larger `max_time` and the first authored camera |
| One 3D clock | `Scene3dHost::SetTime` wrote one time into every model | `Scene3dHost::SetModelTime(name, ticks)` per model |
| Conditional pushes | position and rotation only for models that passed `Moves()`; alpha, view and projection only on a rebind | every model's transform, alpha, speed, blend, scale and visibility, and the camera view and projection, are pushed every frame. This is tolerated difference (1) of `docs/preset_golden.md`, and it is what finally makes alpha and camera ramps reach the GPU |

### Parameters: the bridge is gone

The Parameters pane was the temporary surface that edited a preset before the
timeline editor could. M5 deleted it (`gui_preset_workspace.cpp`) together with the
per-parameter entry points it was the only caller of: `PresetHost::ListParams`,
`SetParam`, `ResetParam`, `ResetGroup`, `ResetAllParams`, `ChangedParamCount`,
`ListStates`, `SetCountdown` and `preset_host_params.*`. `--preset-tweaks` was
already retired in `src/cli/tool_command.cpp`, so no tweak file is loaded either.

What survives is the `param.override` COMMAND, an ordinary document command with its
own `FieldDesc` row (`preset_fields.cpp`) and clip summary, edited in the timeline
editor like any other. Every other value is now edited through the clip properties
modal, the Inspector "Clip" tab or the Document properties modal (docs/gui.md 3.5).

## Built-in documents and the registry

The 18 shipped screens ARE documents. They are C++ functions that build a
`Document` out of owning structs, not a JSON blob compiled into the binary and
parsed at startup, so a mistake in one of them is a compile error and the editor
of a later milestone can duplicate one without a parse step.

| What | File | Entry points |
|------|------|--------------|
| The built-in list | `src/preset/defaults/defaults.h/.cpp` | `BuiltIns()` |
| Shared builders every defaults file uses | `src/preset/defaults/defaults_build.h/.cpp` | `Widen`, `DefaultLens`, `WideLens`, `WideLensAt`, `StandardLights`, `Scene3dAsset`, `Package2dAsset`, `SpriteTrack`, `ModelTrack`, `CameraTrack`, `FxTrack`, `SceneTrack`, `AppendPart` |
| IIDX 10, all 9 screens | `src/preset/defaults/iidx10_defaults.cpp` | `Iidx10Defaults` |
| IIDX RED attract, card in, login, new player | `src/preset/defaults/iidx11_defaults.cpp` | `Iidx11Defaults` |
| IIDX RED dan, expert, mode and music select | `src/preset/defaults/iidx11_select_defaults.cpp` | `Iidx11SelectDefaults` |
| The ending, markers 1 to 9 plus the document header | `src/preset/defaults/iidx11_ending_a_defaults.cpp` | `Iidx11Ending`, file-local `Iidx11EndingPartA` |
| The ending, markers 10 to 18 | `src/preset/defaults/iidx11_ending_b_defaults.cpp` | `Iidx11EndingPartB` |
| The registry over built-ins plus user files | `src/preset/doc/preset_registry.h/.cpp` | `Registry::Load`, `ForBuild`, `Find`, `Problems`, `UserRoot`, `LoadFile`, `Entry`, `ScanStatus` |
| The headless document tools behind the CLI | `src/preset/preset_tools.h/.cpp` | `DumpDefaults`, `ExportJson`, `Validate` |

### How the defaults were produced

They were generated once, mechanically, from the converter, then tidied; they are
not hand-transcribed a second time from the game. The run was: dump
`FromScene(scene, asset_lengths)` for all 18 registered scenes as canonical JSON,
emit C++ from that JSON with a throwaway script (one function per document,
designated initializers in declaration order, the shared builders above for the
shapes that repeat), and run clang-format over the result. The generator was
deleted after the run, because the guarantee does not come from it: the test
`every old scene has a built-in document equal to the converter output`
(`tests/game/preset_defaults_tests.cpp`) compares each built-in with
`FromScene(scene, lengths)` field by field and byte for byte through `Save`, so
the defaults cannot drift from the tables while both exist, and the golden test
keeps holding the behaviour afterwards.

Two conventions in the generated files are worth knowing before editing one:

- `Widen(0.025F)` (and its two- and three-argument forms) is a float value the
  converter widened to `double`. The game's tables are `float`; writing the widened
  literal out in full (`0.02500000037252903`) is what byte equality needs and what
  a reader cannot check, so the exact float is written instead and widened in one
  place. A plain literal is used wherever it is short and exact (`0.5`, `100.0`),
  and where the value is not a widened float at all (the countdown tweens, whose
  ends are computed in double).
- `DefaultLens()` is the lens 9 of the 18 screens share (eye `(0,0,-1)`, near
  `0.1`, far 500, `aspect: "auto"`); `WideLens(eye)` is the IIDX RED lens (near 0,
  far 1000, aspect `1.7708334`), and `WideLensAt(eye, at)` is the one RED screen
  that also aims the camera.

The ending is split across two files because one file would pass the 1000-line
limit. The split is by marker range: part A carries markers 1 to 9 and every clip
that starts before frame 2486, part B the rest. `AppendPart` merges them by track
id, so a track that both halves touch keeps one entry with its clips in frame
order, and part A declares the tracks that only part B fills so that track order
matches the converter exactly. The M3 test asserts the merged document has its 18
markers in ascending order with unique track and clip ids.

### User documents

A user document lives in `presets/<build>/<id>.json` NEXT to `573Renderer.exe`
(`UserRoot()` is the exe directory plus `presets`, the same portable-app rule as
`settings.ini`). The registry scans `presets/*/*.json`, so the build directory is
where a document is FOUND, while the document's own `build` field is what it is
LISTED under; a file whose directory and `build` disagree is listed under its
`build` with a warning, never silently moved. Import (a later milestone) reads a
file from anywhere and copies it there.

`Registry::Load` reports progress through a `ScanProgressFn` taking a
`ScanStatus` (`done`, `total`, `current` path), called once per file before it is
read, so a caller can print or publish "scanning user presets 2/5: <file>" while a
directory is walked; nothing about the scan is silent or unbounded.

Rules the registry applies:

- A user id equal to a BUILT-IN id of the same build is a validation error
  (`Validate`'s `builtin_ids` argument). The file is not dropped: it is listed by
  `Problems()` with its path and its problems, so the library can offer to rename
  it, and the built-in keeps the id. Exactly one entry with that id is resolvable.
  The rule belongs to the REGISTRY, not to a document on its own: `--preset-validate`
  and `--preset-json` pass no `builtin_ids`, so exporting a built-in, editing the
  file and running it by path keeps working without a rename.
- A user id equal to one an EARLIER user file of the same build already claimed is
  the same kind of error, with the winning file's path in the message. Order is the
  scan order, so the first file to claim an id keeps it. Without this the id would
  resolve to two documents: `ForBuild` would list both and `--preset-test <id>`
  would pick whichever came last, silently.
- A file that does not parse is listed by `Problems()` with the line and column,
  and contributes nothing to `ForBuild`.
- A user document with other validation errors IS resolvable (the evaluator skips
  the offending clips, see Validation above) and is listed by `Problems()` too.
- `Find(build, id)` and `ForBuild(build)` never mix builds: a document for another
  build is invisible to this build's list.

## Fixtures and tests

| Fixture | What it is |
|---------|------------|
| `tests/game/fixtures/iidx11-attract.json` | the complete IIDX RED attract document: 9 tracks, 5 markers, 3 assets, the intro fov `camera.tween`, the 4:3 `camera.set`, the warp emitter |
| `tests/game/fixtures/iidx10-card-in.json` | the small IIDX 10 card-in document: one held 2D animation, one additive model, `aspect` auto |
| `tests/game/fixtures/unknown-keys.json` | one unknown key at the document, track, clip and `params` level |

Both real fixtures are the documents printed in sections 3.7 and 3.8 of
`docs/scene_preset_editor_plan.html`, canonicalized once: the printed form packs
several keys per line, and it writes `"blend_mode": "opaque"` on the two r_side
clips where `opaque` is the catalog default and the omit-at-default rule drops it.
No value differs from the plan.

`preset_json_tests.cpp` parses both documents into typed structs, asserts
`Save(Load(x)) == x` byte for byte for all three fixtures, asserts
`Load(Save(doc)) == doc` on the document model (including a non-default ease on a
clip's last key and a whole number written for a `param.override` value), covers a
track whose `name` is absent in both the target and the kind fallback, and pins
the four load failures (unknown enum name with the clip id in the error, newer
version, foreign schema, syntax error with a line and column).
`preset_validate_tests.cpp` asserts both shipped fixtures validate with zero
problems and covers each rule the milestone lists: overlapping primaries on one
target and across two tracks, the one message a two-by-two double-drawn target
produces, the mutually exclusive gate exception and the `not`
gate that is NOT exclusive of an overlapping choice set, an accepted modifier over
its primary, a key past the clip duration, a tween with nothing under it (warning,
not error), unknown option and unknown choice gates, a `param.override` id and a
choice value key outside the grammar, a user id equal to a built-in id, a command
on the wrong track kind, an event clip carrying an end, a `render.settings` clip
that leaves its optional enum absent, hard versus soft ranges, and duplicate clip
ids with unsorted markers.

## Layer verdicts and the layer preview at runtime

`docs/preset_layers.md` stays the single source of truth for what a 2D layer is. The running
renderer has no path to a repository markdown file, so a CMake custom command runs
`tools/ci/gen_layer_verdicts.py docs/preset_layers.md` at build time and writes
`preset_layer_verdicts.cpp` into the build tree (never edited, never committed). It defines
`Preset::VerdictFor(package_dir, layer)` and `Preset::VerdictName`, declared in the tracked
header `src/preset/preset_layer_verdicts.h`. The generator reads exactly the rows the CI gate
reads: package, layer, kind, verdict. Adding a row to the markdown is all it takes for the
editor to show the verdict beside a part.

The verdict is a DISPLAY, not a rule: the hidden-parts checklist in the clip properties modal
prints background / chrome / unclassified next to each child part so the user sees the
classification while choosing. `Validate` does not read it, and nothing in the editor refuses a
layer; the built-in documents stay gated by `tools/ci/check_preset_layers.py`.

There is ONE vocabulary for a part name, and four places have to agree on it: the `hidden_parts`
list a document stores, the `skip_parts` lookup the renderer performs, the `parts` list the
`AssetIndex` publishes for the checklist, and the layer column of `docs/preset_layers.md`. All
four use the BARE name (`OP_BG_U`, `TITLE_TAIKI`), which is exactly how `Gc2d::AppendNodes`
addresses a part: it looks the string up in `SysIdx::Package::animation_names` and in
`cell_names`, so a cell and a child animation of the same name are both hidden by one entry.
`Gc2d::PartNames` therefore lives in `gc_sprite.cpp` beside that lookup and returns the same
strings, and `tests/formats/gc_sprite_tests.cpp` pins the pair: a name PartNames lists must be a
name `skip_parts` hides. A decorated form (`"cell OP_BG_U"`) is silently broken three ways at
once - the checkbox never ticks for a part the document already hides, ticking it writes a string
the renderer ignores, and the verdict lookup never matches a markdown row.

`PresetCmd::PreviewLayer{asset, animation, hidden_parts, samples}` is the render-thread half of
the preview strip. The GUI cannot render a 2D layer itself: the GUI window and the renderer
window own two separate D3D9 devices. So the modal posts the command once per
(asset, animation, hidden_parts) key, `Backend::ApplyPresetCommand` forwards it to
`PresetHost::RequestPreview`, and `Preset::Preview` renders ONE sample per frame from
`Scene3dBackend::AdvanceFrame` - which the render loop calls BEFORE `BeginScene`, the only
place a render target can be swapped and read back safely. Each sample places the animation
alone on `Gc2dHost`, draws it into a private render target, reads it back and downsamples it to
a 160 px thumbnail; the live sprite placements are re-pushed straight afterwards because the
evaluator pushes the whole placement list every frame anyway. The samples are published as BGRA
buffers in `App::State::GetPresetPreview()` with a version counter, and the strip shows a
determinate "sample k / N" cell for every sample that has not arrived. Sample frames are spread
across the animation length the `AssetIndex` reports.

## Choices made where the plan is silent

- Namespace `Preset::Doc`, as above.
- `Load` and `Save` rather than `Parse` and `Serialize`, matching the rest of the
  tree's naming.
- `Problem` carries `severity`, `path` and `message`. The plan's sketch also had a
  `field`; nothing in this milestone reads one, so it is not there yet.
- Unknown keys are stored as `{key, compact json text}` pairs rather than a live
  `nlohmann::ordered_json` member, so `preset_document.h` stays free of the JSON
  library and every consumer of the document model does too.
- The catalog default of a parameter is the member initializer of its command
  struct; `DefaultCommand(type)` is the one place the writer and the forms read it
  from, so there is no second table of defaults to drift.
- Nested parameter objects (`orbit`, `pulse`, `scatter`) are one `FieldDesc` each,
  with their sub-fields documented here rather than described by their own
  descriptor tables. Their ranges are therefore not machine-checked in M1.
- An unknown key inside `params` is preserved, not reported: 3.5 of the plan asks
  for unknown keys at any level to survive, and 5.5 lists no error for them.
- The `length` value `"auto"` is represented as an absent `Document::length`.
