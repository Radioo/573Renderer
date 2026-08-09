# Ready-view shell

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `main-window-move-resize` | The full-screen host window "##main" | drag | - | **none** |
| 2 | `os-window-resize-pane-clamp` *(audit)* | OS window resize (dragging the Win32 window border, maximize/restore, snap) | drag | - | **none** |
| 3 | `splitter-left-drag` | Left vertical splitter between the Browse pane and the centre scene pane | drag | - | 1 |
| 4 | `splitter-right-drag` | Right vertical splitter between the centre scene pane and the inspector pane | drag | - | **none** |
| 5 | `splitter-left-hover` | Hover feedback on the left splitter | hover | - | 1 |
| 6 | `splitter-right-hover` | Hover feedback on the right splitter | hover | - | **none** |
| 7 | `status-export-tag-tooltip-busy` | Tooltip on the export status tag, Capturing/Encoding phase | hover | yes | **none** |
| 8 | `status-export-tag-tooltip-done` | Tooltip on the export status tag, Done phase | hover | yes | **none** |
| 9 | `status-export-tag-tooltip-failed` | Tooltip on the export status tag, Failed phase | hover | yes | **none** |
| 10 | `status-last-error-tooltip` | Tooltip on the truncated error text in the status strip | hover | yes | **none** |
| 11 | `status-open-folder-tooltip` | Tooltip on the "Open folder" button | hover | yes | **none** |
| 12 | `topbar-export-button-tooltip` | Tooltip on the "Export..." button | hover | yes | **none** |
| 13 | `keyboard-nav-focus-activate` *(audit)* | Keyboard navigation over every focusable item on the ready-view shell (main-view tab Buttons, Export Button, export status SmallButtons, "Open folder" SmallButton, IFS filter InputText, IFS tree TreeNodeEx directory nodes and Selectable file rows) | key | - | **none** |
| 14 | `ifs-filter-focus-suppresses-global-shortcuts` *(audit)* | Keyboard capture by the IFS filter text box | left-click | - | 3 |
| 15 | `ifs-tree-dir-expand` | IFS directory node in the browse tree (label "<segment>   (<file_count>)") | left-click | - | **none** |
| 16 | `ifs-tree-file-select` | IFS file leaf row in the browse tree (Selectable named after the last path segment) | left-click | - | **none** |
| 17 | `main-view-tab-button` | Main-view tab button in the top bar (one per active MainTab panel: "Renderer", "qpro", …) | left-click | - | **none** |
| 18 | `status-export-tag-button` | Export status tag SmallButton in the status strip (dynamic label: "capturing N" / "capturing N [NVENC]", "encoding N frames...", "export done", "export failed") | left-click | yes | **none** |
| 19 | `status-open-folder-button` | "Open folder" SmallButton next to the export status tag | left-click | yes | 3 |
| 20 | `topbar-export-button` | "Export..." button (export glyph + label) at the right of the top bar | left-click | yes | **none** |
| 21 | `ifs-tree-scroll` | Scrollable IFS tree region | scroll | - | 7 |
| 22 | `main-view-body-scroll` | Main view body container (the region that hosts whichever MainTab panel is selected) | scroll | - | 3 |
| 23 | `pane-center-scroll` | Centre pane container (scene pane) | scroll | - | 7 |
| 24 | `pane-left-scroll` | Left pane container (Browse pane) | scroll | - | 3 |
| 25 | `pane-right-scroll` | Right pane container (inspector pane) | scroll | - | 11 |
| 26 | `status-strip-region` | Status strip container at the bottom of the window | scroll | - | 4 |
| 27 | `topbar-region` | Top bar container strip | scroll | - | 9 |
| 28 | `ifs-picker-scanning-state` | Browse pane while the IFS scan is running | state-change | - | n/a |
| 29 | `ifs-filter-input` | IFS path filter text box with hint "filter by path..." | text-entry | - | 3 |
| 30 | `ifs-filter-input-editing-keymap` *(audit)* | IFS path filter text box - full ImGui text-editing keymap and mouse text selection | text-entry | - | 3 |
| 31 | `ready-vs-setup-gate` | Whole ready-view shell visibility | window-message | - | n/a |

## Detail

### 1. The full-screen host window "##main"

- **id**: `main-window-move-resize`
- **input**: drag
- **path**: `##main`
- **precondition**: BootState == Ready
- **disabled when**: always - NoMove, NoResize and NoCollapse are set unconditionally
- **effect**: None. The window is pinned each frame to the viewport WorkPos/WorkSize (lines 432-434) and created with ImGuiWindowFlags_NoTitleBar \| NoResize \| NoMove \| NoCollapse \| NoBringToFrontOnFocus (lines 436-438), so dragging or resizing it inside ImGui is impossible; it instead follows the OS window size via the main viewport.
- **source**: `src/gui/gui_panels.cpp:436`
- **notes**: Resizing the real Win32 window changes vp->WorkSize and therefore re-lays out this window; that Win32 message handling lives outside this file (not read here). No WM_* handling exists in gui_panels.cpp.
- **tests**: none
- **audit correction**: Wrong/incomplete effect: 'None.' plus the note 'that Win32 message handling lives outside this file (not read here)'. The consequence of an OS resize that matters for this surface is implemented IN this file and is destructive, not neutral. -> ImGui-level drag/resize/collapse of ##main is indeed impossible (flags at src/gui/gui_panels.cpp:436-438). But an OS resize reaches this file: WM_SIZE (src/gui/gui_window.cpp:47-52) forces a frame, ##main is re-pinned to vp->WorkSize (lines 433-434), and ClampPaneWidths (lines 209-229, called at line 396) permanently subtracts the centre-pane deficit from the file-static right_w then left_w (lines 387-388). See the new os-window-resize-pane-clamp record.

