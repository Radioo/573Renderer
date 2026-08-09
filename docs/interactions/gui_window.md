# GUI window and thread

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `cli-headless` | --headless command line flag | cli-arg | - | n/a |
| 2 | `cli-no-gui` | --no-gui command line flag | cli-arg | - | n/a |
| 3 | `device-creation-fallback` | Launching the app on a machine without a usable D3D9 HAL device | cli-arg | - | n/a |
| 4 | `gui-init-failure-no-window` | Launching the app when window creation or device creation fails | cli-arg | - | n/a |
| 5 | `gui-thread-start-idempotent` | Starting the GUI a second time | cli-arg | - | n/a |
| 6 | `launch-shows-control-window` *(audit)* | Launching the app with the GUI enabled (default: no --no-gui / --headless) | cli-arg | - | n/a |
| 7 | `dpi-unaware-scaling` *(audit)* | Running the control window on a high-DPI display, or dragging it to a monitor with a different scale factor | drag | - | **none** |
| 8 | `imgui-layout-not-persisted` | Dragging / resizing ImGui sub-windows and headers inside the control window | drag | - | **none** |
| 9 | `move-window-titlebar` | Title bar drag to move the window | drag | - | **none** |
| 10 | `render-reentrancy-guard` | Resizing or repainting the window while a frame is already being drawn | drag | - | **none** |
| 11 | `resize-drag-border` | Window edge / corner resize grip | drag | - | **none** |
| 12 | `resize-min-size-clamp` | Attempt to shrink the window below the minimum size | drag | - | **none** |
| 13 | `resize-while-device-lost` | Resizing the window while the D3D9 device is lost | drag | - | **none** |
| 14 | `titlebar-drag-blocks-gui-thread-exit` *(audit)* | Holding the title bar or a resize border (Win32 modal move/size loop) | drag | - | **none** |
| 15 | `warp-resize-changes-present-params` *(audit)* | Resizing the control window while running on the WARP fallback device | drag | - | **none** |
| 16 | `mouse-cursor-shape` | Hovering the control window | hover | - | **none** |
| 17 | `alt-f10-system-menu-suppressed` | Alt key or F10 tap (keyboard system-menu activation) | key | - | **none** |
| 18 | `keyboard-nav-enabled` | Tab / arrow keys / Space / Enter keyboard navigation between widgets | key | - | **none** |
| 19 | `control-window-not-reopenable` *(audit)* | Trying to get the control window back after closing it | left-click | - | **none** |
| 20 | `modal-folder-dialog-parenting` | Opening a native "browse for folder" dialog from any panel | left-click | - | **none** |
| 21 | `per-frame-panel-build` | Any widget activation inside the control window | left-click | - | **none** |
| 22 | `system-menu-titlebar-rightclick` *(audit)* | Right-click the title bar / click the window icon to open the system menu (Restore, Move, Size, Minimize, Maximize, Close) | right-click | - | **none** |
| 23 | `mouse-wheel-scroll` | Mouse wheel over any scrollable ImGui child region in the control window | scroll | - | **none** |
| 24 | `app-exit-stops-gui-thread` | Quitting the app (Exit command from the GUI, or closing the render window) | window-message | - | n/a |
| 25 | `close-window-titlebar-x` | Window close button (title bar X) / Alt+F4 / system menu Close | window-message | - | n/a |
| 26 | `device-lost-recover` | Lock workstation / Ctrl+Alt+Del / user switch / another app taking exclusive fullscreen / GPU reset | window-message | - | n/a |
| 27 | `erase-background-suppressed` | Background erase during resize / expose | window-message | - | n/a |
| 28 | `expose-repaint` | Uncovering / revealing the window (another window moved away, un-minimise, drag off screen edge) | window-message | - | n/a |
| 29 | `gui-shutdown-teardown` | Any path that ends the GUI (window close, ShouldExit, GuiThread::Stop) | window-message | - | n/a |
| 30 | `imgui-input-forwarding` | Any mouse or keyboard input over the control window (move, left/right/middle button, wheel, characters, key down/up, focus change, cursor set, capture loss) | window-message | - | n/a |
| 31 | `initial-blank-window-before-gwin` *(audit)* | The very first paint of the control window at launch (window is shown before the render hook is armed) | window-message | - | n/a |
| 32 | `maximize-restore` | Maximize / Restore title bar button (or double-click title bar) | window-message | - | n/a |
| 33 | `message-pump-tick` | Every user input event reaching the window (the pump that delivers them) | window-message | - | n/a |
| 34 | `minimize-window` | Minimize title bar button | window-message | - | n/a |
| 35 | `raise-window-after-boot` | Booting a game (window is raised to the foreground as a result) | window-message | - | n/a |
| 36 | `warp-clear-fails-zbuffer` *(audit)* | Any frame drawn while the GUI is running on the WARP 9on12 fallback device | window-message | - | n/a |

