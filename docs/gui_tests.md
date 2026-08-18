# GUI tests (Dear ImGui Test Engine, headless)

`gui_tests` drives the real panel code with simulated mouse and keyboard input,
in a process with no window, no D3D9 device, no Konami DLLs and no game data.
It runs under `ctest -L ci` next to every other suite, so it is a hosted-CI
gate, not a local-only tier.

## 1. Why this is possible at all

The GUI already obeys the rule in gui.md section 1.2: **widgets never call the
engine**. They post typed `App::Command` values into `App::State` and read
published `Status` / `LiveState` / `ExportState` / `LoadProgress` snapshots.
That single seam is what makes headless testing viable:

- **The mock is a prefilled `App::State`.** `MakeSceneReady()` in
  `tests/gui/ready_view_tests.cpp` sets `BootState::Ready`, an active backend
  id, `scene_loaded`, and an mc playhead. No IFS is parsed, no DLL is loaded.
- **The assertion is a drained command queue.** A test clicks a widget, then
  pops `App::Global().TakeCommand()` and checks the exact variant and payload.
  That is a behavioural check of the contract the render thread consumes, not a
  screenshot comparison.

Because the GUI's only outputs are commands and state mutations, "did the click
do the right thing" is fully decidable without rendering a single pixel.

## 2. Dependency and licence

Dear ImGui Test Engine arrives through vcpkg's own `imgui` port: `vcpkg.json`
adds the `test-engine` feature. The port fetches `ocornut/imgui_test_engine` at
the tag matching the imgui version, enables the std::thread coroutine
implementation, and patches the installed `imconfig.h` to include
`imgui_te_imconfig.h`, which is what defines `IMGUI_ENABLE_TEST_ENGINE`. Nothing
is vendored and no overlay port is needed.

Consequence: the define is global to the imgui package, so `573Renderer.exe`
also compiles with the item hooks present. The cost is one branch per item
submission. If the hooks are ever unwanted in release builds, the fix is a
manifest feature toggled per preset, not a second imgui build.

Licence: the test engine is free for individuals, non-profits, educational use,
publicly released open-source derivative works, and companies under $2M USD
annual turnover; otherwise a 45-day trial then a paid licence from DISCO HELLO.
This repository is public open source, which qualifies. The licence text must
travel with distributed copies.

## 3. The `r573_app` split

Before this, every GUI translation unit was compiled directly into
`add_executable(renderer WIN32 ...)`, so nothing else could link the panels.
`CMakeLists.txt` now builds `r573_app` (a STATIC library holding everything that
was in that list except `src/main.cpp`) and `renderer` is `src/main.cpp` linked
against it. `gui_tests` links the same library.

This is safe because no registry in the tree uses static self-registration:
`Gui::CollectActivePanels` and `Backend::CreateActive` both walk `constexpr`
tables, so the linker cannot drop a translation unit and silently lose a panel
or a backend.

## 4. The null backend

`GuiTest::Harness` (`tests/gui/gui_test_harness.cpp`) is the whole platform
layer:

- `ImGui::CreateContext()`, `io.DisplaySize = 1600x900`, `DeltaTime = 1/60`,
  `IniFilename`/`LogFilename` nulled so no test writes layout state.
- `Gui::LoadFonts()` + `Gui::ApplyStyle()` are the production functions, so the
  tested layout is the shipped layout. `LoadFonts` reads `%WINDIR%\Fonts` and
  falls back to the ImGui default font, which is what makes it safe on a hosted
  runner that may lack Segoe Fluent Icons.
- imgui 1.92 removed `GetTexDataAsRGBA32`, so the harness sets
  `ImGuiBackendFlags_RendererHasTextures` and services `ImTextureData` requests
  itself (`WantCreate` -> assign a fake id + `ImTextureStatus_OK`,
  `WantUpdates` -> OK, `WantDestroy` -> `Destroyed`). Without that the atlas is
  never marked ready.
- The frame loop is `NewFrame` -> `Panels::Build()` -> `Render` -> service
  textures -> `ImGuiTestEngine_PostSwap`. `Panels::Build()` is drawn every frame
  from the harness rather than from a per-test `GuiFunc`, so every test sees the
  real application UI exactly as `gui_window.cpp` submits it.

