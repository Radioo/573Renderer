# Untested interactions

**228** of **526** interactions have no test that names their
item path or CLI flag. Grouped by surface, ordered by how many are missing.

## Read this before trusting the number

Coverage is matched by imgui item path (or, for the CLI, by flag string). That makes it
a LOWER BOUND on what is tested and an UPPER BOUND on what is missing:

- An interaction driven without naming its path is not credited. The keyboard-shortcut
  tests, the timeline track drag, the splitter drag and the crop-pick flow all drive
  real interactions through mouse coordinates or key chords.
- Several entries document the ABSENCE of behaviour ("dragging a tab does not reorder",
  "clicking the loading overlay passes through"). Those are invariants, not actions.
- Tooltips are NOT part of the residue. Every `ImGui::SetTooltip` call site in `src/gui`
  is asserted by `tests/gui/tooltip_tests.cpp`, including the branch-dependent texts;
  see `docs/gui_tests.md` section 16. A tooltip row appearing below means only that the
  matcher could not tie the hover to that row's path, not that the tooltip is untested.

Use this list to find candidates, then read the surface doc before concluding a gap is
real.

## Shared widgets and gating (35)

Breakdown: left-click 19, drag 5, hover 4, key 4, scroll 2, double-click 1

| id | control | input | source |
|---|---|---|---|
| `vsplitter-double-click-no-reset` | Double-clicking a splitter bar | double-click | `src/gui/gui_splitter.cpp:28` |
| `inspector-tab-reorder-blocked` | Dragging an inspector tab to reorder it | drag | `src/gui/gui_inspector.cpp:444` |
| `overlay-move-resize-collapse-blocked` | Dragging / resizing / collapsing the loading overlay window | drag | `src/gui/gui_loading_overlay.cpp:115` |
| `segmented-wrap-on-pane-resize` | Segmented control row re-flowing when the host pane is narrowed | drag | `src/gui/gui_widgets.cpp:17` |
| `vsplitter-drag` | Vertical splitter bar (InvisibleButton) | drag | `src/gui/gui_splitter.cpp:27` |
| `vsplitter-drag-right-instance` | Right splitter between the centre scene pane and the inspector pane | drag | `src/gui/gui_panels.cpp:416` |
| `section-header-no-interaction` | Gui::SectionHeader (icon + label + suffix + Separator) | hover | `src/gui/gui_widgets.cpp:40` |
| `segmented-item-hover-highlight` | Segmented control segment button hover/press feedback | hover | `src/gui/gui_widgets.cpp:21` |
| `segmented-last-item-tooltip` | Segmented control: hovering the LAST segment surfaces the caller's tooltip | hover | `src/gui/gui_widgets.cpp:28` |
| `vsplitter-hover-cursor` | Splitter hover cursor | hover | `src/gui/gui_splitter.cpp:13` |
| `inspector-tab-keyboard-activate` | Inspector tab item reached via keyboard navigation | key | `src/gui/gui_inspector.cpp:446` |
| `main-tab-button-keyboard-activate` | Main view tab button in the top bar, reached via keyboard navigation | key | `src/gui/gui_panels.cpp:251` |
| `segmented-item-keyboard-activate` | Segmented segment button via keyboard navigation | key | `src/gui/gui_widgets.cpp:28` |
| `vsplitter-keyboard-activate` | Splitter InvisibleButton reached via keyboard nav | key | `src/gui/gui_splitter.cpp:11` |
| `inspector-tab-gc2d` | Inspector tab "2D package" | left-click | `src/gui/panel_registry.cpp:92` |
| `inspector-tab-live-ddr` | Inspector tab "Live" (DDR backend) | left-click | `src/gui/panel_registry.cpp:69` |
| `inspector-tab-live-modern` | Inspector tab "Live" (modern backend) | left-click | `src/gui/panel_registry.cpp:51` |
| `inspector-tab-properties` | Inspector tab "Properties" | left-click | `src/gui/panel_registry.cpp:41` |
| `inspector-tab-render-ddr` | Inspector tab "Render" (DDR backend variant) | left-click | `src/gui/panel_registry.cpp:64` |
| `inspector-tab-render-modern` | Inspector tab "Render" (modern backend variant) | left-click | `src/gui/panel_registry.cpp:46` |
| `inspector-tab-scene3d-ddr` | Inspector tab "3D scene" (DDR backend) | left-click | `src/gui/panel_registry.cpp:74` |
| `inspector-tab-scene3d-standalone` | Inspector tab "3D scene" (scene3d backend) | left-click | `src/gui/panel_registry.cpp:87` |
| `inspector-tab-select` | Inspector tab item (one per InspectorTab panel) | left-click | `src/gui/gui_inspector.cpp:446` |
| `main-tab-button-click` | Main view tab button in the top bar (one per MainTab panel) | left-click | `src/gui/gui_panels.cpp:251` |
| `main-tab-qpro` | Main tab "qpro" | left-click | `src/gui/panel_registry.cpp:36` |
| `main-tab-renderer` | Main tab "Renderer" | left-click | `src/gui/panel_registry.cpp:31` |
| `mc-name-type-segmented-show-gate` | Segmented instance "##mc_name_type" is conditionally hidden | left-click | `src/gui/gui_inspector.cpp:302` |
| `overlay-click-through` | Clicking anywhere on the loading overlay | left-click | `src/gui/gui_loading_overlay.cpp:118` |
| `panel-set-unknown-backend` | Effect of an unrecognised active backend id on every tab on this surface | left-click | `src/gui/panel_registry.cpp:112` |
| `segmented-active-item-click-noop` | Segmented control: the currently selected segment | left-click | `src/gui/gui_widgets.cpp:28` |
| `segmented-current-out-of-range` | Segmented control when *current is outside [0, count) | left-click | `src/gui/gui_widgets.cpp:19` |
| `segmented-item-click` | Segmented control segment button (one per item) | left-click | `src/gui/gui_widgets.cpp:28` |
| `vsplitter-press-no-move` | Splitter click without moving the mouse | left-click | `src/gui/gui_splitter.cpp:16` |
| `main-view-body-child-scroll` | Main view body host child ("main_view") that renders the selected MainTab panel | scroll | `src/gui/gui_panels.cpp:449` |
| `overlay-scroll-blocked` | Scrolling over the loading overlay | scroll | `src/gui/gui_loading_overlay.cpp:118` |