## Detail

### 1. --headless command line flag

- **id**: `cli-headless`
- **input**: cli-arg
- **precondition**: always
- **effect**: Same gate as --no-gui in StartGuiIfWanted: the GUI thread is never started, so this surface does not exist for the run.
- **source**: `src/main.cpp:355`

### 2. --no-gui command line flag

- **id**: `cli-no-gui`
- **input**: cli-arg
- **precondition**: always
- **effect**: StartGuiIfWanted returns false before calling GuiThread::Start, so no window, no WndProc, no ImGui context is ever created and Gui::GetHwnd() stays nullptr for the whole run.
- **source**: `src/main.cpp:355`
- **notes**: Flag parsing itself lives in the CLI surface; recorded here because it is the visibility gate for this entire surface.

### 3. Launching the app on a machine without a usable D3D9 HAL device

- **id**: `device-creation-fallback`
- **input**: cli-arg
- **precondition**: GUI thread starting; Gui::Init reached CreateDevice
- **effect**: CreateDevice tries D3DCREATE_HARDWARE_VERTEXPROCESSING, then D3DCREATE_SOFTWARE_VERTEXPROCESSING; if both fail it releases IDirect3D9 and calls WarpD3D9::CreateForWindow(w.warp, hwnd, clientW, clientH). On success it sets warp_backed = true, device = warp.device, fills pp with X8R8G8B8 and EnableAutoDepthStencil = FALSE, and logs the WARP 9on12 fallback. If WARP also fails it logs "D3D9 HAL and WARP both unavailable" and Init tears down the window, so no GUI appears at all.
- **source**: `src/gui/gui_window.cpp:94`
- **notes**: User-visible outcome: either a WARP-backed control window or no control window.
- **audit correction**: The `input` field is "cli-arg", but no command line argument is involved - the trigger is the machine's GPU/driver state at launch (both D3D9 HAL CreateDevice calls failing at gui_window.cpp:94-99). The same mis-typed input appears on gui-init-failure-no-window (window/device creation failure is not a CLI arg either) and on gui-thread-start-idempotent (a duplicate Start() call is not a CLI arg). -> input should be a launch/environment kind (e.g. "launch"), not "cli-arg". Only cli-no-gui and cli-headless legitimately carry input = "cli-arg".

### 4. Launching the app when window creation or device creation fails

- **id**: `gui-init-failure-no-window`
- **input**: cli-arg
- **precondition**: GuiThread::Start was called
- **effect**: Gui::Init returns false after DestroyWindow + UnregisterClassW; ThreadMain logs "Gui::Init failed - GUI thread exiting", sets g_running = false and fulfils the promise with false; GuiThread::Start joins the thread and returns false; main logs "GUI thread launch failed - continuing without it" and the renderer runs with have_gui == false.
- **source**: `src/gui/gui_thread.cpp:23`
- **notes**: StartGuiIfWanted at src/main.cpp:354-358.

### 5. Starting the GUI a second time

- **id**: `gui-thread-start-idempotent`
- **input**: cli-arg
- **precondition**: GuiThread::Start called again while a GUI thread is live
- **disabled when**: g_running is already true - the call returns true immediately without spawning a thread
- **effect**: g_running.exchange(true) returns the previous value; when it was already true Start returns true at once, so no second window, no second std::thread and no second promise are created.
- **source**: `src/gui/gui_thread.cpp:51`
- **audit correction**: The entry states the g_running.exchange(true) guard makes a second GuiThread::Start() a harmless no-op. That only holds while the first GUI thread is still alive. On the window-close path ThreadMain sets g_running = false at gui_thread.cpp:45 and returns, and nobody joins g_thread, so g_thread stays joinable. A subsequent Start() then passes the exchange gate (g_running is false) and reaches `g_thread = std::thread(&ThreadMain, hinst, std::move(init_promise));` at gui_thread.cpp:56 - a move-assignment onto a still-joinable std::thread, which by the standard calls std::terminate() and kills the process. -> The guard makes a second Start() a no-op ONLY while the GUI thread is still running (g_running == true). After the user closed the control window (g_running reset to false at gui_thread.cpp:45 with no join), a second Start() would std::terminate at gui_thread.cpp:56. Currently latent because Start is only called once, from StartGuiIfWanted (src/main.cpp:356); the path via Stop() is safe because Stop() joins (gui_thread.cpp:66), leaving g_thread non-joinable.

