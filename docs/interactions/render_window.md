# Render window

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `client-double-click-no-dblclks` *(audit)* | Double-click inside the render window client area | double-click | - | **none** |
| 2 | `crop-pick-mousemove-drag` | Render window client area (crop pick mode) - drag to size the crop rectangle | drag | - | **none** |
| 3 | `nonclient-drag-during-crop-pick` *(audit)* | Dragging the title bar / resizing from the frame while crop pick mode is armed | drag | - | **none** |
| 4 | `scene3d-look-mouse-keyboard` | 3D camera look / fly controls in the render window (mouse move, buttons and keys consumed by the scene3d input layer) | drag | - | **none** |
| 5 | `window-resize-drag` | Resizing the window by dragging its border/corner (WS_OVERLAPPEDWINDOW sizing frame) | drag | - | **none** |
| 6 | `crop-pick-setcursor-crosshair` | Moving the pointer over the client area during crop pick mode (cursor feedback) | hover | - | **none** |
| 7 | `crop-pick-escape-cancel` | Escape key while crop pick mode is active | key | - | **none** |
| 8 | `global-escape-quit` | Escape key in the render window (normal mode) | key | - | **none** |
| 9 | `syskey-system-menu-altf4` *(audit)* | Alt / F10 / Alt+Space / Alt+F4 pressed while the render window has focus (system keys) | key | - | **none** |
| 10 | `appwindow-requestclose-api` *(audit)* | Programmatic quit entry point AppWindow::RequestClose() | left-click | - | **none** |
| 11 | `appwindow-resize-api` *(audit)* | Programmatic window resize entry point AppWindow::Resize(hwnd, w, h) | left-click | - | **none** |
| 12 | `crop-pick-lbuttondown-start-drag` | Render window client area (crop pick mode) - left mouse button press | left-click | - | **none** |
| 13 | `crop-pick-lbuttonup-commit` | Render window client area (crop pick mode) - release left button to commit the crop | left-click | - | **none** |
| 14 | `crop-pick-mode-cleared-mid-drag` *(audit)* | Turning crop pick mode off from the GUI (export panel) while the left button is still held down mid-drag | left-click | - | **none** |
| 15 | `render-rt-size-rebind` *(audit)* | Render resolution change feeding AppWindow::SetRenderRtSize(w, h) - the coordinate space every crop drag is reported in | left-click | - | **none** |
| 16 | `crop-pick-other-mouse-buttons` | Right-click / middle-click / mouse wheel inside the client area while crop pick mode is active | right-click | - | **none** |
| 17 | `rbutton-look-aborts-crop-drag` *(audit)* | Right mouse button press/release inside the client area while a crop drag is in progress | right-click | - | **none** |
| 18 | `crop-drag-capture-lost` | Losing mouse capture during a crop drag (Alt+Tab away, another window grabs capture, system UI steals it) | window-message | - | n/a |
| 19 | `window-close-destroy` | Window close (title-bar X, Alt+F4, system menu Close, task-manager close) -> WM_DESTROY | window-message | - | n/a |
| 20 | `window-minimise-maximise-move` | Minimise / maximise / restore / move via title bar, system menu or double-click on the caption | window-message | - | n/a |
| 21 | `window-paint-invalidate` | Uncovering / dragging another window over the render window (repaint request) | window-message | - | n/a |
| 22 | `wm-quit-pump-exit` | Any action that posts WM_QUIT (window destroyed, PostQuitMessage) observed by the message pump | window-message | - | n/a |

## Detail

### 1. Double-click inside the render window client area