## 3D scene camera (27)

Breakdown: key 13, drag 4, right-click 4, cli-arg 2, left-click 2, hover 1, scroll 1

| id | control | input | source |
|---|---|---|---|
| `load-scene-arms-camera` | Load a 3D scene directory (arms the free-look surface and resets the camera) | cli-arg | `src/scene3d/camera.cpp:97` |
| `unload-scene-disarms-camera` | Unload the current 3D scene (disarms the free-look surface) | cli-arg | `src/scene3d/scene3d_input.cpp:38` |
| `gui-move-speed-drag` | "move speed" DragFloat (drag left/right to change camera translation speed) | drag | `src/gui/gui_scene3d_panel.cpp:127` |
| `lmb-crop-drag-steals-look-capture` | Left-click / drag a crop rectangle in the render window while the right button is held for free-look | drag | `src/window.cpp:100-110` |
| `mouse-look-drag` | Move the mouse while the right button is held (free-look drag) | drag | `src/scene3d/scene3d_input.cpp:65` |
| `pitch-clamp` | Drag the mouse vertically past the vertical look limit | drag | `src/scene3d/camera.cpp:23` |
| `gui-freecam-checkbox-hover` | Hover the "Free camera" checkbox | hover | `src/gui/gui_scene3d_panel.cpp:114` |
| `esc-close-during-look` | Press Escape while the render window has focus (including mid free-look) | key | `src/window.cpp:130` |
| `gui-move-speed-ctrl-click` | Ctrl+click (or double-click) the "move speed" drag to type an exact value | key | `src/gui/gui_scene3d_panel.cpp:127` |
| `key-a-left` | Hold A (strafe left) | key | `src/scene3d/scene3d_input.cpp:89` |
| `key-alt-slow` | Hold Alt (slow / precision movement modifier) | key | `src/scene3d/scene3d_input.cpp:94` |
| `key-ctrl-down` | Hold Ctrl (descend / move down along world Y) | key | `src/scene3d/scene3d_input.cpp:92` |
| `key-d-right` | Hold D (strafe right) | key | `src/scene3d/scene3d_input.cpp:90` |
| `key-e-up` | Hold E (rise / move up along world Y) | key | `src/scene3d/scene3d_input.cpp:91` |
| `key-q-down` | Hold Q (descend / move down along world Y) | key | `src/scene3d/scene3d_input.cpp:92` |
| `key-s-back` | Hold S (move backward) | key | `src/scene3d/scene3d_input.cpp:88` |
| `key-shift-fast` | Hold Shift (fast movement modifier) | key | `src/scene3d/scene3d_input.cpp:93` |
| `key-space-up` | Hold Space (rise / move up along world Y) | key | `src/scene3d/scene3d_input.cpp:91` |
| `key-w-forward` | Hold W (move forward) | key | `src/scene3d/scene3d_input.cpp:87` |
| `multi-key-diagonal` | Hold two or more movement keys at once (e.g. W+D, or W+Space) | key | `src/scene3d/camera.cpp:64` |
| `gui-freecam-checkbox` | "Free camera" checkbox in the 3D scene inspector tab | left-click | `src/gui/gui_scene3d_panel.cpp:113` |
| `gui-reset-view-button` | "Reset view" button in the 3D scene inspector tab | left-click | `src/gui/gui_scene3d_panel.cpp:123` |
| `look-armed-but-inert-freecam-off` | Hold right mouse button in the render window while "Free camera" is OFF | right-click | `src/scene3d/scene3d_host.cpp:93` |
| `rmb-during-crop-pick-mode` | Right-click in the render window while crop pick mode is active | right-click | `src/window.cpp:112` |
| `rmb-press-begin-look` | Right mouse button press anywhere in the render window client area | right-click | `src/scene3d/scene3d_input.cpp:53` |
| `rmb-release-end-look` | Right mouse button release | right-click | `src/scene3d/scene3d_input.cpp:56` |
| `mouse-wheel-unbound` | Mouse wheel over the render window | scroll | `src/gui/gui_scene3d_panel.cpp:134` |

