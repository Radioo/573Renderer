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

`build-editor/editor_widget_tests.exe` drives the timeline, the viewport, the
graph and the ease curve with synthetic mouse events, with no window shown and
no host. Its cases live in `editor/tests/editor_widget_tests.cpp`,
`editor/tests/editor_timeline_widget_tests.cpp` (dragging, snapping and dropping
on the timeline, labels and zoom) and `editor/tests/editor_graph_widget_tests.cpp`
(the graph panel), with the mouse helpers (`Send`, `Click`, `Drag`, `Wheel`),
the timeline geometry and the sample scene shared from
`editor/tests/widget_test_support.h`, with the panel tab strip's corner layout
in `editor/tests/editor_panel_tabs_widget_tests.cpp`;
its `main` sets `QT_QPA_PLATFORM=minimal` before creating the `QApplication`,
and the minimal platform plugin is deployed next to it. The cases cover
clicking, Ctrl-clicking and box-selecting keyframes, dragging a selection by
whole frames, a selection losing keyframes that are gone, stage clicks in stage
pixels, moving, scaling and turning a selection with its handles, and dragging
an ease handle past the segment's time. `tools/checks.sh` builds the editor and
runs them. Two mutations (moving the turn handle, clearing the selection on a
Ctrl press) were each seen to fail a case before the tests were trusted.

`build-editor/editor_window_tests.exe` builds the whole `Editor::Window` from the
same sources, with no host, and drives it the way a user would. The cases that
need no game install live in `editor/tests/editor_window_tests.cpp`,
`editor/tests/editor_panel_window_tests.cpp` (the panels),
`editor/tests/editor_depth_window_tests.cpp` (arranging, splitting, sequencing,
trimming the clip, timeline drops and new sprites) and
`editor/tests/editor_key_window_tests.cpp` (the keyframe edits), and the
ones that start the preview host live in
`editor/tests/editor_live_window_tests.cpp` (they skip without
`R573_IIDX_DIR`). The `Script`, its steps and the package helpers are shared
from `editor/tests/window_test_support.h`, including `OwnDroppedDot`, which
drops the dot shape on depth 3 at frame 0, makes a project and lets it own that
depth, the start of every test that edits an owned depth. A case that compares
pictures of a widget takes them through `Picture`, never `grab()` directly: the
window is never shown, so its docked layout is still at its constructed size
until something forces a pass, and the first `grab()` is that pass. It returns a
picture of the pre-layout geometry and the widget is a different size
immediately afterwards, which made a later capture of the same frame compare
unequal to the first one. `Picture` grabs, processes events and grabs again
until the widget's size stops changing. Its `main` points
`QSettings` at a temporary INI directory under its own organisation name, so it
never reads the user's game install or layout, and it asks for the minimal
platform only when `QT_QPA_PLATFORM` is not already set, so
`QT_QPA_PLATFORM=windows ./build-editor/editor_window_tests.exe` runs the same
cases against the real platform. Layout answers can differ between the two (the
minimal plugin lays a panel out where the real one leaves it stale), so a case
about geometry is worth running both ways. Each case writes a small package
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

## Nothing heavy runs on the window's thread

