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

`NativeDialog::SetOverrides()` installs two function pointers consulted at the
top of `BrowseForFolder` and `RevealInFileManager`. Without this a test that
clicks **Browse...** would open a real modal folder picker and hang CI forever,
and **Open folder** would spawn Explorer on the runner.

The harness installs stubs in its constructor and clears them in its destructor.
`GuiTest::SetBrowseResult()` seeds what the picker "returns" (empty string means
the user cancelled, which is a case worth testing); `GuiTest::TakeRevealedPath()`
reports what Explorer would have been asked to select.

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

One deliberate side effect: `PersistSetup` calls `App::SaveCurrentSettings()`,
which writes `settings.ini` next to the running executable. For `gui_tests` that
is `build/settings.ini`, never `bin/settings.ini`, so the developer's real
settings are untouched.

## 9. Coverage

117 test cases across ten files, one file per panel:

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
| `qpro_panel_tests.cpp` | 6 | scan command, category gating of the extract button, the extract payload, picker cancel, fps clamp |
| `host_panel_tests.cpp` | 7 | 3D-scene and 2D-package panels while idle, the loading overlay over both views and its absence, the boot-error banner, Load disabled during boot |

Every interactive widget in `src/gui` is exercised except the four cases below.

**Not coverable headlessly, and why:**

- **The 3D scene panel and 2D package panel bodies.** `Scene3dHost::Active()` /
  `Gc2dHost::Active()` only become true after `Load()` builds renderer resources
  on `g_d3d.device`, so a live D3D9 device plus real game data is required. The
  tests cover the idle branch (the "no scene loaded" copy and the hidden tab).
  A WARP device (as `pixel_golden_tests` uses) plus synthetic `.x` / `system.idx`
  fixtures could reach the rest; that is a separate piece of work.
- **The qpro part list: `All`, `None`, the per-date groups, and `Copy list`.**
  Those widgets only render after `QproExtract::GetScanResult()` returns parts,
  which comes from pattern-scanning a real `bm2dx.dll`. There is no injection
  seam and adding one would be production code with only a test consumer.
- **`gui_window.cpp`'s Win32 message pump**, the modal border-drag loop and the
  DX9 lost-device dance. Those need a real window; they stay manual.
- **Rendering itself.** These tests assert behaviour, not pixels. The pixel nets
  are `pixel_golden_tests` and `docs/local_regression.md`.

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