### 2. OS window resize (dragging the Win32 window border, maximize/restore, snap)

- **id**: `os-window-resize-pane-clamp` *(audit)*
- **input**: drag
- **path**: `##main/main_view/{pane_left,pane_center,pane_right}`
- **precondition**: BootState == Ready and the Renderer MainTab panel is the selected view, so RenderRendererView runs (src/gui/gui_panels.cpp:386).
- **disabled when**: The shrink bottoms out at Gui::kPaneLeftMin (240) and Gui::kPaneRightMin (280); past that the centre is allowed under its minimum and is only floored at 1px (line 227). The client can not be dragged below kMinClientW/kMinClientH, enforced by WM_GETMINMAXINFO at src/gui/gui_window.cpp:59-66 with kMinClientW = kPaneLeftMin + kPaneCenterMin + kPaneRightMin + 2*kSplitterW + 80 (src/gui/gui_layout_constants.h:19).
- **effect**: WM_SIZE (src/gui/gui_window.cpp:47-52) re-renders a frame at the new client size; RenderReadyView re-pins ##main to vp->WorkPos/WorkSize each frame (lines 433-434) and RenderRendererView recomputes avail_w from GetContentRegionAvail().x (line 390) and calls ClampPaneWidths (line 396). When avail_w - left_w - right_w - 2*kSplitterW falls below Gui::kPaneCenterMin (320), ClampPaneWidths PERMANENTLY subtracts the deficit from the file-static right_w first (lines 215-220) and then from the file-static left_w (lines 221-225) - both statics (lines 387-388) are passed by non-const reference. Widening the window afterwards does NOT restore them, so a transient narrow window permanently shrinks the Browse and Inspector panes until the user re-drags the splitters. row_h is also recomputed from the new height minus Gui::kTimelineH (lines 391-392), resizing all three pane children and the splitter hit areas.
- **source**: `src/gui/gui_panels.cpp:396`
- **notes**: This is the one real user interaction that mutates the same persistent state the two splitter entries own, and the existing main-window-move-resize entry records the effect as 'None'. Constants read at src/gui/gui_layout_constants.h:5-16.
- **tests**: none

### 3. Left vertical splitter between the Browse pane and the centre scene pane

- **id**: `splitter-left-drag`
- **input**: drag
- **path**: `##main/main_view/##split_l`
- **precondition**: Renderer main view is active (Panels::RenderRendererView is the selected MainTab panel)
- **disabled when**: drag is refused (widths unchanged) when the resulting left width < Gui::kPaneLeftMin or the resulting centre width < Gui::kPaneCenterMin
- **effect**: Gui::VSplitter("##split_l", ...) draws an ImGui::InvisibleButton and, while IsItemActive(), adds ImGui::GetIO().MouseDelta.x to the file-static left_w and subtracts it from a local copy of center_w, rejecting the delta if either side would fall below Gui::kPaneLeftMin / Gui::kPaneCenterMin (gui_splitter.cpp:9,29-38). The new left_w persists in the static at src/gui/gui_panels.cpp:387 and is re-clamped next frame by ClampPaneWidths (line 396), resizing the pane_left / pane_center children.
- **source**: `src/gui/gui_panels.cpp:405`
- **notes**: Implementation read at src/gui/gui_splitter.cpp:8-40. Press-drag-release on an InvisibleButton, i.e. a genuine mouse-down + move + up gesture, not a click.
- **tests**: `splitter drag moves the boundary between the left and centre panes`

### 4. Right vertical splitter between the centre scene pane and the inspector pane

- **id**: `splitter-right-drag`
- **input**: drag
- **path**: `##main/main_view/##split_r`
- **precondition**: Renderer main view is active
- **disabled when**: drag is refused when the resulting centre width < Gui::kPaneCenterMin or right width < Gui::kPaneRightMin
- **effect**: Gui::VSplitter("##split_r", ...) is passed a local copy of center_w as `left` and the file-static right_w as `right` (src/gui/gui_panels.cpp:416-417); while active it subtracts MouseDelta.x from right_w unless that would push right_w below Gui::kPaneRightMin or the centre below Gui::kPaneCenterMin. right_w persists in the static at line 388 and ClampPaneWidths re-derives center_w next frame, resizing pane_center / pane_right.
- **source**: `src/gui/gui_panels.cpp:416`
- **notes**: Implementation read at src/gui/gui_splitter.cpp:8-40.
- **tests**: none

