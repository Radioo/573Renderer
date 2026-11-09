# 573Renderer

Renders, inspects and edits the 2D animation content that KONAMI arcade games ship inside IFS files, using the game's own afp/avs DLLs as the reference for what the content means.

## IFS editor

### Files

**IFS**:
The container file a game loads, holding a tree of entries. The editor opens and saves an IFS directly, with or without a project.
_Avoid_: archive, scene

**Project**:
An optional editor file tied to one IFS that keeps the live source of what was authored in the editor: keyframes, script source text and source images.
_Avoid_: workspace, scene, source file

**Authored content**:
The depths of an IFS, each over a frame range, that have live source in its project. Their baked data is read-only and changed through that source.
_Avoid_: project content, editable content

**Keyframe**:
A value set on a whole frame for one property of an authored depth, from which export produces a placement on every frame.
_Avoid_: key, placement

**Own**:
To turn one baked depth over a frame range into authored content, keying every frame a property is set on so nothing changes. Not every frame sets every property: measured over IIDX 33, 70653 spans vary which properties each frame carries, so keying every frame with everything would not be lossless.
_Avoid_: import, convert, unbake

**Detach**:
To turn authored content into baked data and drop its source from the project.
_Avoid_: flatten, bake, unlink

**Baked data**:
The parts of an IFS that exist only in the form the game reads, as per-frame placements, instruction lists and atlas pixels. Everything in a shipped game file is baked data.
_Avoid_: flattened data, compiled data, raw data

**Export**:
Writing a project's authored content into its IFS as baked data.
_Avoid_: bake, build, compile, publish

**Entry**:
One file stored inside an IFS. Entries that are not part of a package are edited as whole files: add, replace, remove.
_Avoid_: node, blob, asset

**Package**:
The AFP content inside an IFS: its texture atlases, shapes, animations, fonts and the manifests that list them.
_Avoid_: scene, bundle, afp file

**Dependency**:
Another package that a package names as required, whose images, shapes and fonts it may use. A dependency comes from a different IFS and is read-only unless that IFS is opened itself.
_Avoid_: parent, super, linked package

**Round trip**:
Opening an unmodified IFS and saving it without edits, with every entry encoded again from what the editor understood of it.
_Avoid_: re-pack, rebuild, pass-through

**Target build**:
The game build whose afp/avs DLLs decide which package features a document may use and which the saved IFS must load on.
_Avoid_: game version, afp version

**Verified feature**:
A package feature shown to work in the target build's real game, either because the game's own data uses it or through a recorded in-game check. A feature the target build's afp-core accepts without that proof is an unverified feature.
_Avoid_: supported feature, safe feature

**Unknown data**:
Any part of an entry the editor keeps byte for byte without understanding it, shown as unknown and protected from edits that could disturb it.
_Avoid_: raw data, opaque data, junk

### Animation

**Animation**:
One afp entry of a package, listed in its afplist, with a root clip.
_Avoid_: layer, afp, movie, stream

**Clip**:
A timeline of frames that places characters at depths. Every animation has a root clip, and a clip can define further clips inside it.
_Avoid_: movie clip, mc, sprite, composition, layer

**Character**:
Anything a placement can put into a clip: a shape, image, text, button or another clip.
_Avoid_: symbol, object, asset, definition

**Frame**:
One step of a clip's timeline.
_Avoid_: tick, time

**Depth**:
A stacking slot inside a clip. It holds at most one character at any frame, and may hold different characters over time.
_Avoid_: layer, z-order, track

**Placement**:
An instruction on a frame that puts a character at a depth, or changes the character already there.
_Avoid_: keyframe, place object, layer

**Label**:
A name attached to a frame of a clip, used as a jump target.
_Avoid_: marker, cue

**Script**:
Bytecode that the game runs during playback, attached to a frame, a placement or a button.
_Avoid_: action, actionscript, code

**Library call**:
A script that only calls one aeplib function with constant arguments, such as setting a rect mask or jumping to a label, edited as a single item on the timeline.
_Avoid_: action, command, event

### The editor's own parts

**Command**:
One named thing the editor can do, registered once with its id, its words, its shortcut and the sentence it says when it cannot run. Menus, the selection bar, the tool strip and the command search are all views over the same registry, so a command reads and refuses the same way wherever it is reached from.
_Avoid_: action, operation, feature

**Refusal**:
The sentence a command says instead of running, naming what is missing. A greyed menu item carries its refusal as a tooltip, and running a command from anywhere else says it in a notice.
_Avoid_: error, validation, disabled reason

**Selection bar**:
The strip under the stage that names what is chosen (a depth, several depths, keyframes or nothing) and carries the commands that act on it.
_Avoid_: toolbar, action bar, context bar

**Notice**:
A line that appears under the stage to report what an edit did, sometimes with an Undo, and goes away by itself. The status bar keeps host state and errors; a notice is for the result of something the user just did.
_Avoid_: toast, alert, message box

**In place**:
Editing a clip while the root stays on screen around it, dimmed, with the clip's own outlines mapped onto the root through its placement. The opposite is showing the clip on its own, which is what the editor falls back to when the clip is not placed on the root at the playhead.
_Avoid_: in context, isolation mode, edit in place mode