## Timeline dock (27)

Breakdown: key 10, hover 8, left-click 7, scroll 1, text-entry 1

| id | control | input | source |
|---|---|---|---|
| `export-disabled-notice` | "(seek / pause disabled during export)" notice | hover | `src/gui/gui_timeline.cpp:113` |
| `frame-counter-readout` | "cur / total" frame counter and "loop N" wrap counter (mono font) | hover | `src/gui/gui_timeline.cpp:104` |
| `no-scene-placeholder` | "Load an IFS to control playback." placeholder text | hover | `src/gui/gui_timeline.cpp:222` |
| `transport-jump-back-100-tooltip` | Tooltip on the jump-back-100 button | hover | `src/gui/gui_timeline.cpp:45` |
| `transport-jump-fwd-100-tooltip` | Tooltip on the jump-forward-100 button | hover | `src/gui/gui_timeline.cpp:45` |
| `transport-play-pause-tooltip` | Tooltip on the Play/Pause button | hover | `src/gui/gui_timeline.cpp:45` |
| `transport-step-back-1-tooltip` | Tooltip on the step-back-1 button | hover | `src/gui/gui_timeline.cpp:45` |
| `transport-step-fwd-1-tooltip` | Tooltip on the step-forward-1 button | hover | `src/gui/gui_timeline.cpp:45` |
| `label-combo-keyboard-open-navigate-dismiss` | Keyboard operation of the ##tl_labels combo popup (open, move between rows, select, dismiss) | key | `src/gui/gui_timeline.cpp:54, 61, 64` |
| `nav-activate-focused-transport-button` | Space / Enter activation of the nav-focused transport button (and the Space double-fire against the play/pause shortcut) | key | `src/gui/gui_timeline.cpp:44 and src/gui/gui_timeline.cpp:201` |
| `nav-arrow-move-vs-step-shortcut` | Left / Right arrow while an item on this surface holds nav focus (nav move plus frame step in the same frame) | key | `src/gui/gui_timeline.cpp:203-204` |
| `nav-keyboard-focus-transport-controls` | Keyboard navigation focus (Tab / Shift+Tab) over the transport buttons and the label combo | key | `src/gui/gui_timeline.cpp:44 (Button tab-stops) + src/gui/gui_window.cpp:153 (NavEnableKeyboard)` |
| `shortcut-ctrl-e-export` | Ctrl+E keyboard shortcut (open export panel) | key | `src/gui/gui_timeline.cpp:195` |
| `shortcut-left-step-back` | Left arrow keyboard shortcut (step back 1 frame) | key | `src/gui/gui_timeline.cpp:203` |
| `shortcut-right-step-fwd` | Right arrow keyboard shortcut (step forward 1 frame) | key | `src/gui/gui_timeline.cpp:204` |
| `shortcut-shift-left-jump-back` | Shift+Left arrow chord (step back 100 frames) | key | `src/gui/gui_timeline.cpp:202` |
| `shortcut-shift-right-jump-fwd` | Shift+Right arrow chord (step forward 100 frames) | key | `src/gui/gui_timeline.cpp:202` |
| `shortcut-space-play-pause` | Space keyboard shortcut (toggle play/pause) | key | `src/gui/gui_timeline.cpp:201` |
| `label-combo-item-select` | Label row inside the combo: "<name>   (frame N)" | left-click | `src/gui/gui_timeline.cpp:61` |
| `transport-jump-back-100` | Jump back 100 frames button (ICON_JUMP_BACK glyph) | left-click | `src/gui/gui_timeline.cpp:79` |
| `transport-jump-fwd-100` | Jump forward 100 frames button (ICON_JUMP_FWD glyph) | left-click | `src/gui/gui_timeline.cpp:97` |
| `transport-play-pause` | Play / Pause toggle button (glyph swaps between ICON_PLAY and ICON_PAUSE) | left-click | `src/gui/gui_timeline.cpp:86` |
| `transport-step-back-1` | Step back 1 frame button (ICON_STEP_BACK glyph) | left-click | `src/gui/gui_timeline.cpp:84` |
| `transport-step-fwd-1` | Step forward 1 frame button (ICON_STEP_FWD glyph) | left-click | `src/gui/gui_timeline.cpp:93` |
| `transport-step-with-no-master-clock` | Transport step/jump buttons and Left/Right shortcuts while live.mc_total == 0 | left-click | `src/gui/gui_timeline.cpp:36-41` |
| `label-combo-popup-scroll` | Label combo popup list (scrollable when there are many labels) | scroll | `src/gui/gui_timeline.cpp:55` |
| `shortcut-suppression-while-typing` | Text-input focus suppression of all timeline shortcuts | text-entry | `src/gui/gui_timeline.cpp:193` |

