# GUI tests (Dear ImGui Test Engine, headless)

`gui_tests` drives the real panel code with simulated mouse and keyboard input,
in a process with no window, no D3D9 device, no Konami DLLs and no game data.
It runs under `ctest -L ci` next to every other suite, so it gates hosted CI
like the rest.

## 1. Why this is possible at all

The GUI follows the rule in gui.md section 1.2: widgets never call the engine.
They post typed `App::Command` values into `App::State` and read published
`Status` / `LiveState` / `ExportState` / `LoadProgress` snapshots. That one
boundary is what lets the GUI be tested headless:

- The mock is a prefilled `App::State`. `MakeSceneReady()` in
  `tests/gui/ready_view_tests.cpp` sets `BootState::Ready`, an active backend
  id, `scene_loaded`, and an mc playhead. No IFS is parsed and no DLL is loaded.
- The assertion is a drained command queue. A test clicks a widget, then
  pops `App::Global().TakeCommand()` and checks the exact variant and payload.
  This checks the contract the render thread consumes, with no screenshot
  comparison involved.

Since the GUI's only outputs are commands and state changes, a test can decide
whether a click did the right thing without rendering a single pixel.

## 2. Dependency and licence

Dear ImGui Test Engine comes through vcpkg's own `imgui` port: `vcpkg.json`
adds the `test-engine` feature. The port fetches `ocornut/imgui_test_engine` at
the tag matching the imgui version, enables the std::thread coroutine
implementation, and patches the installed `imconfig.h` to include
`imgui_te_imconfig.h`, which defines `IMGUI_ENABLE_TEST_ENGINE`. Nothing is
vendored and no overlay port is needed.

The define is global to the imgui package, so `573Renderer.exe` also compiles
with the item hooks present. They cost one branch per item submission. If the
hooks are ever unwanted in release builds, the fix is a manifest feature toggled
per preset, not a second imgui build.

Licence: the test engine is free for individuals, non-profits, educational use,
publicly released open-source derivative works, and companies under $2M USD
annual turnover. Everyone else gets a 45-day trial and then needs a paid licence
from DISCO HELLO. This repository is public open source, so it qualifies. The
licence text must travel with distributed copies.

## 3. The `r573_app` split

Every GUI translation unit used to be compiled directly into
`add_executable(renderer WIN32 ...)`, so nothing else could link the panels.
`CMakeLists.txt` now builds `r573_app` (a STATIC library holding everything that
was in that list except `src/main.cpp`), and `renderer` is `src/main.cpp` linked
against it. `gui_tests` links the same library.

This is safe because no registry in the tree uses static self-registration.
`Gui::CollectActivePanels` and `Backend::CreateActive` both walk `constexpr`
tables, so the linker cannot drop a translation unit and silently lose a panel
or a backend.

## 4. The null backend

`GuiTest::Harness` (`tests/gui/gui_test_harness.cpp`) is the whole platform
layer:

- `ImGui::CreateContext()`, `io.DisplaySize = 1600x900`, `DeltaTime = 1/60`,
  and `IniFilename`/`LogFilename` nulled so no test writes layout state.
- `Gui::LoadFonts()` and `Gui::ApplyStyle()` are the production functions, so the
  tested layout is the shipped layout. `LoadFonts` reads `%WINDIR%\Fonts` and
  falls back to the ImGui default font, so it is safe on a hosted runner that
  may lack Segoe Fluent Icons.
- imgui 1.92 removed `GetTexDataAsRGBA32`, so the harness sets
  `ImGuiBackendFlags_RendererHasTextures` and services `ImTextureData` requests
  itself (`WantCreate` -> assign a fake id + `ImTextureStatus_OK`,
  `WantUpdates` -> OK, `WantDestroy` -> `Destroyed`). Without that, the atlas is
  never marked ready.
- The frame loop is `NewFrame` -> `Panels::Build()` -> `Render` -> service
  textures -> `ImGuiTestEngine_PostSwap`. The harness draws `Panels::Build()`
  every frame instead of using a per-test `GuiFunc`, so every test sees the
  application UI exactly as `gui_window.cpp` submits it.

Engine config: `ConfigRunSpeed = Fast`, `ConfigNoThrottle`, saved settings and
capture disabled. The suite takes well under a second per test.

## 5. Referencing items: child windows need `WindowInfo`

