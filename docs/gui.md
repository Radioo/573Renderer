# GUI architecture and debug-viewer parity

Knowledge captured from `src/gui/*`. The GUI mirrors
KONAMI's own in-game AFP debug viewer (`CAfpViewerScene` in bm2dx) wherever a control maps to
an engine call - those mappings are engine facts and are listed in section 5.

Layout lineage: the current design is the "Timeline Studio" direction with the "Scene graph"
center pane, chosen over a control-deck grid, an embedded-monitor center, and a no-right-pane
workspace. Squared styling (all rounding 0) and the export modal are deliberate decisions from
that round, not defaults.

## 1. Threading model

### 1.1 Dedicated GUI thread (gui_thread)

The GUI runs on its OWN thread, separate from the render thread. Reason: dragging a Win32
window by its title bar (or resizing via an edge) enters a "modal move loop" inside
DefWindowProc that only returns on mouse-up - anything on that thread stalls for the duration,
including the render loop and AFP's per-frame tick. A separate GUI thread isolates that modal
loop: the render thread keeps running at its cadence while the GUI thread is stuck in the drag.

- The two threads communicate EXCLUSIVELY through `App::State` (mutex-guarded). Each thread
  owns its own window, D3D9 device and ImGui context - zero cross-thread ImGui/D3D9 sharing.
- The GUI thread exits when (a) the user closes the GUI window - the renderer then continues
  HEADLESSLY (the GUI is a control panel, not the app) - or (b) `App::Global().ShouldExit()`
  flips (render window closed); main.cpp joins the GUI thread before final cleanup.
- The panel loop is rate-limited to ~60 Hz via Sleep(16) - it does not need the renderer's
  120 Hz and burning a core on an idle ImGui loop is wasteful.
- `GuiThread::Start` is idempotent (atomic exchange); `Stop` flips the local running flag and
  joins, letting the GUI stop selectively without tearing the whole app down.

### 1.2 Command/Status pattern

GUI widgets never call the engine. They post typed `App::Command` variants
(`Cmd::LoadContent`, `Cmd::BootGame`, `Cmd::StartExport`, `Cmd::CancelExport`) and wrapped
AFP-backend commands (`AfpCmd::Wrap` over `SeekFrame`, `SetPaused`, `SwitchAnimation`,
`GotoLabel`, `ForceReplay`, `QproStartScan`, `QproStartExtract`) that the
render thread consumes (see docs/state.md "Command semantics"); the render thread publishes
`App::Status` / LiveState / ExportState / LoadProgress snapshots that the GUI polls once per
frame. This seam is also what makes the GUI testable headlessly: `gui_tests` prefills
`App::State`, clicks a widget through the Dear ImGui Test Engine, and asserts on the command
that came out - no window, no device, no game data (docs/gui_tests.md). Keeping a widget
free of direct engine calls is therefore a testability requirement, not just tidiness.
Background workers (arc/customize extractors, qpro scan) publish into their own
mutex-guarded Status structs polled the same way.

### 1.3 gui_window internals (Win32 + DX9 lost-device dance)

- The modal border-drag loop runs inside DispatchMessage and blocks PumpAndRender until
  mouse-up, so live resize must be serviced from INSIDE the message handler: WM_SIZE queues a
  backbuffer resize AND renders one frame immediately (`RenderFrameLocked`, re-entrancy
  guarded) so the panes track the cursor during the drag. WM_PAINT renders a real frame +
  ValidateRect; WM_ERASEBKGND returns 1 (skip GDI erase, avoids resize flicker).
  SIZE_MINIMIZED reports a 0x0 client that must NOT be reset to.
- DEVICE FALLBACK: `CreateDevice` tries a D3D9 HAL device (hardware then software vertex
  processing) and, if both fail, falls back to a D3D9-on-12 WARP device on the same HWND
  (`WarpD3D9::CreateForWindow`, the same path `pixel_golden_tests` uses). Without it the
  control panel simply refuses to start on machines with no D3D9 HAL driver - VMs, some RDP
  sessions, bare Windows Server. `Window::warp_backed` records which path won so `Shutdown`
  does not double-release a device the `WarpD3D9::Device` member owns. This is also what lets
  `window_tests` run the real window on hosts without a HAL device (docs/gui_tests.md 14).
- `Gui::Shutdown` destroys the window, so `WM_DESTROY` posts a WM_QUIT to the THREAD queue.
  That is what ends the GUI thread when the user closes the panel - but it also means any
  code that calls `Gui::Init` again on the same thread must drain the queue first, or the
  next `PumpAndRender` sees the stale quit and returns false immediately.
- `ResetDevice` re-reads the client rect into the present params EVERY time, so a device-lost
  that happens AFTER a resize restores at the current size instead of snapping back to the
  creation size. ImGui's font atlas + buffers live in D3DPOOL_DEFAULT, so
  `ImGui_ImplDX9_InvalidateDeviceObjects` before Reset and `CreateDeviceObjects` after
  (standard DX9 lost-device dance).
- WM_GETMINMAXINFO clamps the min window size to the layout constants (fires during
  CreateWindow while the module global is still null - fine, the constants are static).
- io.IniFilename = nullptr (window layout not persisted). The GUI HWND is exposed via
  `Gui::GetHwnd()` for native dialog parenting.

## 2. Style, fonts, icons (gui_style, gui_icons, gui_widgets)