## Export modal (24)

Breakdown: left-click 12, drag 4, hover 3, key 3, right-click 1, scroll 1

| id | control | input | source |
|---|---|---|---|
| `bg-color-picker` | Colour picker popup opened from the background swatch | drag | `src/gui/gui_export_panel.cpp:500` |
| `bg-color-swatch-dragdrop` | Background colour swatch as drag-and-drop source / target | drag | `src/gui/gui_export_panel.cpp:500` |
| `modal-scrollbar-drag` | Export modal vertical scrollbar | drag | `src/gui/gui_export_panel.cpp:627` |
| `modal-titlebar-drag` | Export modal title bar | drag | `src/gui/gui_export_panel.cpp:626` |
| `bg-color-swatch-hover-tooltip` | Background colour swatch built-in hover tooltip | hover | `src/gui/gui_export_panel.cpp:500` |
| `max-frames-tooltip` | Hover tooltip on the duration-preview text next to the frames input | hover | `src/gui/gui_export_panel.cpp:183` |
| `scale-button-tooltip` | Hover tooltip on a scale-multiplier button | hover | `src/gui/gui_export_panel.cpp:356` |
| `escape-clears-active-item` | Escape key while a widget in the modal is active | key | `src/gui/gui_export_panel.cpp:628` |
| `modal-escape-close` | Escape key while the Export modal has focus | key | `src/gui/gui_export_panel.cpp:628` |
| `modal-keyboard-nav` | Tab / Shift+Tab / arrow-key navigation between the modal's widgets | key | `src/gui/gui_export_panel.cpp:628` |
| `bg-color-swatch` | Background colour swatch (ColorEdit3, NoInputs + NoLabel) | left-click | `src/gui/gui_export_panel.cpp:500` |
| `blend-frames-step-buttons` | Blend frames -/+ step buttons | left-click | `src/gui/gui_export_panel.cpp:212` |
| `format-combo-item` | Format list entry inside the open combo | left-click | `src/gui/gui_export_panel.cpp:82` |
| `fps-step-buttons` | fps InputInt -/+ step buttons | left-click | `src/gui/gui_export_panel.cpp:113` |
| `keyframe-step-buttons` | keyframe interval -/+ step buttons | left-click | `src/gui/gui_export_panel.cpp:139` |
| `loop-count-step-buttons` | Continuous loop count -/+ step buttons | left-click | `src/gui/gui_export_panel.cpp:192` |
| `max-frames-step-buttons` | frames InputInt -/+ step buttons | left-click | `src/gui/gui_export_panel.cpp:175` |
| `modal-click-outside` | Click anywhere outside the Export modal (dimmed background, the main render window, any other panel) | left-click | `src/gui/gui_export_panel.cpp:628` |
| `res-preset-custom` | "Custom" entry in the resolution combo | left-click | `src/gui/gui_export_panel.cpp:277` |
| `res-preset-fixed` | Fixed resolution entry in the resolution combo | left-click | `src/gui/gui_export_panel.cpp:277` |
| `res-preset-native` | "Native (WxH)" entry in the resolution combo | left-click | `src/gui/gui_export_panel.cpp:277` |
| `scale-button` | Scale-current multiplier button ("x0.5 (WxH)" / "x0.25 (WxH)" / "x0.1 (WxH)") | left-click | `src/gui/gui_export_panel.cpp:352` |
| `bg-color-swatch-context-menu` | Background colour swatch right-click options popup | right-click | `src/gui/gui_export_panel.cpp:500` |
| `modal-scroll` | Export modal body (scrollable popup content) | scroll | `src/gui/gui_export_panel.cpp:627` |

## GUI window and thread (20)

Breakdown: drag 9, cli-arg 3, left-click 3, key 2, hover 1, right-click 1, scroll 1

