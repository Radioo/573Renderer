# GUI architecture and debug-viewer parity

Notes on `src/gui/*`. Wherever a control maps to an engine call, the GUI mirrors KONAMI's own
in-game AFP debug viewer (`CAfpViewerScene` in bm2dx). Those mappings are engine facts and are
listed in section 5.

The current layout is the "Timeline Studio" direction with the "Scene graph" center pane. It was
chosen over a control-deck grid, an embedded-monitor center and a workspace with no right pane.
The squared styling (all rounding 0) and the export modal were decided in that same round; they
are not defaults.

## 1. Threading model

### 1.1 Dedicated GUI thread (gui_thread)

The GUI runs on its own thread, separate from the render thread. Dragging a Win32 window by its
title bar (or resizing it by an edge) enters a modal move loop inside DefWindowProc that returns
only on mouse-up, and everything else on that thread stalls until then, including the render
loop and AFP's per-frame tick. With a separate GUI thread, only the GUI thread is stuck in the
drag and the render thread keeps its cadence.

- The two threads communicate only through `App::State` (mutex-guarded). Each thread owns its
  own window, D3D9 device and ImGui context, and no ImGui or D3D9 object is shared across threads.
- The GUI thread exits when (a) the user closes the GUI window, after which the renderer keeps
  running headless (the GUI is a control panel, not the app), or (b) `App::Global().ShouldExit()`
  flips (render window closed). main.cpp joins the GUI thread before final cleanup.
- The panel loop is limited to ~60 Hz via Sleep(16). It does not need the renderer's 120 Hz,
  and an idle ImGui loop would otherwise burn a core.
- `GuiThread::Start` is idempotent (atomic exchange). `Stop` flips the local running flag and
  joins, so the GUI can stop on its own without tearing the whole app down.

### 1.2 Command/Status pattern

GUI widgets never call the engine. They post typed `App::Command` variants
(`Cmd::LoadContent`, `Cmd::BootGame`, `Cmd::StartExport`, `Cmd::CancelExport`) and wrapped
AFP-backend commands (`AfpCmd::Wrap` over `SeekFrame`, `SetPaused`, `SwitchAnimation`,
`GotoLabel`, `ForceReplay`, `QproStartScan`, `QproStartExtract`) that the
render thread consumes (see docs/state.md "Command semantics"). The render thread publishes
`App::Status` / LiveState / ExportState / LoadProgress snapshots that the GUI polls once per
frame. This seam also makes the GUI testable headless: `gui_tests` prefills `App::State`,
clicks a widget through the Dear ImGui Test Engine, and asserts on the command that came out,
with no window, no device and no game data (docs/gui_tests.md). Keeping widgets free of direct
engine calls is therefore needed for testing as well as for tidiness.
Background workers (arc/customize extractors, qpro scan) publish into their own
mutex-guarded Status structs polled the same way.

### 1.3 gui_window internals (Win32 + DX9 lost-device dance)

- The modal border-drag loop runs inside DispatchMessage and blocks PumpAndRender until
  mouse-up, so live resize has to be handled inside the message handler: WM_SIZE queues a
  backbuffer resize and renders one frame immediately (`RenderFrameLocked`, guarded against
  re-entry) so the panes track the cursor during the drag. WM_PAINT renders a real frame plus
  ValidateRect; WM_ERASEBKGND returns 1 (skips the GDI erase, which avoids resize flicker).
  SIZE_MINIMIZED reports a 0x0 client, which must not be used for a reset.
- Device fallback: `CreateDevice` tries a D3D9 HAL device (hardware, then software vertex
  processing) and, if both fail, falls back to a D3D9-on-12 WARP device on the same HWND
  (`WarpD3D9::CreateForWindow`, the same path `pixel_golden_tests` uses). Without it the
  control panel would not start on machines with no D3D9 HAL driver: VMs, some RDP sessions,
  bare Windows Server. `Window::warp_backed` records which path was taken so `Shutdown` does
  not release a device twice when the `WarpD3D9::Device` member owns it. The fallback also lets
  `window_tests` run the real window on hosts without a HAL device (docs/gui_tests.md 14).
- `Gui::Shutdown` destroys the window, so `WM_DESTROY` posts a WM_QUIT to the thread queue.
  That ends the GUI thread when the user closes the panel, but it also means any code that
  calls `Gui::Init` again on the same thread must drain the queue first, or the next
  `PumpAndRender` sees the stale quit and returns false immediately.
- `ResetDevice` re-reads the client rect into the present params every time, so a device loss
  after a resize restores at the current size instead of snapping back to the creation size.
  ImGui's font atlas and buffers live in D3DPOOL_DEFAULT, so
  `ImGui_ImplDX9_InvalidateDeviceObjects` runs before Reset and `CreateDeviceObjects` after
  (the standard DX9 lost-device sequence).
- WM_GETMINMAXINFO clamps the minimum window size to the layout constants. It fires during
  CreateWindow while the module global is still null, which is fine because the constants are
  static.
- io.IniFilename = nullptr (window layout not persisted). The GUI HWND is exposed via
  `Gui::GetHwnd()` for native dialog parenting.

## 2. Style, fonts, icons (gui_style, gui_icons, gui_widgets)

- Square corners everywhere: every rounding in the style struct is 0 (window, frame, grab,
  scrollbar, tab, popup, child). Only popups and child cards have hairline 1px borders; other
  surfaces are separated by four fill levels (ground / raised / overlay / sunken).
- Fonts (all loaded from `%WINDIR%\Fonts` via GetWindowsDirectory, never hardcoded):
  - base = Segoe UI 16px (fallback Consolas, then ImGui default),
  - header = Segoe UI Semibold 18px (`seguisb.ttf`, fallback bold, fallback base),
  - mono = Consolas 13px for everything that changes per frame (playhead, flags, hex ids) so
    digits do not jitter,
  - icons = Segoe Fluent Icons merged into the base atlas from the system font
    (`SegoeIcons.ttf`, fallback `segmdl2.ttf` on Win10), PUA range 0xE700-0xE950,
    `ImFontConfig::MergeMode`. No assets are bundled, so there is nothing to license. Glyph
    escapes live in `gui_icons.h`; only icons with a live consumer are defined there
    (dead-code rule).
- imgui is 1.92+: `PushFont` takes (font, size). `Gui::PushHeaderFont()` / `PushMonoFont()`
  wrap `PushFont(f, f->LegacySize)` so call sites stay one line. Fonts are added before
  `ImGui_ImplDX9_Init` (the backend uploads the atlas on init).
- Glyph ranges: 0x0020-0x00FF plus 0x2010-0x2027 / 0x2030-0x205E (quotes/ellipsis); the
  range arrays are static (ImGui keeps the pointer).
- Per-game accent: `Gui::ApplyAccentForProfile(slug)` re-derives every accent-tinted color
  (buttons, headers, tabs, checkmarks, sliders...) from one accent per game-profile slug
  prefix: sdvx cyan, iidx blue, ddr gold, gitadora red, jubeat silver, default blue.
  `Panels::Build` calls it once per frame behind a change guard, so the window always shows
  which game is loaded. Neutral surfaces never change; only the accent mix does.
- Shared widgets (`gui_widgets`): `Segmented` (row of mutually exclusive small buttons; used
  for root-loop, continuous-loop, background, MC-name-type, and the main-view switch) and
  `SectionHeader` (icon + header-font title + dim suffix + separator).

### 2.1 High-DPI scaling (gui_dpi)

The control panel scales with the monitor's DPI like any other Windows app: text, padding,
panes, modals and every drawn decoration grow together, so a 4K screen at 150% or 200% Windows
scaling shows the same layout at a readable size instead of a tiny one.

The awareness context is raised for the GUI thread only. `WinMain` calls `SetProcessDPIAware()`,
which makes the process system-DPI-aware, and that has to stay: the renderer window
(`AppWindow::Create`) sizes its client area with `AdjustWindowRect` and asserts a 1:1
client-to-backbuffer match, which DPI virtualization would break. So
`Gui::Dpi::MakeThreadPerMonitorAware()` calls
`SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` as the first
statement in `Gui::Init`. Because the GUI owns its own thread (1.1) and creates its window
there, only that HWND inherits the awareness context; the render window is created on the
render thread and keeps the process default. The two DPI APIs are resolved through
`GetProcAddress` on user32, so the binary still loads on a Windows without them and falls back
to the desktop DC's LOGPIXELSX.

The scale: `Gui::Dpi::Scale()` is `dpi / 96`, clamped to [1, 4], set from
`GetDpiForWindow(hwnd)` right after the window is created and set again from `HIWORD(wParam)`
on `WM_DPICHANGED` (which arrives only because of the per-monitor context). It is a plain float
read on the GUI thread; tests set it with `Gui::Dpi::SetScaleFromDpi(dpi)` before building a
context.

Two settings in `Gui::ApplyStyle()` do most of the work:
- `style.FontScaleDpi = Scale()`: imgui 1.92's dynamic font system re-rasterizes every glyph
  at the scaled size, so the text is sharp instead of stretched. `ImFontConfig::GlyphOffset`
  and `GlyphMinAdvanceX` (the merged icon font) are documented as relative to the default size
  and scale with it, so the icon alignment follows automatically.
- `style.ScaleAllSizes(Scale())`: every padding, spacing, scrollbar and grab metric.
  `ApplyStyle` therefore starts with `s = ImGuiStyle()`, so applying it again on
  `WM_DPICHANGED` scales from the unscaled baseline instead of compounding. That reset also
  clears the accent cache (`g_applied_accent_slug`), so the per-game accent is re-derived on
  the next frame.

Every other pixel literal in `src/gui/` is a design unit (DIP) at 96 DPI and is multiplied
where it is used with `Gui::Dpi::S(x)` (or `S(x, y)` for an `ImVec2`). Named constants keep the
raw number as `k...Dips` and expose a scaled accessor (`RulerH()`, `LabelWidth()`,
`OptionRowH()`, ...) so call sites read the same as before. When you add a widget, a drawn
decoration or a modal width, wrap the number; the tests below check the result.

View state is stored in DIPs, not pixels. `Editor::View::header_w` and `View::height`, and the
`RenderRendererView` pane widths, hold design units; the GUI multiplies on the way to the
screen and divides the mouse delta on the way back. That keeps the editor layer free of DPI
(it has to be: `check_gui_isolation.py` forbids ImGui outside `src/gui/`), keeps
`kHeaderWidthMin/Max` and `kEditorHeightDefault` meaningful at any DPI, and means a live DPI
change does not leave the panes at a stale pixel width. The timeline zoom (`px_per_frame`) is
not scaled, on purpose: like the scroll position it is a view setting the user picks, not part
of the chrome.

Pixel quantities the editor layer owns are parameters, not constants. `Editor::RulerStep`
takes a `label_gap`, `Editor::ZoneAt` takes a `handle_px`, `Editor::DragInput` carries
`snap_px` / `label_gap`, and `LaneMetricsFor` / `LaneLabelY` take a `scale`. The GUI passes
`Gui::Dpi::S(Editor::kLabelGapPx)` etc; the editor's own tests pass the unscaled constants.
Tick density has to grow with the text it must not collide with, and the snap and grab
tolerances are mouse-precision distances that would halve in physical terms at 2x if left
unscaled.

The window: `Gui::Init` creates the window at the unscaled default, then `SizeToDpi` resizes
it to `S(kDefaultWindowW/H)`, clamped to the monitor work area and moved so it cannot hang off
the right or bottom edge (at 200% the default 1360x820 would otherwise want 2720x1640).
`WM_GETMINMAXINFO` clamps the minimum to `S(kMinClientW/H)`. `WM_DPICHANGED` applies the style
again and moves and sizes the window to the RECT Windows suggests.

Limits: the scale comes from Windows, so a 4K panel left at 100% Windows scaling reports 96 DPI
and the UI stays small, as every other app on that machine does. There is no manual override.

Tests: `tests/gui/dpi_scaling_tests.cpp` checks the style and font scale factors and that the
top bar, status strip and timeline dock double in height at 192 DPI while still holding their
content. `tests/gui/modal_layout_tests.cpp` runs its whole "content fits inside the modal"
suite at 96, 144 and 192 DPI: `GuiTest::Harness(dpi)` sets the scale and a proportionally
larger `io.DisplaySize` before `LoadFonts()` / `ApplyStyle()`, and resets to 96 on teardown.
Those modal cases caught the original bug: with the fonts scaled and the modal widths fixed,
the Clip properties, Document properties, Option properties and Add track modals all
overflowed at 144 and 192.

## 3. Layout

Ready view = fixed shell, top to bottom:

1. Top bar: brand (header font, accent), main-view switch (only when >1 view: Renderer /
   qpro), active IFS path (mono), then the right-aligned Export button and the measured-fps
   readout.
