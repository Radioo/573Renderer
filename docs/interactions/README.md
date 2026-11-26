# Interaction inventory

573Renderer exposes **546** user interactions across **14** surfaces. The count covers the
render window and the 3D scene camera as well as the GUI panels.

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
| Interpolation judge | 20 | [judge_view.md](judge_view.md) |

## By input kind

| input | count |
|---|---|
| cli-arg | 118 |
| left-click | 115 |
| hover | 74 |
| key | 70 |
| drag | 40 |
| text-entry | 29 |
| window-message | 26 |
| checkbox | 27 |
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
| matched to a test by path or flag | 269 |
| not matched | 228 |
| GUI test cases | 192 (761 assertions) |
| CLI test cases | 29 (162 assertions) |

Tests run at two levels. Per-interaction tests drive one control and assert one effect.
User-scenario tests (`tests/gui/scenario_tests.cpp`) then walk a whole journey the way a
person would:

| scenario | what it walks |
|---|---|
| first run | Browse for a game dir, pick a profile, pick a resolution, set fps, Load, assert the BootGame payload |
| browse and play | filter the tree, select an IFS, assert LoadContent, then double-click a layer and assert SwitchAnimation |
| scrub | pause, step, jump to a label, resume, asserting all four commands in order |
| export | Ctrl+E, name it, pick format, fps, quality, size, open Advanced, limit frames, set loop count, Start, assert the whole request |
| cancel | click the capturing tag in the status strip, land in the modal, Cancel |
| crop | arm Pick region in the modal, watch it close, draw the rect on the real render window with Win32 messages, watch the modal reopen, Start, assert the crop reached the request |
| qpro | scan, All, uncheck one date group, pick a category, set fps, extract, assert the per-part selection |
| live overrides | stack four overrides, then reset them all in one click |
| 3D scene | tab absent, load a synthetic scene, tab appears and drives, unload, tab disappears |

The crop scenario is the only test that spans both windows. It checks that the modal, the
render-window WndProc and `App::State` agree about a rect the user drew.

These areas are covered completely, and the tests check it:

- Each of the 74 CLI flags the parser accepts is referenced by a test.
- Each tooltip a test can reach is asserted to appear, including on disabled controls. Two
  unreachable-tooltip bugs were found there.
- Keyboard navigation: focus movement and nav activation. The harness sets
  `ImGuiConfigFlags_NavEnableKeyboard` to match `Gui::Init`.
- Scrolling in the browse tree, the scene tree and the export modal. The export modal test
  also asserts that the height cap keeps the footer reachable on a short window.
- The render window: crop-pick drag, zero-area discard, Escape cancel, crosshair cursor and
  capture loss, driven by real Win32 messages against a real `AppWindow`.
- The 3D camera: right-button look begin and end, the enable gate, capture loss, movement
  polling only while looking, camera integration and scene framing.

[coverage_gaps.md](coverage_gaps.md) lists what is still unmatched. Read its preamble
before treating the count as a to-do list.

## How this was built

Two independent passes read the same sources.

1. **Discovery.** One reader per surface listed every ImGui widget call, every
   `IsItemHovered`/`SetTooltip` pair, every mouse gesture beyond a plain click, every key
   handler, every scrollable child, every enable or visibility gate, and every Win32
   message a user action produces. It found 440 interactions.
2. **Audit.** A second reader per surface got the first pass's output and was told to
   assume it was incomplete. It found **86** missed interactions and **81** factual errors,
   and all 13 surfaces came back incomplete.

Entries marked *(audit)* come from the second pass. Corrections sit inline on the entry
they correct, so the record still shows what the first reading got wrong.

## Behaviour the audit turned up

The second pass corrected 81 entries. Four of them described real defects, which are now
fixed, each with a regression test. The rest describe behaviour worth knowing.

