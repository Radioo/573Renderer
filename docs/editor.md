# The IFS editor application

`editor/` produces `ifs_editor.exe`, the Qt 6 Widgets front end for the IFS
editor. It is part of the root CMake project but not of the renderer's build:
nothing in `build/` depends on it, and the renderer's own build never
configures it.

## One project, two profiles

The renderer links everything statically (`x64-windows-static`, `/MT`) so
`573Renderer.exe` ships as one file. Qt in that triplet is a very long build
and vcpkg's Qt is only routinely exercised dynamically, so the editor is built
with the dynamic triplet (`x64-windows`) into its own binary directory
(`build-editor/`). A binary directory can only hold one triplet, so the two
live in the same CMake project as two configure presets rather than as two
projects:

- `dev` (and `ci`, `dev32`, ...) build the renderer. `R573_BUILD_EDITOR` is
  off, so `editor/` is never added.
- `editor` sets `R573_BUILD_EDITOR=ON` and `R573_BUILD_RENDERER=OFF`. The root
  `CMakeLists.txt` adds `editor/` and then returns before the renderer's own
  targets, so the editor profile configures nothing of the renderer.

vcpkg follows the same split through manifest features: `vcpkg.json` keeps the
shared libraries in its core dependencies, puts `imgui`, `ffmpeg` and
`dxsdk-d3dx` behind a `renderer` feature that is in `default-features`, and Qt
behind an `editor` feature. The `editor` preset asks for the `editor` feature
with `VCPKG_MANIFEST_NO_DEFAULT_FEATURES`, so building the editor does not
build ffmpeg, and building the renderer does not build Qt.

Being one project is what lets an IDE list both sets of targets: pick the
profile and the run configurations follow. The two still share source rather
than targets: `cmake/r573_shared_sources.cmake` lists the support, format and
preview sources both consume, so a file added to the renderer's format layer is
picked up by the editor with no second list to keep in step.

The editor links no renderer app, GUI or backend code. Everything it needs
from the engine happens in the preview host process (docs/preview_host.md).

## Building

```
editor\build.bat
```

Same shape as the renderer's `build.bat`: vswhere locates Visual Studio,
`vcvarsall x64` sets the toolchain up, vcpkg is bootstrapped if needed, then
`cmake --preset editor` and `cmake --build --preset editor` from the repository
root. The preset writes to `build-editor/` (gitignored) and inherits
`VCPKG_HOST_TRIPLET` `x64-windows` from `base`, so `flatc` is found as a host
tool. Running plain `cmake` from a
shell with no Visual Studio environment fails at the compiler check with
`LNK1104: cannot open file 'kernel32.lib'`, which is what the batch file
exists to prevent.

## Widget tests

`build-editor/editor_widget_tests.exe` drives the timeline, the viewport and
the ease curve with synthetic mouse events, with no window shown and no host:
its `main` sets `QT_QPA_PLATFORM=minimal` before creating the `QApplication`,
and the minimal platform plugin is deployed next to it. The cases cover
clicking, Ctrl-clicking and box-selecting keyframes, dragging a selection by
whole frames, a selection losing keyframes that are gone, stage clicks in stage
pixels, moving, scaling and turning a selection with its handles, and dragging
an ease handle past the segment's time. `tools/checks.sh` builds the editor and
runs them. Two mutations (moving the turn handle, clearing the selection on a
Ctrl press) were each seen to fail a case before the tests were trusted.

`build-editor/editor_window_tests.exe` builds the whole `Editor::Window` from the
same sources, with no host, and drives it the way a user would. Its cases live
in two files: `editor/tests/editor_window_tests.cpp` for the ones that need no
game install and `editor/tests/editor_live_window_tests.cpp` for the ones that
start the preview host (they skip without `R573_IIDX_DIR`), with the `Script`,
its steps and the package helpers shared from
`editor/tests/window_test_support.h`. Its `main` points
`QSettings` at a temporary INI directory under its own organisation name, so it
never reads the user's game install or layout. Each case writes a small package
to a temporary directory and opens it. Context menus are raised by emitting the
widget's `customContextMenuRequested` signal; a `Script` of steps, run from a
timer while the menu and its dialogs block, picks a menu item by selecting it
and pressing Return (which is what makes `QMenu::exec` return it), fills in
`QInputDialog`s, and closes any warning box while keeping its text so a case can
check what was reported. The cases cover opening an animation without a host,
editing and undoing an animation setting, making and removing an animation from
the package menu, refusing a taken name, and the background option. Putting back
the old early return for a missing host was seen to fail four assertions.
Every wait is bounded: `Settle` gives up after a minute, and a `Script` still
running after that closes whatever dialog or menu is open and records a timeout,
so a step that never matches fails the case instead of hanging the gate (seen by
giving the duplicate case a step that cannot match). The bound is only a guard
against hanging, and every wait expects something to happen, so a longer one
slows only a case that is failing anyway. It was ten seconds, which a busy
machine outlasts: with every core kept busy by other processes, a script that
opens a `QFileDialog` and reloads the host, or renders twice through it, ran
past ten seconds and failed on every one of four runs, which is how the suite
came to fail now and then right after a build. With a minute, the same four
loaded runs all passed. A case that needs a second
script closes the first one first, because a live script also takes the warning
boxes.

## Qt plugin deployment

