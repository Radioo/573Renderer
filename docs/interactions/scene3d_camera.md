# 3D scene camera

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `load-scene-arms-camera` | Load a 3D scene directory (arms the free-look surface and resets the camera) | cli-arg | - | **none** |
| 2 | `unload-scene-disarms-camera` | Unload the current 3D scene (disarms the free-look surface) | cli-arg | - | **none** |
| 3 | `gui-move-speed-drag` *(audit)* | "move speed" DragFloat (drag left/right to change camera translation speed) | drag | - | **none** |
| 4 | `lmb-crop-drag-steals-look-capture` *(audit)* | Left-click / drag a crop rectangle in the render window while the right button is held for free-look | drag | - | **none** |
| 5 | `mouse-look-drag` | Move the mouse while the right button is held (free-look drag) | drag | - | **none** |
| 6 | `pitch-clamp` | Drag the mouse vertically past the vertical look limit | drag | - | **none** |
| 7 | `gui-freecam-checkbox-hover` *(audit)* | Hover the "Free camera" checkbox | hover | yes | **none** |
| 8 | `esc-close-during-look` | Press Escape while the render window has focus (including mid free-look) | key | - | **none** |
| 9 | `gui-move-speed-ctrl-click` *(audit)* | Ctrl+click (or double-click) the "move speed" drag to type an exact value | key | - | **none** |
| 10 | `key-a-left` | Hold A (strafe left) | key | - | **none** |
| 11 | `key-alt-slow` | Hold Alt (slow / precision movement modifier) | key | - | **none** |
| 12 | `key-ctrl-down` | Hold Ctrl (descend / move down along world Y) | key | - | **none** |
| 13 | `key-d-right` | Hold D (strafe right) | key | - | **none** |
| 14 | `key-e-up` | Hold E (rise / move up along world Y) | key | - | **none** |
| 15 | `key-q-down` | Hold Q (descend / move down along world Y) | key | - | **none** |
| 16 | `key-s-back` | Hold S (move backward) | key | - | **none** |
| 17 | `key-shift-fast` | Hold Shift (fast movement modifier) | key | - | **none** |
| 18 | `key-space-up` | Hold Space (rise / move up along world Y) | key | - | **none** |
| 19 | `key-w-forward` | Hold W (move forward) | key | - | **none** |
| 20 | `multi-key-diagonal` | Hold two or more movement keys at once (e.g. W+D, or W+Space) | key | - | **none** |
| 21 | `gui-freecam-checkbox` *(audit)* | "Free camera" checkbox in the 3D scene inspector tab | left-click | yes | **none** |
| 22 | `gui-reset-view-button` *(audit)* | "Reset view" button in the 3D scene inspector tab | left-click | - | **none** |
| 23 | `look-armed-but-inert-freecam-off` *(audit)* | Hold right mouse button in the render window while "Free camera" is OFF | right-click | - | **none** |
| 24 | `rmb-during-crop-pick-mode` *(audit)* | Right-click in the render window while crop pick mode is active | right-click | - | **none** |
| 25 | `rmb-press-begin-look` | Right mouse button press anywhere in the render window client area | right-click | - | **none** |
| 26 | `rmb-release-end-look` | Right mouse button release | right-click | - | **none** |
| 27 | `mouse-wheel-unbound` *(audit)* | Mouse wheel over the render window | scroll | - | **none** |
| 28 | `capture-lost` | Anything that takes mouse capture away from the render window (alt-tab, another window grabbing capture, a system modal) | window-message | - | n/a |
| 29 | `window-close-destroy` | Close the render window (title bar X / Alt+F4) | window-message | - | n/a |

## Detail

### 1. Load a 3D scene directory (arms the free-look surface and resets the camera)

