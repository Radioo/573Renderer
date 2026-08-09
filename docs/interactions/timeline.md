# Timeline dock

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `label-combo-open` | "go to label..." combo box (preview shows the active label when set) | combo-select | yes | 1 |
| 2 | `track-drag-scrub` | Timeline track drag-scrub | drag | yes | 4 |
| 3 | `export-disabled-notice` | "(seek / pause disabled during export)" notice | hover | - | **none** |
| 4 | `frame-counter-readout` | "cur / total" frame counter and "loop N" wrap counter (mono font) | hover | - | **none** |
| 5 | `label-combo-tooltip` | Tooltip on the label combo | hover | yes | 1 |
| 6 | `no-scene-placeholder` | "Load an IFS to control playback." placeholder text | hover | - | **none** |
| 7 | `track-drag-tooltip` | Tooltip on the timeline track body | hover | yes | 4 |
| 8 | `track-label-tick-tooltip` | Label tick mark on the track (hit-tested region inside the custom-drawn track) | hover | yes | 4 |
| 9 | `transport-jump-back-100-tooltip` | Tooltip on the jump-back-100 button | hover | yes | **none** |
| 10 | `transport-jump-fwd-100-tooltip` | Tooltip on the jump-forward-100 button | hover | yes | **none** |
| 11 | `transport-play-pause-tooltip` | Tooltip on the Play/Pause button | hover | yes | **none** |
| 12 | `transport-step-back-1-tooltip` | Tooltip on the step-back-1 button | hover | yes | **none** |
| 13 | `transport-step-fwd-1-tooltip` | Tooltip on the step-forward-1 button | hover | yes | **none** |
| 14 | `label-combo-keyboard-open-navigate-dismiss` *(audit)* | Keyboard operation of the ##tl_labels combo popup (open, move between rows, select, dismiss) | key | - | **none** |
| 15 | `nav-activate-focused-transport-button` *(audit)* | Space / Enter activation of the nav-focused transport button (and the Space double-fire against the play/pause shortcut) | key | - | **none** |
| 16 | `nav-arrow-move-vs-step-shortcut` *(audit)* | Left / Right arrow while an item on this surface holds nav focus (nav move plus frame step in the same frame) | key | - | **none** |
| 17 | `nav-keyboard-focus-transport-controls` *(audit)* | Keyboard navigation focus (Tab / Shift+Tab) over the transport buttons and the label combo | key | - | **none** |
| 18 | `shortcut-ctrl-e-export` | Ctrl+E keyboard shortcut (open export panel) | key | - | **none** |
| 19 | `shortcut-left-step-back` | Left arrow keyboard shortcut (step back 1 frame) | key | - | **none** |
| 20 | `shortcut-right-step-fwd` | Right arrow keyboard shortcut (step forward 1 frame) | key | - | **none** |
| 21 | `shortcut-shift-left-jump-back` | Shift+Left arrow chord (step back 100 frames) | key | - | **none** |
| 22 | `shortcut-shift-right-jump-fwd` | Shift+Right arrow chord (step forward 100 frames) | key | - | **none** |
| 23 | `shortcut-space-play-pause` | Space keyboard shortcut (toggle play/pause) | key | - | **none** |
| 24 | `label-combo-item-select` | Label row inside the combo: "<name>   (frame N)" | left-click | - | **none** |
| 25 | `track-click-seek` | Timeline track (custom-drawn InvisibleButton scrub bar) | left-click | yes | 4 |
| 26 | `track-inert-when-total-zero` *(audit)* | Timeline track (##tl_track) in the mc_total == 0 state - hit target exists but is fully inert | left-click | - | 4 |
| 27 | `track-label-tick-click` | Label tick mark on the track (click target) | left-click | yes | 4 |
| 28 | `transport-jump-back-100` | Jump back 100 frames button (ICON_JUMP_BACK glyph) | left-click | yes | **none** |
| 29 | `transport-jump-fwd-100` | Jump forward 100 frames button (ICON_JUMP_FWD glyph) | left-click | yes | **none** |
| 30 | `transport-play-pause` | Play / Pause toggle button (glyph swaps between ICON_PLAY and ICON_PAUSE) | left-click | yes | **none** |
| 31 | `transport-step-back-1` | Step back 1 frame button (ICON_STEP_BACK glyph) | left-click | yes | **none** |
| 32 | `transport-step-fwd-1` | Step forward 1 frame button (ICON_STEP_FWD glyph) | left-click | yes | **none** |
| 33 | `transport-step-with-no-master-clock` *(audit)* | Transport step/jump buttons and Left/Right shortcuts while live.mc_total == 0 | left-click | yes | **none** |
| 34 | `label-combo-popup-scroll` | Label combo popup list (scrollable when there are many labels) | scroll | - | **none** |
| 35 | `timeline-dock-child-region` | Timeline dock container (fixed-height child window) | scroll | - | **none** |
| 36 | `shortcut-suppression-while-typing` | Text-input focus suppression of all timeline shortcuts | text-entry | - | **none** |

## Detail

### 1. "go to label..." combo box (preview shows the active label when set)

- **id**: `label-combo-open`
- **input**: combo-select
- **path**: `##timeline_dock/##tl_labels`
- **precondition**: status.scene_loaded == true AND status.labels is non-empty (the whole combo is skipped when there are no labels)
- **disabled when**: export phase == Capturing
- **effect**: ImGui::BeginCombo("##tl_labels", preview) with SetNextItemWidth(170). Opening it pops up the label list; no state change on open itself. Preview text is status.active_label or "go to label..." when empty.
- **source**: `src/gui/gui_timeline.cpp:54`
- **tooltip**: yes
- **notes**: Drawn on the same line as the transport buttons (SameLine spacing 12).
- **tests**: `timeline label combo is absent without labels`

### 2. Timeline track drag-scrub

- **id**: `track-drag-scrub`
- **input**: drag
- **path**: `##timeline_dock/##tl_track`
- **precondition**: status.scene_loaded == true AND live.mc_total > 0 AND not exporting; the InvisibleButton is held (IsItemActive stays true while the button is down, including outside the item rect)
- **disabled when**: export phase == Capturing
- **effect**: Every frame the item is active, the frame under the mouse x is recomputed and PostSeekPaused fires again: repeated AfpCmd::SeekFrame commands plus paused = true, producing continuous scrubbing.
- **source**: `src/gui/gui_timeline.cpp:186`
- **tooltip**: yes
- **notes**: Same code path as the click entry (IsItemActive), listed separately because it is a distinct gesture with continuous effect.
- **tests**: `timeline label tick on the track jumps to that label`, `timeline shows a hint and no transport before a scene loads`, `timeline track drag seeks and pauses`, `timeline track ignores clicks while no master length is known`
- **audit correction**: The effect is wrong about continuity. It states 'Every frame the item is active, the frame under the mouse x is recomputed and PostSeekPaused fires again', but the label-tick branch preempts the seek. LabelHitTest runs whenever `hovered` is true (line 170), and IsItemHovered() stays true for the item that is itself active, so while dragging with the button held, any time the cursor passes within 5px of a label tick the `if (label_hit >= 0)` branch at line 171 executes SetTooltip and then `return;` at line 178 - which is BEFORE the `if (active)` seek block at line 184. The scrub therefore freezes at its last position for as long as the cursor sits in the +/-5px band around any tick, and the label tooltip pops up mid-drag. -> Effect: while ##tl_track is held (ImGui::IsItemActive(), line 144), each frame recomputes frame = lround(((io.MousePos.x - p0.x) / w) * (total - 1)) and calls PostSeekPaused, which clamps to [0, total-1], posts AfpCmd::Wrap(AfpCmd::SeekFrame{frame}) and sets o.paused = true (lines 184-188). EXCEPTION: the seek is skipped on any frame where the cursor is hovering the track AND is within 5px of a label tick, because the label-hit branch at lines 171-179 returns early (and instead shows the 'label ... click to play from here' tooltip). Dragging across a dense label region therefore stutters/stalls rather than scrubbing smoothly. Once the cursor leaves the item rect entirely, `hovered` becomes false, LabelHitTest is skipped (line 170) and continuous scrubbing resumes with the raw, clamped mouse x.

### 3. "(seek / pause disabled during export)" notice

- **id**: `export-disabled-notice`
- **input**: hover
- **path**: `##timeline_dock/(seek / pause disabled during export)`
- **precondition**: export phase == Capturing
- **disabled when**: n/a (static disabled-styled text)
- **effect**: TextDisabled appended to the transport row to explain why every transport control is greyed out. No command posted.
- **source**: `src/gui/gui_timeline.cpp:113`
- **notes**: Visibility GATE mirror of the BeginDisabled at line 224.
- **tests**: none

### 4. "cur / total" frame counter and "loop N" wrap counter (mono font)

- **id**: `frame-counter-readout`
- **input**: hover
- **path**: `##timeline_dock (unnamed text items)`
- **precondition**: status.scene_loaded == true
- **disabled when**: n/a (read-only text, never interactive)
- **effect**: Displays live.mc_cur / live.mc_total via ImGui::Text and live.mc_wrap_count via ImGui::TextDisabled inside a PushMonoFont/PopFont pair. No user action, no command posted.
- **source**: `src/gui/gui_timeline.cpp:104`
- **notes**: Included for completeness: it is the only non-interactive readout in the transport row besides the export notice.
- **tests**: none

### 5. Tooltip on the label combo

- **id**: `label-combo-tooltip`
- **input**: hover
- **path**: `##timeline_dock/##tl_labels`
- **precondition**: IsItemHovered() on the ##tl_labels combo (checked after EndCombo, so it applies to the closed combo widget)
- **disabled when**: while exporting the item is disabled
- **effect**: ImGui::SetTooltip explaining that it jumps to a frame label and plays from there (label playback), and that labels are also the ticks on the track which can be hovered for their name and clicked to jump.
- **source**: `src/gui/gui_timeline.cpp:68`
- **tooltip**: yes
- **notes**: Single tooltip covering both the combo and the track ticks.
- **tests**: `timeline label combo is absent without labels`

### 6. "Load an IFS to control playback." placeholder text

- **id**: `no-scene-placeholder`
- **input**: hover
- **path**: `##timeline_dock/Load an IFS to control playback.`
- **precondition**: status.scene_loaded == false
- **disabled when**: n/a (static disabled-styled text, never interactive)
- **effect**: Renders TextDisabled placeholder instead of the transport row, track and shortcut handler. While this branch is active NO timeline interaction exists at all (transport buttons, track, label combo and the Space/Left/Right/Ctrl+E shortcuts are all skipped because HandleShortcuts is only called in the else branch).
- **source**: `src/gui/gui_timeline.cpp:222`
- **notes**: This is the visibility GATE for every other entry on this surface: everything else requires status.scene_loaded == true.
- **tests**: none

### 7. Tooltip on the timeline track body

- **id**: `track-drag-tooltip`
- **input**: hover
- **path**: `##timeline_dock/##tl_track`
- **precondition**: IsItemHovered() on ##tl_track, not exporting, live.mc_total > 0, and the cursor is NOT within 5px of a label tick (the tick branch returns first)
- **disabled when**: export phase == Capturing (DrawTrack returns before any tooltip code)
- **effect**: ImGui::SetTooltip explaining drag-to-seek via afp_mc_control 0xF08 and that seeking pauses playback, mirroring the AFP debug viewer's TIME controls.
- **source**: `src/gui/gui_timeline.cpp:181`
- **tooltip**: yes
- **notes**: Mutually exclusive with the label-tick tooltip.
- **tests**: `timeline label tick on the track jumps to that label`, `timeline shows a hint and no transport before a scene loads`, `timeline track drag seeks and pauses`, `timeline track ignores clicks while no master length is known`

### 8. Label tick mark on the track (hit-tested region inside the custom-drawn track)

- **id**: `track-label-tick-tooltip`
- **input**: hover
- **path**: `##timeline_dock/##tl_track`
- **precondition**: IsItemHovered() on ##tl_track, not exporting, live.mc_total > 0, and \|mouse_x - tick_x\| <= 5px for some label (LabelHitTest; the LAST matching label wins when ticks overlap)
- **disabled when**: export phase == Capturing
- **effect**: ImGui::SetTooltip naming the label and its frame number and stating that clicking plays from there. Suppresses the generic drag-to-seek tooltip and the seek-on-active handler for that frame (early return).
- **source**: `src/gui/gui_timeline.cpp:173`
- **tooltip**: yes
- **notes**: ONE entry covering all ticks, which are drawn in a loop over status.labels (DrawTrackMarkers, line 129) at x = x0 + w*frame/total.
- **tests**: `timeline label tick on the track jumps to that label`, `timeline shows a hint and no transport before a scene loads`, `timeline track drag seeks and pauses`, `timeline track ignores clicks while no master length is known`

### 9. Tooltip on the jump-back-100 button

- **id**: `transport-jump-back-100-tooltip`
- **input**: hover
- **path**: `##timeline_dock/`
- **precondition**: IsItemHovered() on the jump-back-100 button; scene_loaded == true
- **disabled when**: while exporting the item is disabled, so it is not hovered/tooltipped
- **effect**: ImGui::SetTooltip explaining that it steps back 100 frames, is bound to Shift+Left, wraps around the master timeline, and that seeking pauses playback.
- **source**: `src/gui/gui_timeline.cpp:45`
- **tooltip**: yes
- **notes**: Tooltip emitted by the shared TransportButton helper with the per-button tip string passed at line 79.
- **tests**: none

### 10. Tooltip on the jump-forward-100 button

- **id**: `transport-jump-fwd-100-tooltip`
- **input**: hover
- **path**: `##timeline_dock/`
- **precondition**: IsItemHovered() on the jump-forward-100 button
- **disabled when**: while exporting the item is disabled
- **effect**: ImGui::SetTooltip stating it steps forward 100 frames and is bound to Shift+Right.
- **source**: `src/gui/gui_timeline.cpp:45`
- **tooltip**: yes
- **notes**: Tip text supplied at line 97.
- **tests**: none

### 11. Tooltip on the Play/Pause button

- **id**: `transport-play-pause-tooltip`
- **input**: hover
- **path**: `##timeline_dock/ (or ##timeline_dock/)`
- **precondition**: IsItemHovered() on the play/pause button
- **disabled when**: while exporting the item is disabled
- **effect**: ImGui::SetTooltip explaining Space binding, that it sets stream playback speed to 1/0 via afp_stream_set_speed mirroring the debug viewer's RETURN+SHIFT toggle, and that playback is forced running while exporting.
- **source**: `src/gui/gui_timeline.cpp:45`
- **tooltip**: yes
- **notes**: Tip text supplied at lines 87-89.
- **tests**: none

### 12. Tooltip on the step-back-1 button

- **id**: `transport-step-back-1-tooltip`
- **input**: hover
- **path**: `##timeline_dock/`
- **precondition**: IsItemHovered() on the step-back-1 button
- **disabled when**: while exporting the item is disabled
- **effect**: ImGui::SetTooltip stating it steps back one frame and is bound to the Left arrow key.
- **source**: `src/gui/gui_timeline.cpp:45`
- **tooltip**: yes
- **notes**: Tip text supplied at line 84.
- **tests**: none

### 13. Tooltip on the step-forward-1 button

- **id**: `transport-step-fwd-1-tooltip`
- **input**: hover
- **path**: `##timeline_dock/`
- **precondition**: IsItemHovered() on the step-forward-1 button
- **disabled when**: while exporting the item is disabled
- **effect**: ImGui::SetTooltip stating it steps forward one frame and is bound to the Right arrow key.
- **source**: `src/gui/gui_timeline.cpp:45`
- **tooltip**: yes
- **notes**: Tip text supplied at line 93.
- **tests**: none

### 14. Keyboard operation of the ##tl_labels combo popup (open, move between rows, select, dismiss)

- **id**: `label-combo-keyboard-open-navigate-dismiss` *(audit)*
- **input**: key
- **path**: `##timeline_dock/##tl_labels/(popup)`
- **precondition**: status.scene_loaded == true AND status.labels is non-empty AND the ##tl_labels combo holds nav focus (or its popup is open)
- **disabled when**: export phase == Capturing
- **effect**: Space/Enter on the focused combo opens the popup (ImGui::BeginCombo, line 54). Up/Down move the nav cursor across the per-label ImGui::Selectable rows (line 61); ImGui::SetItemDefaultFocus() at line 64 places the initial nav cursor on the currently active label and scrolls the popup to it. Enter/Space on a row makes the Selectable return true, posting AfpCmd::Wrap(AfpCmd::GotoLabel{.name = l.name}) and auto-closing the popup. Escape closes the popup with no command posted; clicking outside does the same. While the popup is open, HandleShortcuts (line 228) still runs unconditionally, so Space also toggles pause and Left/Right still step frames.
- **source**: `src/gui/gui_timeline.cpp:54, 61, 64`
- **notes**: The inventory covers only mouse click-select and mouse scroll of this popup; the entire keyboard path, the Escape/click-outside dismissal, and the shortcut bleed-through while the popup is open are absent.
- **tests**: none

### 15. Space / Enter activation of the nav-focused transport button (and the Space double-fire against the play/pause shortcut)

- **id**: `nav-activate-focused-transport-button` *(audit)*
- **input**: key
- **path**: `##timeline_dock/<focused transport item>`
- **precondition**: status.scene_loaded == true, io.WantTextInput == false, and one of the five transport buttons (or the ##tl_labels combo) currently holds ImGui nav focus
- **disabled when**: export phase == Capturing (item is disabled AND HandleShortcuts returns at line 199 before the Space branch)
- **effect**: ImGui NavActivate (Space) / NavInput (Enter) presses the focused ImGui::Button at line 44, so it returns true and runs its handler (StepWrapped -100/-1/+1/+100 or PostTogglePause). CRITICALLY, when the key used is Space this happens IN ADDITION TO the Space shortcut at line 201: HandleShortcuts calls ImGui::IsKeyPressed(ImGuiKey_Space, false), whose public overload uses ImGuiKeyOwner_Any and performs no focus routing (see imgui.h:1058 'Consider using Shortcut() ... can do focus routing check'), so nav ownership of Space does not suppress it. Pressing Space with e.g. the step-forward button focused therefore both seeks +1 frame with paused=true AND toggles the pause flag in the same frame. Enter does not have this conflict.
- **source**: `src/gui/gui_timeline.cpp:44 and src/gui/gui_timeline.cpp:201`
- **notes**: The existing shortcut-space-play-pause entry claims Space only calls PostTogglePause. That is only true when no transport item holds nav focus.
- **tests**: none

### 16. Left / Right arrow while an item on this surface holds nav focus (nav move plus frame step in the same frame)

- **id**: `nav-arrow-move-vs-step-shortcut` *(audit)*
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock, competing with ImGui nav)`
- **precondition**: status.scene_loaded == true, io.WantTextInput == false, keyboard nav is active and the nav cursor is inside the timeline dock (or its combo popup)
- **disabled when**: export phase == Capturing (HandleShortcuts returns at line 199)
- **effect**: ImGui's nav consumes Left/Right to move the nav cursor between the horizontally laid-out transport items, while lines 203-204 independently call ImGui::IsKeyPressed(ImGuiKey_LeftArrow / ImGuiKey_RightArrow) with the owner-agnostic public overload and default repeat=true, so StepWrapped(state, live, -/+step) also fires. One key press both moves the focus ring and seeks the playhead (100 frames when io.KeyShift is held, line 202). Holding the key repeats both.
- **source**: `src/gui/gui_timeline.cpp:203-204`
- **notes**: The inventory's shortcut-left/right entries describe only the seek half. The nav-move half is a second, simultaneous effect of the same key press.
- **tests**: none

### 17. Keyboard navigation focus (Tab / Shift+Tab) over the transport buttons and the label combo

- **id**: `nav-keyboard-focus-transport-controls` *(audit)*
- **input**: key
- **path**: `##timeline_dock/<focused transport item>`
- **precondition**: status.scene_loaded == true; ImGuiConfigFlags_NavEnableKeyboard is enabled application-wide (src/gui/gui_window.cpp:153), so Tab/Shift+Tab cycles keyboard focus through every tab-stop item of the focused window including this child
- **disabled when**: export phase == Capturing (BeginDisabled at src/gui/gui_timeline.cpp:224 marks the items disabled, which also removes them as nav tab-stops)
- **effect**: Tab / Shift+Tab moves the ImGui nav cursor across the five ImGui::Button transport items (line 44, submitted at lines 79, 84, 86, 93, 97) and the ##tl_labels combo (line 54), drawing the ImGuiCol_NavHighlight ring (set in gui_style.cpp) on the focused one. No command is posted by focus movement itself; it selects which item Space/Enter will activate. The ##tl_track InvisibleButton (line 142) is NOT reachable: ImGui 1.92.7 InvisibleButton adds ImGuiItemFlags_NoTabStop\|ImGuiItemFlags_NoNav unless ImGuiButtonFlags_EnableNav is passed, and line 142 passes no flags.
- **source**: `src/gui/gui_timeline.cpp:44 (Button tab-stops) + src/gui/gui_window.cpp:153 (NavEnableKeyboard)`
- **notes**: The inventory has no keyboard-nav entries at all; it treats the transport controls as mouse-only. Nav focus is a real reachable state on this surface and is the precondition for the two entries below.
- **tests**: none

### 18. Ctrl+E keyboard shortcut (open export panel)

- **id**: `shortcut-ctrl-e-export`
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock)`
- **precondition**: status.scene_loaded == true AND ImGuiIO::WantTextInput == false (any focused text field swallows all shortcuts on this surface)
- **disabled when**: never - this is the ONLY shortcut that still works while exporting, since the exporting early-return happens after it
- **effect**: Export::RequestOpen() is called (opens/raises the export panel), then HandleShortcuts returns so no other shortcut runs this frame.
- **source**: `src/gui/gui_timeline.cpp:195`
- **notes**: IsKeyPressed(ImGuiKey_E, false) with repeat disabled, gated on io.KeyCtrl.
- **tests**: none
- **audit correction**: The notes ('IsKeyPressed(ImGuiKey_E, false) with repeat disabled, gated on io.KeyCtrl') and the precondition imply an exclusive Ctrl+E chord. Line 195 tests only `ImGui::IsKeyPressed(ImGuiKey_E, false) && io.KeyCtrl`; io.KeyShift, io.KeyAlt and io.KeySuper are never excluded, so Ctrl+Shift+E, Ctrl+Alt+E, Ctrl+Win+E and any combination of those also open the export panel and also swallow the rest of HandleShortcuts for that frame via the `return` at line 197. -> Precondition: status.scene_loaded == true AND io.WantTextInput == false AND ImGuiKey_E was pressed this frame (no repeat) AND io.KeyCtrl is down. Modifiers are NOT exclusive: Shift/Alt/Super may additionally be held, so Ctrl+Shift+E and Ctrl+Alt+E fire the same handler. Notes should record that the chord is non-exclusive and that it is not routed through ImGui::Shortcut(), so it fires regardless of which window or item has focus.

### 19. Left arrow keyboard shortcut (step back 1 frame)

- **id**: `shortcut-left-step-back`
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock)`
- **precondition**: status.scene_loaded == true AND io.WantTextInput == false AND io.KeyShift == false
- **disabled when**: export phase == Capturing
- **effect**: StepWrapped(state, live, -1) -> PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{wrapped frame})) plus MutateLiveOverrides(paused = true).
- **source**: `src/gui/gui_timeline.cpp:203`
- **notes**: IsKeyPressed(ImGuiKey_LeftArrow) with default repeat = true, so holding the key repeats the step.
- **tests**: none