vcpkg's applocal step copies the Qt DLLs next to the executable, but not the
Qt *plugins*, and Qt aborts at startup with a message box when it cannot find
a platform plugin. vcpkg's qtbase does not install `windeployqt` (it lives in
qttools), so `editor/CMakeLists.txt` copies the plugins the application needs
straight from their imported targets in a post-build step:
`Qt6::QWindowsIntegrationPlugin` into `platforms/`,
`Qt6::QModernWindowsStylePlugin` into `styles/` and `Qt6::QJpegPlugin` into
`imageformats/`. The destination subdirectory comes from each target's
`QT_PLUGIN_TYPE` property, so the copy cannot drift from what Qt expects to
find.

A plugin's own dependencies need the same care: `qjpeg.dll` needs
`jpeg62.dll`, and vcpkg's applocal step only resolves the executable's
dependencies, not a plugin's. Windows also looks for a plugin's dependencies
next to the executable rather than next to the plugin, which was checked by
hand: with `jpeg62.dll` in `imageformats/` Qt still listed no JPEG support,
and with it beside the executable it did. So the step copies the plugin into
its subdirectory, copies it once more beside the executable, runs vcpkg's
`z-applocal` on that second copy and deletes it, which leaves the plugin where
Qt looks and its dependencies where Windows looks.