### 5. Hover feedback on the left splitter

- **id**: `splitter-left-hover`
- **input**: hover
- **path**: `##main/main_view/##split_l`
- **precondition**: Mouse over the "##split_l" InvisibleButton, or the button is being dragged
- **effect**: Calls ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW) and repaints the 1px separator line with ImGuiCol_SeparatorHovered (hover) or ImGuiCol_SeparatorActive (drag) instead of ImGuiCol_Separator (gui_splitter.cpp:12-26). No tooltip.
- **source**: `src/gui/gui_splitter.cpp:14`
- **notes**: Separate from the drag entry because a bare hover already produces a visible window-level effect (cursor shape change).
- **tests**: `splitter drag moves the boundary between the left and centre panes`
- **audit correction**: Wrong source line: cites src/gui/gui_splitter.cpp:14 for the SetMouseCursor call and 'gui_splitter.cpp:12-26' for the colour logic. Line 14 is blank; the hover test is line 12, the SetMouseCursor is line 13, the colour pick is lines 15-20 and the separator line is drawn at line 25. -> source should be src/gui/gui_splitter.cpp:13 (IsItemActive :11, IsItemHovered :12, SetMouseCursor(ImGuiMouseCursor_ResizeEW) :13, ImGuiCol_Separator/Hovered/Active selection :15-20, AddLine :25).

### 6. Hover feedback on the right splitter

- **id**: `splitter-right-hover`
- **input**: hover
- **path**: `##main/main_view/##split_r`
- **precondition**: Mouse over the "##split_r" InvisibleButton, or the button is being dragged
- **effect**: SetMouseCursor(ImGuiMouseCursor_ResizeEW) plus the hovered/active separator colour (gui_splitter.cpp:12-26). No tooltip.
- **source**: `src/gui/gui_splitter.cpp:14`
- **tests**: none
- **audit correction**: Same wrong source line as splitter-left-hover: cites src/gui/gui_splitter.cpp:14 and 'gui_splitter.cpp:12-26'. Line 14 is blank. -> source should be src/gui/gui_splitter.cpp:13 (hover test :12, SetMouseCursor :13, colour selection :15-20, AddLine :25).

### 7. Tooltip on the export status tag, Capturing/Encoding phase

- **id**: `status-export-tag-tooltip-busy`
- **input**: hover
- **path**: `##main/status_strip/<capturing|encoding label> (tooltip)`
- **precondition**: IsItemHovered() on the status tag AND phase is neither Done nor Failed, i.e. Capturing or Encoding (lines 326-328)
- **effect**: SetTooltip explaining that clicking opens the export dialog. No state change.
- **source**: `src/gui/gui_panels.cpp:327`
- **tooltip**: yes
- **notes**: Third of the three tooltip branches on the status tag item.
- **tests**: none

### 8. Tooltip on the export status tag, Done phase

- **id**: `status-export-tag-tooltip-done`
- **input**: hover
- **path**: `##main/status_strip/export done (tooltip)`
- **precondition**: IsItemHovered() on the status tag AND ex.phase == App::ExportPhase::Done (lines 321-323)
- **effect**: SetTooltip showing the produced output path plus an instruction that clicking opens the export dialog. No state change.
- **source**: `src/gui/gui_panels.cpp:323`
- **tooltip**: yes
- **notes**: One of three mutually exclusive SetTooltip calls on the same item; listed separately per instructions.
- **tests**: none

### 9. Tooltip on the export status tag, Failed phase

- **id**: `status-export-tag-tooltip-failed`
- **input**: hover
- **path**: `##main/status_strip/export failed (tooltip)`
- **precondition**: IsItemHovered() on the status tag AND ex.phase == App::ExportPhase::Failed (lines 324-325)
- **effect**: SetTooltip showing the stored export error string plus an instruction that clicking opens the export dialog. No state change.
- **source**: `src/gui/gui_panels.cpp:325`
- **tooltip**: yes
- **notes**: Second of the three tooltip branches on the status tag item.
- **tests**: none

### 10. Tooltip on the truncated error text in the status strip

- **id**: `status-last-error-tooltip`
- **input**: hover
- **path**: `##main/status_strip/<error text> (tooltip)`
- **precondition**: status.last_error is non-empty (the else branch at lines 361-365) AND the mouse hovers the coloured error text item
- **effect**: SetTooltip showing the full, untruncated error string (the strip itself renders PrettifyPath(last_error, 60)). No state change.
- **source**: `src/gui/gui_panels.cpp:364`
- **tooltip**: yes
- **notes**: When last_error is empty the strip instead prints a non-interactive "render ok" (line 360) with no tooltip.
- **tests**: none

### 11. Tooltip on the "Open folder" button