### 6. Launching the app with the GUI enabled (default: no --no-gui / --headless)

- **id**: `launch-shows-control-window` *(audit)*
- **input**: cli-arg
- **precondition**: Gui::Init succeeded (window created and a D3D9 HAL or WARP device is up)
- **disabled when**: cli.no_gui \|\| cli.headless (src/main.cpp:355), or Gui::Init returned false
- **effect**: CreateWindowExW makes a WS_OVERLAPPEDWINDOW titled "573Renderer - Control" at a hardcoded x=80, y=60, 1360x820, then ShowWindow(w.hwnd, SW_SHOW) + UpdateWindow(w.hwnd) make it visible. Window position and size are never saved or restored: nothing in src/ calls GetWindowPlacement/SetWindowPlacement and no setting stores geometry (grep for window_x/window_pos/GetWindowPlacement/SetWindowPlacement over src/ returns nothing), so every run reopens at exactly 80,60 / 1360x820 no matter where the user last moved or sized the window.
- **source**: `src/gui/gui_window.cpp:147`
- **notes**: Window creation at gui_window.cpp:133-134. The inventory has the two negative gates (cli-no-gui, cli-headless) and the failure paths but no record for the default positive case. Complements imgui-layout-not-persisted, which only covers the ImGui-side layout (io.IniFilename = nullptr at gui_window.cpp:154); the Win32 geometry is a separate, also-unpersisted thing.

### 7. Running the control window on a high-DPI display, or dragging it to a monitor with a different scale factor

- **id**: `dpi-unaware-scaling` *(audit)*
- **input**: drag
- **precondition**: Windows display scaling is not 100%
- **effect**: The process never opts into DPI awareness: ImGui_ImplWin32_EnableDpiAwareness is not called, no SetProcessDpiAwareness / SetProcessDpiAwarenessContext call exists anywhere in src/, and no application manifest is produced (no *.manifest in the repo root and CMakeLists.txt contains no manifest directive). WndProc also has no WM_DPICHANGED case (gui_window.cpp:46-75). Consequence: the window is DPI-unaware, so Windows renders it at 96 DPI and bitmap-stretches it - all ImGui text and widgets are visibly blurry at 125/150/200% scaling, dragging between monitors of different scale re-stretches rather than re-laying-out, and the WM_GETMINMAXINFO clamp (kMinClientW = 932, kMinClientH = 600) is enforced in virtual rather than physical pixels.
- **source**: `src/gui/gui_window.cpp:44`
- **notes**: kMinClientW evaluates to 240 + 320 + 280 + 2*6 + 80 = 932 (src/gui/gui_layout_constants.h:18-20). Confirmed absent via grep for EnableDpiAwareness\|WM_DPICHANGED\|SetProcessDpi\|DPI_AWARE over src/ and CMakeLists.txt.
- **tests**: none

### 8. Dragging / resizing ImGui sub-windows and headers inside the control window

- **id**: `imgui-layout-not-persisted`
- **input**: drag
- **precondition**: always
- **effect**: io.IniFilename = nullptr, so ImGui writes no imgui.ini: any in-window layout the user changes is discarded on exit and the app always starts with the default layout.
- **source**: `src/gui/gui_window.cpp:154`
- **notes**: Persistence gate, not a widget.
- **tests**: none

### 9. Title bar drag to move the window

- **id**: `move-window-titlebar`
- **input**: drag
- **precondition**: always
- **effect**: Not intercepted: falls through the switch default to DefWindowProcW, which performs the standard move loop. Repaints during the move come through the WM_PAINT handler.
- **source**: `src/gui/gui_window.cpp:76`
- **notes**: Window is created at x=80, y=60, 1360x820 with WS_OVERLAPPEDWINDOW and title "573Renderer - Control" (gui_window.cpp:133).
- **tests**: none

### 10. Resizing or repainting the window while a frame is already being drawn

- **id**: `render-reentrancy-guard`
- **input**: drag
- **precondition**: A RenderFrameLocked call is already on the stack
- **effect**: The function-static bool in_frame short-circuits the nested call and returns immediately, so a WM_SIZE or WM_PAINT dispatched from inside a frame cannot start a second ImGui frame or a second device Reset.
- **source**: `src/gui/gui_window.cpp:221`
- **notes**: Gate that exists specifically because WM_SIZE calls RenderFrameLocked synchronously.
- **tests**: none

### 11. Window edge / corner resize grip