2. Three panes split by two draggable `Gui::VSplitter`s:
   - left = Browse (IFS tree), center = the backend's main view, right = Inspector (tabs).
   - The left pane holds only Browse on every backend. The center pane is chosen by the panel
     registry (`PanelSlot::CenterPane`): the AFP backends register nothing there and get the
     Scene layers view (clip hierarchy, `RenderScenePane`); the scene3d backend registers the
     Preset library (3.6), which then owns the whole center column. See 3.6 for why.
   - Side-pane widths last for the session only, and the center pane takes the remainder.
     When the window shrinks below fit, the center takes space from the right pane first,
     then the left.
   - `VSplitter` is built on `ImGui::InvisibleButton` only (no imgui_internal); a drag moves
     pixels symmetrically and commits only if both neighbours stay >= their minimum.
3. Timeline dock (fixed height `kTimelineH`): transport + custom track, section 4.
4. Status strip (`kStatusStripH`, mono font): profile name, render WxH, render health
   (last_error, tooltip carries the full text), the export status tag (clickable, reopens
   the export modal), right-aligned afp version. Once an export finishes, an "Open folder"
   button appears beside the status tag and inside the export modal; both call
   `NativeDialog::RevealInFileManager`, which uses `SHOpenFolderAndSelectItems` to open
   Explorer with the output selected, and falls back to opening the parent directory when the
   item cannot be resolved. Relative export paths are made absolute first, and a PNG-sequence
   export (which writes a directory) is selected the same way.

- Layout constants live in `gui_layout_constants.h` so WM_GETMINMAXINFO and the pane layout
  cannot drift. Pane minimums: left 240, center 320, right 280; defaults 300/340; splitter 6.
  All of them are design units at 96 DPI (see 2.1), and every use goes through
  `Gui::Dpi::S()`.
- Views: BootState WaitingForDir/Booting/Failed -> Setup view; Ready -> the shell above. The
  loading overlay renders on top of either view from the same LoadProgress API.
- Keyboard (active when no text input is focused): Space = play/pause, Left/Right = step 1,
  Shift+Left/Right = step 100, Ctrl+E = export modal. Handled in the timeline dock; steps are
  suppressed while an export captures.

### 3.0 Panel registry - backends register their panels

The main-view switch and the inspector tab set come from the registry
(`src/gui/panel_registry.{h,cpp}`): `PanelDesc{id, tab_label, slot, draw, visible}` entries in
per-backend `PanelSet` tables keyed by `App::State::ActiveBackendId`. Slots:

- `MainTab`: whole-shell views. afp_modern = Renderer + qpro (visible predicate
  `GetGameProfileSlug() == "iidx33"`); afp_ddr = Renderer only. With one visible entry the
  top-bar switch does not render.
- `InspectorTab`: right-pane tabs. afp_modern = Properties / Render / Live; afp_ddr =
  Render (reduced draw fn) / Live / 3D scene; scene3d = 3D scene / 2D package. The last two
  carry visible predicates (`Scene3dHost::Active()`, `Gc2dHost::Active()`) so only the tab for
  the kind of content that is loaded is present.

Rules kept from the original registry: membership in a set declares the capability (an
unsupported control is absent, never greyed out); ImGui stays entirely inside src/gui/
(isolation gate); a future backend adds a `PanelSet` row plus draw fns, with no shell changes.
The Scene pane and timeline are shared shell surfaces whose per-backend differences come from
data: DDR publishes no afplist names, no mc_tree and no slots, so those tree features do not
appear.

### 3.1 Browse pane (IFS picker)

- The IFS tree is rebuilt from the entry list every frame (linear in entries x depth, fine for
  ~6000 files). ImGui ID pitfall: pushing the entry pointer as the row ID broke clicking.
  The rebuild gives a different pointer each frame, so press (frame N) and release (frame N+1)
  had different IDs and ImGui never registered the click. The fix is PushID on the full path
  string.
- Auto-expand is keyed off the size of the whole tree (<= 20 files), not per subtree: a tiny
  install opens automatically while a real SDVX/IIDX install stays collapsed.
- The active-row highlight compares the full absolute path (Status::current_ifs_path), not the
  basename, because installs have same-named IFSes across subdirs.
- The .ifs directory scan runs on a background thread; while it runs, the pane shows the
  scan's live status line instead of "none found".
- On the scene3d backend the pane is the top half of the left column and the preset library
  (3.6) is the bottom half; both are separate child windows, so each scrolls on its own.
- File leaves post `load_new_ifs` requests carrying `from_arc` for DDR .arc-wrapped IFSes.
- Right-clicking a file leaf opens a context menu with "Load alongside". It posts
  `AfpCmd::LoadAlongside{path}` and mounts that IFS as an extra afpu package on top of the
  loaded one, so its bitmaps fill in names the main IFS does not carry (afpu lookup is
  last-loaded-wins; see docs/boot_and_render_loop.md "Companions"). Rows already loaded that
  way carry an accent `alongside` badge, and their menu offers "Unload"
  instead. The header area lists every loaded-alongside IFS with its own Unload button, from
  `Status::overlay_ifs`.
- The menu item is disabled (with a tooltip saying why) when nothing is loaded yet, when the
  backend is not `afp_modern` (only it has afpu packages), when the row is the active IFS, or
  when the row is a DDR .arc entry.
- Order pitfall: `BeginPopupContextItem` binds to the last submitted item, so the context menu
  must be opened directly after the row's `Selectable` and before the `alongside` badge's
  `SameLine`/`TextColored`. Anchoring it after the badge produced a menu that never opened,
  with no error (caught by the `browse_unload_alongside` GUI test).

### 3.2 Scene pane (gui_scene_panel) - the clip hierarchy

One tree replaces the old Layers + Sub-layers + Variants panels:

- Top level = afplist.xml `<afp>` entries (KONAMI's own debug viewer labels this exact list
  "[LAYERNUM]"). A single click selects (drives the Properties tab); a double-click plays or
  replays (same-name picks route through ForceReplay in the dispatcher, a valid replay).
- Under the playing layer: the live child tree from `afp_mc_enumerate_children`
  (Status::mc_tree), with per-clip visibility checkboxes, positions (when MC-name enumeration
  is on; the same data as F3), and an accent "variant" badge on nodes matched by a
  VariantSlot (`slot.path == node.path or node.name`).
- Sub-clip behaviour kept from the old sub-layers panel: expansion is lazy (opening a
  node writes its path into the State expand-set, which the render thread reads to enumerate on
  demand). ImGui row IDs use the sibling index because afp clips can have same-named siblings
  whose paths collide. The path stays the afp toggle key, so same-named siblings toggle as a
  group, as in the game (SetClipVisible walks the dir-6 same-name chain). No override means
  authored visibility.
- Slots that do not correspond to an enumerated node render as ghost rows under the playing
  layer ("(unresolved slot)" until the render thread's probe resolves them).
- Bottom row: add-slot-by-clip-path input (probed by the render thread next frame).
- Selection model (`Panels::Scene::Selection`, GUI-thread-local): None / Layer / Child with
  path + name; reset on IFS switch.
- When values commit: `InputInt`/`InputText` return true on every keystroke unless given
  `ImGuiInputTextFlags_EnterReturnsTrue`. Any numeric field whose value leaves the panel
  (to `App::State`, to `SaveCurrentSettings()`, or to a live override) must therefore stage
  the value and commit on `ImGui::IsItemDeactivatedAfterEdit()`, not on the widget's return
  value. Setup fps and render W/H and the inspector trim field do this. Fields that only
  feed a panel-local static (the whole export modal) can commit on every keystroke.
  Two pitfalls: a stepped `InputInt` (step != 0) appends -/+ buttons, so
  `IsItemDeactivatedAfterEdit()` must be taken after wrapping the call in
  `BeginGroup`/`EndGroup` or it reports on the "+" button; and any sibling button that sets
  the same value (the quick-fps row) still has to commit immediately.
- Tooltips on disabled controls: `IsItemHovered()` returns false for an item inside
  `BeginDisabled`, so a tooltip attached after `EndDisabled` never shows in the very state it
  usually explains. Every tooltip on a control that can be disabled passes
  `ImGuiHoveredFlags_AllowWhenDisabled`. Both live cases (3D "Animate camera", export
  "Hardware encoding") exist to say why the control is greyed out.
- `IsItem*` pitfall: `RenderSceneNode` stores `ImGui::IsItemToggledOpen()` in a local on
  the line after `TreeNodeEx`, before anything else is submitted. `IsItem*` reads
  `g.LastItemData`, which every later `ItemAdd` overwrites, and this row submits more items
  after the tree node (the `(x, y)` position text, the variant badge). Querying it at the
  bottom of the function silently dropped the expansion write for exactly the clips that
  had a position or a badge, so lazy enumeration never ran for them while a bare clip
  worked. Found by `gui_tests` (docs/gui_tests.md section 10); keep the call where it is.
- DDR: there is no afplist and no bulk child-enumerate in afp 2.13.7, so the pane shows only
  the no-layers hint; everything else follows from the missing data.

### 3.3 Inspector pane (gui_inspector)

- Properties (afp_modern only): the Scene selection. Layer -> kind, frame count,
  playing state, Play/Replay button. Child -> full clip path (mono), position when known,
  and when a VariantSlot matches: slot-visible checkbox + the bitmap swap combo.
  Variant behaviour (engine facts): "(default)" cannot remove a clip's override, because
  `afp_play_work_load_bitmap` only rebinds (there is no "restore authored" path). So
  "(default)" drops the override locally and posts `force_replay` so the master timeline
  re-authors the clip's original bitmap; other latched slots re-apply next frame.
- Render: loop master (persisted), root-loop Segmented, continuous-loop Segmented
  (OFF/default/ON = -1/0/1), trim frames, master scale slider + 1.0x/1.5x presets,
  background Segmented, filter (F7) and MC names (F3) toggles + name-type Segmented, reset
  overrides. The DDR variant of this tab has only background and reset.
  - "Loop root" reflects a game mechanism. When a scene-BG root reaches its end, the real
    game's generic choice is either hold (mount once, afp's own tick reaches its natural
    terminal branch while nested children keep running; the game default) or force loop
    (ForceReplay + the continuous-loop flag sequence, needed for one-shot masters like
    bg_common). The renderer mounts via afplist and cannot read the dispatcher's per-BG
    choice at runtime, so this is an explicit user control that defaults to the game default.
  - "Master scale": SDVX I-IV-era 720x1280 IFSes need x1.5 on a 1080x1920 game. The real
    game does this per BG via the BG entry's payload+28 float; the preview has no notion of
    a BG id, so it is a slider with a 1.5x preset.
- Live: the live state readout (mono font), file info block, and the MC-names column
  list (shown when MC names are on in "column" mode).

### 3.4 Timeline dock (gui_timeline)

- Transport: -100 / -1 / play-pause / +1 / +100 icon buttons; the step buttons wrap around
  mc_total (matching afp's own wrap to 0). Mono frame readout `cur / total`, wrap count
  "loop N", and the label combo: a dropdown listing every label with its frame, with the
  active label as its preview. Labels are switched from a dropdown because label names drawn
  on the track overlapped and could not be read whenever labels sat a few frames apart (e.g.
  wait/out).
- Track: one custom ImDrawList widget over an InvisibleButton. Progress fill + 2px playhead
  from the bounded mc playhead; frame labels are drawn only as amber ticks at `frame/total`
  (hover a tick for its name, click it to post `GotoLabel`; clicking or dragging anywhere
  else seeks). Seeking pauses, like the debug viewer's TIME controls (slider-style seeks
  clamp; only the step buttons wrap).
- During capture the track also tints `frames_captured/total` green, which gives the
  export a determinate display. The whole dock is disabled while capturing: a seek would
  corrupt the export's loop-wrap counter and a pause would freeze the capture. The render
  thread ignores such requests anyway; greying out the dock makes that visible.
- The dock renders in the Renderer view whenever the scene preset timeline editor (3.5) is
  not active; the editor replaces it in the same full-width slot. Without a loaded scene the
  dock shows a hint line.

### 3.5 Scene preset timeline editor (src/gui/timeline)

The editor replaces the 76 px dock while a preset document is loaded on the scene3d
backend. `Panels::Timeline::Active()` is the switch: the GUI-side `Editor::State` holds a
document and the `App::PresetStatus` snapshot published by the render thread names the same
`id`. Everything else (the AFP backends, the scene3d backend with a bare package loaded)
keeps the dock.