This part is not obvious, and it took a round of debugging to find.

A path like `"##setup/setup_card/Load"` does not resolve. ImGui child windows are
real windows whose name is mangled to `"<parent>/<child>_<08X id>"`, and the ids
of items inside them are seeded from that mangled window's id, not from the id of
the `BeginChild` string. The test engine has the fix:
`ctx->WindowInfo("setup_card").Window` returns the actual `ImGuiWindow*`, and
`ctx->SetRef(window)` retargets to it. `GuiTest::FocusChild(ctx, path)` wraps
those two calls.

Every test uses this pattern:

```
ctx->SetRef("##setup");                  // top-level window
GuiTest::FocusChild(ctx, "setup_card");  // descend into the child
ctx->ItemClick("Load");                  // items are now plain names
```

Nested children work the same way with a multi-segment path, for example
`FocusChild(ctx, "main_view/##timeline_dock")` from ref `##main`.

Two more rules:

- Do not use the `//` absolute prefix here. With an empty base ref the engine
  concatenates to `"///##setup/..."` and the lookup fails with id 0. Call
  `SetRef(window_name)` first and use relative paths.
- Items are addressed by their full label, including `##` suffixes. The fps
  quick buttons are `"60##fps_60"`, and the label combo rows are
  `"chorus   (frame 120)##lbl1"`. If a test breaks after a label edit, the gate
  is doing its job, because the label is the contract.

Transport buttons are labelled with the Segoe Fluent Icons PUA glyphs from
`gui_icons.h`, so tests include that header and click `ICON_STEP_FWD` directly.
The id is the string, so this works whether or not a font has the glyph.

### 5.1 Four more id-scoping rules the suite depends on

- A selected tab pushes its own id. `BeginTabItem` calls `PushOverrideID`, so
  everything inside the Render tab lives at `##inspector_tabs/Render/<item>`, not
  at `<item>`. The same goes for the `mc_names_list` child inside the Live tab.
  Every inspector test opens the tab first and then uses the prefixed path.
- `Gui::Segmented` nests two id levels. It does `PushID(id)` and then `PushID(i)`
  per button, so the second option of the root-loop control is
  `##root_loop/$$1/Force loop`. `$$N` is the test engine's syntax for an integer
  id pushed with `PushID(int)`.
- `ComboClick` splits its path at the first slash, so it only works for
  `combo/item` and cannot reach a combo that is itself nested (inside a tab,
  for example). `GuiTest::ComboPick` does the same job the long way: it clicks
  the combo by full path, then clicks the entry inside `//$FOCUSED`. Use it for
  anything deeper than one level.
- `ColorEdit3` with `NoInputs` submits its swatch as a child id. The item to
  query is `##exp_bg_color/##ColorButton`, not `##exp_bg_color`.

### 5.2 Tree nodes that only open on their arrow

Scene-tree child rows use `ImGuiTreeNodeFlags_OpenOnArrow`, and clicking the
label selects the row instead of expanding it. The engine's `ItemOpen` clicks
the centre of the item, which does nothing for these nodes, so it reports
"Unable to Open item". `GuiTest::ClickTreeArrow` hovers the item first (which
also sets the mouse viewport; `MouseMoveToPos` with a raw position does nothing
without it), then clicks at `RectFull.Min.x + fontSize/2`, inside ImGui's arrow
hit box.

### 5.3 Panel-local statics leak between test cases

The panels keep filter buffers, the selected main view, the export settings and
the resolution-combo index in file-scope statics, as the running app does. A
`Harness` resets `App::State` but cannot reset those statics. The suite handles
the two consequences directly:

- Tests that type into a filter (`##ifsfilter`, `##scene_filter`) clear it again
  before finishing, and tests that need the unfiltered tree clear it on entry.
- Tests whose assertion depends on a specific setting (export format, resolution
  preset) set that setting explicitly instead of trusting the default. Each
  scene-panel test also loads its own ifs path, because `Scene::Reset` only fires
  when the active ifs changes; otherwise the selection from the previous test
  survives.

### 5.4 `Yield` is a Windows macro

`winbase.h` defines `Yield()`, so a translation unit that reaches `windows.h`
(the harness does, through `native_dialog.h`) cannot call `ctx->Yield(2)`. The
harness `#undef`s it after its includes.

## 6. Native dialogs are stubbed