The copy beside the executable is named after the target
(`ifs_editor_qjpeg.dll`, `editor_window_tests_qjpeg.dll`). `ifs_editor` and both
test executables deploy `qjpeg` into the same directory, and ninja links them in
parallel; with one shared name, one target's delete removed the copy while
another's `z-applocal` was reading it (`qjpeg.dll: warning: no such file or
directory`), which failed about one relink in three. Eight relinks in a row
passed once each target had its own name.

## Window layout

`Editor::Window` is a `QMainWindow` hosting a Qt Advanced Docking System
`CDockManager`. The viewport is the dock manager's central widget, with the
package tree docked left, the library below it, the inspector right with the
history below it, and the timeline bottom. `View > Panels` has a toggle for each panel (the dock's
own toggle action), so a panel closed with its title bar button can be opened
again. Qt 6
Widgets is ADR 0003; the panel arrangement is the one the editor design
settled on before implementation started.

Window geometry, `QMainWindow` state and the dock manager's own state are
saved to `QSettings` on close and restored in the constructor
(`editor/src/editor_layout.cpp`). The dock state is saved and restored with a
layout version (`kDocksVersion`), and the dock manager ignores a saved state of
another version, so a layout saved before a panel existed falls back to the
default arrangement instead of restoring without the new panel. Raise the
version whenever the set of panels changes; it went to 1 with the library and
2 with the history. The organisation and application names
`QSettings` keys off are set in `main` before the window exists.

## The host

`Editor::Host` (`editor/src/editor_host.cpp`) is the Qt-free wrapper around
`PreviewClient::Host`. Callers say `ShowAnimation(package, animation, bytes)`
and the wrapper picks the request: the same package again is a
`SelectAnimation`, a different one is a `LoadPackage`, and the reload flag is
set once any package has been loaded, because the host has to destroy what it
holds before it can take new bytes under a new name.

`FindPreviewHost` looks for `preview_host.exe` next to the editor first and
then in the renderer's `build/` directory, so a developer with both projects
built needs no copying. The application starts the host on the install it
remembers, or on the one the user picks from the File menu, and stops it on
close; a host that fails to start or refuses a request shows in a warning box
and leaves the window and its document alone.

Nothing but the picture needs the host. Without one, choosing an animation
still fills the clip list, the timeline (its frame count and depths come from the
document, `Document::DescribeClip`) and the inspector, and every edit reruns the
same refresh (`Window::Reload` skips only the host calls), so an IFS can be
edited without a game install; the viewport says which install to choose for a
preview.

## Panels

The window holds no format or engine knowledge: the document model
(docs/document.md) computes everything and the widgets draw it.

**Package tree.** `File > Open IFS...`, or a path on the command line, reads
the file, runs `Ifs::Read` and builds a `Document::Outline`. Each node becomes
a `QTreeWidgetItem` with the entry name, what it is (directory, special, `in
super image N`, or the role name) and the stored size; the item carries the
node's path in `Qt::UserRole`, which is the only handle the rest of the window
uses. Problems the outline reported show in the status bar rather than a
dialog, because a package with a bad texture list still opens.

**Library.** The characters of the open animation, as `Document::Characters`
labels them, each with how many placements use it (`Document::CharacterUses`),
greyed when nothing does (`editor_library.cpp`). It is filled again whenever the
clip timeline is (`ShowClipTimeline`), which every edit, reload and clip change
goes through, and emptied when the animation closes. Double-clicking a sprite
shows it on its own, as picking it in the clip box does; the switch is posted to
the event loop because it refills the library, which would otherwise delete the
item while its own double-click signal is still being delivered. Its menu places
the character on a new depth from the playhead, asking for the depth (the first
free one is suggested) and the last frame the way the timeline's add depth does
(`Window::AskForLastFrame`, `Window::AddCharacterDepth`, both shared with it).
On a sprite it also offers `Duplicate this sprite` (`Document::DuplicateSprite`,
one undo step, refilling the clip box so the copy can be edited on its own), and
with a depth selected, `Use on depth N from frame F`, which sets the character
of the placement live on that frame through the same field edit the inspector's
Character row uses, as After Effects replaces a layer's source. Duplicating a
sprite and using the copy on a depth is how a variant is made without touching
the other places the original is shown.
The package tree and the library are named (`package`, `library`) so a test can
tell the two trees apart.

A character can also be dragged from the library onto the stage, as footage is
dragged into a composition. The library's drag data carries the character id
under `application/x-ifs-editor-character` (`editor_mime.h`); the viewport takes
that format only, maps the drop point to stage pixels through the same
rectangle every other stage gesture uses, and emits `CharacterDropped`. The
window places the character on the first free depth (`Window::NextFreeDepth`,
the rule add depth suggests with) from the playhead to the clip's last frame,
with its origin at the drop point (`Document::PlaceAtPoint`), as one undo step,
and selects the new depth. While a sprite is edited over the root view the
stage shows the root, so a drop there has no place in the sprite and is refused
with a message saying to show the sprite on its own first.

**History.** Every step the undo stack holds, under a first row, `Start`, which
is the oldest document the stack can still reach (32 steps back at most), then
each edit by the name the Edit menu gives it. The row for where the document is
now is selected and the steps after it, which redo would bring back, are
greyed. Clicking a row jumps there in one go (`Window::JumpInHistory`,
`Document::History::Jump`) and shows the result once, instead of once per
step. The list is refilled with the Edit menu's labels (`RefreshState`), so it
follows every edit, undo, redo and jump; the jump is posted to the event loop
because refilling deletes the clicked row.

**Inspector.** Selecting an item calls `Outline::Describe` and prints
`Document::Fields` as one line per field. When the selection is an animation
the window also shows it.

**Viewport.** `Editor::Viewport` paints one `QImage` scaled to fit, and emits
its new size when it is resized. The window debounces that by 120 ms, asks the
host for the largest size with the stage's aspect that fits (the `Frame` reply
carries the stage size, so a first render at the default size tells the window
what to ask for), renders, and turns the shared texture into pixels with
`SharedTexture::Reader` (docs/preview_host.md). A render that fails leaves the
last good image on screen: the error goes to the status bar every time and to a
dialog only when it is not the error already showing, so a host that dies while
the viewport is being dragged does not produce one dialog per frame.

Ctrl and the mouse wheel zoom the stage around the point under the cursor,
which stays put, by the timeline's step of 1.25 a notch between a quarter and
32 times the fitted size; dragging with the middle button pans it, and `View >
Fit the stage in the view` (Ctrl+0) puts both back. Every mapping between the
widget and the stage goes through one rectangle (`Viewport::Target`), the
fitted stage scaled by the zoom and moved by the pan, so picking, dragging,
the handles, the snap reach and the guides follow the zoom without code of
their own. A zoom also asks the host for a larger render
(`Viewport::FittedSize`, reached through `ZoomChanged` and the same debounce a
resize uses), up to the stage's own size or the fitted size when that is
larger. Past that the game's pixels are shown enlarged, which is what the game
would draw, rather than a render the game never makes.

`View > Onion skin` (remembered, off by default) draws the frames either side
of the one shown over it at 35% opacity while playback is stopped, as Flash
and After Effects do, so a pose can be judged against its neighbours. After
each render the window asks the host for the previous and next frames of what
it has loaded (`Window::ShowGhostsAround`), seeks it back to the frame shown,
and hands the two pictures to the viewport (`Viewport::ShowGhosts`); a new
frame clears them, so a ghost never outlives the frame it belonged to. Playback
skips them, since every step would cost two more renders. The live window test
turns it on at frame 401, sees the picture change, and checks a later render
that does not seek still shows frame 401; it also sees the picture come back
when onion skin is turned off. Leaving out the seek back, the ghost renders,
the ghost painting or the clearing on a new frame fails the tests.

Clicking the viewport maps the point to stage pixels through that rectangle
and selects the highest depth whose `Document::StageOutlines` outline
holds it, or clears the selection; the selected depth is outlined. Dragging
inside that outline moves the outline with the pointer, and letting go moves
the depth by the offset: `Document::MoveOwnedDepth` through `EditAuthored` for
an owned depth, `Document::MoveBakedDepth` through `EditAnimation` otherwise,
each one undo step. The selection also shows a square on each corner, a round
handle above the top edge and a cross at the anchor. Dragging a corner scales
the object along its own axes so the corner follows the pointer; dragging the
round handle turns it about the anchor. Both preview on the outline and are
applied on release through `Document::ReshapeOwnedDepth` or
`Document::ReshapeBakedDepth`. Pressing a handle keeps the selection even when
the handle lies outside the outline. The timeline shades the selected depth's
row.

`View > Rulers` (Ctrl+R, remembered, off by default as in After Effects) draws
a ruler along the top and left of the viewport in stage pixels, its ticks
spaced by the first of 1, 5, 10, 25, 50, 100, 250, 500 or 1000 stage pixels
that is at least 50 screen pixels apart at the current zoom. Pressing on a
ruler and dragging pulls out a guide, horizontal from the top ruler and
vertical from the left one, which is drawn in cyan across the viewport; a guide
can be picked up within 4 screen pixels and dragged again, and dropping it
back on its ruler removes it. The selection's scale and turn handles are
reached before a guide under them. Guides are view state held by the viewport
in stage coordinates, so they follow zoom and pan; `View > Clear guides`
removes them all, and opening a document clears them. The widget test pulls a
guide out, sees it drawn and snapped to, drops it back and clears another, and
sees a press on the ruler picked normally while rulers are off; leaving out
the guide in snapping, the axis check, the ruler press, the rulers setting, the
removal or the clear fails it.

Moving snaps (`Document::SnapMove`) to the stage's edges and centre, to the
other outlines' edges and centres, and to the guides within 6 screen pixels, drawing the lines it
snapped to in pink while the drag lasts. Holding Alt during the drag moves
freely. `View > Snap while moving on stage` turns snapping off and is
remembered; it is on by default. Scaling and turning do not snap.

With the viewport focused (it takes focus on a click or Tab), the arrow keys
nudge the selected depth by one stage pixel, or ten with Shift, as After
Effects does. A nudge is a finished `Dragged` with the offset, so it goes
through the same `MoveOnStage` commit as a drag, is one undo step, and does not
snap. With nothing selected, or while a drag is under way, the key is left to
the rest of the window.

While a drag is under way the viewport also emits `Dragged` and `Reshaped` with
`finished` false on every pointer move, and once more with `finished` true on
release. An unfinished one is a preview (`Window::PreviewOnStage`): the change
is applied to a copy of the document, an owned depth through the same
`BakedFor` and `WriteAuthored` steps `EditAuthored` takes, and the copy is sent
to the host and rendered at the current frame. The document and the undo
history are untouched. Previews are coalesced through a zero-length timer, so
a host that is slower than the mouse only ever renders the latest offset. The
finished one commits as before; if the commit is refused after a preview, the
view is reloaded from the document. On `graphic/1/title.ifs` one preview takes
about a quarter of a second, most of it encoding and splitting the 28 MB
package (the textures are not reloaded, see `docs/preview_host.md`).

Outlines are shown only when the viewport shows the clip
being edited, that is the root or a sprite shown on its own. Shape boxes are
read once per animation and read again after every edit.

Dragging a depth's bar by at least four pixels moves that span along the
timeline, with an outline showing where it will land, and letting go applies
`Document::MoveSpan` as one undo step; an owned span's keyframes and range move
with it (`Document::ShiftAuthored`) and the project is saved. Pressing within three
pixels of a bar's start or end, where the pointer turns into a sizing arrow,
trims the span instead (`Document::TrimSpan`, or `Document::TrimOwnedSpan` for
an owned depth, whose trimmed keyframes are then kept). The timeline menu
offers to move the span under the playhead to another depth number
(`Document::ChangeSpanDepth`), which moves an owned depth's record with it, and
to duplicate it onto another depth (`Window::DuplicateSpanToDepth`, suggesting
the first depth above every depth the clip uses), after which the copy is the
selected depth. `Copy depth N here` keeps that span in the window
(`Window::CopySpanAt`), and `Paste the copied depth here...` asks for a depth,
suggesting the same free one duplicate does, and pastes it at the frame under
the cursor into whichever clip is picked (`Window::PasteSpanAt`,
`Document::PasteSpanInto`), which is how a depth moves from the root into a sprite
or between sprites. The copy remembers the animation it came from, so pasting
after opening another animation of the package brings the sprites and shapes
the depth uses along under new ids, as one undo step. Opening a package drops
the copy, because its shapes and images belong to the package it came from. It also offers to group the selected depth and the ones above it
into a sprite (`Window::GroupDepthsIntoSprite`): it asks for the last depth, then
the first and last frame, which start as the widest span those depths have under
the playhead, and applies `Document::GroupIntoSprite` as one undo step. It is
refused while the project owns any depth in that range. The clip box is refilled
so the new sprite can be picked, keeping the clip that was being edited. The
same menu offers to ungroup the sprite on the selected depth
(`Window::UngroupSpriteAt`, through `Document::UngroupSprite`), which is refused
while the project owns that depth and refills the clip box the same way.

The menu also hides the selected depth in the view, or shows it again, and
offers to show every hidden depth when any is hidden, which is After Effects'
eye toggle. The window keeps the hidden depths per animation and clip
(`Window::hidden_`), and `LoadViewportClip` sends the host a copy of the
document without them (`Document::ViewWithout`), so every reload, stage preview
and sprite view leaves them out while the document, the undo history and the
saved file keep them. Only when something is hidden is the copy made, so the
usual preview pays nothing. A hidden depth cannot be picked on stage and has no
outline (`Window::VisibleOutlines`), and its bars are grey on the timeline
(`Timeline::SetHiddenDepths`). Opening another document shows everything again.

Solo (`Window::SoloDepth`) hides every other depth of the picked clip in the
same way, replacing whatever was hidden in that clip, and `Show every hidden
depth` undoes it. Lock (`Window::ToggleLocked`) is view state too: a locked
depth has no outline and cannot be picked or dragged on stage, while the
timeline and the inspector still reach it (`Timeline::SetLockedDepths`). Both
are forgotten when another document opens.

Each depth's gutter carries the two switches After Effects puts beside a layer:
an eye, open while the depth is shown and hollow while it is hidden, and a
padlock, filled while the depth is locked. Clicking one toggles that depth
through the same `ToggleHidden` and `ToggleLocked` the menu uses
(`Timeline::VisibilityToggled`, `Timeline::LockToggled`), without choosing the
depth or moving the playhead; clicking the depth number still chooses it. The
widget tests click each switch and the number, and see the gutter drawn
differently for a hidden and a locked depth; the window test hides, locks and
shows a depth again through the switches. Leaving out the switch press, either
connection or the drawing fails them.

Several depths can be chosen at once, as layers are in After Effects. Clicking
a depth's number chooses it alone without moving the playhead, Ctrl+click adds
or drops a depth, and Shift+click chooses every depth row from the last chosen
one to the clicked one (`Timeline::DepthsChosen`, the clicked depth last). The
window keeps the chosen depths (`Window::SelectedDepths`, the last one being the
depth the inspector and the menus act on) and the timeline shades all of their
rows. On stage every chosen depth is outlined, the last one with its handles,
and picking one of them keeps the group instead of choosing it alone. Dragging
or nudging moves the whole group by the same offset as one undo step
(`Window::MoveGroupOnStage`): baked depths through `Document::MoveBakedDepth`,
and project-owned ones by keying their translation on a copy that is written
into the animation in the same edit and stored in the project once the edit
lands, so the project never disagrees with the document. While a group moves it
does not snap to its own members, which move with it, and the preview shows
every member moved. The widget tests choose depths with Ctrl and Shift, see the
group outlined and not snapped to; the window test drops a second depth, lets
the project own it, moves the pair and checks both moved, that a later nudge of
the owned depth starts from where the group left it, and that undo puts both
back. Leaving out the group move, the kept pick, storing the owned depth, the
group outlines, the snap filter, the Ctrl toggle or the unseeking number click
fails them.

`Edit > Align` lines the chosen depths up by their left edges, horizontal
centres, right edges, tops, vertical centres or bottoms, and spreads their
centres evenly across or down (`Window::ArrangeChosen`, through
`Document::AlignOffsets` and `Document::SpreadOffsets`). It works on the chosen
depths that have an outline on the stage, needs at least two of them to align
and three to spread, and says so otherwise; when nothing would move it says the
depths are already lined up. The offsets differ per depth, so the group move
takes a list of depth and offset pairs (`Window::MoveDepthsOnStage`), which a
drag fills with one offset for all; either way owned and baked depths move in
one undo step. The window test drops a second depth, aligns the pair by their
left edges and by their centres, undoes, and checks the refusals; giving every
depth the same offset, or dropping the minimum, fails it.

Removing works on the chosen depths too. The timeline menu offers `Remove N
depths here` when several are chosen, and Delete, which removes the selected
keyframes, removes the chosen depths when no keyframe is selected, as After
Effects deletes the selected layers (`Window::RemoveChosenDepths`). Each depth's
span at that frame goes through `Document::RemoveDepth`, all in one undo step,
and the choice is cleared afterwards. It is refused, with nothing removed, when
the project owns any of the depths there, the same as grouping and ungrouping,
because the project would otherwise keep a record for a depth that is no longer
placed; that also applies to removing a single depth, which used to go ahead.
The window test drops a second depth, removes both with Delete, undoes, and
sees the removal refused once the project owns one of them; removing only the
first depth, dropping the ownership check or leaving Delete to keyframes only
fails it.
With a sprite picked in the clip box, the menu also names that sprite's export
(`Window::NameShownSpriteExport`, through `Document::NameSpriteExport`): the box
starts on the current name and an empty answer removes it. It is an undoable
document edit, and the clip box keeps the sprite picked under its new label.

A live window test hides the widest depth of `graphic/1/title.ifs` and requires
the rendered frame to change and then come back exactly; comparing with the
selection outline in the picture was seen to pass even with the filter
switched off, so both pictures are taken with no depth selected.

**Timeline.** `Editor::Timeline` draws a ruler carrying the animation's labels
at their frames, then one row per depth from `Document::DepthRows`, with a bar
over each frame span and the depth number in the gutter. Each bar carries the
name of what its span places (`DepthRow::shows`, named by the same
`Document::Characters` labels the library uses and handed over with
`SetCharacterNames` whenever the clip timeline is filled), elided to fit and
left out of a bar under 24 pixels; hovering a bar shows the name as a tooltip,
which is how a narrow bar is read (`Timeline::SpanNameAt`). The bars are inset
two pixels from their row so a 10 pixel name fits inside them. Clicking or dragging
picks a frame, which seeks the host and re-renders. The widget lives in a
`QScrollArea` because a busy animation has more depths than the dock is tall.

## What the editor remembers

`QSettings` (organisation `573Renderer`, application `IFS Editor`) keeps the
window geometry, the `QMainWindow` state, the dock manager state, the last game
install and the last directory an IFS was opened from. The game install is
booted at startup when it is set, so the preview is ready without going through
the menu; the first animation in a freshly opened package is selected, so
opening a file shows something.

## Editing

`Editor::Window` holds one `Document::File`. Clicking a depth row in the
timeline picks that depth; the inspector then shows the placement live at the
current frame (or says the depth holds nothing there) with the editable fields
writable. Committing a cell reads the animation, sets the field, writes the
animation back into the document and runs the reload loop: encode the whole IFS,
`LoadPackage` it again with the reload flag so afp-core destroys what it holds
first, seek back to the frame the timeline is on and render. Nothing touches
the disk until `File > Save`.

A field that refuses a value puts the message in a dialog and repaints the
inspector from the model, so the cell can never show a value the document does
not hold.

Right-clicking the package tree offers the entry edits: a new animation, add
an image from a file, replace the selected entry from a file, and remove it. On
an image it also offers `Save <name> as PNG...` (`Window::SaveImageAs`), which
reads the pixels through `Document::ReadImage` before asking for a path, so an
image in a format the editor cannot convert is reported without a dialog, and
writes them through `QImage`. Saving an added image gives back exactly the
pixels it was added with, alpha included. On an image, replace reads a picture
the same way adding one does (`Window::ReplaceImageWithPicture`,
`Document::ReplaceImage`) rather than taking the file's bytes as they are, which
would have put PNG bytes where the texture blob belongs; every other entry is
still replaced with a file's raw bytes. So an image can be saved, edited in
another program and put back, and a picture of a different size is refused.
Adding, replacing and saving images live in `editor_images.cpp`.
A new animation asks for a name and a frame count and copies its header from the
open animation, or the package's first one (`Window::AddNewAnimation`), then
opens it. In a package with no animation it first asks for another IFS and which
of its animations to copy (`Window::ChooseAnimationSource`). Removing an animation goes through `Document::RemoveAnimation`, is
refused while the project owns depths in it, and closes it first when it is the
one on screen. `Rename <name>...` on an animation asks for the new name
(`Window::RenameAnimationEntry`, `Document::RenameAnimation`), is refused the
same way while the project owns depths in it, closes it first when it is on
screen so nothing reads the old path mid-edit, and opens it again under the new
name; hidden and locked depths follow it. `Duplicate <name>...` asks for the
copy's name, suggesting `<name>_copy`, makes it through
`Document::DuplicateAnimation` as one undo step and opens the copy; it is
allowed while the project owns depths in the original, since the original is
left as it was. `Remove unused definitions from
<name>` (`Window::RemoveUnusedDefinitionsFrom`) runs
`Document::RemoveUnusedDefinitions` on a copy first, so a package with nothing
to remove says so in the status bar without adding an undo step; otherwise it
applies the result as one undo step, refills the clip box when that animation
is on screen (falling back to the root when the shown sprite was removed) and
says how many definitions went. It is refused while the project owns depths in
the animation, like rename and remove. The image path goes
through `QImage`, so the editor reads image formats through Qt rather than
carrying a decoder of its own, and converts to the BGRA the package stores.
That means the qtbase features decide what can be added: the editor asks for
`png` and `jpeg` on top of `gui` and `widgets` in the root manifest's `editor`
feature. Without them Qt reads only BMP, PPM, XBM and XPM, which is what the
dialog's own filter promised and could not deliver until the features were
turned on. Removing a texture goes through `RemoveImage` so the texture list
loses its node too; removing anything else is a plain entry removal.

Ctrl and the mouse wheel zoom the timeline around the frame under the cursor,
which stays put; the zoom is held as pixels per frame and turns the widget's
minimum width into what the scroll area scrolls over, and zooming out until the
animation fits the panel goes back to fitting it. The zoom is kept across the
reload that follows an edit. The ruler marks every 1, 2, 5, 10, 20, 50, 100,
200, 500 or 1000 frames, whichever first puts the marks 60 pixels apart, and
while zoomed the playhead is kept in view.

Right-clicking the timeline offers the label edits: add one at the frame under
the cursor, and rename, move or remove the label the cursor is near. Below the
labels it offers the structure edits: insert or remove a frame at that point,
and add or remove the selected depth over a range; adding one asks which of the
animation's characters it places, or which of the package's images to place as
a new shape (`Document::PlaceImage`), which is recorded as one undo step. Last it offers the camera:
add one on that frame when it has none, remove the one it has. The widget
does not own the dialogs; it emits the position, the frame and the label it
found and the window builds the menu.

A camera belongs to a frame rather than to a depth, so the inspector shows it
under whatever the selected depth holds, and shows it even when no depth is
selected. Committing one of its cells goes through the same `EditAnimation`
step as everything else, named after the frame instead of the depth.

When the selected placement carries a script, the inspector shows it under the
placement fields. A script that is one `aeplib` call reads as the call and one
row per argument, and those rows are editable; anything else is one read-only
row per instruction. Every edit, a field or a label or a call argument, goes
through the same `EditAnimation` step: read the animation, apply the change,
record the history, write it back, reload.

Every edit is recorded in a `Document::History` before it is applied, together
with the project's authored content, and undo and redo restore both and write
the manifest again, so the keyframes, the owned depths and the scripts follow
undo exactly as the package does. Owning a depth and editing a script are steps
of their own; a script edit goes through `EditAuthored`, so it is compiled and
shown straight away and changes only the owned depth it was made on. Each step
is named after the field and depth it changed, and `Edit > Undo` / `Edit > Redo` restore
the document and run the same reload loop so the viewport follows. The menu
items carry the name of the step they would undo or redo and are disabled when
there is nothing to do.

`File > New project...` asks for a folder and writes a `project.json` there
naming the open IFS; `Open project...` reads one and opens the IFS it names, and
`Close project` drops it and leaves the IFS open on its own. The title carries
the project folder's name next to the file's while one is open, and the two menu
items are enabled only when they can do anything. A project is only adopted once
its IFS has actually opened, so a manifest pointing at a file that no longer
loads leaves the window as it was. Opening a project restarts the preview host
when one is running, because the target build comes from the project.

With a project open, right-clicking the timeline also offers to let the project
own the selected depth from the frame under the cursor, and to detach one it
already owns. An owned depth's placement is shown in the inspector with a row
saying so and every cell read-only, because its baked data is produced from the
keyframes rather than edited directly. Owning and detaching both write the
manifest straight away, so what a project owns is never held only in memory.

Right-clicking the package tree with a project open offers to add an image the
project owns: the file is copied into the project's `sources/` folder under the
name given, and export packs it into the project's own atlas. That is different
from the plain add, which writes an image straight into the package as baked
data.

An owned depth's timeline menu also offers to start animating a property it does
not animate yet, from `Document::PropertiesToAdd`; the new property's lane
appears with one keyframe at its resting value, selected so its value can be
typed straight away.

An owned depth's timeline menu also offers to edit its script. The box opens on
the source the project holds, or on the depth's baked script read back as source
when it has none yet, so owning a script is something the user asks for rather
than something that happens on its own. Clearing the box hands the script back
to the baked data.

`File > Export into the IFS` writes the project's images and every owned depth's
keyframes into the
document as baked data. It is one undoable step like any other edit, and it runs
the same reload loop, so the viewport shows what was exported. Export changes the
open document and not the file on disk; `File > Save` is still what writes it
out.

Opening a project checks it against its IFS before the user can touch anything.
`Window::ReportDrift` asks `Document::ProjectDrift` which exported entries no
longer match, and asks about each one in turn: keep what the IFS holds, which
calls `KeepIfsVersion` and so drops the project's source for that entry, or
export again, which leaves the project owning it so the next export overwrites
it. The choices are written to the manifest straight away, the same as owning
and detaching, so a decision is never held only in memory. A project whose IFS
nobody touched asks nothing.

Selecting an owned depth opens its animated properties as lanes under its depth
row on the timeline, one per track, with a mark at every keyframe: a diamond for
a keyframe the animation eases out of and a square for one it holds. Clicking a
mark selects that keyframe alone, Ctrl-clicking adds or drops it, and dragging
across empty lane space draws a box that selects every keyframe it touches (with
Ctrl, on top of what was selected). Dragging a selected mark moves the whole
selection by the same number of frames, shown live, as one undo step
(`Document::ShiftKeys`). With the timeline focused, Ctrl+C copies the selection,
Ctrl+V pastes it with its earliest keyframe on the playhead into the depth
selected there (starting any track it does not have yet), Delete removes it and
Ctrl+A selects every keyframe of the depth. Right-clicking a lane offers the same
copy, paste and delete, to add a keyframe where there is none, and to remove one
or change how it leaves its frame where there is. The copied keyframes are kept
by the window, so they can be pasted into another owned depth. How a keyframe leaves its frame applies to the whole
selection when the right-clicked keyframe is part of it, and to that keyframe
alone otherwise. Bezier opens `Editor::EaseDialog`: the curve drawn with
`Document::EaseProgress` over a unit square that has room above and below for
an overshoot, two handles to drag (kept inside the segment's time), a button
for each `Document::EasePresets` curve, and the four numbers as text, kept in
step with the handles.

With a keyframe selected the inspector carries three rows for it above the
placement fields: which property and frame it is, its value, and the ease it
leaves on. A Filters keyframe shows its filters as rows instead of the value,
and an edit to one of them goes through `Window::ApplyKeyFilterEdit`. Right-clicking
the inspector with a Filters keyframe selected offers to add a colour matrix or
an HSV filter, and, on a filter's rows, to remove that filter
(`Window::ShowInspectorMenu`).

Right-clicking an editable colour row (a placement's `Multiply colour` or
`Add colour`, or the value of a keyframe on either) offers `Pick a colour...`,
which opens Qt's colour dialog on the current value, alpha included, and
writes the choice into the cell (`Window::PickColour`). Writing the cell is the
same as typing into it, so the edit is checked and recorded the usual way. The
dialog is opened after the menu closes, not from inside it: opened from the
menu's own event loop it never received its input in the window tests.

The value row is the one editable cell an owned depth has, because
the placement rows below it are produced from the keyframes rather than edited.

With no depth selected on the root timeline the inspector shows the
animation's settings instead (stage size, frame rate, background colour and
whether the header's colour is used), and editing one is an undoable animation
edit like any other. `Playback > Draw the background colour` asks the preview
host to draw the animation's background (`Window::DrawBackground`). It is off by
default, as in the game, is remembered in the settings and is sent again
whenever the host starts.

What those rows are is `Document::InspectFrame`, not window code: `ShowFrame`
reads the animation, works out which depth the project owns here, and hands the
selection to it. `FillInspector` copies the rows into the table and stores each
row's `EditTarget` on its item, and `ApplyFieldEdit` switches on that target
rather than on the row's name, so a renamed row cannot quietly become
uneditable or reach the wrong setter.

Every keyframe edit goes through `Window::EditAuthored`, which applies the change
to a copy of the `AuthoredDepth`, rebakes and writes it into the open document
through the same `EditAnimation` step as every other edit, and only then keeps
the change and writes the manifest. So a keyframe edit is undoable, reloads the
viewport, and is never recorded in the project when the write into the package
failed. It also means the open document always shows what the project says
rather than waiting for an export.

The Playback menu also steps the playhead as After Effects does: Page Up and
Page Down go one frame back and forward, Home and End go to the clip's first
and last frame, and J and K go to the previous and next change on the selected
depth. A change is a frame where a placement or remove touches it
(`Document::DepthMarks`), or, for a depth the project owns, a frame that holds a
keyframe, so an eased track is not stepped through frame by frame. Each step
stops playback, stays inside the clip, and seeks the same way a click on the
ruler does (`Window::JumpToFrame`); the clip's frame count comes from the host
when it shows the clip and from the model otherwise (`Window::ClipFrameCount`).

B and N start and end the work area at the playhead and `Clear the work area`
drops it (`Window::SetWorkArea`). Playback then stays inside it, and the ruler
shades it (`Timeline::SetWorkArea`). It belongs to the clip on screen and is
cleared when another clip or animation is shown. A live window test sets a
three frame work area on `graphic/1/title.ifs`, plays for 0.7 seconds and
requires the playhead to have stopped inside it; with the work area left out
of playback it was seen to fail.

`Playback > Play` (the space bar) plays the animation and pauses it, and
`Playback > Loop` decides whether it wraps at the end; the choice is remembered
between runs. The timer's interval is `Document::FrameIntervalMs` of the
animation's own rate, read when playback starts, so an animation authored at 30
plays at 30 rather than at whatever the editor felt like.

Each tick asks `Document::Advance` for the next frame and seeks there, which
moves the timeline playhead with it. While playing, `SeekTo` does not rebuild the
inspector: that path reads the animation back out of the package, which a scrub
can afford once and playback cannot afford sixty times a second. The inspector
is refreshed once when playback stops. Playback also stops when the animation
changes, when a document is opened, when an edit is made and when a seek fails,
so it can never keep running against something that is no longer there.

The timeline panel has a clip box above the rows. Picking an animation fills it
with the animation's clips, the root first, and picking a sprite shows that
sprite's depths, labels and frames on the timeline and its placements and
cameras in the inspector. Placement fields, library call arguments, cameras,
labels and the structure edits all apply to the clip that is picked.

Picking a sprite shows it on its own in the viewport. `Window::LoadViewportClip`
asks `Document::PreviewSymbolFor` for the bytes and the symbol name, reloads the
package with those bytes and sends `ShowSymbol`, so from then on the frame
count, the timeline, seeking and playback are all the sprite's, read back from
afp-core like the root's are. A sprite with an export name is shown under it and
the package bytes are the document's own; a sprite without one is shown under a
name that exists only in the bytes handed to the host. Picking the root reloads
the document's own bytes, which puts the whole animation back, and returns to
the frame the root was on (`root_frame_`). Every reload after an edit goes
through the same step, so an edited sprite stays on screen by itself.

If the host refuses the symbol, the window says so and falls back: the viewport
keeps showing the root at `root_frame_`, the timeline takes the sprite's frame
count from the model, scrubbing moves only the sprite's frame, and playback is
refused because there is nothing of the sprite on screen to play. If the picked
sprite no longer exists after an edit or an undo, the box refills and falls
back to the root.

Owning, detaching, keyframes and scripts work in whichever clip is picked. The
window matches an owned depth by animation, clip, depth and frame range, so a
sprite depth and a root depth with the same number are never confused, and
detaching removes exactly the record that was detached rather than anything
that shares its depth and first frame.

Scrubbing now refreshes the inspector whether or not a depth is picked, so the
camera rows of the frame under the playhead follow the playhead; before, they
only did while a depth was picked.

`File > Save the frame as PNG...` (Ctrl+Alt+S) writes what the viewport shows
to a PNG. It asks the host for one render at the stage size rather than the
fitted viewport size, restores the viewport size afterwards, and composites the
frame onto black exactly as the viewport paints it, so the file is opaque and
matches the screen. What the host's alpha means has not been read, so this is
deliberately not a transparent export; the renderer's own export path is where
that belongs. Without a preview host it says so rather than writing a file.

`File > Save the work area as PNG frames...` (Ctrl+Alt+Shift+S,
`Window::SaveFramesAs`) does the same for every frame of the work area, or of
the whole clip when no work area is set, into a chosen folder as
`<animation>_<frame>.png`, the frame number padded to four digits so the files
sort in order. Each frame is a host seek and one render at the stage size,
composited onto black by the same `Opaque` step as a single frame, so frame N
of a sequence is the file saving frame N on its own gives; a modal progress
dialog shows the frame and how many of how many, and stopping it keeps the
frames already written. Afterwards the viewport is resized back and the host
sought back to the playhead, so the next render that does not seek (a resize,
for one) shows the frame the timeline says. While a sprite is edited over the
root view the host shows the root, so saving is refused there until the sprite
is shown on its own. The live window test sets a work area of three frames,
compares each saved frame with a single save of it and checks the playhead
frame comes back; leaving out the seek, the work area's end, the black
background or the restore each fails it.

`File > Save` writes the encoded document over the opened path and `Save as...`
asks for a new one. Whether the document is unsaved comes from the history, not
from the document: the title carries `(unsaved)` while the current position in
the stack is not the position of the last save, and opening another file or
closing the window offers to save first.
