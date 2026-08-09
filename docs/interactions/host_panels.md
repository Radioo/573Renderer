# 3D scene and 2D package panels

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `gc2d-pause-checkbox` | "Pause" checkbox (2D package playback) | checkbox | - | 1 |
| 2 | `s3d-animate-camera-checkbox` | "Animate camera" checkbox | checkbox | yes | 2 |
| 3 | `s3d-animate-models-checkbox` | "Animate models" checkbox | checkbox | yes | 1 |
| 4 | `s3d-free-camera-checkbox` | "Free camera" checkbox | checkbox | yes | 1 |
| 5 | `s3d-model-visible-checkbox` | Per-model visibility checkbox (one per model row) | checkbox | - | 2 |
| 6 | `s3d-pause-checkbox` | "Pause" checkbox (3D scene playback) | checkbox | - | 2 |
| 7 | `gc2d-animation-combo-open` | Animation picker combo (2D package) | combo-select | - | 2 |
| 8 | `s3d-model-blend-combo-open` | Per-model blend mode combo (one per model row) | combo-select | - | 1 |
| 9 | `gc2d-frame-slider-drag` | Frame scrubber ("frame %d", 0 .. length-1) | drag | yes | 1 |
| 10 | `gc2d-speed-slider-drag` | "speed" slider (2D package playback speed, 0.00x - 4.00x) | drag | - | 1 |
| 11 | `s3d-freelook-mouse-move` | Mouse movement while holding right mouse (look around) | drag | - | **none** |
| 12 | `s3d-move-speed-drag` | "move speed" drag float (free camera translation speed, units/s) | drag | - | 1 |
| 13 | `s3d-speed-slider-drag` | "speed" slider (3D scene playback speed, 0.00x - 4.00x) | drag | - | 1 |
| 14 | `s3d-time-slider-drag` | Animation time scrubber (full width, "tick %.0f") | drag | yes | 1 |
| 15 | `gc2d-frame-slider-tooltip` | Frame scrubber hover tooltip | hover | yes | 1 |
| 16 | `s3d-animate-camera-tooltip` | "Animate camera" hover tooltip (three variants) | hover | yes | 2 |
| 17 | `s3d-animate-models-tooltip` | "Animate models" hover tooltip | hover | yes | 1 |
| 18 | `s3d-free-camera-tooltip` | "Free camera" hover tooltip | hover | yes | 1 |
| 19 | `s3d-model-row-blend-tooltip` | Model-list hover tooltip (blend mode explanation) | hover | yes | **none** |
| 20 | `s3d-time-slider-tooltip` | Animation time scrubber hover tooltip | hover | yes | 1 |
| 21 | `panel-keyboard-nav-focus` *(audit)* | Keyboard navigation across every control in both panels (arrow keys / Tab to move focus, Space or Enter to activate) | key | - | 2 |
| 22 | `panel-slider-enter-key-text-input` *(audit)* | Enter key on a nav-focused slider or drag (opens the value text box) | key | - | **none** |
| 23 | `panel-slider-keyboard-tweak` *(audit)* | Left / Right arrow keys on a nav-activated slider or drag | key | - | **none** |
| 24 | `s3d-freelook-alt-slow` | Alt (move slower) | key | - | **none** |
| 25 | `s3d-freelook-down-keys` | Q or Ctrl (move camera down) | key | - | **none** |
| 26 | `s3d-freelook-shift-fast` | Shift (move faster) | key | - | **none** |
| 27 | `s3d-freelook-up-keys` | E or Space (move camera up) | key | - | **none** |
| 28 | `s3d-freelook-wasd` | W / A / S / D movement keys during free look | key | - | **none** |
| 29 | `s3d-move-speed-drag-speed-modifiers` *(audit)* | Shift / Alt held while mouse-dragging the "move speed" DragFloat | key | - | 1 |
| 30 | `gc2d-animation-combo-item` | Animation name row inside the picker combo | left-click | - | **none** |
| 31 | `s3d-model-blend-combo-item` | Blend mode option inside the per-model combo (opaque / alpha / additive / subtract) | left-click | - | **none** |
| 32 | `s3d-models-collapsing-header` | "Models" collapsing header | left-click | - | **none** |
| 33 | `s3d-reset-view-button` | "Reset view" button | left-click | - | 1 |
| 34 | `select-2d-package-tab` | "2D package" inspector tab | left-click | - | 2 |
| 35 | `select-3d-scene-tab` | "3D scene" inspector tab | left-click | - | 3 |
| 36 | `s3d-freelook-rmb-down` | Hold right mouse button in the render window to enter free look | right-click | - | **none** |
| 37 | `gc2d-animation-combo-popup-scroll` *(audit)* | Scroll region inside the animation picker combo popup (##gc2danim) | scroll | - | **none** |
| 38 | `scroll-inspector-pane` | Inspector pane scroll region containing both panels | scroll | - | 10 |
| 39 | `gc2d-frame-slider-ctrl-click-entry` | Frame scrubber keyboard value entry | text-entry | - | 1 |
| 40 | `gc2d-speed-slider-ctrl-click-entry` | "speed" slider keyboard value entry (2D package) | text-entry | - | 1 |
| 41 | `s3d-move-speed-ctrl-click-entry` | "move speed" keyboard value entry | text-entry | - | 1 |
| 42 | `s3d-speed-slider-ctrl-click-entry` | "speed" slider keyboard value entry | text-entry | - | 1 |
| 43 | `s3d-time-slider-ctrl-click-entry` | Animation time scrubber keyboard value entry | text-entry | - | 1 |
| 44 | `s3d-freelook-capture-lost` | Loss of mouse capture during free look (alt-tab, focus steal) | window-message | - | n/a |
| 45 | `s3d-freelook-rmb-up` | Release right mouse button to leave free look | window-message | - | n/a |

## Detail

### 1. "Pause" checkbox (2D package playback)

- **id**: `gc2d-pause-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/2D package/Pause##gc2d`
- **precondition**: 2D package panel body is drawn, i.e. Gc2dHost::Active() (early-out at gui_gc2d_panel.cpp:55)
- **effect**: Calls Gc2dHost::SetPaused(paused), stopping/resuming package frame advance
- **source**: `src/gui/gui_gc2d_panel.cpp:34`
- **notes**: Also flipped implicitly to true by scrubbing the frame slider.
- **tests**: `2D package panel drives a loaded mock package`
- **audit correction**: Precondition cites the panel early-out at gui_gc2d_panel.cpp:55. Line 55 is the ImGui::TextDisabled("No 2D package loaded.") inside the early-out body; the guard itself is line 54. -> early-out at src/gui/gui_gc2d_panel.cpp:54 (`if (!Gc2dHost::Active()) {`), with the placeholder text at :55-57. Compare the sibling entry for the 3D panel, which correctly cites gui_scene3d_panel.cpp:144 for the guard line.

### 2. "Animate camera" checkbox

- **id**: `s3d-animate-camera-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/3D scene/Animate camera##s3d`
- **precondition**: 3D scene panel body is drawn AND Status::has_authored_camera is true AND Status::free_camera is false
- **disabled when**: !st.has_authored_camera \|\| st.free_camera (wrapped in ImGui::BeginDisabled at gui_scene3d_panel.cpp:62)
- **effect**: Calls Scene3dHost::SetAnimateCamera(camera); when off the camera is evaluated at the frozen hold time g_camera_hold (scene3d_host.cpp:109)
- **source**: `src/gui/gui_scene3d_panel.cpp:64`
- **tooltip**: yes
- **notes**: Drawn on the same line as Animate models (SameLine at line 61).
- **tests**: `3D scene camera checkbox is gated on an authored camera`, `3D scene free camera and reset drive the camera state`

### 3. "Animate models" checkbox

- **id**: `s3d-animate-models-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/3D scene/Animate models##s3d`
- **precondition**: 3D scene panel body is drawn
- **effect**: Calls Scene3dHost::SetAnimateModels(models); when off, models are drawn at the frozen hold time g_model_hold instead of the running clock (scene3d_host.cpp:108)
- **source**: `src/gui/gui_scene3d_panel.cpp:55`
- **tooltip**: yes
- **tests**: `3D scene panel drives a loaded mock scene`

### 4. "Free camera" checkbox

- **id**: `s3d-free-camera-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/3D scene/Free camera##s3d`
- **precondition**: 3D scene panel body is drawn
- **effect**: Calls Scene3dHost::SetFreeCamera(freecam), which flips g_camera.active; when on, Scene3dHost::RenderFrame feeds the free camera view matrix to the renderer instead of the authored camera (scene3d_host.cpp:93, :111). Turning it on also disables the Animate camera checkbox
- **source**: `src/gui/gui_scene3d_panel.cpp:113`
- **tooltip**: yes
- **tests**: `3D scene free camera and reset drive the camera state`

### 5. Per-model visibility checkbox (one per model row)

- **id**: `s3d-model-visible-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/3D scene/Models##s3d/<row i>/##vis`
- **precondition**: Models header is open and the model list is non-empty
- **effect**: Calls Scene3dHost::SetModelVisible(index, vis) for that row's model index
- **source**: `src/gui/gui_scene3d_panel.cpp:87`
- **notes**: ONE entry describing a row drawn in a loop over Scene3dHost::ListModels(); each row is scoped by ImGui::PushID((int)i) at line 85.
- **tests**: `3D scene model list toggles visibility and blend mode`, `scene pane child visibility checkbox records a sublayer override`

### 6. "Pause" checkbox (3D scene playback)

- **id**: `s3d-pause-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/3D scene/Pause##s3d`
- **precondition**: 3D scene panel body is drawn, i.e. Scene3dHost::Active() is true (early-out at gui_scene3d_panel.cpp:144)
- **effect**: On toggle calls Scene3dHost::SetPaused(paused), which stops/resumes advancing g_time in Scene3dHost::RenderFrame (scene3d_host.cpp:95)
- **source**: `src/gui/gui_scene3d_panel.cpp:20`
- **notes**: Checkbox state is mirrored from Scene3dHost::Status::paused each frame.
- **tests**: `3D scene panel drives a loaded mock scene`, `3D scene panel explains how to load a scene while none is live`

### 7. Animation picker combo (2D package)

- **id**: `gc2d-animation-combo-open`
- **input**: combo-select
- **path**: `##inspector_tabs/2D package/##gc2danim`
- **precondition**: Gc2dHost::Active() and Gc2dHost::ListAnimations() is non-empty; otherwise a "This package declares no animations." disabled text is drawn instead (gui_gc2d_panel.cpp:18)
- **disabled when**: never (hidden when the package declares no animations)
- **effect**: Opens the combo popup listing every animation name; the preview shows Gc2dHost::Status::animation
- **source**: `src/gui/gui_gc2d_panel.cpp:22`
- **notes**: Item width -FLT_MIN (full pane width, SetNextItemWidth at line 21).
- **tests**: `2D package animation combo switches the playing animation`, `2D package panel stays hidden while no package is live`

### 8. Per-model blend mode combo (one per model row)

- **id**: `s3d-model-blend-combo-open`
- **input**: combo-select
- **path**: `##inspector_tabs/3D scene/Models##s3d/<row i>/##blend`
- **precondition**: Models header is open
- **effect**: Opens the combo popup listing the four blend modes; the closed preview shows BlendLabel(models[i].blend_mode) (opaque / alpha / additive / subtract)
- **source**: `src/gui/gui_scene3d_panel.cpp:90`
- **notes**: Row-loop widget; fixed width 110 (SetNextItemWidth at line 89).
- **tests**: `3D scene model list toggles visibility and blend mode`

### 9. Frame scrubber ("frame %d", 0 .. length-1)

- **id**: `gc2d-frame-slider-drag`
- **input**: drag
- **path**: `##inspector_tabs/2D package/##gc2dframe`
- **precondition**: 2D package panel body is drawn
- **disabled when**: never (range collapses to 0..0 when Status::length <= 1, via std::max(st.length - 1, 0))
- **effect**: Calls Gc2dHost::SetPaused(true) and then Gc2dHost::SetFrame(frame): scrubbing force-pauses playback and jumps the playhead to that package frame
- **source**: `src/gui/gui_gc2d_panel.cpp:42`
- **tooltip**: yes
- **notes**: Item width -FLT_MIN (full pane width). The implicit auto-pause is the behavioural difference from the 3D time scrubber.
- **tests**: `2D package panel drives a loaded mock package`

### 10. "speed" slider (2D package playback speed, 0.00x - 4.00x)

- **id**: `gc2d-speed-slider-drag`
- **input**: drag
- **path**: `##inspector_tabs/2D package/speed##gc2d`
- **precondition**: 2D package panel body is drawn
- **effect**: Calls Gc2dHost::SetSpeed(speed)
- **source**: `src/gui/gui_gc2d_panel.cpp:38`
- **notes**: Fixed width 120 (SetNextItemWidth at line 37); format "%.2fx"; drawn on the same line as the Pause checkbox.
- **tests**: `2D package panel drives a loaded mock package`

### 11. Mouse movement while holding right mouse (look around)

- **id**: `s3d-freelook-mouse-move`
- **input**: drag
- **precondition**: Free look is active (g_looking true)
- **disabled when**: Not looking, or Scene3d input disabled
- **effect**: WM_MOUSEMOVE accumulates cursor delta from the anchor into g_dx/g_dy and warps the cursor back with SetCursorPos; PollCameraInput hands the delta to Scene3d::UpdateFreeCamera as look_dx/look_dy, rotating yaw/pitch
- **source**: `src/scene3d/scene3d_input.cpp:65`
- **notes**: Yaw/pitch results are displayed read-only at gui_scene3d_panel.cpp:138.
- **tests**: none

### 12. "move speed" drag float (free camera translation speed, units/s)

- **id**: `s3d-move-speed-drag`
- **input**: drag
- **path**: `##inspector_tabs/3D scene/move speed##s3d`
- **precondition**: 3D scene panel body is drawn
- **disabled when**: never (writable even when free camera is off)
- **effect**: Writes directly into Scene3dHost::MutCamera().speed (Scene3d::FreeCamera::speed), range 0.01 - 10000.0, step 0.05, format "%.2f /s"; consumed by Scene3d::UpdateFreeCamera
- **source**: `src/gui/gui_scene3d_panel.cpp:127`
- **notes**: Fixed width 140 (SetNextItemWidth at line 126). No setter call; the panel mutates the live camera struct by reference (line 125).
- **tests**: `3D scene move speed drag edits the camera`

### 13. "speed" slider (3D scene playback speed, 0.00x - 4.00x)

- **id**: `s3d-speed-slider-drag`
- **input**: drag
- **path**: `##inspector_tabs/3D scene/speed##s3d`
- **precondition**: 3D scene panel body is drawn
- **effect**: While dragging, calls Scene3dHost::SetSpeed(speed); the value multiplies the per-frame tick advance in Scene3dHost::RenderFrame
- **source**: `src/gui/gui_scene3d_panel.cpp:24`
- **notes**: Fixed item width 120 (SetNextItemWidth at line 23); display format "%.2fx".
- **tests**: `3D scene panel drives a loaded mock scene`

### 14. Animation time scrubber (full width, "tick %.0f")

- **id**: `s3d-time-slider-drag`
- **input**: drag
- **path**: `##inspector_tabs/3D scene/##s3dtime`
- **precondition**: 3D scene panel body is drawn
- **effect**: Calls Scene3dHost::SetTime(t) with a tick value in 0 .. max(Status::max_time, 1.0), scrubbing the .x AnimationKey playhead
- **source**: `src/gui/gui_scene3d_panel.cpp:30`
- **tooltip**: yes
- **notes**: Item width is -FLT_MIN (stretches to pane width). Unlike the 2D frame slider, scrubbing does NOT auto-pause playback.
- **tests**: `3D scene panel drives a loaded mock scene`

### 15. Frame scrubber hover tooltip

- **id**: `gc2d-frame-slider-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/2D package/##gc2dframe`
- **precondition**: Mouse hovers the ##gc2dframe slider
- **effect**: ImGui::SetTooltip explaining that the value is the playhead in package frames and reporting how many frames the current animation runs (Status::length)
- **source**: `src/gui/gui_gc2d_panel.cpp:46`
- **tooltip**: yes
- **notes**: Only tooltip in gui_gc2d_panel.cpp.
- **tests**: `2D package panel drives a loaded mock package`

### 16. "Animate camera" hover tooltip (three variants)

- **id**: `s3d-animate-camera-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/3D scene/Animate camera##s3d`
- **precondition**: Mouse hovers the Animate camera checkbox; the tooltip is emitted after EndDisabled so it also shows while the control is disabled
- **effect**: ImGui::SetTooltip with one of three messages: no camera exists in the .x file; free camera is on so the authored camera is not driving the view; or an explanation that the option freezes the .x-authored camera without pausing the models
- **source**: `src/gui/gui_scene3d_panel.cpp:66`
- **tooltip**: yes
- **notes**: Counted as one tooltip site with branching text (lines 67-75).
- **tests**: `3D scene camera checkbox is gated on an authored camera`, `3D scene free camera and reset drive the camera state`
- **audit correction**: Precondition claims "the tooltip is emitted after EndDisabled so it also shows while the control is disabled", and the effect lists three reachable messages. That is wrong: IsItemHovered() with default flags returns false for an item whose recorded ItemFlags carry ImGuiItemFlags_Disabled (imgui.cpp:4868-4870 for the mouse path, imgui.cpp:4821-4822 for the nav path). EndDisabled() at gui_scene3d_panel.cpp:65 does not clear g.LastItemData, so the checkbox submitted inside BeginDisabled still reports as disabled at line 66. -> Only ONE of the three branches is ever reachable. The two disabled-state messages at gui_scene3d_panel.cpp:68 ("This scene has no camera in its .x file.") and :70 ("Free camera is on, ...") are dead code, because they are only selected when the checkbox was disabled - exactly the case in which IsItemHovered() returns false. The user only ever sees the line-73 message ("Freeze the camera animated inside the .x file..."). Fixing it needs ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled).