`NativeDialog::SetOverrides()` installs four function pointers that are checked
at the top of `BrowseForFolder`, `RevealInFileManager`, `OpenFile` and
`SaveFile`. Without them, a test that clicks **Browse...**, **Import...** or
**Export...** would open a real modal dialog and hang CI, and **Open folder**
would start Explorer on the runner.

The harness installs stubs in its constructor and clears them in its destructor.
`GuiTest::SetBrowseResult()` sets what the folder picker returns (an empty string
means the user cancelled, which is worth testing too). `SetOpenFileResult()` and
`SetSaveFileResult()` do the same for the file dialogs, and
`GuiTest::TakeRevealedPath()` reports what Explorer would have been asked to
select. The library tests point them at files under a temp directory, so an
import or export reads and writes real files with no dialog on screen.

The preset library's scan root is injected the same way.
`Panels::PresetLibrary::SetUserRoot()` replaces `presets/` next to the executable
with a temp folder, so a test can put a user document on disk and expect a User
row without touching the developer's own presets.

## 7. Assertions: two frameworks, two threads

The test engine runs `TestFunc` on a coroutine thread, where Catch2 assertion
macros are not thread-safe. The split is:

- Inside `TestFunc`, only `IM_CHECK*` macros (engine-native, recorded into
  the test log).
- After `harness.Run(test)`, Catch2 `CHECK` / `REQUIRE` against `App::State`.

`Harness::Run` queues one test and pumps frames until the queue drains or a
budget of 4000 frames runs out (a guard against waiting forever). It attaches
the engine's log through Catch2 `INFO`, so a failure prints the exact
`ItemClick` that could not resolve, and then requires `ImGuiTestStatus_Success`.

## 8. State isolation between tests

