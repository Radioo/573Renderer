# The IFS editor application

`editor/` is a separate CMake project producing `ifs_editor.exe`, the Qt 6
Widgets front end for the IFS editor. It is not part of the renderer build and
nothing in `build/` depends on it.

## Why a separate project

The renderer links everything statically (`x64-windows-static`, `/MT`) so
`573Renderer.exe` ships as one file. Qt in that triplet is a very long build
and vcpkg's Qt is only routinely exercised dynamically, so the editor gets its
own manifest and its own triplet (`x64-windows`) and its own binary directory
(`build-editor/`). The two projects share source rather than targets:
`cmake/r573_shared_sources.cmake` lists the support, format and preview
sources both consume, so a file added to the renderer's format layer is picked
up by the editor with no second list to keep in step.

The editor links no renderer app, GUI or backend code. Everything it needs
from the engine happens in the preview host process (docs/preview_host.md).

## Building

```
editor\build.bat
```

Same shape as the renderer's `build.bat`: vswhere locates Visual Studio,
`vcvarsall x64` sets the toolchain up, vcpkg is bootstrapped if needed, then
`cmake --preset editor` and `cmake --build --preset editor`. The preset writes
to `build-editor/` (gitignored) and sets `VCPKG_HOST_TRIPLET` to
`x64-windows` so `flatc` is found as a host tool. Running plain `cmake` from a
shell with no Visual Studio environment fails at the compiler check with
`LNK1104: cannot open file 'kernel32.lib'`, which is what the batch file
exists to prevent.

## Qt plugin deployment

vcpkg's applocal step copies the Qt DLLs next to the executable, but not the
Qt *plugins*, and Qt aborts at startup with a message box when it cannot find
a platform plugin. vcpkg's qtbase does not install `windeployqt` (it lives in
qttools), so `editor/CMakeLists.txt` copies the two plugins the application
needs straight from their imported targets in a post-build step:
`Qt6::QWindowsIntegrationPlugin` into `platforms/` and
`Qt6::QModernWindowsStylePlugin` into `styles/`. The destination subdirectory
comes from each target's `QT_PLUGIN_TYPE` property, so the copy cannot drift
from what Qt expects to find.

## Window layout

`Editor::Window` is a `QMainWindow` hosting a Qt Advanced Docking System
`CDockManager`. The viewport is the dock manager's central widget, with the
package tree docked left, the inspector right and the timeline bottom. Qt 6
Widgets is ADR 0003; the panel arrangement is the one the editor design
settled on before implementation started.

Window geometry, `QMainWindow` state and the dock manager's own state are
saved to `QSettings` on close and restored in the constructor
(`editor/src/editor_layout.cpp`). The organisation and application names
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

**Inspector.** Selecting an item calls `Outline::Describe` and prints
`Document::Fields` as one line per field. When the selection is an animation
the window also shows it.

**Viewport.** `Editor::Viewport` paints one `QImage` scaled to fit, and emits
its new size when it is resized. The window debounces that by 120 ms, asks the
host to resize, renders, and turns the shared texture into pixels with
`SharedTexture::Reader` (docs/preview_host.md). A render that fails leaves the
last good image on screen: the error goes to the status bar every time and to a
dialog only when it is not the error already showing, so a host that dies while
the viewport is being dragged does not produce one dialog per frame.

**Timeline.** `Editor::Timeline` draws a ruler carrying the animation's labels
at their frames, then one row per depth from `Document::DepthRows`, with a bar
over each frame span and the depth number in the gutter. Clicking or dragging
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

Right-clicking the package tree offers the entry edits: add an image from a
file, replace the selected entry from a file, and remove it. The image path goes
through `QImage`, so the editor reads whatever image formats Qt was built with
rather than carrying a decoder of its own, and converts to the BGRA the package
stores. Removing a texture goes through `RemoveImage` so the texture list loses
its node too; removing anything else is a plain entry removal.

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
animation's characters it places. Last it offers the camera:
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
mark selects that keyframe, dragging it retimes it, and right-clicking a lane
offers to add a keyframe where there is none, and to remove one or change how it
leaves its frame where there is. An ease of bezier asks for its four control
points as `x1, y1, x2, y2`.

With a keyframe selected the inspector carries three rows for it above the
placement fields: which property and frame it is, its value, and the ease it
leaves on. The value row is the one editable cell an owned depth has, because
the placement rows below it are produced from the keyframes rather than edited.

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

`File > Save` writes the encoded document over the opened path and `Save as...`
asks for a new one. Whether the document is unsaved comes from the history, not
from the document: the title carries `(unsaved)` while the current position in
the stack is not the position of the last save, and opening another file or
closing the window offers to save first.