1. **Fixed: numeric fields committed on every keystroke instead of on edit completion.**
   None of the 13 `InputInt` call sites passes `ImGuiInputTextFlags_EnterReturnsTrue`, so
   each one returned true on every keystroke. That is harmless where the value lands in a
   panel-local static (the whole export modal), but three sites acted on it at once: the
   setup frame rate and the render width and height were applied to `App::State` and saved
   with `SaveCurrentSettings()`, and the inspector trim field pushed a live override.
   Typing `144` applied and saved 1, then 14, then 144, which is three disk writes. Since
   `render_loop.cpp:278` reads the fps every frame, the preview really ran at 1 fps while
   the user was typing. The trim field was worse: `game_runtime_modern.cpp:348` replays
   the animation when `frames_since_switch >= trim_frames`, so typing a trim value
   restarted playback on the intermediate `2`. Those three fields now stage the value and
   commit on `IsItemDeactivatedAfterEdit()`, which fires on Enter or when focus leaves the
   field. The stepped fps field is wrapped in `BeginGroup`/`EndGroup` so the status flag
   survives the -/+ buttons ImGui adds, and the quick-fps buttons still apply immediately.
   (The raw audit also claimed that render width and height cause a live resize. They do
   not: `GetRenderSize` is read only at boot and by the export.)
2. **Fixed: a disabled control's tooltip could never be seen.** With default flags,
   `IsItemHovered()` returns false for an item inside `BeginDisabled`. Five controls
   attach a tooltip to a control that can be disabled, and in two of them the tooltip is
   there to explain why the control is greyed out: the 3D panel's **Animate camera** ("This
   scene has no camera in its .x file" / "Free camera is on...") and the export modal's
   **HW accel** ("h264_nvenc unavailable on this machine" / "Hardware acceleration
   unavailable..."). Neither could be shown, so the user saw a greyed control with no
   reason. All five now pass `ImGuiHoveredFlags_AllowWhenDisabled`.
3. **Fixed: Escape did not close the export modal.** `BeginPopupModal` forces
   `ImGuiWindowFlags_Modal` and is passed `p_open == nullptr`, and ImGui honours the Escape
   cancel request only for non-modal popups, so the Close button was the only way out.
   The modal now closes on Escape when it is the focused window and no text field is
   capturing input. A field's own Escape-to-revert and an open combo's Escape-to-dismiss
   still work. `src/gui/gui_export_panel.cpp:628`.
4. **A combo's tooltip never fires while its own popup is open**, because the open popup
   blocks hover. The profile, render-preset, format and resolution combos all attach their
   explanation this way, so it shows only before the list is opened.
5. **Expanding a scene-tree node does more than change the view.** `SetSublayerExpanded` is
   the only thing that makes the render thread enumerate that node's children, and the
   tree has no other way to be populated. `src/gui/gui_scene_panel.cpp:122`.
6. **Fixed: the timeline's label ticks interrupted drag-scrubbing.** `LabelHitTest` ran
   whenever the track was hovered and returned early on a hit, so dragging the playhead
   across a label tick stalled the scrub there. The hit-test is now skipped while
   `IsMouseDragging` is true. A click on a tick still jumps to it (a click has not crossed
   the drag threshold), and a scrub passes straight over.
   `src/gui/gui_timeline.cpp:170`.
7. **`ImGuiWindowFlags_NoScrollbar` does not disable wheel scrolling.** Only
   `NoScrollWithMouse` does, so several regions that look unscrollable do scroll.
8. **Fixed: the HW-accel tooltip described AV1 while MP4 HEVC-alpha was selected.**
   `DrawHwAccelTooltip` had no branch for `Format::MP4_HEVC_Alpha`, and
   `MediaSink::HardwareProbeFormat` maps that format onto `AVIF`, so the probe answered a
   question about AV1 NVENC that does not apply. The tooltip fell through to either "Needs
   an NVIDIA GPU with AV1 encode support" or "Encode the AV1 stream with NVENC
   (av1_nvenc)". HEVC-alpha is encoded by `libx265` in `VideoEncoder::OpenMp4HevcAlpha` and
   has no hardware path on any GPU, because NVENC's HEVC encoder carries no alpha, and
   alpha is what this format exists for. The new branch says so, and it runs before the
   machine probe because the reason comes from the format alone. The bug was found by
   extending the branch-text tooltip test from six formats to all seven.
   `src/gui/gui_export_panel.cpp:445`.

## Regenerating

These files are generated. The inventory data and the generator live in the session
scratchpad, and this directory is the durable copy. If the UI changes substantially,
re-run the two workflows in docs/gui_tests.md.