### 17. "Animate models" hover tooltip

- **id**: `s3d-animate-models-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/3D scene/Animate models##s3d`
- **precondition**: Mouse hovers the Animate models checkbox
- **effect**: ImGui::SetTooltip explaining that the option freezes every frame transform in place while the clock keeps running
- **source**: `src/gui/gui_scene3d_panel.cpp:56`
- **tooltip**: yes
- **tests**: `3D scene panel drives a loaded mock scene`

### 18. "Free camera" hover tooltip

- **id**: `s3d-free-camera-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/3D scene/Free camera##s3d`
- **precondition**: Mouse hovers the Free camera checkbox
- **effect**: ImGui::SetTooltip explaining that OFF uses the .x-authored camera, with a second line that differs depending on whether the scene has an authored camera
- **source**: `src/gui/gui_scene3d_panel.cpp:114`
- **tooltip**: yes
- **notes**: One tooltip site with a branching second line (lines 117-120).
- **tests**: `3D scene free camera and reset drive the camera state`

### 19. Model-list hover tooltip (blend mode explanation)

- **id**: `s3d-model-row-blend-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/3D scene/Models##s3d/<last row>/<model name text>`
- **precondition**: Models header is open and the mouse hovers the last submitted item of the model list, i.e. the model name text of the final row
- **effect**: ImGui::SetTooltip explaining that the blend mode comes from the game's per-screen setup code and can be overridden here to inspect a layer
- **source**: `src/gui/gui_scene3d_panel.cpp:105`
- **tooltip**: yes
- **notes**: The IsItemHovered() call sits AFTER the row loop and after PopID, so it tests only the last row's TextUnformatted item (line 102), not every row.
- **tests**: none

