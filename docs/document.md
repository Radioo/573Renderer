# The document model (`r573_document`)

`src/document/` turns an `Ifs::Archive` into what the editor's panels show. It
has no Qt in it and no engine in it, so every rule below is tested under `ci`
with synthetic packages, and once more under `local` against a shipped IIDX 33
package.

## The outline (`document/outline.h`)

`Document::Outline::Build(archive)` walks the manifest once and produces a tree
of `Node` values in manifest order. Each node carries:

- `kind`, straight from the archive: directory, file or special node. A file
  whose `super_index` is non-zero keeps that index and has no bytes of its own,
  which is how the tree marks a super-image reference.
- `stored_name`, the manifest node name, and `name`, that name unescaped
  (`Ifs::UnescapeName`). `tex/texturelist_Exml` reads as `texturelist.xml`.
- `path`, the unescaped names joined with `/`. This is the key everything else
  in the editor uses, so it never changes when a nicer name is found.
- `role`, what the entry is.

**A role comes from where the entry sits, never from its bytes.** The package
layout is fixed (docs/formats.md and the package notes): `magic`, `version.xml`
or `cversion` at the root, `tex/texturelist.xml` and the images beside it,
`afp/afplist.xml` and the animations beside it, `afp/bsi/` for byte order
scripts, `geo/` for shapes. Sniffing bytes would be guesswork and would
misread a file whose content the editor does not understand yet.

Entries under `tex/`, `afp/`, `afp/bsi/` and `geo/` are stored under the MD5 of
their logical name, so `Build` reads `texturelist.xml` and `afplist.xml`, hashes
each listed name and hands the pretty name to the matching node (an animation's
byte order script gets the same name). Anything that does not line up, a
texture list that will not parse or an image with no entry, lands in
`Problems()` instead of failing the open: a package with a broken list still
has to be openable, because that is the package a user wants to look at.

`Describe(archive, path)` fills a `Details` for one entry, and only then does
the expensive work: a texture's format and pixel size come from the list read at
build time, and an animation is decoded through `AfpAnimation::ReadStored` with
its `afp/bsi/` script to get the frame count, the labels and the depth rows. An
animation with no script is an error naming the path it looked for.

`Fields(details)` turns that into the name and value pairs the inspector shows,
so the widget holds no formatting rules.

## Depth rows (`document/timeline.h`)

`Document::DepthRows(clip)` is what the timeline draws: one row per depth, each
with the frame spans the depth is placed over.

The walk follows the clip's own frame list. A placement without the `0x1`
update bit creates or replaces the character at its depth, so it opens a span
(and closes an open one on the previous frame, because a replacement is a
different character in the same slot). A remove closes the open span on the
previous frame. Spans still open at the end run to the last frame. A frame
whose tag range points past the tag list stops the walk rather than reading out
of bounds.

Rows come back sorted by depth and each row's spans sorted by first frame.

## The open document (`document/document.h`)

`Document::File` is the editor's open IFS. `Open(bytes)` reads the archive and
builds its outline; `Nodes`, `Problems` and `Describe` are the outline's, so a
caller holds one object rather than an archive and an outline that must be
passed to each other.

`ReadAnimation(path)` decodes an animation with its `afp/bsi/` script, and
`WriteAnimation(path, animation)` encodes both again with
`AfpAnimation::WriteStored`, replaces the two entries and rebuilds the outline.
An entry that lives in a super image is refused rather than half written. `Encode()` is `Ifs::Write` over the archive, which is
both what the preview host is fed after an edit and what `File > Save` writes.

Replacing an entry's bytes also updates its stored size, which is what makes
`Ifs::Write` lay the data region out again: the writer reuses stored offsets
only while every file still matches the size it was stored with, so an edit can
never leave a file overlapping its neighbour.

`document/entries.h` holds the path helpers both the outline and the document
use: `JoinPath`, `ScriptPath` (an `afp/<name>` path to its `afp/bsi/<name>`),
and `FindEntry`, which walks a slash path of unescaped names.

## Field values (`document/field_values.h`)

Every editable field is text, and one set of helpers turns text into the numbers
the format stores and back. `Numbers(text, count)` splits on commas, trims
spaces and refuses anything that is not exactly `count` integers; `SetScalar`
and `SetVector` range-check each one against the field's own type and refuse a
value that does not fit rather than truncating it. An empty string clears the
optional, which is how a field is dropped from a tag. `Scalar`, `Vector` and
`Join` print the same shape back. Placement fields and camera fields both go
through these, so the two cannot drift in what they accept.

## Editing a placement (`document/placement_edit.h`)

`LivePlacementTag(clip, depth, frame)` returns the tag index of the placement
that is live at a depth on a frame: the last placement at that depth on or
before the frame, unless a remove came after it. That is the placement the
inspector edits.

`PlacementFields(animation, placement)` lists the placement as name and value
pairs, and `SetPlacementField` writes one back. The value text is what the
format stores, not a converted unit, so nothing is lost on the way through:
a pair or a matrix is its numbers separated by commas, a name is the string it
resolves to in the animation's table, and an empty value means the field is
not there. That is the whole mechanism for adding and dropping a field: the
placement writer derives every presence bit from which optionals are set
(`PresenceFlags` in `afp_placement_write.cpp`), so clearing the optional clears
the bit and its bytes.

Setting a name interns the text in `Animation::strings`, reusing a string that
is already there.

Only the fields the editor understands are editable. A placement that carries
clip actions, filters, curves, a controller or discarded words shows them as
`Unknown data`, and `PlacementFieldIsEditable` refuses them, so the parts of the
format that are kept byte for byte cannot be disturbed from the inspector.

## Undo and redo (`document/history.h`)