### 20. Right arrow keyboard shortcut (step forward 1 frame)

- **id**: `shortcut-right-step-fwd`
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock)`
- **precondition**: status.scene_loaded == true AND io.WantTextInput == false AND io.KeyShift == false
- **disabled when**: export phase == Capturing
- **effect**: StepWrapped(state, live, +1) -> PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{wrapped frame})) plus MutateLiveOverrides(paused = true).
- **source**: `src/gui/gui_timeline.cpp:204`
- **notes**: IsKeyPressed(ImGuiKey_RightArrow) with default repeat = true.
- **tests**: none

### 21. Shift+Left arrow chord (step back 100 frames)

- **id**: `shortcut-shift-left-jump-back`
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock)`
- **precondition**: status.scene_loaded == true AND io.WantTextInput == false AND io.KeyShift == true
- **disabled when**: export phase == Capturing
- **effect**: step is set to 100 by the Shift modifier, so StepWrapped(state, live, -100) runs -> AfpCmd::SeekFrame + paused = true.
- **source**: `src/gui/gui_timeline.cpp:202`
- **notes**: Same IsKeyPressed(LeftArrow) branch; the chord differs only by the io.KeyShift-driven step size computed at line 202. Key repeat applies.
- **tests**: none

### 22. Shift+Right arrow chord (step forward 100 frames)