### 20. Animation time scrubber hover tooltip

- **id**: `s3d-time-slider-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/3D scene/##s3dtime`
- **precondition**: Mouse hovers the ##s3dtime slider
- **effect**: ImGui::SetTooltip explaining that the value is animation time in .x AnimationKey ticks and stating the tick count at which the scene loops (Status::max_time)
- **source**: `src/gui/gui_scene3d_panel.cpp:33`
- **tooltip**: yes
- **notes**: Single tooltip, formatted with st.max_time.
- **tests**: `3D scene panel drives a loaded mock scene`

### 21. Keyboard navigation across every control in both panels (arrow keys / Tab to move focus, Space or Enter to activate)

- **id**: `panel-keyboard-nav-focus` *(audit)*
- **input**: key
- **path**: `##inspector_tabs/3D scene, ##inspector_tabs/2D package`
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard is set at GUI init (src/gui/gui_window.cpp:153), and the 573Renderer control window has focus with no text item active
- **disabled when**: The Animate camera checkbox is skipped by nav while inside BeginDisabled(!st.has_authored_camera \|\| st.free_camera) (gui_scene3d_panel.cpp:62-65)
- **effect**: Nav focus walks the submitted items in order. Space/Enter toggles Pause##s3d, Animate models##s3d, Animate camera##s3d, the per-model ##vis checkboxes, Free camera##s3d, Pause##gc2d; presses Reset view##s3d (calling Scene3dHost::ResetCamera); toggles the Models##s3d CollapsingHeader; and opens the ##blend / ##gc2danim combo popups. Escape closes an open combo popup without selecting.
- **source**: `src/gui/gui_window.cpp:153`
- **notes**: This is why SetItemDefaultFocus at gui_scene3d_panel.cpp:97 and gui_gc2d_panel.cpp:26 is load-bearing rather than decorative. Also note IsItemHovered() at gui_scene3d_panel.cpp:33/:56/:66/:105/:114 and gui_gc2d_panel.cpp:46 takes the NAV branch when nav highlight is active (imgui.cpp:4817-4826), so nav-focusing an item shows its tooltip without the mouse.
- **tests**: `2D package panel stays hidden while no package is live`, `inspector hides host tabs while no 3D scene or 2D package is live`