The editor covers the whole command catalog: the palette inserts every command, the options
track edits and switches `document.options`, and the library (3.6) is where a document is
picked, created, saved and validated. Track kinds are `sprite` (2D), `model` (3D), `camera`
(CAM), `light` (LIGHT), `fx` (FX), `poly` (POLY) and `scene` (SCENE); `KindBadge` and
`TrackKindColor` in `gui_tl_colors.cpp` give each one its lane badge and colour, and
`CommandColor` maps every catalog entry onto the same legend.

#### Layout and docking

The editor is a full-width bottom dock, not a column (user decision, 2026-08-15). It takes
the place of the 76 px dock and grows: `RenderRendererView` (gui_panels.cpp) lays out the
three-column row (Browse, viewport, Inspector) above it and the editor below, spanning the
whole content width, with `Gui::HSplitter` (the horizontal twin of `Gui::VSplitter`)
between them. The status strip stays below both, drawn by `RenderReadyView` outside the
`main_view` child, so the editor's bottom edge sits directly on it.

The remembered editor height lives in `Editor::View::height`: default 280 px, minimum 120 px,
and the only other limit is `Gui::kPaneRowMinH` (120 px) for the row above, so the splitter
can be dragged until the editor owns nearly the whole window. `ClampEditorHeight` applies
that every frame, so a window resize never leaves the row shorter than its minimum.
Double-clicking the splitter resets the editor to its default height. When no document is
loaded the three columns keep the full height and the 76 px `RenderTimelineDock` is drawn
instead, exactly as before.

Item paths for tests: the editor child is `main_view/##timeline_editor` (it was
`main_view/pane_center/##timeline_editor` while it lived in the column) and the splitter is
`##split_timeline` inside `main_view`. `tl_bottom_dock` pins the geometry: the editor's left
and right edges equal `main_view`'s, its bottom edge is on the status strip, and the left
column ends above it.

Inside the editor child, top to bottom: transport row, two-row ruler, track header column
(200 px, clamped 120..320) plus the lane area, and the horizontal range scroll bar. The
transport row, the ruler and the range scroll bar are fixed. Only the header column and the
lanes scroll vertically, together, inside a clipped region between the ruler and the scroll
bar (`ImGui::PushClipRect` in `DrawTracks`, so a half-scrolled row is cut at the region edge
instead of painting over the scroll bar). The child itself carries `NoScrollWithMouse` so the
wheel can never move the whole editor and leave the transport half off screen.
`Editor::ClampTrackScroll` is applied once per frame against the height of the lanes region,
so the last track's bottom edge lands exactly on the bottom of that region.

Every track row is `TrackHeight(track)` tall, and the header column and the lane use that
same function, so the two cannot drift apart; that is also why `###tl_head_<id>` covers the
whole row height. A track gets a sub-lane only when it holds a primary clip and modifier
clips (`Editor::LaneCount` / `Editor::LaneOf` in `src/editor/timeline_lanes.cpp`, unit
tested): one sub-lane per distinct modifier command type, with the primary on top. A track
whose clips all belong to one family (only draws, only tweens, only emitters) is a single
lane and its clips fill it at the full main-lane height, which is why a camera.tween track and
an fx track are the same height as a plain model track.

Every one of those heights comes from
`Editor::LaneMetricsFor(text height, FramePadding.y, dpi scale)`
(`src/editor/timeline_lanes.cpp`), a pure function that the renderer and the tests both call.
The heights are derived from the font, not fixed: `clip` and `sub_clip` are each at least
`GetTextLineHeight() + 2 * FramePadding.y` (the same rule `ToggleSide` uses for the header
toggles), over floors of 22 and 14 design units; `sub_lane` is `sub_clip` plus a 2 unit gap,
and `row` is `clip` plus 4 units over a 26 unit floor. The floors and the gap are the only
raw pixel numbers in there, which is why the caller passes in the DPI scale (2.1) instead of
the function reading a constant it cannot scale. `RowHeight` then takes the larger of that row
and `ToggleSide() + 4`, so the header controls and the label rule can each only make the row
taller. Bar heights were hardcoded at 22 px and 14 px until 2026-08-18, and a 14 px modifier
sub-lane is shorter than a 16 px line of Segoe UI: the label was drawn in `ImGuiCol_WindowBg`
over the bar, so the part hanging below the bar landed dark on dark and the user saw
"scroll x 1/f, wrap 640" with its bottom cut off. A font or padding change would have broken
it again, so the rule is derived from the metrics instead of being a bigger constant. The
modifier lane stays visually secondary through its command colour and its own clip range, not
through a height too short to read.

The label's own Y is
`Editor::LaneLabelY(bar top, bar height, text height, keyed, dpi scale)` from the
same file: a keyed clip drops the label below the key diamonds by up to 6 units, an unkeyed one
centres it, and both clamp to the room the bar actually has, so the text always ends up inside
the bar. `tl_label_fits` (`tests/gui/timeline_editor_tests.cpp`) walks every clip of a
document with `sprite.animate` + `sprite.scroll` and `model.draw` + `model.motion` sub-lanes
and asserts the `###tl_clip_<id>` rect is at least `text + 2 * padding` tall and that
`LaneLabelY` + text fits inside it, plus that each `###tl_head_<id>` is still exactly as tall
as its `###tl_lane_<id>`; the pure counterpart sweeps text heights 9..34 px against paddings
0..6 px.

The header row is laid out from its right edge, so the controls cannot be squeezed out.
The M / S / L toggles are a group flush against the column edge, each a square of side
`ToggleSide` = `max(text line height + 2 * FramePadding.y, widest of M/S/L + 2 * FramePadding.x)`,
so a letter is never clipped whatever the font is. The kind badge sits to their left and is
dropped when the remaining name space would fall below 24 px (the colour chip already shows
the kind). The name takes what is left and is shortened with an ellipsis to fit (`Ellipsized`,
the same helper the clip bars use) instead of being cut hard or pushing anything off the
column. `tl_header_toggles` pins the rule at the default 200 px and at the
120 px minimum: each toggle is at least its letter plus twice the frame padding in both axes,
its clipped rect equals its full rect, and it lies inside the header column. The
lanes scroll vertically with Shift+wheel (`Editor::View::track_scroll`, clamped by
`Editor::ClampTrackScroll` so the last track cannot be scrolled off the top).

#### Threading rule (enforced by a gate)

Nothing under `src/gui/timeline/` (and later `gui_preset_library.cpp`) may name
`PresetHost::`, `Scene3dHost::` or `Gc2dHost::`. The editor owns a GUI-side working
document and talks to the render thread only through `App::State::PostCommand`:

| command | effect on the render thread |
|---|---|
| `PresetCmd::Seek{frame}` | `PresetHost::Seek`, clamped to `[0, length - 1]` |
| `PresetCmd::SetPaused{paused}` | pause / resume the document clock |
| `PresetCmd::SetLoop{loop}` | on: the frame after `length - 1` is 0; off: playback pauses on `length - 1` |
| `PresetCmd::SetOption{option, choice}` | select an option choice; the host re-pushes its whole rebind list, because a choice can flip a `when` gate and visibility is a rebind-only push (docs/preset_document.md) |
| `PresetCmd::ReplaceDocument{shared_ptr<const Document>}` | swap the evaluator's document at the frame boundary and re-simulate `EvalState` from frame 0 |

`Backend::ApplyPresetCommand` (src/backend/preset_command_apply.cpp) is the single seam that
unpacks the `std::any` payload; `Scene3dBackend::HandleCommand` delegates to it, and
`preset_host_tests.cpp` drives it directly against the recording host stubs. Status flows
back the other way in `App::PresetStatus` (id, frame, length, fps, playing, loop), published
once per render frame from `Scene3dBackend::AdvanceFrame` and read once per GUI frame.

The playhead the editor draws is the render thread's frame from that snapshot. The GUI
never owns a second clock, and the transport counter reads `frame / length` from it. Scrubbing
posts `Seek` plus `SetPaused{true}`.

Editing while playing is allowed and never blocks: the GUI keeps its own working copy,
`Editor::State::Apply` publishes a fresh immutable snapshot, and the editor posts
`ReplaceDocument` whenever `Editor::State::Revision()` changes. All document changes are
deferred to the end of the GUI frame. `Ctx::pending` is a vector of `Editor::Edit`, applied in
the order they were queued, so the `Document` the draw code is walking can never be replaced
under it, and two edits raised in one frame (a Ctrl+drag duplicate plus the move that carries
it) both land instead of the second overwriting the first. A `shared_ptr` kept for the frame
protects the walk from undo/redo too.

#### Editor state, undo and selection (src/editor)

`src/editor/` is ImGui-free and unit tested by `tests/editor/timeline_editor_tests.cpp`
(built into `game_tests`):

- `preset_editor_state.h/.cpp`: the working `Document` as `shared_ptr<const Document>`,
  selection (clip ids), clipboard, view, dirty flag, the M5 `Request` channel, and snapshot
  undo. `Apply(edit)` copies the document, pushes the previous one plus the selection it
  applied to, and clears redo. Depth 200. `BeginGesture()/EndGesture()` bracket a drag so
  every intermediate state is published but only one undo entry is recorded. Undo restores the
  selection it was applied with; otherwise selection and playhead are not part of undo.
- `timeline_view.h/.cpp`: zoom (0.05..8 px per frame), scroll clamping, fit, and the ruler
  tick rule: the finest spacing in 1, 5, 10, 30, 60, 300, 600, 1800 frames that still keeps
  at least 56 px between labels (the coarsest entry wins when even 1800 frames is tighter
  than that).
- `timeline_edits.h/.cpp`: every document change as a pure function: move, move to
  another track, resize, split, trim start/end, make open-ended, mute clip, delete, copy,
  paste, duplicate, duplicate onto a track at a frame, track M/S/L, rename, reorder,
  duplicate/delete track, markers, length.
  `PrimaryPeers` / `Overlaps` implement the overlap rule: one primary per family per frame
  per target across all its tracks, modifiers overlap freely, and two primaries whose `when`
  gates are mutually exclusive may overlap.
- `timeline_drag.h/.cpp`: hit testing (`ZoneAt`: `kClipHandlePx` = 6 unit resize handles,
  never more than 30 percent of a short bar) and the drag state machine as a pure function.
  `SnapFrame` walks the snap targets in priority order: playhead, other clips' starts and
  ends on any track, tween keys, document start and end, ruler major ticks; within
  `DragInput::snap_px` (`kSnapPx` = 8 units), the nearest candidate of the first non-empty
  group wins. The handle width, the snap radius and the ruler `label_gap` arrive as
  parameters from the GUI, already multiplied by the DPI scale (2.1), so the editor layer
  never has to know how big a pixel is. `ResolveDrag` keeps the duration on a move, refuses a
  move that would overlap a same-family primary, and clamps a resize at the neighbour
  instead of overlapping it. Alt clears `DragInput::snap`, so the raw frame is kept. Ctrl+drag
  duplicates: `DuplicateClipTo` creates the copy at the drop position under the same
  rules as a move (snap, compatible track, refusal), the original stays where it was, and the
  drag then carries the copy. `OverlapsSelfCopy` refuses a copy dropped on top of its
  own source, which plain `Overlaps` cannot see because it excludes the dragged clip itself.
- `clip_summary.h/.cpp`: the text a clip bar carries: its `label` when set, otherwise the
  command's own summary (`model.draw` = `<model>, <blend>, spin <axis> N deg/f`;
  `sprite.animate` = `<animation>, <playback>, prio N`; every other family its key params;
  tweens their key count). Pure, with one unit test per family, so the bar text cannot drift
  from what the tests pin. The GUI only shortens it: `Ellipsized` trims to the bar width and
  appends an ellipsis.

#### Items, and their Test Engine ids

Every interactive element is a hand-drawn `ImDrawList` shape over an `InvisibleButton` or a
plain widget, and carries a `###` id so the glyph in its label cannot change the hash.

The transport row is one line of widgets plus a right-aligned tail (zoom, fit, undo depth,
document badge). `PlaceTail` measures the tail and puts it flush against the right edge of
the editor, or on a second row when the left group already reaches that far, so nothing is
clipped at the app's own default window width. A Test Engine case pins that at 1360 px by
comparing each tail item's clipped rect with its full rect.