- **id**: `load-scene-arms-camera`
- **input**: cli-arg
- **precondition**: Scene3d::Load succeeds and the renderer initialises (src/scene3d/scene3d_host.cpp:52-59)
- **disabled when**: Scene load or renderer init fails - input stays disabled and no camera controls respond
- **effect**: PlaceCameraFromBounds computes the scene bounding-box centre and radius and calls Scene3d::PlaceFreeCamera, which positions cam at (center.x, center.y + dist*0.25, center.z - dist) with dist = radius*2.2 (or 5.0 if radius is 0), sets yaw = 0, pitch = 0.12, and sets cam.speed = radius (or 2.0 if radius is 0). g_camera.active is then set to (scene.camera_model < 0), and Scene3d::SetInputEnabled(true) arms the whole free-look surface.
- **source**: `src/scene3d/camera.cpp:97`
- **notes**: This is the enabling gate for every other entry on this surface. Note the derived cam.speed: movement rate scales with the loaded model's size, so the same key held on a small model moves slowly and on a large model moves fast.
- **tests**: none
- **audit correction**: input is "cli-arg", but this is not primarily a command-line action - a scene is loaded by picking a folder in the Browse list (src/backend/scene3d_backend.cpp:103 and src/backend/afp_family_backend.cpp:421 both call Scene3dHost::Load from the GUI-driven backend request path). The cited source (src/scene3d/camera.cpp:97) is the placement helper, not the control site. -> input: "click" (Browse list folder marked [3D scene]; a CLI path is a secondary route). source: src/scene3d/scene3d_host.cpp:60 (PlaceCameraFromBounds + g_camera.active = g_scene.camera_model < 0 at :61 + SetInputEnabled(true) at :69), with camera.cpp:97-105 cited as the placement math.

### 2. Unload the current 3D scene (disarms the free-look surface)

- **id**: `unload-scene-disarms-camera`
- **input**: cli-arg
- **precondition**: g_active true (a scene is currently loaded)
- **disabled when**: g_active already false - Unload early-returns (src/scene3d/scene3d_host.cpp:77)
- **effect**: Scene3d::SetInputEnabled(false) sets g_enabled = false and calls EndLook(), which - if a look was in progress - releases mouse capture and restores the cursor with ShowCursor(TRUE). All subsequent HandleLookMessage calls return false and PollCameraInput yields no movement flags.
- **source**: `src/scene3d/scene3d_input.cpp:38`
- **notes**: The cleanup path that guarantees a hidden/captured cursor is always restored when the surface goes away.
- **tests**: none
- **audit correction**: input is "cli-arg", but unload is triggered by the GUI/backend request path (src/backend/scene3d_backend.cpp:83/105/115 and src/backend/afp_family_backend.cpp:425), i.e. selecting a different folder or a non-3D asset, not by a command-line argument. -> input: "click" (selecting another entry in the Browse list, or the backend switching away from the scene3d path); source stays src/scene3d/scene3d_input.cpp:38 with the trigger at src/scene3d/scene3d_host.cpp:76-78.

### 3. "move speed" DragFloat (drag left/right to change camera translation speed)

- **id**: `gui-move-speed-drag` *(audit)*
- **input**: drag
- **path**: `GUI window > Inspector tabs > "3D scene" > move speed##s3d`
- **precondition**: A 3D scene is loaded (panel drawn)
- **disabled when**: Never disabled; the write happens unconditionally (the call is not wrapped in an if), and it applies even when Free camera is OFF (the value simply has no visible effect until free camera is on)
- **effect**: ImGui::DragFloat writes DIRECTLY into Scene3dHost::MutCamera().speed (a non-const reference to g_camera, scene3d_host.cpp:179-181), step 0.05, clamped to [0.01, 10000.0], displayed "%.2f /s". cam.speed is the base of `step = cam.speed * dt` in UpdateFreeCamera (camera.cpp:36), so it scales every WASD/QE/Space/Ctrl translation before the Shift x5 / Alt x0.2 modifiers.
- **source**: `src/gui/gui_scene3d_panel.cpp:127`
- **notes**: This is the only runtime control over movement rate. Note the load-time default is derived from model size (camera.cpp:104), so this is how the user fixes an unusable speed on a very large or very small scene. "Reset view" overwrites whatever is set here.
- **tests**: none

### 4. Left-click / drag a crop rectangle in the render window while the right button is held for free-look