### 22. Enter key on a nav-focused slider or drag (opens the value text box)

- **id**: `panel-slider-enter-key-text-input` *(audit)*
- **input**: key
- **path**: `##inspector_tabs/3D scene/speed##s3d, ##s3dtime, move speed##s3d; ##inspector_tabs/2D package/speed##gc2d, ##gc2dframe`
- **precondition**: Keyboard nav enabled (gui_window.cpp:153) and the numeric widget has nav focus
- **effect**: ImGuiSliderFlags_NoInput is not set on any of the five widgets, so Enter turns the slider/drag into a text input exactly like Ctrl+click (imgui.h:1939). Committing the typed value calls the widget's setter: Scene3dHost::SetSpeed / SetTime, Gc2dHost::SetSpeed, Gc2dHost::SetPaused(true)+SetFrame, or writes Scene3dHost::MutCamera().speed directly.
- **source**: `src/gui/gui_gc2d_panel.cpp:42`
- **notes**: The inventory has five *-ctrl-click-entry records that name only the Ctrl+click gesture; Enter is a second, equally available route into the same text box, and it is the only route when navigating without the mouse.
- **tests**: none

### 23. Left / Right arrow keys on a nav-activated slider or drag

- **id**: `panel-slider-keyboard-tweak` *(audit)*
- **input**: key
- **path**: `##inspector_tabs/3D scene/speed##s3d, ##s3dtime, move speed##s3d; ##inspector_tabs/2D package/speed##gc2d, ##gc2dframe`
- **precondition**: Keyboard nav enabled (gui_window.cpp:153) and the widget has been activated with Space/Enter
- **effect**: DragBehaviorT / SliderBehaviorT take the keyboard input path and step the value by GetNavTweakPressedAmount * tweak_factor (imgui_widgets.cpp:2540-2547). Holding Ctrl (ImGuiKey_NavKeyboardTweakSlow, imgui_internal.h:1506) makes the step 10x smaller, holding Shift (TweakFast, imgui_internal.h:1507) makes it 10x larger. Applies to speed##s3d, ##s3dtime, move speed##s3d, speed##gc2d and ##gc2dframe, driving the same setters as the mouse drag.
- **source**: `src/gui/gui_scene3d_panel.cpp:24`
- **notes**: One entry covering all five numeric widgets in the two files, which all pass ImGuiSliderFlags_None. Arrow-tweaking ##gc2dframe also fires Gc2dHost::SetPaused(true) each step (gui_gc2d_panel.cpp:43).
- **tests**: none