- **id**: `resize-drag-border`
- **input**: drag
- **precondition**: Window is not minimized (wp != SIZE_MINIMIZED)
- **effect**: WM_SIZE sets g_win->resize_pending = true and immediately calls RenderFrameLocked(*g_win) so the window repaints live during the drag. Inside that frame resize_pending is consumed and ResetDevice(w) re-reads GetClientRect, writes pp.BackBufferWidth/Height, calls ImGui_ImplDX9_InvalidateDeviceObjects(), device->Reset(&pp), then ImGui_ImplDX9_CreateDeviceObjects().
- **source**: `src/gui/gui_window.cpp:47`
- **notes**: ResetDevice body at gui_window.cpp:197-216; the resize_pending consumption at gui_window.cpp:225-228.
- **tests**: none

### 12. Attempt to shrink the window below the minimum size

- **id**: `resize-min-size-clamp`
- **input**: drag
- **precondition**: always - WM_GETMINMAXINFO is sent on every resize/move begin
- **effect**: WM_GETMINMAXINFO builds RECT{0,0,kMinClientW,kMinClientH}, runs AdjustWindowRect for WS_OVERLAPPEDWINDOW, and writes mmi->ptMinTrackSize.x/y, so Windows refuses to drag the frame smaller. kMinClientH is 600; kMinClientW is derived from kPaneLeftMin + kPaneCenterMin + kPaneRightMin + 2*kSplitterW + 80.
- **source**: `src/gui/gui_window.cpp:59`
- **notes**: Constants in src/gui/gui_layout_constants.h:18-20.
- **tests**: none

### 13. Resizing the window while the D3D9 device is lost

- **id**: `resize-while-device-lost`
- **input**: drag
- **precondition**: w.resize_pending is true
- **disabled when**: w.device_lost is true - ResetDevice is skipped for this frame
- **effect**: resize_pending is cleared but ResetDevice(w) is only called when !w.device_lost, so the backbuffer resize is deferred to the device-lost recovery path (which re-reads GetClientRect anyway).
- **source**: `src/gui/gui_window.cpp:227`
- **tests**: none

### 14. Holding the title bar or a resize border (Win32 modal move/size loop)

- **id**: `titlebar-drag-blocks-gui-thread-exit` *(audit)*
- **input**: drag
- **precondition**: GUI thread running and the user is holding the mouse button on the frame
- **effect**: DefWindowProcW enters the modal move/size loop inside DispatchMessage (gui_window.cpp:279), so Gui::PumpAndRender never returns while the button is held. The ThreadMain loop conditions - g_running.load() and state.ShouldExit().load() at gui_thread.cpp:34-35 - are therefore not evaluated for the whole duration of the drag, and GuiThread::Stop()'s g_thread.join() (gui_thread.cpp:66) blocks until the user lets go. Quitting the app while the user is dragging or resizing the control window stalls shutdown for exactly as long as the gesture lasts. A resize still repaints (WM_SIZE calls RenderFrameLocked synchronously); a pure title-bar move only repaints through WM_PAINT.
- **source**: `src/gui/gui_thread.cpp:34`
- **notes**: There is no WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE handling and no timer to keep the loop alive during the modal loop. The inventory's move-window-titlebar entry mentions the DefWindowProcW move loop but not that it freezes the GUI thread's exit checks.
- **tests**: none

### 15. Resizing the control window while running on the WARP fallback device

- **id**: `warp-resize-changes-present-params` *(audit)*
- **input**: drag
- **precondition**: w.warp_backed == true and WM_SIZE set resize_pending
- **disabled when**: w.device_lost is true (ResetDevice is skipped, gui_window.cpp:227)
- **effect**: ResetDevice resets the borrowed WARP device using Gui's own w.pp, which differs from the parameters WarpD3D9 created it with. w.pp still carries PresentationInterval = D3DPRESENT_INTERVAL_ONE (gui_window.cpp:91) and BackBufferCount = 0, while src/warp_device.cpp:117-119 created the device with BackBufferCount = 1 and D3DPRESENT_INTERVAL_IMMEDIATE. The user's first resize therefore silently switches the WARP-backed GUI from unthrottled presentation to vsync-locked presentation for the rest of the run.
- **source**: `src/gui/gui_window.cpp:208`
- **notes**: Only reachable on the WARP fallback; on the HAL path w.pp is the same struct the device was created from, so a resize changes nothing but the backbuffer size.
- **tests**: none

### 16. Hovering the control window