- **id**: `client-double-click-no-dblclks` *(audit)*
- **input**: double-click
- **precondition**: Window class 'AFPRendererClass' is registered with style CS_HREDRAW \| CS_VREDRAW only - CS_DBLCLKS is absent (src/window.cpp:151), so Windows never synthesises WM_LBUTTONDBLCLK / WM_RBUTTONDBLCLK for this window.
- **disabled when**: n/a - the double-click message class is unconditionally unavailable
- **effect**: A double-click is delivered as two independent WM_LBUTTONDOWN/WM_LBUTTONUP pairs. In crop pick mode the FIRST pair already exits pick mode (state.SetCropPickMode(false), src/window.cpp:106), so the second down/up pair is no longer routed to HandleCropPick (gate at src/window.cpp:119) and instead falls through to Scene3d / DefWindowProcA. Outside crop pick mode there is no double-click behaviour at all (no maximise-on-double-click in the client area, no reset gesture).
- **source**: `src/window.cpp:151`
- **notes**: Consequence worth recording: a stationary single click in pick mode publishes a 0x0 rect and then src/window.cpp:108 resets the crop to a default-constructed CropRect, so 'click to cancel' silently CLEARS an existing crop.
- **tests**: none

### 2. Render window client area (crop pick mode) - drag to size the crop rectangle

- **id**: `crop-pick-mousemove-drag`
- **input**: drag
- **precondition**: g_crop_drag_active == true (a WM_LBUTTONDOWN happened in crop pick mode) AND crop pick mode still on
- **disabled when**: g_crop_drag_active == false - WM_MOUSEMOVE returns false and falls through to Scene3d::HandleLookMessage / DefWindowProcA
- **effect**: On every WM_MOUSEMOVE, PublishDragRect(hwnd, current) recomputes the normalised min/max rect between anchor and current point and calls App::Global().SetCropRect(r) with x/y/w/h in render-target pixels. Message consumed.
- **source**: `src/window.cpp:93`
- **notes**: Because the window has mouse capture, dragging outside the client area still updates; ClientToRt clamps the point to [0,cw] x [0,ch] (src/window.cpp:36-37), so the rect saturates at the window edges.
- **tests**: none

### 3. Dragging the title bar / resizing from the frame while crop pick mode is armed

- **id**: `nonclient-drag-during-crop-pick` *(audit)*
- **input**: drag
- **precondition**: App::Global().GetCropPickMode() == true; press starts on a non-client hit-test area (caption, border, corner)
- **disabled when**: n/a - non-client messages are never intercepted
- **effect**: Non-client messages (WM_NCHITTEST, WM_NCLBUTTONDOWN, WM_NCMOUSEMOVE) hit HandleCropPick's default branch (src/window.cpp:112) and Scene3d's default branch, then DefWindowProcA runs the modal move/size loop normally. The crosshair cursor is NOT applied because the WM_SETCURSOR case only swaps the cursor when LOWORD(lParam) == HTCLIENT (src/window.cpp:78); the frame keeps the normal arrow/resize cursors. Crop pick mode stays armed across the move/resize, and because ClientToRt re-reads GetClientRect on every message (src/window.cpp:33), a resize performed while armed rescales all subsequent crop coordinates.
- **source**: `src/window.cpp:78`
- **notes**: The modal size/move loop internally takes mouse capture, so a crop drag begun before the frame drag would be cancelled through the WM_CAPTURECHANGED handler at src/window.cpp:136.
- **tests**: none

### 4. 3D camera look / fly controls in the render window (mouse move, buttons and keys consumed by the scene3d input layer)

