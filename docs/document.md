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