- **id**: `mouse-cursor-shape`
- **input**: hover
- **precondition**: always
- **effect**: The window class registers wc.hCursor = LoadCursor(nullptr, IDC_ARROW), so the standard arrow is the window's default cursor (ImGui's Win32 backend may override it per-widget through the WM_SETCURSOR it claims at gui_window.cpp:45).
- **source**: `src/gui/gui_window.cpp:130`
- **tests**: none

### 17. Alt key or F10 tap (keyboard system-menu activation)

- **id**: `alt-f10-system-menu-suppressed`
- **input**: key
- **precondition**: Window has focus
- **effect**: WM_SYSCOMMAND with (wp & 0xfff0) == SC_KEYMENU returns 0, swallowing it so the system menu does not open and the Alt key stays usable as an ImGui modifier. All other WM_SYSCOMMAND values break out of the switch to DefWindowProcW.
- **source**: `src/gui/gui_window.cpp:67`
- **tests**: none

### 18. Tab / arrow keys / Space / Enter keyboard navigation between widgets

- **id**: `keyboard-nav-enabled`
- **input**: key
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard is set at init
- **effect**: Gui::Init ORs ImGuiConfigFlags_NavEnableKeyboard into io.ConfigFlags, so every ImGui widget in the app is reachable and activatable with the keyboard, not just the mouse.
- **source**: `src/gui/gui_window.cpp:153`
- **notes**: Gate that enables an entire class of interactions across all panel surfaces.
- **tests**: none

### 19. Trying to get the control window back after closing it

- **id**: `control-window-not-reopenable` *(audit)*
- **input**: left-click
- **precondition**: The user already closed the control window during this run
- **disabled when**: always - no UI control, hotkey, tray icon or command reopens it
- **effect**: Nothing reopens it. GuiThread::Start is called only from StartGuiIfWanted (src/main.cpp:356) during startup, and by the time the close is observed ThreadMain has already run Gui::Shutdown (gui_thread.cpp:44), which DestroyWindow's the HWND, UnregisterClassW's the window class and sets g_current_hwnd = nullptr (gui_window.cpp:188-193). For the rest of the run Gui::GetHwnd() returns nullptr, so RaiseGuiWindow (src/main.cpp:258-265) becomes a no-op and any browse-for-folder dialog would be created unparented. The close is also unconfirmed and irreversible: WM_CLOSE is not intercepted anywhere in WndProc, so there is no "are you sure" prompt and no way to veto it.
- **source**: `src/gui/gui_window.cpp:70`
- **notes**: The inventory's close-window-titlebar-x records what closing does but not that it is one-way for the rest of the process lifetime.
- **tests**: none

### 20. Opening a native "browse for folder" dialog from any panel

- **id**: `modal-folder-dialog-parenting`
- **input**: left-click
- **precondition**: Gui::GetHwnd() returns a live HWND
- **disabled when**: GUI window destroyed - the dialog would be created unparented
- **effect**: Gui::GetHwnd() supplies the owner HWND to NativeDialog::BrowseForFolder, so the shell dialog is modal over the control window and the control window stops accepting input (and therefore stops pumping its own messages responsively) until the dialog closes.
- **source**: `src/gui/gui_window.cpp:28`
- **notes**: Call sites: src/gui/gui_setup_view.cpp:83, 291, 336 and src/gui/gui_qpro_panel.cpp:284. The buttons themselves belong to those panel surfaces.
- **tests**: none

### 21. Any widget activation inside the control window

- **id**: `per-frame-panel-build`
- **input**: left-click
- **precondition**: Device present (w.device != nullptr) and not re-entrant (in_frame == false)
- **disabled when**: in_frame is already true (a nested WM_PAINT/WM_SIZE during a frame) or w.device == nullptr
- **effect**: RenderFrameLocked runs ImGui_ImplDX9_NewFrame / ImGui_ImplWin32_NewFrame / ImGui::NewFrame, then Panels::Build() which draws and evaluates every widget, then EndFrame, sets ZENABLE/ALPHABLENDENABLE/SCISSORTESTENABLE off, Clear to RGBA(16,17,20,255), BeginScene, ImGui::Render + ImGui_ImplDX9_RenderDrawData, EndScene, Present.
- **source**: `src/gui/gui_window.cpp:250`
- **notes**: Dispatch point for every ImGui widget in the app; the widgets themselves are inventoried on the panel surfaces.
- **tests**: none
- **audit correction**: The effect describes the clear as unconditional: "Clear to RGBA(16,17,20,255)". That only happens on the D3D9 HAL path. When w.warp_backed is true the device has no depth-stencil (gui_window.cpp:117 and src/warp_device.cpp:111-120), so the D3DCLEAR_ZBUFFER flag in the Clear call at gui_window.cpp:257 makes the entire Clear fail with D3DERR_INVALIDCALL - and the return value is discarded. -> "...Clear to RGBA(16,17,20,255) on the HAL path; on the WARP fallback the same Clear fails entirely (D3DCLEAR_ZBUFFER with no attached depth-stencil) and the backbuffer is left uncleared." See the new warp-clear-fails-zbuffer record.