Engine config: `ConfigRunSpeed = Fast`, `ConfigNoThrottle`, saved settings and
capture disabled. The whole suite finishes in well under a second per test.

## 5. Referencing items: child windows need `WindowInfo`

This is the one non-obvious part and it cost a debugging round.

A path like `"##setup/setup_card/Load"` does **not** resolve. ImGui child
windows are real windows whose name is mangled to
`"<parent>/<child>_<08X id>"`, and items inside them are seeded from that
mangled window's id, not from the id of the `BeginChild` string. The test
engine exposes the fix: `ctx->WindowInfo("setup_card").Window` returns the
actual `ImGuiWindow*`, and `ctx->SetRef(window)` retargets to it. That is what
`GuiTest::FocusChild(ctx, path)` wraps.

The pattern every test uses:

```
ctx->SetRef("##setup");                  // top-level window
GuiTest::FocusChild(ctx, "setup_card");  // descend into the child
ctx->ItemClick("Load");                  // items are now plain names
```

Nested children work the same way with a multi-segment path, e.g.
`FocusChild(ctx, "main_view/##timeline_dock")` from ref `##main`.

Two further rules learned the hard way:

- **Never use the `//` absolute prefix here.** With an empty base ref the engine
  concatenates to `"///##setup/..."` and the lookup fails with id 0. Always
  `SetRef(window_name)` first and use relative paths.
- **Items are addressed by their full label including `##` suffixes.** The fps
  quick buttons are `"60##fps_60"`, the label combo rows are
  `"chorus   (frame 120)##lbl1"`. If a test breaks after a label edit, that is
  the gate working: the label is the contract.

Transport buttons are labelled with the Segoe Fluent Icons PUA glyphs from
`gui_icons.h`, so tests include that header and click `ICON_STEP_FWD` directly.
The id is the string, so this works whether or not the glyph has a font.

### 5.1 Four more id-scoping rules the suite depends on

- **A selected tab pushes its own id.** `BeginTabItem` calls `PushOverrideID`, so
  everything inside the Render tab lives at `##inspector_tabs/Render/<item>`, not
  at `<item>`. Same for the `mc_names_list` child inside the Live tab. Every
  inspector test opens the tab first and then uses the prefixed path.
- **`Gui::Segmented` nests two id levels.** It does `PushID(id)` then `PushID(i)`
  per button, so the second option of the root-loop control is
  `##root_loop/$$1/Force loop`. `$$N` is the test engine's syntax for an integer
  id pushed with `PushID(int)`.
- **`ComboClick` splits its path at the FIRST slash**, so it only works for
  `combo/item` - it cannot reach a combo that is itself nested (inside a tab,
  say). `GuiTest::ComboPick` does the same job the long way: click the combo by
  full path, then click the entry inside `//$FOCUSED`. Use it for anything
  deeper than one level.
- **`ColorEdit3` with `NoInputs` submits its swatch as a child id.** The item to
  query is `##exp_bg_color/##ColorButton`, not `##exp_bg_color`.

### 5.2 Tree nodes that only open on their arrow

Scene-tree child rows use `ImGuiTreeNodeFlags_OpenOnArrow`, and clicking the
label deliberately selects instead of expanding. The engine's `ItemOpen` clicks
the item CENTRE, which for these nodes does nothing, so it reports "Unable to
Open item". `GuiTest::ClickTreeArrow` hovers the item first (which also sets the
mouse viewport - `MouseMoveToPos` with a raw position is a no-op without it),
then clicks at `RectFull.Min.x + fontSize/2`, which is inside ImGui's arrow hit
box.

### 5.3 Panel-local statics leak between test cases

The panels keep filter buffers, the selected main view, the export settings and
the resolution-combo index in file-scope statics, exactly as the running app
does. A `Harness` resets `App::State` but cannot reset those. Two consequences,
both handled in the suite rather than papered over:

- Tests that type into a filter (`##ifsfilter`, `##scene_filter`) clear it again
  before finishing, and tests that need the unfiltered tree clear it on entry.