- **id**: `lmb-crop-drag-steals-look-capture` *(audit)*
- **input**: drag
- **precondition**: Crop pick mode active (App::State::GetCropPickMode() true, window.cpp:119) AND a free-look session in progress (g_looking true)
- **disabled when**: Crop pick mode off - HandleCropPick is not called at all and left clicks fall through to DefWindowProcA
- **effect**: HandleCropPick gets first refusal (window.cpp:119-121): WM_LBUTTONDOWN calls SetCapture and sets g_crop_drag_active, then every WM_MOUSEMOVE is consumed by the crop handler and returns before HandleLookMessage, so the look drag stops accumulating deltas. On WM_LBUTTONUP the crop handler calls ReleaseCapture (window.cpp:104), which makes Windows post WM_CAPTURECHANGED; that reaches HandleLookMessage first and clears g_looking + ShowCursor(TRUE) (scene3d_input.cpp:59-64) even though the right button is still physically down. Free-look silently ends mid-gesture and the user must release and re-press RMB.
- **source**: `src/window.cpp:100-110`
- **notes**: The subsequent WM_RBUTTONUP hits EndLook's `if (!g_looking) return` guard (scene3d_input.cpp:26) so ReleaseCapture is not called a second time; the ShowCursor counter stays balanced.
- **tests**: none

### 5. Move the mouse while the right button is held (free-look drag)

- **id**: `mouse-look-drag`
- **input**: drag
- **precondition**: g_enabled true AND g_looking true (right button currently held). Also requires the crop-pick handler not to have consumed WM_MOUSEMOVE first, which it does only while g_crop_drag_active (src/window.cpp:93-98).
- **disabled when**: g_looking is false - handler returns false at scene3d_input.cpp:66 and the move is treated as an ordinary mouse move
- **effect**: Reads the absolute cursor position with GetCursorPos, accumulates (p.x - g_anchor.x) into g_dx and (p.y - g_anchor.y) into g_dy, then warps the cursor back with SetCursorPos(g_anchor) so the pointer never leaves the anchor point (infinite relative look). Returns true, swallowing the message. On the next frame PollCameraInput copies g_dx/g_dy into CameraInput::look_dx/look_dy and zeroes them, and UpdateFreeCamera applies cam.yaw += look_dx * 0.0035 and cam.pitch += look_dy * 0.0035 (src/scene3d/camera.cpp:21-22), rotating the view matrix returned by FreeCameraView.
- **source**: `src/scene3d/scene3d_input.cpp:65`
- **notes**: Accumulated deltas are drained by PollCameraInput before its enabled/looking early-return (scene3d_input.cpp:81-85), so a delta captured just before the button release still applies one frame later. The camera only actually rotates when g_camera.active is true (scene3d_host.cpp:93), which is only the case for scenes with no baked camera model (scene3d_host.cpp:61).
- **tests**: none

### 6. Drag the mouse vertically past the vertical look limit

- **id**: `pitch-clamp`
- **input**: drag
- **precondition**: An active free-look drag producing look_dy, and g_camera.active true
- **disabled when**: never (the clamp always runs when UpdateFreeCamera runs)
- **effect**: cam.pitch is clamped by std::clamp to +/-1.5533431 radians (kPitchLimit, just under 90 degrees), so the camera cannot flip over the poles. Yaw is unbounded and wraps freely.
- **source**: `src/scene3d/camera.cpp:23`
- **notes**: Recorded separately because it is a distinct user-visible gate on the look-drag gesture.
- **tests**: none

### 7. Hover the "Free camera" checkbox

- **id**: `gui-freecam-checkbox-hover` *(audit)*
- **input**: hover
- **path**: `GUI window > Inspector tabs > "3D scene" > Free camera##s3d (hover)`
- **precondition**: 3D scene panel drawn and the item hovered
- **effect**: SetTooltip shows "OFF uses the camera animated inside the .x file." plus a second line whose text depends on st.has_authored_camera: "This scene has one." or "This scene has NO authored camera, so free look is the only way to see it."
- **source**: `src/gui/gui_scene3d_panel.cpp:114`
- **tooltip**: yes
- **notes**: Only IsItemHovered/SetTooltip pair in DrawCameraControls; the Reset view button and the move speed drag have none.
- **tests**: none

### 8. Press Escape while the render window has focus (including mid free-look)