- **id**: `status-open-folder-tooltip`
- **input**: hover
- **path**: `##main/status_strip/Open folder (tooltip)`
- **precondition**: IsItemHovered() on the Open folder button (line 334); therefore also requires phase == Done with a non-empty output path
- **effect**: SetTooltip naming the output path that will be shown in Explorer. No state change.
- **source**: `src/gui/gui_panels.cpp:335`
- **tooltip**: yes
- **tests**: none

### 12. Tooltip on the "Export..." button

- **id**: `topbar-export-button-tooltip`
- **input**: hover
- **path**: `##main/topbar/\xEE\xA2\x98  Export... (tooltip)`
- **precondition**: Mouse hovers the Export button item (ImGui::IsItemHovered() at line 276). Fires even when the button is disabled, since the hover test follows EndDisabled.
- **effect**: ImGui::SetTooltip(...) draws a tooltip explaining that the button exports the playing animation to video / image and naming the Ctrl+E shortcut. No state change.
- **source**: `src/gui/gui_panels.cpp:277`
- **tooltip**: yes
- **notes**: Separate entry per the tooltip-inventory rule.
- **tests**: none

### 13. Keyboard navigation over every focusable item on the ready-view shell (main-view tab Buttons, Export Button, export status SmallButtons, "Open folder" SmallButton, IFS filter InputText, IFS tree TreeNodeEx directory nodes and Selectable file rows)

- **id**: `keyboard-nav-focus-activate` *(audit)*
- **input**: key
- **path**: `##main/** (every focusable item submitted by gui_panels.cpp)`
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard is set once at startup (src/gui/gui_window.cpp:153), so every item this file submits is reachable and activatable without a mouse. Requires BootState == Ready so RenderReadyView draws the shell (src/gui/gui_panels.cpp:468).
- **disabled when**: Items inside BeginDisabled are skipped by nav, so the Export button at src/gui/gui_panels.cpp:272 is not reachable by keyboard while status.scene_loaded == false (BeginDisabled/EndDisabled at 271/275).
- **effect**: Tab / Shift+Tab and the nav arrow keys move the ImGui nav highlight between the submitted items; Space or Enter activates the focused item and produces exactly the same effect as a left-click - g_main_view = i (src/gui/gui_panels.cpp:251), Export::RequestOpen() (273 and 319), NativeDialog::RevealInFileManager (333), App::Cmd::LoadContent post (141-142). Left/Right arrow on a focused directory TreeNodeEx (line 155) closes/opens it without a mouse. Nav focus also auto-scrolls the enclosing child window (ifs_scroll at line 195, main_view at line 449, pane_* at 398/409/421) to keep the focused item visible, which is a scroll path independent of the wheel entries already in the inventory.
- **source**: `src/gui/gui_window.cpp:153`
- **notes**: The inventory has zero keyboard entries. This is a global, always-on input path for this surface: NavEnableKeyboard is enabled unconditionally in Gui::Window init and io.IniFilename is nulled on the next line, so nav state is never persisted between runs and focus always starts unset.
- **tests**: none

### 14. Keyboard capture by the IFS filter text box

- **id**: `ifs-filter-focus-suppresses-global-shortcuts` *(audit)*
- **input**: left-click
- **path**: `##main/main_view/pane_left/##ifsfilter`
- **precondition**: The filter InputText (src/gui/gui_panels.cpp:188) is the active item, i.e. the user clicked into it or nav-activated it. Applies whenever the Renderer main view is drawn, because RenderRendererView calls RenderTimelineDock() at src/gui/gui_panels.cpp:425 in the same frame.
- **effect**: An active InputText sets ImGuiIO::WantTextInput, and HandleShortcuts() returns immediately on that flag (src/gui/gui_timeline.cpp:193). While the filter box holds focus the global chords are therefore DEAD: Ctrl+E export (gui_timeline.cpp:195), Space play/pause (gui_timeline.cpp:201) and Left/Right frame-step, x100 with Shift (gui_timeline.cpp:202-204) do not fire; those keys type a space / move the caret inside the field instead. Deactivating the field (Enter, Escape, or clicking anywhere else) clears WantTextInput and restores all three shortcuts on the next frame.
- **source**: `src/gui/gui_timeline.cpp:193`
- **notes**: The inventory notes that Ctrl+E lives in gui_timeline.cpp but never records that a control on THIS surface gates it. This is the reason the tooltip's advertised Ctrl+E stops working while the browse filter is focused.
- **tests**: `browse pane filter hides non-matching entries`, `browse pane hides the tree while a scan is running`, `browse pane reports an empty catalog`

### 15. IFS directory node in the browse tree (label "<segment>   (<file_count>)")

- **id**: `ifs-tree-dir-expand`
- **input**: left-click
- **path**: `##main/main_view/pane_left/ifs_scroll/<segment PushID>/##dir  (PushID(node.segment) at line 153, widget id literal "##dir", display text formatted)`
- **precondition**: The node is a directory (node.entry == nullptr) AND its subtree passes the current filter (line 133) AND every ancestor node is open
- **effect**: Toggles the ImGui TreeNodeEx open/closed state stored in the ImGui storage under the pushed ID (line 155). When open, the children are recursed into (lines 156-161), revealing sub-directories and file rows.
- **source**: `src/gui/gui_panels.cpp:155`
- **notes**: ONE entry describing a node drawn in a recursion over the tree. Nodes are force-opened (ImGuiTreeNodeFlags_DefaultOpen) whenever the filter box is non-empty or the whole catalog has <= 20 entries (tree_small, lines 151/198), so user collapse state can be overridden the first time a node appears under those conditions. ImGuiTreeNodeFlags_SpanAvailWidth makes the whole row clickable; the arrow itself is also clickable.
- **tests**: none