- **id**: `scene3d-look-mouse-keyboard`
- **input**: drag
- **precondition**: Scene3d::HandleLookMessage(hwnd, msg, wParam, lParam) returns true for the message; reached only after HandleCropPick declines it, so crop pick mode takes priority over camera look
- **disabled when**: crop pick mode is active and HandleCropPick already consumed the message (mouse down/move/up, Escape, WM_SETCURSOR)
- **effect**: WndProc forwards every unclaimed message to Scene3d::HandleLookMessage; when it returns true the message is consumed (WndProc returns 0) and the scene3d input module has applied its camera/look state change.
- **source**: `src/window.cpp:123`
- **notes**: Single entry covering the whole delegated input set; the specific gestures and keys live in src/scene3d/scene3d_input.* which was not part of this file.
- **tests**: none
- **audit correction**: Three errors. (1) The control/effect say keys are 'consumed by the scene3d input layer' via WndProc - they are not; HandleLookMessage handles only WM_RBUTTONDOWN (BeginLook: SetCapture + ShowCursor(FALSE)), WM_RBUTTONUP (EndLook: ReleaseCapture + ShowCursor(TRUE)), WM_MOUSEMOVE while looking (delta accumulate + SetCursorPos re-anchor), and WM_CAPTURECHANGED which returns FALSE (src/scene3d/scene3d_input.cpp:52-76). WASD/E/Q/Space/Ctrl/Shift/Alt are polled with GetAsyncKeyState in Scene3d::PollCameraInput (src/scene3d/scene3d_input.cpp:87-94), outside the message stream entirely, and only while g_looking. (2) The master gate is missing: HandleLookMessage returns false immediately unless Scene3d::SetInputEnabled(true) has set g_enabled (src/scene3d/scene3d_input.cpp:50). (3) 'WndProc forwards every unclaimed message' overstates it - only those four message types can ever be claimed. -> control: '3D camera look in the render window - right-button hold to look, mouse move to aim'. input: 'right-click'. precondition: 'Scene3d input enabled (g_enabled via SetInputEnabled, src/scene3d/scene3d_input.cpp:50) AND HandleCropPick declined the message first (src/window.cpp:119-122)'. disabled_when: 'Scene3d input disabled, or crop pick mode already consumed the message (left button down/move/up, Escape, WM_SETCURSOR over HTCLIENT)'. effect: 'right-button down grabs capture and hides the cursor; mouse moves while looking accumulate look deltas and re-centre the pointer; right-button up releases capture and reshows the cursor. Keyboard fly (W/A/S/D/E/Q/Space/Ctrl/Shift/Alt) is polled separately via GetAsyncKeyState and never passes through WndProc.'

### 5. Resizing the window by dragging its border/corner (WS_OVERLAPPEDWINDOW sizing frame)

- **id**: `window-resize-drag`
- **input**: drag
- **precondition**: always - the window is created with WS_OVERLAPPEDWINDOW, which includes WS_THICKFRAME
- **effect**: No WM_SIZE/WM_SIZING handler exists; the messages fall through to DefWindowProcA. The class style CS_HREDRAW \| CS_VREDRAW forces a full client invalidate on any width/height change. The changed client size is picked up by ClientToRt on the next crop-pick mouse message, so crop coordinates always rescale to the current client rect against g_rt_w/g_rt_h.
- **source**: `src/window.cpp:151`
- **notes**: Creation logs a warning when the achieved client area does not match the requested size because Present will then scale (src/window.cpp:175-179).
- **tests**: none
- **audit correction**: Wrong source line for the stated precondition. The entry's precondition rests on WS_OVERLAPPEDWINDOW / WS_THICKFRAME, but src/window.cpp:151 is 'wc.style = CS_HREDRAW \| CS_VREDRAW' (the class style, which only backs the invalidate half of the effect). The sizing frame comes from the WS_OVERLAPPEDWINDOW argument to CreateWindowExA and the matching AdjustWindowRect. -> source: 'src/window.cpp:164' (CreateWindowExA with WS_OVERLAPPEDWINDOW; AdjustWindowRect at src/window.cpp:159), keeping src/window.cpp:151 as a secondary reference for the CS_HREDRAW \| CS_VREDRAW invalidate behaviour.

### 6. Moving the pointer over the client area during crop pick mode (cursor feedback)

- **id**: `crop-pick-setcursor-crosshair`
- **input**: hover
- **precondition**: App::Global().GetCropPickMode() == true AND LOWORD(lParam) == HTCLIENT (pointer over client area, not the frame/caption)
- **disabled when**: pointer is over a non-client hit-test area (border, caption, buttons) - returns false so the default arrow/resize cursors apply; also disabled when crop pick mode is off
- **effect**: SetCursor(LoadCursor(nullptr, IDC_CROSS)) swaps the pointer to a crosshair and the handler returns TRUE from WM_SETCURSOR to suppress the class cursor (wc.hCursor = IDC_ARROW).
- **source**: `src/window.cpp:77`
- **notes**: This is the only cursor-shape change in the file; the registered class cursor is IDC_ARROW (src/window.cpp:154).
- **tests**: none