### 24. Alt (move slower)

- **id**: `s3d-freelook-alt-slow`
- **input**: key
- **precondition**: Free look is active
- **disabled when**: Right mouse not held, or no scene loaded
- **effect**: Sets CameraInput::slow (VK_MENU), scaling down the movement speed
- **source**: `src/scene3d/scene3d_input.cpp:94`
- **notes**: Documented at gui_scene3d_panel.cpp:133. The panel also states the mouse wheel is deliberately NOT bound to camera speed (line 134), so wheel stays available for panel scrolling.
- **tests**: none

### 25. Q or Ctrl (move camera down)

- **id**: `s3d-freelook-down-keys`
- **input**: key
- **precondition**: Free look is active
- **disabled when**: Right mouse not held, or no scene loaded
- **effect**: Sets CameraInput::down, lowering the free camera
- **source**: `src/scene3d/scene3d_input.cpp:92`
- **notes**: Documented at gui_scene3d_panel.cpp:132.
- **tests**: none

### 26. Shift (move faster)

- **id**: `s3d-freelook-shift-fast`
- **input**: key
- **precondition**: Free look is active
- **disabled when**: Right mouse not held, or no scene loaded
- **effect**: Sets CameraInput::fast, scaling the movement speed used by Scene3d::UpdateFreeCamera
- **source**: `src/scene3d/scene3d_input.cpp:93`
- **notes**: Documented at gui_scene3d_panel.cpp:133.
- **tests**: none

### 27. E or Space (move camera up)

- **id**: `s3d-freelook-up-keys`
- **input**: key
- **precondition**: Free look is active
- **disabled when**: Right mouse not held, or no scene loaded
- **effect**: Sets CameraInput::up, raising the free camera in Scene3d::UpdateFreeCamera
- **source**: `src/scene3d/scene3d_input.cpp:91`
- **notes**: Documented at gui_scene3d_panel.cpp:132.
- **tests**: none

### 28. W / A / S / D movement keys during free look