`Document::History` is a stack of whole documents. Before an edit the caller
records the document as it was, with a name a user recognises; `Undo(current)`
hands back the recorded document and keeps the current one for `Redo`.

A step is a `Snapshot`: the `File` and the project's authored content
(`std::vector<AuthoredDepth>`) together. The authored content is not stored in
the file until export, so a history of files alone undid a keyframe edit in the
viewport while leaving the keyframe changed, and the next export wrote the
undone edit back. With both in one step, undo and redo can never leave the two
disagreeing, and an edit that only changes authored content, such as owning a
depth, is undone the same way as any other.

Snapshots rather than deltas is a deliberate choice: an undone edit is then
proved by comparing two `Document::File` values, which is what the `ci` tests
do, and no edit can forget to write its own inverse. The cost is a copy of the
archive per step, so the stack is bounded (32 steps by default) and the oldest
step is dropped when it overflows. A package is tens of megabytes, so a much
deeper stack would need deltas instead.

The history also owns whether the document matches what is on disk, because
that is a property of where you are in the stack, not of the document object.
`MarkSaved` records the current depth, `Saved()` compares it to the depth now,
so undoing past a save marks the document unsaved again and redoing back to it
marks it saved. Two cases the `ci` tests pin down:

- A step falling off the bottom shifts the saved depth down with it, because the
  document state it names is still reachable through the steps that remain.
- A saved depth of zero that falls off the bottom becomes unreachable, and the
  document is unsaved from then on however far it is undone. This is the
  freshly opened document after enough edits to overflow the stack.

Recording an edit drops the redo branch, and a saved point that lived in that
branch becomes unreachable the same way.

## Labels (`document/label_edit.h`)

Labels are keyed by name, not by frame, because that is how the game finds
them: `afp_get_labeled_frame_no` and `afp_get_script_labeled_frame_no` look a
label up by its name string. So `AddLabel`, `RenameLabel`, `MoveLabel` and
`RemoveLabel` all take the name, a name must be unique in the clip, and an
empty name is refused.

**The order the labels are stored in is the game's, not ours.** With the
animation header's `0x8` flag clear, afp-core finds a label by *binary search*
on the name (`afp_get_script_labeled_frame_no` always does), so the table has to
be in string order or the lookup misses. With `0x8` set the normal lookup is a
linear scan and the order does not matter, so the editor keeps those tables in
frame order, which is what a reader expects to see. `SortLabels` picks between
the two from that flag rather than guessing.

The install agrees. Over IIDX 33, none of the 29110 animations sets `0x8`, and
7015 of the 7021 containers with two or more labels and 2106 of the 2108 with
two or more script labels are already in name order, so writing them sorted by
name is what the shipped data does rather than a choice the editor is making.
The two tables are searched separately and have to be measured separately: a
first pass concatenated them and reported frame order as the more common shape,
which was an artefact of the measurement.

Removing or renaming a label runs `CompactStrings`, so a string no part of the
animation refers to any more leaves the table.

## The string table (`document/animation_strings.h`)

`InternString` adds a string only when the animation does not already hold it,
and `CompactStrings` rebuilds the table from what is still referenced, keeping
index 0 (the empty string the writer requires) and file order for the rest.
Both walk every place a string id can live: the movie name, exports, imports and
their assets, every container's labels and script labels, and inside tags the
placement instance and class names, image names and every bytecode string list,
through nested sprites. A reference the compaction misses would point at the
wrong string after the rebuild, so the visitor is one function used for both the
collect and the remap pass and cannot drift between them.

## Library calls (`document/library_call.h`)

Almost every script in the target build's data is one call on the imported
`aeplib` object with constant arguments. `ReadLibraryCall` recognises exactly
that shape: a `PUSH` holding the arguments in reverse order then the argument
count then the object, `GET_VARIABLE`, a `PUSH` holding the method,
`CALL_METHOD`, an optional `POP`, then `END`. The argument count in the script
has to match the items that precede it or the script is not treated as a call.
Anything else, including the scripts that hold two calls in a row and the
`getInstanceAtDepth` pattern, is not a call, and `ScriptListing` renders it as
one line per instruction instead.

**The object and the method are built-in ids, not strings.** A push of type 19
carries `0x300 + b` and one of type 39 carries `0x800 + b`, and afp-core
resolves those ids to names through a table of its own. The editor carries the
nine ids the target build's data actually uses, resolved from that table and
recorded in the repo notes; an id outside them reads as `builtin 0x...` rather
than a guess.

`WriteLibraryCall` re-reads the original script and replaces only the items the
call changed, so a call written back unchanged produces the same bytes. A string
argument is interned in the animation and then in the script's own string list;
a numeric argument is written through the smallest push type that holds it,
which is what the shipped data uses. An argument the editor did not model, such
as the `this` the calls pass, keeps its original item.

## Structure edits (`document/frame_edit.h`)

A clip stores its tags in one flat list and each frame names a range into it, so
every structure edit is really an edit to that list plus a fix-up of the ranges
that follow. `document/tags.h` does that once for everything that adds or drops
a tag: `InsertTag` puts it at the end of its frame's range and bumps `first_tag`
for every later frame, and `EraseTag` lowers the count of the frame that owned it
and `first_tag` for every frame after it. The tags before the first frame's
`first_tag` are the clip's definition tags, which belong to no frame and are
never touched.

`AddDepth(animation, depth, character, first, last)` refuses a depth that is
already taken anywhere in the range, then writes a create placement on `first`
and a remove on `last + 1`. The placement's `end_frame` is `last + 1` because
afp-core runs a tag only while its end frame is above the target frame, so a
placement live through `last` ends at `last + 1`. A span that runs to the last
frame of the clip gets no remove, which is what the shipped data does.