### 7. Escape key while crop pick mode is active

- **id**: `crop-pick-escape-cancel`
- **input**: key
- **precondition**: App::Global().GetCropPickMode() == true; window has keyboard focus
- **disabled when**: crop pick mode is off - Escape instead hits the global Escape handler and quits the app
- **effect**: If a drag is in progress, ReleaseCapture() and g_crop_drag_active = false; then state.SetCropPickMode(false) leaves pick mode. The crop rect keeps whatever value the in-progress drag last published (it is NOT rolled back). Message consumed, so the app does NOT quit.
- **source**: `src/window.cpp:69`
- **notes**: Any non-VK_ESCAPE WM_KEYDOWN returns false immediately (src/window.cpp:70) and falls through to the normal handlers.
- **tests**: none

### 8. Escape key in the render window (normal mode)

- **id**: `global-escape-quit`
- **input**: key
- **precondition**: App::Global().GetCropPickMode() == false AND Scene3d::HandleLookMessage did not consume the WM_KEYDOWN; window focused
- **disabled when**: crop pick mode is active (Escape is intercepted to cancel the pick) or scene3d input consumes the key
- **effect**: g_running = false and DestroyWindow(hwnd) - tears the window down, which produces WM_DESTROY, ends the main loop via AppWindow::IsRunning()/PumpMessages() and quits the app. Returns 0.
- **source**: `src/window.cpp:130`
- **notes**: All other WM_KEYDOWN keys are swallowed here too (the case returns 0 unconditionally, src/window.cpp:135), so unhandled keys never reach DefWindowProcA - no system menu key handling, no WM_SYSCOMMAND from keyboard accelerators in this branch.
- **tests**: none
- **audit correction**: The precondition ('Scene3d::HandleLookMessage did not consume the WM_KEYDOWN') and disabled_when ('or scene3d input consumes the key') are hallucinated. Scene3d::HandleLookMessage has no key case whatsoever: its switch handles only WM_RBUTTONDOWN, WM_RBUTTONUP, WM_CAPTURECHANGED and WM_MOUSEMOVE, with default: return false (src/scene3d/scene3d_input.cpp:52-76). It can never swallow a WM_KEYDOWN. -> precondition: 'App::Global().GetCropPickMode() == false; window focused.' disabled_when: 'crop pick mode is active - Escape is intercepted by HandleCropPick (src/window.cpp:69-76) to cancel the pick instead.' Nothing else can intercept the key.
- **audit correction**: The effect claims the loop ends 'via AppWindow::IsRunning()/PumpMessages()'. AppWindow::IsRunning() (src/window.cpp:211) has no caller outside window.cpp - grep -rn 'AppWindow::IsRunning' matches only the definition and src/window.h:13. The host loop is 'while (AppWindow::PumpMessages())' at src/render_loop.cpp:366. -> effect: '... DestroyWindow(hwnd) produces WM_DESTROY, whose handler posts WM_QUIT; the next AppWindow::PumpMessages() sees WM_QUIT, sets g_running = false and returns false (src/window.cpp:201-204), ending the loop at src/render_loop.cpp:366. IsRunning() is not part of the shutdown path.'
- **audit correction**: The notes claim 'unhandled keys never reach DefWindowProcA - no system menu key handling, no WM_SYSCOMMAND from keyboard accelerators in this branch'. Only WM_KEYDOWN is swallowed by the case at src/window.cpp:130-135. WM_SYSKEYDOWN / WM_SYSCHAR / WM_SYSKEYUP are not matched by any case and do reach DefWindowProcA at src/window.cpp:144, which produces the standard SC_KEYMENU / SC_CLOSE behaviour (Alt, F10, Alt+Space, Alt+F4 all work). -> notes: 'All other non-system WM_KEYDOWN keys are swallowed (the case returns 0 unconditionally, src/window.cpp:135) and never reach DefWindowProcA. System keys are unaffected: WM_SYSKEYDOWN/WM_SYSCHAR are unmatched and fall through to DefWindowProcA, so Alt/F10/Alt+Space open the system menu and Alt+F4 still closes the app.' See the new syskey-system-menu-altf4 record.