- **id**: `shortcut-shift-right-jump-fwd`
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock)`
- **precondition**: status.scene_loaded == true AND io.WantTextInput == false AND io.KeyShift == true
- **disabled when**: export phase == Capturing
- **effect**: step = 100 via the Shift modifier, so StepWrapped(state, live, +100) runs -> AfpCmd::SeekFrame + paused = true.
- **source**: `src/gui/gui_timeline.cpp:202`
- **notes**: Same IsKeyPressed(RightArrow) branch as the plain Right entry, differentiated by io.KeyShift at line 202.
- **tests**: none

### 23. Space keyboard shortcut (toggle play/pause)

- **id**: `shortcut-space-play-pause`
- **input**: key
- **path**: `n/a (global key handler inside ##timeline_dock)`
- **precondition**: status.scene_loaded == true AND io.WantTextInput == false
- **disabled when**: export phase == Capturing (HandleShortcuts returns right after the Ctrl+E check)
- **effect**: PostTogglePause: MutateLiveOverrides flips o.paused, then PostCommand(AfpCmd::Wrap(AfpCmd::SetPaused{new_paused})).
- **source**: `src/gui/gui_timeline.cpp:201`
- **notes**: IsKeyPressed(ImGuiKey_Space, false) - no key repeat.
- **tests**: none

### 24. Label row inside the combo: "<name>   (frame N)"

- **id**: `label-combo-item-select`
- **input**: left-click
- **path**: `##timeline_dock/##tl_labels/<name>   (frame N)##lbl<i>`
- **precondition**: the ##tl_labels combo is open; one row exists per entry in status.labels
- **disabled when**: never while the popup is open
- **effect**: ImGui::Selectable returns true -> state.PostCommand(AfpCmd::Wrap(AfpCmd::GotoLabel{.name = l.name})), which jumps the afp playhead to that label and plays from there. The currently active label row is drawn selected and gets ImGui::SetItemDefaultFocus().
- **source**: `src/gui/gui_timeline.cpp:61`
- **notes**: ONE entry describing a row drawn in a loop over status.labels (i = 0..n-1). Empty names render as "(unnamed)"; the ##lbl<i> suffix disambiguates duplicate names.
- **tests**: none

### 25. Timeline track (custom-drawn InvisibleButton scrub bar)

- **id**: `track-click-seek`
- **input**: left-click
- **path**: `##timeline_dock/##tl_track`
- **precondition**: status.scene_loaded == true AND live.mc_total > 0 AND not exporting AND the click position is NOT within 5px of a label tick
- **disabled when**: export phase == Capturing (DrawTrack returns early at the `if (exporting) return;` guard, and the whole row is inside BeginDisabled)
- **effect**: ImGui::IsItemActive() on ##tl_track is true on press -> frame = lround(((mouse_x - track_x0)/w) * (total-1)) -> PostSeekPaused: clamps to [0, total-1], posts AfpCmd::Wrap(AfpCmd::SeekFrame{frame}) and MutateLiveOverrides(paused = true).
- **source**: `src/gui/gui_timeline.cpp:184`
- **tooltip**: yes
- **notes**: Track is w = content region width by 22px high; it also renders the progress fill (ImGuiCol_Header), the green export-capture overlay when exporting with frames_captured > 0, the label ticks and the playhead bar.
- **tests**: `timeline label tick on the track jumps to that label`, `timeline shows a hint and no transport before a scene loads`, `timeline track drag seeks and pauses`, `timeline track ignores clicks while no master length is known`

### 26. Timeline track (##tl_track) in the mc_total == 0 state - hit target exists but is fully inert

- **id**: `track-inert-when-total-zero` *(audit)*
- **input**: left-click
- **path**: `##timeline_dock/##tl_track`
- **precondition**: status.scene_loaded == true AND live.mc_total == 0 (scene loaded but the master clock reports no total, e.g. a still/no-timeline asset)
- **disabled when**: export phase == Capturing (additionally disabled by BeginDisabled at line 224)
- **effect**: ImGui::InvisibleButton("##tl_track", ...) is still submitted at line 142, so the full-width 22px strip still swallows hover and left-press and still becomes IsItemActive. DrawTrack then hits `if (total == 0) return;` at line 150 right after painting the empty ScrollbarBg rect and Border, so NO progress fill, NO export overlay, NO label ticks, NO playhead, NO tooltip (neither the drag-to-seek one at line 181 nor the label-tick one at line 173) and NO PostSeekPaused ever run. Clicking and dragging the track does nothing at all and gives no feedback.
- **source**: `src/gui/gui_timeline.cpp:142 and src/gui/gui_timeline.cpp:150`
- **notes**: The inventory carries `live.mc_total > 0` as a precondition on four track entries but never records the complementary state, in which a visible, clickable, tooltip-less dead control is presented to the user. This is the gate demanded by the early-return sweep.
- **tests**: `timeline label tick on the track jumps to that label`, `timeline shows a hint and no transport before a scene loads`, `timeline track drag seeks and pauses`, `timeline track ignores clicks while no master length is known`

### 27. Label tick mark on the track (click target)

- **id**: `track-label-tick-click`
- **input**: left-click
- **path**: `##timeline_dock/##tl_track`
- **precondition**: cursor within 5px of a label tick on the hovered ##tl_track, not exporting, live.mc_total > 0
- **disabled when**: export phase == Capturing
- **effect**: ImGui::IsItemClicked(ImGuiMouseButton_Left) -> state.PostCommand(AfpCmd::Wrap(AfpCmd::GotoLabel{.name = l.name})). The function returns immediately afterwards, so the normal position-seek does NOT also fire.
- **source**: `src/gui/gui_timeline.cpp:175`
- **tooltip**: yes
- **notes**: ONE entry for all ticks (loop over status.labels). Clicking a tick jumps to the label rather than to the raw pixel frame.
- **tests**: `timeline label tick on the track jumps to that label`, `timeline shows a hint and no transport before a scene loads`, `timeline track drag seeks and pauses`, `timeline track ignores clicks while no master length is known`

### 28. Jump back 100 frames button (ICON_JUMP_BACK glyph)

- **id**: `transport-jump-back-100`
- **input**: left-click
- **path**: `##timeline_dock/`
- **precondition**: status.scene_loaded == true
- **disabled when**: export phase == Capturing (ImGui::BeginDisabled wraps the whole transport row and track)
- **effect**: Calls StepWrapped(state, live, -100): computes total = live.mc_total (min 1), cur = min(live.mc_cur, total-1), wraps ((cur-100)%total+total)%total, then PostSeekPaused -> state.PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{frame})) and state.MutateLiveOverrides(o.paused = true).
- **source**: `src/gui/gui_timeline.cpp:79`
- **tooltip**: yes
- **notes**: Label is the raw icon glyph macro ICON_JUMP_BACK = U+E892; ImGui::Button(icon, ImVec2(34,0)) inside helper TransportButton (line 44).
- **tests**: none