`RemoveDepth(animation, depth, frame)` walks out from the frame to find the span
it belongs to, then erases every placement and remove at that depth inside it,
back to front so the indices stay valid.

`InsertFrame` and `RemoveFrame` move everything that counts frames: the labels,
and every placement's `end_frame`. Removing a frame drops the tags that were on
it. A label on a removed frame stays in the clip on the frame that took its
index, clamped to the last one, because a label is a jump target and silently
dropping it would break a script that names it.

## The camera (`document/camera_edit.h`)

`CameraTag(clip, frame)` finds the camera tag placed on a frame, and
`CameraFields` / `SetCameraField` read and write it the way placement fields
work. Three fields: `Camera` is the camera number and is always there, while
`Projection centre and depth` and `Focal length` are optional and map to the
tag's two presence bits.

The names are afp-core's own. The tag's x and y are the projection centre,
because the built-in `projectionCenter` property setter writes the same two
slots of the camera object that a place writes them into; the z is added to the
focal length rather than being a third position axis; and the reader derives the
field of view from the focal length and the view extent it seeds from the
centre. `Core/afp_format.md` in the notes repo has the reader, the slots and how
to find them again. Every number is stored in twentieths of a unit, which the
editor does not convert, so what the inspector shows is what the tag holds.

`AddCamera(animation, frame, id)` places a camera on a frame that has none, with
both optional fields present and zeroed so every field is editable straight
away. `RemoveCamera` takes it off again. Both go through `document/tags.h`, so
the frame ranges stay consistent.

A camera tag updates a stored camera but does not make it the one the movie
draws with, which is a pointer afp-core only changes from the script side, so
adding a camera to a clip that never activates one leaves the viewport looking
exactly the same. A local test pins that: the edited package loads, seeks and
renders through the preview host, and the tag reads back with the values that
were typed. That is the measurement, not an assumption; the note repo has the
three functions that reach the camera list and which of them sets the active
one.

## The project (`document/project.h`)

ADR 0006's project is a folder beside an IFS holding the live source of what was
authored in the editor. This is its first piece: the folder and the manifest.
Everything the project will own, keyframes and source images and script text,
lands in the tickets that add each of them; nothing here assumes any of it, and
an IFS still opens, edits and saves with no project at all.

`project.json` is JSON because the repo already depends on nlohmann and a
manifest a person can open in an editor is worth more than a compact one. It
holds the format number it was written in, the target build, the path of the
IFS, and under `owns` the depths the project has authored: for each one the
animation, the depth, the frame range and its tracks, and a `sprite` number when
the depth is inside a sprite rather than the root. The key is left out for a
root depth, so a manifest written before sprites could be owned still reads, as
owning root depths, and a `sprite` that is not a 16-bit whole number is refused.
A keyframe writes its
curve only when its ease is `bezier`, so a manifest of ordinary keyframes stays
readable. `document/project_content.h` does that half on its own so neither file
grows without bound. `ReadProject` refuses anything it does not recognise rather
than filling in a default, including a format number that is not the one it
writes, so a project from a later editor says so instead of loading wrong.
`WriteProject` writes the same bytes for the same project every time, which is
the first half of ADR 0006's determinism obligation.

The IFS path is stored relative to the project folder, so moving a project and
its IFS together keeps working. `StoredIfsPath` computes that relative path and
falls back to the whole path when there is none, which on Windows means the two
are on different drives; `ResolvedIfsPath` reverses it. Both are pure path
arithmetic and touch no disk, so they are tested under `ci`.

The target build comes from the project when a document has one. Without a
project the editor uses the build it was compiled against, because nothing in
the editor decides a build yet.

## Keyframes (`document/keyframes.h`)

A `Track` is one property of an authored depth: a list of keyframes, each on a
whole frame, each holding the property's numbers and the ease that reaches the
next one. Values are `int64_t` rather than the field's own type, because a
packed colour is a `uint32_t` and would come back negative through an `int32_t`;
the range check happens when the value is written back into a placement, not
when it is keyed.

`SampleTrack` is the only thing that turns keyframes into numbers, so export and
anything drawing a curve cannot disagree. Before the first key and after the
last it holds that key's value. Between two keys the ease decides:

- `hold` keeps the earlier value until the next key, which is what afp does
  anyway when a frame does not set a property;
- `linear` walks each component evenly;
- `bezier` is a cubic timing function with control points `(x1,y1)` and
  `(x2,y2)` and endpoints fixed at `(0,0)` and `(1,1)`, the same shape CSS
  `cubic-bezier` uses. The parameter is found by exactly 40 bisection steps, a
  fixed count with no early exit, so the result is the same number everywhere,
  which is what ADR 0006's deterministic export needs.

Eases are the editor's own idea, not the format's: the game never sees one,
because export samples them into per-frame placements. `Placement::curves` is a
different thing that afp-core reads itself and is not touched here.

A sampled component is rounded away from zero, so a value halfway between two
integers moves rather than sticking. `x1` and `x2` are refused outside 0 to 1
because the timing function stops being a function of time otherwise; `y1` and
`y2` are free, which is what lets an ease overshoot.

`EaseProgress` is the function sampling uses, exposed so a curve editor draws
exactly what export will write. `WithinTime` pulls a dragged control point back
inside 0 to 1 in time and leaves its value alone. `EasePresets` holds the named
curves the editor offers: Ease (1/3, 0, 2/3, 1), the After Effects easy ease,
Ease in, Ease out, Straight and an Overshoot.

## Own and detach (`document/authored.h`)