### 9. Alt / F10 / Alt+Space / Alt+F4 pressed while the render window has focus (system keys)

- **id**: `syskey-system-menu-altf4` *(audit)*
- **input**: key
- **precondition**: Window focused. WM_SYSKEYDOWN / WM_SYSCHAR / WM_SYSKEYUP are generated instead of WM_KEYDOWN, so HandleCropPick's switch (src/window.cpp:68-114) does not match them (default: return false) even while crop pick mode is armed, and Scene3d::HandleLookMessage has no key case at all.
- **disabled when**: never - there is no branch anywhere in WndProc that swallows WM_SYSKEYDOWN
- **effect**: The message reaches DefWindowProcA (src/window.cpp:144), which gives full stock behaviour: Alt / F10 activate the menu loop and post WM_SYSCOMMAND SC_KEYMENU, Alt+Space opens the system menu, Alt+F4 posts SC_CLOSE -> WM_CLOSE -> DestroyWindow -> the WM_DESTROY case at src/window.cpp:126. Alt+F4 therefore quits the app even in the middle of a crop drag, with no rollback of the last published CropRect.
- **source**: `src/window.cpp:141`
- **notes**: Directly contradicts the 'no system menu key handling, no WM_SYSCOMMAND from keyboard accelerators' claim in the notes of global-escape-quit; only the non-system WM_KEYDOWN stream is swallowed by src/window.cpp:130-135.
- **tests**: none

### 10. Programmatic quit entry point AppWindow::RequestClose()

- **id**: `appwindow-requestclose-api` *(audit)*
- **input**: left-click
- **precondition**: none
- **effect**: Sets g_running = false ONLY (src/window.cpp:214-216). It does not DestroyWindow and does not PostQuitMessage, so the HWND and the D3D device stay alive; the shutdown happens on the next AppWindow::PumpMessages() call, which drains the queue and then returns g_running == false at src/window.cpp:208, ending the host loop at src/render_loop.cpp:366. This is a distinctly softer shutdown than the Escape path (src/window.cpp:130-134), which destroys the window first.
- **source**: `src/window.cpp:214`
- **notes**: No caller found. Searched: grep -rn 'RequestClose' across src/, tests/ and tools/ - only src/window.cpp:214 and src/window.h:14 match (all other hits are in vendor/ and build/). Reachability UNRESOLVED, not proven dead.
- **tests**: none

### 11. Programmatic window resize entry point AppWindow::Resize(hwnd, w, h)

- **id**: `appwindow-resize-api` *(audit)*
- **input**: left-click
- **precondition**: hwnd != nullptr (early return at src/window.cpp:191 otherwise). Intended to be driven by a resolution change in the UI.
- **disabled when**: hwnd == nullptr - the function returns immediately and nothing happens
- **effect**: AdjustWindowRect inflates the requested client size by the WS_OVERLAPPEDWINDOW frame and SetWindowPos applies it with SWP_NOMOVE \| SWP_NOZORDER (src/window.cpp:192-195), so the window keeps its screen position and z-order and only the client area changes. It does NOT touch g_rt_w/g_rt_h, so crop coordinates keep mapping to the old render-target size until AppWindow::SetRenderRtSize is called separately.
- **source**: `src/window.cpp:190`
- **notes**: No caller found. Searched: grep -rn 'AppWindow::Resize' over the whole repo - only the definition (src/window.cpp:190) and the declaration (src/window.h:9) match. Recording as UNRESOLVED reachability rather than dead, per the prove-the-negative rule.
- **tests**: none

### 12. Render window client area (crop pick mode) - left mouse button press