### 22. Right-click the title bar / click the window icon to open the system menu (Restore, Move, Size, Minimize, Maximize, Close)

- **id**: `system-menu-titlebar-rightclick` *(audit)*
- **input**: right-click
- **precondition**: Window visible and not in a modal loop
- **disabled when**: Keyboard invocation only: Alt+Space arrives as WM_SYSCOMMAND with (wp & 0xfff0) == SC_KEYMENU and is swallowed at gui_window.cpp:68, so the menu cannot be opened from the keyboard
- **effect**: WM_NCRBUTTONUP / WM_CONTEXTMENU are not intercepted by WndProc and fall through to DefWindowProcW (gui_window.cpp:76), which opens the standard system menu. Choosing Move or Size starts the same modal move/size loop as a frame drag; Minimize and Maximize produce the WM_SIZE paths (SIZE_MINIMIZED gated out, SIZE_MAXIMIZED triggering resize_pending + RenderFrameLocked); Close produces WM_CLOSE -> DestroyWindow -> WM_DESTROY -> PostQuitMessage(0). Only the keyboard route into this menu is disabled, the mouse route is fully live.
- **source**: `src/gui/gui_window.cpp:76`
- **notes**: Distinct gesture from move-window-titlebar (drag) and from alt-f10-system-menu-suppressed (keyboard). The close-window-titlebar-x entry lists "system menu Close" as an input but there is no record of the menu itself or of which routes to it work.
- **tests**: none

### 23. Mouse wheel over any scrollable ImGui child region in the control window

- **id**: `mouse-wheel-scroll`
- **input**: scroll
- **precondition**: Cursor is over the control window and ImGui claims the wheel message
- **effect**: WM_MOUSEWHEEL / WM_MOUSEHWHEEL are consumed by ImGui_ImplWin32_WndProcHandler and become io.MouseWheel / io.MouseWheelH, scrolling whichever ImGui child window is hovered during the next Panels::Build frame.
- **source**: `src/gui/gui_window.cpp:45`
- **notes**: The individual scrollable BeginChild regions belong to the panel surfaces; this entry records that the window forwards the gesture.
- **tests**: none

### 24. Quitting the app (Exit command from the GUI, or closing the render window)

- **id**: `app-exit-stops-gui-thread`
- **input**: window-message
- **precondition**: have_gui is true
- **effect**: App::Global().ShouldExit() is stored true, which fails the ThreadMain loop condition; ShutdownAfterLoop then calls GuiThread::Stop(), which sets g_running = false and joins the thread. ThreadMain falls out of the loop, runs Gui::Shutdown(w) and logs "GUI thread exited".
- **source**: `src/gui/gui_thread.cpp:34`
- **notes**: ShouldExit store + Stop at src/render_loop.cpp:326-327; other Stop call sites at src/main.cpp:312, 335, 342, 424.

### 25. Window close button (title bar X) / Alt+F4 / system menu Close

- **id**: `close-window-titlebar-x`
- **input**: window-message
- **precondition**: GUI thread started (not --no-gui / --headless) and window created
- **effect**: DefWindowProcW turns WM_CLOSE into DestroyWindow; WM_DESTROY calls PostQuitMessage(0); the next PeekMessage loop in PumpAndRender sees WM_QUIT and returns false; GuiThread::ThreadMain breaks its loop, logs "GUI window closed - GUI thread stopping (renderer continues headlessly)" and calls Gui::Shutdown(w). The app does NOT exit - the renderer keeps running headlessly and g_current_hwnd becomes nullptr.
- **source**: `src/gui/gui_window.cpp:70`
- **notes**: WM_CLOSE itself is not in the switch; it falls through to DefWindowProcW at gui_window.cpp:76. The WM_QUIT check is at gui_window.cpp:280, the thread-side break at gui_thread.cpp:36-40.

### 26. Lock workstation / Ctrl+Alt+Del / user switch / another app taking exclusive fullscreen / GPU reset