`OwnDepth` takes the span of a depth around a frame and splits it in two. The
`AuthoredDepth` is what the project keeps: the animation, the clip the depth is
in, the depth, the frame range, and one `Track` per property, keyed on exactly the frames that set it,
every ease `hold`. The `BakedDepth` is what the IFS keeps and the project does
not: the create placement with its animatable properties cleared, the flags
every update carries, and the frames whose update sets no property.

That split is what makes the project file honest. Only the authored half is
written into the manifest; `BakedFor` derives the baked half again from the IFS
whenever it is needed, which works because the derivation is stable under
export. Re-deriving from what export wrote gives back the same baked half, and
the export tests pin that by exporting twice and comparing bytes.

`AuthoredPlacements` is the one function that turns keyframes back into
placements, used by export and by detach alike, so the two cannot disagree.

### Control bits

A placement's `0x4` (use matrix) and `0x8` (use colour) decide whether the game
applies its matrix and colour fields at all, so they cannot be treated like the
rest of an update's flags. Surveyed over IIDX 33, a field is never written
without its bit, but a bit is often set without its field: every create sets
`0x4`, and 377321 updates set one of the two with nothing behind it. Updates in
one span differ in these bits all the time, since one frame moves and the next
only tints.

So `BakedDepth::update_flags` leaves the two bits out, and own records, per
update frame, the bits it carried beyond what its fields need
(`extra_controls`). `AuthoredPlacements` then writes each frame with the bits
its written fields need plus that frame's recorded extras, and gives the create
the bits its fields need on top of the flags it was owned with. An unedited
frame writes exactly the fields it had, so it gets back exactly the bits it had.
An edited frame always gets the bit that makes the game apply what was keyed
there, which is what fixes a colour keyed on a frame that only moved being
written and then ignored. A span whose updates differ in anything else is still
refused, and a placement carrying a field without its bit is refused too,
because own could not give that back.

### Groups reset what they leave out (`document/placement_effect.h`, `document/property_groups.h`)

The game does not hold a matrix or colour part an update leaves out. With `0x4`
it starts from the identity matrix and copies the parts the update carries, and
with `0x8` it starts from multiply (1, 1, 1, 1), add 0 and copies the colours
the update carries (in 3D the matrix bit only resets the translation). The
keyframe model holds every property between keyframes, so the two only agree if
own and export account for the reset.

`placement_effect.h` is the game's rule as pure data: `ApplyPlacement` applies
one placement to an `AppliedState` (the 2D matrix, the multiply and the add
colour) and `ReplayDepth` replays a depth frame by frame the way the game does,
fresh on a create, ignored while nothing is placed, cleared by a remove.
`KeyedState` is the same state as the keyframes describe it. Every test that
changes own or export compares the two on every exported frame, and the `local`
survey compares them over every owned span of the install, which is what makes
"what the keyframes say is what the game draws" a checked statement.
`WriteAuthored` makes the same comparison after it writes a depth
(`CheckDrawnAsKeyed`) and refuses the write when any frame disagrees, so a
future mistake in the writer stops an export with the frame it would get wrong
instead of shipping it.

`property_groups.h` says which properties belong to which group and what each
one's identity is: the five matrix properties (scale, rotate, translation and
the two short forms) and the four colour properties (multiply, add and the two
packed forms). In a 3D span only translation is in the matrix group, since the
game does not apply the 2D parts there, and a span that switches between 2D and
3D is refused.

Own keys every property the span animates on every frame where the game applies
that property's group: with the value the frame carries, or with the identity
when it carries none, because that is what the game shows there. It also
records which frames carried an identity value explicitly
(`explicit_identities`), since a key cannot tell an identity the game wrote from
one it reset to.

Export then treats a group as applied on a frame when any of its properties is
keyed there, is eased through it, or the frame had a recorded extra bit. On
such a frame it writes every property of the group whose value is not the
identity, and an identity only where it was explicit, and sets the group's bit.
On other frames it writes nothing of the group, so the game holds it, which is
what the held keyframes say. An unedited span writes exactly the fields it had.
An eased translation next to a held scale now writes the scale on every eased
frame as well, where it used to be dropped and drawn at 1.

### Starting a new property

`PropertiesToAdd` lists what an owned depth could start animating: scale,
rotate, translation, multiply and add colour, each only when neither of its
encodings is animated already, and in a 3D span only the colours. `AddTrack`
starts one with a single keyframe at its identity on the first frame. The
identities are the ones the game resets to, so a new track draws nothing new
until its keyframes change, and the group rules above take it from there. Ratio,
blend, origin, the 3D fields and HSV are not offered: their values on an object
that never set them have not been read from the game, and a guessed starting
value would move what is drawn.

`OwnDepth` takes the clip to own a depth in, and `BakedFor` and `WriteAuthored`
read the clip off the `AuthoredDepth`, so a sprite depth is captured from and
written back into its own sprite. A sprite that is gone is refused with the
same message the clip edits use rather than being written into the root. The
promise own makes is the same for both: the `local` survey owns every span in
the install, root and sprite alike, writes it straight back and requires the
whole animation to come back identical.

**Keying only the frames a property is set on is the measured shape of the
data, not a simplification.** Over IIDX 33, 234363 spans set the same properties
on every frame but 70653 do not, so a keyframe on every frame carrying
everything would rewrite a quarter of the install. A property is written on a
frame when that frame has a key, or when the ease reaching it is not `hold`,
which is what makes an ease produce the per-frame placements the game needs
while an untouched depth writes back exactly the frames it came from.

Three things own keeps that look like baked detail and are: the non-presence
flag bits every update carries, the extended flag word, and the frames whose
update sets no property at all. All three were found by running own and detach
over the whole install and comparing: without the flags 41222 spans came back
different, and without the blank frames 16802 did. IIDX 33 has 529434 updates
that set nothing, so dropping them is not a corner case.