| item | id | behaviour |
|---|---|---|
| jump to 0 | `###tl_jump_start` | Home |
| step back | `###tl_step_back` | Left (Shift x100) |
| play / pause | `###tl_play` | Space; posts `SetPaused` |
| step forward | `###tl_step_fwd` | Right |
| jump to last frame | `###tl_jump_end` | End |
| loop toggle | `###tl_loop` | posts `SetLoop` |
| snap toggle | `###tl_snap` | editor-side only; Alt suspends it per drag |
| add command | `###tl_add_command` | opens the command palette for the selected track at the playhead (also the `A` key) |
| add track | `###tl_add_track` | opens the Add track modal |
| previous / next edge | `###tl_prev_edge`, `###tl_next_edge` | `[` / `]` |
| zoom slider | `###tl_zoom` | 0.05..8 px per frame, logarithmic |
| fit | `###tl_fit` | Ctrl+0 |
| document badge | `###tl_doc_badge` | shows `*` when dirty; opens the Document properties modal |
| validation badge | `###tl_problems` | only present while `Validate` reports errors; opens the Problems modal |
| ruler | `###tl_ruler` | click or drag seeks; Shift+drag sets the export range; double-click adds a marker; right-click on a marker triangle opens the marker popup, and elsewhere clears the export range |
| marker name field | `###tl_marker_name` | inside the marker popup |
| document end line | `###tl_end_line` | drag it to change `length`; the region beyond is dimmed and labelled `end N` |
| header column edge | `###tl_header_split` | drag to resize the header column, clamped to 120..320 px |
| track header | `###tl_head_<track id>` | double-click renames, drag reorders, right-click opens the track menu |
| track rename field | `###tl_rename_<track id>` | Enter commits |
| track M / S / L | `###tl_mute_<id>`, `###tl_solo_<id>`, `###tl_lock_<id>` | square buttons, filled with the accent when on |
| track lane | `###tl_lane_<track id>` | press starts the rubber band, right-click opens the empty-area menu |
| clip bar | `###tl_clip_<clip id>` | click selects, double-click opens the clip properties modal, drag moves, edge drag resizes, Ctrl+drag drops a duplicate. An event clip draws a 4 px tick but its hit rect is 12 px wide, because a 4 px click target cannot be hit reliably (the Test Engine could not hover one either) |
| tween key diamond | `###tl_key_<clip id>_<index>` | 10 px square hit rect centred on the diamond; click selects the key, drag moves it in time (snapped like a clip), double-click opens the clip modal with the Tween tab focused on that key |
| options track header | `###tl_opt_head_<option id>` | one virtual row per `document.options` entry, above the content tracks |
| option choice | `###tl_opt_choice_<option id>_<index>` | segmented control; a click posts `PresetCmd::SetOption`, the live choice is filled with the accent |
| option edit | `###tl_opt_edit_<option id>` | opens the option properties modal |
| curve editor | `###tl_curve_close`, `###tl_curve_channel`, `###tl_curve_ease_<ease name>`, `###tl_curve_key_<index>`, `###tl_curve_handle_<0\|1>` | the curve editor takes over the lane area, see below |
| range scroll bar | `###tl_scroll` | drag to pan |

Clip bars fill `LaneMetrics::clip` in the row's main lane, centred in it, and
`LaneMetrics::sub_clip` in a modifier sub-lane (sub-lane rule above). The bar text is
`Editor::ClipSummary`
shortened to the bar width with an ellipsis. The bar is filled with the command
colour (a twelve-colour legend, `gui_tl_colors.cpp`, with separate dark and light
values chosen from `ImGuiCol_WindowBg` luminance; these are the only colours in the shell not
derived from the profile accent). Muted clips and muted or non-solo tracks draw at 40 percent alpha,
a `when` gate draws a 3 px accent stripe along the top edge, a locked track draws a
desaturated left bar, an open-ended clip shows a `>` at its right edge, and tween keys draw
as diamonds.

Selection: a click selects, Shift extends, Ctrl toggles, and a press-drag on empty lane space
draws a rectangle (`UpdateBand`) that selects exactly the clip bars its rectangle covers, in
both axes. The clip rects are the ones drawn this frame, so a band never picks up a lane it
did not reach. Esc clears.

#### Menus

Clip right-click: Properties (opens the modal), Cut, Copy, Paste at playhead, Duplicate,
Split at playhead, Trim start / end to playhead, Make open-ended, Add key at playhead
(`###tl_menu_add_key`), Add transition to next clip (`###tl_menu_transition_next`, enabled when
`Editor::NextDrawClip` finds a later `model.draw` on the same target), Add transition between
selected (`###tl_menu_transition_selected`, enabled with exactly two clips selected), Mute clip,
Delete.

Empty lane right-click: Add command here (the palette anchored at the clicked frame), Paste at
this frame, Add track above / below.

Track header right-click: Rename, Move up, Move down, Duplicate track, Change target,
Delete track.

#### Shortcuts

Active when the editor is focused or hovered and `io.WantTextInput` is false.

| key | action | key | action |
|---|---|---|---|
| Space | play / pause | Ctrl+Z / Ctrl+Y | undo / redo |
| Left / Right | step 1 frame (Shift: 100) | Ctrl+C / X / V | copy / cut / paste at playhead |
| Home / End | frame 0 / `length - 1` | Ctrl+D | duplicate selection |
| `[` / `]` | previous / next clip edge, key or marker | Ctrl+0 | fit the whole document |
| Delete | delete the selected key when there is one, else the clip selection | S | split selection at playhead |
| M | mute the selected clips' track | L | lock the selected clips' track |
| Enter | properties of the selected clip | Esc | cancel a drag, clear the selection |
| A | command palette at the playhead on the selected track | K | add a tween key at the playhead on the selected clip |
| C | curve editor for the selected clip | | |
| Ctrl+S / Ctrl+Shift+S | save / save as (3.6) | Ctrl+O / Ctrl+N | import / new document (3.6) |
| Ctrl+E | export video | | |
| Ctrl+wheel | zoom about the cursor | Shift+wheel | scroll the tracks |

#### Modals: palette, clip properties, document properties, problems

Every editor modal is a centred `BeginPopupModal` drawn by `Panels::Timeline::RenderModals()`,
which `RenderReadyView` calls next to `Export::RenderModal()`, outside the editor child, so a
modal is never clipped by it. `RenderModals` first drains `Editor::State::TakeRequest()` and
turns the four `RequestKind`s into an open request, so a double-click, the Enter key, the
context menu, the transport buttons and the Clip inspector tab all reach the same code.

The clip and document modals have a fixed size (660 and 640 px wide, capped at 640 / 560 px
tall or the viewport minus 80) instead of auto-resizing, so a long form (the hidden-parts
checklist of a 30-part animation) scrolls inside the popup. Before, an auto-resized popup with
a max-height constraint pushed Cancel and Done off the bottom of the screen.

A form row is a label plus a field, and the field is sized `-kResetColumn` (30 px), so it
already runs to within 30 px of the modal's right edge. The room that column leaves is for the
per-field reset button `###tl_reset_<field id>` and nothing else, so a badge, hint or warning
can never be appended with `SameLine` after a field. The modals pin their width
(`SetNextWindowSizeConstraints` with equal min and max), so `AlwaysAutoResize` cannot grow to
make room and the text is cut at the window edge. That happened to the Add track target row,
where "asset not loaded" ran past the edge and read as "asset r" (fixed 2026-08-18).

The rule is a shared helper in `gui_tl_forms.cpp`: `RowNote` (dim) and `RowWarning` (amber) draw
the note on its own line, indented by `kLabelWidth` so it lines up under the field, and under
`PushTextWrapPos(0)` so a note of any length wraps inside the content region instead of running
off it. Both the Add track "asset not loaded" warning and the Params/Asset "not in the loaded
asset" warning go through it, as do the per-field "outside the usual range" and degrees notes
that already used this shape by hand. A hint that follows a checkbox (`insert` /
"below the selected track", the Document properties hints) still uses `SameLine`: a checkbox is
~20 px, so those rows have the whole field column left, and the audit found none of them
overflowing.

`modal_layout_tests.cpp` guards this. It uses no item ids on purpose, so it also catches text
that has no item id: for each modal it compares the window's `ContentSize.x` against its
`WorkRect` width, which is the union of every item submitted that frame, and it also pins the
widgets with ids in the warning rows inside `InnerRect`. It covers Add track, Clip properties
(Params and Asset, against a stubbed `AssetIndex` whose model list does not contain the clip's
model), Document properties on all four tabs, and Option properties. Before the fix it read
"//Add track draws 566.0 px of content into 500.0 px of room" and
"//Clip properties/###tl_clip_body draws 770.0 px of content into 640.0 px of room".

`ResetPalette` also resets the Add track kind and asset index, not just the target and name.
They are file-static, so without the reset the modal opens with the previous document's kind.