| id | control | input | source |
|---|---|---|---|
| `device-creation-fallback` | Launching the app on a machine without a usable D3D9 HAL device | cli-arg | `src/gui/gui_window.cpp:94` |
| `gui-init-failure-no-window` | Launching the app when window creation or device creation fails | cli-arg | `src/gui/gui_thread.cpp:23` |
| `gui-thread-start-idempotent` | Starting the GUI a second time | cli-arg | `src/gui/gui_thread.cpp:51` |
| `dpi-unaware-scaling` | Running the control window on a high-DPI display, or dragging it to a monitor with a different scale factor | drag | `src/gui/gui_window.cpp:44` |
| `imgui-layout-not-persisted` | Dragging / resizing ImGui sub-windows and headers inside the control window | drag | `src/gui/gui_window.cpp:154` |
| `move-window-titlebar` | Title bar drag to move the window | drag | `src/gui/gui_window.cpp:76` |
| `render-reentrancy-guard` | Resizing or repainting the window while a frame is already being drawn | drag | `src/gui/gui_window.cpp:221` |
| `resize-drag-border` | Window edge / corner resize grip | drag | `src/gui/gui_window.cpp:47` |
| `resize-min-size-clamp` | Attempt to shrink the window below the minimum size | drag | `src/gui/gui_window.cpp:59` |
| `resize-while-device-lost` | Resizing the window while the D3D9 device is lost | drag | `src/gui/gui_window.cpp:227` |
| `titlebar-drag-blocks-gui-thread-exit` | Holding the title bar or a resize border (Win32 modal move/size loop) | drag | `src/gui/gui_thread.cpp:34` |
| `warp-resize-changes-present-params` | Resizing the control window while running on the WARP fallback device | drag | `src/gui/gui_window.cpp:208` |
| `mouse-cursor-shape` | Hovering the control window | hover | `src/gui/gui_window.cpp:130` |
| `alt-f10-system-menu-suppressed` | Alt key or F10 tap (keyboard system-menu activation) | key | `src/gui/gui_window.cpp:67` |
| `keyboard-nav-enabled` | Tab / arrow keys / Space / Enter keyboard navigation between widgets | key | `src/gui/gui_window.cpp:153` |
| `control-window-not-reopenable` | Trying to get the control window back after closing it | left-click | `src/gui/gui_window.cpp:70` |
| `modal-folder-dialog-parenting` | Opening a native "browse for folder" dialog from any panel | left-click | `src/gui/gui_window.cpp:28` |
| `per-frame-panel-build` | Any widget activation inside the control window | left-click | `src/gui/gui_window.cpp:250` |
| `system-menu-titlebar-rightclick` | Right-click the title bar / click the window icon to open the system menu (Restore, Move, Size, Minimize, Maximize, Close) | right-click | `src/gui/gui_window.cpp:76` |
| `mouse-wheel-scroll` | Mouse wheel over any scrollable ImGui child region in the control window | scroll | `src/gui/gui_window.cpp:45` |

## Render window (17)

Breakdown: left-click 6, drag 4, key 3, right-click 2, double-click 1, hover 1

| id | control | input | source |
|---|---|---|---|
| `client-double-click-no-dblclks` | Double-click inside the render window client area | double-click | `src/window.cpp:151` |
| `crop-pick-mousemove-drag` | Render window client area (crop pick mode) - drag to size the crop rectangle | drag | `src/window.cpp:93` |
| `nonclient-drag-during-crop-pick` | Dragging the title bar / resizing from the frame while crop pick mode is armed | drag | `src/window.cpp:78` |
| `scene3d-look-mouse-keyboard` | 3D camera look / fly controls in the render window (mouse move, buttons and keys consumed by the scene3d input layer) | drag | `src/window.cpp:123` |
| `window-resize-drag` | Resizing the window by dragging its border/corner (WS_OVERLAPPEDWINDOW sizing frame) | drag | `src/window.cpp:151` |
| `crop-pick-setcursor-crosshair` | Moving the pointer over the client area during crop pick mode (cursor feedback) | hover | `src/window.cpp:77` |
| `crop-pick-escape-cancel` | Escape key while crop pick mode is active | key | `src/window.cpp:69` |
| `global-escape-quit` | Escape key in the render window (normal mode) | key | `src/window.cpp:130` |
| `syskey-system-menu-altf4` | Alt / F10 / Alt+Space / Alt+F4 pressed while the render window has focus (system keys) | key | `src/window.cpp:141` |
| `appwindow-requestclose-api` | Programmatic quit entry point AppWindow::RequestClose() | left-click | `src/window.cpp:214` |
| `appwindow-resize-api` | Programmatic window resize entry point AppWindow::Resize(hwnd, w, h) | left-click | `src/window.cpp:190` |
| `crop-pick-lbuttondown-start-drag` | Render window client area (crop pick mode) - left mouse button press | left-click | `src/window.cpp:84` |
| `crop-pick-lbuttonup-commit` | Render window client area (crop pick mode) - release left button to commit the crop | left-click | `src/window.cpp:100` |
| `crop-pick-mode-cleared-mid-drag` | Turning crop pick mode off from the GUI (export panel) while the left button is still held down mid-drag | left-click | `src/window.cpp:119` |
| `render-rt-size-rebind` | Render resolution change feeding AppWindow::SetRenderRtSize(w, h) - the coordinate space every crop drag is reported in | left-click | `src/window.cpp:218` |
| `crop-pick-other-mouse-buttons` | Right-click / middle-click / mouse wheel inside the client area while crop pick mode is active | right-click | `src/window.cpp:112` |
| `rbutton-look-aborts-crop-drag` | Right mouse button press/release inside the client area while a crop drag is in progress | right-click | `src/window.cpp:112` |