Own refuses rather than losing anything. A span whose later frames change a
name, filters, curves or anything else a keyframe cannot hold is refused and
says which; so is a span placed twice, one whose updates disagree on their
flags, and one whose frames end somewhere other than its first frame does. The
current numbers over the install are in `docs/local_regression.md`.

### Stepped properties

Character, Clip depth, Blend and Filters are properties whose values do not
blend: an object is one character or another. They are keyed like any other
property but only hold, so `SetKeysEase` and `AuthoredPlacements` refuse any
other ease on them (`PropertyIsStepped`). Character and Filters become tracks
only when an update in the span carries them; otherwise they stay on the baked
create placement, so an ordinary depth does not grow a lane for them. A swap is
then a key on the frame the update carried it, and moving that key moves the
swap.

A filter list is kept in a track as numbers (`document/filter_values.h`):
its count, then per filter a kind (0 colour matrix, 1 lookup, 2 anything else)
followed by its fields: the 4 head bytes, 20 matrix values and an HSV presence
flag with its three values for a colour matrix; the 6 head bytes, the 4 unread
bytes and the length-prefixed table for a lookup; the length-prefixed bytes
otherwise. `FiltersFrom` reads that back to the same filters and refuses numbers
that do not follow the layout. Two lists with the same kinds of filter and the
same table lengths take the same count of numbers, which is what a track needs;
a span whose updates change that count is refused (`changes the shape of its
Filters`).

## Atlases (`document/atlas.h`, `document/atlas_write.h`)

An IFS does not store a composited atlas bitmap. Each image has its own
`tex/<md5 of its name>` entry holding exactly that image's pixels, and the
texture list says where in the atlas it belongs. So packing decides rectangles,
not pixels, which is why this is a layout problem and not a compositing one.

`PackAtlas` is `stb_rect_pack`'s skyline packer with two things pinned around
it. The input is sorted by name first, and `STBRP_SORT` is pointed at
`std::stable_sort`, so images of the same size cannot swap places depending on
how the C library's `qsort` breaks ties. The atlas size is the smallest power of
two by area that fits, ties broken by width, searched from 64 up to 4096, so the
same images always land in the same atlas whatever order they were given in.
All three of those are tests.

The shipped data decided the rest. Measured over IIDX 33: of 12522 atlases,
11433 are powers of two on both sides, no image falls outside its atlas, no two
images overlap, no coordinate sits on a half pixel, and in every atlas holding
more than one image at least one pair touches with no gap at all. So the editor
packs tight, and the one pixel that `uvrect` insets on every side has to come
from inside the image rather than from padding around it.

`WithGuardRing` is that pixel: it grows an image by one pixel on each side and
repeats the edge pixels into the ring. The cell in the atlas is the artwork plus
its ring, the `imgrect` covers the whole cell, and the `uvrect` insets back to
exactly the artwork. That matches the shape 106322 of 106372 shipped images have,
and it holds whichever of the two rects the engine samples, because the ring only
ever repeats what is already at the edge.

`WriteAtlas` puts all of it into the package: one `texture` node named by the
caller, replaced rather than appended when it is already there, so exporting
twice does not grow the list. It copies the format and the other attributes from
a texture already in the package rather than inventing them.

## Script source (`document/script_source.h`)

The source language is not a design choice, it is what IIDX 33's own scripts
do. Of 463562 scripts in the install, nearly all are a run of calls on the
imported `aeplib` object, so that is the whole language: one call per line.

```
aep_set_set_frame(this, 30)
gotoAndPlay("loop")
stop()
```

An argument is a whole number, double-quoted text, or `this`. `this` is there
because the data put it there: the most common call in the install passes the
clip itself as its first argument, and leaving it out meant the compiler could
not read back a single shipped script on the first run.

The eight call names are the eight the survey counted, and nothing else
compiles. The editor would otherwise have to invent a built-in id it has never
measured, and afp-core resolves a call by id, not by name.

Compiling emits the shape the data uses: for each call a `PUSH` of the
arguments in reverse then the count then `aeplib`, `GET_VARIABLE`, a `PUSH` of
the method, `CALL_METHOD`, `POP`, and one `END` at the end. `AfpScript::BuiltinItem`
turns an id back into a push item, which is the direction the reader never
needed.

**How far this was checked.** Every script in the install was read back as
source and compiled again: 444874 of 463562 produce source, and every one of
those 444874 compiles to the same instructions and arguments, 381871 of them
byte for byte. The other 63003 differ only in whether a small number was
written in the compact one-byte form or the wide four-byte one, which is the
original author's choice and not something source text carries. The 18688 that
produce no source are the `getInstanceAtDepth` pattern and the variant with no
trailing `POP`; they stay as instruction lists and are never turned into source
on their own.

The padding after `END` is not the script's. Scripts of 16, 17 and 18 bytes of
instructions all end up occupying 18 bytes, so the slack belongs to the block
that holds them, and a compiled script simply ends at its `END`.

A depth's script is written into its create placement's clip actions at export,
keeping the triggers and the other fields the baked data had. A depth that
carried no clip actions refuses to be given a script, because what makes the
game run one has not been measured yet.

## Export (`document/project_export.h`)

`ExportProject` writes the images a project owns and then every depth it owns
into its IFS. Images come first because a placement can name one.

Reading a source image needs a decoder, and the document model has no Qt in it,
so export takes an `ImageLoader` callback instead: the editor hands it one
backed by `QImage`, and the tests hand it synthetic pixels. The project stores
the file it copied into its own folder, never the decoded pixels, so the
manifest cannot go stale against the image on disk.