- Tests whose assertion depends on a specific setting (export format, resolution
  preset) set that setting explicitly instead of trusting the default; and each
  scene-panel test loads a UNIQUE ifs path, because `Scene::Reset` only fires
  when the active ifs actually changes, and without that the selection from the
  previous test survives.

### 5.4 `Yield` is a Windows macro

`winbase.h` defines `Yield()`, so any translation unit that reaches `windows.h`
(the harness does, via `native_dialog.h`) cannot call `ctx->Yield(2)`. The
harness `#undef`s it after its includes.

## 6. Native dialogs are stubbed

`NativeDialog::SetOverrides()` installs four function pointers consulted at the
top of `BrowseForFolder`, `RevealInFileManager`, `OpenFile` and `SaveFile`. Without
this a test that clicks **Browse...**, **Import...** or **Export...** would open a
real modal dialog and hang CI forever, and **Open folder** would spawn Explorer on
the runner.

The harness installs stubs in its constructor and clears them in its destructor.
`GuiTest::SetBrowseResult()` seeds what the folder picker "returns" (empty string
means the user cancelled, which is a case worth testing), `SetOpenFileResult()` and
`SetSaveFileResult()` do the same for the file dialogs, and
`GuiTest::TakeRevealedPath()` reports what Explorer would have been asked to select.
The library tests point them at files under a temp directory, so an import or export
reads and writes real files with no dialog on screen.

The preset library's scan root is injected the same way:
`Panels::PresetLibrary::SetUserRoot()` replaces `presets/` next to the executable
with a temp folder, so a test can put a user document on disk and expect a User row
without touching the developer's own presets.

## 7. Assertions: two frameworks, two threads

The test engine runs `TestFunc` on a coroutine thread. Catch2 assertion macros
are not thread-safe there, so the division is strict:

- **Inside `TestFunc`**: only `IM_CHECK*` macros (engine-native, recorded into
  the test log).
- **After `harness.Run(test)`**: Catch2 `CHECK` / `REQUIRE` against `App::State`.

`Harness::Run` queues one test, pumps frames until the queue drains or a 4000
frame budget is hit (an infinite-wait guard), attaches the engine's log via
Catch2 `INFO` so a failure prints the exact `ItemClick` that could not resolve,
then requires `ImGuiTestStatus_Success`.

## 8. State isolation between tests