- **id**: `crop-pick-lbuttondown-start-drag`
- **input**: left-click
- **precondition**: App::Global().GetCropPickMode() == true (crop pick mode armed from the GUI); pointer over the render window client area
- **disabled when**: crop pick mode is off - the message falls through to Scene3d::HandleLookMessage / DefWindowProcA instead
- **effect**: SetCapture(hwnd) grabs the mouse; g_crop_drag_active = true; g_crop_drag_anchor is set from GET_X_LPARAM/GET_Y_LPARAM(lParam); PublishDragRect() immediately calls App::Global().SetCropRect() with a degenerate (w=0,h=0) rect at the anchor, in render-target pixel space. Message is consumed (returns 0).
- **source**: `src/window.cpp:84`
- **notes**: Anchor is stored in client pixels; conversion to RT space happens in ClientToRt using g_rt_w/g_rt_h, which are set by AppWindow::SetRenderRtSize (src/window.cpp:218).
- **tests**: none

### 13. Render window client area (crop pick mode) - release left button to commit the crop

- **id**: `crop-pick-lbuttonup-commit`
- **input**: left-click
- **precondition**: g_crop_drag_active == true
- **disabled when**: g_crop_drag_active == false - returns false and the message falls through
- **effect**: Final PublishDragRect() -> App::Global().SetCropRect(); ReleaseCapture(); g_crop_drag_active = false; state.SetCropPickMode(false) exits pick mode; then re-reads state.GetCropRect() and, if r.w <= 0 \|\| r.h <= 0, calls state.SetCropRect({}) to clear the crop entirely. Message consumed.
- **source**: `src/window.cpp:100`
- **notes**: A simple click without movement therefore selects nothing and RESETS the crop rect to the default-constructed (empty) CropRect.
- **tests**: none

### 14. Turning crop pick mode off from the GUI (export panel) while the left button is still held down mid-drag

- **id**: `crop-pick-mode-cleared-mid-drag` *(audit)*
- **input**: left-click
- **precondition**: g_crop_drag_active == true (WM_LBUTTONDOWN already happened) AND another thread/frame calls App::State::SetCropPickMode(false) - the export panel does this at src/gui/gui_export_panel.cpp:382 and :401
- **disabled when**: n/a
- **effect**: The gate at src/window.cpp:119 stops routing messages to HandleCropPick, so the pending WM_LBUTTONUP is never seen by the crop handler: ReleaseCapture() is never called and g_crop_drag_active stays true. The window keeps the mouse capture, and the flag is only cleared later when something else produces WM_CAPTURECHANGED (src/window.cpp:136). If pick mode is re-armed before that, the very next WM_MOUSEMOVE resumes publishing crop rects against the STALE anchor without any button press.
- **source**: `src/window.cpp:119`
- **notes**: The pick-mode gate is checked per message, and neither the WM_LBUTTONUP path (src/window.cpp:100) nor SetCropPickMode has any teardown for an in-flight drag; only the Escape path (src/window.cpp:71-74) releases capture explicitly.
- **tests**: none

### 15. Render resolution change feeding AppWindow::SetRenderRtSize(w, h) - the coordinate space every crop drag is reported in

- **id**: `render-rt-size-rebind` *(audit)*
- **input**: left-click
- **precondition**: w > 0 AND h > 0 (guard at src/window.cpp:219); called once during boot with the D3D9 offscreen size at src/boot.cpp:71 after g_d3d.GetOffscreenSize()
- **disabled when**: w <= 0 or h <= 0 - the call is silently ignored and the previous values are kept
- **effect**: Sets g_rt_w / g_rt_h, which ClientToRt uses to scale client pixels into render-target pixels (rt_x = px * g_rt_w / cw, src/window.cpp:38-39). Every crop rect the user drags is published in this space, so this call decides whether the exported crop matches the actual render target. If it never runs, the defaults 1920x1080 (src/window.cpp:17 and :20) are used and crop rects are wrong for any other resolution (e.g. SDVX 1080x1920 or IIDX 640x480).
- **source**: `src/window.cpp:218`
- **notes**: The existing inventory only mentions this in a note on crop-pick-lbuttondown-start-drag; it deserves its own record because the w>0/h>0 guard is a real gate and the fallback silently mis-scales user drags.
- **tests**: none

### 16. Right-click / middle-click / mouse wheel inside the client area while crop pick mode is active

