# IFS editor

Status: ready-for-agent

A separate Qt 6 application in this repo that opens, edits and saves IFS files, with an optional project file for authored content and a viewport drawn by the target build's afp-core in a preview host process. Vocabulary is in `CONTEXT.md`; the decisions are ADRs 0002 to 0006.

## Milestones

Each milestone is blocked by the one before it.

1. Container and textures: IFS read and write, the binary XML manifest, AVS-LZ77, texture images, and the round trip gate. No UI.
2. Lossless AFP animation reader and writer for the tags the target build's data uses, through the round trip gate.
3. GE2D shape reader and writer, through the round trip gate.
4. Preview host: the afp host code carved out of `r573_app` into a library, a host executable, IPC, ramfs reload.
5. Editor application shell (Qt 6 Widgets, docking) with the chosen layout: viewport centre, package tree left, inspector right, depth-row timeline below.
6. Placement editing, then library calls and labels, structure edits, the 3D camera.

Reverse engineering of unknown placement flags and blend values runs alongside every milestone.

## Milestone 1: container and textures

### Behaviour

- Reading an IFS yields its header fields, its manifest tree and every entry's bytes, without the game DLLs.
- Writing produces an IFS avs2-core mounts, from that same model.
- A round trip encodes every entry again through the writers, never copying original bytes through (ADR 0002). The gate is that every entry's decoded bytes and the container fields compare equal; whole-file byte identity is a tracked metric that does not block.
- Binary XML trees read and write losslessly, keeping string bytes exactly as stored even when they are not valid in their declared encoding.
- AVS-LZ77 compresses and decompresses; the compressor aims to match avs2-core's output byte for byte.
- Texture images in `argb8888rev` decode to and encode from 8-bit BGRA; `avslz` image blobs keep their header framing, and raw blobs stay raw.

### Seams and tests

- Every codec is a pure function over byte spans in the `r573_formats` module (stdlib, tl-expected, and hash-library for MD5), tested with synthetic fixtures under the `ci` label.
- A local test under the `local` label runs the round trip over every IFS below `R573_IIDX_DIR/data`, reporting the entry-level pass count and the whole-file identity count.
- A local test under the `local_dll` label compares the binary XML and LZ77 writers against avs2-core's own writers on the same inputs.

### Out of scope

- Restoring hashed entry names to logical names, MD5 name hashing for new entries, atlas packing, PNG import: those arrive with the first feature that adds new entries.
- Any UI.

## Milestone 2: AFP animations

### Behaviour