- SQUARE EVERYTHING: every rounding in the style struct is 0 (window, frame, grab, scrollbar,
  tab, popup, child). Hairline 1px borders on popups/child cards only; surfaces are separated
  by four fill levels (ground / raised / overlay / sunken) instead of borders.
- Fonts (all loaded from `%WINDIR%\Fonts`, GetWindowsDirectory, never hardcoded):
  - base = Segoe UI 16px (fallback Consolas, then ImGui default),
  - header = Segoe UI Semibold 18px (`seguisb.ttf`, fallback bold, fallback base),
  - mono = Consolas 13px for everything that changes per frame (playhead, flags, hex ids) so
    digits do not jitter,
  - icons = **Segoe Fluent Icons merged into the base atlas** from the SYSTEM font
    (`SegoeIcons.ttf`, fallback `segmdl2.ttf` on Win10) - PUA range 0xE700-0xE950,
    `ImFontConfig::MergeMode`; zero bundled assets, nothing to license. Glyph escapes live in
    `gui_icons.h`; only icons with a live consumer are defined there (dead-code rule).
- imgui is 1.92+: `PushFont` takes (font, size); `Gui::PushHeaderFont()` / `PushMonoFont()`
  wrap `PushFont(f, f->LegacySize)` so call sites stay one-liners. Fonts are added BEFORE
  `ImGui_ImplDX9_Init` (the backend uploads the atlas on init).
- Glyph ranges: 0x0020-0x00FF plus 0x2010-0x2027 / 0x2030-0x205E (quotes/ellipsis); the
  range arrays are static (ImGui keeps the pointer).
- PER-GAME ACCENT: `Gui::ApplyAccentForProfile(slug)` re-derives every accent-tinted color
  (buttons, headers, tabs, checkmarks, sliders...) from one accent per game-profile slug
  prefix: sdvx cyan, iidx blue, ddr gold, gitadora red, jubeat silver, default blue. Called
  once per frame from `Panels::Build` with a change-guard, so the window always signals which
  game is loaded. Neutral surfaces never change - only the accent mix does.
- Shared widgets (`gui_widgets`): `Segmented` (row of mutually exclusive small buttons; used
  for root-loop, continuous-loop, background, MC-name-type, and the main-view switch) and
  `SectionHeader` (icon + header-font title + dim suffix + separator).

## 3. Layout

Ready view = fixed shell, top to bottom:

1. **Top bar**: brand (header font, accent), main-view switch (only when >1 view: Renderer /
   qpro), active IFS path (mono), then right-aligned Export button + measured-fps readout.
2. **Three panes** split by two draggable `Gui::VSplitter`s:
   - left = Browse (IFS tree), center = Scene (clip hierarchy), right = Inspector (tabs).
   - Side-pane widths are session-only; the CENTER pane absorbs the remainder; when the
     window shrinks below fit, the center steals from the right pane first, then the left.
   - `VSplitter` is built on `ImGui::InvisibleButton` only (no imgui_internal); a drag moves
     pixels symmetrically and commits only if BOTH neighbours stay >= their minimum.
3. **Timeline dock** (fixed height `kTimelineH`): transport + custom track, section 4.
4. **Status strip** (`kStatusStripH`, mono font): profile name, render WxH, render health
   (last_error, tooltip carries the full text), the export status tag (clickable, reopens
   the export modal), right-aligned afp version. Once an export finishes, an "Open folder"
   button appears beside the status tag and inside the export modal; both call
   `NativeDialog::RevealInFileManager`, which uses `SHOpenFolderAndSelectItems` to open
   Explorer with the output SELECTED, falling back to opening the parent directory when the
   item cannot be resolved. Relative export paths are made absolute first, and a PNG-sequence
   export (which writes a directory) is selected the same way.

- Layout constants live in `gui_layout_constants.h` so WM_GETMINMAXINFO and the pane layout
  cannot drift. Pane minimums: left 240, center 320, right 280; defaults 300/340; splitter 6.
- Views: BootState WaitingForDir/Booting/Failed -> Setup view; Ready -> the shell above. The
  loading overlay renders on top of EITHER view from the same LoadProgress API.
- Keyboard (active when no text input is focused): Space = play/pause, Left/Right = step 1,
  Shift+Left/Right = step 100, Ctrl+E = export modal. Handled in the timeline dock; steps are
  suppressed while an export captures.

### 3.0 Panel registry - backends register their panels

The main-view switch and the inspector tab set are REGISTRY-DRIVEN
(`src/gui/panel_registry.{h,cpp}`): `PanelDesc{id, tab_label, slot, draw, visible}` entries in
per-backend `PanelSet` tables keyed by `App::State::ActiveBackendId`. Slots:

- `MainTab`: whole-shell views. afp_modern = Renderer + qpro (visible predicate
  `GetGameProfileSlug() == "iidx33"`); afp_ddr = Renderer only. With one visible entry the
  top-bar switch does not render.
- `InspectorTab`: right-pane tabs. afp_modern = Properties / Render / Live; afp_ddr =
  Render (reduced draw fn) / Live / 3D scene; scene3d = 3D scene / 2D package. The last two
  carry visible predicates (`Scene3dHost::Active()`, `Gc2dHost::Active()`) so only the tab for
  the kind of content actually loaded is present.