- **id**: `s3d-freelook-wasd`
- **input**: key
- **precondition**: Free look is active (g_enabled && g_looking); otherwise PollCameraInput returns all-false movement (scene3d_input.cpp:85)
- **disabled when**: Right mouse not held, or no scene loaded
- **effect**: GetAsyncKeyState sets CameraInput forward/back/left/right, moving Scene3dHost::MutCamera() position via Scene3d::UpdateFreeCamera each frame
- **source**: `src/scene3d/scene3d_input.cpp:87`
- **notes**: Documented in the panel by BulletText at gui_scene3d_panel.cpp:131. One entry for the four movement keys read on lines 87-90.
- **tests**: none

### 29. Shift / Alt held while mouse-dragging the "move speed" DragFloat

- **id**: `s3d-move-speed-drag-speed-modifiers` *(audit)*
- **input**: key
- **path**: `##inspector_tabs/3D scene/move speed##s3d`
- **precondition**: The ##s3d move speed DragFloat is being dragged with the mouse (g.ActiveIdSource == ImGuiInputSource_Mouse)
- **effect**: ImGuiSliderFlags_NoSpeedTweaks is not passed, so DragBehaviorT scales the per-pixel delta: Shift multiplies the drag rate by 10, Alt divides it by 100 (imgui_widgets.cpp:2535-2538). Fine-tuning the free-camera speed therefore requires Alt-drag; a plain drag at step 0.05 over a 0.01-10000 range is very coarse.
- **source**: `src/gui/gui_scene3d_panel.cpp:127`
- **notes**: Notable collision: the BulletText four lines below (gui_scene3d_panel.cpp:133) documents Shift = faster / Alt = slower for FREE-LOOK movement. The same two modifiers do a different thing on this widget, and neither is documented in the panel.
- **tests**: `3D scene move speed drag edits the camera`

### 30. Animation name row inside the picker combo

- **id**: `gc2d-animation-combo-item`
- **input**: left-click
- **path**: `##inspector_tabs/2D package/##gc2danim/<animation name>`
- **precondition**: The ##gc2danim popup is open
- **effect**: ImGui::Selectable click calls Gc2dHost::SelectAnimation(name), switching the played animation of the loaded system.idx/idr package
- **source**: `src/gui/gui_gc2d_panel.cpp:25`
- **notes**: ONE entry for the row drawn in the loop over ListAnimations(); the currently selected row additionally gets SetItemDefaultFocus (line 26).
- **tests**: none

### 31. Blend mode option inside the per-model combo (opaque / alpha / additive / subtract)

- **id**: `s3d-model-blend-combo-item`
- **input**: left-click
- **path**: `##inspector_tabs/3D scene/Models##s3d/<row i>/##blend/<opaque|alpha|additive|subtract>`
- **precondition**: The ##blend combo popup for that row is open
- **effect**: ImGui::Selectable click calls Scene3dHost::SetModelBlend(index, mode) with one of Scene3d::kBlendOpaque / kBlendAlpha / kBlendAdditive / kBlendSubtract, overriding the mode the game's setup code assigned
- **source**: `src/gui/gui_scene3d_panel.cpp:95`
- **notes**: One entry for the four Selectables drawn in the inner loop over modes[] (lines 91-98). The currently selected entry also gets SetItemDefaultFocus so keyboard/gamepad nav starts on it.
- **tests**: none

### 32. "Models" collapsing header

- **id**: `s3d-models-collapsing-header`
- **input**: left-click
- **path**: `##inspector_tabs/3D scene/Models##s3d`
- **precondition**: Scene3dHost::ListModels() returns a non-empty list (gui_scene3d_panel.cpp:81)
- **effect**: Toggles the header open/closed; when closed DrawModelList returns early and the per-model rows are not submitted. Defaults open (ImGuiTreeNodeFlags_DefaultOpen)
- **source**: `src/gui/gui_scene3d_panel.cpp:82`
- **notes**: Header state is ImGui-persisted, not app state.
- **tests**: none

### 33. "Reset view" button

- **id**: `s3d-reset-view-button`
- **input**: left-click
- **path**: `##inspector_tabs/3D scene/Reset view##s3d`
- **precondition**: 3D scene panel body is drawn
- **effect**: Calls Scene3dHost::ResetCamera(), re-placing the free camera from the scene bounds
- **source**: `src/gui/gui_scene3d_panel.cpp:123`
- **notes**: Drawn on the same line as the Free camera checkbox (SameLine at line 122).
- **tests**: `3D scene free camera and reset drive the camera state`

### 34. "2D package" inspector tab

- **id**: `select-2d-package-tab`
- **input**: left-click
- **path**: `##inspector_tabs/2D package`
- **precondition**: Gc2dHost::Active() is true (panel_registry.cpp:26 Gc2dTabVisible gate); only present in the scene3d panel set (panel_registry.cpp:92)
- **disabled when**: never (hidden instead of disabled when no package is loaded)
- **effect**: ImGui activates the tab item inside BeginTabBar("##inspector_tabs") and calls Panels::Gc2dPanel::Render() for the tab body each frame
- **source**: `src/gui/gui_inspector.cpp:446`
- **notes**: Same loop as the 3D scene tab entry; separate entry because it is a distinct tab with its own visibility gate.
- **tests**: `2D package panel stays hidden while no package is live`, `inspector hides host tabs while no 3D scene or 2D package is live`

### 35. "3D scene" inspector tab