### 16. IFS file leaf row in the browse tree (Selectable named after the last path segment)

- **id**: `ifs-tree-file-select`
- **input**: left-click
- **path**: `##main/main_view/pane_left/ifs_scroll/<full_path PushID>/<segment>  (PushID(entry->full_path) at line 137, label is the file segment)`
- **precondition**: The node is a file (node.entry != nullptr, line 135) AND its subtree passes the current filter (SubtreeMatchesFilter, line 133) AND every ancestor TreeNodeEx is open
- **disabled when**: never - but the click is a no-op when the row is already the active IFS (is_active == true)
- **effect**: If the row is not already the active file, posts App::Cmd::LoadContent{.path = entry->full_path, .from_arc = entry->from_arc} through state.PostCommand (lines 140-142) and writes a LOG("Gui", ...) line. Clicking the already-selected row does nothing (guarded by `if (!is_active)`). The row draws highlighted when entry->full_path == status.current_ifs_path.
- **source**: `src/gui/gui_panels.cpp:138`
- **notes**: ONE entry describing a row drawn in a loop / recursion over the IFS catalog (RenderIfsTreeNode recursion at line 158, seeded by the loop at lines 199-201). Flag ImGuiSelectableFlags_SpanAllColumns means the whole row width is the hit area. No double-click or right-click handling exists on this row.
- **tests**: none
- **audit correction**: Wrong effect: the notes claim 'Flag ImGuiSelectableFlags_SpanAllColumns means the whole row width is the hit area.' SpanAllColumns only makes a Selectable span the columns of an enclosing ImGui table, and there is no BeginTable anywhere in gui_panels.cpp (grep for BeginTable returns nothing) - the tree rows are plain window items, so the flag is a no-op here. -> The full-width hit area comes from Selectable's default size_arg.x == 0 at src/gui/gui_panels.cpp:138-139, which already stretches the item from the current (TreePush-indented) cursor x to the right edge of the content region. ImGuiSelectableFlags_SpanAllColumns has no effect because the tree is not inside a table.

### 17. Main-view tab button in the top bar (one per active MainTab panel: "Renderer", "qpro", …)

- **id**: `main-view-tab-button`
- **input**: left-click
- **path**: `##main/topbar/<tab_label>  (e.g. "##main/topbar/Renderer", "##main/topbar/qpro")`
- **precondition**: BootState == Ready (Build() at src/gui/gui_panels.cpp:468 draws RenderReadyView) AND the collected MainTab panel list has more than one entry (tabs.size() > 1, line 243). The "qpro" tab exists only for game profile slug "iidx33" (panel_registry.cpp:19); "Renderer" is always present.
- **disabled when**: never (never wrapped in BeginDisabled); the whole row is hidden, not disabled, when only one MainTab panel is active
- **effect**: Sets the file-static int g_main_view = i (src/gui/gui_panels.cpp:207,251). RenderReadyView clamps g_main_view and calls tabs[view]->draw() inside BeginChild("main_view") (lines 446-451), so the whole centre body switches to that panel's draw function (Panels::RenderRendererView or Panels::RenderQproTabBody).
- **source**: `src/gui/gui_panels.cpp:251`
- **notes**: ONE entry describing a row drawn in a loop over the collected PanelDesc list (for i in tabs, lines 245-253). The active tab is only styled differently (ImGuiCol_HeaderActive vs ImGuiCol_FrameBg pushed at 248-250) - clicking the already-active tab re-sets the same index and is a no-op. Line 255 auto-resets g_main_view to 0 if the selected index is now >= tabs.size(), which happens when a conditionally-visible tab (qpro) disappears after a profile change.
- **tests**: none
- **audit correction**: Incomplete precondition: it gives the qpro tab's gate as only 'game profile slug "iidx33" (panel_registry.cpp:19)'. CollectActivePanels first selects a PanelSet by App::Global().ActiveBackendId() (src/gui/panel_registry.cpp:114-116) and only kModernPanels (backend id "afp_modern") contains the qpro MainTab entry. -> The tab row exists only when ActiveBackendId() == "afp_modern" (kPanelSets at src/gui/panel_registry.cpp:104-108, set once at boot from Backend::Active()->Id() at src/boot.cpp:102) AND QproTabVisible() is true (slug == "iidx33", panel_registry.cpp:18-20). Under "afp_ddr" and "scene3d" the only MainTab panel is "Renderer" so tabs.size() == 1 and the row is skipped (gui_panels.cpp:243); if backend_id matches no set at all, tabs is left empty and gui_panels.cpp:445 skips the entire main_view body.