### 29. Jump forward 100 frames button (ICON_JUMP_FWD glyph)

- **id**: `transport-jump-fwd-100`
- **input**: left-click
- **path**: `##timeline_dock/`
- **precondition**: status.scene_loaded == true
- **disabled when**: export phase == Capturing
- **effect**: StepWrapped(state, live, +100) -> PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{wrapped frame})) plus MutateLiveOverrides(paused = true).
- **source**: `src/gui/gui_timeline.cpp:97`
- **tooltip**: yes
- **notes**: ICON_JUMP_FWD = U+E893.
- **tests**: none

### 30. Play / Pause toggle button (glyph swaps between ICON_PLAY and ICON_PAUSE)

- **id**: `transport-play-pause`
- **input**: left-click
- **path**: `##timeline_dock/ (or ##timeline_dock/)`
- **precondition**: status.scene_loaded == true
- **disabled when**: export phase == Capturing (playback is forced running while exporting)
- **effect**: PostTogglePause: state.MutateLiveOverrides flips o.paused and captures the new value, then state.PostCommand(AfpCmd::Wrap(AfpCmd::SetPaused{new_paused})), which sets afp stream speed 1/0.
- **source**: `src/gui/gui_timeline.cpp:86`
- **tooltip**: yes
- **notes**: Label depends on state.GetLiveOverrides().paused: ICON_PLAY (U+E768) when paused, ICON_PAUSE (U+E769) when running. The ImGui ID therefore changes with playback state.
- **tests**: none