## Ready-view shell (16)

Breakdown: hover 7, left-click 5, drag 3, key 1

| id | control | input | source |
|---|---|---|---|
| `main-window-move-resize` | The full-screen host window "##main" | drag | `src/gui/gui_panels.cpp:436` |
| `os-window-resize-pane-clamp` | OS window resize (dragging the Win32 window border, maximize/restore, snap) | drag | `src/gui/gui_panels.cpp:396` |
| `splitter-right-drag` | Right vertical splitter between the centre scene pane and the inspector pane | drag | `src/gui/gui_panels.cpp:416` |
| `splitter-right-hover` | Hover feedback on the right splitter | hover | `src/gui/gui_splitter.cpp:14` |
| `status-export-tag-tooltip-busy` | Tooltip on the export status tag, Capturing/Encoding phase | hover | `src/gui/gui_panels.cpp:327` |
| `status-export-tag-tooltip-done` | Tooltip on the export status tag, Done phase | hover | `src/gui/gui_panels.cpp:323` |
| `status-export-tag-tooltip-failed` | Tooltip on the export status tag, Failed phase | hover | `src/gui/gui_panels.cpp:325` |
| `status-last-error-tooltip` | Tooltip on the truncated error text in the status strip | hover | `src/gui/gui_panels.cpp:364` |
| `status-open-folder-tooltip` | Tooltip on the "Open folder" button | hover | `src/gui/gui_panels.cpp:335` |
| `topbar-export-button-tooltip` | Tooltip on the "Export..." button | hover | `src/gui/gui_panels.cpp:277` |
| `keyboard-nav-focus-activate` | Keyboard navigation over every focusable item on the ready-view shell (main-view tab Buttons, Export Button, export status SmallButtons, "Open folder" SmallButton, IFS filter InputText, IFS tree TreeNodeEx directory nodes and Selectable file rows) | key | `src/gui/gui_window.cpp:153` |
| `ifs-tree-dir-expand` | IFS directory node in the browse tree (label "<segment>   (<file_count>)") | left-click | `src/gui/gui_panels.cpp:155` |
| `ifs-tree-file-select` | IFS file leaf row in the browse tree (Selectable named after the last path segment) | left-click | `src/gui/gui_panels.cpp:138` |
| `main-view-tab-button` | Main-view tab button in the top bar (one per active MainTab panel: "Renderer", "qpro", …) | left-click | `src/gui/gui_panels.cpp:251` |
| `status-export-tag-button` | Export status tag SmallButton in the status strip (dynamic label: "capturing N" / "capturing N [NVENC]", "encoding N frames...", "export done", "export failed") | left-click | `src/gui/gui_panels.cpp:319` |
| `topbar-export-button` | "Export..." button (export glyph + label) at the right of the top bar | left-click | `src/gui/gui_panels.cpp:272` |

## 3D scene and 2D package panels (14)

Breakdown: key 7, left-click 3, drag 1, hover 1, right-click 1, scroll 1