### 18. Export status tag SmallButton in the status strip (dynamic label: "capturing N" / "capturing N [NVENC]", "encoding N frames...", "export done", "export failed")

- **id**: `status-export-tag-button`
- **input**: left-click
- **path**: `##main/status_strip/<dynamic label>  (label is the runtime buf: "capturing 120 [NVENC]", "encoding 120 frames...", "export done", "export failed")`
- **precondition**: state.GetExport().phase != App::ExportPhase::Idle - the Idle case returns before drawing anything (src/gui/gui_panels.cpp:313-314)
- **effect**: Calls Export::RequestOpen() (line 319), re-opening the export dialog. The label text and pushed ImGuiCol_Text colour are derived from the current ExportPhase (lines 293-315).
- **source**: `src/gui/gui_panels.cpp:319`
- **tooltip**: yes
- **notes**: ONE entry: the label varies with export phase but it is a single SmallButton call site. Because the ImGui ID is the visible label, the widget's identity changes every frame while the frame counter increments during Capturing/Encoding.
- **tests**: none

### 19. "Open folder" SmallButton next to the export status tag

- **id**: `status-open-folder-button`
- **input**: left-click
- **path**: `##main/status_strip/Open folder`
- **precondition**: ex.phase == App::ExportPhase::Done AND ex.output_path is non-empty - line 331 returns early otherwise
- **disabled when**: never (hidden rather than disabled when the phase/path precondition fails)
- **effect**: Calls NativeDialog::RevealInFileManager(ex.output_path) (line 333), which asks the host OS shell to reveal the exported file in Explorer.
- **source**: `src/gui/gui_panels.cpp:333`
- **tooltip**: yes
- **tests**: `export modal reveals the finished file`, `status strip Open folder reveals the finished export`, `status strip hides the reveal button unless an export finished`

### 20. "Export..." button (export glyph + label) at the right of the top bar

- **id**: `topbar-export-button`
- **input**: left-click
- **path**: `##main/topbar/\xEE\xA2\x98  Export...`
- **precondition**: Ready view is drawn (BootState == Ready)
- **disabled when**: status.scene_loaded == false - wrapped in ImGui::BeginDisabled(!status.scene_loaded) / EndDisabled (lines 271, 275)
- **effect**: Calls Export::RequestOpen() (src/gui/gui_panels.cpp:273), which requests the export modal be opened; the modal itself is drawn later in the same frame by Export::RenderModal() at line 455.
- **source**: `src/gui/gui_panels.cpp:272`
- **tooltip**: yes
- **notes**: Its horizontal position is computed from the measured text widths of the fps string and the button label (lines 266-270), so it re-lays out as the window is resized. The tooltip advertises Ctrl+E, but that chord is NOT handled in this file - it is handled in src/gui/gui_timeline.cpp:195 (IsKeyPressed(ImGuiKey_E) && io.KeyCtrl).
- **tests**: none

### 21. Scrollable IFS tree region

- **id**: `ifs-tree-scroll`
- **input**: scroll
- **path**: `##main/main_view/pane_left/ifs_scroll`
- **precondition**: IFS picker reached the tree (not scanning, list non-empty)
- **effect**: Scrolls the BeginChild("ifs_scroll") region. Created with ImGuiWindowFlags_HorizontalScrollbar (line 195), so both a vertical wheel scroll and a horizontal scrollbar drag / shift-wheel are possible when deeply nested paths overflow the pane width.
- **source**: `src/gui/gui_panels.cpp:195`
- **notes**: Also draggable scrollbars (vertical auto, horizontal explicit).
- **tests**: `browse pane carries the from_arc flag through LoadContent`, `browse pane directory node collapses on click`, `browse pane does not reload the already-active IFS`, `browse pane filter hides non-matching entries` (+3 more)

### 22. Main view body container (the region that hosts whichever MainTab panel is selected)

- **id**: `main-view-body-scroll`
- **input**: scroll
- **path**: `##main/main_view`
- **precondition**: BootState == Ready AND at least one MainTab panel is active (tabs non-empty, line 445)
- **effect**: Scrolls BeginChild("main_view", ImVec2(0, content_h)) whose height is the window's remaining height minus Gui::kStatusStripH and one ItemSpacing (lines 447-448). Default flags, so the wheel and a scrollbar work if the selected panel's content overflows.
- **source**: `src/gui/gui_panels.cpp:449`
- **notes**: Selected panel index comes from std::clamp(g_main_view, 0, tabs.size()-1) at line 446.
- **tests**: `qpro controls explain themselves on hover`, `qpro group checkbox clears just that date group`, `splitter drag moves the boundary between the left and centre panes`

### 23. Centre pane container (scene pane)