- **id**: `select-3d-scene-tab`
- **input**: left-click
- **path**: `##inspector_tabs/3D scene`
- **precondition**: Scene3dHost::Active() is true (panel_registry.cpp:22 Scene3dTabVisible gate); the active game profile uses the modern or scene3d panel set (panel_registry.cpp:74, :87)
- **disabled when**: never (the tab is hidden rather than disabled when no scene is loaded)
- **effect**: ImGui activates the tab item inside BeginTabBar("##inspector_tabs") and calls Panels::Scene3dPanel::Render() for the tab body on every subsequent frame
- **source**: `src/gui/gui_inspector.cpp:446`
- **notes**: Tab label comes from PanelDesc.tab_label "3D scene" (panel_registry.cpp:75). Rendered in a loop over CollectActivePanels; this entry covers the 3D scene row of that loop.
- **tests**: `3D scene panel explains how to load a scene while none is live`, `inspector exposes the modern backend tab set`, `inspector hides host tabs while no 3D scene or 2D package is live`
- **audit correction**: Precondition says "the active game profile uses the modern or scene3d panel set". kModernPanels (panel_registry.cpp:30-55) contains renderer_view, qpro_view, properties, render and live - it has NO scene3d entry, so the 3D scene tab never appears for that set. The cited lines 74 and 87 are in kDdrPanels and kScene3dPanels. The selection key is also not the game profile slug (that is only used by QproTabVisible, panel_registry.cpp:19); it is App::Global().ActiveBackendId(). -> Precondition: Scene3dHost::Active() is true (panel_registry.cpp:21-23 Scene3dTabVisible gate) AND App::Global().ActiveBackendId() is "afp_ddr" or "scene3d", the two sets that declare the scene3d panel (panel_registry.cpp:74 in kDdrPanels, :87 in kScene3dPanels; set lookup at :105-107 and :114-116).

### 36. Hold right mouse button in the render window to enter free look

- **id**: `s3d-freelook-rmb-down`
- **input**: right-click
- **precondition**: A 3D scene is loaded so Scene3d::SetInputEnabled(true) has run (scene3d_host.cpp:69); the mouse is over the main render window
- **disabled when**: Input is disabled after Scene3dHost::Unload() calls SetInputEnabled(false) (scene3d_host.cpp:78)
- **effect**: WM_RBUTTONDOWN reaches Scene3d::HandleLookMessage from the window proc and runs BeginLook: SetCapture(hwnd), GetCursorPos anchor, ShowCursor(FALSE); the message is swallowed (returns 0 from the wndproc)
- **source**: `src/scene3d/scene3d_input.cpp:53`
- **notes**: Advertised by the panel's help text at gui_scene3d_panel.cpp:130. Dispatched from src/window.cpp:123.
- **tests**: none

### 37. Scroll region inside the animation picker combo popup (##gc2danim)

- **id**: `gc2d-animation-combo-popup-scroll` *(audit)*
- **input**: scroll
- **path**: `##inspector_tabs/2D package/##gc2danim/##Combo_00`
- **precondition**: The ##gc2danim popup is open AND the package declares more than 8 animations (Gc2dHost::ListAnimations().size() > 8)
- **effect**: BeginCombo is called with no ImGuiComboFlags, so ImGui applies ImGuiComboFlags_HeightRegular and constrains the popup to 8 items tall (imgui_widgets.cpp:2029, :2036 CalcMaxPopupHeightFromItemCount). Beyond that the popup gets its own scrollbar; the mouse wheel over the open popup scrolls the animation list instead of the pane_right inspector child. No application state changes.
- **source**: `src/gui/gui_gc2d_panel.cpp:22`
- **notes**: The sibling ##blend combo (gui_scene3d_panel.cpp:90) never scrolls: it submits exactly 4 Selectables, under the 8-item cap. The inventory has combo-open and combo-item entries for ##gc2danim but no entry for the popup's own scroll region.
- **tests**: none

### 38. Inspector pane scroll region containing both panels

- **id**: `scroll-inspector-pane`
- **input**: scroll
- **path**: `pane_right`
- **precondition**: Panel content is taller than the right pane child ("pane_right")
- **effect**: Scrolls the ImGui child window "pane_right" that hosts the inspector tab bar and the 3D scene / 2D package panel bodies; no application state changes
- **source**: `src/gui/gui_panels.cpp:421`
- **notes**: Neither panel file creates its own BeginChild; both draw directly into the enclosing scrollable pane child.
- **tests**: `2D package panel stays hidden while no package is live`, `3D scene panel explains how to load a scene while none is live`, `inspector exposes the ddr backend tab set`, `inspector exposes the modern backend tab set` (+6 more)

### 39. Frame scrubber keyboard value entry

- **id**: `gc2d-frame-slider-ctrl-click-entry`
- **input**: text-entry
- **path**: `##inspector_tabs/2D package/##gc2dframe`
- **precondition**: Scrubber hovered/active; ImGui Ctrl+click converts the SliderInt to an input box
- **effect**: Committing a typed frame number calls Gc2dHost::SetPaused(true) then Gc2dHost::SetFrame(frame)
- **source**: `src/gui/gui_gc2d_panel.cpp:42`
- **notes**: Built-in ImGui SliderInt behaviour.
- **tests**: `2D package panel drives a loaded mock package`

### 40. "speed" slider keyboard value entry (2D package)