- A stored animation and its byte order script restore to native order the way afp-core's op 8 does, and a native animation stores back with a generated script.
- Restored animations read into a model with no offsets: header, exports, imports, the string table in file order, containers, and typed tags for the seven the target build uses. Anything else stays as bytes and cannot be stored, since nothing says which bytes to swap.
- Writing rebuilds every offset and records every field's byte width; the script comes from those widths with the converter's greedy rule.
- Stored details the restored data cannot show (scrambled strings, the header background colour's byte order) live in the model as an explicit stored form.
- The round trip gate re-encodes every animation and its script, fails on any model difference, and reports byte identity of both.

### Seams and tests

- `AfpByteOrder` and `AfpAnimation` in `r573_formats`, tested with synthetic fixtures under `ci`.
- The `local` round trip gate covers every `afp/<name>` with an `afp/bsi/<name>`.

## Milestone 3: GE2D shapes

### Behaviour

- A shape reads into a model with no offsets, in the byte order the package `magic` file selects, and writes back with every count, offset and size recomputed in the shipped table order.
- Fields with no known meaning keep their values; floats keep their raw bits.
- The round trip gate re-encodes every shape and fails on any model difference.

### Seams and tests

- `Ge2dShape` in `r573_formats`, tested with synthetic fixtures under `ci`.
- The `local` round trip gate covers every `geo/` entry.

## Milestone 4: preview host

### Behaviour

- IFS bytes held in memory mount through avs2-core's `ramfs` and `imagefs`, so a package loads without touching the disk.
- A loaded package reloads under the same name: layers and streams are destroyed first, then the package, then the new bytes load and the animation seeks back to its frame. afp state (frame count, labels, playhead) proves the new content is live, never pixels.
- The afp boot, mount, package, animation and D3D9 draw code the renderer already has moves into an `r573_afp_host` library that takes its session and GPU context explicitly and has no GUI, `App` or window dependency. The renderer links it and renders exactly as before.
- The library renders one frame into an offscreen D3D9Ex texture whose shared handle another process can open.
- A preview host executable boots a target build from its install directory and serves the editor over IPC: load or reload a package from bytes, pick an animation, seek to a frame, set the viewport size, and return the shared texture.

### Seams and tests

- Mount and reload are proved by `local_dll` tests against the target build.
- The carve-out is proved by the existing pixel golden and byte-compare gates staying green.
- The host protocol is tested with the host process launched by a `local_dll` test.

- The host protocol is FlatBuffers over a named pipe (ADR 0007).

## Milestone 5: editor shell

### Behaviour

- A Qt 6 Widgets application, separate from the renderer, built by its own CMake project against a dynamic vcpkg triplet (ADR 0003), linking the shared format and protocol code again.
- It starts the preview host, boots a target build in it, and shuts it down when the editor closes; a host that dies is reported with the request that killed it, and the editor keeps the document.
- Layout is the chosen one: viewport in the centre, package tree on the left, inspector on the right, timeline across the bottom, in dockable panels whose arrangement is remembered between runs.
- Opening an IFS shows its entries as a tree, with an inspector for the selected entry, and picking an animation shows it in the viewport at the frame the timeline is on.
- The viewport draws the shared texture the host renders into, and follows the panel size.

### Seams and tests

- The host client (launch, requests, shutdown) is Qt-free in `r573_preview_protocol`, tested under `local_dll`.
- Document and panel logic stays out of the Qt widgets so it can be tested without a window; widget code is thin.

## Milestone 6: editing baked data

Everything here edits baked data in place. Authored content (ADR 0006's
project file, keyframes, own and detach) is milestone 7 and nothing in this
milestone may assume it exists.

### Behaviour

- A change to the open document is applied to the model, encoded through the
  same writers the round trip gate uses, reloaded in the preview host and
  seeked back to the frame the timeline is on, which is the loop ADR 0004
  asks for. Nothing is written to disk until the user saves.
- Saving writes the IFS with `Ifs::Write`. Saving over the opened file and
  saving to a new path are both possible, and an unsaved document is visible
  in the window title and refuses to be closed silently.
- Every change is one undoable edit with a name the user recognises, and undo
  and redo restore the document and the viewport.
- Selecting a depth at a frame selects its placement, and the inspector edits
  the fields the target build supports: translation, scale, rotate and skew,
  the 3D matrix, colour multiply and add, blend, HSV, the rotation origin and
  the instance name. A field the placement does not carry can be added and one
  it carries can be dropped, because presence is what the format stores.
- A placement that came from an unknown tag or carries unknown data is shown
  and never edited.
- Labels can be added, renamed, moved and removed on the timeline ruler.
- A library call is edited as one item: the aeplib function and its constant
  arguments, never as bytecode text.
- Structure edits: add and remove a depth over a frame range, add and remove
  frames of a clip, and add, replace and remove whole entries of the IFS.
- The 3D camera of a clip is edited as position and focal length, with the
  same reload loop.

### Seams and tests

- Every edit is a function over the document model in `r573_document`, with no
  Qt and no host in it, tested under `ci`. The window applies an edit, asks
  for the bytes and hands them to the host.
- The undo stack holds documents, not widget state, so an undone edit is
  proved by comparing models rather than by what a panel shows.
- A `local_dll` test drives the whole loop on a shipped package: change a
  placement, encode, reload, seek, render, and check afp-core reports the
  frame count and labels the edited model has.

## Milestone 7: authored content

Everything here is ADR 0006's project: live source that the editor owns,
exported into the IFS as baked data. An IFS still opens, edits and saves with
no project at all, and nothing in milestone 6 may start depending on one.

### Behaviour

- A project is a folder beside its IFS holding a text manifest, copies of the
  source images it owns and the source text of the scripts it owns. Creating
  one for an open IFS, opening one, and opening the IFS it names are all
  possible, and closing one leaves the IFS exactly as it was.
- The target build of a document with a project comes from the project; a
  document without one falls back to the user's settings.
- Owning a depth over a frame range turns its baked placements into authored
  content with a keyframe on every frame, so nothing about the render changes.
  Detaching does the reverse and drops the source.
- A property of an authored depth is a list of keyframes with an ease between
  them, edited on the timeline, and export samples it to one placement per
  frame.
- A script an authored depth owns keeps its source text as the truth, and
  export compiles it; a script in baked data is still an instruction list.
- Export is deterministic: the same project always produces the same IFS
  bytes, and images the project owns get an atlas layout computed at export
  while atlases of baked data keep the layout they came with.
- Export records the digest of every entry it wrote. When an entry of the IFS
  no longer matches, the user chooses per entry between keeping the IFS
  version, which detaches the authored content in it, and exporting again.

### Seams and tests

- The project model, the manifest, keyframe sampling and export are all in
  `r573_document` with no Qt and no host, tested under `ci`.
- Export determinism is a test, not a claim: exporting the same project twice
  produces the same bytes, and the round trip gate still passes on an
  untouched install.
- Owning a depth and exporting it again with no edit produces the placements
  that were there before, which is what proves own is lossless.

## Milestone 8: authoring an animation

Milestone 7 made authored content exist and export correctly. Nothing in it
lets a user change a keyframe, so an owned depth can only reproduce what it
captured. This milestone is the editing itself.

### Behaviour

- The timeline shows an owned depth's animated properties as rows of keyframes,
  and a keyframe can be selected, added, removed, retimed and given an ease.
- The inspector edits the selected keyframe's value, so an owned depth stops
  being read-only.
- Every such edit is one undoable step that runs the reload loop and writes the
  project manifest, the same as every other edit.

### Seams and tests

- The edits are functions over `AuthoredDepth` in `r573_document`, with no Qt
  and no host, tested under `ci`.
- Adding a keyframe on a covered frame is proved not to change what any frame
  samples, which is what makes it safe to refine a dense track into a sparse one.

## Tickets

- `issues/01-avs-lz77-module.md`
- `issues/02-binary-xml.md`
- `issues/03-ifs-container.md`
- `issues/04-texture-images.md`
- `issues/05-round-trip-gate.md`
- `issues/06-avs2-writer-comparison.md`
- `issues/07-afp-byte-order.md` (milestone 2)
- `issues/08-afp-animation-model.md` (milestone 2)
- `issues/09-afp-round-trip-gate.md` (milestone 2)
- `issues/10-ge2d-shapes.md` (milestone 3)
- `issues/11-ge2d-round-trip-gate.md` (milestone 3)
- `issues/12-ramfs-package-mount.md` (milestone 4)
- `issues/13-package-reload.md` (milestone 4)
- `issues/14-afp-host-library.md` (milestone 4)
- `issues/15-shared-texture-target.md` (milestone 4)
- `issues/16-preview-host-protocol.md` (milestone 4)
- `issues/17-preview-host-executable.md` (milestone 4)
- `issues/18-preview-client.md` (milestone 5)
- `issues/19-editor-shell.md` (milestone 5)
- `issues/20-package-tree.md` (milestone 5)
- `issues/21-viewport.md` (milestone 5)
- `issues/22-timeline-depth-rows.md` (milestone 5)
- `issues/23-edit-loop-and-save.md` (milestone 6)
- `issues/24-placement-editing.md` (milestone 6)
- `issues/25-undo-and-redo.md` (milestone 6)
- `issues/26-labels-and-library-calls.md` (milestone 6)
- `issues/27-structure-edits.md` (milestone 6)
- `issues/28-camera.md` (milestone 6)
- `issues/29-project-file.md` (milestone 7)
- `issues/30-own-and-detach.md` (milestone 7)
- `issues/31-keyframes-and-easing.md` (milestone 7)
- `issues/32-export.md` (milestone 7)
- `issues/33-source-images-and-atlas-layout.md` (milestone 7)
- `issues/34-script-source.md` (milestone 7)
- `issues/35-export-drift.md` (milestone 7)
- `issues/36-keyframe-editing.md` (milestone 8)
- `issues/37-authored-value-editing.md` (milestone 8)
- `issues/38-inspector-rows-in-the-document.md` (milestone 8)
- `issues/39-playback.md` (milestone 8)
- `issues/40-sprite-clips.md` (milestone 8)
- `issues/41-sprite-structure-labels-camera.md` (milestone 8)
- `issues/42-authored-sprite-content.md` (milestone 8)
- `issues/43-isolated-sprite-preview.md` (milestone 8)
- `issues/44-control-bits.md` (milestone 8)
- `issues/45-group-resets.md` (milestone 8)
- `issues/46-undo-authored-content.md` (milestone 8)
- `issues/47-choose-what-to-place.md` (milestone 8)
- `issues/48-timeline-zoom.md` (milestone 8)
- `issues/49-start-animating-a-property.md` (milestone 8)
- `issues/50-place-a-texture.md` (milestone 8)
- `issues/51-select-and-move-on-stage.md` (milestone 8)
- `issues/52-scale-and-turn-on-stage.md` (milestone 8)
- `issues/53-keyframe-selection.md` (milestone 8)
- `issues/54-ease-curve-editor.md` (milestone 8)
- `issues/55-character-swaps.md` (milestone 8)
- `issues/56-filter-tracks.md` (milestone 8)
- `issues/57-filter-rows.md` (milestone 8)
- `issues/58-editor-widget-tests.md` (milestone 8)
- `issues/59-move-spans.md` (milestone 8)
- `issues/60-trim-spans.md` (milestone 8)
- `issues/61-filter-lists.md` (milestone 8)
- `issues/62-add-and-remove-animations.md` (milestone 8)
- `issues/63-animation-settings.md` (milestone 8)
- `issues/64-window-tests-and-editing-without-a-host.md` (milestone 8)
- `issues/65-first-animation-and-converter-exports.md` (milestone 8)
- `issues/66-duplicate-spans.md` (milestone 8)
- `issues/67-live-stage-preview.md` (milestone 8)
- `issues/68-stage-snapping.md` (milestone 8)
- `issues/69-trim-3d-spans.md` (milestone 8)
- `issues/70-curve-tracks.md` (milestone 8)
- `issues/71-group-into-sprite.md` (milestone 8)
- `issues/72-ungroup-sprite.md` (milestone 8)
- `issues/73-nudge-on-stage.md` (milestone 8)
- `issues/74-hide-depths-in-view.md` (milestone 8)
- `issues/75-sprite-export-names.md` (milestone 8)
- `issues/76-solo-and-lock.md` (milestone 8)
- `issues/77-frame-keys.md` (milestone 8)
- `issues/78-work-area.md` (milestone 8)
- `issues/79-save-the-frame.md` (milestone 8)
- `issues/80-qt-image-formats.md` (milestone 8)
- `issues/81-copy-and-paste-depths.md` (milestone 8)
- `issues/82-colour-picker.md` (milestone 8)
- `issues/83-rename-animations.md` (milestone 8)
- `issues/84-paste-across-animations.md` (milestone 8)
- `issues/85-save-images.md` (milestone 8)
- `issues/86-remove-unused-definitions.md` (milestone 8)
- `issues/87-replace-images-with-pictures.md` (milestone 8)
- `issues/88-zoom-and-pan-the-stage.md` (milestone 8)
- `issues/89-duplicate-animations.md` (milestone 8)
- `issues/90-paste-grid-controllers.md` (milestone 8)
- `issues/91-window-tests-under-load.md` (milestone 8)
- `issues/92-library-panel.md` (milestone 8)
- `issues/93-name-timeline-spans.md` (milestone 8)
- `issues/94-drop-characters-on-the-stage.md` (milestone 8)
- `issues/95-duplicate-sprites.md` (milestone 8)
- `issues/96-history-panel.md` (milestone 8)
- `issues/97-save-frame-sequences.md` (milestone 8)
- `issues/98-gutter-switches.md` (milestone 8)
- `issues/99-onion-skin.md` (milestone 8)
- `issues/100-rulers-and-guides.md` (milestone 8)
- `issues/101-choose-several-depths.md` (milestone 8)
- `issues/102-align-and-spread.md` (milestone 8)
- `issues/103-remove-chosen-depths.md` (milestone 8)
- `issues/104-search-filters.md` (milestone 8)
- `issues/105-arrange-depths.md` (milestone 8)
- `issues/106-motion-path.md` (milestone 8)
- `issues/107-split-spans.md` (milestone 8)
- `issues/108-time-reverse-keyframes.md` (milestone 8)
- `issues/109-easy-ease.md` (milestone 8)
- `issues/110-trim-clip-to-work-area.md` (milestone 8)
- `issues/111-graph-editor.md` (milestone 8)
- `issues/112-sequence-depths.md` (milestone 8)
- `issues/113-timeline-snapping.md` (milestone 8)
- `issues/114-graph-retime.md` (milestone 8)
- `issues/115-drop-characters-on-the-timeline.md` (milestone 8)