- **id**: `pane-center-scroll`
- **input**: scroll
- **path**: `##main/main_view/pane_center`
- **precondition**: Renderer main view is active
- **effect**: Scrolls BeginChild("pane_center", ImVec2(center_w, row_h)) which hosts Panels::RenderScenePane(). Default flags, so wheel/scrollbar work when the scene pane content overflows.
- **source**: `src/gui/gui_panels.cpp:409`
- **notes**: The contents of RenderScenePane are drawn from another file and are outside this surface.
- **tests**: `scene pane Add ignores an empty slot name`, `scene pane Add refuses a duplicate slot path`, `scene pane Add registers a variant slot`, `scene pane filter narrows the layer list` (+3 more)

### 24. Left pane container (Browse pane)

- **id**: `pane-left-scroll`
- **input**: scroll
- **path**: `##main/main_view/pane_left`
- **precondition**: Renderer main view is active
- **effect**: Scrolls BeginChild("pane_left", ImVec2(left_w, row_h)) when its content (the section header + filter box + inner ifs_scroll child) exceeds the pane height. Default child flags, so a vertical scrollbar appears and the wheel scrolls it.
- **source**: `src/gui/gui_panels.cpp:398`
- **notes**: The inner ifs_scroll child (own entry) normally consumes the wheel first when the pointer is over the tree.
- **tests**: `browse pane filter hides non-matching entries`, `browse pane hides the tree while a scan is running`, `browse pane reports an empty catalog`
- **audit correction**: Wrong effect: it states that a vertical scrollbar appears and the wheel scrolls BeginChild("pane_left") when its content exceeds the pane height. pane_left can never overflow - its last child, BeginChild("ifs_scroll", ImVec2(0, 0), ...) at src/gui/gui_panels.cpp:195, is sized 0 on both axes, which in ImGui means 'use the remaining parent space', so pane_left's content always ends exactly at its inner height and ScrollMax.y stays 0. -> pane_left (src/gui/gui_panels.cpp:398) never scrolls and never shows a scrollbar in any of its three states (scanning early-return at 175-180, empty early-return at 181-184, or the tree). All vertical/horizontal scrolling in the Browse pane happens in the nested ifs_scroll child (line 195), which is the ifs-tree-scroll entry.

### 25. Right pane container (inspector pane)

- **id**: `pane-right-scroll`
- **input**: scroll
- **path**: `##main/main_view/pane_right`
- **precondition**: Renderer main view is active
- **effect**: Scrolls BeginChild("pane_right", ImVec2(right_w, row_h)) which hosts Panels::RenderInspectorPane(). Default flags, so wheel/scrollbar work when the inspector tabs overflow.
- **source**: `src/gui/gui_panels.cpp:421`
- **notes**: The contents of RenderInspectorPane are drawn from another file and are outside this surface.
- **tests**: `2D package panel stays hidden while no package is live`, `3D scene panel explains how to load a scene while none is live`, `inspector exposes the ddr backend tab set`, `inspector exposes the modern backend tab set` (+7 more)

### 26. Status strip container at the bottom of the window

- **id**: `status-strip-region`
- **input**: scroll
- **path**: `##main/status_strip`
- **precondition**: BootState == Ready
- **effect**: BeginChild("status_strip", ImVec2(0, Gui::kStatusStripH), 0, ImGuiWindowFlags_NoScrollbar) - fixed height, no scrollbar. Non-interactive readouts inside it: game profile name (line 350), render size WxH from state.GetRenderSize (lines 352-356), and the right-aligned "afp x.y.z" version shown only when state.GetLiveState().have_file_info is true (lines 369-376). None of those three has a tooltip or click handler.
- **source**: `src/gui/gui_panels.cpp:343`
- **notes**: Recorded to document the non-interactive elements and the have_file_info visibility gate on the afp-version readout.
- **tests**: `status strip export tag and reveal button explain themselves`, `status strip export tag reopens the export modal`, `status strip hides the reveal button unless an export finished`, `status strip render error explains itself on hover`

### 27. Top bar container strip

- **id**: `topbar-region`
- **input**: scroll
- **path**: `##main/topbar`
- **precondition**: BootState == Ready
- **effect**: BeginChild("topbar", ImVec2(0, Gui::kTopBarH), border=1, ImGuiWindowFlags_NoScrollbar) - fixed height with no scrollbar, so a wheel over it produces no visible scrolling; only the buttons inside it (tab buttons, Export) react to input. The title text, the loaded-IFS path (PrettifyPath-truncated to 72 chars, lines 259-263) and the fps readout (lines 266-281) are non-interactive text with no tooltip.
- **source**: `src/gui/gui_panels.cpp:234`
- **notes**: Recorded so the inventory is exhaustive about what in the top bar is NOT interactive: the "573Renderer" wordmark, the truncated IFS path, and the fps text have no hover tooltip and no click handler.
- **tests**: `export modal controls all explain themselves`, `export modal stays inside a short window and keeps its footer`, `qpro controls explain themselves on hover`, `setup view Export tooltip is reachable while the button is disabled` (+5 more)

### 28. Browse pane while the IFS scan is running