| id | control | input | source |
|---|---|---|---|
| `s3d-freelook-mouse-move` | Mouse movement while holding right mouse (look around) | drag | `src/scene3d/scene3d_input.cpp:65` |
| `s3d-model-row-blend-tooltip` | Model-list hover tooltip (blend mode explanation) | hover | `src/gui/gui_scene3d_panel.cpp:105` |
| `panel-slider-enter-key-text-input` | Enter key on a nav-focused slider or drag (opens the value text box) | key | `src/gui/gui_gc2d_panel.cpp:42` |
| `panel-slider-keyboard-tweak` | Left / Right arrow keys on a nav-activated slider or drag | key | `src/gui/gui_scene3d_panel.cpp:24` |
| `s3d-freelook-alt-slow` | Alt (move slower) | key | `src/scene3d/scene3d_input.cpp:94` |
| `s3d-freelook-down-keys` | Q or Ctrl (move camera down) | key | `src/scene3d/scene3d_input.cpp:92` |
| `s3d-freelook-shift-fast` | Shift (move faster) | key | `src/scene3d/scene3d_input.cpp:93` |
| `s3d-freelook-up-keys` | E or Space (move camera up) | key | `src/scene3d/scene3d_input.cpp:91` |
| `s3d-freelook-wasd` | W / A / S / D movement keys during free look | key | `src/scene3d/scene3d_input.cpp:87` |
| `gc2d-animation-combo-item` | Animation name row inside the picker combo | left-click | `src/gui/gui_gc2d_panel.cpp:25` |
| `s3d-model-blend-combo-item` | Blend mode option inside the per-model combo (opaque / alpha / additive / subtract) | left-click | `src/gui/gui_scene3d_panel.cpp:95` |
| `s3d-models-collapsing-header` | "Models" collapsing header | left-click | `src/gui/gui_scene3d_panel.cpp:82` |
| `s3d-freelook-rmb-down` | Hold right mouse button in the render window to enter free look | right-click | `src/scene3d/scene3d_input.cpp:53` |
| `gc2d-animation-combo-popup-scroll` | Scroll region inside the animation picker combo popup (##gc2danim) | scroll | `src/gui/gui_gc2d_panel.cpp:22` |

## Command line (12)

Breakdown: cli-arg 12

| id | control | input | source |
|---|---|---|---|
| `duplicate-scalar-flag-last-wins` | any scalar flag repeated in one command line | cli-arg | `src/cli/cli.cpp:637-652 (loop), :565 (bool assign), :575 (string assign), :586/:595/:615 (int assigns)` |
| `help-short` | -h | cli-arg | `src/cli/cli.cpp:639` |
| `help-slash` | /? | cli-arg | `src/cli/cli.cpp:639` |
| `missing-value` | any value-taking flag typed as the last argv token | cli-arg | `src/cli/cli.cpp:185` |
| `no-arguments-at-all` | launching with an empty command line | cli-arg | `src/cli/cli.cpp:637 (loop starts at c.i = 1), :653; src/main.cpp:413-419` |
| `no-tool-command` | launching with no tool subcommand at all | cli-arg | `src/tool_commands.cpp:116` |
| `qpro-oneshot-precedence` | passing more than one qpro one-shot flag at once | cli-arg | `src/main.cpp:147-181 (RunQproOneShot), :183-194 (RunQproCliMode), :331-337 (RunQproCliAndExit), :429 (gate)` |
| `tool-scene3d-test` | --scene3d-test <in> [out.png] [frames] | cli-arg | `src/cli/tool_command.cpp:65` |
| `tool-scene3d-test-frames` | --scene3d-test positional 3: frame count | cli-arg | `src/cli/tool_command.cpp:58` |
| `tool-scene3d-test-out` | --scene3d-test positional 2: output PNG path | cli-arg | `src/cli/tool_command.cpp:57` |
| `unknown-argument` | any unrecognised token | cli-arg | `src/cli/cli.cpp:649` |
| `value-flag-consumes-following-flag` | any value-taking flag whose next argv token is itself a flag | cli-arg | `src/cli/cli.cpp:185-192 (NextArg), consumed by src/cli/cli.cpp:575, :585, :593, :602 and every Handle* special` |

## Inspector (10)

Breakdown: left-click 7, scroll 2, key 1

| id | control | input | source |
|---|---|---|---|
| `properties-slot-bitmap-combo-keyboard-nav` | Bitmap combo popup keyboard navigation (arrows / Enter / Escape) | key | `src/gui/gui_inspector.cpp:54` |
| `inspector-tab-select` | Inspector tab (Properties / Render / Live / 3D scene / 2D package) | left-click | `src/gui/gui_inspector.cpp:446` |
| `properties-play-replay-button` | Play / Replay button (label is "Replay" when this layer is already the playing animation, otherwise "Play") | left-click | `src/gui/gui_inspector.cpp:107` |
| `properties-slot-bitmap-combo-name-item` | Bitmap name entry inside the bitmap combo | left-click | `src/gui/gui_inspector.cpp:50` |
| `render-background-segment` | "Background" segmented control ("default" / "grey" / "black" / "red" / "green" / "blue") | left-click | `src/gui/gui_inspector.cpp:270` |
| `render-continuous-loop-segment` | "Continuous loop" segmented control ("OFF" / "default" / "ON") | left-click | `src/gui/gui_inspector.cpp:205` |
| `render-mc-name-type-segment` | MC name type segmented control ("at clip pos" / "column") | left-click | `src/gui/gui_inspector.cpp:305` |
| `render-root-loop-segment` | "Loop root" segmented control ("Auto-hold" / "Force loop") | left-click | `src/gui/gui_inspector.cpp:184` |
| `live-mc-names-list-scroll` | "MC names (N)" list child window | scroll | `src/gui/gui_inspector.cpp:415` |
| `properties-slot-bitmap-combo-popup-scroll` | Bitmap combo popup list | scroll | `src/gui/gui_inspector.cpp:48` |

## Setup view (10)

Breakdown: combo-select 4, left-click 3, drag 1, hover 1, scroll 1

| id | control | input | source |
|---|---|---|---|
| `game-profile-select-auto` | 'Auto (...)' entry in the game profile combo | combo-select | `src/gui/gui_setup_view.cpp:122` |
| `game-profile-select-row` | Profile row in the game profile combo (one per GameProfile::All() entry) | combo-select | `src/gui/gui_setup_view.cpp:128` |
| `render-preset-select-custom` | 'Custom' row in the render preset combo | combo-select | `src/gui/gui_setup_view.cpp:230` |
| `render-preset-select-row` | Resolution preset row in the render preset combo | combo-select | `src/gui/gui_setup_view.cpp:230` |
| `setup-window-scrollbar-drag` | '##setup' window vertical scrollbar grab | drag | `src/gui/gui_setup_view.cpp:404` |
| `error-banner-region` | Last-attempt-failed error banner | hover | `src/gui/gui_setup_view.cpp:62` |
| `render-fps-quick-preset` | Quick frame-rate preset button (30 / 60 / 120 / 144) | left-click | `src/gui/gui_setup_view.cpp:169` |
| `render-fps-step-minus` | InputInt step-down button ('-') on the frame rate field | left-click | `src/gui/gui_setup_view.cpp:152` |
| `render-fps-step-plus` | InputInt step-up button ('+') on the frame rate field | left-click | `src/gui/gui_setup_view.cpp:152` |
| `setup-window-scroll` | Setup full-viewport window background (scroll) | scroll | `src/gui/gui_setup_view.cpp:404` |

## qpro panel (10)

Breakdown: checkbox 4, left-click 3, hover 1, key 1, scroll 1

| id | control | input | source |
|---|---|---|---|
| `cat-face` | Face category checkbox | checkbox | `src/gui/gui_qpro_panel.cpp:266` |
| `cat-hair` | Hair category checkbox | checkbox | `src/gui/gui_qpro_panel.cpp:264` |
| `cat-hand` | Hand category checkbox | checkbox | `src/gui/gui_qpro_panel.cpp:262` |
| `part-checkbox` | Individual scanned part checkbox | checkbox | `src/gui/gui_qpro_panel.cpp:141` |
| `animated-parts-note-tooltip` | "Animated parts -> .webm (VP9) + .mp4 (HEVC-alpha, Safari) + .avif poster" label (hover tooltip) | hover | `src/gui/gui_qpro_panel.cpp:247` |
| `output-fps-keyboard-edit` | Output fps text field, keyboard editing keymap | key | `src/gui/gui_qpro_panel.cpp:234` |
| `folder-picker-dialog` | Native "browse for folder" shell dialog raised by the extract button | left-click | `src/gui/gui_qpro_panel.cpp:284` |
| `output-fps-minus` | Output fps decrement step button | left-click | `src/gui/gui_qpro_panel.cpp:234` |
| `output-fps-plus` | Output fps increment step button | left-click | `src/gui/gui_qpro_panel.cpp:234` |
| `issues-list-scroll` | Skipped/failed issues list scroll region | scroll | `src/gui/gui_qpro_panel.cpp:40` |

## Scene pane (6)

Breakdown: key 3, left-click 3

| id | control | input | source |
|---|---|---|---|
| `scene-keyboard-nav-focus` | Keyboard nav cursor over the scene pane items | key | `src/gui/gui_scene_panel.cpp:267 (BeginChild) with nav enabled at src/gui/gui_window.cpp:153` |
| `scene-tree-keyboard-arrow-expand` | Left / Right arrow on a nav-focused tree node (layer row or sublayer node) | key | `src/gui/gui_scene_panel.cpp:107 and src/gui/gui_scene_panel.cpp:175` |
| `sublayer-node-keyboard-toggle` | Sublayer tree node activated with the keyboard (Space / Enter) | key | `src/gui/gui_scene_panel.cpp:107` |
| `sublayer-node-expand` | Sublayer tree node expand/collapse arrow | left-click | `src/gui/gui_scene_panel.cpp:107` |
| `sublayer-node-select` | Sublayer tree node (click on the label) | left-click | `src/gui/gui_scene_panel.cpp:109` |
| `unresolved-slot-selectable` | Unresolved / unmatched variant slot row | left-click | `src/gui/gui_scene_panel.cpp:142` |