Tab contents are nested under their tab bar in the Test Engine id path, so a field inside the
clip modal's Params tab is `###tl_clip_tabs/###tl_tab_params/###tl_field_<json key>`. A
Vec2 / Vec3 row has no item of its own (ImGui's `DragScalarN` pushes the label as an id scope):
its axes are `.../###tl_field_position/$$0` and so on. `clip_modal_tests.cpp` accepts either.

The emitter's `burst` block follows the `orbit` / `pulse` / `scatter` pattern: a checkbox that
creates or clears the whole optional value, then one int row per field while it is present.

| modal | window | items |
|---|---|---|
| Command palette | `Add command` | `###tl_palette_filter`, one `###tl_palette_<command type>` per catalog entry, `###tl_palette_cancel` |
| Add track | `Add track` | `###tl_track_kind`, `###tl_track_asset`, `###tl_track_target`, `###tl_track_name`, `###tl_track_below`, `###tl_track_add`, `###tl_track_cancel` |
| Clip properties | `Clip properties` | tabs `###tl_tab_general` / `###tl_tab_params` / `###tl_tab_tween` / `###tl_tab_asset` under `###tl_clip_tabs`, `###tl_clip_cancel`, `###tl_clip_done` |
| Document properties | `Document properties` | tabs `###tl_doc_tab_general` / `_render` / `_camera` / `_lights` under `###tl_doc_tabs`, `###tl_doc_done`, `###tl_doc_cancel`, `###tl_doc_convert_fps` |
| Convert fps | `Convert fps` | `###tl_fps_target`, `###tl_fps_rounding`, `###tl_fps_convert` |
| Problems | `Problems` | one `###tl_problem_<index>` per row, `###tl_problems_close` |

The Document properties tabs are written by hand instead of being generated from `FieldsFor`,
because `RenderSpec`, `CameraSpec` and `LightSpec` are document blocks, not commands, and carry
no field table. Adding a document-level field therefore means adding its row here in the same change: the
Render tab holds width, height, `###tl_doc_opaque`, shading, `###tl_doc_split` and
`###tl_doc_clear` (the frame clear colour), and each light on the Lights tab holds
`###tl_doc_light_dir`, `_diffuse`, `_specular` and `_ambient`.

Every one of these modals closes on Esc, and Esc acts as the Cancel button, not the Done
button: the clip and document modals rewind the undo stack to the depth captured when they
opened, as their Cancel does. This has to be written by hand. `BeginPopupModal(title, nullptr, ...)`
passes no `p_open`, and ImGui only closes a modal on Escape when one is supplied, so without the
explicit `IsKeyPressed(ImGuiKey_Escape)` branch these modals swallow Escape and stay open. The
branch is also gated on `IsWindowFocused(ImGuiFocusedFlags_ChildWindows)`, so a nested
popup (the Convert fps modal, an open combo) consumes its own Escape first, and on
`!io.WantTextInput`, so Escape cancels a text edit before it cancels the modal.

The palette is generated from the catalog: `Editor::PaletteEntries` (pure, in `src/editor`,
unit tested) returns every command type with a display name, a one-line summary of its key
params, and the section it belongs to: the clicked track's kind first, then "Other tracks
(creates a track)", then "Document" for `option.select`. The filter matches the name and the
summary, case-insensitively. The sort is by section, then catalog order, and nothing else,
because with these names no other ranking ever changes the order. Up and Down move a highlight
over the list and Enter inserts the highlighted entry; both arrows skip refused entries and wrap,
typing in the filter resets the highlight to the first enabled entry, and Esc closes. The
highlight is what `Selectable`'s selected flag draws, so the keyboard and the mouse agree on
which entry Enter will take. An entry whose insertion
would put a second primary of the same family over an existing one at that frame
(`Editor::PrimaryBlocker`) is drawn disabled, with the clip id that blocks it as the reason.
Enter in the filter box inserts the first enabled entry. Insertion uses `DefaultCommand(type)`
plus the document's first asset of the matching kind, is open-ended for a primary with nothing
after it on the track, ends at the next clip when there is one, and is 60 frames for a
modifier; the new clip is selected and its properties modal opens. Choosing a command whose
track kind differs from the clicked track creates that track first.

Forms are generated from `FieldsFor(command type)`: one row per `FieldDesc`, with the widget
chosen from the value the descriptor reads (`gui_tl_forms.cpp`). `asset`, `model`, `cell` and
`animation` become combos filled from the `AssetIndex` in the status snapshot, with a warning
beside a name the loaded asset does not carry; an optional field that is unset draws a
checkbox that materialises it ("inherits the document value"). The nested blocks (`orbit`,
`pulse`, `scatter`, `burst`, and `poly.tile_grid`'s `texture`) draw the same way: a checkbox
that turns the whole block on or off, then the block's own rows under it. `texture` unchecked
is `null`, which is the untextured tile grid the game draws with no movie loaded, so the
checkbox is a real choice and not an inheritance. Hard ranges clamp on commit
(`Editor::ClampField`), soft ranges only print "outside the usual range"
(`Editor::OutsideSoftRange`), radian fields print a degree readout under them
(`Editor::DegreesText`), and a field that differs from the catalog default grows a
`###tl_reset_<key>` glyph in the right column whose tooltip names the default value. Field
commits go through `Editor::State::Apply` inside a `BeginGesture`/`EndGesture` pair, so one
field is one undo entry even while a drag is live; the whole edited document is published, so
the viewport follows immediately. Ctrl+Z inside a modal undoes one field at a time down to the
depth the modal opened at, Cancel undoes back to that depth and drops the redo stack, and Done
collapses the entries into one (`Editor::State::CollapseUndo`).

The clip modal's Params tab also carries, for `sprite.animate`, the hidden parts checklist
(one `###tl_hidden_<part>` per child part from the `AssetIndex`, with the
`docs/preset_layers.md` verdict printed beside it) and the layer preview strip. The strip
posts `PresetCmd::PreviewLayer` once per (asset, animation, hidden_parts) key, then draws
`total` cells: each sample that has arrived as an image, the rest as a determinate
"sample k / N" cell, with a "N / M sample(s) rendered on the render thread" line above them.
The samples are rendered one per frame on the render thread (`Preset::Preview`, called from
`Scene3dBackend::AdvanceFrame`, outside the scene) and published as small BGRA buffers. The
GUI uploads them to its own device through `Gui::GetDevice()`, because the GUI and the
renderer hold two separate D3D9 devices and a texture cannot be shared between them. They are
`D3DPOOL_MANAGED`, so they survive the GUI device reset that a window resize triggers.

The render thread publishes one snapshot object, not a copy per frame. `Preset::Preview::Get`
returns a `shared_ptr<const Snapshot>` that is rebuilt only when a sample lands or a request is
posted, and `Scene3dBackend::AdvanceFrame` calls `App::State::SetPresetPreview` only when the
pointer differs from the one it published last. The same rule applies to the `AssetIndex` in
`App::PresetStatus`: `PresetHost::GetAssetIndex` hands out a `shared_ptr<const AssetIndex>`
rebuilt only when a document load changes the assets. Both used to be deep-copied on the way
out of the host, again into the status struct, and again on every `GetPresetStatus()` in the
GUI. For a real IIDX package that is thousands of strings and, while a preview is open, a few
hundred KB of pixel buffers, several times per frame. `preset_host_tests.cpp` pins the
invariant: eight `RenderFrame` calls in a row must return the same `AssetIndex` pointer, and a
`ReplaceDocument` that changes the assets must return a different one.

Finished previews are kept in `Preset::Preview::Cache`, an 8-entry LRU keyed by the same
(asset, animation, hidden_parts) string the request uses. `Post` looks the key up first and, on
a hit, republishes the cached snapshot complete and leaves `Pump` nothing to do, so switching
the animation combo away and back is instant instead of costing six more render-thread
samples. The cache is a plain class with `Find` / `Insert` / `Clear` so
`preset_preview_tests.cpp` can pin the eviction order without a device.

`Reset()` can be called from the GUI thread (unloading a preset) while `Pump()` is running on
the render thread, so it must not touch D3D. It clears the request, the published snapshot and
the cache under the lock and raises a release flag; the next `Pump` consumes the flag and frees
the render target, on the render thread, under the lock. `PresetHost::PumpPreview` is therefore
called on every frame instead of only while a preset is loaded, so the deferred release still
happens after an unload.

Closing the modal must not release those textures immediately. `DrawPreviewStrip` records the
raw `IDirect3DTexture9*` in the window draw list with `AddImage`, and the Cancel and Done
buttons are drawn later in the same frame, so releasing them there hands
`ImGui_ImplDX9_RenderDrawData` a freed COM object when `ImGui::Render()` walks that draw list
at the end of the frame. `Close()` therefore only raises a flag, and `RenderClipModal` calls
`ForgetPreview()` at the top of the next frame, before anything is drawn. `gui_tests` cannot
see this: with no `Gui::Window` there is no device, `Sync()` never uploads, and there is
nothing to free.

The Asset tab of the commands that have an `asset` field shows the asset combo, the resolved
dir with a loaded / missing badge, the kind, the model / cell / animation combo, and an "add
asset..." button that opens the existing `NativeDialog::BrowseForFolder`, stores the picked
directory relative to the game dir, and points the clip at the new asset id.

The Tween tab is read-only in this milestone: it lists the keys the document holds (at, ease,
values). Key editing and the curve editor are M6. It is shown only for commands that can carry
keys at all, which is `any_of(KeyFieldsFor(type), tweenable)` and not `!KeyFieldsFor(type).empty()`:
`KeyFieldsFor` falls through to `FieldsFor` for every non-tween command, so the empty test is
true for `rng.seed` and `rhythm.beat` too and would show all four tabs on a command that cannot
hold a single key. `model.tween` and `camera.tween` have no fields of their own, so they show
General and Tween only.

Every form row states its unit twice: in the label (`position  world`) and in the row tooltip,
which also carries the JSON key and the help. The unit comes from the `FieldDesc`, never from
the widget, so the two cannot drift; the vec3 rows used to drop it because `DrawVec3Row` took no
unit at all, and `position`, `scale`, the camera `eye` / `at` / `up` and the light vectors
carried no unit in the descriptor either. The tooltip covers the whole row: a vec3 is three
`DragFloat`s and a reset button, so the row is wrapped in `BeginGroup`/`EndGroup` and hovered as
one item. `IsItemHovered` after the widgets would only cover the last axis.

An empty `model` on a `model.draw` is not a missing value: the evaluator falls back to the
track's target (`eval_models.cpp`, `if (!command.model.empty()) slot.name = command.model`), so
the combo shows `<target> (track target)` instead of `(none)`, and `hidden_parts` shows the
`(none)` hint when the list is empty.

The modal sizes to its content. It pins its width and limits its height to the viewport
instead of fixing both, so a short tab (General on a `model.draw`) is exactly as tall as its
rows and has no 200 px of empty space under the buttons. The preview strip sits between
the param form and the checklist so that it stays above the fold: with the checklist
first, the strip would be pushed past the cap and the user would have to scroll to see the
layer they are classifying.

Auto-resize alone is not enough, because a popup is one scrolling window: once the content
passes the cap, everything below the fold scrolls out of reach, including the Cancel and Done
buttons, and no cap fixes that because the content is what grows. So the tabs live in a
`###tl_clip_body` child sized to the measured content height (last frame's `GetCursorPosY`)
clamped to the viewport, and the footer is drawn after it, in the popup itself. Short tabs get a
short child and no empty space, tall ones scroll inside the child, and the footer never moves.

The Test Engine treats the child as a separate window, so items inside it are not reachable as
`Clip properties/###tl_clip_tabs/...`: a test must set its ref to the child
(`GuiTest::FocusChild(ctx, "//Clip properties/###tl_clip_body")`, which `clip_modal_tests.cpp`
wraps as `ClipBody`) for tab and field items, and address the footer buttons absolutely as
`//Clip properties/###tl_clip_done`. The fixed footer is worth that extra step in the tests.

A real package animation also nests dozens of parts (`TITLE` on IIDX RED has about 80), so the
hidden-parts checklist is a `CollapsingHeader` labelled with its count, open by default only
when the animation has 8 parts or fewer: a wall of 80 checkboxes buries the preview strip above
it even when scrolling works. `clip_modal_tests.cpp` pins the footer geometrically against an
80-part stub: the item rect of `###tl_clip_done` must sit inside the modal's `InnerRect`.

#### Tween tab, curve editor and the key diamonds

Keys draw as diamonds on the clip bar with a thin connector line between consecutive keys, on
an upper line inside the bar so the connector never runs through the clip text. The selected
key draws in the accent. Every diamond is its own `InvisibleButton` submitted after the clip's
button (which sets `SetNextItemAllowOverlap`), so a key wins the hit test over the bar under it.
Dragging one calls `Editor::MoveKey` through the same `Editor::SnapFrame` the clip drag uses, and
the whole drag is one undo entry (`BeginGesture` on activation, `EndGesture` on release).

The Tween tab (`gui_tl_tween_tab.cpp`, shown for every command whose `KeyFieldsFor` has a
tweenable field, so `model.tween` and `camera.tween` show only General and Tween) is a key list
of `at`, the ease that reaches the next key, the values the key names and, for a `sine_deg`
segment, the fraction of the way to its next key that is actually reached, which is the
unnormalised sine the game itself uses. Below it are `Add key at playhead` (`###tl_tween_add_key`)
and the selected key's editor. `Add key at playhead` is disabled, with the reason spelled out
beside it, while the playhead is outside the clip: `AddKeyAt` refuses a clip-relative `at`
outside `[0, duration]`, and a button that silently does nothing looks like a bug. Then the
selected key's editor: `at` (`###tl_tween_at`), ease (`###tl_tween_ease`), `rate` when the ease
is `sine_deg`, the four control points when it is `bezier`, then one row per tweenable field:
either a value widget (`###tl_tween_field_<id>`) with an `x` to unset it, or `(not keyed)` with a
`+ add` (`###tl_tween_add_<id>`) that captures the value the clip resolves at the playhead. Every
edit goes through the pure `src/editor/tween_edits.*` functions, so the tab, the diamonds, the
curve editor and the unit tests share one rule set (documented in `docs/preset_document.md`).

The curve editor (`gui_tl_curve.cpp`) opens from the Tween tab's `Open curve editor` button, the
`C` key on a selected clip, or a double-click on a diamond, and takes over the lane area for
that clip: while it is open the ruler, tracks and playhead of the timeline are not drawn, and
`Close` (or Esc) returns to them. X is clip-relative frames, Y the value range of the chosen
channel (a vec3 field contributes one channel per axis), padded so a flat curve still has
height; the range is frozen while a point is being dragged so the curve does not chase the
cursor. The curve itself is sampled through `Preset::Eval::SampleKeys`, the same code the
evaluator runs, so the picture cannot drift from the render. The clip's other channels and the
channels of other tween clips on the same target draw in the muted neutral for reference. Keys
that name the channel are filled diamonds and drag in both axes (horizontal = `MoveKey`,
vertical = `SetKeyValue`); keys that do not name it are hollow and sit on the sampled curve. The
ease preset buttons apply to the selected key's segment, bezier handles appear only for the
`bezier` ease and map through the pure `src/editor/curve_geometry.*` helpers, and the playhead
draws as a line with a live value readout.

The drawn polyline and the automatic Y range come from one pure sampler, `Editor::CurveSamples`
in `src/editor/curve_geometry.cpp`, and `Editor::CurveAutoRange` is that same sampler plus the
raw key values run through `RangeOfValues`. Both take the underlying value the clip resolves at
`clip.start` (`Editor::ResolvedFieldValue`, converted with `Preset::Eval::FromParamValue`) as an
argument, so the GUI passes the identical value to both and the range cannot disagree with the
line. This affects what is drawn: `SampleKeys` blends from the underlying value whenever the
first key that names the channel is not key 0. A two-key tween whose key 0 names only `alpha`
and whose key 1 names `alpha` and `scale` draws `scale` from the underlying value at frame 0 to
3.0 at the last frame. The range used to be computed with a default-constructed `TweenValue`
(a zero scalar) while the line used the resolved one, so with an underlying `scale` of 10 the
range came out `[-0.3, 3.3]` and the whole left half of the curve was drawn above the top of the
plot. `curve_geometry_tests.cpp` pins it: every sample of that clip must lie inside
`CurveAutoRange` for the same underlying value. `CurveSamples` also always ends on the last
frame, appending it when the `duration / 240` step does not divide the duration, so the polyline
reaches the right edge and the range covers the endpoint the picture shows.

#### Options track and the option properties modal

`gui_tl_options.cpp` draws a virtual band above the content tracks whenever `document.options`
is non-empty. Nothing in `tracks` corresponds to it: it is the view of `Document::options`, and
it scrolls with the tracks like any row. Each option row carries the OPT badge and label in the
header column, an `Edit...` button, and in the lane a segmented control of the choices with the
live one filled in the accent (the accent is pushed for `Button`, `ButtonHovered` and
`ButtonActive`, because the chip's text is `WindowBg` and the theme's hover grey under that dark
text is unreadable), followed by `selected <label>` and the transition summary
(`Editor::TransitionSummary`, e.g. `100 f at step 4, kick 15`). Clicking a choice posts
`PresetCmd::SetOption`; the choice is session state read back from `App::PresetStatus`, never
part of the document.

While a transition is in flight the render thread publishes it in `App::PresetStatus::transition`
(`{option, from, to, frames_left}`, filled from the evaluator's counter), and the band draws the
runtime overlay: a dashed accent rectangle from the playhead over `frames_left / step` document
frames on the row of every track the moved targets belong to (`Editor::MovedTargets` plus
`Editor::TrackIdsForTargets`). It is not a clip, is not on the frame axis and is never saved.

`TrackIdsForTargets` never returns an empty list: when no track row matches it returns the
single marker `Editor::kOptionBandRow`, and the overlay then draws on the option's own header
row inside the band (`Ctx::options_top` plus the option index times the row height, published by
`DrawTracks` before it draws the band). Without that fallback a running transition was
invisible for any choice whose targets have no row of their own (a camera-only choice on a
document with no `camera` track, or a choice that carries no values at all), and the viewport
itself was the only sign that anything was happening. With the fallback a transition is always
visible somewhere. `options_model_tests.cpp` pins the marker for a camera-only choice on a
document with a single model track.

`Add option` in the palette's Document section (`###tl_palette_add_option`) appends an
`OptionSpec` and opens the option properties modal (`gui_tl_option_modal.cpp`, window
`Option properties`): id, label, default choice, the choices table with per-choice label,
`up###tl_option_choice_up_<index>` / `down###tl_option_choice_down_<index>` reorder and remove,
plus the selected choice's target values (targets picked from
`Editor::ChoiceValueTargets`, never typed), and the transition block `###tl_option_frames`,
`###tl_option_step`, `###tl_option_ease` (restricted to hold / linear / ease_in / ease_out /
ease_in_out) and `###tl_option_kick`. Renaming a choice goes through `Editor::RenameChoice`,
which rewrites every `when` gate that named the old label in the same undo entry and refuses a
duplicate. Reordering goes through `Editor::MoveChoice`, which swaps the two entries and carries
`default_choice` with them, because that field is an index while the gates address labels;
otherwise the default would silently point at a different choice after a reorder. The modal
edits a working copy and commits through `EditorState::Apply` like the clip modal, so Cancel
unwinds to the pre-modal undo depth.

#### Inspector "Clip" tab

`Panels::Timeline::RenderClipTab` (registered in `panel_registry.cpp` for the scene3d backend)
is the non-modal view of the selected clip: the command type, the summary, the id, the track,
the frame range and duration, a `###insp_clip_props` button that opens the modal, and the same
descriptor-driven Params form with the same live apply. Per 4.7 it also carries the command's
colour chip beside the type, the `when` gate as read-only text (`Editor::GateText`, the same
three forms the modal edits), a muted checkbox, and inline label, start and end fields, so the
common edits need no modal at all; `Properties...` opens the modal for everything else. With
nothing selected it says "No clip selected". Because the form is live, this tab needs the same gesture bracket the clip modal uses:
a `DragFloat` reports `FieldEvent::Changed` on every frame the mouse moves, and calling
`EditorState::Apply` on each of those without `BeginGesture` records one undo entry per frame, so
undoing a single drag takes dozens of Ctrl+Z. The tab opens the gesture on the first Changed,
closes it on Committed (`IsItemDeactivatedAfterEdit`, i.e. mouse release), and also closes it on
every early return, because the selection can vanish mid-drag and a gesture left open stops the
undo stack recording for the rest of the session. `clip_modal_tests.cpp` pins it: a drag in this
tab must raise `UndoDepth()` by exactly one. The "Screen parameters" centre pane is gone with `gui_preset_workspace.cpp`, so the
centre pane is the preset library on this backend (3.6), and the per-parameter surface of `PresetHost` (ListParams,
SetParam, ResetParam, ResetGroup, ResetAllParams, ChangedParamCount, ListStates, SetCountdown
and `preset_host_params.*`) went with its only caller.

#### Inspector "Frame" tab

`Panels::Timeline::RenderFrameTab` is the read-only view of what the evaluator resolved for
the frame under the playhead, and of which clip wrote each value (plan 4.7). It takes over the
Screens panel's status line and its sprite frame and scroll sliders: a live nudge would be
state outside the document that the next seek or edit throws away, so the sprite clocks are
shown and the clip's `offset` / `scroll_offset` params are what a user edits.

The data path avoids both a second evaluation and a dangling pointer. `FrameState` carries the
provenance itself: every slot has a small `from` struct of `const Doc::Clip*` (model:
visible, position, rotation, scale, alpha, anim_speed, blend_mode, spin_per_frame, motion;
sprite: the same per field plus source and scroll; camera: one per field; light, shading,
split, beat and jitter one each), written next to the value it belongs to in `eval_models`,
`eval_sprites`, `eval_camera_lights` and `WriteTarget`, so a pointer store is the whole cost
and it cannot drift from the value. Because those pointers are only valid while the render
thread's document is alive, `PresetHost::Publish` turns them into a self-contained
`Preset::Eval::FrameReport` (`BuildFrameReport`) once per frame and publishes it in
`App::PresetStatus::frame_report`; the GUI reads it out of the status snapshot like every
other live value.

The report holds entities (one per model, per sprite, per active emitter, camera, per light,
scene, options, markers), each a list of typed values with the winning clip id. `Editor::FrameRows` turns it
into the flat header / value rows the tab draws, and is unit-tested without ImGui. Numbers
mirror what is pushed: the position runs through `PlacedPosition` (jitter, orbit) and the
rotation adds the runtime spin, as `Evaluator::Rebind` does, and the model tick, sprite clock,
particle count, beat, pulse, jitter and RNG draw count come from `EvalState` (`Ran3::Draws()`
counts draws since the last seed).

Each value row's third column is a `###frame_win_<entity>_<field>` button naming the clip that
won; clicking it selects that clip in the timeline. A value no clip wrote (a document default,
an option choice value, a runtime counter) shows `-`. All three columns are `WidthFixed`, each
measured against the widest cell the report actually holds (the value column measured in the
mono font it is drawn in, the clip column plus one button's frame padding), and the table
carries `ScrollX`, so nothing is ever cut off mid-text and the pane's width only decides how
far you scroll. A proportional layout cut `sprite_split_priority` in half against the value
beside it, and it also printed the position `0, 0, -0.15` as `0, 0, -0`, which reads as a
different number. A stretch column cannot help either, because ImGui treats stretch columns as
fixed once `ScrollX` is on. The Inspector pane is narrow by default, so the third column
usually starts off screen and is reached by scrolling the table (or by widening the pane).

Fog rows: the scene entity always carries a boolean `fog`, and the four values that make it
up (`fog color`, `fog start`, `fog end`, `fog density`) only while it is on, because a document
with no `scene.fog` clip would otherwise show four numbers that affect nothing. All five name the
`scene.fog` clip that won, from `FogState::from`, so the button jumps to it. The scene row's
`clear_color` names a `render.clear_cycle` clip on the frames the cycle runs, since the cycle is
resolved after the track walk and overwrites both the colour and its provenance.

Ease rows: a model entity carries an `ease` row while a `model.ease` clip is winning it, and
the camera entity carries one while a `camera.ease` clip is, both naming that clip so the button
jumps to it. The values they move (`position`, `scale`, `alpha`, `eye`, `at`) already show the
eased number, because the accumulator is written back into the frame state before the report is
built. A model entity also carries a `mesh` row whenever the instance name and the mesh name
differ, which is the only place the second instance of a shared mesh is visible.

Emitter rows: one entity per `FrameState::emitters` clip (every Fx clip live at this frame,
whether or not it spawns on it), carrying `reach` in pixels from `RingReach`, `ring phase` in
degrees from `RingPhase` and `live`, the number of its particles currently drawn. A `burst`
emitter has no ring, so it shows `spawn: rising burst` in place of the two ring rows. Reach and
phase are the same two pure functions the spawner calls, so the readout cannot drift from what
the screen does. `live` counts `age >= 0`, which is what is on screen: a particle is created at
age -1 and is first drawn on the next frame (`docs/preset_document.md`), so the attract's
`fx_warp_in_rotating_and_zooming` reads 960 at frame 640 (60 frames of 16) while the scene row's
`particle pool` reads 976, the 61 batches the pool holds. Attribution is a `Particle::emitter`
field set to the spawning clip's id. The scene row is labelled `particle pool` so that the two
counts are not read as contradicting each other.

#### Validation UI

Every clip whose id a problem names gets a 4 px left bar in its lane, the error colour when
any of its problems is an error and the warn colour when they are all warnings, and its hover
tooltip lists the messages under the summary. The lookup is `Editor::ProblemsForClip`, a pure
function over the problem list in `src/editor/clip_problems.cpp`, so the bar, the tooltip and
the unit test all read the same thing.

An overlap concerns two clips but is reported once, so `Problem` carries a `related` id beside
its `path`: `CheckClipPair` files the message under the later clip and names the earlier one in
`related`, and `ProblemsForClip` matches either. That gives one row in the Problems list and a
bar on both clips in the lane. Without `related` the earlier clip of an overlap would look
clean, and that is usually the clip the user needs to move.

`Panels::Timeline::CurrentProblems()` runs `Preset::Doc::Validate` on the working document and
caches the result against `Editor::State::Revision()`, so it re-runs once per edit and not once
per frame. The transport row grows a red `###tl_problems` badge with the error count whenever
there is one; it opens the Problems modal, which lists every problem with its severity, path
and message and selects the clip a row names when it is clicked. Warnings use the warn colour
and never gate anything.

A problem row is not a plain `Selectable` with the message as its label. Two clip ids and an
explanation routinely run past the modal's pinned 720px, and a Selectable label is clipped at the
window edge with no ellipsis and no wrap, which hides the half of the message that names the
other clip. Each row is instead an id-only `Selectable` sized to the height the wrapped message
needs, with the text drawn over it at the same cursor position under `PushTextWrapPos`. The
Selectable is registered first so it still owns the hover and the click. `clip_modal_tests.cpp`
guards this geometrically: with a message long enough to wrap, the item rect of
`###tl_problem_0` must be taller than one `GetTextLineHeightWithSpacing()`.

### 3.6 Preset library (gui_preset_library)

The library is the only way to pick, create, import, export, save or revert a scene preset
document. It replaces the Screens tab (`gui_preset_panel.cpp`, deleted in M7) and its
countdown and per-sprite sliders, which the Frame tab now shows read-only.

It is the center pane, in place of the Scene layers view (user decision 2026-08-18). It got
there through the panel registry, not through a backend name test in the layout code: the
scene3d panel set registers `preset_library` in `PanelSlot::CenterPane`, and
`RenderViewportPane` draws the one CenterPane panel a backend registers, falling back to
`RenderScenePane` when a backend registers none. So the AFP backends keep the Scene layers view
as before and never draw a library item, and the scene3d backend gets the library over the
full height of the center column. The switch depends on a backend capability, never on a game
title; the same registry already decides the Clip / Frame / 2D package inspector tabs.

Why it moved: the library started as a section of the left pane under Browse, split off it by
a `Gui::HSplitter` (plan 4.1, decision 2026-08-15). That left the library about 120 px tall,
so the groups, the action buttons, the did-not-load list and the validation list all competed
for one column. And the Scene layers view it now replaces lists `afplist.xml` layers, which the
scene3d games do not have at all, so that pane read "Scene 0 layers, 0 bitmaps, 0 slots / No
layers listed in afplist.xml" on every IIDX 10th style or RED session. The left pane holds only
Browse again, `RenderLeftPane` and the `Gui::kLibraryShare` / `kLibraryMinH` / `kBrowseMinH`
constants are gone, and with the whole column the action buttons fit on one row.

Registry ownership: the library owns a `Preset::Doc::Registry` on the GUI thread. That does
not break the host-isolation rule (docs/gates.md): the registry only reads built-in documents
and JSON files, and it names no host. Loading a document into the renderer is the part that
touches the engine, and it goes through `PresetCmd::LoadDocument{document, game_dir}`, which
`Backend::ApplyPresetCommand` runs on the render thread with a `Backend::LoadReporter` wired
to `BeginLoad` / `UpdateLoadStage` / `EndLoad`, so asset loading reports progress through the
loading overlay. The GUI-side `Editor::State` takes the same document with a clean undo stack.
The discovery scan also reports progress (file name, done / total) through the same
overlay, because a user preset folder is walked file by file.

Scan trigger: the fingerprint (`GameFingerprint::Identify`) plus the registry scan run when
the game directory changes and after every Save or Import. With no known build the library
says so and never scans, so nothing raises the overlay for a directory that has no presets.
Only the discovery scan raises the overlay (`Rescan(true)`). The refresh after a Save or an
Import is `Rescan(false)`: it re-reads the same folder in a millisecond, and the overlay's
600 ms minimum hold (`BootLifecycle::kLoadMinHoldMs`) would otherwise put a "Loading scene
presets" panel over the app every time a user pressed Save.

Rows are grouped by `Editor::GroupLibrary` (a pure model, `src/editor/library_model.cpp`):
Built-in, User (`presets/<build>/*.json` next to `573Renderer.exe`), Other builds. A row shows
the name, and beside it "Modified" when it is the loaded document with unsaved edits, else the
error or warning count, else its id; the tooltip carries the id, the build, the length, the
marker and track counts and the file path. The filter box matches the name and the id, and a
group with no match is not drawn. Single click selects and loads, per the 2026-08-15 decision.

Item paths for tests: the library is under `main_view/pane_center` (it was
`main_view/pane_left/library_area` while it lived under Browse), its rows in the `lib_scroll`
child and the did-not-load plus validation lists in `lib_problems`; Browse keeps
`main_view/pane_left/ifs_scroll`. A child window is not addressable through a slash path from
an item ref, so a test reaches a row with `GuiTest::FocusChild` on the child first. Three tests
pin the split: `library_center_pane` (library items under pane_center, none under pane_left,
no Scene view in the centre), `library_afp_center` (an AFP backend keeps `##scene_filter` and
`scene_scroll` in the centre and has no `###lib_*` item anywhere) and `library_center_height`
(the `lib_scroll` child is more than 40 percent of the pane height, so the old squeeze cannot
come back).

| item | id | behaviour |
|---|---|---|
| filter | `###lib_filter` | matches name and id, case-insensitive |
| group headers | `###lib_group_builtin`, `###lib_group_user`, `###lib_group_other` | `CollapsingHeader`, open by default |
| row | `###lib_row_<document id>` | loads the document (prompting first when the loaded one is dirty) |
| New | `###lib_new` | Ctrl+N; makes an empty document for this build and opens Document properties with the name focused |
| Duplicate | `###lib_duplicate` | copies the loaded document under `<id>-copy` (`Editor::CopyId`) and marks it modified |
| Import | `###lib_import` | Ctrl+O; `NativeDialog::OpenFile` from anywhere |
| Export | `###lib_export` | `NativeDialog::SaveFile`; writes `Preset::Doc::Save` (canonical form) |
| Save | `###lib_save` | Ctrl+S; writes `presets/<build>/<id>.json`, creating the folders |
| Revert | `###lib_revert` | reads the document back from its file or built-in |
| Reset | `###lib_reset` | only for a built-in: back to the document the renderer ships |
| Properties | `###lib_properties` | opens the Document properties modal for the loaded document |
| problem row | `###lib_problem_<n>` | live `Validate` output for the loaded document; clicking one selects the clip it names |
| did-not-load row | `###lib_rejected_<n>` | one per `Registry::Rejected()` entry: the file name and why it was refused; hover shows the full path |

Files that did not load: a user file the registry refused is not in `Registry::All()`, so it has
no library row and would otherwise be invisible. An unparseable file and a file whose id is
already owned by another user file of the same build both land in `rejected_`.
`Registry::Rejected()` returns exactly those (unlike `Problems()`, which the CLI uses and which
also reports loaded documents that only carry validation errors), and the problem list prints
them above the loaded document's own validation rows under a "Files that did not load"
heading. Clicking these rows selects no clip, because they have none. Their message is the
parser's or the registry's own text with nothing prefixed, for the same reason the import modal
prefixes nothing.

Vertical layout of the section: the row list and the problem list are two children that share
whatever the section has left after its header, build line, filter and action rows. The
problems child takes 30 percent of it, clamped between one and three text lines plus one
`ScrollbarSize`, and the row list takes the rest with a one-line floor. The scrollbar allowance
is needed because a rejected-file message carries an absolute path, so that child always shows
a horizontal scrollbar, and a bound of exactly three lines spent one of them on the bar and cut
the last "Files that did not load" row in half. Neither child has a fixed pixel height: a fixed
floor on the row list pushed the problems child past the bottom of the section on a short
window, and the problem list then could not be reached at all, since a section is not itself
scrollable. Both children scroll, so a long list of rows or of problems stays reachable at any
pane height, and the HSplitter over the section is how a user trades Browse height for library
height.

Save rules. Built-in ids are reserved (a user document that claims one is a validation error,
docs/preset_document.md), so Save on a built-in does not write in place: it opens "Save as user
copy" (`###lib_saveas_id`, `###lib_saveas_ok`, `###lib_saveas_cancel`) prefilled with
`<id>-copy`, applies the new id to the document and writes that. Ctrl+Shift+S opens the same
modal for any document. A successful write calls `Editor::State::MarkSaved()`, so the row's
"Modified" mark and the transport badge's `*` clear together, and rescans so the new file
appears under User.

Import rules (the load rule of docs/preset_document.md): a file that does not parse blocks.
The "Import problems" modal shows the path, `ParseError::message` as it stands, a Copy report
button (`###lib_import_copy`) and Close (`###lib_import_close`), and nothing is loaded. The line
and column are not prefixed to it: nlohmann already opens its own text with "parse error at line
L, column C", and prefixing produced "line 4, column 9: [json.exception...] parse error at line 4,
column 9: ..." on screen. `ParseError::line` and `::column` stay on the struct for callers that
want the numbers on their own. A file that parses but fails validation loads
anyway: the editor and the renderer take it, the evaluator skips the offending clips, and the
problem list under the row list names them. An imported document for this build whose id is
not a built-in one is also copied into `presets/<build>/`, which is what makes it a User row.
An id that already belongs to a different user file is renamed on the way in
(`Editor::UniqueId`, so `remix` becomes `remix-2`), and the same modal reports the rename with
its "the document is loaded and saved under the free id" note. Without that step the copy into
`presets/<build>/<id>.json` would overwrite the file that owns the id. Re-importing the same
file that owns it (`std::filesystem::equivalent`) is not a clash and keeps the id, which is
what makes an Export followed by an Import a round trip. A built-in id still loads unrenamed
and is not copied, so the validation error stays visible until the user renames it.

Other builds open read-only. Selecting a row whose `build` is not the current fingerprint calls
`Editor::State::LoadDocument(document, true)` and posts no `PresetCmd::LoadDocument`, so the
renderer keeps whatever it was showing: the document's assets cannot be resolved against this game
directory, and a document for another build is never silently loaded as if it were for this one.
`Editor::State::Apply`, `Undo` and `Redo` all refuse while `ReadOnly()`, which is the one hard
guarantee (every edit in the editor goes through `Apply`). In addition, the request pump in
`gui_tl_editor.cpp` drops every modal request, the transport disables `Add command` / `Add track`, and
the library disables Save, Revert, Reset and Properties. All of them carry the same reason string,
`Read-only: built for <build>`, which the library also shows in place of the build name and the
transport shows beside the add buttons.

The editor is still drawn for such a document, since that is why the user opened it.
`Panels::Timeline::Active()` normally requires the render thread to be playing the same
document id, and a read-only document is never sent to the render thread, so the plain id test
hid the editor completely and left the reason string, the disabled add buttons and the tracks
themselves unreachable. `Active()` therefore returns true whenever the editor holds a read-only
document. Because the renderer is meanwhile showing something else, nothing in the editor may
command it or hand it this document: `PostSeek`, `PostPaused` and `PublishDocument` do nothing
while `ReadOnly()` (one choke point that covers the ruler drag, the playhead, the transport keys
and the buttons alike), the transport disables the jump / step / play buttons, the loop
checkbox, the edge jumps and the document badge, and `Ctx::status` is not the live
`App::PresetStatus` at all but a stand-in built from the document itself (frame 0, its own
`length` and `fps`, not playing), so the ruler and the playhead show this document's axis
instead of the playing one's.

Duplicate and Export stay live on a read-only document. Duplicate retargets the copy to the
current build (`document.build` becomes the current fingerprint, and the id is deduped against
this build's ids), because that is the only outcome that can be both edited and loaded here: a
copy that kept the foreign build would open read-only again, and Save would write it into
another build's folder. The Duplicate tooltip says so whenever the loaded document is foreign.

Modified prompt: switching to another row, New, Import, Revert, Reset and a window close request
while the loaded document is dirty open "Unsaved changes" (`###lib_prompt_save`,
`###lib_prompt_discard`, `###lib_prompt_cancel`) instead of acting; Save writes first and then acts,
Discard acts, and Cancel drops the pending action.

Closing the window: the GUI window's `WM_CLOSE` is not passed to `DefWindowProc` while
`App::State::CloseNeedsPrompt()` is set (the library publishes `Loaded() && Dirty()` into it every
frame). Instead it calls `PostCloseRequest()` and returns 0, so nothing is destroyed.
`PumpCloseRequest()`, run from `RenderModals()` every GUI frame, turns that request into the
same prompt, and Discard or Save calls `ConfirmClose()`, which is what `Gui::PumpAndRender`
reports as "stop", so the GUI thread shuts the window down as before. Cancel calls
`ClearCloseRequest()` and the app keeps running. With nothing unsaved the request is confirmed
immediately, so a clean close behaves as it always did. A second `WM_CLOSE` while a request is
already pending always falls through to `DefWindowProc`, so the window can never be trapped by a
prompt that nothing is drawing. Actions are deferred by one frame through
`Library::pending_action`, so a modal is never opened from inside the child window that asked
for it.

Shortcuts owned by the library (active whenever it is drawn, no popup is open and
`io.WantTextInput` is false): Ctrl+S save, Ctrl+Shift+S save as, Ctrl+O import, Ctrl+N new,
Ctrl+E export video. Ctrl+E is handled here as well as in the old dock because the dock is not
drawn while the timeline editor is up, and the export modal must stay reachable.

`Panels::PresetLibrary::SetUserRoot` overrides the scan root. The app never calls it and
`gui_tests` does, so a test scans a temp folder instead of the folder next to the exe.

## 4. Export modal + status tag (gui_export_panel)

- The export form is a modal ("Export", centered, 620px), opened from the top-bar button,
  Ctrl+E, or the status-strip tag. It takes no permanent space in the shell.
- Sizing pitfall: the window uses `AlwaysAutoResize` for height, but its width must be pinned
  via `SetNextWindowSizeConstraints`. The form is full of widgets that stretch to the
  available width (`SetNextItemWidth(-FLT_MIN)`, `GetContentRegionAvail()`), and auto-resize
  and stretch widgets feed back into each other: every frame the window measures that the
  content wants more room and grows until it is as wide as the screen. Pinning the width
  breaks the loop.
- Height pitfall: the max-height constraint is the viewport work height minus a margin, and
  the position is re-centered every frame (`ImGuiCond_Always`, 0.5 pivot). Auto-resize
  windows never scroll on their own; with a position set only on Appearing, expanding Advanced
  grew the modal past the bottom edge and hid the Start/Close footer. With the height capped,
  the scrollbar takes over, and centering every frame keeps growth symmetric. The cost is that
  the modal cannot be dragged by the user, which is fine because the crop-pick flow closes it
  anyway.
- Common path: filename stem + format, fps + quality, output resolution (preset combo +
  WxH + scale buttons), transparent-bg + HW-accel. Everything else (keyframe interval,
  frame limit, loop count, blend seam, crop, bg color) sits behind one "Advanced"
  CollapsingHeader.
- Escape closes the modal, guarded on the modal being the focused window and
  `io.WantTextInput` being false, so a field's Escape-to-revert and a combo's
  Escape-to-dismiss still take priority. `BeginPopupModal` with `p_open == nullptr` has no
  Escape handling of its own, which is why this is explicit.
- Starting an export closes the modal; progress shows in the status strip (capturing N /
  encoding / done / failed, clickable to reopen) and as the timeline capture tint.
- The scale buttons build their label and their `##` id into one `snprintf` buffer that must
  stay wide enough for both ("x0.25 (480x270)##exp_scl_x0.25##exp_scl" is 39 bytes). A short
  buffer truncates the id, not the visible text, so a collision would show up as two buttons
  sharing one id with nothing on screen to explain it. Found by `gui_tests`.
- Crop pick handshake: arming "Pick region" closes the modal (the drag happens on the render
  window), and the modal reopens on its own when pick mode ends (`g_reopen_after_pick`).
- With a preset document loaded (the `App::PresetStatus` id matches the editor's document) the
  modal takes its defaults from it, because a document already says how long the screen is and
  at what rate it is authored: opening the modal sets fps to `document.fps`, and the Advanced
  block gains "Document range" (`Document range##exp_preset_range`), which exports the ruler's
  export range (Shift+drag, `Editor::View::export_range`) or the whole document. It fills
  `ExportRequest::start_frame` and `max_frames`, and `Scene3dCaptureDriver::BeginCapture` seeks
  the host to `start_frame` and plans `length - start_frame` frames. Turning it off falls back
  to the plain frame limit.
- The export fps must be an integer multiple or divisor of `document.fps`
  (`Editor::FpsRatioAllowed`): the capture advances the document clock by wall time, so 45 fps
  out of a 60 fps document would advance one document frame on some captured frames and two on
  others. A bad ratio prints a red note beside the fps field and disables Start with a tooltip
  saying why (rule and rationale in docs/export_pipeline.md).
- Form state is file-scope statics so choices survive modal close and IFS reloads. The whole
  form is disabled during capture so a click mid-capture cannot change values already
  snapshotted into the export session; the crop rect is snapshotted at start for the same
  reason. Numeric crop edits write state only on change to avoid racing the drag handler.
- The filename stem regenerates automatically only on an IFS or animation change. Output
  resolution (0,0) = "native at submit time"; a two-static state machine (shown_idx + last
  dims) keeps a user's "Custom" pick from being reverted by the auto-match on the next frame.
  Setup's render-resolution widget uses the same pattern.
- Encoder facts in the tooltips: AVIF = AV1 dual-stream with an auxiliary alpha
  plane (img-tag transparency; color stream NVENC on RTX 40+, alpha stream stays software);
  WebM VP9 = yuva420p single stream (video-tag transparency; software only, since no NVIDIA
  desktop GPU ships a VP9 encoder); WebM AV1 = NVENC-capable and smooth in a video tag, but
  opaque only; WebP anim = alpha + smooth img-tag playback everywhere, ~2-3x larger; PNG
  sequence = lossless frame_NNNNNN.png folder via WIC. video-tag playback beats animated img
  for long clips because Blink clamps animated-image frame durations to 10 ms.
- Keyframe interval: shown only when `MediaSink::UsesKeyframeInterval(format)`; 0 = auto
  (one keyframe/sec); a larger value or one >= the frame count forces a single keyframe.
  Feeds `Request::export_keyframe_interval` -> encoder `gop_size` (docs/media_formats.md).
- H.264 export requires h264_nvenc on this ffmpeg build (no libx264/openh264 compiled in).
  av1_nvenc needs Ada (RTX 40) or newer; the HW probe is format-specific.
- "Blend loop seam" is the only sanctioned pixel-compare feature: a synthesized crossfade the
  user opts into, for backgrounds with no clean authored loop. The real game does not do this.

## 5. Debug-viewer parity controls (engine-call mappings)

The live controls mirror KONAMI's CAfpViewerScene key bindings. Each GUI control drives the
same engine call as the scene's key:

| Control (scene key) | Engine call |
| --- | --- |
| timeline track + step buttons (LEFT/RIGHT; SHIFT = 100-step) | absolute-frame seek via afp_mc_control 0xF08 (deep_goto_play) on the master mc; seeking pauses playback, as the scene does |
| play/pause button + Space (RETURN+SHIFT) | stream playback speed 0 / 1 via afp_stream_set_speed, re-applied each frame by the render thread |
| Background Segmented (F4) | preview RT clear color: default transparent -> grey -> black -> red -> green -> blue (kBgPresets); live preview only, export has its own bg |
| Filter toggle (F7) | afp-core set-filter ord 0x032 on the active stream, filter id 0x80000000 OR enable, the same call the scene's CLayer slot-32 wrapper makes |
| Show MC names (F3 DISP MC) | afp_mc_enumerate_children ord 0x079 on the master; name-type (F6): "at clip pos" = draw each name over the preview, "column" = the Live tab list; also feeds Scene-tree positions |
| timeline frame readout | the bounded mc playhead (work+0x76 via afp_mc_set 0x1010), shown the same way as in the IIDX debug viewer |
| "loop N" readout | our own count of backward wraps since the clip/label last (re)started, not afp's work+0x104; the debug scene shows no loop_count |
| Live tab size readout | afp_get_layer_info word[8]/word[9] |
| timeline label markers | current label name + the label list (goto via label playback) |
| Live tab raw cur/total | free-running stream counter afp_get_layer_info w[13]/w[12], which climbs past total under the continuous-loop dance (diagnostic) |
| Live tab file info | afp-utils package metadata; version triplets unpack as major=(v>>16)&0xFFFF, minor=(v>>8)&0xFF, patch=v&0xFF (matches the scene's printf). Locale is left out on purpose: it is the real game's runtime region from a bm2dx CRT call and cannot be derived from afp/avs |

Other live-control semantics:

- Continuous loop OFF/default/ON: OFF = clear the CLayer flag (the master falls back to
  gotoAndStop saturation), default = leave the engine default, ON = apply the BG-dispatcher
  flag sequence (the master keeps advancing past total_length while sub-clips run as authored;
  BG 20 needs this). Re-applied on the next stream switch.
- Trim frames: >0 restarts the master via ForceReplay at rendered frame N since the last
  switch (to watch an exact-length loop at full framerate).
- DDR gating: DDR's afp 2.13.7 exports none of the modern-only knobs (continuous-loop flag
  dance, trim ForceReplay, afp_set_filter, bulk child-enumerate), so those widgets are absent
  from the DDR panel sets; Background, Reset and the Live readout stay (each gated on its
  own have_* flag).

## 6. Loading overlay and progress rules

- Full-screen dimmed overlay with `ImGuiWindowFlags_NoInputs`. Hit-testing is suppressed, so
  drawing it after the regular view is enough to make everything non-interactive.
- A determinate bar when a fraction is known (e.g. textures_expected counted in advance from
  texturelist.xml); otherwise an indeterminate marquee plus a changing detail line (current
  item or climbing count) so it never looks frozen. Logging on state changes records overlay
  ON/OFF transitions in renderer.log without a line per frame.
- The arc/customize extractor panels use the same two-pass pattern: pass 1 (scan,
  total unknown) shows an animated bar + "Scanning... N found" + the current path; pass 2
  shows done/total. This implements the project-wide progress rule (see CLAUDE.md).

## 7. Setup view

- Centered bordered card (640px, `ImGuiChildFlags_AutoResizeY`, vertically offset ~35% of
  the free space) instead of a top-left form; brand header inside the card.
- Game profile combo: Konami ships per-game builds of afp-core.dll that share the export-name
  scheme but differ in the ordinal-to-function mapping; the profile selects ordinals and
  boot-call gating. Auto-detect matches known substrings in the game dir path; the Auto
  label previews what it would pick.
- Render fps: controls the live loop tick rate and the per-tick dt (dt = 1/fps), so animation
  speed is the same at any rate. DDR content is authored at 60; IIDX/SDVX previews
  historically ran at 120.
- Render resolution presets are named after their games (3840x2160 GITADORA 4K, 1920x1080
  IIDX, 1280x720 DDR, 640x480 legacy 4:3, 1080x1920 SDVX / jubeat portrait, 720x1280 SDVX
  old-era, 520x704 qpro avatar) because AFP's GetScreenSize callback must
  match the offscreen RT shape or layout coords land wrong. Values clamp to 64..8192.
  640x480 carries no game name because it is the generic pre-HD arcade size and not any
  one title's native mode. The export panel's output-resolution list carries the same 640x480
  entry, in the same position relative to its trailing Custom row.
- No resolution preset is gated on anything; every preset is offered for every game. The
  qpro entry used to be hidden unless the slug was literally `iidx33`, so it vanished as soon
  as IIDX 34 got its own profile, and the picker has no reason to decide which sizes a user
  may want. A `[gui][setup]` test picks the qpro size while a DDR profile is selected.
- The resolution shown on Setup is the one Load uses. `Load` posts its BootGame with
  `size_explicit` set, because a size sitting in the picker is a size the user chose.
  Without it `BootFromGameDir` replaced it with the profile's own default, and the picker
  showed a size the render never used. Picking a game profile sets the resolution to that
  profile's default in the same click, so switching to IIDX 27+ still lands on 1920x1080 and
  the picker keeps showing what will happen. Two `[gui][setup]` tests cover it: one checks
  that Load carries the chosen 520x704 through, the other that choosing a profile moves the
  picker.
- Settings persist to settings.ini on every change (atomic tempfile + rename).
- The DDR-only Tools (batch .arc extractor, customize image extractor; docs/ddr.md) render
  inside the card, gated on the effective setup selection (the explicit combo slug, else
  auto-detect of the typed dir).

## 8. qpro view

Reached via the top-bar view switch (iidx33 only; registry MainTab entry).

- Drives the qpro extractor (docs/qpro.md): the category checkboxes combine with the per-part
  date-grouped scan selection (a part renders iff its category is on and the part is checked).
  A scan that selects nothing blocks extract; no scan = all parts.
- Body parts need the 520x704 render size (Setup preset); the tab shows a warning otherwise.
- The skip/failure Issue list is shown prominently with a copy-to-clipboard button, so a skip
  is never silent.
- Uses ImGuiListClipper for the per-part checkbox lists (thousands of rows).

## 9. History: locale overlay became "Load alongside"

The original locale companion picker was a GUI card that offered `<base>_{j,a,k}.ifs` as an
exclusive choice, inferred from that naming rule (`AfpCmd::ToggleCompanion`,
`IGameRuntime::ToggleCompanion`, `App::CompanionIfs`, `IfsInspect::FindCompanions`). All of
that is gone: nothing infers a locale, nothing is exclusive, and there is no dedicated card.

The Browse pane's "Load alongside" context menu (3.1) replaced it: any .ifs in the
tree can be stacked on the loaded one, several at a time, and each is unloaded on its own.
The engine machinery underneath is the same as before
(`AfpManager::LoadCompanion` / `UnloadCompanion` / mount aliasing), now with a named-overlay
layer on top (`AfpManager::LoadOverlay` / `UnloadOverlay` / `OverlayPaths`, keyed by path).
qpro still loads its co-present part packages through the raw companion calls
(docs/qpro.md), and the bm2dx name-shadowing facts stay in
docs/boot_and_render_loop.md "Companions".

## 3D scene viewer (IIDX 17 / 18)

Directories that contain a `.inz` manifest plus at least one `.xz` model are
listed in Browse with a `[3D scene]` suffix. Selecting one loads it through the
normal content path: the backend detects a scene directory in `LoadContent`,
hands it to `Scene3dHost`, and `RenderScene` draws the scene instead of the AFP
content until a normal package is loaded again.

A "3D scene" tab appears in the inspector while a scene is live. It shows the
model / tile / draw-call counts and carries:

- "Pause" and a speed slider, plus a scrubber over the animation in `.x`
  AnimationKey ticks (the scene loops at its longest key time).
- "Animate models" and "Animate camera" freeze each part independently, holding
  whatever pose it was in instead of snapping to tick 0. Use them to stop a scene
  orbiting or a layer drifting while the rest keeps running; Pause stops the clock
  for everything at once. "Animate camera" is disabled when the scene has no camera
  in its `.x`, or while free camera is driving the view, with a tooltip saying which.
- A "Models" list with a per-model visibility checkbox and a blend-mode combo
  (opaque / alpha / additive / subtract). The blend value shown is the one the
  game's per-screen setup code assigns; the combo overrides it so a layer can be
  isolated or inspected.
- A "Free camera" toggle. Off uses the camera animated inside the `.x` file;
  scenes without one (`resort_st`, `boss_st`) start with free look on because it
  is the only way to see them. "Reset view" re-frames from the scene bounds.
- A "Move speed" drag, seeded from the scene's bounding radius so a 10-unit
  scene and a 10000-unit scene both feel the same.

Camera controls, handled in `src/scene3d/scene3d_input.cpp`:

| input | action |
|---|---|
| hold right mouse button | look around (cursor is hidden and re-centred each frame) |
| W / A / S / D | move forward / left / back / right |
| E or Space | move up |
| Q or Ctrl | move down |
| Shift | 5x faster |
| Alt | 5x slower |

Movement keys only apply while the right mouse button is held, so the keyboard
stays free for the rest of the UI. The look handler runs before the normal
window proc and swallows only the messages it uses, so crop-pick and the other
window interactions are unaffected. Pitch is clamped just short of vertical to
avoid gimbal flip.

## 2D package viewer (IIDX 17)

Directories holding a `system.idx` or `system.idr` are listed in Browse with a
`[2D package]` suffix and load through the same content path as 3D scenes. A
"2D package" tab appears in the inspector while one is live, showing the cell /
record / animation / tile counts and the quad count for the current frame.

- An animation combo listing every animation the package names. A package's
  first animation is often not its content (see `docs/game_profiles.md`), so
  without the combo some packages look broken.
- "Pause" and a speed slider, plus a frame scrubber over the current
  animation's length. Dragging the scrubber pauses first, so the playhead stays
  where it was put.

`--animation <name>` picks the starting animation from the command line, which
is what the headless screenshot path uses.