- **id**: `ifs-picker-scanning-state`
- **input**: state-change
- **path**: `##main/main_view/pane_left`
- **precondition**: state.IsIfsScanning() == true
- **disabled when**: always while IsIfsScanning() is true
- **effect**: RenderIfsPicker prints the live scan status string from state.GetIfsScanStatus() (or a fallback "Scanning for IFS files...") and returns early (lines 175-180), so the filter box and the whole tree are absent - there is nothing to click in the Browse pane during a scan.
- **source**: `src/gui/gui_panels.cpp:175`
- **notes**: Recorded as the visibility gate for ifs-filter-input, ifs-tree-scroll, ifs-tree-file-select and ifs-tree-dir-expand. A second gate at lines 181-184 shows a disabled "No .ifs files found under the game dir." text and likewise suppresses all Browse-pane interaction when ListAvailableIfs() is empty. The header count suffix "(N)" comes from list.size() (lines 171-173) and is not clickable.
- **tests**: `browse pane filter hides non-matching entries`, `browse pane hides the tree while a scan is running`, `browse pane reports an empty catalog`

### 29. IFS path filter text box with hint "filter by path..."

- **id**: `ifs-filter-input`
- **input**: text-entry
- **path**: `##main/main_view/pane_left/##ifsfilter`
- **precondition**: Renderer main view is active; the IFS picker got past both early returns - state.IsIfsScanning() must be false (lines 175-180) and state.ListAvailableIfs() must be non-empty (lines 181-184)
- **effect**: Types into the file-static char filter_buf[128] (line 186). The text is lower-cased into `filter` (lines 189-191) and passed down as lower_filter to RenderIfsTreeNode, which hides any subtree whose file names do not contain it (SubtreeMatchesFilter, lines 117-127) and force-opens all remaining directory nodes via ImGuiTreeNodeFlags_DefaultOpen (line 151). No App::Command is posted; purely local filtering state, and it persists across view switches because the buffer is static.
- **source**: `src/gui/gui_panels.cpp:188`
- **notes**: All normal ImGui text-field keyboard interactions apply here (typing, backspace/delete, Home/End, arrow keys, ctrl+A/C/V/X, Enter/Escape to deactivate). SetNextItemWidth(-FLT_MIN) makes it span the pane, so it re-lays out on splitter drags.
- **tests**: `browse pane filter hides non-matching entries`, `browse pane hides the tree while a scan is running`, `browse pane reports an empty catalog`

### 30. IFS path filter text box - full ImGui text-editing keymap and mouse text selection

- **id**: `ifs-filter-input-editing-keymap` *(audit)*
- **input**: text-entry
- **path**: `##main/main_view/pane_left/##ifsfilter`
- **precondition**: The InputTextWithHint at src/gui/gui_panels.cpp:188 is submitted (not scanning, list non-empty) AND the field is activated by a left-click inside it or by Enter/Space on it under keyboard nav.
- **disabled when**: never; typing simply stops accepting characters once 127 bytes plus the NUL fill filter_buf (sizeof passed at line 188)
- **effect**: ImGui's InputText edit machinery writes the file-static filter_buf[128] (line 186) in place: printable characters insert, Backspace/Delete remove, Home/End and Ctrl+Left/Right move by line/word, Shift+movement extends a selection, Ctrl+A selects all, Ctrl+C/X/V use the OS clipboard, Ctrl+Z/Ctrl+Y undo and redo, Enter deactivates keeping the typed value, Escape deactivates and REVERTS the buffer to the value it held when the field was activated. With the mouse, click positions the caret, click-drag selects a range and double-click selects a word. Every one of these mutations changes `filter` on the very next lines (189-191) and therefore re-filters the whole tree that same frame via SubtreeMatchesFilter (117-127) and re-applies ImGuiTreeNodeFlags_DefaultOpen (line 151).
- **source**: `src/gui/gui_panels.cpp:188`
- **notes**: The existing ifs-filter-input entry mentions this only inside its notes field and only for typing; per the checklist item 'text fields accept the full editing keymap' it needs its own record. Escape-reverts and Ctrl+Z are genuinely distinct effects from 'text-entry' - both can silently restore a previous filter and re-expand the tree.
- **tests**: `browse pane filter hides non-matching entries`, `browse pane hides the tree while a scan is running`, `browse pane reports an empty catalog`

### 31. Whole ready-view shell visibility

- **id**: `ready-vs-setup-gate`
- **input**: window-message
- **precondition**: App::Global().GetBootState() == App::BootState::Ready
- **disabled when**: BootState != Ready - the entire top bar / browse tree / status strip surface is replaced by the setup view
- **effect**: Build() calls RenderReadyView() when boot state is Ready, otherwise Panels::Setup::RenderView() (lines 468-472); in both cases Panels::LoadingOverlay::Render(App::Global().GetLoadProgress()) is drawn on top (line 474), and Gui::ApplyAccentForProfile(GetGameProfileSlug()) restyles the UI accent each frame (line 463). While the loading overlay is up it covers this surface.
- **source**: `src/gui/gui_panels.cpp:468`
- **notes**: Not a widget: recorded because it is the gate that makes every other entry on this surface reachable or unreachable. The setup view and loading overlay are separate surfaces in other files.