Rules unchanged from the original registry: MEMBERSHIP IS THE CAPABILITY DECLARATION (an
unsupported control is ABSENT, never greyed); ImGui stays 100% inside src/gui/ (isolation
gate); a future backend adds a `PanelSet` row plus draw fns, no shell changes. The Scene pane
and timeline are shared shell surfaces whose per-backend differences are DATA-driven: DDR
publishes no afplist names, no mc_tree, no slots, so those tree features simply do not appear.

### 3.1 Browse pane (IFS picker)

- The IFS tree is rebuilt from the entry list EVERY frame (linear in entries x depth, fine for
  ~6000 files). ImGui ID GOTCHA: pushing the entry POINTER as the row ID broke clicking -
  the rebuild gives a different pointer each frame, so press (frame N) and release (frame N+1)
  had different IDs and ImGui never registered the click. Fix: PushID on the full path STRING.
- Auto-expand is keyed off the WHOLE tree size (<= 20 files), not per-subtree: a tiny install
  auto-opens while a real SDVX/IIDX install stays collapsed.
- Active-row highlight compares the FULL absolute path (Status::current_ifs_path), not the
  basename - installs have same-named IFSes across subdirs.
- The .ifs directory scan runs on a background thread; the pane shows the scan's live status
  line instead of "none found" while running.
- File leaves post `load_new_ifs` requests carrying `from_arc` for DDR .arc-wrapped IFSes.

### 3.2 Scene pane (gui_scene_panel) - the clip hierarchy

One tree replaces the old Layers + Sub-layers + Variants panels:

- Top level = afplist.xml `<afp>` entries (KONAMI's own debug viewer labels this exact list
  "[LAYERNUM]"). Single-click SELECTS (drives the Properties tab); DOUBLE-CLICK plays /
  replays (same-name picks route through ForceReplay in the dispatcher, a valid replay).
- Under the PLAYING layer: the live child tree from `afp_mc_enumerate_children`
  (Status::mc_tree), with per-clip visibility checkboxes, positions (when MC-name enumeration
  is on - the same data as F3), and an accent "variant" badge on nodes matched by a
  VariantSlot (`slot.path == node.path or node.name`).
- Sub-clip semantics preserved from the old sub-layers panel: expansion is LAZY (opening a
  node writes its path into the State expand-set the render thread reads to enumerate on
  demand); ImGui row IDs use the SIBLING INDEX because afp clips can have same-named siblings
  whose paths collide; the path stays the afp toggle key, so same-named siblings toggle as a
  group - matching the game (SetClipVisible walks the dir-6 same-name chain). Absent
  override = authored visible.