`App::Global()` is a process-wide singleton and the panel files hold static
state (`g_dir_buf`, the resolution combo's `shown_idx`, `g_main_view`). Each
`Harness` constructor resets every `App::State` field it touches and drains the
command queue, and each destructor tears the ImGui context down, so tests do not
depend on declaration order. The panel-local statics re-sync from state on the
next frame (`SyncDirBufFromState` compares against the last observed value).

The reset also clears `App::PresetStatus` and closes `Editor::Global()`, because
both outlive an ImGui context: a document left loaded by one case made
`Panels::Timeline::Active()` true in the next one, which drew the timeline editor
over an AFP test and pushed a `ReplaceDocument` command in front of the command the
test was reading. `BootLifecycle::SetLoadProgress` clears the minimum-hold timestamp
for the same reason - without that, the loading overlay raised by one case's preset
scan was still "active" for the first frames of the next case.

One deliberate side effect: `PersistSetup` calls `App::SaveCurrentSettings()`,
which writes `settings.ini` next to the running executable. For `gui_tests` that
is `build/settings.ini`, never `bin/settings.ini`, so the developer's real
settings are untouched.

### 8.1 Waiting on a background job: never spin on `ctx->Yield()`

The `.arc` and customize extractors run on a detached `Support::FolderJob`
thread. A test that clicks the button and then wants the finished status must
wait on WALL-CLOCK TIME, outside the ImGui test func:

```cpp
REQUIRE(WaitForJob([] { return ArcExtract::IsRunning(); }));
ArcExtract::Status const st = ArcExtract::GetStatus();
```

`WaitForJob` polls `IsRunning()` with a 1 ms sleep against a 30 s deadline. Both
halves matter, and the earlier version had neither:

- **A frame count is not a timeout.** The original wait was
  `for (int i = 0; i < 600 && IsRunning(); i++) ctx->Yield();`. Against a null
  backend 600 frames is tens of milliseconds, not the seconds the name suggests.
  It passed on a developer machine and failed on a GitHub runner, where thread
  start plus a directory scan of an empty temp folder took longer than that. The
  test then read a status the worker had not published yet and failed on
  `CHECK_FALSE(st.running)` / `CHECK(st.finished)`, with the extractor's own
  "done" log line landing AFTER the Catch2 failure output - the tell that the
  worker was still alive.
- **Spinning starves the thread you are waiting for.** `ctx->Yield()` renders a
  frame as fast as the CPU allows; on a two-core runner that competes directly
  with the worker. The sleep is what lets it finish.
- Do the wait AFTER `harness.Run(test)` returns, not inside `TestFunc`. The
  status is a mutex-guarded struct written by the worker, so no ImGui frames are
  needed for it to progress, and a wait inside `TestFunc` would eat the harness's
  4000-frame budget for nothing.

The ordering the wait relies on is real: `FolderJob::Start` sets the atomic
BEFORE spawning the thread (so `IsRunning()` is already true when `Run` returns),
and the worker calls `Publish(finished)` BEFORE `Finish()` clears the atomic (so
`IsRunning() == false` guarantees the terminal status is visible). Waiting on the
atomic and then reading the status is therefore race-free; waiting on
`st.finished` directly would not be, because a job that never starts never
publishes.

## 9. Coverage

One file per panel or editor surface. The counts below are the panels the first
version of this suite covered; the scene preset editor files (`timeline_tests`,
`timeline_editor_tests`, `clip_modal_tests`, `tween_ui_tests`, `library_tests`,
`frame_inspector_tests`) grew with milestones M4 to M7:

| file | cases | what it drives |
|---|---|---|
| `setup_view_tests.cpp` | 13 | game-dir input, Browse accept/cancel, the disabled Load gate, fps quick buttons, profile combo, resolution preset + custom clamping, the BootGame payload, the DDR-only arc and customize extractors (visibility, cancel, and a real run over an empty temp dir) |
| `shell_tests.cpp` | 13 | setup-vs-ready view swap, top-bar Export (enabled + disabled), the per-profile main-view switch, all three backend tab sets, tab switching, splitter drag, status-strip export tag and reveal button |
| `browse_panel_tests.cpp` | 8 | empty catalog, scanning state, selection posting LoadContent, the `from_arc` flag, the no-reload-of-active guard, filter, directory grouping, expand/collapse |
| `scene_panel_tests.cpp` | 13 | layer double-click, child visibility override, child expansion, filter, add-slot (accept, empty, duplicate), slot visibility, bitmap combo pick + restore-default, the text-field fallback, unresolved slots |
| `inspector_tests.cpp` | 16 | Play/Replay, loop-master, root-loop and continuous-loop segmenteds, trim, master-scale slider + both presets, background segmented, filter and MC-names toggles, name-type segmented, reset overrides, the DDR-reduced Render tab, the Live tab's MC list |
| `timeline_tests.cpp` | 12 | jump back, resume, track drag-to-seek, the zero-length guard, label ticks, Space, arrows, Shift+arrows, Ctrl+E, shortcut suppression during capture, the no-scene and no-labels states |
| `ready_view_tests.cpp` | 7 | transport play/pause, step 1, step 100, wrap at frame 0, label combo, status-strip reveal |
| `export_panel_tests.cpp` | 22 | stem + format to output path, PNG directory form, fps/quality, resolution preset/native/custom/scale buttons, transparent-bg toggle and its colour gate, hardware-encode gate, keyframe interval, frame limit, loop count, blend seam, crop inputs/Clear/Pick/reopen, Start, Cancel, Close, reveal |
| `qpro_panel_tests.cpp` | 11 | scan command, category gating, extract payload, picker cancel, fps clamp, scan-error state, the per-date part groups, All / None, a group checkbox, the issue list's clipboard copy |
| `host_panel_tests.cpp` | 7 | 3D-scene and 2D-package panels while idle, the loading overlay over both views and its absence, the boot-error banner, Load disabled during boot |
| `host_live_tests.cpp` | 7 | the 3D and 2D panels driving REAL loaded hosts: pause, speed, playhead, animation combo, animate-models, free camera, reset view, move speed, model visibility and blend mode, and the camera-checkbox gates |
| `library_tests.cpp` | 22 | the preset library: the library owning the CENTER pane on the scene3d backend with nothing of it left under `main_view/pane_left`, an AFP backend keeping `##scene_filter` / `scene_scroll` in the centre with no `###lib_*` item anywhere, the `lib_scroll` list taking more than 40 percent of the pane height, Built-in / User / Other builds groups from a temp scan root, selecting a row loading the document and posting `PresetCmd::LoadDocument`, an unparseable import reporting its line and column and loading nothing, an import with a validation error loading and listing it, an import whose id is taken keeping the file that owns it, export then import byte-equal, Save writing `presets/<build>/<id>.json` and clearing the modified mark, Ctrl+S, the unsaved-changes prompt, New generating the id from the name, Duplicate, a built-in offering a user copy instead of saving in place, the problem list staying inside a short pane, an other-build row opening read-only with no `LoadDocument` posted and every editing control disabled, Duplicate turning it into an editable copy for this build, an unloadable user file appearing under "Files that did not load", and a close request prompting when dirty (Cancel keeps running, Discard confirms) but exiting straight away when clean |
| `frame_inspector_tests.cpp` | 2 | the Inspector "Frame" tab naming the clip that won a conflicting value, and clicking that winner selecting the clip |
| `window_tests.cpp` | 10 | `Gui::Init` device creation, the min-track-size clamp, `WM_ERASEBKGND`, `WM_SYSCOMMAND`/SC_KEYMENU, live resize, `WM_PAINT` + validation, the pump's WM_QUIT exit, lost-device recovery, and two BACKBUFFER PIXEL assertions |

Every interactive widget in `src/gui` is now exercised. The four gaps the first
version of this document listed are all closed; sections 12 to 14 record how.

The one thing still out of scope is **golden-image comparison**. The pixel
assertions here are structural (see 14), not reference-image diffs; the
image-diff nets remain `pixel_golden_tests` and `docs/local_regression.md`.

## 10. Two defects the suite found on its first run

Both were fixed in the same change; they are recorded here because each is a
bug class that will recur.

1. **`ImGui::IsItemToggledOpen()` queried too late** (`gui_scene_panel.cpp`).
   `RenderSceneNode` called it several items after the `TreeNodeEx`, past the
   `TextDisabled` that prints a clip's `(x, y)` position and past the variant
   badge. `IsItem*` reads `g.LastItemData`, which every subsequent `ItemAdd`
   overwrites - so expanding any child clip THAT HAS A POSITION never recorded
   its expansion, while a bare clip worked. The fix latches
   `const bool toggled_open = ImGui::IsItemToggledOpen();` immediately after the
   `TreeNodeEx` and uses the latched value. Rule: capture `IsItem*` results on
   the line after the widget, never further down.
2. **Truncated button id** (`gui_export_panel.cpp`). The export scale buttons
   build `"%.*s (%dx%d)##exp_scl_%s"` into a 32-byte buffer; the real string is
   37+ bytes, so every id was silently cut short. The visible text was fine
   (it precedes the `##`), which is why nobody noticed - but ids that differ only
   past the cut would collide. Buffer raised to 64.

## 11. How to add a case

Pick the view, prefill `App::State` for the scenario, `SetRef` + `FocusChild` to
the owning child window, act, then assert on the drained command or the mutated
state. If the widget calls something outside `App::State` (a background worker,
a native dialog), give it a seam like `NativeDialog::Overrides` rather than
letting the test touch the real thing. If it depends on a panel-local static,
set that static through the UI first (see 5.3) instead of trusting the value the
previous test happened to leave behind.

## 12. Synthetic game assets (`gui_mock_assets`)

The 3D scene and 2D package panels only render their bodies once
`Scene3dHost::Active()` / `Gc2dHost::Active()` are true, which needs a real
`Load()` over real files. `gui_mock_assets.cpp` builds those files byte by byte
into a `TempAssetDir` (created under the OS temp dir, removed in its destructor),
so no game data is required and nothing is committed.

Both loaders read LZSS-compressed payloads, and the compressor is trivial to
write for the all-literal case: a 4-byte little-endian uncompressed size, then a
`0xFF` flag byte before every group of 8 literals. `tests/formats/model3d_tests.cpp`
already used the same trick; this is the second consumer.

- **2D package** (`WriteMock2dPackage`): a `system.idx` in the two-chunk
  little-endian form - `[u32 size0][chunk0][u32 size1][chunk1]`. Chunk 0 carries
  the 0x1B8-byte fixed header (texture count at 0x002, cell-table offset at
  0x004, record-table offset at 0x010, 32-byte texture path slots from 0x014),
  then two cells terminated by a zero-width cell, then five 36-byte records:
  draw, end-animation (`t_end` 30), draw, end-animation (`t_end` 45),
  end-table. Chunk 1 holds the three consecutive name tables the parser expects
  (cells, an empty unused table, animations), each a run of NUL-terminated names
  each followed by a `u16` index and closed by a `0` byte. The animation start
  indices are what make `anim_intro` 30 frames and `anim_loop` 45, which is
  exactly what the panel's frame slider is asserted against.
- **Textures** are 24-byte GC headers (`"GC "` magic, big-endian origin, size and
  declared byte count, depth byte at 0x13) followed by opaque 16bpp pixels.
- **3D scene** (`WriteMock3dScene`): a `.inz` manifest (one image slice plus one
  pattern rect, so `TileForTexture` resolves), the `0.gcz` tile that slice names,
  and a `.xz` model - plain `xof 0303txt` text with a frame hierarchy, one
  textured triangle, `MeshTextureCoords`, and an `AnimationSet` whose last key is
  at tick 600 (which is what `max_time == 600` asserts). Passing
  `with_camera = true` adds a frame whose name contains `camera`, because
  `Scene3d::FindCamera` matches on that substring - that is how the
  **Animate camera** checkbox's enabled and disabled branches are both reached.

## 13. The WARP GPU fixture and the qpro seams

`GuiTest::WarpGpu` creates a D3D9-on-12 WARP device (the same
`src/warp_device.cpp` `pixel_golden_tests` uses) and points `g_d3d.device` at it
for the lifetime of the test, restoring the previous pointer afterwards. That is
all `Scene3dHost::Load` / `Gc2dHost::Load` need in order to upload their tile
textures and flip `Active()`.

The qpro part list needed two publish seams, both introduced as refactors with
production callers rather than test-only entry points:

- `QproExtract::PublishScanResult(parts, error)` collapses the two duplicated
  lock-and-store blocks at the end of `RunScan` into one function; the tests call
  it with synthetic `ScanPart`s carrying two different dates, which is what makes
  the per-date groups, `All`, `None` and the group checkbox reachable.
- `QproExtract::PublishStatus(status)` is what `BeginRunStatus` now uses to
  install its fresh status; the tests use it to stage a finished run with issues
  so the **Copy list** button renders.

Note that the clipboard assertion reads `ctx->Clipboard` from inside `TestFunc`,
not the ImGui platform IO: the test engine swaps its own clipboard handlers in
while a test runs, so anything the app copies lands in the engine's per-test
buffer, and a handler installed by the harness would never be called.

## 14. The window and pixel tests

`window_tests.cpp` drives the real `Gui::Init` / `PumpAndRender` / `Gui::Shutdown`
against a real Win32 window and D3D9 device, so it covers the message handling
that `Panels::Build`-only tests cannot: the `WM_GETMINMAXINFO` clamp against
`gui_layout_constants.h`, `WM_ERASEBKGND` returning 1, the SC_KEYMENU swallow,
`WM_SIZE` servicing a resize from inside the handler, `WM_PAINT` validating its
region, the pump's WM_QUIT exit, and the lost-device recovery path.

Two contracts this exposed:

- **`Gui::Shutdown` leaves a WM_QUIT in the thread queue** (`DestroyWindow` ->
  `WM_DESTROY` -> `PostQuitMessage`). That is correct for the app - the GUI
  thread is meant to end when the window closes - but it means a second
  `Gui::Init` on the same thread starts with a quit already pending, and the very
  next `PumpAndRender` returns false. The fixture drains the queue around every
  window, which is what any code that restarts the GUI would also have to do.
- **`Gui::Init` now falls back to WARP** when `Direct3DCreate9` cannot produce a
  HAL device (see docs/gui.md section 1.3). That keeps the control panel usable
  on machines and VMs without a D3D9 HAL driver, and it is what lets these tests
  run where no HAL device exists.

The pixel assertions read the backbuffer through `GetRenderTargetData` into a
system-memory surface and count matching pixels:

- the setup view must actually paint the shell clear colour over a large part of
  the frame (a blank or unpresented frame fails);
- the ready view must contain the CURRENT profile's accent button colour and NOT
  the other profile's. The expected colour is read from
  `ImGui::GetStyleColorVec4(ImGuiCol_Button)` at capture time rather than
  hardcoded, because `ApplyAccentColors` MIXES the accent with the surface colour
  (`Mix(kRaised, accent, 0.28)`), so the raw accent from the profile table never
  appears on screen. Deriving it from the live style keeps the test honest
  without freezing the palette. The scene must be marked loaded first, otherwise
  the only accent-coloured button in the top bar is the disabled Export button,
  whose alpha blend puts it off the exact colour.

## 15. Running the GPU-backed tests

`WarpGpu`, `LiveWindow` and `pixel_golden_tests` all SKIP (Catch2 `SKIP`, ctest
reports "Skipped") when no D3D9 device can be created, so the suite is green on
machines and CI legs without one.

That skip also triggers in any shell that cannot reach the graphics stack - a
sandboxed or non-interactive parent process will see
`IDirect3D9::CreateDevice ... hr=0x88760868` for both HAL and WARP while the same
binary succeeds when launched normally. If the GPU-backed cases skip
unexpectedly, run the executable detached before concluding anything about the
code:

```
powershell -Command "Start-Process -FilePath .\build\gui_tests.exe -Wait -RedirectStandardOutput out.txt"
```

With a device present the whole suite is 192 cases / 761 assertions and nothing
skips (`cli_tests` adds 29 cases / 162 assertions).

## 16. Asserting tooltip TEXT, not just presence

`GuiTest::HoverShowsTooltip` only answers "did a `##Tooltip` window appear". That
is enough for a tooltip whose text is a constant, but several tooltips change
what they say depending on state - the hardware-accel note names a different
reason per format, the animate-camera note names a different reason for each way
it can be greyed out, the export status tag carries either the output path or
the encoder error. Presence alone would pass while the wrong branch renders.

`GuiTest::HoverAndCaptureText` hovers an item and returns the text imgui
actually rendered that frame, so the test can assert the branch:

- The harness owns the capture because the log has to be armed inside its frame
  loop: `ImGui::LogToBuffer()` right after `NewFrame()`, `Panels::Build()`, then
  read `GetCurrentContext()->LogBuffer` **before** `ImGui::LogFinish()`.
  `LogFinish` clears the buffer, so reading after it always yields an empty
  string. That ordering bug cost a debugging cycle.
- `GuiTest::CaptureFrameText` is the same capture without the hover, for
  tooltips owned by an item that has no id and therefore no addressable path.
  Two exist: the timeline label tick (drawn on the `##tl_track` invisible
  button, hit-tested by mouse x) and the qpro "Animated parts ->" note
  (`ImGui::TextDisabled`, id 0). Both are reached with `MouseMoveToPos`. For the
  qpro note the y is derived from the preceding `Output fps` item rect plus
  `2 * ItemSpacing.y + FontSize * 0.5` - that is exactly what `ImGui::Spacing()`
  followed by one text line advances, so it does not drift with the style.
- `MouseMoveToPos` does not set the mouse viewport. Call `MouseMove` on any item
  in the target window first, as `ClickTreeArrow` does.

The suite asserts every `ImGui::SetTooltip` call site in `src/gui` (58 sites at
the time of writing: 22 export modal, 9 inspector, 7 3D scene, 6 shell/status
strip, 4 timeline, 4 qpro, 3 setup, 2 scene pane, 1 2D package). Branch-carrying
tooltips are driven through each branch rather than once. If you add a
`SetTooltip`, add its assertion in the same change - `grep -rn SetTooltip src/`
is the checklist.