### 31. Step back 1 frame button (ICON_STEP_BACK glyph)

- **id**: `transport-step-back-1`
- **input**: left-click
- **path**: `##timeline_dock/`
- **precondition**: status.scene_loaded == true
- **disabled when**: export phase == Capturing
- **effect**: StepWrapped(state, live, -1) -> wrapped frame index -> PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{frame})) and MutateLiveOverrides(paused = true).
- **source**: `src/gui/gui_timeline.cpp:84`
- **tooltip**: yes
- **notes**: ICON_STEP_BACK = U+E76B.
- **tests**: none

### 32. Step forward 1 frame button (ICON_STEP_FWD glyph)

- **id**: `transport-step-fwd-1`
- **input**: left-click
- **path**: `##timeline_dock/`
- **precondition**: status.scene_loaded == true
- **disabled when**: export phase == Capturing
- **effect**: StepWrapped(state, live, +1) -> PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{wrapped frame})) plus MutateLiveOverrides(paused = true).
- **source**: `src/gui/gui_timeline.cpp:93`
- **tooltip**: yes
- **notes**: ICON_STEP_FWD = U+E76C.
- **tests**: none

### 33. Transport step/jump buttons and Left/Right shortcuts while live.mc_total == 0

- **id**: `transport-step-with-no-master-clock` *(audit)*
- **input**: left-click
- **path**: `##timeline_dock/, , , `
- **precondition**: status.scene_loaded == true AND live.mc_total == 0
- **disabled when**: export phase == Capturing
- **effect**: Unlike the track, the four step/jump buttons stay live. StepWrapped substitutes `total = live.mc_total > 0 ? (int)live.mc_total : 1` at line 37, so maxf = 0, cur = min(mc_cur, 0) = 0 and the wrap ((0 +/- delta) % 1 + 1) % 1 always evaluates to 0. Every press therefore posts AfpCmd::Wrap(AfpCmd::SeekFrame{.frame = 0}) and sets o.paused = true via MutateLiveOverrides. Net user-visible effect: the step buttons act purely as a 'pause and rewind to 0' with no stepping. The same applies to the Left/Right/Shift+Left/Shift+Right shortcuts at lines 203-204.
- **source**: `src/gui/gui_timeline.cpp:36-41`
- **tooltip**: yes
- **notes**: The tooltips still promise 'Step back/forward N frames', which is misleading in this state. The inventory's four step entries describe only the mc_total > 0 behaviour.
- **tests**: none