`App::Global()` is a process-wide singleton, and the panel files hold static
state (`g_dir_buf`, the resolution combo's `shown_idx`, `g_main_view`). Each
`Harness` constructor resets every `App::State` field it touches and drains the
command queue, and each destructor tears down the ImGui context, so tests do not
depend on declaration order. The panel-local statics re-sync from state on the
next frame (`SyncDirBufFromState` compares against the last value it saw).

The reset also clears `App::PresetStatus` and closes `Editor::Global()`, because
both outlive an ImGui context. A document left loaded by one case made
`Panels::Timeline::Active()` true in the next, which drew the timeline editor
over an AFP test and pushed a `ReplaceDocument` command ahead of the command the
test was reading. `BootLifecycle::SetLoadProgress` clears the minimum-hold
timestamp for the same reason: without that, the loading overlay raised by one
case's preset scan was still "active" for the first frames of the next case.

`PersistSetup` calls `App::SaveCurrentSettings()`, which writes `settings.ini`
next to the running executable. This side effect is intended. For `gui_tests`
the file is `build/settings.ini`, never `bin/settings.ini`, so the developer's
real settings are untouched.

### 8.1 Waiting on a background job: never spin on `ctx->Yield()`

The `.arc` and customize extractors run on a detached `Support::FolderJob`
thread. A test that clicks the button and then needs the finished status must
wait on wall-clock time, outside the ImGui test func:

```cpp
REQUIRE(WaitForJob([] { return ArcExtract::IsRunning(); }));
ArcExtract::Status const st = ArcExtract::GetStatus();
```

`WaitForJob` polls `IsRunning()` with a 1 ms sleep against a 30 s deadline. The
earlier version had neither the deadline nor the sleep, and both are needed:

- A frame count is not a timeout. The original wait was
  `for (int i = 0; i < 600 && IsRunning(); i++) ctx->Yield();`. Against a null
  backend, 600 frames take tens of milliseconds, not the seconds the number
  suggests. It passed on a developer machine and failed on a GitHub runner,
  where starting the thread and scanning an empty temp folder took longer than
  that. The test then read a status the worker had not published yet and failed
  on `CHECK_FALSE(st.running)` / `CHECK(st.finished)`. The extractor's own "done"
  log line appeared after the Catch2 failure output, which showed the worker was
  still running.
- Spinning starves the thread you are waiting for. `ctx->Yield()` renders a
  frame as fast as the CPU allows, and on a two-core runner that competes
  directly with the worker. The sleep gives the worker time to finish.
- Wait after `harness.Run(test)` returns, not inside `TestFunc`. The status is a
  mutex-guarded struct written by the worker, so it needs no ImGui frames to
  progress, and a wait inside `TestFunc` would use up the harness's 4000-frame
  budget for nothing.

The ordering the wait relies on holds: `FolderJob::Start` sets the atomic
before it starts the thread (so `IsRunning()` is already true when `Run`
returns), and the worker calls `Publish(finished)` before `Finish()` clears the
atomic (so `IsRunning() == false` guarantees the final status is visible).
Waiting on the atomic and then reading the status is therefore race-free.
Waiting on `st.finished` directly would not be, because a job that never starts
never publishes.

## 9. Coverage

There is one file per panel or editor surface. The counts below cover the panels
in the first version of this suite; the scene preset editor files
(`timeline_tests`, `timeline_editor_tests`, `clip_modal_tests`, `tween_ui_tests`,
`library_tests`, `frame_inspector_tests`) were added in milestones M4 to M7:

| file | cases | what it drives |
|---|---|---|
| `setup_view_tests.cpp` | 13 | game-dir input, Browse accept/cancel, the disabled Load gate, fps quick buttons, profile combo, resolution preset + custom clamping, the BootGame payload, the DDR-only arc and customize extractors (visibility, cancel, and a real run over an empty temp dir) |
| `shell_tests.cpp` | 13 | setup-vs-ready view swap, top-bar Export (enabled + disabled), the per-profile main-view switch, all three backend tab sets, tab switching, splitter drag, status-strip export tag and reveal button |
| `browse_panel_tests.cpp` | 12 | empty catalog, scanning state, selection posting LoadContent, the `from_arc` flag, the no-reload-of-active guard, filter, directory grouping, expand/collapse, the Load alongside context menu, its active-IFS guard, Unload from alongside, the loaded-alongside list's Unload button |
| `scene_panel_tests.cpp` | 13 | layer double-click, child visibility override, child expansion, filter, add-slot (accept, empty, duplicate), slot visibility, bitmap combo pick + restore-default, the text-field fallback, unresolved slots |
| `inspector_tests.cpp` | 16 | Play/Replay, loop-master, root-loop and continuous-loop segmenteds, trim, master-scale slider + both presets, background segmented, filter and MC-names toggles, name-type segmented, reset overrides, the DDR-reduced Render tab, the Live tab's MC list |
| `timeline_tests.cpp` | 12 | jump back, resume, track drag-to-seek, the zero-length guard, label ticks, Space, arrows, Shift+arrows, Ctrl+E, shortcut suppression during capture, the no-scene and no-labels states |
| `ready_view_tests.cpp` | 7 | transport play/pause, step 1, step 100, wrap at frame 0, label combo, status-strip reveal |
| `export_panel_tests.cpp` | 22 | stem + format to output path, PNG directory form, fps/quality, resolution preset/native/custom/scale buttons, transparent-bg toggle and its colour gate, hardware-encode gate, keyframe interval, frame limit, loop count, blend seam, crop inputs/Clear/Pick/reopen, Start, Cancel, Close, reveal |
| `qpro_panel_tests.cpp` | 11 | scan command, category gating, extract payload, picker cancel, fps clamp, scan-error state, the per-date part groups, All / None, a group checkbox, the issue list's clipboard copy |
| `host_panel_tests.cpp` | 7 | 3D-scene and 2D-package panels while idle, the loading overlay over both views and its absence, the boot-error banner, Load disabled during boot |
| `host_live_tests.cpp` | 7 | the 3D and 2D panels driving real loaded hosts: pause, speed, playhead, animation combo, animate-models, free camera, reset view, move speed, model visibility and blend mode, and the camera-checkbox gates |
| `library_tests.cpp` | 22 | the preset library: the library owning the center pane on the scene3d backend with nothing of it left under `main_view/pane_left`, an AFP backend keeping `##scene_filter` / `scene_scroll` in the centre with no `###lib_*` item anywhere, the `lib_scroll` list taking more than 40 percent of the pane height, Built-in / User / Other builds groups from a temp scan root, selecting a row loading the document and posting `PresetCmd::LoadDocument`, an unparseable import reporting its line and column and loading nothing, an import with a validation error loading and listing it, an import whose id is taken keeping the file that owns it, export then import byte-equal, Save writing `presets/<build>/<id>.json` and clearing the modified mark, Ctrl+S, the unsaved-changes prompt, New generating the id from the name, Duplicate, a built-in offering a user copy instead of saving in place, the problem list staying inside a short pane, an other-build row opening read-only with no `LoadDocument` posted and every editing control disabled, Duplicate turning it into an editable copy for this build, an unloadable user file appearing under "Files that did not load", and a close request prompting when dirty (Cancel keeps running, Discard confirms) but exiting straight away when clean |
| `frame_inspector_tests.cpp` | 2 | the Inspector "Frame" tab naming the clip that won a conflicting value, and clicking that winner selecting the clip |
| `window_tests.cpp` | 10 | `Gui::Init` device creation, the min-track-size clamp, `WM_ERASEBKGND`, `WM_SYSCOMMAND`/SC_KEYMENU, live resize, `WM_PAINT` + validation, the pump's WM_QUIT exit, lost-device recovery, and two backbuffer pixel assertions |
| `modal_layout_tests.cpp` | 3 x 3 | every editor modal (Add track, Clip properties, Document properties, Option properties) keeping its content inside the modal, run at 96, 144 and 192 DPI |
| `dpi_scaling_tests.cpp` | 2 | the style/font scale factors at 96/144/192 DPI, and the top bar, status strip and timeline dock doubling in height at 192 while still holding their content |

Every interactive widget in `src/gui` is now exercised. The four gaps the first
version of this document listed are all closed; sections 12 to 14 describe how.

Golden-image comparison is still out of scope. The pixel assertions here are
structural (see 14), not reference-image diffs; the image-diff checks are
`pixel_golden_tests` and `docs/local_regression.md`.

## 10. Two defects the suite found on its first run

Both were fixed in the same change. They are recorded here because each is a
kind of bug that will come back.

1. `ImGui::IsItemToggledOpen()` was queried too late (`gui_scene_panel.cpp`).
   `RenderSceneNode` called it several items after the `TreeNodeEx`, past the
   `TextDisabled` that prints a clip's `(x, y)` position and past the variant
   badge. `IsItem*` reads `g.LastItemData`, which every later `ItemAdd`
   overwrites, so expanding a child clip that had a position never recorded the
   expansion, while a clip without one worked. The fix latches
   `const bool toggled_open = ImGui::IsItemToggledOpen();` right after the
   `TreeNodeEx` and uses the latched value. Rule: read `IsItem*` results on the
   line after the widget, never further down.
2. Truncated button id (`gui_export_panel.cpp`). The export scale buttons
   build `"%.*s (%dx%d)##exp_scl_%s"` into a 32-byte buffer. The real string is
   37+ bytes, so every id was silently cut short. The visible text was fine
   (it comes before the `##`), which is why nobody noticed, but ids that differ
   only after the cut would have collided. The buffer is now 64 bytes.

## 11. How to add a case

Pick the view, prefill `App::State` for the scenario, `SetRef` + `FocusChild` to
the owning child window, act, then assert on the drained command or the changed
state. If the widget calls something outside `App::State` (a background worker,
a native dialog), give it a seam like `NativeDialog::Overrides` so the test does
not touch the real thing. If it depends on a panel-local static, set that static
through the UI first (see 5.3) instead of trusting whatever value the previous
test left behind.

## 12. Synthetic game assets (`gui_mock_assets`)

The 3D scene and 2D package panels render their bodies only once
`Scene3dHost::Active()` / `Gc2dHost::Active()` are true, which needs a real
`Load()` over real files. `gui_mock_assets.cpp` builds those files byte by byte
into a `TempAssetDir` (created under the OS temp dir and removed in its
destructor), so no game data is required and nothing is committed.

Both loaders read LZSS-compressed payloads, and an all-literal compressor is
easy to write: a 4-byte little-endian uncompressed size, then a `0xFF` flag byte
before every group of 8 literals. `tests/formats/model3d_tests.cpp` already used
the same approach.

- 2D package (`WriteMock2dPackage`): a `system.idx` in the two-chunk
  little-endian form, `[u32 size0][chunk0][u32 size1][chunk1]`. Chunk 0 carries
  the 0x1B8-byte fixed header (texture count at 0x002, cell-table offset at
  0x004, record-table offset at 0x010, 32-byte texture path slots from 0x014),
  then two cells terminated by a zero-width cell, then five 36-byte records:
  draw, end-animation (`t_end` 30), draw, end-animation (`t_end` 45),
  end-table. Chunk 1 holds the three consecutive name tables the parser expects
  (cells, an empty unused table, animations). Each is a run of NUL-terminated
  names, each followed by a `u16` index, closed by a `0` byte. The animation
  start indices make `anim_intro` 30 frames and `anim_loop` 45, and the panel's
  frame slider is asserted against those lengths.
- Textures are 24-byte GC headers (`"GC "` magic, big-endian origin, size and
  declared byte count, depth byte at 0x13) followed by opaque 16bpp pixels.
- 3D scene (`WriteMock3dScene`): a `.inz` manifest (one image slice plus one
  pattern rect, so `TileForTexture` resolves), the `0.gcz` tile that slice names,
  and a `.xz` model. The model is plain `xof 0303txt` text with a frame
  hierarchy, one textured triangle, `MeshTextureCoords`, and an `AnimationSet`
  whose last key is at tick 600 (which `max_time == 600` asserts). Passing
  `with_camera = true` adds a frame whose name contains `camera`, because
  `Scene3d::FindCamera` matches on that substring. That is how the tests reach
  both the enabled and the disabled branch of the **Animate camera** checkbox.

## 13. The WARP GPU fixture and the qpro seams

`GuiTest::WarpGpu` creates a D3D9-on-12 WARP device (the same
`src/warp_device.cpp` that `pixel_golden_tests` uses) and points `g_d3d.device`
at it for the lifetime of the test, restoring the previous pointer afterwards.
That is all `Scene3dHost::Load` / `Gc2dHost::Load` need to upload their tile
textures and flip `Active()`.

The qpro part list needed two publish seams. Both were added as refactors that
production code also calls, not as test-only entry points:

- `QproExtract::PublishScanResult(parts, error)` merges the two duplicated
  lock-and-store blocks at the end of `RunScan` into one function. The tests
  call it with synthetic `ScanPart`s carrying two different dates, which makes
  the per-date groups, `All`, `None` and the group checkbox reachable.
- `BeginRunStatus` now uses `QproExtract::PublishStatus(status)` to install its
  fresh status. The tests use it to stage a finished run with issues so the
  **Copy list** button renders.

The clipboard assertion reads `ctx->Clipboard` from inside `TestFunc`, not the
ImGui platform IO. The test engine swaps in its own clipboard handlers while a
test runs, so anything the app copies lands in the engine's per-test buffer, and
a handler installed by the harness would never be called.

## 14. The window and pixel tests

`window_tests.cpp` drives the real `Gui::Init` / `PumpAndRender` / `Gui::Shutdown`
against a real Win32 window and D3D9 device. It covers the message handling that
tests using only `Panels::Build` cannot reach: the `WM_GETMINMAXINFO` clamp
against `gui_layout_constants.h`, `WM_ERASEBKGND` returning 1, the SC_KEYMENU
swallow, `WM_SIZE` servicing a resize from inside the handler, `WM_PAINT`
validating its region, the pump's WM_QUIT exit, and the lost-device recovery
path.

These tests exposed two contracts:

- `Gui::Shutdown` leaves a WM_QUIT in the thread queue (`DestroyWindow` ->
  `WM_DESTROY` -> `PostQuitMessage`). That is correct for the app, since the GUI
  thread should end when the window closes, but a second `Gui::Init` on the same
  thread then starts with a quit already pending, and the next `PumpAndRender`
  returns false. The fixture drains the queue around every window, and any code
  that restarts the GUI would have to do the same.
- `Gui::Init` falls back to WARP when `Direct3DCreate9` cannot produce a HAL
  device (see docs/gui.md section 1.3). The control panel then still works on
  machines and VMs without a D3D9 HAL driver, and these tests can run where no
  HAL device exists.

The pixel assertions read the backbuffer through `GetRenderTargetData` into a
system-memory surface and count matching pixels:

- the setup view must paint the shell clear colour over a large part of the
  frame (a blank or unpresented frame fails);
- the ready view must contain the current profile's accent button colour and
  must not contain the other profile's. The expected colour is read from
  `ImGui::GetStyleColorVec4(ImGuiCol_Button)` at capture time instead of being
  hardcoded, because `ApplyAccentColors` mixes the accent with the surface colour
  (`Mix(kRaised, accent, 0.28)`) and the raw accent from the profile table never
  appears on screen. Reading it from the live style keeps the test accurate
  without freezing the palette. The scene must be marked loaded first. Otherwise
  the only accent-coloured button in the top bar is the disabled Export button,
  whose alpha blend moves it off the exact colour.

## 15. Running the GPU-backed tests

`WarpGpu`, `LiveWindow` and `pixel_golden_tests` all skip (Catch2 `SKIP`, ctest
reports "Skipped") when no D3D9 device can be created, so the suite stays green
on machines and CI legs without one.

The skip also triggers in any shell that cannot reach the graphics stack. A
sandboxed or non-interactive parent process sees
`IDirect3D9::CreateDevice ... hr=0x88760868` for both HAL and WARP, while the
same binary succeeds when launched normally. If the GPU-backed cases skip
unexpectedly, run the executable detached before drawing any conclusion about
the code:

```
powershell -Command "Start-Process -FilePath .\build\gui_tests.exe -Wait -RedirectStandardOutput out.txt"
```

With a device present, the whole suite is 192 cases / 761 assertions and nothing
skips (`cli_tests` adds 29 cases / 162 assertions).

## 16. Asserting tooltip text, not just presence

`GuiTest::HoverShowsTooltip` only checks whether a `##Tooltip` window appeared.
That is enough for a tooltip with constant text, but several tooltips change
what they say with state: the hardware-accel note gives a different reason per
format, the animate-camera note gives a different reason for each way it can be
greyed out, and the export status tag carries either the output path or the
encoder error. A presence check would pass while the wrong branch renders.

`GuiTest::HoverAndCaptureText` hovers an item and returns the text imgui
rendered that frame, so the test can assert the branch:

- The harness owns the capture, because the log has to be armed inside its frame
  loop: `ImGui::LogToBuffer()` right after `NewFrame()`, then `Panels::Build()`,
  then read `GetCurrentContext()->LogBuffer` before `ImGui::LogFinish()`.
  `LogFinish` clears the buffer, so reading after it always gives an empty
  string. Getting this order wrong cost a round of debugging.
- `GuiTest::CaptureFrameText` is the same capture without the hover, for
  tooltips owned by an item that has no id and so no addressable path.
  There are two: the timeline label tick (drawn on the `##tl_track` invisible
  button, hit-tested by mouse x) and the qpro "Animated parts ->" note
  (`ImGui::TextDisabled`, id 0). Both are reached with `MouseMoveToPos`. For the
  qpro note, y is the preceding `Output fps` item rect plus
  `2 * ItemSpacing.y + FontSize * 0.5`, which is how far `ImGui::Spacing()`
  followed by one text line advances, so it does not drift with the style.
- `MouseMoveToPos` does not set the mouse viewport. Call `MouseMove` on any item
  in the target window first, as `ClickTreeArrow` does.

The suite asserts every `ImGui::SetTooltip` call site in `src/gui` (58 sites at
the time of writing: 22 export modal, 9 inspector, 7 3D scene, 6 shell/status
strip, 4 timeline, 4 qpro, 3 setup, 2 scene pane, 1 2D package). Tooltips with
branches are driven through each branch, not just once. If you add a
`SetTooltip`, add its assertion in the same change; `grep -rn SetTooltip src/`
is the checklist.

## 17. Running a case at a non-default DPI

`GuiTest::Harness` takes a DPI: `GuiTest::Harness harness(192)`. The constructor calls
`Gui::Dpi::SetScaleFromDpi(dpi)` before `LoadFonts()` / `ApplyStyle()` (both read the scale)
and sizes `io.DisplaySize` to `S(1600) x S(900)`, so the context simulates a denser screen of
the same physical size, not a 1600x900 window with giant text. The destructor resets the scale
to 96, so a case that forgets to reset it cannot leak into the next one.

Catch2's `GENERATE(96U, 144U, 192U)` re-runs a whole `TEST_CASE` per value. Pair it with
`INFO("display dpi " << dpi)` so a failure names the scale. That is how `modal_layout_tests`
covers all three without duplicating the drive code.

This catches the whole class of bugs where a hardcoded pixel size meets scaled text: a fixed
modal width or child height silently overflows once the font grows, and only an assertion that
the content fits its space, run at a second DPI, can see it. When you add a layout case that
could be size-sensitive, run it at 192 too, and check that the check can fail by removing one
`Gui::Dpi::S()` and watching it go red (docs/gui.md 2.1).
