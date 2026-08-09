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

## 9. Coverage today and how to extend

Covered: the setup view (game dir input, Browse accept + cancel, the disabled
Load gate, fps quick buttons, profile combo, resolution preset combo, custom
resolution clamping, the BootGame payload) and the ready view (transport
play/pause, step 1, step 100, wrap-around at frame 0, the label combo, the
no-scene hint, and the status strip's export reveal).

Not covered, and not coverable this way: `gui_window.cpp`'s Win32 message pump,
the modal border-drag path, and the DX9 lost-device dance. Those need a real
window and stay manual.

To add a case: pick the view, prefill `App::State` for the scenario, `SetRef` +
`FocusChild` to the owning child window, act, then assert on the drained
command or the mutated state. If the widget you are testing calls something
outside `App::State` (a background worker, a native dialog), give it a seam like
`NativeDialog::Overrides` rather than letting the test touch the real thing.