- **id**: `gc2d-speed-slider-ctrl-click-entry`
- **input**: text-entry
- **path**: `##inspector_tabs/2D package/speed##gc2d`
- **precondition**: Slider hovered/active; ImGui Ctrl+click converts it to an input box
- **effect**: Committing a typed value calls Gc2dHost::SetSpeed(speed) clamped to 0.0-4.0
- **source**: `src/gui/gui_gc2d_panel.cpp:38`
- **notes**: Built-in ImGui SliderFloat behaviour.
- **tests**: `2D package panel drives a loaded mock package`
- **audit correction**: Same error as the 3D speed slider: effect says "clamped to 0.0-4.0". No slider flags are passed at gui_gc2d_panel.cpp:38, so ImGui does not clamp Ctrl+click / Enter text input (imgui_widgets.cpp:3355-3356). -> The typed value is passed through unclamped to Gc2dHost::SetSpeed, which clamps to 0.0 - 8.0 (src/gc2d/gc_host.cpp:138-140). The effective range for typed entry is 0.0 - 8.0, not 0.0 - 4.0.

### 41. "move speed" keyboard value entry

- **id**: `s3d-move-speed-ctrl-click-entry`
- **input**: text-entry
- **path**: `##inspector_tabs/3D scene/move speed##s3d`
- **precondition**: The drag is hovered/active; ImGui Ctrl+click on a DragFloat opens an input box
- **effect**: Committing a typed number writes the clamped value into Scene3dHost::MutCamera().speed
- **source**: `src/gui/gui_scene3d_panel.cpp:127`
- **notes**: Built-in ImGui DragFloat behaviour.
- **tests**: `3D scene move speed drag edits the camera`
- **audit correction**: Effect says the typed value is written "clamped". It is NOT clamped anywhere. DragFloat is called with no flags, and imgui_widgets.cpp:2764 passes the clamp bounds to TempInputScalar only when ImGuiSliderFlags_ClampOnInput (i.e. AlwaysClamp) is set - see the comment at imgui.h:668 "Manually input values aren't clamped by default and can go off-bounds". The panel then writes straight into the live struct returned by Scene3dHost::MutCamera() (scene3d_host.cpp:179-181), which applies no clamp of its own, so the 0.01 / 10000.0 bounds are bypassed entirely. -> Committing a typed number writes it UNCLAMPED into Scene3dHost::MutCamera().speed, bypassing the DragFloat's 0.01 - 10000.0 bounds (no ImGuiSliderFlags_ClampOnInput; imgui_widgets.cpp:2764) and with no host-side clamp (scene3d_host.cpp:179). A negative value inverts free-camera movement and a huge value makes WASD teleport the camera; the only recovery is Reset view##s3d plus retyping a sane speed.

### 42. "speed" slider keyboard value entry

- **id**: `s3d-speed-slider-ctrl-click-entry`
- **input**: text-entry
- **path**: `##inspector_tabs/3D scene/speed##s3d`
- **precondition**: Slider is hovered/active; standard ImGui Ctrl+click (or double-click) on a SliderFloat turns it into an input box
- **effect**: Typing a number and committing calls Scene3dHost::SetSpeed(speed) with the typed value clamped to 0.0-4.0
- **source**: `src/gui/gui_scene3d_panel.cpp:24`
- **notes**: Built-in ImGui SliderFloat behaviour, not extra code in the panel; listed separately because it is a distinct user gesture.
- **tests**: `3D scene panel drives a loaded mock scene`
- **audit correction**: Effect says the typed value is "clamped to 0.0-4.0". SliderScalar clamps Ctrl+click input only when ImGuiSliderFlags_ClampOnInput is set (imgui_widgets.cpp:3355-3356, and imgui.h:693), and no flags are passed at gui_scene3d_panel.cpp:24, so the slider's 4.0 maximum is not the limit. -> The typed value is NOT clamped by ImGui; it reaches Scene3dHost::SetSpeed, which clamps to 0.0 - 8.0 (scene3d_host.cpp:143-145). Typing 8 therefore yields 8x playback, double the maximum the slider handle can reach, and the handle then pins at the right edge.

### 43. Animation time scrubber keyboard value entry

- **id**: `s3d-time-slider-ctrl-click-entry`
- **input**: text-entry
- **path**: `##inspector_tabs/3D scene/##s3dtime`
- **precondition**: Scrubber is hovered/active; ImGui Ctrl+click converts the slider to an input box
- **effect**: Committing a typed tick value calls Scene3dHost::SetTime(t)
- **source**: `src/gui/gui_scene3d_panel.cpp:30`
- **notes**: Built-in ImGui SliderFloat behaviour.
- **tests**: `3D scene panel drives a loaded mock scene`

### 44. Loss of mouse capture during free look (alt-tab, focus steal)

- **id**: `s3d-freelook-capture-lost`
- **input**: window-message
- **precondition**: Free look is active
- **disabled when**: Scene3d input disabled
- **effect**: WM_CAPTURECHANGED clears g_looking and calls ShowCursor(TRUE), ending look mode; returns false so the message still falls through to the rest of the window proc
- **source**: `src/scene3d/scene3d_input.cpp:59`
- **notes**: User-visible as: the cursor reappears and camera keys stop responding.

### 45. Release right mouse button to leave free look

- **id**: `s3d-freelook-rmb-up`
- **input**: window-message
- **precondition**: Free look is active (g_looking)
- **disabled when**: Scene3d input disabled (no scene loaded)
- **effect**: WM_RBUTTONUP runs EndLook: ReleaseCapture() and ShowCursor(TRUE); the message is swallowed
- **source**: `src/scene3d/scene3d_input.cpp:56`