- Slots that do not correspond to an enumerated node render as ghost rows under the playing
  layer ("(unresolved slot)" until the render thread's probe resolves them).
- Bottom row: add-slot-by-clip-path input (probed by the render thread next frame).
- Selection model (`Panels::Scene::Selection`, GUI-thread-local): None / Layer / Child with
  path + name; reset on IFS switch.
- COMMIT SEMANTICS: `InputInt`/`InputText` return true on EVERY keystroke unless given
  `ImGuiInputTextFlags_EnterReturnsTrue`. Any numeric field whose value ESCAPES the panel -
  to `App::State`, to `SaveCurrentSettings()`, or to a live override - must therefore stage
  the value and commit on `ImGui::IsItemDeactivatedAfterEdit()`, not on the widget's return
  value. Setup fps and render W/H and the inspector trim field do this. Fields that only
  feed a panel-local static (the whole export modal) are fine committing per keystroke.
  Two traps: a stepped `InputInt` (step != 0) appends -/+ buttons, so
  `IsItemDeactivatedAfterEdit()` must be taken after wrapping the call in
  `BeginGroup`/`EndGroup` or it reports on the "+" button; and any sibling button that sets
  the same value (the quick-fps row) still has to commit immediately.
- DISABLED TOOLTIPS: `IsItemHovered()` returns false for an item inside `BeginDisabled`, so
  a tooltip attached after `EndDisabled` never shows in exactly the state it is usually
  written to explain. Every tooltip on a gateable control passes
  `ImGuiHoveredFlags_AllowWhenDisabled`. Both live cases (3D **Animate camera**, export
  **HW accel**) exist to say WHY the control is greyed out.
- `IsItem*` GOTCHA: `RenderSceneNode` LATCHES `ImGui::IsItemToggledOpen()` into a local on
  the line after `TreeNodeEx`, before anything else is submitted. `IsItem*` reads
  `g.LastItemData`, which every later `ItemAdd` overwrites - and this row submits more items
  after the tree node (the `(x, y)` position text, the variant badge). Querying it at the
  bottom of the function silently dropped the expansion write for exactly those clips that
  had a position or a badge, so lazy enumeration never fired for them while a bare clip
  worked. Found by `gui_tests` (docs/gui_tests.md section 10); do not re-inline the call.
- DDR: no afplist, no bulk child-enumerate in afp 2.13.7, so the pane shows only the
  no-layers hint; everything else is driven by absent data.

### 3.3 Inspector pane (gui_inspector)

- **Properties** (afp_modern only): the Scene selection. Layer -> kind, frame count,
  playing state, Play/Replay button. Child -> full clip path (mono), position when known,
  and when a VariantSlot matches: slot-visible checkbox + the bitmap swap combo.
  Variant semantics (engine facts): "(default)" cannot un-override a clip -
  `afp_play_work_load_bitmap` only REBINDS (there is no "restore authored" path), so
  "(default)" drops the override locally and posts `force_replay` so the master timeline
  re-authors the clip's original bitmap; other latched slots re-apply next frame.
- **Render**: loop master (persisted), root-loop Segmented, continuous-loop Segmented
  (OFF/default/ON = -1/0/1), trim frames, master scale slider + 1.0x/1.5x presets,
  background Segmented, filter (F7) and MC names (F3) toggles + name-type Segmented, reset
  overrides. The DDR variant of this tab is background + reset only.
  - "Loop root" encodes a game mechanism: the real game's generic decision for a scene-BG
    root that reaches its end is HOLD (mount once, afp's own tick reaches its natural
    terminal branch while nested children free-run - the game default) vs FORCE loop
    (ForceReplay + the continuous-loop flag sequence - needed for one-shot masters like
    bg_common). The renderer mounts via afplist and cannot read the dispatcher's per-BG
    choice at runtime, so this is an explicit user control defaulting to the game default.
  - "Master scale": SDVX I-IV-era 720x1280 IFSes need x1.5 on a 1080x1920 game - the real
    game does this per-BG via the BG entry's payload+28 float; exposed as a slider with a
    1.5x preset since the preview has no BG-id notion.
- **Live**: the live state readout (mono font), file info block, and the MC-names column
  list (shown when MC names are on in "column" mode).

### 3.4 Timeline dock (gui_timeline)

- Transport: -100 / -1 / play-pause / +1 / +100 icon buttons; step buttons WRAP around
  mc_total (matching afp's own wrap-to-0). Mono frame readout `cur / total`, wrap-count
  "loop N", and the LABEL COMBO: a dropdown listing every label with its frame, preview =
  the active label. Labels switched via dropdown because label NAMES drawn on the track
  overlapped unreadably whenever labels sat frames apart (e.g. wait/out).
- Track: one custom ImDrawList widget over an InvisibleButton. Progress fill + 2px playhead
  from the BOUNDED mc playhead; frame labels drawn as amber TICKS ONLY at `frame/total`
  (hover a tick for its name, click it to post `GotoLabel`; clicking/dragging anywhere else
  seeks). Seeking pauses, mirroring the debug viewer's TIME controls (slider-style seeks
  CLAMP; only the step buttons wrap).
- During capture the track additionally tints `frames_captured/total` green, giving the
  export a determinate visual; the whole dock is disabled while capturing (a seek would
  corrupt the export's loop-wrap counter, a pause would freeze the capture - the render
  thread also hard-ignores such requests; greying just makes it visible).
- The dock renders in the Renderer view whenever the scene preset TIMELINE EDITOR is not
  active (3.5), which replaces it in the same full-width slot; without a loaded scene it
  shows a hint line.

### 3.5 Scene preset timeline editor (src/gui/timeline)

The editor replaces the 76 px dock while a preset DOCUMENT is loaded on the scene3d
backend. `Panels::Timeline::Active()` is the switch: the GUI-side `Editor::State` holds a
document AND the `App::PresetStatus` snapshot published by the render thread names the same
`id`. Everything else (the AFP backends, the scene3d backend with a bare package loaded)
keeps the dock.

#### Layout and docking

The editor is a FULL-WIDTH bottom dock, not a column (user decision, 2026-08-15). It takes
the place of the 76 px dock and grows: `RenderRendererView` (gui_panels.cpp) lays out the
three-column row (Browse, viewport, Inspector) above it and the editor below, spanning the
whole content width, with `Gui::HSplitter` - the horizontal twin of `Gui::VSplitter` -
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
transport row, the ruler and the range scroll bar are FIXED: only the header column and the
lanes scroll vertically, together, inside a clipped region between the ruler and the scroll
bar (`ImGui::PushClipRect` in `DrawTracks`, so a half-scrolled row is cut at the region edge
instead of painting over the scroll bar). The child itself carries `NoScrollWithMouse` so the
wheel can never move the whole editor and leave the transport half off screen.
`Editor::ClampTrackScroll` is applied once per frame against the LANES region height, so the
last track's bottom edge lands exactly on the bottom of that region.

Every track row is `TrackHeight(track)` tall, and the header column and the lane use that
same function, so the two can never drift; `###tl_head_<id>` covers the whole row height for
that reason. A track grows a 16 px SUB-LANE only when it holds a PRIMARY clip AND modifier
clips (`Editor::LaneCount` / `Editor::LaneOf` in `src/editor/timeline_lanes.cpp`, unit
tested): one sub-lane per distinct modifier command type, the primary on top. A track whose
clips are all one family - only draws, only tweens, only emitters - is a single lane and its
clips fill it at the full 22 px, which is why a camera.tween track and an fx track are the
same height as a plain model track.

The header row is laid out from its RIGHT edge, so the controls can never be squeezed out:
the M / S / L toggles are a group flush against the column edge, each a SQUARE of
`ToggleSide` = `max(text line height + 2 * FramePadding.y, widest of M/S/L + 2 * FramePadding.x)`,
so a letter is never clipped whatever the font is; the kind badge sits to their left and is
DROPPED when the remaining name space would fall below 24 px (the colour chip already carries
the kind); the name takes what is left and is ELLIPSIZED to it (`Ellipsized`, the same helper
the clip bars use) rather than being hard cut or pushing anything off the column. `tl_header_toggles` pins the rule at the default 200 px and at the
120 px minimum: each toggle is at least its letter plus twice the frame padding in both axes,
its clipped rect equals its full rect, and it lies inside the header column. The
lanes scroll vertically with Shift+wheel (`Editor::View::track_scroll`, clamped by
`Editor::ClampTrackScroll` so the last track cannot be scrolled off the top).

#### Threading rule (enforced by a gate)

Nothing under `src/gui/timeline/` (and later `gui_preset_library.cpp`) may name
`PresetHost::`, `Scene3dHost::` or `Gc2dHost::`. The editor owns a GUI-side working
document and talks to the render thread ONLY through `App::State::PostCommand`:

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

The playhead the editor draws is the render thread's frame from that snapshot - the GUI
never owns a second clock, and the transport counter reads `frame / length` from it. Scrubbing
posts `Seek` plus `SetPaused{true}`.

Editing while playing is allowed and never blocks: the GUI keeps its own working copy,
`Editor::State::Apply` publishes a fresh immutable snapshot, and the editor posts
`ReplaceDocument` whenever `Editor::State::Revision()` changes. All document mutations are
DEFERRED to the end of the GUI frame: `Ctx::pending` is a VECTOR of `Editor::Edit`, applied in
the order they were queued, so the `Document` the draw code is walking can never be replaced
under it and two edits raised by one frame (a Ctrl+drag duplicate plus the move that carries
it) both land instead of the second overwriting the first. A `shared_ptr` kept for the frame
protects the walk from undo/redo too.

#### Editor state, undo and selection (src/editor)

`src/editor/` is ImGui-free and unit tested by `tests/editor/timeline_editor_tests.cpp`
(built into `game_tests`):

- `preset_editor_state.h/.cpp` - the working `Document` as `shared_ptr<const Document>`,
  selection (clip ids), clipboard, view, dirty flag, the M5 `Request` channel, and SNAPSHOT
  undo: `Apply(edit)` copies the document, pushes the previous one plus the selection it
  applied to, and clears redo. Depth 200. `BeginGesture()/EndGesture()` bracket a drag so
  every intermediate state publishes but only ONE undo entry is recorded. Undo restores the
  selection it was applied with; selection and playhead are otherwise not part of undo.
- `timeline_view.h/.cpp` - zoom (0.05..8 px per frame), scroll clamping, fit, and the ruler
  tick rule: the FINEST spacing in 1, 5, 10, 30, 60, 300, 600, 1800 frames that still keeps
  at least 56 px between labels (the coarsest entry wins when even 1800 frames is tighter
  than that).
- `timeline_edits.h/.cpp` - every document mutation as a pure function: move, move to
  another track, resize, split, trim start/end, make open-ended, mute clip, delete, copy,
  paste, duplicate, duplicate onto a track at a frame, track M/S/L, rename, reorder,
  duplicate/delete track, markers, length.
  `PrimaryPeers` / `Overlaps` implement the overlap rule: one PRIMARY per family per frame
  per TARGET across all its tracks, modifiers overlap freely, and two primaries whose `when`
  gates are mutually exclusive may overlap.
- `timeline_drag.h/.cpp` - hit testing (`ZoneAt`: 6 px resize handles, never more than 30
  percent of a short bar) and the drag state machine as a pure function. `SnapFrame` walks
  the snap targets in priority order: playhead, other clips' starts and ends on any track,
  tween keys, document start and end, ruler major ticks; within 8 px, the nearest candidate
  of the first non-empty group wins. `ResolveDrag` keeps the duration on a move, REFUSES a
  move that would overlap a same-family primary, and CLAMPS a resize at the neighbour
  instead of overlapping it. Alt drops `DragInput::snap`, so the raw frame survives. Ctrl+drag
  duplicates: the copy is created by `DuplicateClipTo` at the DROP position under the same
  rules as a move (snap, compatible track, refusal), the original stays where it was, and the
  drag then carries the copy. `OverlapsSelfCopy` is what refuses a copy dropped on top of its
  own source, which plain `Overlaps` cannot see because it excludes the dragged clip itself.
- `clip_summary.h/.cpp` - the text a clip bar carries: its `label` when set, otherwise the
  command's own summary (`model.draw` = `<model>, <blend>, spin <axis> N deg/f`;
  `sprite.animate` = `<animation>, <playback>, prio N`; every other family its key params;
  tweens their key count). Pure, one unit test per family, so the bar text cannot drift from
  what the tests pin. The GUI only shortens it: `Ellipsized` trims to the bar width and
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
| add command | `###tl_add_command` | M5 placeholder: posts `RequestKind::AddCommand` |
| add track | `###tl_add_track` | M5 placeholder: posts `RequestKind::AddTrack` |
| previous / next edge | `###tl_prev_edge`, `###tl_next_edge` | `[` / `]` |
| zoom slider | `###tl_zoom` | 0.05..8 px per frame, logarithmic |
| fit | `###tl_fit` | Ctrl+0 |
| document badge | `###tl_doc_badge` | shows `*` when dirty; posts `RequestKind::DocumentProperties` |
| ruler | `###tl_ruler` | click or drag seeks; double-click adds a marker; right-click on a marker triangle opens the marker popup |
| marker name field | `###tl_marker_name` | inside the marker popup |
| document end line | `###tl_end_line` | drag it to change `length`; the region beyond is dimmed and labelled `end N` |
| header column edge | `###tl_header_split` | drag to resize the header column, clamped to 120..320 px |
| track header | `###tl_head_<track id>` | double-click renames, drag reorders, right-click opens the track menu |
| track rename field | `###tl_rename_<track id>` | Enter commits |
| track M / S / L | `###tl_mute_<id>`, `###tl_solo_<id>`, `###tl_lock_<id>` | square buttons, filled with the accent when on |
| track lane | `###tl_lane_<track id>` | press starts the rubber band, right-click opens the empty-area menu |
| clip bar | `###tl_clip_<clip id>` | click selects, double-click posts the properties request, drag moves, edge drag resizes, Ctrl+drag drops a duplicate |
| range scroll bar | `###tl_scroll` | drag to pan |