- **id**: `crop-pick-other-mouse-buttons`
- **input**: right-click
- **precondition**: App::Global().GetCropPickMode() == true
- **disabled when**: never (but the action has no crop effect)
- **effect**: HandleCropPick's default: branch returns false, so the message is offered to Scene3d::HandleLookMessage and then DefWindowProcA. Crop pick mode stays active and the crop rect is unchanged - only the LEFT button drives crop selection.
- **source**: `src/window.cpp:112`
- **notes**: Recorded as an explicit non-gate: crop pick mode does not swallow non-left-button input.
- **tests**: none
- **audit correction**: Effect claims right/middle/wheel input during crop pick mode leaves everything untouched: 'Crop pick mode stays active and the crop rect is unchanged - only the LEFT button drives crop selection.' That is wrong for the right button when Scene3d input is enabled: WM_RBUTTONDOWN starts camera look (SetCapture + ShowCursor(FALSE), src/scene3d/scene3d_input.cpp:18-23) so the crosshair vanishes, and the matching WM_RBUTTONUP calls ReleaseCapture (src/scene3d/scene3d_input.cpp:27), which fires WM_CAPTURECHANGED and clears g_crop_drag_active at src/window.cpp:136 - aborting an in-progress crop drag. -> effect: 'HandleCropPick's default branch returns false, so the message is offered to Scene3d::HandleLookMessage and then DefWindowProcA. Middle button and wheel are handled by neither and are inert. The RIGHT button is not inert when Scene3d input is enabled: it starts/stops camera look, hides and reshows the cursor, and its ReleaseCapture on button-up aborts any in-progress crop drag through the WM_CAPTURECHANGED handler. Crop pick mode itself stays armed and the published rect is not rewritten.' See the new rbutton-look-aborts-crop-drag record.

### 17. Right mouse button press/release inside the client area while a crop drag is in progress

- **id**: `rbutton-look-aborts-crop-drag` *(audit)*
- **input**: right-click
- **precondition**: g_crop_drag_active == true, crop pick mode on, AND Scene3d input enabled (Scene3d::SetInputEnabled(true) set g_enabled, src/scene3d/scene3d_input.cpp:38/50)
- **disabled when**: Scene3d input disabled - HandleLookMessage returns false at src/scene3d/scene3d_input.cpp:50 and the right button then does nothing at all
- **effect**: HandleCropPick declines WM_RBUTTONDOWN (default branch, src/window.cpp:112) so Scene3d::BeginLook runs: SetCapture(hwnd) again plus ShowCursor(FALSE) - the crosshair disappears mid-drag while the crop drag continues (crop still wins WM_MOUSEMOVE because HandleCropPick runs first, src/window.cpp:121, so the camera receives no look delta). On WM_RBUTTONUP, Scene3d::EndLook calls ReleaseCapture() (src/scene3d/scene3d_input.cpp:27), which fires WM_CAPTURECHANGED; that reaches src/window.cpp:136 and sets g_crop_drag_active = false, silently ABORTING the crop drag while the left button is still held. Further mouse movement no longer updates the crop, and the subsequent WM_LBUTTONUP takes the g_crop_drag_active == false path (src/window.cpp:111), so pick mode is never exited and the degenerate-rect cleanup at src/window.cpp:108 never runs.
- **source**: `src/window.cpp:112`
- **notes**: Cross-file interaction that only exists because the two capture owners (crop drag and camera look) share one HWND and one capture. Traced through Scene3d::HandleLookMessage src/scene3d/scene3d_input.cpp:53-64.
- **tests**: none

### 18. Losing mouse capture during a crop drag (Alt+Tab away, another window grabs capture, system UI steals it)