`ExportProject` writes every depth a project owns into its IFS. It works one
animation at a time, taking the animations in name order and the depths within
one animation in clip, then depth, then frame order, with the root first, so the result does not depend on the
order the user owned things in. For each depth it derives the baked half from
the animation as it currently stands, asks `AuthoredPlacements` for the
placements, and writes them; a depth it cannot write names itself in the error
rather than leaving the package half done.

ADR 0006 asks for two things and both are tests rather than claims. The same
project exported into two copies of the same package produces the same bytes.
Exporting the same project a second time changes nothing, which is what proves
the baked half can be re-derived; without it the project could not store only
the authored half. A depth the project does not own is not touched at all,
because export only ever writes the spans it was given.

## Characters (`document/characters.h`)

A placement names what it shows by character id. An animation's characters are
the sprites (`AP2_DEFINE_SPRITE`), images (`AP2_IMAGE`, a package texture found
by name) and shapes (`AP2_SHAPE`, host geometry) defined in its root, plus the
assets it imports from other animations, all in one id space. `Characters` lists
them in id order, labelled the way a person would pick one: a sprite by its
export name, an image by its texture name, an import by its asset name and the
animation it comes from. A shape is labelled with the one image its GE2D file
draws, which `File::ShapeImages` reads from `geo/`, unless an export names it.
The editor's add depth offers exactly this list, so a new depth is always
pointed at something the animation can actually place.

### Placing a package image (`document/image_shape.h`, `document/place_image.h`)

A placement cannot show a texture directly. An `AP2_IMAGE` tag placed on a
depth draws nothing, and no shipped placement points at one; every picture on
stage is a shape whose GE2D file names the image. So to put a package image on
stage, `AddImageShape` makes the animation a new shape, the way the converter
does:

- The id is one above every character the animation defines or imports
  (`NextCharacterId`), which keeps the shape tags in id order, as every shipped
  animation has them. An animation already using id `0xFFFE` has none left.
- `geo/<animation name>_shape<id>` is a textured quad (`ImageQuad`): four
  vertices from (0, 0) to the image's `uvrect` size in pixels, UVs of
  `uvrect / (2 * atlas size)` on the same corners, one triangle-list primitive
  with draw flags `0x3` naming the image. The name is the animation's header
  name, which is the prefix afp-core formats shape names with.
- In a package whose `version.xml` has `shapetype` `mesh` the header flags are
  `0x20` and the file carries its bounds; elsewhere the flags are 0 and it does
  not.
- The id is appended to the animation's `geo` array in `afplist.xml`, created
  as a u16 array when the animation has none, because afp-utils reads only the
  shape files that array lists.
- An `AP2_SHAPE` tag with the id and a leading word of 2 goes at the end of root
  frame 0.

The shape is written in the byte order the package `magic` asks for. Nothing
changes unless every step succeeds. `PlaceImage` does this and then adds the
depth over the frames given, as one step, which is what the editor records as
one undo entry. The image has to be in the package's own texture list; placing
another package's image (a `0x43` shape with image-relative UVs) is not offered.

The numbers behind each rule are in `docs/local_regression.md` under the shape
survey.

## Stage outlines (`document/stage_bounds.h`)

`StageOutlines` gives the four corners, in stage pixels, of every depth a clip
shows on a frame, so the viewport can draw a selection and find what is under
the pointer. afp-core has no bounds query to ask instead: the movie clip rect
getters only read back a rect a script stored. The outline follows the engine's
own composition, read from the IIDX 33 afp-core:

- A shape's box is its GE2D rect when the file carries one, otherwise the
  bounds of its vertices (`File::ShapeBounds`).
- A placed object maps a point through `(point - origin) * matrix`: the matrix
  from its applied placement, translation and origin in twentieths of a pixel.
  The origin (`0x1000000`) is held across updates that leave it out, and a new
  object starts without one.
- A sprite's box is the box around everything it shows on any of its frames,
  each child mapped the same way. A sprite that places itself contributes
  nothing the second time round.
- Objects placed in 3D, removed ones and characters with no known box (images,
  imports) have no outline.

Each outline also carries its anchor, the stage point the object's rotation
origin lands on (its translation), and the 2x2 part of its matrix.

A sprite shown on its own is outlined in its own space, which is the stage
space the host draws it in. `DepthAt` returns the highest depth whose outline
holds a point, which is the object drawn on top.

The sprite rule is an editor choice, the way After Effects boxes a precomposed
layer by its whole composition: a sprite's current frame depends on its own
playhead and scripts, which the document does not run.

## Moving a depth on stage (`document/stage_move.h`)

`MoveBakedDepth` moves a depth by a stage offset from the placement that is
live on the frame. The offset is added to that placement's translation in
twentieths of a pixel. When that placement does not carry the matrix bit (a
colour-only update), the matrix it was showing is written into it first, with
the bit set, because a matrix bit on its own resets the scale and rotation it
leaves out. 3D placements are refused.

`MoveOwnedDepth` does the same for a depth the project owns: it starts a
Translation track if there is none, adds a key on the frame if there is none,
and shifts that key.

### Scaling and turning about the anchor

A `Reshape` is a scale along the object's own axes and a turn about its anchor.
Because the anchor is where the translation puts the origin, neither changes
the translation: the 2x2 part becomes `S * M * R` in the row-vector form afp-core
uses, with `S` the scale and `R` the turn (positive is clockwise on screen, as
y points down). `ScaleToReach` works out the scale that takes a grabbed point
to the pointer in the object's own coordinates, so a dragged corner follows the
pointer exactly; `TurnToReach` takes the angle swept around the anchor.
`ReshapedOutline` applies either to an outline for a live preview. An object
whose matrix is flat cannot be scaled from a drag.