Clip bars are 22 px in the row's main lane, centred in it, and 16 px in a modifier sub-lane
(sub-lane rule above). The bar text is `Editor::ClipSummary`
shortened to the bar width with an ellipsis. The bar is filled with the command
colour (the plan's twelve-colour legend, `gui_tl_colors.cpp`, with separate dark and light
values chosen from `ImGuiCol_WindowBg` luminance - the only colours in the shell not derived
from the profile accent). Muted clips and muted or non-solo tracks draw at 40 percent alpha,
a `when` gate draws a 3 px accent stripe along the top edge, a locked track draws a
desaturated left bar, an open-ended clip shows a `>` at its right edge, and tween keys draw
as diamonds.

Selection: a click selects, Shift extends, Ctrl toggles, and a press-drag on empty lane space
draws a RECTANGLE (`UpdateBand`) that selects exactly the clip bars its rectangle covers, in
both axes - the clip rects are the ones drawn this frame, so a band never picks up a lane it
did not reach. Esc clears.

#### Menus

Clip right-click: Properties (posts the request), Cut, Copy, Paste at playhead, Duplicate,
Split at playhead, Trim start / end to playhead, Make open-ended, Add key at playhead
(registered DISABLED - key insertion is M6), Mute clip, Delete.

Empty lane right-click: Add command here, Paste at this frame, Add track above / below. The
three "add" entries post an M5 `Request` and change nothing yet.

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
| Delete | delete selection | S | split selection at playhead |
| M | mute the selected clips' track | L | lock the selected clips' track |
| Enter | properties of the selected clip (M5) | Esc | cancel a drag, clear the selection |
| Ctrl+wheel | zoom about the cursor | Shift+wheel | scroll the tracks |