- **id**: `device-lost-recover`
- **input**: window-message
- **precondition**: A D3D9 device exists
- **effect**: Present returns D3DERR_DEVICELOST which sets w.device_lost = true. On subsequent frames TestCooperativeLevel is polled: D3DERR_DEVICENOTRESET triggers ResetDevice and clears device_lost on success; D3DERR_DEVICELOST aborts the frame (GUI stays frozen but alive); any other result clears device_lost. ResetDevice returning false (Reset itself reported D3DERR_DEVICELOST) leaves device_lost set for the next attempt.
- **source**: `src/gui/gui_window.cpp:230`
- **notes**: Present-side latch at gui_window.cpp:266-267; ResetDevice's own D3DERR_DEVICELOST branch at gui_window.cpp:209-212.

### 27. Background erase during resize / expose

- **id**: `erase-background-suppressed`
- **input**: window-message
- **precondition**: always
- **effect**: WM_ERASEBKGND returns 1 without erasing, so Windows never paints the class background - prevents flicker while the user drags the window edge.
- **source**: `src/gui/gui_window.cpp:57`

### 28. Uncovering / revealing the window (another window moved away, un-minimise, drag off screen edge)

- **id**: `expose-repaint`
- **input**: window-message
- **precondition**: g_win != nullptr (Gui::Init completed)
- **effect**: WM_PAINT calls RenderFrameLocked(*g_win) to draw a full ImGui frame, then ValidateRect(hwnd, nullptr) and returns 0 without ever calling BeginPaint/EndPaint.
- **source**: `src/gui/gui_window.cpp:53`

### 29. Any path that ends the GUI (window close, ShouldExit, GuiThread::Stop)

- **id**: `gui-shutdown-teardown`
- **input**: window-message
- **precondition**: Gui::Init previously succeeded
- **effect**: Gui::Shutdown clears g_win, and if an ImGui context exists runs ImGui_ImplDX9_Shutdown / ImGui_ImplWin32_Shutdown / ImGui::DestroyContext; for a WARP-backed window it just nulls the borrowed device (warp_backed handling), otherwise it Releases device and d3d; then DestroyWindow(hwnd), UnregisterClassW and g_current_hwnd = nullptr.
- **source**: `src/gui/gui_window.cpp:169`
- **notes**: After this, Gui::GetHwnd() returns nullptr so RaiseGuiWindow and the folder-browse dialogs become no-ops / unparented.

### 30. Any mouse or keyboard input over the control window (move, left/right/middle button, wheel, characters, key down/up, focus change, cursor set, capture loss)

- **id**: `imgui-input-forwarding`
- **input**: window-message
- **precondition**: ImGui context created (Gui::Init ran ImGui_ImplWin32_Init)
- **effect**: Every message is first passed to ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp); if it returns non-zero the WndProc returns 1 immediately and the message is consumed by ImGui (this is what feeds io.MousePos, io.MouseDown, io.MouseWheel, io.AddInputCharacter, key events and cursor shape). Only messages ImGui does not claim reach the switch below.
- **source**: `src/gui/gui_window.cpp:45`
- **notes**: Single gate for every widget interaction on every panel surface; the per-widget behaviour lives in Panels::Build.

### 31. The very first paint of the control window at launch (window is shown before the render hook is armed)

- **id**: `initial-blank-window-before-gwin` *(audit)*
- **input**: window-message
- **precondition**: Gui::Init has passed CreateWindowExW but has not yet reached g_win = &w
- **effect**: ShowWindow + UpdateWindow (gui_window.cpp:147-148) force a synchronous WM_PAINT while g_win is still nullptr - it is only assigned at gui_window.cpp:164, after ImGui_ImplWin32_Init/ImGui_ImplDX9_Init. The WM_PAINT case's `if (g_win != nullptr)` gate therefore fails: RenderFrameLocked is skipped and only ValidateRect(hwnd, nullptr) runs, marking the window painted with nothing drawn. The user sees an unpainted/garbage client area (WM_ERASEBKGND also returns 1 without erasing) until the GUI thread's first PumpAndRender frame, up to one Sleep(16) later. The same g_win nullptr gate swallows any WM_SIZE that arrives during CreateWindowExW.
- **source**: `src/gui/gui_window.cpp:54`
- **notes**: This is one of the early-return gates the audit checklist asks for: `if (g_win != nullptr)` at gui_window.cpp:48 and :54. The inventory's expose-repaint entry lists that precondition but never records the launch-time window where it is false.

### 32. Maximize / Restore title bar button (or double-click title bar)

- **id**: `maximize-restore`
- **input**: window-message
- **precondition**: always
- **effect**: Same WM_SIZE non-minimized path as a border drag: resize_pending = true plus an immediate RenderFrameLocked, causing a D3D9 device Reset to the new client size on the next frame.
- **source**: `src/gui/gui_window.cpp:48`
- **notes**: Distinct user gesture, identical code path to resize-drag-border.

