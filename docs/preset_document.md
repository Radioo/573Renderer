# Scene preset documents

A scene preset document is the owning, JSON round-trippable timeline that replaces
the `constexpr Preset::Scene` tables: tracks of clips on a frame axis, plus the
document-wide render, camera, light, asset, option and marker blocks. This file is
the single source of truth for the schema, the command catalog, the canonical
serialization rules and the validation rules.

Status: milestone M1 of `docs/scene_preset_editor_plan.html` is in the tree. That
milestone is the document model, the JSON layer and validation only. Nothing loads
or renders a document yet: `preset_host.cpp`, `preset_effective.*`, `scene_preset.h`
and the `scene_presets_*.cpp` tables are untouched and still drive the app. The
evaluator (M2), the built-in documents and the registry (M3), and the timeline
editor (M4 onward) are not here yet, so no code outside `tests/game` consumes these
modules.

## Where the code is

| What | File | Entry points |
|------|------|--------------|
| Enums and their JSON name tables | `src/preset/doc/preset_enum_names.h/.cpp` | `kCommandTypeNames`, `kEaseNames`, ..., `IndexForName`, `NameForIndex` |
| One struct per command, the `Command` variant, the command traits | `src/preset/doc/preset_commands.h` | `Command`, `ParamValue`, `TypeOf`, `TraitsFor`, `IsEvent`, `kCommandTraits` |
| Document, Track, Clip, Key, Gate, OptionSpec, Marker, Asset, render/camera/light blocks | `src/preset/doc/preset_document.h` | `Document`, `kSchemaId`, `kSchemaVersion`, `HasTarget` |
| One `FieldDesc` per command parameter | `src/preset/doc/preset_fields.h/.cpp` | `FieldsFor`, `KeyFieldsFor`, `FindField`, `DefaultCommand` |
| JSON load and save | `src/preset/doc/preset_json.h/.cpp` | `Load`, `Save`, `ParseError`, `Loaded` |
| Validation | `src/preset/doc/preset_validate.h/.cpp` | `Validate`, `Problem`, `Severity` |
| Tests | `tests/game/preset_json_tests.cpp`, `tests/game/preset_validate_tests.cpp` | fixtures in `tests/game/fixtures/` |

Everything lives in `namespace Preset::Doc`. The nested namespace is deliberate:
the old table structs (`Preset::ParamOverride`, `Preset::ModelMotion`,
`Preset::Camera`, `Preset::Option`, ...) keep their names in `namespace Preset`
until the last milestone deletes them, and the converter of M2 has to include both
headers at once.

JSON is `nlohmann-json` (vcpkg port `nlohmann-json`, header only), used through
`nlohmann::ordered_json` so key order is what the writer wrote. It is linked into
`game_tests` today; the app targets pick it up when M2 gives them a consumer.

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
  render.height/2)`. Making that true needs the `Gc2dHost::SetCanvas` change
  listed as an M2 engine gap.
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

The formulas themselves arrive with `eval_tween.cpp` in M2; this table is what that
code has to implement.

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
- A key `at` outside `[0, duration]`, where duration is `end - start`, or
  `length - start` for an open-ended clip (not checked when `length` is `"auto"`).
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

Blocking versus loadable: only a JSON syntax error, a wrong `schema` string, a
version this build cannot read, a missing required key, a wrong JSON type and an
unknown enum NAME prevent a document from loading. Everything above loads with the
problems listed, so the editor can show them and the evaluator can skip the
offending clip.

`ParseError` carries `line` and `column` for syntax errors (computed from the byte
offset nlohmann reports) and `path` plus a message for everything else, where
`path` is the clip id when the failure was inside a clip.

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