## 4. Export modal + status tag (gui_export_panel)

- The export form is a MODAL ("Export", centered, 620px), opened from the top-bar button or
  Ctrl+E, and via the status-strip tag. Zero permanent pixels in the shell.
- SIZING GOTCHA: the window uses `AlwaysAutoResize` for height but its width MUST be pinned
  via `SetNextWindowSizeConstraints`. The form is full of stretch-to-available
  widgets (`SetNextItemWidth(-FLT_MIN)`, `GetContentRegionAvail()`), and auto-resize +
  stretch widgets feed back into each other: the window re-measures "content wants more"
  every frame and balloons to screen width. Pinning the width breaks the loop.
- HEIGHT GOTCHA: the max-height constraint is the viewport work height minus a margin, and
  the position is re-centered EVERY frame (`ImGuiCond_Always`, 0.5 pivot). Auto-resize
  windows never scroll on their own; with an Appearing-only position, expanding Advanced
  grew the modal past the bottom edge and hid the Start/Close footer. Capped height makes
  the scrollbar take over, and per-frame centering keeps growth symmetric. Cost: the modal
  is not user-draggable, which is fine because the crop-pick flow closes it anyway.
- Common path: filename stem + format, fps + quality, output resolution (preset combo +
  WxH + scale buttons), transparent-bg + HW-accel. Everything else - keyframe interval,
  frame limit, loop count, blend seam, crop, bg color - sits behind one "Advanced"
  CollapsingHeader.
- Escape closes the modal, guarded on the modal being the focused window and
  `io.WantTextInput` being false so a field's Escape-to-revert and a combo's
  Escape-to-dismiss still win. `BeginPopupModal` with `p_open == nullptr` gives no Escape
  handling of its own, which is why this is explicit.
- Starting an export closes the modal; progress lives in the status strip (capturing N /
  encoding / done / failed, clickable to reopen) and as the timeline capture tint.
- The scale buttons build their label AND their `##` id into one `snprintf` buffer that must
  stay wide enough for both ("x0.25 (480x270)##exp_scl_x0.25##exp_scl" is 39 bytes). A short
  buffer truncates the id, not the visible text, so a collision would appear as two buttons
  sharing one id with nothing on screen to explain it. Found by `gui_tests`.
- Crop pick handshake: arming "Pick region" CLOSES the modal (the drag happens on the render
  window), and the modal auto-reopens when pick mode ends (`g_reopen_after_pick`).
- Form state is file-scope statics so choices survive modal close and IFS reloads. The whole
  form is disabled during capture so a mid-capture click cannot mutate values already
  snapshotted into the export session; the crop rect is snapshotted at start for the same
  reason. Numeric crop edits write state only on change to avoid racing the drag handler.