`ReshapeBakedDepth` writes the new 2x2 into the live placement (carrying the
matrix first, as for a move). A placement that uses the short forms keeps them
while the value fits a s16 at `/32768`; otherwise the long form at `/1024` is
written and the short one dropped. A part that ends up as the identity is left
out, which the matrix bit reads as the identity. `ReshapeOwnedDepth` keys Scale
and Rotate skew on the frame (or their short twins when the depth tracks those),
starting either track when it is missing.

## Clips (`document/clip.h`, `document/clip_edit.h`)

A clip is the root of an animation or one of its sprites, named by a `ClipId`
whose `sprite` is empty for the root. It is never a path: shipped sprites are
only defined in the root, so the root and its sprites are every clip there is.
That matters because the sprites are most of the content. Surveyed over IIDX 33,
69% of all placements are inside sprites, every animation has more placements in
its sprites than in its root, and sprites carry their own labels and more of the
cameras than roots do.

`Clips` lists an animation's clips with the root first and the sprites in the
order they are defined, each with its export name when an export entry names
it, and `ClipLabel` is how a person reads one. `FindClip` resolves an id and
`RequireClip` does the same with a message naming the sprite when it is gone,
which is what every edit uses, so an edit aimed at a sprite that no longer
exists is refused instead of landing in the root. `DescribeClip` is the frame
count, labels and depth rows of one clip, and `Outline::Describe` is the root
case of it.

Every edit that works on a clip takes the `ClipId` as its second argument:
the structure edits, the label edits and the camera edits, and the three in
`clip_edit.h` that the inspector's cells reach (a placement field, a library
call argument, a camera field). A sprite's label table is kept in the same order
as the root's, by name unless the header asks for a linear lookup, because the
survey found shipped sprite tables sorted that way too. A label or a depth is
bounded by the frame count of its own clip, not the root's.

`InspectFrame` reads the clip its `Selection` names, so a sprite's placements
and cameras show when the sprite is picked, and a sprite that has gone shows one
row saying so and nothing to edit.

### Previewing a sprite (`document/sprite_preview.h`)

`PreviewSymbolFor(file, animation, clip)` answers the question the preview host
needs answered before it can show a sprite by itself: which bytes to load and
which name to ask for. The game only finds a symbol by its export name, so a
sprite with one gets the document's own bytes and that name. A sprite without
one gets a copy of the package whose animation carries an extra export entry,
named `r573_preview_sprite_<id>` with underscores added until no export already
uses it, and the open document is never changed. The entry goes where the
game's lookup expects it: the export table is binary searched with ASCII
letters folded to lower case, and every shipped table (29110 in IIDX 33) is in
that order, 1239 of them in that order but not in byte order, so the new entry
is inserted before the first name that folds greater. The root clip has no
symbol to show and a sprite that is gone is refused with the usual message.

### Removing a frame keeps the definitions in it

`RemoveFrame` drops only the per-frame commands in the frame (place, remove,
frame action, camera and start sound) and keeps the rest, merging what is left
into the frame that takes the removed one's place, or into the one before it
when the last frame goes. It used to drop every tag in the frame, and that was
destructive: 145900 of the 171786 shipped sprite definitions sit inside root
frame 0's range, so removing root frame 0 deleted most of an animation's
sprites. afp-core handles definition tags when it initializes rather than as
frame commands, which is why keeping them and moving them to a neighbouring
frame does not change what the animation defines. A tag the model does not know
is kept too, unless its code is start sound, because a frame edit has no
business deleting what it cannot read.

## Playback (`document/playback.h`)

The rate an animation plays at is in the animation, not in a setting.
`FrameRate` reads the header's `fps` as `s32 / 1024` when the header flags carry
`0x2` and as a float otherwise, and falls back to 60 when the result is not
finite or is not a rate anything could play at.

That fallback is safe rather than a guess, because the two encodings do not
overlap in the playable range: a float between 1 and 240 read as fixed point is
about a million, and a fixed point rate in that range read as a float is a
denormal near zero. Reading with the wrong rule therefore always lands far
outside the range instead of producing a plausible wrong rate. Surveyed over
IIDX 33, all 29110 shipped animations set the flag and land on 60, 30, 29.97 and
15, which is also what confirms the scale: a wrong divisor would not give whole
numbers across that many files. The survey is a `local` test so it can be run
again on another build.

`Advance` is where playback goes next, and it takes the frame count rather than
looking at what was drawn: the end of an animation is afp's frame count, never a
comparison of rendered frames. It returns both the next frame and whether
playback continues, so a caller stops for the same reason the model does, and a
looping animation of one frame keeps playing rather than stopping on itself.

`FrameIntervalMs` is whole milliseconds and never zero, since a timer given zero
would spin.

## Inspector rows (`document/inspector.h`)

`InspectFrame` is the whole of what the inspector shows for a frame: the rows
and, per row, whether the user may type into it and what an edit to it changes.
It takes a `Selection` (the depth, the frame, the authored depth when the
project owns one, and the selected keyframe) and nothing about a window, so what
the panel shows is tested rather than looked at.

It lives here rather than in the widget because it is not formatting, it is
policy. The order the rows come in, that an owned depth keeps its own rows
alongside the placement fields, that a depth holding nothing on a frame says so
instead of showing an empty placement, and that only the keyframe value is
editable on an owned depth are all decisions about the document. The one time
this lived in the widget, `fields = PlacementFields(...)` dropped the rows
gathered before it and the "Owned by the project" row went missing for as long
as owned depths existed, because nothing could test it.

`EditTarget` is the second half of that. A row carries what it edits, so the
widget neither works out editability from the row's name nor decides which
setter an edit reaches. Naming was doing both jobs before, which is why a
keyframe row had to be given a name no placement field could collide with.

