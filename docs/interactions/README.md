# Interaction inventory

Every user interaction 573Renderer exposes: **526** across **13** surfaces,
including the render window and the 3D scene camera, not just the GUI panels.

## Surfaces

| surface | interactions | doc |
|---|---|---|
| Command line | 103 | [cli.md](cli.md) |
| Export modal | 73 | [export_modal.md](export_modal.md) |
| 3D scene and 2D package panels | 45 | [host_panels.md](host_panels.md) |
| Shared widgets and gating | 37 | [shared_widgets.md](shared_widgets.md) |
| Timeline dock | 36 | [timeline.md](timeline.md) |
| GUI window and thread | 36 | [gui_window.md](gui_window.md) |
| Inspector | 32 | [inspector.md](inspector.md) |
| Ready-view shell | 31 | [shell.md](shell.md) |
| qpro panel | 29 | [qpro_panel.md](qpro_panel.md) |
| Setup view | 29 | [setup_view.md](setup_view.md) |
| 3D scene camera | 29 | [scene3d_camera.md](scene3d_camera.md) |
| Scene pane | 24 | [scene_pane.md](scene_pane.md) |
| Render window | 22 | [render_window.md](render_window.md) |

## By input kind

| input | count |
|---|---|
| cli-arg | 111 |
| left-click | 108 |
| hover | 74 |
| key | 61 |
| drag | 40 |
| text-entry | 28 |
| window-message | 25 |
| checkbox | 25 |
| scroll | 25 |
| combo-select | 12 |
| right-click | 9 |
| state-change | 4 |
| double-click | 3 |
| slider | 1 |

122 of these are hover tooltips.

## Test coverage

| | |
|---|---|
| interactions inventoried | 526 |
| matched to a test by path or flag | 261 |
| not matched | 236 |
| GUI test cases | 162 (596 assertions) |
| CLI test cases | 29 (162 assertions) |

What is deliberately, verifiably complete:

- **Every one of the 74 CLI flags** the parser accepts is referenced by a test in
  `tests/cli/cli_tests.cpp`.
- **Every tooltip that a test can reach** is asserted to actually appear
  (`tests/gui/tooltip_tests.cpp`), including on DISABLED controls, which is where two
  unreachable-tooltip bugs were found.
- **Keyboard navigation** is exercised (`tests/gui/input_mode_tests.cpp`): focus movement,
  nav activation of a button, and nav activation of a transport control. The harness now
  sets `ImGuiConfigFlags_NavEnableKeyboard` to match `Gui::Init`; before that the tests ran
  a subtly different application from the shipped one.
- **Scrolling** is exercised for the browse tree, the scene tree and the export modal, the
  last of which also asserts the documented height cap keeps the Start/Close footer
  reachable on a short window.

See [coverage_gaps.md](coverage_gaps.md) for what is still unmatched, and read its preamble
before treating the count as a to-do list - the matcher cannot see interactions driven by
coordinates or key chords, and it cannot distinguish a click from a hover.

## How this was built

Two independent passes over the same sources.

1. **Discovery** - one reader per surface, told to enumerate every ImGui widget call,
   every `IsItemHovered`/`SetTooltip` pair, every mouse gesture beyond a plain click,
   every key handler, every scrollable child, every enable/visibility gate, and every
   Win32 message a user action produces. Result: 440 interactions.
2. **Audit** - a second reader per surface, given the first pass's output and told to
   assume it was incomplete. It found **86** missed interactions and
   **81** factual errors. Every one of the 13 surfaces came back INCOMPLETE,
   which is the honest measure of how easy it is to under-count this.

Entries marked *(audit)* were added by the second pass. Corrections are recorded inline
on the entry they correct, so the record shows what the first reading got wrong.

## Behaviour the audit turned up

The second pass corrected 81 entries. Four described genuine defects and have been FIXED;
the rest are behaviour worth knowing. Each fix carries a regression test.

1. **FIXED - numeric fields committed on every keystroke, not on edit completion.**
   `ImGuiInputTextFlags_EnterReturnsTrue` appears at none of the 13 `InputInt` call sites,
   so each returned true per keystroke. Harmless where the value lands in a panel-local
   static (the whole export modal), but three sites acted on it immediately: the setup
   frame rate and render width/height applied to `App::State` AND called
   `SaveCurrentSettings()`, and the inspector trim field pushed a live override. Typing
   `144` applied and persisted 1, then 14, then 144 - three disk writes; and because
   `render_loop.cpp:278` reads the fps live every frame, the preview really did run at
   1 fps mid-typing. Worse, `game_runtime_modern.cpp:348` force-replays the animation when
   `frames_since_switch >= trim_frames`, so typing a trim value restarted playback on the
   intermediate `2`. Those three now stage the value and commit on
   `IsItemDeactivatedAfterEdit()` - Enter, or clicking away. The stepped fps field is
   wrapped in `BeginGroup`/`EndGroup` so the status flag survives the -/+ buttons ImGui
   appends, and the quick-fps buttons still apply immediately.
   (One claim in the raw audit was wrong and is not repeated here: render width/height do
   NOT cause a live resize, because `GetRenderSize` is only read at boot and by the export.)
2. **FIXED - a disabled control's tooltip could never be seen.** `IsItemHovered()` with
   default flags returns false for an item inside `BeginDisabled`. Five controls attach a
   tooltip to a control that can be disabled, and in two of them the tooltip exists
   PRECISELY to explain why it is greyed out: the 3D panel's **Animate camera** ("This
   scene has no camera in its .x file" / "Free camera is on...") and the export modal's
   **HW accel** ("h264_nvenc unavailable on this machine" / "Hardware acceleration
   unavailable..."). Both were unreachable - the user got a greyed control and no reason.
   All five now pass `ImGuiHoveredFlags_AllowWhenDisabled`.
3. **FIXED - Escape did not close the export modal.** `BeginPopupModal` forces
   `ImGuiWindowFlags_Modal` and is passed `p_open == nullptr`, and ImGui only honours the
   Escape cancel request for non-modal popups, so the Close button was the only way out.
   The modal now closes on Escape when it is the focused window and no text field is
   capturing input, which leaves the field's own Escape-to-revert and an open combo's
   Escape-to-dismiss working. `src/gui/gui_export_panel.cpp:628`.
4. **A combo's tooltip never fires while its own popup is open**, because hover is blocked
   by the open popup. The profile, render-preset, format and resolution combos all attach
   their explanation this way, so it is only visible before opening the list.
5. **Expanding a scene-tree node is not cosmetic.** `SetSublayerExpanded` is the ONLY
   trigger for the render thread to enumerate that node's children; the tree cannot be
   populated any other way. `src/gui/gui_scene_panel.cpp:122`.
6. **FIXED - the timeline's label ticks pre-empted drag-scrubbing.** `LabelHitTest` ran
   whenever the track was hovered, and a hit returned early, so dragging the playhead across
   a label tick stalled the scrub instead of seeking through it. The hit-test is now skipped
   while `IsMouseDragging` is true, which keeps click-to-jump on a tick (a click has not
   crossed the drag threshold) but lets a scrub pass straight over.
   `src/gui/gui_timeline.cpp:170`.
7. **`ImGuiWindowFlags_NoScrollbar` does not disable wheel scrolling** - only
   `NoScrollWithMouse` does. Several regions that look unscrollable do scroll.

## Regenerating

These files are generated. The inventory data and the generator live in the session
scratchpad; the durable artefact is this directory. Re-run the two workflows in
docs/gui_tests.md if the UI changes substantially.