- Filename stem auto-regenerates on IFS/animation change only. Output resolution (0,0) =
  "native at submit time"; a two-static-state machine (shown_idx + last dims) keeps a user
  "Custom" pick from being auto-reverted by the auto-match next frame. Same pattern in
  Setup's render-resolution widget.
- Encoder facts baked into the tooltips: AVIF = AV1 dual-stream with an auxiliary alpha
  plane (img-tag transparency; color stream NVENC on RTX 40+, alpha stream stays software);
  WebM VP9 = yuva420p single stream (video-tag transparency; software only - NO NVIDIA
  desktop GPU ships a VP9 encoder); WebM AV1 = NVENC-capable AND video-tag smooth but OPAQUE
  only; WebP anim = alpha + smooth img-tag playback everywhere, ~2-3x larger; PNG sequence =
  lossless frame_NNNNNN.png folder via WIC. video-tag playback beats animated img for long
  clips because Blink clamps animated-image frame durations to 10 ms.
- Keyframe interval: shown only when `MediaSink::UsesKeyframeInterval(format)`; 0 = auto
  (one keyframe/sec); a larger value or one >= the frame count forces a single keyframe.
  Feeds `Request::export_keyframe_interval` -> encoder `gop_size` (docs/media_formats.md).
- H.264 export REQUIRES h264_nvenc on this ffmpeg build (no libx264/openh264 compiled in).
  av1_nvenc needs Ada (RTX 40) or newer; the HW probe is format-specific.
- "Blend loop seam" is the ONLY sanctioned pixel-compare feature: a user-opted-in synthesized
  crossfade for backgrounds with no clean authored loop. The real game does NOT do this.

## 5. Debug-viewer parity controls (engine-call mappings)

The live controls mirror KONAMI's CAfpViewerScene key bindings. Each GUI control drives the
exact engine call the scene's key does:

| Control (scene key) | Engine call |
| --- | --- |
| timeline track + step buttons (LEFT/RIGHT; SHIFT = 100-step) | absolute-frame seek via afp_mc_control 0xF08 (deep_goto_play) on the master mc; seeking force-pauses, exactly like the scene |
| play/pause button + Space (RETURN+SHIFT) | stream playback speed 0 / 1 via afp_stream_set_speed, re-applied each frame by the render thread |
| Background Segmented (F4) | preview RT clear color: default transparent -> grey -> black -> red -> green -> blue (kBgPresets); live preview only, export has its own bg |
| Filter toggle (F7) | afp-core set-filter ord 0x032 on the active stream, filter id 0x80000000 OR enable - the same call the scene's CLayer slot-32 wrapper makes |
| Show MC names (F3 DISP MC) | afp_mc_enumerate_children ord 0x079 on the master; name-type (F6): "at clip pos" = draw each name over the preview, "column" = the Live tab list; also feeds Scene-tree positions |
| timeline frame readout | the BOUNDED mc playhead (work+0x76 via afp_mc_set 0x1010), shown exactly like the IIDX debug viewer |
| "loop N" readout | OUR detected backward-wrap tally since the clip/label last (re)started - NOT afp's work+0x104; the debug scene shows no loop_count |
| Live tab size readout | afp_get_layer_info word[8]/word[9] |
| timeline label markers | current label name + the label list (goto via label playback) |
| Live tab raw cur/total | free-running stream counter afp_get_layer_info w[13]/w[12] - climbs past total under the continuous-loop dance (diagnostic) |
| Live tab file info | afp-utils package metadata; version triplets unpack as major=(v>>16)&0xFFFF, minor=(v>>8)&0xFF, patch=v&0xFF (matches the scene's printf). Locale is intentionally omitted - it is the real game's runtime region via a bm2dx CRT call, not derivable from afp/avs |

Other live-control semantics:

- Continuous loop OFF/default/ON: OFF = clear the CLayer flag (master reverts to gotoAndStop
  saturation), default = leave engine default, ON = apply the BG-dispatcher flag sequence
  (master keeps advancing past total_length while sub-clips evolve naturally; what BG 20
  needs). Re-applied on the NEXT stream switch.
- Trim frames: >0 restarts the master via ForceReplay at rendered frame N since the last
  switch (watch an exact-length loop at full framerate).
- DDR gating: DDR's afp 2.13.7 exports none of the modern-only knobs (continuous-loop flag
  dance, trim ForceReplay, afp_set_filter, bulk child-enumerate), so those widgets are absent
  in the DDR panel sets; Background, Reset and the Live readout stay (self-gated on have_*).

## 6. Loading overlay and progress rules

- Full-screen dimmed overlay with `ImGuiWindowFlags_NoInputs` - hit-testing suppressed, so
  drawing it AFTER the regular view is enough to make everything non-interactive.
- Determinate bar when a fraction is known (e.g. textures_expected pre-counted from
  texturelist.xml); otherwise an indeterminate marquee PLUS a changing detail line (current
  item or climbing count) so it never looks frozen. Edge-triggered logging records overlay
  ON/OFF transitions in renderer.log without per-frame spam.
- The same two-pass pattern is used by the arc/customize extractor panels: pass 1 (scan,
  total unknown) shows an animated bar + "Scanning... N found" + the current path; pass 2
  shows done/total. This implements the project-wide progress rule (see CLAUDE.md).

## 7. Setup view