- **id**: `esc-close-during-look`
- **input**: key
- **precondition**: Crop pick mode NOT active (when it is, HandleCropPick consumes VK_ESCAPE first to cancel crop picking, src/window.cpp:69-76). HandleLookMessage never consumes WM_KEYDOWN, so Escape always falls through to WndProc while free-look is armed.
- **disabled when**: never (while the render window has keyboard focus and crop pick mode is off)
- **effect**: WndProc sets g_running = false and calls DestroyWindow(hwnd), tearing down the render window; the subsequent WM_DESTROY posts the quit message. The free-look session is not explicitly ended, but capture dies with the window.
- **source**: `src/window.cpp:130`
- **notes**: Handled outside the camera module, but it is reachable while free-look is active because the look handler passes WM_KEYDOWN through.
- **tests**: none

### 9. Ctrl+click (or double-click) the "move speed" drag to type an exact value

- **id**: `gui-move-speed-ctrl-click` *(audit)*
- **input**: key
- **path**: `GUI window > Inspector tabs > "3D scene" > move speed##s3d (ctrl+click text entry)`
- **precondition**: Panel drawn and the DragFloat hovered/active; standard ImGui DragFloat behaviour (no ImGuiSliderFlags_NoInput is passed)
- **effect**: The drag turns into an InputText field accepting the full ImGui text-edit keymap; on Enter the parsed value is written to cam.speed and clamped to [0.01, 10000.0] by DragFloat's min/max (ImGui clamps typed input because both bounds are non-zero and ImGuiSliderFlags_AlwaysClamp is not needed when v_min < v_max for DragFloat text entry).
- **source**: `src/gui/gui_scene3d_panel.cpp:127`
- **notes**: Implicit ImGui widget behaviour, but it is the only practical way to reach the far end of the 0.01..10000 range.
- **tests**: none

### 10. Hold A (strafe left)

- **id**: `key-a-left`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::left, subtracting the right basis vector (rx = cos(yaw), rz = -sin(yaw)) from the move accumulator; strafes left in the horizontal plane only (no Y component).
- **source**: `src/scene3d/scene3d_input.cpp:89`
- **notes**: Strafe ignores pitch by design - see camera.cpp:34-35 and 57-60.
- **tests**: none

### 11. Hold Alt (slow / precision movement modifier)

- **id**: `key-alt-slow`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::slow, multiplying the per-frame step by kSlowFactor = 0.2 (src/scene3d/camera.cpp:38).
- **source**: `src/scene3d/scene3d_input.cpp:94`
- **notes**: VK_MENU matches either Alt key. Holding Shift and Alt together stacks: 5.0 * 0.2 = 1.0, i.e. they cancel out (camera.cpp:37-38 apply sequentially).
- **tests**: none

### 12. Hold Ctrl (descend / move down along world Y)