A camera row is editable whichever depth is selected, because a camera belongs
to the frame. The all-or-nothing editable flag this replaced made the camera
read-only whenever the selected depth happened to be owned, which was never
intended.

## Editing keyframes (`document/keyframe_edit.h`)

`keyframes.h` holds the track and its sampling; `keyframe_edit.h` is what an
editor calls, because every edit a user makes names a property and a frame
rather than a track and an index. Each function finds the track on an
`AuthoredDepth` by property name and refuses a property the depth does not
animate, so a stale selection cannot reach into the wrong track.

`AddKeyAt` is the one with a rule worth stating. It takes the value the track
already samples at that frame and the ease of the segment it splits, so adding
a keyframe to a hold or a linear segment changes nothing that any frame samples.
That is what makes a dense track safe to refine: `OwnDepth` captures a keyframe
on every frame, and the user thins it out and re-eases it without the render
moving underneath them. On a bezier segment the two halves cannot carry the
shape of the whole, so the curve does change there.

`AddKeyAt` and the selection edits below refuse a frame outside the range the depth was owned
over, because `AuthoredPlacements` only writes inside that range and a keyframe
beyond it would be silently dropped at export.

`RemoveKeyAt` refuses to take the last keyframe of a property rather than
deleting the track. A track that is gone stops being written, so the property
would vanish from the placements and the render would change on a step the user
asked to be a deletion of one keyframe. Detaching the depth is how a user stops
animating it.

`SetKeyValueAt` takes the value as text and parses it with `Numbers` against the
arity the keyframe already holds, which is the same rule the placement fields
use, so a keyframe and the placement field it feeds are typed the same way.
`KeyAt` and `KeyValueText` are the read side, so a widget showing a keyframe
does not reimplement the lookup or the joining.

## Keyframe selections (`document/key_selection.h`)

A selection is a list of `KeyRef`, a property and a frame. Every selection edit
works on a copy of the depth and keeps it only when all of it succeeded, so a
paste or a move that fails halfway leaves nothing behind.

- `CopyKeys` returns a `KeyClip`: the selected keyframes grouped by track in the
  depth's track order, their frames counted from the earliest selected one, so
  the spacing between tracks is kept.
- `PasteKeys` puts a clip down with its first frame at a given frame. A keyframe
  landing on an existing one replaces its value and ease. A property the depth
  does not animate yet is started with `AddTrack`, which also refuses the ones
  that cannot be started (a short twin of a tracked property, for one). It
  returns the pasted keyframes so the editor can select them.
- `RemoveKeys` removes them all; a track still keeps its last keyframe.
- `SetKeysEase` gives them all one ease, with its bezier when it is one.
- `ShiftKeys` moves them all by the same number of frames. A move that would
  land on a keyframe that is not moving, or leave the owned range, is refused.
  It returns where the keyframes went.

## Export drift (`document/project_drift.h`)

A project stores only the authored half, so the baked spans in the IFS are the
one copy of that content the user can lose. Anything can change them: another
editor, a game update, a hand edit. `project_drift` is how the editor notices.

`ExportedPaths` is the list of entries an export of this project would write:
each owned animation and its byte-order script, and, when the project owns
images, the texture list plus every stored image name. It drops paths the
package does not hold, so the list is always entries that exist. `RecordExported`
turns that list into `ExportedEntry` rows carrying `File::EntryDigest`, the MD5
of the stored bytes, and `ExportProject` records them at the end of every
export. The digest is of the entry as the package stores it, not of the
placements, so a rewrite that happens to produce the same bytes is correctly not
drift.

`ProjectDrift` compares those recorded digests against the package as it stands
now and returns one `DriftedEntry` per entry that disagrees: `Missing` when the
entry is gone, `Changed` when its bytes differ. Entries the project never
exported are not in the list and so never reported, which is what keeps drift
about the project's own content rather than about the package.

Note that one owned animation covers two entries, the animation and its
byte-order script, because `WriteAnimation` writes both. Changing an animation
therefore drifts two entries, and the tests assert that rather than one.

`KeepIfsVersion(project, path)` is the resolution that gives the package the
last word: it drops that path from `exported` and drops any authored depth whose
animation is that path, so the project stops owning the content and a later
export leaves the entry alone. The other resolution needs no function, because
exporting again overwrites the entry and records a fresh digest.

## Entry edits (`document/entry_edit.h`)

`AddEntry(archive, directory, logical_name, bytes)` never invents a name: the
name is hashed for the directories the package looks up by hash (`tex`, `afp`,
`afp/bsi`, `geo`) and escaped everywhere else, which is the same rule
`Ifs::HashedName` and `Ifs::EscapeName` already carry. The new entry takes its
binary XML node type from the files already in that directory rather than a
constant, so an archive whose files are `2s32` does not gain a `3s32` one.

`ReplaceEntry` and `RemoveEntry` work on the same unescaped paths the outline
uses. Replacing keeps the stored size in step with the new bytes, which is what
makes `Ifs::Write` lay the data region out again.

`AddImage` and `RemoveImage` keep the texture list and the entries in step,
which is the only pair in the package where one cannot change without the other.
A new image gets a `texture` of its own rather than a rect inside one that is
already packed, so it can never land on top of another image: the new node is a
copy of an existing texture's attributes, which is where its filters and wrap
modes come from, with the name and the `size` replaced and one `image` child.

That child is built the way the shipped data builds it: `uvrect` first, then
`imgrect`, both `4u16` in half pixels, with `imgrect` the whole image at the
origin and `uvrect` the same rect inset by one pixel on every side. The inset is
not a guess, it is what 106322 of the install's 106372 images do (docs are in
the repo notes), and it is why an image must be at least two pixels on a side.

Removing an image takes its node out of the list, drops a `texture` that has no
images left, and removes the entry.