The window never blocks. Every piece of work that takes more than an instant
runs on a thread pool the window owns (`pool_`, a `QThreadPool` member), is
started through `Editor::Jobs::Start` (`editor/src/editor_jobs.h`, a thin
wrapper over `QtConcurrent::run` with a `QPromise` for progress and a
`QFutureWatcher` that delivers the result back on the window's thread), and
reports what it is doing while it runs. `Window::Loading` is true while any job
or a pending viewport resize is outstanding, which is also what the tests wait
on (`WaitForOpen` in `window_test_support.h`).

The jobs, and what each one shows:

- **Opening a package** (`Window::OpenDocument` → `OpenPackage`,
  `editor/src/editor_open.cpp`). The worker reads the file, parses the IFS,
  then decodes every animation and every texture to build the package rows,
  reporting `Reading <name>` per entry. The window switches to the Busy page
  the moment the open starts, so no panel is ever shown empty before its
  content is on the way. `Window::FinishOpen` moves the parsed document in and
  fills the trees from prepared rows, which is pure widget work.
- **Refreshing the package rows after a structural edit** (`Window::ReloadRows`).
  `EditDocument` compares the entry paths before and after a change and starts
  this job only when the set of entries moved, so a keyframe edit costs
  nothing. The old rows stay on screen until the new ones land.
- **Reading a clip for the timeline, the library and the inspector**
  (`Window::ShowClipTimeline` → `ReadClipView`, `editor/src/editor_clip_view.cpp`).
  This is the work that used to cost 219 ms on IIDX 33's `title.ifs` every time
  an animation was opened or an edit landed: decoding the animation, listing
  the characters, decoding a tile for each one, and walking the depth rows. The
  timeline says `Reading <name>` while its first read is in flight.
- **Loading the preview host with a package** (`Window::LoadIntoHost` →
  `Editor::LoadIntoHost`, `editor/src/editor_host_load.cpp`): the encode, the
  optional hidden-depth filtering or sprite preview, the upload and the symbol
  switch. The stage says what it is doing, and while it runs
  `Window::HostReady` is false, so seeks, renders, resizes, onion skins and
  snapshots stand aside instead of talking to a host that is mid-load. Requests
  coalesce: a new one made while a load is in flight replaces the pending one,
  which is what keeps a drag on the stage smooth. A resize asked for during a
  load re-arms its timer rather than being dropped, and a stage preview waits
  for the host instead of throwing its change away.
- **Booting the preview host** (`Window::StartHost`): the process start and the
  DLL load, which take seconds. The status bar says `Booting the preview
  host...` and the window stays usable.
- **Saving** (`Window::WriteDocument`): the encode and the write, on the Busy
  page. `Save` and `OfferToSave` take a continuation rather than returning a
  verdict, so the flows that need the save to finish first (opening another
  file, closing the window) carry on in that continuation; `closeEvent` ignores
  the first close, offers the save, and closes again once the answer is in.
- **Exporting a project** (`Window::ExportToPackage`): the export, which
  rewrites entries, on the Busy page.
- **Saving frames** (`Window::SaveFramesAs`): the loop is chunked, one frame per
  turn of the event loop, with the PNG encode of each frame on the pool. The
  Busy page shows `Frame N, x of y` and a Stop button (`Busy::StopAsked`).

The window's destructor clears and drains the pool before its members are
destroyed, because a QFutureWatcher only waits when it is destroyed, and by
then the window's own members would already be gone.

What is left on the window's thread is widget work plus two small reads: the
host render round trip (a few ms to about 100 ms for a big stage, and skipped
entirely while a load is in flight) and `Window::ShowFrame`, which decodes the
open animation to fill the inspector and the outlines. That measured 6 to 9 ms
on IIDX 33's `title.ifs`, and it does not run during playback, so it is left
uncached rather than risking a stale-document cache.

Two layout defects fell out of this work and are fixed: the selection bar and
the timeline bar put their contents in a `QScrollArea`, so their minimum widths
no longer force the whole window wider than the screen (the selection bar alone
demanded 2283 px, which pushed the stage off the edge of a 1600 px window).

## The look, and how it is checked

The window follows a design canvas (an artboard set; the link is in
`.scratch/ifs-editor-redesign/design.local.md`, which is gitignored). Matching
it is a hard requirement, and a behaviour test cannot see a wrong colour, font
or metric, so every visual change is checked by looking at the running window.

The accent is Windows's, not the artboard's. `Theme::SystemAccent` reads
`HKEY_CURRENT_USER\Software\Microsoft\Windows\DWM`: `AccentColor` is a DWORD
in `0xAABBGGRR` order, `ColorizationColor` the same colour in `0xAARRGGBB`, and
`Theme::AccentFromDwm` unpacks either, refusing pure black and pure white as
"nothing set". With no value at all the design's `#4c9dff` stands in.
`Theme::UseAccent` then derives everything that hangs off it, so one colour
from the system re-tints the whole window: `OnAccent` is whichever of the text
white and the near-black ink reads better on it, `Lifted` is the hover, tilted
away from that ink rather than always toward white, `QuietOnAccent` is the ink
blended back toward the accent as far as 5:1 allows (the `Ctrl O` chip),
`Chosen` is the page blended a quarter of the way to the accent (selected rows,
checked chips, the timeline's chosen lane) and `OnChosen` is the accent lifted
toward the text and then pushed further until it clears 4.5:1 on `Chosen`. An
accent that cannot carry either ink is darkened or lightened until it can
(`Fitted`), so a mid-grey system accent still reads. The accent is read once in
`Theme::Apply`, at startup.

`editor/src/editor_theme.{h,cpp}` holds the design's other tokens once: the colours
(page `#0c0d0f`, panel `#141518`, field `#1a1c20`, line `#282b31`, edge
`#353941`, text `#e7e8eb`, soft `#b0b5be`, faint `#8a909b`, the ink `#06121f`,
amber `#f2b84b`, green `#3ecf8e`), the families (Segoe UI Variable Text, Cascadia Mono for
anything numeric or key-capped) and the heights (44 top bar, 24 status bar, 28
controls, 30 panel tab strip). `Theme::Apply` sets the application font, a full
`QPalette` and one global stylesheet; `Theme::DockStyle` is the separate sheet
the dock manager needs. Panels key off object names and properties
(`QToolButton[transport="true"]`, `QWidget[section="true"]`, ...) rather than
inline styles, so a colour exists in one place.

The icons are Lucide, vendored as SVG files under `editor/icons/` from
`lucide-static` 1.47.0 with its ISC licence beside them
(`editor/icons/LICENSE`), compiled into the binary by `editor/icons/icons.qrc`
and `CMAKE_AUTORCC`. `editor/src/editor_icons.{h,cpp}` maps each `Glyph` to a
file name; `Drawn` renders `:/icons/<name>.svg` into a pixmap at a side, then
fills it through `CompositionMode_SourceIn` so the whole glyph takes the asked
colour whatever the file strokes or fills; `Of` makes a QIcon with a 1x and a
2x pixmap, and `Toggling` adds an On pixmap in the chosen colour for a
checkable button. `qtsvg` is in `vcpkg.json`'s `editor` feature and the
`qsvg`/`qsvgicon` plugins are deployed like the others. Nothing is
hand-painted.

`icons.qrc` belongs to every target that draws, the test binaries included: a
target without it renders each icon as an empty pixmap and says nothing.
`Every glyph draws its file...` in
`editor/tests/editor_icons_widget_tests.cpp` walks the whole `Glyph` enum at 14
and 32 px and checks each one inks pixels and that every solid pixel carries
the asked colour, which catches a missing resource, a misspelled file name and
a tint that did not apply. It guards `editor_widget_tests`;
`editor_window_tests` is guarded by the search hover case, which fails when the
glyph stops changing colour. Adding `icons/icons.qrc` to `editor_window_tests`
is how that case went green again.

They used to be path data typed into `Icons::Body`, transcribed from the
artboards, and the artboards' own icons are hand-drawn approximations of this
same set. Several were wrong. Undo was
`<path d="M9 7L4 12l5 5"/><path d="M4 7h10a6 6 0 0 1 0 12h-2"/>`: the arrow
head's vertex sits at y 12 while the tail it belongs to runs at y 7, so the
head floated five pixels under the curve and the pair read as a detached loop
rather than an arrow. Lucide's `undo-2` is
`<path d="M9 14 4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 5.5 5.5a5.5 5.5 0 0 1-5.5 5.5H11"/>`,
where both start at (4, 9). **Do not type path data for a new icon.** Fetch the
file (`https://unpkg.com/lucide-static@1.47.0/icons/<name>.svg`), put it in
`editor/icons/`, add it to `icons.qrc` and name it from `Named`.

Two places where Lucide differs from the artboards on purpose: its `play` is an
outlined triangle where the artboard filled it, and `previous change` /
`next change` keep the `diamond` the artboards use as the label for that pair,
so the two buttons look alike. The artboards draw them directionally, with the
same shapes as previous and next frame, which is ambiguous in the other
direction. Neither is settled.

`build-editor/ifs_editor_shot.exe` (`editor/src/editor_shot_main.cpp`) is how
the window is looked at:

```bash
./build-editor/ifs_editor_shot.exe --out screenshots --name start
```

It builds the real `Editor::Window` with the theme applied, opens it on the
native Windows platform so the shot shows what the user will see, and writes a
PNG into the given directory (`screenshots/` is gitignored). The window is a
`Qt::Tool` with `WindowDoesNotAcceptFocus` and `WA_ShowWithoutActivating`
moved to -32000, so it never takes focus or interrupts anything on screen, and
it clears its own `QSettings` first so the shot is the same every time.
Options: `--ifs <file>` opens a package, `--animation <name>` picks that
animation out of the Animations list the way a click does (without it the shot
shows the first one), `--clip N` descends into a clip of it by its index in
`Document::Clips`, 0 being the root, which is the breadcrumb the stage bar
draws, `--depth N` and `--frame N` choose one
(so the inspector, the selection bar and the timeline have something to show),
`--game <dir>` points at an install, `--hover <objectName>` puts the named
widget under the pointer, `--report <file>` writes a text line per named widget
(object name, class, position in the window, size, size hint, font size and
whether it is shown) next to the PNG, `--size WxH` (default 1600x1000, the
artboard's size), `--platform <name>` and `--keep-settings`.

`--report` is how a layout question gets an answer instead of a guess. Reading
a screenshot tells you two things overlap; the report tells you the label sits
at x 248 with width 5 while its hint is 34, which names the defect. Use it
whenever a widget looks wrong in a shot, and quote its numbers rather than
estimating them from pixels.

`--hover` goes through `Editor::Hover` (`editor_hover.h`), which the window
tests use too. Faking a hover needs more than `WA_UnderMouse`: a `QPushButton`
only keeps `State_MouseOver` when `QPushButtonPrivate::hovering` agrees, and
that flag is set solely by `QPushButton::mouseMoveEvent` with `WA_Hover` on
(`qpushbutton.cpp`, `styleButtonState`). So `Hover` sets `WA_Hover` and
`WA_UnderMouse`, sends `Enter`, then sends a `MouseMove` at the widget's
middle; `Unhover` clears the attribute and sends `Leave`. Setting the
attribute alone leaves the button painting its resting rule and a shot that
proves nothing.

Colours are not left to the eye either: `Theme::Contrast` is the WCAG
relative-luminance ratio, `Theme::kLeastContrast` is 4.5:1, and the cases in
`editor/tests/editor_contrast_window_tests.cpp` fail when any word the editor
draws lands under it. They grab the real window and measure the painted
pixels, so a stylesheet's `:checked` colour and a delegate's own painting are
both covered; see docs/gates.md for how the sampling works and what it caught.

Known deviations from the artboards, all deliberate:

- The library is a list of rows with thumbnails rather than the artboard's grid
  of cards. The rows carry the same information; the grid would mean replacing
  the tree the drag and drop, the filters and the tests are built on.
- The stage bar has no "outlines of the chosen depths" toggle and the selection
  bar keeps one button per command instead of the artboard's Flip, Arrange and
  Align menus: there is no such command, and grouping would take the popover
  anchors (`bar_<id>`) the keyframe popovers hang off.
- The timeline header has no "+ Depth" button. The header is painted inside the
  scrolling timeline widget, so a child button there would scroll away; the
  command stays in the Depth menu.
- The preview card on the start screen has no "Restart host" button because
  there is no such command.

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

`Editor::Window` is a `QMainWindow` whose central widget is a two-page
`QStackedWidget` (`centre_stack`): the start screen, and the Qt Advanced
Docking System `CDockManager` with every panel. Opening a document turns the
page, so the welcome fills the window and the panels appear only when there is
something to show.

Inside the dock manager the stage is the central widget, the package and the
library are docked left of it one above the other, the timeline (with the graph
as a second tab) is docked at the bottom of the whole container so it runs
under the stage and the left panels alike, and the inspector (with the history
as a second tab) is docked right at the full height of the window, which stops
the timeline at its edge. The order `BuildPanels` adds them in is what gives
that shape: the package and the library beside the stage first, then the
timeline at the container's bottom, then the inspector at the container's
right, so the last split is the one that spans the whole height.
`View > Panels` has a toggle for each panel (the dock's own toggle action), so
a panel closed from a menu can be opened again, and `View > Show the history`
brings the History tab to the front. Qt 6 Widgets is ADR 0003.

The dock chrome is the design's, not ADS's default: config flags (set before
the manager is built, because they are static) take away the close, undock and
tab-menu buttons and the per-tab close button, and hide the title bar of the
single central widget. The package and the library areas each set
`HideSingleWidgetTitleBar`, because their own 30 px tab strip is the design's
panel header; the timeline and the inspector keep theirs, since Timeline/Graph
and Inspector/History are exactly the tabs the design draws there.
`Theme::DockStyle` gives those tabs the panel colours and the 2 px accent
underline.

The default sizes are set once the panels exist, with
`CDockManager::setSplitterSizes` on each of the four splitters ADS builds:
package against library, the left column against the stage, the stage against
the timeline, and everything against the inspector. That call only lands when
the count of sizes matches the splitter's child count, so each one names the
area whose parent splitter holds exactly those two children. The left column
was the one nobody sized for a while, which left its width to whatever ADS
worked out from size hints. The panels also carry their own minimum sizes
(220 px for the package and the library, 260 px for the inspector, 180 px high
for the timing area), so a restored layout can never squeeze them down to a
strip of icons.

`SaveLayout` and `RestoreLayout` (`editor_layout.cpp`) keep that layout in
`QSettings` between runs, and both refuse a dead one:

- **Saving** writes the dock state only when the dock manager is visible. It
  lives on a page of `centre_stack`, and `QStackedLayout` gives geometry to the
  current page alone, so a window closed while the start screen or the busy
  page was up has never laid its splitters out. Its state is a set of zeroes.
  The window geometry and `QMainWindow` state are still written, since those
  are real whatever page is up.
- **Restoring** checks the stored state with `LayoutSized` before handing it to
  ADS: it uncompresses the XML (ADS compresses it by default) and reads every
  `<Sizes>` list. A list that is all zeroes means that splitter was never laid
  out, and `QSplitter::setSizes` with zeroes collapses each panel to its
  minimum and gives the rest to the stage. One such list is enough to throw the
  whole state away and keep the built-in defaults.

That is what a user saw as "the default layout": the package and the library at
60 px, the inspector clipped off the right edge and the timeline reduced to its
transport bar, all of it restored from a `window/docks` value whose every
`<Sizes>` read `0 0`. `A layout saved before a package was opened...` in
`editor/tests/editor_shell_window_tests.cpp` closes a window that never showed
its docks and checks that nothing was stored, then opens a package and measures
each panel; `A stored layout whose splitters are all zero...` pins `LayoutSized`
itself.

ADS takes a tab that is not in front out of the window's widget tree, so a test
looks a panel up through the dock manager (`Panel` in `window_test_support.h`,
which calls `CDockManager::findDockWidget`) rather than with `findChild`. A
test that measures where the panels are has to open a document first, because
the dock manager has no laid-out geometry while the start screen is the page in
front.

Window geometry, `QMainWindow` state and the dock manager's own state are
saved to `QSettings` on close and restored in the constructor
(`editor/src/editor_layout.cpp`). The dock state is saved and restored with a
layout version (`kDocksVersion`), and the dock manager ignores a saved state of
another version, so a layout saved before a panel existed falls back to the
default arrangement instead of restoring without the new panel. Raise the
version whenever the set or the default arrangement of panels changes; it went
to 1 with the library, 2 with the history, 3 with the graph, 4 with the
redesigned arrangement and 5 when the timeline moved under the left panels. The
organisation and application names `QSettings` keys off are set in `main` before
the window exists.

### Top bar

A fixed tool bar (`top_bar`, `editor_shell.cpp`) is the window's only visible
chrome above the panels. The menu bar is built as usual but hidden
(`menuBar()->setVisible(false)`); every top-level menu is also opened by a flat
button on the bar (`menu_button`), so the menus stay a full index of the
commands and the tests that walk `menuBar()->actions()` still see them. Left to
right: the `IFS` logo chip, the menu buttons, a divider, undo, redo and the
history button (SVG icons from the design), then the command search field
(`search`: a button with the search glyph, the placeholder and a `Ctrl K` chip,
centred in what the bar has left), then the document group and the Save button.

Under the pointer the search field lifts its background from `kField` to
`kLine` and `SearchHover` (`editor_shell.cpp`) repaints the glyph and the
placeholder from `kFaint` to `kSoft`. The border stays `kEdge` on both the
field and the chip: brightening only the field's border left two borders of
different colours a few pixels apart at the right end, which is what the
design never shows. The glyph and the placeholder are not stylable from the
hover rule, because a Qt style sheet does not track an ancestor's
pseudo-state: `QPushButton#search:hover QLabel#search_text` paints the label
whether or not the field is hovered, so both live in the event filter
instead. `Hovering the command search...` in
`editor/tests/editor_shell_window_tests.cpp` grabs the field quiet, hovers it,
and checks that the picture changed, that the field's border matches the
chip's, and that leaving puts both back.

The document group (`trailing`) holds the file glyph, the open file's name
(`document_state`) and how far it is from its saved state (`document_edits`:
"saved" in faint, "unsaved, 1 edit" or "unsaved, 3 edits" in amber once it has
fallen off the undo stack; the count is `Document::History::StepsFromSaved`).
With a project open the project chip (`project`, a menu with showing the
project folder, export and closing the project) and the Export button
(`export`) join it. The Export button's count is `Document::AwaitingExport`:
the IFS entries the project writes whose bytes no longer match what the last
export recorded, plus the project images the IFS does not hold yet. Every
keyframe edit is written into the open document straight away, so the count is
not "edits the IFS is missing"; it is the entries the drift check at the next
opening would report if the file were saved without exporting. The Save button
carries the `file.save` command. With no document open the whole group and Save
are hidden, which is the start screen's bar in the design. Everything on the
bar runs through the command registry, so it greys out and refuses the same way
the menus do. `RefreshTopBar` runs from `RefreshState`, which every edit, undo,
redo and save goes through.

### Status bar

The status bar is 24 px and splits in two (`editor_shell.cpp`). On the left,
next to a dot that is green while the preview host runs and edge grey when it
does not, sit the host (`host_named` "Preview host" and `host_status`: "not
running", or "ready, N ms a frame" with the time the last render took), the
stage size and frame rate of the open animation (`stage_status`, from
`Document::StageSizeOf` and `Document::FrameRate`) and the frame
(`frame_status`, "Frame 2 of 3"). On the right sit what is chosen
(`chosen_status`: a depth, several depths, or selected keyframes), snapping
(`snap_status`) and the stage zoom (`zoom_status`, the shown stage width over
the stage's own width, `Viewport::StageScale`). `RefreshStatus` is cheap enough
for every rendered frame of playback because it never reads the animation; the
stage section is filled from `ShowFrame`, which reads it anyway.

Temporary messages (refusals, results of an edit) take the whole left side:
the group hides itself while one is shown (`QStatusBar::messageChanged`), so
the two never overlap, and the messages that are not a refusal carry a timeout
so the host and frame sections come back. A boot clears its own message
instead: "Preview host running on ..." would only repeat what the left group
already says.

## Commands

Every command the editor offers is registered once in `Editor::Commands`
(`editor/src/editor_commands.cpp`, object name `commands`) with a stable id
(`depth.split`, `clip.trim`, `key.wiggle`, ...), its menu text, its shortcut and
a refusal check. The refusal check returns nothing when the command can run and
one sentence saying what it needs otherwise ("Choose a depth first", "Select
two or more keyframes first", "The project owns depth 3 here. Detach it before
splitting it."). The menus File, Edit, Depth, Keyframe, Clip, View and Playback
are built from the registry (`editor_menus.cpp`, `editor_depth_menu.cpp`), so
every command is in a menu and a menu is a full index of what the editor can do.

Running a command, by its menu item, its shortcut or `Commands::Run`, asks the
refusal check first. A refused command changes nothing and its reason goes to
`Window::ShowRefusal`, which puts it in the status bar. When a menu opens, each
of its commands is greyed while it is refused, with the reason as its tooltip,
and put back as it was when the menu closes, so a shortcut is never left dead
by a menu that happened to be opened while its command was refused. Undo and
Redo are the exception that is also disabled outside menus, since
`RefreshState` keeps them in step with the history.

The refusal checks live in `editor_refusals.cpp` and the command bodies use the
same functions, so a refusal reached from a context menu (which acts on the
frame under the cursor rather than the playhead) reads the same as the greyed
reason. Refusals only the document can make, such as a span that already starts
on the frame being split, are still found by running the edit and reported in a
dialog. Commands scoped to the timeline (copy, cut, paste, delete, select every
keyframe and the keyframe commands) keep a widget shortcut on the timeline, so
Ctrl+C in a text field is left to the text field.

`editor_window_tests` reaches commands the same way: `CommandsOf`, `RunCommand`,
`RefusalOf` and `CommandFor` in `window_test_support.h` run a command by id or
shortcut and return its refusal or the dialogs it raised. The command cases
check that every command is in a menu, that no two commands share a shortcut,
that every shortcut the editor had before the registry still runs a command,
and that a refused command greys out with its reason and leaves the undo
history alone.

### Notices

`Editor::Notices` (`editor_notices.cpp`) is a strip between the stage and the
selection bar. It stays hidden until there is something to say, then shows up to
three notices, each with a Close button, an Undo button when the thing it
reports can be undone, and a timer that takes it away after eight seconds.
`Window::ShowRefusal` sends every refused command there, and `Window::ShowResult`
sends what an edit did beyond the obvious: how many keyframes a simplify
removed, how many clips a trim, extract or lift left restarting, what owning a
depth captured, what an export wrote, how many unused definitions went. Undo on
a notice runs the `edit.undo` command, so it undoes exactly one step, the one the
notice is about.

The status bar keeps what is about the session rather than the edit: the preview
host, opening and saving, playback, the project, and errors.

### The selection bar

Under the stage sits `Editor::SelectionBar` (`editor_selection_bar.cpp`), 40 px
with a thumbnail, what is chosen in two lines (`selection_summary` "Depth 9"
over `selection_detail` "baked, frame 300"), and that selection's commands as
buttons: fit, centre anchor, flip, arrange, split, duplicate, group, keyframe
or detach for a depth; align, spread and sequence for several; hold, easy ease,
reverse, stretch, wiggle, simplify and copy for keyframes; and nothing at all
for "Nothing chosen", which also hides the thumbnail and the divider. Anything
that removes (`.remove`, `.delete`) is a trash icon pushed to the right end,
away from the rest. Every button is a command id: its label is the registry's
short name (`Command::brief`, which falls back to the menu text), it is
disabled while the command is refused with the reason as its tooltip, its
shortcut is appended to that tooltip, and clicking it runs the command through
the registry, so the bar can never do something a menu would not.
`Window::RefreshSelectionBar` fills it from the same state the status bar reads,
on every `ShowFrame`.

### Popovers instead of questions

Wiggle, simplify, time-stretch and group into a sprite used to ask for their
numbers through a chain of `QInputDialog`s. They now open `Editor::Popover`
(`editor_popover.cpp`) beside the selection bar button that started them: a
title, one spin box per number with the default already filled in, an optional
line of detail, and Cancel and an apply button named after the command.

While a popover is open the values are previewed on the stage. The preview goes
through `Window::PreviewAuthored`, which applies the change to a copy of the
owned depth, bakes it into a copy of the animation and renders that copy through
the host (`PreviewOnStage`), so nothing touches the document or the undo
history. Cancelling, or pressing Escape, calls `Window::CancelPreview`, which
reloads the real document and puts the keyframe selection back. Applying runs
the same edit the menu item used to, as one undo step.

A popover is reused for every command, so `Ask` empties its rows and its button
row first. The buttons are deleted through `qDeleteAll` on a copied list of the
direct children, because deleting them while walking `QObject::children()` walks
a list that each deletion edits.

Each one keeps what it needs: wiggle draws its seed once when the popover opens,
so the preview and the applied wiggle are the same jitter; simplify's detail
line says how many of the selected keyframes would go at the tolerance shown
(`Window::KeysAfterSimplify`, which runs the document's own simplify on a copy);
group into a sprite starts from the chosen depths and the work area, or the
widest span under the playhead when there is no work area, and its fields are
there to adjust that rather than to be typed from nothing.

### Command search

`Edit > Search commands, depths and animations` (Ctrl+K), or the search field in
the middle of the top bar, opens `Editor::CommandSearch` (`editor_search.cpp`), a
popup over the window with a text box and a result list. The window fills it
each time it opens (`Window::OpenSearch`): every registered command with the
menu it belongs to, its text and its shortcut, the depths of the clip on screen
("Depth 4, splash", with the frames its spans cover), and the animations of the
package. A command that is refused right now stays in the list, greyed, with
its refusal under its name, and cannot be run from there; a toggle shows
whether it is on. Typing keeps the results that hold every typed word,
ignoring case, anywhere in the menu name and the text; this is a filter, not a
fuzzy match, which is what a list of a hundred named commands needs. The
arrow keys move to the next result that can run, Enter runs it after closing
the popup (so a command that opens a dialog opens it over the window, not over
the popup), and Escape closes it. Going to a depth seeks to its first frame
when it shows nothing on the playhead and then chooses it; opening an animation
selects it in the package tree, as a click there does. The widget tests drive
the filter, the greyed result, the arrows skipping it, Enter and Escape; the
window tests search for a refused split, run it once a depth is chosen, go to a
depth and find an animation.

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
shows it on its own, as entering it from the timeline does; the switch is posted to
the event loop because it refills the library, which would otherwise delete the
item while its own double-click signal is still being delivered. Its menu places
the character on a new depth from the playhead, asking for the depth (the first
free one is suggested) and the last frame the way the timeline's add depth does
(`Window::AskForLastFrame`, `Window::AddCharacterDepth`, both shared with it).
On a sprite it also offers `Duplicate this sprite` (`Document::DuplicateSprite`,
one undo step, refilling the clip list so the copy can be edited on its own), and
with a depth selected, `Use on depth N from frame F`, which sets the character
of the placement live on that frame through the same field edit the inspector's
Character row uses, as After Effects replaces a layer's source. Duplicating a
sprite and using the copy on a depth is how a variant is made without touching
the other places the original is shown. The menu always offers `New empty
sprite...`, with or without a character under the cursor, as After Effects
offers New Composition: it asks for a frame count (the current clip's is
suggested), defines an empty sprite of that many frames (`Document::NewSprite`,
one undo step), refills the clip list and opens the new sprite
(`Window::ShowLibrarySprite`, as a double-click does), so depths can be added to
it straight away, for one by dropping a character on the timeline. The window
test makes a five frame sprite from a library with nothing selected, sees it in
the crumb and the library, drops a character into it on its fifth frame, and
sees the entry offered with an item selected too; dropping the refill, the
opening, the entry or its handling fails it.
The package tree and the library are named (`package`, `library`) so a test can
tell the two trees apart.

Both trees have a search box above them (`editor_filter.cpp`), as the After
Effects Project panel does. Typing hides every row whose name does not contain
the text, ignoring case; a folder stays shown while something inside it
matches, and everything inside a matching folder stays shown, so searching
`tex` keeps the whole texture folder. The boxes are named `package_filter` and
`library_filter`. A refill builds every row again shown, so `FillTree` and
`FillLibrary` apply the current text once they finish; without that, the first
edit after a search would bring every row back while the box still held the
text. `editor_widget_tests` checks the matching rules on a small tree, and the
window test searches both trees, drops a character and duplicates an animation
(the two refills) and sees the search still applied; dropping either reapply,
the case folding, the folder rule or the matching folder rule fails one of them.

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

A character can be dropped on the timeline too, as footage is dropped into an
After Effects timeline at a time. The timeline takes the same format over its
frame area (not the gutter, and not before an animation is shown) and reports
the character, the frame under the cursor and, when it lands on a depth row,
that depth (`Timeline::CharacterDropped`). The window starts a depth showing
the character from that frame to the clip's last
(`Window::DropCharacterOnTimeline`, through `AddCharacterDepth`, one undo
step), on the row's own depth when that depth is free for the whole stretch
(`Document::CheckFree`) and on the first depth above every depth the clip uses
otherwise. Unlike a stage drop it places the character at the stage origin,
since the drop says when and not where. The widget test drops on a depth row,
on a property lane, on the gutter, with another format, with a payload that is
not a number, and on a timeline with nothing shown; the window test drops on a
taken row and sees a new depth start on the dropped frame, then frees a row
and sees the drop land on it. Dropping the frame, the gutter or empty-timeline
rule, the number check, the lane rule, the free-row check, the fallback or the
connection fails them.

**History.** Every step the undo stack holds, under a first row, `Start`, which
is the oldest document the stack can still reach (32 steps back at most), then
each edit by the name the Edit menu gives it. The row for where the document is
now is selected and the steps after it, which redo would bring back, are
greyed. Clicking a row jumps there in one go (`Window::JumpInHistory`,
`Document::History::Jump`) and shows the result once, instead of once per
step. The list is refilled with the Edit menu's labels (`RefreshState`), so it
follows every edit, undo, redo and jump; the jump is posted to the event loop
because refilling deletes the clicked row.

**Inspector.** `Editor::Inspector` (`editor_inspector.cpp`) is a panel, not a
table. At the top it names what is chosen (the depth, the character it shows and
the frames of its span) and whether that depth is BAKED or KEYED. Under that are
the Transform and Appearance sections, built from `Document::ViewPlacement`
(docs/document.md): Position and Anchor in stage pixels, Scale in percent,
Rotation and Skew in degrees, the multiply and add colours as a swatch, a hex
value and an alpha. Each row carries a mark saying whether this frame set that
group or an earlier one did, with the frame in its tooltip, and each value box is
a spin box that takes a typed number; the X and Y captions beside a pair are
`ScrubLabel`s, so dragging one changes that number. Committing a box emits
`ValueEdited`, which `Window::ApplyViewEdit` applies through
`Document::SetViewedOwned` for a depth the project owns and
`Document::SetViewedBaked` otherwise, as one undo step named after the row.
Clicking a colour swatch opens Qt's colour dialog on the colour shown and writes
the choice back the same way.

Rebuilding those rows deletes the widgets that were showing them, and one of
them is usually the spin box whose `editingFinished` started the edit, so the
rows are taken out of the layout, hidden and then `deleteLater`ed. Deleting them
outright freed a widget while its own key handler was still running.

An owned depth's rows also carry a keying button: a plus to start animating that
value (`Document::AddTrack` on the track `Document::ViewTrack` names), a hollow
mark when the value is animated but not keyed on this frame, and a filled one
when it is, which removes that keyframe. The track a row keys is the one the
depth already animates, so Rotation and Skew key the rotate skew track a turn
writes.

Every field the format holds is still there: `Raw placement fields` at the
bottom of the panel is the old two-column table (`raw_fields`), collapsed by
default, with the same rows, the same `Document::InspectFrame` model and the
same edit targets, so animation settings, cameras, library call arguments,
keyframe values and filters are edited exactly as before, and the unknown data
is still shown and protected. Selecting an entry in the package tree prints
`Document::Fields` into that table as it always did.


**Start screen.** With no IFS open the window's central widget shows
`Editor::StartScreen` instead of the whole dock manager (both are pages of one
`centre_stack` `QStackedWidget`), so the welcome fills everything between the
top bar and the status bar the way the design has it. It is three columns:
"IFS editor" over the accent Open IFS button (with its `Ctrl O` chip) and Open
project, then a dashed drop zone; the recent files; and a 320 px preview card.
The buttons run the same commands the File menu runs. A recent row
(`recent_row`, 56 px, a thumbnail, the file name, its folder and animation
count, an amber project badge when a project folder sits beside it, and how
long ago it was touched) opens the IFS, or the project folder if that is what
was remembered. `Window::RememberRecent` keeps the last eight paths in
`recent/files`, newest first, with each one's animation count in
`recent/animations` taken from the package as it is opened, so the start screen
never parses a package to draw itself. `Window::RefreshStartScreen` reads them
back, drops what is no longer on disk, and marks the ones that have a project
folder beside them; a path remembered before the count was kept simply shows no
count. With nothing remembered the list is a single "Nothing opened yet"
(`recent_empty`). The card says where the game install is (`start_install`),
whether the preview is running (a green or grey dot and `start_running`), how
the build is named (`start_build`), and carries Change install and the line
that without an install everything except the picture still works. Packages
have no thumbnail on this screen: the picture of an animation comes from the
preview host, which is not up while the start screen is, so the rows and the
card use the design's stripe placeholder (`Rows::Stripes`).

The window takes drops anywhere (`Window::dropEvent`): an `.ifs` file opens as
a package, a folder opens as a project, and anything else is refused by name.

**Library.** Each character is a row with a thumbnail of what it draws
(`Window::LibraryTile` decodes the image the shape uses through
`Document::File::ShapeImages` and `ReadImage`; a sprite has no picture of its
own so it has no tile), its name and how many depths use it, greyed when
nothing does. Above the list, four buttons narrow it by kind: Sprites, Images,
Shapes and Imported. `Window::ShowLibraryKinds` applies the text filter first
and then hides whatever kind is switched off, so the two filters stack instead
of fighting. Return on a chosen character places it on the next free depth from
the playhead to the end of the clip (`Window::PlaceLibraryCharacter`), the same
edit the Place menu item makes. A character dragged onto the inspector's
Character row replaces what the chosen depth places
(`Editor::CharacterDrop`, `Window::UseCharacterOnDepth`). `New empty sprite`
no longer asks how many frames to make: it makes one as long as the clip it was
asked from.

**Package panel.** The panel is three tabs over the same package
(`Window::BuildPackageTabs`), drawn as the design's 30 px strip: an
`Editor::PanelTabs` whose `PanelTabBar` paints each tab's name with its count
beside it in faint 11 px and a 2 px accent underline under the current one
(`PanelTabs::ShowCount`, filled as the rows are), and a corner widget with the
plus (a new animation, or an image from a file on the Images tab) and the
`...` that opens the package menu. Each tab is a filter field with the search
glyph over the list (`WithFilter`).

A corner widget that changes width after the panel has been laid out needs
`PanelTabs::Relayout`. `QTabWidget` gives the corner its geometry from
`setUpLayout`, which is private and runs on a resize, a tab change or a
`QEvent::LayoutRequest` sent to the tab widget, never because the corner's own
size hint grew. Qt would normally post that request through the corner's
layout, but `QLayout::update` posts only while the layout is `activated`, and a
layout invalidated while its widget was hidden stays quiet, which is exactly
what a panel on a hidden page of `centre_stack` does. So `Relayout` activates
the corner's layout and sends the tab widget a `LayoutRequest` itself. The
library calls it after writing the animation name, since that name is what
changes width. Without it the corner kept the width it had when the label was
empty, and the plus button was laid out on top of the name: `Library`, then
`of title` with the plus drawn through its last letter. `A corner widget that
grows after the panel is laid out...` in
`editor/tests/editor_panel_tabs_widget_tests.cpp` is that case, and it needs no
window or game install.

Animations lists every animation as a 40 px row: a thumbnail, the name, and
"1440 frames, 1280 x 720, 60 fps" under it, and ends with a `+ New animation`
row that makes one when it is double-clicked. Images lists the package's
textures the same way, with a thumbnail decoded through
`Document::File::ReadImage` and its size under the name; an image the decoder
cannot read keeps its row and loses only the size and the picture. Both are
`QTreeWidget`s with their header hidden and every column but the first hidden,
painted by `Rows::Delegate` (`editor/src/editor_rows.cpp`), which draws the
decoration pixmap, the name, and `Rows::kDetailRole` under it. The hidden
columns still carry the frame count, stage size and rate as text, which is what
the panel tests read. Files is the raw entry tree with its filter box,
unchanged, and still the one named `package`. Every row of every tab carries
the same entry path, so choosing one selects it in the Files tree
(`Window::ChooseEntryFrom` through `Window::SelectEntry`) and the same context
menu opens over it.

An animation row is renamed by typing over it: `Window::RenameEntryRow` sends
the typed name to `Window::ApplyAnimationRename`, the half of
`Window::RenameAnimationEntry` that does the work, so the dialog and the inline
edit take the same path, including the refusal when the project owns depths in
that animation, after which the row is filled again from the package. Image
rows are not editable, because an image has no rename of its own.

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

**Stage bar and tool strip.** `Editor::StageBar` (36 px) sits above the
viewport and `Editor::ToolStrip` (40 px) down its left edge. Both are views
over the registry built after `BuildMenus` has registered the commands, so a
button is a `QToolButton` that runs a command id and, for the checkable ones,
follows its action's `toggled`; a checkable one carries a `Toggling` icon, so
it turns `#9cc8ff` on the chosen blue when it is on. The bar carries the clip
breadcrumb on the left (the file, the animation and each clip entered, each a
button that opens that clip, the last one in text white and semibold, rebuilt
only when the crumbs change so seeking does not churn the layout), then the
overlay toggles as 28 px icon buttons (rulers, snap, onion, path, background),
a divider, the zoom chip (`stage_zoom`: the current percentage in the mono font
with a chevron, opening a menu with zoom in, zoom out and fit), then Fit and
the save-the-frame icon. Zoom in and out (Ctrl+- and Ctrl++) step
`Viewport::ZoomStep` by the same 1.25 a wheel notch uses, around the middle of
the view rather than the pointer.

The strip holds the five tools, one at a time: Select (V), Anchor (A), Pan (H),
Zoom (Z) and Motion sketch (Y). They are ordinary commands made checkable and
put in one exclusive `QActionGroup`, and each one's `run` sets the viewport's
tool and checks its own action, so choosing a tool from the menu, the strip,
its shortcut or the command search all end in the same place and running the
chosen tool again is not an unchoosing. Select is what the editor has always
done. Anchor makes every drag on the chosen depth an anchor move, so the cross
does not have to be hit. Pan drags the stage the way the middle button does,
and Zoom clicks in where it is clicked, out with Alt, both through the same
`Viewport::ZoomBy` the wheel uses. Holding Space pans whatever the tool is: the
viewport accepts the shortcut override for Space so the key reaches it rather
than the play command, and lets go with `PlayAsked` when the hold moved
nothing, so a tap on the stage still plays and a hold still pans. The cursor
says which of these is live.

`View > Onion skin` (remembered, off by default) draws the frames either side
of the one shown over it at 35% opacity while playback is stopped, as Flash
and After Effects do, so a pose can be judged against its neighbours. After
each render the window asks the host for the previous and next frames of what
it has loaded (`Window::ShowGhostsAround`), seeks it back to the frame shown,
and hands the two pictures to the viewport (`Viewport::ShowGhosts`); a new
frame clears them, so a ghost never outlives the frame it belonged to. Playback
skips them, since every step would cost two more renders, and so does a window
with no animation open: `ShowGhostsAround` returns at once while
`animation_name_` is empty, because the host cannot seek what it has not
loaded. Without that guard, starting the editor with a game install set and
onion skin remembered on put "Seek failed: seeking failed" on screen before
anything was open: the boot renders one frame through `DrawBackground`, and
that render asked for the ghosts, whose seek back the host refused. The live
test "Starting with onion skin on and no file open says nothing about seeking"
builds exactly that window and fails on the message box. The live window test
turns it on at frame 401, sees the picture change, and checks a later render
that does not seek still shows frame 401; it also sees the picture come back
when onion skin is turned off. Leaving out the seek back, the ghost renders,
the ghost painting or the clearing on a new frame fails the tests.

`View > Motion path` (remembered, on by default) draws the selected depth's
motion path over the stage, as After Effects draws a selected layer's position
path: a line through the anchor's place on every frame of the span under the
playhead, a dot per frame, and a box on each frame where the project keeps a
`Translation` keyframe (`Window::PathOfDepth`, `Document::MotionPath`,
`Viewport::ShowPath`). It is refreshed with the outlines on every
`ShowFrame`, so it follows edits, seeks and the selection. There is no path for
a depth hidden in the view, or when the viewport is not showing the clip being
edited (a sprite the host could not show on its own), since its points would
be in the sprite's space. The widget test draws a path and checks the line, the
box on a keyed point and none on a plain one. The live window test picks the
widest depth of `title` at frame 400 and sees the picture change when the path
is switched off and come back when it is switched on, sees no change once that
depth is hidden, and sees the path drawn in a sprite shown on its own. Removing
the drawing, the line, the keyed check, the refresh on toggling, the setting's
default, the hidden check or the clip being edited fails them. The guard for a
sprite not shown on its own is not covered, because the live host always shows
a picked sprite on its own and there is no picture without the host.

Clicking the viewport maps the point to stage pixels through that rectangle
and selects the highest depth whose `Document::StageOutlines` outline
holds it, or clears the selection; the selected depth is outlined. A drag that
starts where no outline is, and so not on the selection, draws a dashed marquee
instead, and letting go chooses every depth whose outline it touches
(`Viewport::DepthsBanded`, `Document::DepthsTouching`, then
`Window::ChooseDepths`, as choosing several depths on the timeline does), so
they move, align and arrange together. Hidden and locked depths have no outline
on the stage, so a marquee cannot catch them. A press and release without a
drag stays a click. The widget test draws marquees over one outline and over
both, sees the dashed band drawn while dragging, a tiny drag reporting nothing,
and a drag from inside the selection moving it; the window test sees a marquee
choose two depths. Starting a band where there is a selection, dropping the
drag threshold, the band following the pointer, the drawing, or the connection
fails them. Dragging
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

`Depth > Centre the anchor in the content` (Ctrl+Alt+Home, After Effects' Center
Anchor Point in Layer Content) moves the chosen depth's anchor to the centre of
what it shows on the playhead's frame, over the whole span, without moving it
on screen (`Window::CentreChosenAnchor`, through `Document::CentreAnchor`), as
one undo step. It reads the shape boxes from the file each time rather than
from the outline cache, which is only refreshed while outlines are shown. A
depth the project owns is refused: its keyframes have no origin track, so the
next export would write the old anchor back. The window test centres the dot's
anchor, reads the new origin (the centre of its 2x1 quad) and the changed
translation, is refused a second time, undoes, and is refused on an owned
depth; dropping the owned check or the menu entry fails it.

`Depth > Fit to the stage` (Ctrl+Alt+F), `Fit to the stage's width`
(Ctrl+Alt+Shift+H) and `Fit to the stage's height` (Ctrl+Alt+Shift+G), After
Effects' Fit to Comp and its width and height forms, scale the chosen depth
about its anchor so its box fills the stage (or one side of it, keeping the
aspect) and centre it, on the playhead's frame, as one undo step
(`Window::FitChosenToStage`, through `Document::FitToStage`). An owned depth
takes the change through `ReshapeOwnedDepth` and `MoveOwnedDepth`, a baked one
through `ReshapeBakedDepth` and `MoveBakedDepth`. Inside a sprite it is
refused. The window test fits the dot each way and reads its scale and
translation, undoing in between, is refused on depth 1 (no box), fits an owned
depth to the width and sees it scaled and moved, and is refused inside a new
sprite. Dropping the sprite check, either move, or a shortcut's mode fails it.

The Motion sketch tool (`tool.sketch`, Y, on the tool strip and in the
Playback menu) is After Effects' Motion Sketch. While it is the chosen tool, dragging a depth the project owns on the stage starts playback
(`Window::SketchMove`, which `Window::MoveOnStage` asks first), and every frame
the playhead shows while the button is held records the pointer's offset from
where it was pressed: `Window::SeekTo` records the latest offset for each frame
it moves to, and each drag event records it for the frame on screen. Releasing
stops playback, puts the playhead back on the frame the sketch started from,
and keys the recording through `Document::SketchOwnedDepth` as one undo step.
The outline follows the pointer as in any drag; the edit is not previewed
through the host during the sketch, since reloading the clip on every move
would fight the playback for the frame. Baked depths move as usual with the
sketch on. Without a preview host there is no playback, so a sketch records
only the frames the playhead is moved to, which is what the host-less window
test does: it sketches the owned dot over three frames by choosing each one,
reads every keyed translation and that the playhead went back, sees a baked
depth moved normally, and sees a normal move with the sketch off. The live
test owns the widest depth of `title.ifs`, starts a sketch at frame 400, sees
playback running, lets it play for 0.7 seconds and releases, then sees it
stopped and reads frame 402, which only per-frame recording keys at the
pressed offset (interpolating between the press and the release would not).
It then sketches from frame 400 onto the frame just outside the owned span and
sees the sketch refused with playback stopped, which is why the release stops
playback itself rather than leaving it to the edit: a refused edit never
reaches the edit loop that would stop it.
Dropping the per-frame recording, the playback start or stop, the return to the
start frame, the owned check, the mode check or the recording of the drag's
own frame fails them.

`Depth > Flip horizontally` and `Flip vertically` (Flash's Flip Horizontal and
Flip Vertical, no shortcut) mirror the chosen depth along its own x or y axis
about its anchor on the playhead's frame, the way a -100% scale does in After
Effects, as one undo step. Each is `Window::ReshapeOnStage` with a scale of
-1 on that axis, so owned and baked depths go through the same reshape edits as
a drag, and a turned depth flips across its own axis, not the screen's. The
window test flips the dot horizontally, back, and vertically, reading the
scale each time, then flips an owned depth; swapping the two axes fails it.

Dragging the anchor cross of the selection moves the anchor without moving
the object (After Effects' Pan Behind, done here with the selection itself
rather than a separate tool). The cross follows the pointer while dragging, and
the release sends `Viewport::AnchorMoved` with the stage offset;
`Window::MoveAnchorOnStage` applies it through `Document::MoveAnchor` as one
undo step, refused on an owned depth as the centring is. Corner handles are
checked first, so a corner the anchor sits on (the top left of an object
placed without an origin) still scales; centre the anchor to move it off the
corner. The widget test drags such a corner and sees a scale and no anchor
move, then drags an anchor inside the box, reads the cross at the pointer and
not at the old place mid-drag, and gets one offset on release and no move. The
window test sends an offset for the dot and reads its new origin and
translation, and is refused on an owned depth. Dropping the gesture, the
corner priority, the preview, the release-only signal, the handle check that
keeps the selection, the window's connection or the owned check fails them.

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
an owned depth, whose trimmed keyframes are then kept). Holding Shift while
dragging a bar snaps it, as it does in After Effects (`Timeline::SnappedTo`,
through `Document::SnapTargets`, `SnapShift` and `SnapEdge`): a moved bar puts
whichever of its ends lands nearest on a target, a trimmed edge goes to the
nearest one, and the outline shows the snapped place while dragging. The
targets are where every other span starts and where it ends (the frame after
its last), the playhead, the labels, and the clip's first frame and end. The
reach is 8 pixels turned into frames at the current zoom, so it grows as the
timeline is zoomed out and is no frames at all once a frame is wider than 8
pixels. Pressing a bar chooses its depth straight away but seeks only if the
mouse is let go without dragging, so a drag leaves the playhead where it was
and a bar can be snapped to it; a click on a bar still seeks as before. The
widget tests Shift-drag bars onto another span, the playhead, a label and the
clip's end, see the outline snap before the release and a plain drag not snap,
trim both edges with Shift, and check that a drag on a bar does not seek while
a click does. Dropping the Shift check while dragging or on release, any of
the marks, either edge rule, the reach, the deferred seek or choosing the depth
on the press fails them.

The timeline menu
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
the copy, because its shapes and images belong to the package it came from.

`Depth > Split depth at the playhead` (Ctrl+Shift+D), or `Split depth N at
frame F` in the timeline menu, splits the selected depth's span at the playhead
into two bars on the same depth (`Window::SplitDepthAt`, through
`Document::SplitSpan`), as one undo step. Each half can then be moved, trimmed
or removed on its own. A split of a depth the project owns is refused, because
its record describes one span, the same as removing or grouping it; so is a
split `Document::SplitSpan` refuses, with its reason in the status bar. The
window test splits the dot's depth, reads the same translation on both sides
of the split, sees a second split on that frame refused because it is now a
first frame, sees a depth showing a character that is not an image or a shape
refused, undoes, splits again from the timeline menu, and sees an owned depth
refused. Dropping the ownership check, the shortcut or the menu entry fails it.

With several depths chosen, the timeline menu offers `Sequence N depths one
after another` (`Window::SequenceChosenDepths`, through `Document::SequenceSpans`):
their spans under the cursor are put end to end in depth order, as one undo
step, and every project-owned record among them moves with its span and is
saved. The window test inserts frames, shortens two spans, sequences them and
reads which frames each depth shows, undoes, then sequences an owned depth and
sees its record offered for detaching on its new frames and saved there.
Dropping the menu entry, the ownership lookup, the record move or the save
fails it.

With the timeline focused, the clipboard keys work on keyframes when some are
selected and on the chosen depth otherwise, the way Delete already does:
Ctrl+C copies the selected keyframes or the chosen depth's span under the
playhead (`Window::CopySelection`), Ctrl+X copies and then deletes them or
removes the chosen depths (`Window::CutSelection`, deleting only when the copy
worked), and Ctrl+V pastes whichever was copied last, keyframes onto the
playhead or a span through the usual paste that asks for a depth
(`Window::PasteClipboard`, `keys_copied_last_`). The depth window test copies
a depth, pastes it onto a new depth, cuts that depth and pastes it back; the
keyframe window test cuts a keyframe and pastes it on another frame, copies
one and pastes it, then copies a depth and sees Ctrl+V paste the depth rather
than the keyframes. Dropping either record of what was copied, the delete
after a keyframe cut, the removal after a depth cut, the depth copy, the paste
choice, the cut shortcut or the keyframe copy fails them.

`Depth > Duplicate depth` (Ctrl+D, After Effects' Duplicate Layer) copies the
selected depth's span under the playhead onto the first depth above it that is
free for it (`Window::DuplicateChosenDepth`, `Document::FreeDepthAbove`), with
no dialog, as one undo step, and selects the copy; the timeline menu's
duplicate still asks for a depth, and both go through
`Window::DuplicateSpanOnto`. A depth showing nothing on the playhead is
reported. The window test duplicates depth 1 past the taken depth 2 onto 3,
then 3 onto 4, undoes, and reads the report's exact text, which is what tells
it apart from the document's own refusal; skipping a depth, taking a taken
one, dropping the report or the shortcut fails it.

`Depth > At the playhead` holds After Effects' bracket keys for the selected
depth: `[` moves its span so it starts on the playhead and `]` so it ends there
(`Window::MoveEdgeToPlayhead`, through `MoveSpanInTime`), Alt+`[` trims its
start to the playhead and Alt+`]` its end (`Window::TrimEdgeToPlayhead`,
through `TrimSpanOnTimeline`), so a project-owned span moves or trims its record
with it as a drag on the timeline does. The span is the one under the playhead,
or the nearest one on that depth (`Document::NearestSpan`), so Alt+`[` before a
span stretches it back to the playhead. A move that would change nothing is not
made, so it leaves no empty undo step, and a depth with nothing in the clip is
reported. The window test shortens a span and walks it through all four keys,
reading the frames it shows after each, sees `[` on an aligned span leave the
undo name alone, and sees the refusal; swapping either end of the move, either
trim end, the empty-move guard or the report fails it.

`Depth > Arrange` changes the selected depth's place in the stacking order at the
playhead, with After Effects' shortcuts: `Bring forward` (Ctrl+]), `Send
backward` (Ctrl+[), `Bring to front` (Ctrl+Shift+]) and `Send to back`
(Ctrl+Shift+[) (`Window::ArrangeDepth`, through `Document::ArrangeSpan`). It is
one undo step named after the depth, the arranged span stays selected on its new
depth, and every project-owned record among the spans that moved takes its new
depth before the project is saved, the same as `Move to another depth` does for
one span. A refusal, such as a depth already at the front, shows in the status
bar and changes nothing. The window test sends an owned depth backward and
brings another to the front, reads the depths, the characters, the owned
translation and the saved manifest, and undoes both; leaving the owned records
or the manifest alone, not following the selection, or wiring `Bring to front`
to `Forward` fails it.

The timeline menu also offers to group the selected depth and the ones above it
into a sprite (`Window::GroupDepthsIntoSprite`): it asks for the last depth, then
the first and last frame, which start as the widest span those depths have under
the playhead, and applies `Document::GroupIntoSprite` as one undo step. It is
refused while the project owns any depth in that range. The clip list is refilled
so the new sprite can be picked, keeping the clip that was being edited. The
same menu offers to ungroup the sprite on the selected depth
(`Window::UngroupSpriteAt`, through `Document::UngroupSprite`), which is refused
while the project owns that depth and refills the clip list the same way.

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

`Depth > Align` lines the chosen depths up by their left edges, horizontal
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
With a sprite open, the menu also names that sprite's export
(`Window::NameShownSpriteExport`, through `Document::NameSpriteExport`): the box
starts on the current name and an empty answer removes it. It is an undoable
document edit, and the breadcrumb keeps the sprite open under its new label.

A live window test hides the widest depth of `graphic/1/title.ifs` and requires
the rendered frame to change and then come back exactly; comparing with the
selection outline in the picture was seen to pass even with the filter
switched off, so both pictures are taken with no depth selected.

**Timeline bar.** Above the timeline, `Editor::TimelineBar`
(`editor_timeline_bar.cpp`) is 36 px and follows the design: the transport
icons sit in a 276 px block over the timeline's own depth column (first frame,
previous frame, play on its own darker plate, next frame, last frame, previous
and next change on the depth, and loop as a checkable that turns blue), then
the frame spin box in the mono font (typing one seeks there), `/ 1439`, the
time in seconds from the animation's own rate, the label at or before the
playhead behind an amber keyframe glyph, and the work area ("Work area",
"420 to 600" or "none", and the cross that clears it). On the right, the
Timeline and Graph switch in one bordered pair brings that panel's tab to the
front, and the timeline zoom is a minus icon, a slider in pixels a frame and a
plus icon. `Window::RefreshTimelineBar` fills it on every seek, work area
change and clip change, from `shown_labels_` and `shown_rate_`, which
`ShowClipTimeline` caches so a scrub costs no read. Setting and clearing the
work area (`clip.work_start`, `clip.work_end`) and adding a depth
(`depth.add`) are in the Clip and Depth menus, which is where the design keeps
them.

The labels on the bar have fixed widths. They used to be sized to their text,
and because the bar sets the timeline panel's minimum width, a longer label
widened the panel, which moved every frame's x and made a rendered timeline
differ from the same frame rendered a moment earlier: the frame-key test
compares those pictures and caught it.

The slider is `Timeline::SetZoomPixels` and `Timeline::ZoomPixels`, the same
zoom the wheel and the commands use, said in pixels a frame. The timeline never
zooms out past fitting the whole clip, so a slider dragged below that snaps back
on the next `ZoomChanged`, which `Window::RefreshTimelineBar` listens for. The
slider's top is the larger of 48 (the most pixels a frame the timeline zooms to)
and the fit, so a short clip that already fits at more than that still reads its
true scale rather than sitting pinned at the end.

+ Depth (`depth.add`, `Window::AddDepthHere`) asks which character or package
image to place with the same `ChoosePlaceable` list the timeline menu uses, then
puts it on the next free depth from the playhead to the clip's last frame. The
window test places one, sees the timeline picture change, and undoes it back to
what it was.

**The scripts lane.** Between the ruler and the rows, `Timeline::DrawNotes`
draws one lane for what a frame carries besides placements: a dot on every frame
with a script and a square on every frame with a camera, from
`Document::FrameNotes` (`src/document/frame_notes.cpp`), which walks each frame's
tags and keeps the frames holding an `Action` or a `Camera`. Clicking a mark
seeks to that frame; the lane's gutter is a Camera button that runs `clip.camera`,
so adding or removing the camera at the playhead does not need the context menu.
`ShowClipTimeline` fills the lane with the clip it is showing, so entering a
sprite shows that sprite's scripts.

The ruler's gutter carries the column headers: an eye that runs
`depth.show_all` and a padlock that runs `depth.unlock_all`
(`Window::UnlockEveryDepth`), each lit while anything is hidden or locked, so
undoing a solo or a lock is one click rather than a walk down the rows. They
replaced the frame number that used to sit there, which the bar's frame field
already says. Double-clicking the ruler asks for a label at that frame
(`LabelAsked`, `Window::AddLabelAt`) and double-clicking a label's flag renames
it (`LabelRenameAsked`, `Window::RenameLabel`, which the context menu now calls
too).

The rows are listed front depth first: `ShowClipTimeline` sorts
`Document::DescribeClip`'s rows by depth, descending, before handing them over,
so the list reads like the stacking order on the stage. Each bar is coloured by
what its span places, from `SetCharacterKinds` (image, shape, sprite or text),
and carries a tick on every frame where a placement changes that depth
(`SetDepthMarks`, from `Document::DepthMarks`), so a baked depth shows where its
motion changes and not just where it starts and ends. A depth the project owns
gets an amber line along the bottom of its bar (`SetKeyedDepths`), which is the
timeline's half of the inspector's KEYED badge.

Each row's gutter carries a third switch beside the eye and the padlock: solo,
which hides every other depth of the clip, drawn filled while that depth is the
only one shown. Dragging a row by its depth number onto another row moves that
depth's span there (`Timeline::DepthDragged`, then `Window::MoveSpanOntoDepth`),
with the target row outlined while the drag lasts, which is the direct gesture
that replaces the "Move to another depth" question; the menu item still asks, for
a depth that is not on screen.

The gutter is 276 pixels, the width of the package panel beside it, so the
design's depth column reads as one column with the panel above it: the three
switches at 8, 27 and 46, the depth number right-aligned at 104 in the mono
font, and the name of what the depth places from 112, taken from the span under
the playhead (`Timeline::RowName`) and elided. The ruler is 30 pixels with the
scripts lane's 22 under it, and a row is 22. Those sizes and every colour live
in `editor_timeline_metrics.h`, which the widget tests include: their
`kTimelineWidth`, `kDepthRowY`, the property lanes and `FrameX` are all derived
from them, so a metric change moves the tests with it instead of breaking them.
The colours are the design's: the gutter and ruler `#1a1c20`, the frame area
`#141518` with a gridline `#1a1c20` at each ruler tick, rows separated by
`#141518`, the chosen row tinted `#1b3350` at a third, an image bar `#2f557c`
under a `#5d8fc4` top edge, a shape `#2b6a5b` under `#4fb59c`, a sprite
`#5a4787` under `#9179d1`, the playhead `#ff5d5d` and the work area a `#4c9dff`
wash over the frames and a stronger band on the ruler.

`editor_timeline.cpp` was 963 lines, so its painting moved to
`editor_timeline_paint.cpp` and the sizes and colours both halves share moved to
`editor_timeline_metrics.h`.

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
window geometry, the `QMainWindow` state, the dock manager state (only when it
is worth keeping, see **Window layout**), the last game install and the last
directory an IFS was opened from. The game install is
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
applies the result as one undo step, refills the clip list when that animation
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

`View > Zoom the timeline in` (=) and `out` (-), After Effects' keys, zoom the
same way around the playhead (`Timeline::ZoomIn`, `ZoomOut`). Both routes go
through `Timeline::ZoomAround`, which does nothing on a timeline with no
animation, so a key pressed before one is shown leaves no stale zoom. A zoom
step starts from the zoom the timeline is at, or from the fitting scale when
it is not zoomed, rather than from the widget's current width: the width is a
whole number of pixels, and before the panel is laid out it is not the panel's
width at all, so reading the scale back from it left a zoom in followed by a
zoom out short of fitting. The comparison with the fitting scale allows for
the rounding of multiplying and dividing by the step. The widget test, inside
a scroll area, zooms an empty timeline first, then zooms in and out with the
keys and the wheel (and sees a wheel without Ctrl do nothing) and the scroll
follow the playhead; the window test makes a 600 frame animation and zooms in
and back to fitting with the keys, which is how the old read-back was seen to
fail. Dropping the empty guard, either direction, the stored scale, the
rounding allowance, the scroll or either connection fails them.

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
longer match and shows them all in one sheet (`Editor::DriftSheet`): a row an
entry, saying whether it changed or is gone, each ticked to keep what the IFS
holds, with a button to tick or untick every row at once. Closing the sheet
calls `KeepIfsVersion` for the ticked ones, which drops the project's source for
them; the rest are left owned, so the next export overwrites them. The choices are written to the manifest straight away, the same as owning
and detaching, so a decision is never held only in memory. A project whose IFS
nobody touched asks nothing.

`Depth > Keyframe this depth` no longer needs a project open: with none,
`Window::MakeProjectIn` makes one in the folder `Window::SuggestedProjectFolder`
names, beside the IFS and called after it, and the depth is owned in it. The
File menu's New project is still there for putting one somewhere else.

**What the timeline keeps when a clip is shown again.** `Timeline::ShowKeys` is
the only place that decides which keyframes stay selected: it clears the
selection when it is handed a different depth than the one it holds, and
otherwise drops the keys whose property or frame is no longer there.
`Timeline::ShowAnimation` keeps the shown depth, its lanes and the selection,
because it runs again on every reload after an edit, an undo or a redo, showing
the same clip as before. It used to clear all three, which is why an undo left
the selection bar and the inspector's Keyframes section empty until the depth
was chosen again, and why `Window::EditAuthored` carried its own copy of the
selection across every edit.

`Window::ShowClipTimeline` hands the keys over itself
(`ShowKeysForDepth(AuthoredAt(depth_, frame_))`, after the frame is clamped to
the new clip), so the timeline's key state follows the window's chosen depth on
every path that shows a clip, including the one that opens an animation with the
preview host running and never reaches `Window::ShowFrame`. Opening another
animation or clip resets `depth_` first, so that call hands over nothing and the
selection goes. Two window tests pin it: an ease applied to two chosen
keyframes survives an undo and a redo with both still chosen, and an undo of the
edit that made one of them drops that one alone.

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
by the window, so they can be pasted into another owned depth. With two or
more keyframes selected, the lane menu also offers `Time-reverse N
keyframe(s)` (`Window::ReverseSelectedKeys`, through `Document::ReverseKeys`),
which keeps the same keyframes selected at their new frames and keeps the
focused one focused where it went. The window test inserts a frame so the
dropped dot spans four, owns it, sets keyframes on frames 0, 1 and 3 by
dragging it, reverses the three from the menu, and reads the values on every
frame, the new selection (frames 0, 2 and 3), the focused keyframe on frame 3,
and the undo. Dropping the menu entry, the selection or the focus fails it.

The lane menu offers `Wiggle N keyframe(s)...` for two or more selected
keyframes (`Window::WiggleSelectedKeys`, through `Document::WiggleKeys`). It
asks how many frames apart the wiggle keyframes go and the largest change, in
the property's own units, then adds the jittered keyframes between the first
and last selected of each property as one undo step and selects them with the
ends. Each wiggle is seeded from `QRandomGenerator`. The window test owns the
dot's depth, keys frames 0 and 3, cancels at each of the two questions and sees
nothing change, wiggles every frame by up to 20, and reads the four keyframes
selected, the ends unchanged and frames 1 and 2 within 20 of where they were.
Dropping the menu entry or its count, either cancel check or the new selection
fails it.

The lane menu offers `Simplify N keyframe(s)...` for three or more selected
keyframes (`Window::SimplifySelectedKeys`, through `Document::SimplifyKeys`).
It asks for the largest change allowed on any frame, in the property's own
units, 0 meaning nothing drawn may change, and removes every keyframe a line
can stand in for, as one undo step. The status bar says how many went.
The selection keeps itself across an edit without the keyframes that edit
removed, so the survivors stay selected, and a focus on a removed keyframe
shows no keyframe row, since the inspector finds no keyframe there. The window
test drags the dot's owned depth into a straight run on four frames, sees the
entry missing for two keyframes, cancels once and sees nothing change,
simplifies at 0, and reads the two ends selected, the message, no keyframe row
and every frame's translation unchanged. Dropping the menu entry or its count,
the cancel check or the message fails it.

The lane menu also offers `Time-stretch N keyframe(s)...` for two or more
selected keyframes (`Window::StretchSelectedKeys`, through
`Document::StretchKeys`). It asks for the stretch factor as a percentage, as
After Effects' Time Stretch does, and spreads the selection from its earliest
keyframe (a `KeyStretch` of that frame and `percent / 100`), as one undo step. Like the reverse, it keeps the moved keyframes
selected and the focused one focused where it went (`Window::SelectMovedKeys`
does both for the two). The window test owns the dot's depth, keys frames 0
and 1, cancels the dialog and sees nothing change, stretches them to 200% and
reads the values on frames 0, 2 and 3, the selection and the focus, undoes,
and is refused at 400% because frame 4 is outside the owned range. Dropping
the menu entry, the cancel check, the new selection or the focus fails it.

Alt-dragging the first or last selected keyframe on the timeline stretches the
selection with the other end fixed, as Alt-dragging does in After Effects. The
timeline works out the fixed end on the press (`Timeline::FixedEnd`: only the
earliest or latest selected frame, and only when they differ), shows every
selected keyframe where the stretch will put it while dragging, and on release
sends `KeysStretched` with the fixed end as the anchor and the ratio of the new
distance to the old one; `Window::StretchSelectedKeysBy` applies it, the same
edit the menu makes. The preview goes through `Document::StretchedFrame`, the
function the edit uses, so what the drag shows is what the edit does. An
Alt-drag of any other keyframe, or of a lone one, moves the selection as a
plain drag does. The widget test selects the keyframes on 0, 4 and 8,
Alt-drags the last to 4 and sees the preview's mark on frame 2 and the stretch
sent, Alt-drags the first the other way, and sees a middle keyframe, an
Alt-click that did not drag followed by a plain drag, and a lone keyframe all
shift instead. The window test sends a stretch and reads the values it moved.
Dropping the fixed end, either end's case, the lone-keyframe check, the stretch
preview, the press reset or the window's connection fails them.

Ctrl+Alt+H, or `Toggle hold` in the lane menu, toggles hold on the selected
keyframes (`Window::ToggleHoldSelectedKeys`, through `Document::ToggleHoldKeys`),
one undo step. The window test drags two held keyframes onto the dot's owned
depth, toggles them to linear and back with the keys and to linear again from
the menu, reading the value between them; dropping the shortcut or the menu
entry fails it.

Easy ease is After Effects' F9 (`Window::EasyEaseSelectedKeys`, through
`Document::EasyEaseKeys`): F9 eases both sides of each selected keyframe,
Shift+F9 only the way into it and Ctrl+Shift+F9 only the way out of it, and
the lane menu's `Easy ease` submenu offers the same three. It is refused, with
the reason in the status bar, when no selected keyframe has a neighbour on that
side. Any edit of an owned depth keeps the keyframe selection: rebuilding the
timeline after the edit used to drop it, so after easing from the menu nothing
was selected any more. `Window::EditAuthored` puts the selection back, and the
timeline drops any keyframe the edit removed, while the edits that choose their
own selection (moving, time-reversing, pasting, deleting) still set it after.
The window test gives the dot's depth keyframes on frames 0 and 3, presses F9
and reads the eased values between them, eases the first from the lane menu to
`Linear` and sees both keyframes still selected, eases the way into the last
from the submenu, and sees Ctrl+Shift+F9 on the last and Shift+F9 on the first
refused. Dropping the selection restore, the F9 shortcut or the submenu's `In`
fails it.

The Graph panel, a tab beside the timeline, is After Effects' value graph
(`Editor::GraphEditor`, `editor_graph.cpp`). It shows the focused keyframe's
property of the selected owned depth over the depth's owned frames: one line
per value (red, green, blue, amber in order, so x and y of a translation are
red and green), sampled every frame with `Document::SampleTrack`, a filled box
on every keyframe, the playhead as a line, and zero as an axis when it is in
view. Which track it shows is `Document::GraphedTrack`: the focused property if
the depth animates it and it eases between keyframes, so a character, blend,
clip depth, filter list or curve set, which only hold, is not graphed. The
vertical range is the values' range with a tenth of headroom, measured when the
track is shown and kept while dragging. Dragging a box up or down changes that
one value of that keyframe, and dragging it sideways moves the keyframe to
another frame, both drawn live. A keyframe moves only between its neighbours,
so the track stays in frame order while it is dragged, and not outside the
owned frames. Holding Shift keeps the drag to whichever way the mouse has moved
further from where it was pressed, so a keyframe can be retimed without
touching its value or the other way round, as in After Effects' graph editor.
On release the graph asks the window to focus the keyframe and, when its frame
or value changed, to move it (`Window::ApplyGraphMove`: `Document::ShiftKeys`
for the frame, then `Document::SetKeyValuesAt`) as one undo step, after which
the keyframe is focused on its new frame. A move the document refuses, onto
another keyframe for one, changes nothing.
Clicking elsewhere seeks to the nearest frame. Its signals are sent on release,
because focusing a keyframe refills the graph, which would end a drag begun on
the press. The widget tests read where each value is drawn (`KeyPoint`), the
colours of a box and of the line between two keyframes, the live drag, which
value a drag changes, a click on a keyframe changing nothing, the seek, a
keyframe retimed live and held between its neighbours at both ends, and Shift
keeping a drag across or up while a free diagonal drag changes both. The
window test focuses a keyframe of the dot's owned depth, sets a value from the
graph, focuses another and seeks from it, undoes, retimes a keyframe and sees
it focused on its new frame, sees a move onto another keyframe refused, and
sees the graph empty for a depth the project does not own. Dropping either
neighbour bound, the live frame, either Shift rule, the frame check on release,
the bound itself, the refocus, the retime or the new frame of the value also
fails them. Dropping the axis flip, the reach, the
component, the change check, the focus, the rounding, the keyframe boxes, the
line, the live value, the empty graph, the value edit, either connection or
the dock fails one of them. Whether a keyframe is drawn as a box or a dot is
not tested.

A tab that is not in front is taken out of the window's widget tree by the dock
manager, so `findChild` on the window cannot see the graph while the timeline
tab is showing; the window test finds it through `QApplication::allWidgets`.

**The graph panel.** `Editor::GraphEditor` draws every track of the owned depth
that is checked in the property list beside it (`graph_properties`), one colour
a track from `GraphEditor::ColourOf`, the first component in a thicker line than
the rest, with a box on each keyframe. `Window::FillGraphProperties` lists the
tracks with their colours and check states, and `Window::RefreshGraphTracks`
hands the checked ones to the graph. Unchecking a property refreshes the graph
only: rebuilding the list from inside its own `itemChanged` would delete the
item whose signal is running, which crashed the window test, so the list is
rebuilt only when the track names themselves change.

`Window::SeekTo` moves both playheads: `Timeline::SetFrame` and
`GraphEditor::SetFrame`. The graph used to learn the frame only from
`ShowTracks`, which `RefreshGraphTracks` calls when the chosen depth, the
property list or the document changes. A seek looked fine because `SeekTo` ends
in `ShowFrame` and that refreshes the keys, but playback skips it
(`if (!Playing()) ShowFrame();`), so while the animation played the timeline's
playhead ran and the graph's stood still. `The graph's playhead keeps up while
the animation plays` in `editor/tests/editor_live_window_tests.cpp` owns a
depth of `title.ifs`, brings the Graph tab to the front, grabs the widget,
plays, and grabs again until the picture changes. It is a live case because
`TogglePlay` refuses when `frame_count_` is 0, and that count comes from the
preview host, so nothing plays without a game install.

Fit all and Fit keys (`graph.fit_all`, `graph.fit_keys`, on the panel and in the
View menu) set the value range `GraphEditor::Fit` works out: every sampled frame
of the shown tracks, or only their keyframes. A bezier ease can overshoot
between two keys, so the two ranges differ exactly when the curve leaves the
values it joins, which is what the widget test pins.

A key with a bezier ease carries two handles on the way to the next key, drawn
at the curve's control points in the graph's own space. Dragging one moves that
control point (time clamped to the segment) and letting go emits `EaseEdited`,
which `Window::ApplyGraphEase` writes with `Document::SetKeysEase` for that one
keyframe, as one undo step. The numbers it writes are the same four the
inspector's Keyframes section shows.
 How a keyframe leaves its frame applies to the whole
selection when the right-clicked keyframe is part of it, and to that keyframe
alone otherwise. Bezier sets the curve the keyframe already had, or the first
`Document::EasePresets` curve, and the curve itself is then edited in the
inspector rather than in a dialog.

**The Content section.** Above Transform, the inspector says what the depth
places and how it is drawn: the character's name with a Replace button
(`Window::ReplaceCharacterOnDepth` asks which of the animation's characters to
use and writes it through `Window::UseCharacterOnDepth`), the blend mode as a
dropdown, a Masks up to depth number, and the placement's filters. The blend
names come from `Document::BlendModes` (`src/document/blend_modes.cpp`), which
carries every mode `Blend::MapAfpMode` handles, including the second and third
codes the game gives additive and subtractive, so a file that uses mode 8 keeps
mode 8 rather than being rewritten to 4. A value outside that list is shown as
"Mode N" and kept. The dropdown and the number write through
`Window::EditPlacementFieldOnDepth`, which is `Document::EditPlacementField` on
the same "Blend" and "Clip depth" fields the raw table edits.

Filters are listed as "Filter 1: colour matrix". Adding and removing them goes
through the keyframe path (`Document::AddKeyFilterAt`,
`Document::RemoveKeyFilterAt`), so the buttons are there only for a depth the
project owns, and a depth with no Filters keyframe at the playhead is told so
rather than silently doing nothing. `Window::ShowInspectorExtras` fills the
section from the same `Document::InspectFrame` rows the raw table uses.

**The Keyframes section.** `Editor::EaseEditor` (`editor_ease_editor.cpp`) is
the old ease dialog with its buttons taken off, embedded in the inspector's
Keyframes section, which appears only while keyframes are chosen. It says how
many are chosen, carries Hold, Linear and Bezier as checkable buttons showing
the ease of the first one, the `Editor::CurveEditor` drawn with
`Document::EaseProgress` over a unit square that has room above and below for
an overshoot, two handles to drag (kept inside the segment's time), a button
for each `Document::EasePresets` curve, and the four numbers as text, kept in
step with the handles. The curve and the numbers are greyed out unless the ease
is Bezier. Every one of those, the moment it is used, is one undo step over the
whole selection through `Window::ApplySelectedKeysEase` and
`Document::SetKeysEase`. `Window::RefreshEaseSection` fills the section from
`Document::KeyAt` on the first chosen keyframe, and runs whenever the selection
or the shown keys change. There is no modal ease dialog any more.

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
edit like any other. `View > Draw the background colour` asks the preview
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

A label can be dragged along the ruler, as a marker is in After Effects:
pressing within 4 pixels of its line on the ruler grabs it instead of seeking,
it is drawn where it is being dragged once the pointer has moved past the drag
threshold, and letting go moves it there (`Timeline::LabelMoved`, then
`Window::MoveLabelTo` with `Document::MoveLabel`, one undo step, the same edit
as the menu's `Move <label> to frame N`). A press that is not dragged seeks as
any click on the ruler does, a drag that ends on the label's own frame changes
nothing, and a press further than 4 pixels from a label, or on a depth row
under it, is an ordinary seek or span press. The widget test drags a label and
sees it drawn at the new frame before the release, clicks it, drags it back to
its own frame, drags from 10 pixels beside it and from the depth row below it;
the window test moves the sample's label and reads its frame from the saved
IFS. Dropping the ruler rule, the tight reach, the drag threshold, the
same-frame guard, the seek on a click, the live drawing, the connection or the
frame fails them.

`Playback > Go to frame...` (Alt+Shift+J, After Effects' Go To Time) asks for a
frame of the clip on screen, suggesting the playhead, and seeks there through
`JumpToFrame` (`Window::GoToFrame`). With nothing open it asks nothing. The
window test answers 2 and reads the translation frame 2 shows, and triggers it
on a window with no package to see no dialog; dropping the empty-clip check,
the answer or the shortcut fails it.

B and N start and end the work area at the playhead and `Clear the work area`
drops it (`Window::SetWorkArea`). Playback then stays inside it, and the ruler
shades it (`Timeline::SetWorkArea`). It belongs to the clip on screen and is
cleared when another clip or animation is shown. A live window test sets a
three frame work area on `graphic/1/title.ifs`, plays for 0.7 seconds and
requires the playhead to have stopped inside it; with the work area left out
of playback it was seen to fail.

`Clip > Trim the clip to the work area` (Ctrl+Shift+X, After Effects' Trim Comp
to Work Area) keeps only the work area's frames of the clip on screen
(`Window::TrimClipToWorkArea`, through `Document::TrimClipToFrames`), as one
undo step. The work area is cleared, since it is now the whole clip, and the
playhead stays on the same content. The playhead is worked out before the edit,
because the edit clamps it to the new frame count and it would otherwise land
on frame 0. When spans showing a sprite or another clip cross the new first
frame, the status bar says how many now start again. It is refused without a
work area, and while the project owns a depth in the clip, since its records
count frames. The window test trims the dot's intro to frames 1 and 2 with the
playhead on 1, reads the translations on the new frames, sees the work area
gone, the whole clip refused, the undo, a trim with nothing restarting that
says nothing, and the owned refusal; the live test trims `title` to six frames
around frame 400 and reads the host's frame. Dropping the clearing, the seek,
the quiet case, the owned or work-area check, or the shortcut fails them.

`Clip > Extract the work area` (After Effects' Extract Work Area, no shortcut)
takes the work area's frames out of the clip and closes the gap
(`Window::ExtractWorkArea`, through `Document::ExtractFrames`), as one undo
step. The work area is cleared, and the playhead keeps its content: a frame
after the cut moves up by the cut's length, and a frame inside the cut lands on
the frame that now follows it. When sprites or other clips run across the cut,
the status bar says their own timelines no longer line up. It shares the trim's
refusals (`Window::WorkAreaToEdit`): no work area, or a depth the project owns.
The window test gives the dot's intro six frames, extracts frames 2 and 3 with
the playhead on each side of the cut, reads the translations on the new frames,
sees the status message, the cleared work area, the whole clip refused and the
owned refusal; dropping the playhead's branch, the clearing or either
refusal fails it.

`Clip > Lift the work area` (After Effects' Lift Work Area, no shortcut)
empties the work area's frames and leaves every other frame where it was
(`Window::LiftWorkArea`, through `Document::LiftFrames`), as one undo step.
The playhead and the work area stay, so the same frames can be looked at
straight away. When sprites or other clips start again after the lifted
frames, the status bar says how many. It shares the trim's refusals, and is
refused when nothing is shown on the work area. The window test gives the
dot's intro six frames, lifts frames 2 and 3, reads the translation on either
side and nothing on frame 3, sees the status message, and lifts again to be
refused with the same work area; dropping the menu action or the quiet case
fails it.

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

There is no clip box any more. `Window::FillClips` keeps the animation's clips
(`clips_`, the root first) and which one is open (`clip_index_`), the stage
bar's breadcrumb shows the file, the root and the open clip, and a crumb opens
that clip. A clip is entered by double-clicking its bar on the timeline
(`Timeline::SpriteAt` reads the character the span places and only a sprite
counts), by double-clicking the object itself on stage (`Viewport::EnterAsked`,
`Window::EnterSpriteAt` picks the depth under the point and enters what it
places) or by double-clicking it in the library. Escape leaves it
(`clip.leave`, refused at the root).

Only a sprite has a timeline of its own. The timeline's own double-click
already checked that (`SpriteAt` returns nothing for a shape, an image or an
imported asset), but the stage's did not: it handed whatever the depth placed
to `EnterSprite`, which looked it up in `clips_`, missed, and said "Sprite 67 is
not a clip of this animation" about a character that is a shape. A user
double-clicked a character on IIDX 33's `x_logo_loop`, read that, and went
looking for a sprite that does not exist. `EnterSpriteAt` now asks
`Timeline::IsSprite` first: a sprite is entered, and anything else chooses that
depth and says "Shape 67: 33title_chara_01 has no timeline of its own: its
keyframes are on depth 7", which is where they are. Those two characters are
shapes placed straight onto root depths 7 and 6, re-placed nearly every frame,
so the bob is a row of marks on the root row, not a nested clip.
`A shape double-clicked on the stage...` in
`editor/tests/editor_bar_window_tests.cpp` drops the sample's dot shape, double
clicks it and checks the notice names a shape and its depth; it caught the old
message saying "Sprite". Opening a clip shows that sprite's depths,
labels and frames on the timeline and its placements and cameras in the
inspector. Placement fields, library call arguments, cameras, labels and the
structure edits all apply to the clip that is open.

**Editing a clip in place.** `Clip > Edit a clip in place on the root`
(`clip.in_context`, remembered, on by default) keeps the root on screen while a
sprite is open. `Window::ContextOf` finds the root depth that places this
sprite at `root_frame_` and takes its stage outline; `Window::RefreshContext`
stores it on every `ShowFrame`. The viewport dims everything outside that
outline and draws its edge (`Viewport::ShowContext`), the sprite's own outlines
are mapped onto the root through it (`Document::OutlineThrough`), and a drag is
mapped back the other way (`Window::UnderContext` through
`Document::UnderOutline`), so dragging a depth inside a clip that is placed at
200% moves it by half what the pointer moved. Scale and turn gestures are
computed against the mapped outline, so they carry the clip's own axes with
them; a clip placed with a skew is the one case where the reshape is only an
approximation of what the pointer drew.

Everything that hit-tests the stage goes through `Window::OutlinesOnStage`,
which maps the clip's outlines the same way the drawing does, so a click picks
what is under the pointer and the motion path is drawn where the object is.

What the host renders is still the root at `root_frame_`, so seeking inside the
clip moves the outlines and the inspector but not the picture under them. A
clip that is not placed on the root at `root_frame_` has no outline to map
through, so the window falls back to showing it on its own and the status bar
says that is why.

With `clip.in_context` off, picking a sprite shows it on its own in the
viewport. `Window::LoadViewportClip`
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