- **id**: `key-ctrl-down`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Down(VK_CONTROL) also sets CameraInput::down (OR'd with Q), subtracting 1.0 from the world-Y move component.
- **source**: `src/scene3d/scene3d_input.cpp:92`
- **notes**: VK_CONTROL matches either Left or Right Ctrl.
- **tests**: none

### 13. Hold D (strafe right)

- **id**: `key-d-right`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::right, adding the right basis vector (rx, rz) to the move accumulator; strafes right in the horizontal plane.
- **source**: `src/scene3d/scene3d_input.cpp:90`
- **tests**: none

### 14. Hold E (rise / move up along world Y)

- **id**: `key-e-up`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::up, adding +1.0 to the world-Y component of the move accumulator (src/scene3d/camera.cpp:61); camera rises independently of pitch.
- **source**: `src/scene3d/scene3d_input.cpp:91`
- **notes**: OR'd with Space in the same expression; recorded separately because they are two distinct keys the user can press.
- **tests**: none

### 15. Hold Q (descend / move down along world Y)

- **id**: `key-q-down`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::down, subtracting 1.0 from the world-Y component of the move accumulator (src/scene3d/camera.cpp:62).
- **source**: `src/scene3d/scene3d_input.cpp:92`
- **notes**: OR'd with Ctrl.
- **tests**: none

### 16. Hold S (move backward)

- **id**: `key-s-back`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::back, subtracting the forward basis vector (fx, fy, fz) from the move accumulator; camera translates backward by cam.speed * dt after normalization (src/scene3d/camera.cpp:48-52).
- **source**: `src/scene3d/scene3d_input.cpp:88`
- **tests**: none

### 17. Hold Shift (fast movement modifier)

- **id**: `key-shift-fast`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Sets CameraInput::fast, multiplying the per-frame step by kFastFactor = 5.0 (src/scene3d/camera.cpp:37). Affects all six translation directions.
- **source**: `src/scene3d/scene3d_input.cpp:93`
- **notes**: VK_SHIFT matches either Left or Right Shift. On its own it moves nothing - it only scales an active movement key.
- **tests**: none

### 18. Hold Space (rise / move up along world Y)

- **id**: `key-space-up`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true for effect
- **disabled when**: Right mouse button not held, or scene input disabled
- **effect**: Down(VK_SPACE) also sets CameraInput::up (OR'd with E), adding +1.0 to the world-Y move component.
- **source**: `src/scene3d/scene3d_input.cpp:91`
- **notes**: Alternate binding for the same action as E.
- **tests**: none

### 19. Hold W (move forward)

- **id**: `key-w-forward`
- **input**: key
- **precondition**: g_enabled true AND g_looking true (right button held) - PollCameraInput returns early at scene3d_input.cpp:85 otherwise; and g_camera.active true for the movement to be applied (scene3d_host.cpp:93)
- **disabled when**: Right mouse button not held (g_looking false) or scene input disabled - all movement flags stay false
- **effect**: GetAsyncKeyState('W') sets CameraInput::forward, which adds the forward basis vector (fx = sin(yaw)*cos(pitch), fy = -sin(pitch), fz = cos(yaw)*cos(pitch)) into the move accumulator; after normalization cam.x/y/z advance by (cam.speed * dt) along it.
- **source**: `src/scene3d/scene3d_input.cpp:87`
- **notes**: Polled with GetAsyncKeyState, so it is level-triggered per frame and works regardless of keyboard focus routing/ImGui capture.
- **tests**: none

### 20. Hold two or more movement keys at once (e.g. W+D, or W+Space)

- **id**: `multi-key-diagonal`
- **input**: key
- **precondition**: g_enabled true AND g_looking true; g_camera.active true; combined move vector length > 1e-6
- **disabled when**: Combined vector length <= 1e-6 (opposing keys cancel) - the position update is skipped entirely
- **effect**: The summed direction vector (mx,my,mz) is normalized by its length before the step is applied (src/scene3d/camera.cpp:64-69), so diagonal movement is the same speed as axis-aligned movement. Exactly-opposing keys (W+S, A+D, E+Q) cancel to a near-zero vector, fail the 1e-6 length test, and produce no movement at all.
- **source**: `src/scene3d/camera.cpp:64`
- **notes**: Recorded once for all key combinations; it is the shared normalization/cancellation behaviour of the movement set.
- **tests**: none

### 21. "Free camera" checkbox in the 3D scene inspector tab

- **id**: `gui-freecam-checkbox` *(audit)*
- **input**: left-click
- **path**: `GUI window > Inspector tabs > "3D scene" > Free camera##s3d`
- **precondition**: A 3D scene is loaded (Scene3dHost::Active() true, otherwise Render() early-returns at src/gui/gui_scene3d_panel.cpp:144-149 and the whole camera block is not drawn); the 3D scene tab is visible (panel_registry.cpp:74-78, Scene3dTabVisible)
- **disabled when**: Never disabled once the panel is drawn; the panel itself is hidden when no 3D scene is loaded
- **effect**: Calls Scene3dHost::SetFreeCamera(on) which writes g_camera.active (src/scene3d/scene3d_host.cpp:131-133). That flag is the master gate every free-look entry in this inventory already depends on: when true, RenderFrame calls Scene3d::UpdateFreeCamera (scene3d_host.cpp:93) and passes &view from FreeCameraView into Renderer::Draw (scene3d_host.cpp:110-111); when false, mouse-look and all WASDQE keys are polled but discarded, and Draw receives nullptr so the .x authored camera drives the view. Toggling it also re-enables/disables the "Animate camera" checkbox (BeginDisabled(!has_authored_camera \|\| free_camera), gui_scene3d_panel.cpp:62).
- **source**: `src/gui/gui_scene3d_panel.cpp:113`
- **tooltip**: yes
- **notes**: The inventory repeatedly cites "g_camera.active true for effect" as a precondition but never records the control that sets it. Load() initialises it to (scene.camera_model < 0) at scene3d_host.cpp:61, so on a scene WITH an authored camera the user must tick this box before any free-look input does anything visible.
- **tests**: none

### 22. "Reset view" button in the 3D scene inspector tab

- **id**: `gui-reset-view-button` *(audit)*
- **input**: left-click
- **path**: `GUI window > Inspector tabs > "3D scene" > Reset view##s3d`
- **precondition**: A 3D scene is loaded (panel drawn)
- **disabled when**: Never disabled; it is also live while Free camera is OFF, in which case the reset is invisible until free camera is turned back on
- **effect**: Scene3dHost::ResetCamera() -> PlaceCameraFromBounds() (scene3d_host.cpp:147-149, 37-45) recomputes the scene bounding-box centre/radius and calls Scene3d::PlaceFreeCamera, which snaps the camera back to (center.x, center.y + dist*0.25, center.z - dist) with dist = radius*2.2 (or 5.0 when radius is 0), yaw = 0, pitch = 0.12 AND resets cam.speed = radius (or 2.0), silently discarding any value the user typed/dragged into "move speed".
- **source**: `src/gui/gui_scene3d_panel.cpp:123`
- **notes**: camera.cpp:97-105 is the shared code path with the initial scene-load placement, so this is the only user-facing way to recover from getting lost in free-look. The speed side-effect (camera.cpp:104) is the surprising part.
- **tests**: none

### 23. Hold right mouse button in the render window while "Free camera" is OFF

- **id**: `look-armed-but-inert-freecam-off` *(audit)*
- **input**: right-click
- **precondition**: g_enabled true (a scene is loaded) AND g_camera.active false - i.e. the scene has an authored camera (scene3d_host.cpp:61) and the user has not ticked "Free camera"
- **disabled when**: g_enabled false (no scene) - then HandleLookMessage returns false at scene3d_input.cpp:50 and the cursor is not even hidden
- **effect**: BeginLook still runs in full: SetCapture, cursor hidden with ShowCursor(FALSE), cursor warped back to the anchor on every WM_MOUSEMOVE, and PollCameraInput still reads all eight movement/modifier keys. But RenderFrame skips UpdateFreeCamera entirely (`if (g_camera.active)`, scene3d_host.cpp:93), so the accumulated look deltas and key flags are drained and thrown away and NOTHING moves. The user sees a hidden, frozen cursor and a dead camera.
- **source**: `src/scene3d/scene3d_host.cpp:93`
- **notes**: Genuine gap between the two gates: SetInputEnabled(true) arms the WndProc hook for every loaded scene (scene3d_host.cpp:69) regardless of g_camera.active, so the look gesture is armed on scenes where it does nothing. The panel tooltip at gui_scene3d_panel.cpp:115-121 is the only hint.
- **tests**: none

### 24. Right-click in the render window while crop pick mode is active

- **id**: `rmb-during-crop-pick-mode` *(audit)*
- **input**: right-click
- **precondition**: Crop pick mode active AND g_enabled true
- **disabled when**: g_enabled false
- **effect**: HandleCropPick has no WM_RBUTTONDOWN/WM_RBUTTONUP case and falls to `default: return false` (window.cpp:112-114), so the message reaches HandleLookMessage and BeginLook runs normally - free-look starts on top of crop picking, with the crop crosshair cursor (set by the crop handler's WM_SETCURSOR at window.cpp:77-83) hidden by ShowCursor(FALSE). Both modes are then live at once.
- **source**: `src/window.cpp:112`
- **notes**: The two capture owners are not coordinated: BeginLook's SetCapture and the crop handler's SetCapture target the same HWND, so no WM_CAPTURECHANGED is emitted on the second SetCapture and neither module knows the other is active.
- **tests**: none

### 25. Right mouse button press anywhere in the render window client area

- **id**: `rmb-press-begin-look`
- **input**: right-click
- **precondition**: Scene3d input enabled (Scene3d::SetInputEnabled(true) runs when a 3D scene loads successfully, src/scene3d/scene3d_host.cpp:69); crop pick mode not active (window.cpp:119-121 gives HandleCropPick first refusal, though it does not consume WM_RBUTTONDOWN)
- **disabled when**: g_enabled is false (no scene loaded, or scene was unloaded) - HandleLookMessage returns false at src/scene3d/scene3d_input.cpp:50 and the message falls through to normal window handling
- **effect**: BeginLook(hwnd): sets g_looking = true, calls Win32 SetCapture(hwnd) so the window keeps mouse capture, records the current cursor position with GetCursorPos into g_anchor, and calls ShowCursor(FALSE) to hide the cursor. HandleLookMessage returns true so WndProc returns 0 and the message never reaches DefWindowProcA (no context menu).
- **source**: `src/scene3d/scene3d_input.cpp:53`
- **notes**: This is the master gesture: holding RMB is what arms both mouse-look and all WASDQE movement keys.
- **tests**: none

### 26. Right mouse button release

- **id**: `rmb-release-end-look`
- **input**: right-click
- **precondition**: g_enabled is true; a look session may or may not be in progress
- **disabled when**: g_enabled is false - message falls through untouched
- **effect**: EndLook(): if g_looking, clears g_looking, calls ReleaseCapture() and ShowCursor(TRUE) restoring the cursor. Returns true so WndProc returns 0. If g_looking was already false the function early-returns and only the message swallow happens.
- **source**: `src/scene3d/scene3d_input.cpp:56`
- **notes**: EndLook is idempotent (guard at scene3d_input.cpp:26).
- **tests**: none

### 27. Mouse wheel over the render window

- **id**: `mouse-wheel-unbound` *(audit)*
- **input**: scroll
- **precondition**: Render window focused/hovered; any scene state
- **disabled when**: n/a (never bound)
- **effect**: No handler: HandleLookMessage's switch has no WM_MOUSEWHEEL case (default: return false, scene3d_input.cpp:74-75) and WndProc's switch falls through to DefWindowProcA (window.cpp:141-144). The wheel does not change camera speed or dolly the camera - the panel help text says so explicitly: "Mouse wheel is free for the panel; use 'move speed' above".
- **source**: `src/gui/gui_scene3d_panel.cpp:134`
- **notes**: Recorded because wheel-to-adjust-speed is the near-universal convention in free-look viewers, and this build deliberately does NOT implement it; the documented substitute is the "move speed" drag.
- **tests**: none

### 28. Anything that takes mouse capture away from the render window (alt-tab, another window grabbing capture, a system modal)

- **id**: `capture-lost`
- **input**: window-message
- **precondition**: g_enabled true and g_looking true
- **disabled when**: g_enabled false - the handler returns false before the switch and the camera module ignores capture loss entirely
- **effect**: WM_CAPTURECHANGED clears g_looking and calls ShowCursor(TRUE) to restore the cursor, but deliberately does NOT call ReleaseCapture (capture is already gone). HandleLookMessage returns false, so the message continues to WndProc's own WM_CAPTURECHANGED case which clears g_crop_drag_active (src/window.cpp:136-140).
- **source**: `src/scene3d/scene3d_input.cpp:59`
- **notes**: Only look state is cleaned up; the accumulated g_dx/g_dy are not discarded and still get applied on the next PollCameraInput.

### 29. Close the render window (title bar X / Alt+F4)

- **id**: `window-close-destroy`
- **input**: window-message
- **precondition**: always
- **effect**: WM_DESTROY sets g_running = false and calls PostQuitMessage(0), ending the message loop. The look handler ignores WM_DESTROY (default case returns false).
- **source**: `src/window.cpp:126`
- **notes**: Free-look leaves the OS cursor hidden if the window is destroyed mid-look, since ShowCursor(TRUE) is only paired in EndLook/WM_CAPTURECHANGED.
- **audit correction**: The notes over-claim a bug: "Free-look leaves the OS cursor hidden if the window is destroyed mid-look, since ShowCursor(TRUE) is only paired in EndLook/WM_CAPTURECHANGED." DestroyWindow releases the mouse capture held by BeginLook (scene3d_input.cpp:20) and Windows delivers WM_CAPTURECHANGED to that same WndProc before destruction completes; HandleLookMessage is still armed (g_enabled is only cleared by Unload) and its WM_CAPTURECHANGED case at scene3d_input.cpp:59-63 calls ShowCursor(TRUE). Additionally ShowCursor's display counter is per-process and dies with the process. -> Notes should read: the capture released by DestroyWindow generates WM_CAPTURECHANGED, which HandleLookMessage (scene3d_input.cpp:59-63) uses to clear g_looking and restore the cursor, so the cursor is not left hidden; nothing in the destroy path needs an explicit EndLook.

