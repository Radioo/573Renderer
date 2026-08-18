# Scene preset parameters (replaced)

This document described the parameter schema of the compiled `Preset::Scene` tables:
declarations and instances, the sparse tweak set, the "Screen parameters" pane, the
tweak file and the Screens panel. Every one of those surfaces is gone.

A preset is a `Preset::Doc::Document`. Read instead:

- **docs/preset_document.md** - the schema, the command catalog with every parameter,
  its kind, default, unit and whether it is tweenable, the canonical serialization, the
  validation rules, the evaluation order, the registry and the library's save, id and
  import rules.
- **docs/gui.md 3.5** - the timeline editor: the clip properties modal and the
  Inspector "Clip" tab build their forms from `FieldsFor(command type)`, the Document
  properties modal edits every document-level key, and the "Frame" tab shows what the
  evaluator resolved this frame and which clip won each value.
- **docs/gui.md 3.6** - the preset library: New, Duplicate, Import, Export, Save,
  Revert, Reset and the validation list.
- **docs/preset_states.md** - what each screen's states are and where in the game they
  come from.

Two rules from the old schema survive and live on in the new one, so they are recorded
here rather than lost:

- **The id grammar survives for addressing, not for editing.** `model[<target>].alpha`,
  `sprite[<target>].scroll_x`, `camera.fov_y`, `light[<i>].diffuse`,
  `sprite_split_priority` and `shading` are still the ids a `param.override` clip and an
  option choice value name (docs/preset_document.md, "param.override"). The old
  occurrence suffix (`sprite[BG_SKY#2]`) is gone: a track's `target` is unique in the
  document, so the second instance of an animation is an ordinary target name
  (`BG_SKY_2`).
- **A range never quantizes a value.** `Range::step` is a drag-speed and slider hint,
  never applied to the stored number, because snapping would make the game's own value
  unrepresentable: IIDX's `camera.fov_y` of 1.0471976 does not sit on a 0.001 grid, the
  IIDX 10 game over screen authors a negative per-frame speed, and every IIDX RED screen
  authors a near plane of 0. Validation therefore checks hard ranges (enums, counts,
  booleans) and leaves soft float ranges alone.