- **id**: `crop-drag-capture-lost`
- **input**: window-message
- **precondition**: g_crop_drag_active == true and WM_CAPTURECHANGED reaches WndProc (i.e. crop pick mode already returned false / is off for this message)
- **disabled when**: g_crop_drag_active == false - the handler does nothing and still returns 0
- **effect**: g_crop_drag_active = false, aborting the drag so subsequent WM_MOUSEMOVE no longer publishes crop rects. Crop pick mode is NOT cleared and the last published CropRect stays in App::State. Returns 0.
- **source**: `src/window.cpp:136`
- **notes**: Note the ordering hazard: HandleCropPick has no WM_CAPTURECHANGED case (default: return false, src/window.cpp:112), so the message correctly reaches this handler even while pick mode is on.
- **audit correction**: The precondition is inaccurate: 'WM_CAPTURECHANGED reaches WndProc (i.e. crop pick mode already returned false / is off for this message)'. Pick mode being off is not why the message arrives. It arrives because HandleCropPick's switch has no WM_CAPTURECHANGED case (default: return false, src/window.cpp:112) AND Scene3d's WM_CAPTURECHANGED case deliberately returns false after its own cleanup (src/scene3d/scene3d_input.cpp:59-64). The precondition also omits the Scene3d gate at src/window.cpp:123 entirely. -> precondition: 'g_crop_drag_active == true and WM_CAPTURECHANGED is delivered. It always reaches this handler regardless of crop pick mode, because HandleCropPick has no case for it (src/window.cpp:112) and Scene3d::HandleLookMessage returns false for it after clearing its own look state (src/scene3d/scene3d_input.cpp:59-64).'

### 19. Window close (title-bar X, Alt+F4, system menu Close, task-manager close) -> WM_DESTROY

- **id**: `window-close-destroy`
- **input**: window-message
- **precondition**: always (window exists)
- **effect**: g_running = false and PostQuitMessage(0); AppWindow::IsRunning() then reports false and PumpMessages() returns false on the resulting WM_QUIT, ending the application main loop. Returns 0.
- **source**: `src/window.cpp:126`
- **notes**: There is no WM_CLOSE handler, so DefWindowProcA turns the close request into DestroyWindow -> WM_DESTROY. No save/confirm prompt.
- **audit correction**: Effect states 'AppWindow::IsRunning() then reports false and PumpMessages() returns false on the resulting WM_QUIT, ending the application main loop.' IsRunning() is never called by the host loop (no callers outside src/window.cpp; the loop is src/render_loop.cpp:366 'while (AppWindow::PumpMessages())'), so naming it as the mechanism is wrong. -> effect: 'g_running = false and PostQuitMessage(0) (src/window.cpp:127-128). The next AppWindow::PumpMessages() dequeues WM_QUIT, sets g_running = false and returns false without dispatching (src/window.cpp:201-204), ending the main loop at src/render_loop.cpp:366.'

### 20. Minimise / maximise / restore / move via title bar, system menu or double-click on the caption

- **id**: `window-minimise-maximise-move`
- **input**: window-message
- **precondition**: always
- **effect**: Not handled by WndProc - WM_SYSCOMMAND / WM_SIZE / WM_MOVE reach DefWindowProcA and get standard behaviour. No app state is updated and the render loop keeps running (there is no minimise pause).
- **source**: `src/window.cpp:141`
- **notes**: Listed because it is a distinct user action on this surface whose handling is the explicit default: branch (src/window.cpp:141-143).

### 21. Uncovering / dragging another window over the render window (repaint request)

- **id**: `window-paint-invalidate`
- **input**: window-message
- **precondition**: always
- **effect**: No WM_PAINT or WM_ERASEBKGND handler; DefWindowProcA validates the update region and the frame content is refreshed only by the host's own per-frame D3D9 Present, not by the paint message.
- **source**: `src/window.cpp:144`
- **notes**: Included for completeness of the Win32 message inventory - paint is deliberately unhandled.

### 22. Any action that posts WM_QUIT (window destroyed, PostQuitMessage) observed by the message pump

- **id**: `wm-quit-pump-exit`
- **input**: window-message
- **precondition**: AppWindow::PumpMessages() is being called each frame by the host loop
- **effect**: On msg.message == WM_QUIT the pump sets g_running = false and returns false immediately without dispatching, which stops the host render loop.
- **source**: `src/window.cpp:201`
- **notes**: All other messages are TranslateMessage'd (so WM_KEYDOWN can generate WM_CHAR for any text consumer) and DispatchMessageA'd to WndProc (src/window.cpp:205-206).