### 33. Every user input event reaching the window (the pump that delivers them)

- **id**: `message-pump-tick`
- **input**: window-message
- **precondition**: w.hwnd != nullptr; otherwise PumpAndRender returns false immediately
- **disabled when**: w.hwnd == nullptr
- **effect**: PumpAndRender drains the thread message queue with PeekMessage(PM_REMOVE) + TranslateMessage (which is what turns WM_KEYDOWN into WM_CHAR for ImGui text fields) + DispatchMessage, returns false on WM_QUIT, otherwise calls RenderFrameLocked. ThreadMain then Sleep(16) between iterations, capping the GUI at roughly 60 Hz.
- **source**: `src/gui/gui_window.cpp:273`
- **notes**: Sleep(16) pacing at src/gui/gui_thread.cpp:41; TranslateMessage at gui_window.cpp:278 is what makes typing in any InputText work.
- **audit correction**: The effect claims "ThreadMain then Sleep(16) between iterations, capping the GUI at roughly 60 Hz". That is wrong: it ignores w.pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE (src/gui/gui_window.cpp:91), which makes Present at gui_window.cpp:266 block for the next vblank on top of the sleep. Sleep(16) is also ~15.6 ms or worse at the default (unmodified) system timer resolution, since nothing in src/ calls timeBeginPeriod. -> Each iteration costs a Sleep(16) PLUS a vsync wait inside Present (D3DPRESENT_INTERVAL_ONE, gui_window.cpp:91), i.e. roughly two refresh intervals - about 30 Hz on a 60 Hz display, not 60 Hz. Everything else in the entry (PeekMessage PM_REMOVE drain, TranslateMessage producing WM_CHAR at gui_window.cpp:278, WM_QUIT returning false at :280, Sleep(16) at gui_thread.cpp:41) is accurate.

### 34. Minimize title bar button

- **id**: `minimize-window`
- **input**: window-message
- **precondition**: always
- **effect**: WM_SIZE arrives with wp == SIZE_MINIMIZED, so the (g_win != nullptr) && wp != SIZE_MINIMIZED gate fails: resize_pending is NOT set and no frame is rendered (avoids resetting the device to a 0x0 client rect). The handler still returns 0, swallowing the message.
- **source**: `src/gui/gui_window.cpp:48`
- **notes**: This is the explicit SIZE_MINIMIZED gate.

### 35. Booting a game (window is raised to the foreground as a result)

- **id**: `raise-window-after-boot`
- **input**: window-message
- **precondition**: Gui::GetHwnd() != nullptr
- **disabled when**: GUI thread not running or already shut down - RaiseGuiWindow returns early
- **effect**: RaiseGuiWindow calls ShowWindow(gh, SW_SHOW), BringWindowToTop(gh) and SetForegroundWindow(gh) on the control window and logs "Raised control window to foreground after boot", so the control window pops in front of the newly created render window.
- **source**: `src/main.cpp:259`
- **notes**: Consumer of Gui::GetHwnd() declared at src/gui/gui_window.cpp:28-30.

### 36. Any frame drawn while the GUI is running on the WARP 9on12 fallback device

- **id**: `warp-clear-fails-zbuffer` *(audit)*
- **input**: window-message
- **precondition**: w.warp_backed == true (both D3D9 HAL CreateDevice attempts failed)
- **effect**: The fallback path sets w.pp.EnableAutoDepthStencil = FALSE (gui_window.cpp:117), and the WARP device is itself created with no depth-stencil (src/warp_device.cpp:111-120 never sets EnableAutoDepthStencil). RenderFrameLocked nevertheless clears with D3DCLEAR_TARGET \| D3DCLEAR_ZBUFFER; D3D9 rejects D3DCLEAR_ZBUFFER with D3DERR_INVALIDCALL when no depth-stencil is attached, and the whole Clear becomes a no-op, not just the depth portion. The HRESULT is discarded, so on the WARP path the backbuffer is never cleared - with SwapEffect D3DSWAPEFFECT_DISCARD the user sees undefined/stale content behind the ImGui draw list instead of the intended RGBA(16,17,20) background. The HAL path is unaffected because it requests EnableAutoDepthStencil = TRUE / D3DFMT_D16 (gui_window.cpp:89-90).
- **source**: `src/gui/gui_window.cpp:257`
- **notes**: User-visible only on machines that fall back to WARP. Fix would be to mask the flag on warp_backed, but that is a code change, not an inventory item; recorded here as the actual observable behaviour of the fallback.

