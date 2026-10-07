# An optional project file keeps authored source

The IFS editor keeps an optional project file tied to one IFS, holding the live source of what was authored in the editor: keyframes and easing, script source text and source images. Exporting writes that authored content into the IFS as baked data, and the IFS stays a complete document that opens and saves without its project. This reverses ADR-0001 because the IIDX 33 game data supplied the cases it asked for: all 29110 of its animations store motion as one placement per frame baked out of After Effects, so an IFS-only editor turns every committed ease into per-frame numbers that can never be adjusted as an ease again, and scripts written in the editor would lose their source. Shipped game files have no project and are never turned back into keyframes automatically, so ADR-0002's round trip is unchanged; the project adds two obligations instead, a deterministic export where the same project always produces the same IFS, and detection of an IFS changed outside the editor since its last export.

## Consequences

- A script written in a project keeps its source text as the truth; scripts in baked data stay instruction lists.
- A document's target build is stored in its project when it has one, and in the user's settings otherwise.
- Atlas layout for images the project owns is computed on export; atlases of baked data keep their layout.
- Motion authored in a project keeps live keyframes; baked motion is edited as sampled per-frame values.
- A project is a folder: a text manifest, copies of its source images and script sources, and the path of its IFS.
- The unit of authored content is one depth over a frame range. The baked output of authored content is read-only; turning it into ordinary baked data is an explicit detach, and turning baked data into authored content is an explicit own that keys every frame.
- When entries of the IFS differ from the hashes recorded at the last export, the user chooses per entry between keeping the IFS version, which detaches the authored content in it, and exporting again.
