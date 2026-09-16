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

Right-clicking the timeline offers the label edits: add one at the frame under
the cursor, and rename, move or remove the label the cursor is near. Below the
labels it offers the structure edits: insert or remove a frame at that point,
and add or remove the selected depth over a range. Last it offers the camera:
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

Every edit is recorded in a `Document::History` before it is applied, named
after the field and depth it changed, and `Edit > Undo` / `Edit > Redo` restore
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

`File > Export into the IFS` writes every owned depth's keyframes into the
document as baked data. It is one undoable step like any other edit, and it runs
the same reload loop, so the viewport shows what was exported. Export changes the
open document and not the file on disk; `File > Save` is still what writes it
out.

`File > Save` writes the encoded document over the opened path and `Save as...`
asks for a new one. Whether the document is unsaved comes from the history, not
from the document: the title carries `(unsaved)` while the current position in
the stack is not the position of the last save, and opening another file or
closing the window offers to save first.