### 34. Label combo popup list (scrollable when there are many labels)

- **id**: `label-combo-popup-scroll`
- **input**: scroll
- **path**: `##timeline_dock/##tl_labels/(popup)`
- **precondition**: the ##tl_labels combo popup is open and the label list exceeds the default combo popup height
- **effect**: Standard ImGui combo popup scrolling over the status.labels rows; no application state changes.
- **source**: `src/gui/gui_timeline.cpp:55`
- **notes**: The loop emits one Selectable per label with no explicit child/height, so ImGui's combo popup provides the scrolling.
- **tests**: none

### 35. Timeline dock container (fixed-height child window)

- **id**: `timeline-dock-child-region`
- **input**: scroll
- **path**: `##timeline_dock`
- **precondition**: always (the dock child is created unconditionally in RenderTimelineDock)
- **effect**: Creates a bordered child window of height Gui::kTimelineH with ImGuiCol_ChildBg pushed to PopupBg; ImGuiWindowFlags_NoScrollbar is set so no scrollbar is drawn. Mouse-wheel over it is captured by the child; because all content (one transport row + one 22px track) fits the fixed height, there is no scrollable overflow in practice.
- **source**: `src/gui/gui_timeline.cpp:217`
- **notes**: Listed because it is the only BeginChild on this surface. NoScrollbar + fixed kTimelineH height means it is scroll-capable in principle only.
- **tests**: none
- **audit correction**: The effect reasons from the wrong flag and states an unverified conclusion. ImGuiWindowFlags_NoScrollbar only hides the scrollbar; it does NOT disable mouse-wheel scrolling (that is ImGuiWindowFlags_NoScrollWithMouse, which line 217 does not set). The claim 'there is no scrollable overflow in practice' is true only by a 0px margin: with Gui::kTimelineH = 76 (src/gui/gui_layout_constants.h:15), WindowPadding.y = 10 and FramePadding.y = 5 (src/gui/gui_style.cpp:169-172) and the 16px base font (src/gui/gui_style.cpp:138), the inner region is 76 - 2*1 border - 2*10 padding = 54px while the content is FrameHeight 26 + ItemSpacing.y 6 + track height 22 = 54px exactly. -> Effect: creates a bordered child of height Gui::kTimelineH (76px) with ImGuiCol_ChildBg pushed to PopupBg. ImGuiWindowFlags_NoScrollbar hides the scrollbar but leaves wheel scrolling enabled (ImGuiWindowFlags_NoScrollWithMouse is not set), so if the content ever exceeds the 54px inner region the dock scrolls silently with no visible scrollbar. At the current style (16px font, FramePadding.y 5, ItemSpacing.y 6, track h 22) the content is exactly 54px, i.e. zero slack: any font-size, FramePadding, ItemSpacing or DPI-scale increase makes the wheel scroll the transport row out of view.

### 36. Text-input focus suppression of all timeline shortcuts

- **id**: `shortcut-suppression-while-typing`
- **input**: text-entry
- **precondition**: any ImGui text field anywhere in the app has keyboard focus (ImGuiIO::WantTextInput == true)
- **disabled when**: n/a (it is itself the gate)
- **effect**: HandleShortcuts returns immediately, so Ctrl+E, Space, Left/Right and their Shift variants do nothing while the user is typing. Typing itself goes to the focused field, not to the timeline.
- **source**: `src/gui/gui_timeline.cpp:193`
- **notes**: Recorded as a distinct gate because it changes what every keyboard entry above does.
- **tests**: none