- Centered bordered card (640px, `ImGuiChildFlags_AutoResizeY`, vertically offset ~35% of
  the free space) instead of a top-left form; brand header inside the card.
- Game profile combo: Konami ships per-game builds of afp-core.dll sharing the export-name
  scheme but DIVERGING in ordinal-to-function mapping; the profile selects ordinals +
  boot-call gating. Auto-detect matches known substrings in the game dir path; the Auto
  label previews what it would pick.
- Render fps: controls the live loop tick rate AND per-tick dt (dt = 1/fps) so animation
  speed is unchanged at any rate. DDR content is authored at 60; IIDX/SDVX previews
  historically ran at 120.
- Render resolution presets are NAMED FOR THEIR GAMES (3840x2160 GITADORA 4K, 1920x1080
  IIDX, 1280x720 DDR, 640x480 legacy 4:3, 1080x1920 SDVX / jubeat portrait, 720x1280 SDVX
  old-era, 520x704 qpro avatar - iidx33 only) because AFP's GetScreenSize callback must
  match the offscreen RT shape or layout coords land wrong. Values clamp to 64..8192.
  640x480 carries no game name because it is the generic pre-HD arcade size rather than any
  one title's native mode; it is listed inside the landscape group (after 1280x720) because
  the qpro entry is located POSITIONALLY as `kCustomIdx - 1`, so any new preset must be
  inserted before it. The export panel's output-resolution list carries the same 640x480
  entry, in the same position relative to its trailing Custom row.
- Settings persist to settings.ini on every change (atomic tempfile + rename).
- The DDR-only Tools (batch .arc extractor, customize image extractor - docs/ddr.md) render
  inside the card, gated on the EFFECTIVE setup selection (explicit combo slug, else
  auto-detect of the typed dir).

## 8. qpro view

Reached via the top-bar view switch (iidx33 only; registry MainTab entry).

- Drives the qpro extractor (docs/qpro.md): category checkboxes compose with the per-part
  date-grouped scan selection (a part renders iff category on AND part checked). A scan that
  selects nothing blocks extract; no scan = all parts.
- Body parts need the 520x704 render size (Setup preset) - the tab shows a warning otherwise.
- The skip/failure Issue list renders prominently with a copy-to-clipboard button - a skip
  must never pass silently.
- Uses ImGuiListClipper for the per-part checkbox lists (thousands of rows).

## 9. Removed: locale overlay (companions UI)

The locale companion picker (`<base>_{j,a,k}.ifs` exclusive overlay selection) was removed as
a feature: GUI card, `AfpCmd::ToggleCompanion`, `IGameRuntime::ToggleCompanion`,
`App::CompanionIfs`, `IfsInspect::FindCompanions` are all gone. The ENGINE-level companion
package machinery (`AfpManager::LoadCompanion` / `UnloadCompanion` / mount aliasing) remains -
qpro loads its co-present part packages through it (docs/qpro.md), and the bm2dx locale
name-shadowing facts stay documented in docs/boot_and_render_loop.md "Companions".

## 3D scene viewer (IIDX 17 / 18)

Directories that contain a `.inz` manifest plus at least one `.xz` model are
listed in Browse with a `[3D scene]` suffix. Selecting one loads it through the
normal content path: the backend detects a scene directory in `LoadContent`,
hands it to `Scene3dHost`, and `RenderScene` draws the scene instead of the AFP
content until a normal package is loaded again.

A **3D scene** tab appears in the inspector while a scene is live. It shows the
model / tile / draw-call counts and carries:

- **Pause** and a speed slider, plus a scrubber over the animation in `.x`
  AnimationKey ticks (the scene loops at its longest key time).
- **Animate models** and **Animate camera** freeze each independently, holding
  whatever pose they were in rather than snapping to tick 0. This is the way to
  stop a scene orbiting or a layer drifting while still letting the rest run;
  Pause stops the clock for everything at once. "Animate camera" is disabled
  when the scene has no camera in its `.x`, or while free camera is driving the
  view, with a tooltip saying which.
- A **Models** list with a per-model visibility checkbox and a blend-mode combo
  (opaque / alpha / additive / subtract). The blend value shown is the one the
  game's per-screen setup code assigns; the combo overrides it so a layer can be
  isolated or inspected.
- **Free camera** toggle. OFF uses the camera animated inside the `.x` file;
  scenes without one (`resort_st`, `boss_st`) start with free look ON because it
  is the only way to see them. **Reset view** re-frames from the scene bounds.
- A **move speed** drag, seeded from the scene's bounding radius so a 10-unit
  scene and a 10000-unit scene both feel the same.

Camera controls, handled in `src/scene3d/scene3d_input.cpp`:

| input | action |
|---|---|
| hold RIGHT MOUSE | look around (cursor is hidden and re-centred each frame) |
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
**2D package** tab appears in the inspector while one is live, showing the cell /
record / animation / tile counts and the quad count for the current frame.

- An **animation combo** listing every animation the package names. This is not
  cosmetic: a package's first animation is often not its content (see
  `docs/game_profiles.md`), so without the combo some packages look broken.
- **Pause** and a speed slider, plus a frame scrubber over the current
  animation's length. Dragging the scrubber pauses first, so the playhead stays
  where it was put.

`--animation <name>` picks the starting animation from the command line, which
is what the headless screenshot path uses.
