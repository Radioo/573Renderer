# qpro panel

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `cat-back` | Back category checkbox | checkbox | - | 1 |
| 2 | `cat-body` | Body category checkbox | checkbox | - | 2 |
| 3 | `cat-face` | Face category checkbox | checkbox | - | **none** |
| 4 | `cat-hair` | Hair category checkbox | checkbox | - | **none** |
| 5 | `cat-hand` | Hand category checkbox | checkbox | - | **none** |
| 6 | `cat-head` | Head category checkbox | checkbox | - | 4 |
| 7 | `group-select-checkbox` | Per-date-group select-all checkbox | checkbox | - | 2 |
| 8 | `hue-scope-checkbox` | Keep static base fills un-hue-shifted | checkbox | yes | 2 |
| 9 | `part-checkbox` | Individual scanned part checkbox | checkbox | - | **none** |
| 10 | `animated-parts-note-tooltip` | "Animated parts -> .webm (VP9) + .mp4 (HEVC-alpha, Safari) + .avif poster" label (hover tooltip) | hover | yes | **none** |
| 11 | `hue-scope-tooltip` | Keep static base fills un-hue-shifted (hover tooltip) | hover | yes | 2 |
| 12 | `output-fps-tooltip` | Output fps (hover tooltip) | hover | yes | 2 |
| 13 | `scan-button-tooltip` | Scan parts from bm2dx.dll (hover tooltip) | hover | yes | 1 |
| 14 | `output-fps-keyboard-edit` *(audit)* | Output fps text field, keyboard editing keymap | key | - | **none** |
| 15 | `extract-button` | Choose output folder + extract... | left-click | - | 6 |
| 16 | `folder-picker-dialog` | Native "browse for folder" shell dialog raised by the extract button | left-click | - | **none** |
| 17 | `group-treenode` | Per-date-group tree node (date - N parts (M selected)) | left-click | - | 1 |
| 18 | `issues-copy-list` | Copy list (copy skipped/failed part report) | left-click | - | 1 |
| 19 | `main-tab-renderer-select` *(audit)* | "Renderer" main tab button (leaves the qpro panel) | left-click | - | 2 |
| 20 | `output-fps-minus` | Output fps decrement step button | left-click | - | **none** |
| 21 | `output-fps-plus` | Output fps increment step button | left-click | - | **none** |
| 22 | `qpro-tab-select` | "qpro" main tab | left-click | - | 3 |
| 23 | `scan-button` | Scan parts from bm2dx.dll | left-click | yes | 1 |
| 24 | `select-all-parts` | All (select every scanned part) | left-click | - | 3 |
| 25 | `select-no-parts` | None (deselect every scanned part) | left-click | - | 2 |
| 26 | `issues-list-scroll` | Skipped/failed issues list scroll region | scroll | - | **none** |
| 27 | `parts-list-scroll` | Scanned parts list scroll region | scroll | - | 2 |
| 28 | `qpro-body-scroll` *(audit)* | qpro tab body scroll region ("main_view" child window) | scroll | - | 3 |
| 29 | `output-fps-text` | Output fps (numeric text field) | text-entry | yes | 2 |

## Detail

### 1. Back category checkbox

- **id**: `cat-back`
- **input**: checkbox
- **path**: `Back`
- **precondition**: always
- **effect**: Toggles s_ex_back; copied into AfpCmd::QproStartExtract::parts.back on extract, and feeds the any_cat gate
- **source**: `src/gui/gui_qpro_panel.cpp:270`
- **notes**: Default true. Unchecking all six shows the "Select at least one category to extract." warning and disables the extract button.
- **tests**: `qpro extract posts the selected options`

### 2. Body category checkbox

- **id**: `cat-body`
- **input**: checkbox
- **path**: `Body`
- **precondition**: always
- **effect**: Toggles s_ex_body; copied into AfpCmd::QproStartExtract::parts.body on extract, and feeds the any_cat gate
- **source**: `src/gui/gui_qpro_panel.cpp:268`
- **notes**: Default true. Body assembly only actually works when App::State render size is 520x704 (checked and reported by DrawQproIntro at gui_qpro_panel.cpp:206-218), but the checkbox is never disabled for it.
- **tests**: `qpro None then All flip the whole part selection`, `qpro group checkbox clears just that date group`

### 3. Face category checkbox

- **id**: `cat-face`
- **input**: checkbox
- **path**: `Face`
- **precondition**: always
- **effect**: Toggles s_ex_face; copied into AfpCmd::QproStartExtract::parts.face on extract, and feeds the any_cat gate
- **source**: `src/gui/gui_qpro_panel.cpp:266`
- **notes**: Default true.
- **tests**: none

### 4. Hair category checkbox

- **id**: `cat-hair`
- **input**: checkbox
- **path**: `Hair`
- **precondition**: always
- **effect**: Toggles s_ex_hair; copied into AfpCmd::QproStartExtract::parts.hair on extract, and feeds the any_cat gate
- **source**: `src/gui/gui_qpro_panel.cpp:264`
- **notes**: Default true.
- **tests**: none

### 5. Hand category checkbox

- **id**: `cat-hand`
- **input**: checkbox
- **path**: `Hand`
- **precondition**: always
- **effect**: Toggles s_ex_hand; copied into AfpCmd::QproStartExtract::parts.hand on extract, and feeds the any_cat gate
- **source**: `src/gui/gui_qpro_panel.cpp:262`
- **notes**: Default true.
- **tests**: none

### 6. Head category checkbox

- **id**: `cat-head`
- **input**: checkbox
- **path**: `Head`
- **precondition**: always
- **effect**: Toggles s_ex_head; copied into AfpCmd::QproStartExtract::parts.head on extract, and feeds the any_cat gate on the extract button
- **source**: `src/gui/gui_qpro_panel.cpp:260`
- **notes**: Default true.
- **tests**: `qpro category checkboxes gate the extract button`, `qpro extract is abandoned when the folder picker is cancelled`, `qpro output fps clamps to the accepted range`, `qpro tab warns unless the render size matches the avatar`

### 7. Per-date-group select-all checkbox

- **id**: `group-select-checkbox`
- **input**: checkbox
- **path**: `qpro_parts/<group index>/##grp`
- **precondition**: A scan has completed; one such checkbox exists per entry in s_groups (one per distinct source-IFS date, plus a "(no source IFS / unknown date)" bucket)
- **effect**: Sets s_checked[pi] = new value for every part index in that group (whole date group selected/deselected at once)
- **source**: `src/gui/gui_qpro_panel.cpp:126`
- **notes**: ONE entry describing a row drawn in a loop over s_groups (DrawPartGroup called from gui_qpro_panel.cpp:177 under ImGui::PushID((int)gi)). Its displayed state is computed as "all parts in group checked".
- **tests**: `qpro group checkbox clears just that date group`, `qpro part groups appear once a scan publishes parts`

### 8. Keep static base fills un-hue-shifted

- **id**: `hue-scope-checkbox`
- **input**: checkbox
- **path**: `Keep static base fills un-hue-shifted`
- **precondition**: always (qpro tab visible)
- **effect**: Toggles file-static bool s_scope_hue; the value is copied into AfpCmd::QproStartExtract::hue_scope when the extract button is pressed
- **source**: `src/gui/gui_qpro_panel.cpp:223`
- **tooltip**: yes
- **notes**: Default true.
- **tests**: `qpro extract posts the selected options`, `qpro tab warns unless the render size matches the avatar`

### 9. Individual scanned part checkbox

- **id**: `part-checkbox`
- **input**: checkbox
- **path**: `qpro_parts/<group index>/node/<part index>/<part label>`
- **precondition**: A scan has completed AND the enclosing date-group TreeNode is expanded AND the row is inside the ImGuiListClipper's visible range
- **effect**: Sets s_checked[pi] for that part; BuildSelection() later maps s_checked into QproExtract::PartSelection.sel[category][index] passed as AfpCmd::QproStartExtract::part_sel
- **source**: `src/gui/gui_qpro_panel.cpp:141`
- **notes**: ONE entry for a row drawn in a loop over the group's parts; label is s_parts[pi].label, scoped by ImGui::PushID(pi).
- **tests**: none

### 10. "Animated parts -> .webm (VP9) + .mp4 (HEVC-alpha, Safari) + .avif poster" label (hover tooltip)

- **id**: `animated-parts-note-tooltip`
- **input**: hover
- **path**: `Animated parts -> .webm (VP9) + .mp4 (HEVC-alpha, Safari) + .avif poster`
- **precondition**: always
- **effect**: ImGui::SetTooltip explaining the three output files produced for animated parts (transparent WebM/VP9, HEVC-with-alpha MP4 for Safari, static AVIF poster) and that static parts export a lone AVIF
- **source**: `src/gui/gui_qpro_panel.cpp:247`
- **tooltip**: yes
- **notes**: IsItemHovered() on a TextDisabled item; the text itself is not clickable, only hoverable.
- **tests**: none

### 11. Keep static base fills un-hue-shifted (hover tooltip)

- **id**: `hue-scope-tooltip`
- **input**: hover
- **path**: `Keep static base fills un-hue-shifted`
- **precondition**: always
- **effect**: ImGui::SetTooltip explaining that effect parts contain a rainbow effect fill plus a static gold base fill, that afp hue-shifts both while the live game shifts only the rainbow, and what On vs Off does
- **source**: `src/gui/gui_qpro_panel.cpp:224`
- **tooltip**: yes
- **notes**: IsItemHovered() on the preceding Checkbox.
- **tests**: `qpro extract posts the selected options`, `qpro tab warns unless the render size matches the avatar`

### 12. Output fps (hover tooltip)

- **id**: `output-fps-tooltip`
- **input**: hover
- **path**: `Output fps`
- **precondition**: always
- **effect**: ImGui::SetTooltip explaining that this is the encode/sample rate for exported clips, that animations play at native game speed while backgrounds are real-time-sampled to this rate, and that 60 matches the avatar
- **source**: `src/gui/gui_qpro_panel.cpp:237`
- **tooltip**: yes
- **notes**: IsItemHovered() on the InputInt.
- **tests**: `qpro extract posts the selected options`, `qpro output fps clamps to the accepted range`

### 13. Scan parts from bm2dx.dll (hover tooltip)

- **id**: `scan-button-tooltip`
- **input**: hover
- **path**: `Scan parts from bm2dx.dll`
- **precondition**: always (fires even while the button is disabled, since IsItemHovered is checked after EndDisabled)
- **effect**: ImGui::SetTooltip explaining that scanning enumerates every qpro part read directly from bm2dx.dll, groups them by source IFS modified date so only recent-update parts can be rendered, and that the JSON manifests always cover the full part set
- **source**: `src/gui/gui_qpro_panel.cpp:99`
- **tooltip**: yes
- **notes**: IsItemHovered() is evaluated after ImGui::EndDisabled(), so the tooltip is reachable in the disabled state too.
- **tests**: `qpro scan button posts the scan command`

### 14. Output fps text field, keyboard editing keymap

- **id**: `output-fps-keyboard-edit` *(audit)*
- **input**: key
- **path**: `Output fps/#`
- **precondition**: The InputInt's text box is activated (click into it, or double-click/ctrl+click the number)
- **effect**: ImGui's standard InputText keymap applies to the fps field: character entry is filtered to ImGuiInputTextFlags_CharsDecimal by InputScalar; Ctrl+A selects all, Ctrl+C/Ctrl+X/Ctrl+V copy/cut/paste, Left/Right/Home/End/shift-selection move the caret, Backspace/Delete edit. Because InputScalar has no EnterReturnsTrue, DataTypeApplyFromText runs on EVERY keystroke, so s_qpro_fps (and therefore the [1,240] clamp at gui_qpro_panel.cpp:235-236) is applied per character while the text box still shows the raw typed text; the box is reformatted from the clamped value only after deactivation. Enter or Tab or clicking away commits and deactivates. Escape aborts the edit and restores the value the field had when it was activated.
- **source**: `src/gui/gui_qpro_panel.cpp:234`
- **notes**: The inner text box submitted by InputScalar has the empty label "" inside PushID("Output fps"), so its ImGui path element is the InputText id, not "Output fps" itself. While this field is active ImGuiIO::WantTextInput is true - relevant because the app's global shortcut handler bails on WantTextInput (src/gui/gui_timeline.cpp:192), though that handler is not reachable from the qpro tab anyway (it is called from RenderTimelineDock inside RenderRendererView, gui_panels.cpp:425).
- **tests**: none

### 15. Choose output folder + extract...

- **id**: `extract-button`
- **input**: left-click
- **path**: `Choose output folder + extract...`
- **precondition**: always visible
- **disabled when**: QproExtract::GetStatus().running OR no category checked (!any_cat) OR nsel == 0 (all scanned parts deselected). Note nsel is -1 when no scan has been run, which counts as "any_part" true
- **effect**: Opens the native folder picker via NativeDialog::BrowseForFolder(Gui::GetHwnd(), ""); if a folder is returned, builds AfpCmd::QproStartExtract{out_dir, hue_scope=s_scope_hue, fps=s_qpro_fps, parts.head/hand/hair/face/body/back from the category checkboxes, and part_sel=BuildSelection() only when nsel >= 0} and calls state.PostCommand(AfpCmd::Wrap(...))
- **source**: `src/gui/gui_qpro_panel.cpp:283`
- **notes**: Fixed size ImVec2(280,32). If no scan was ever run (nsel == -1) part_sel is omitted, so the whole part set is extracted.
- **tests**: `qpro None then All flip the whole part selection`, `qpro category checkboxes gate the extract button`, `qpro extract is abandoned when the folder picker is cancelled`, `qpro extract posts the selected options` (+2 more)

### 16. Native "browse for folder" shell dialog raised by the extract button

- **id**: `folder-picker-dialog`
- **input**: left-click
- **precondition**: Reachable only after clicking "Choose output folder + extract..."
- **effect**: NativeDialog::BrowseForFolder returns the chosen path (extraction command is posted) or an empty string on cancel, in which case nothing is posted and the panel returns to idle. It is modal to the app HWND (Gui::GetHwnd()), so the ImGui frame is blocked while it is open
- **source**: `src/gui/gui_qpro_panel.cpp:284`
- **notes**: Win32 shell dialog, not an ImGui widget; the cancel path is a distinct user outcome (no command posted).
- **tests**: none

### 17. Per-date-group tree node (date - N parts (M selected))

- **id**: `group-treenode`
- **input**: left-click
- **path**: `qpro_parts/<group index>/node`
- **precondition**: A scan has completed; one per s_groups entry
- **effect**: ImGui::TreeNode expand/collapse; when open, the group's part checkboxes are emitted through an ImGuiListClipper over g.parts
- **source**: `src/gui/gui_qpro_panel.cpp:132`
- **notes**: ONE entry for a looped row. ID is the literal "node"; the visible text is the formatted date/count. Default ImGui behaviour also toggles on arrow click and on double-click of the label.
- **tests**: `qpro part groups appear once a scan publishes parts`

### 18. Copy list (copy skipped/failed part report)

- **id**: `issues-copy-list`
- **input**: left-click
- **path**: `Copy list`
- **precondition**: Extraction status has a non-empty issues vector, i.e. either st.running (progress view) or st.finished with no error - DrawQproIssues returns immediately when st.issues is empty
- **effect**: Concatenates every issue as "FAILED: <text>" / "skipped: <text>" lines and calls ImGui::SetClipboardText with the result
- **source**: `src/gui/gui_qpro_panel.cpp:32`
- **notes**: DrawQproIssues is called from both the running branch (gui_qpro_panel.cpp:310) and the finished branch (gui_qpro_panel.cpp:323), but only one of those runs per frame, so the button appears at most once.
- **tests**: `qpro issue list offers a clipboard copy`

### 19. "Renderer" main tab button (leaves the qpro panel)

- **id**: `main-tab-renderer-select` *(audit)*
- **input**: left-click
- **path**: `##main/topbar/Renderer`
- **precondition**: Boot state is App::BootState::Ready AND more than one MainTab panel is active (tabs.size() > 1 at src/gui/gui_panels.cpp:243), which is exactly the iidx33 + afp_modern case that makes the qpro tab exist in the first place
- **effect**: Sets g_main_view = 0, so RenderReadyView draws Panels::RenderRendererView instead of Panels::RenderQproTabBody in the "main_view" child. This is the only way to leave the qpro panel; all qpro panel state (s_scope_hue, s_qpro_fps, the six category bools, s_parts/s_checked/s_groups, s_scan_gen) is file-static and survives the switch, and a running extract keeps running because it lives on the render thread via the posted AfpCmd.
- **source**: `src/gui/gui_panels.cpp:251`
- **notes**: Same ImGui::Button call site as the qpro tab button, different loop index i (the buttons are emitted in a for loop over the CollectActivePanels result). Not a duplicate of qpro-tab-select: different control label/ID and the opposite effect.
- **tests**: `top bar hides the view switch when only one main panel is active`, `top bar view switch appears for iidx33 and swaps the main panel`

### 20. Output fps decrement step button

- **id**: `output-fps-minus`
- **input**: left-click
- **path**: `Output fps/-`
- **precondition**: always
- **effect**: Decrements s_qpro_fps by step 1 (step_fast 10 when Ctrl-clicked), then the clamp to [1,240] runs
- **source**: `src/gui/gui_qpro_panel.cpp:234`
- **notes**: Implicit ImGui::InputInt(step=1, step_fast=10) button; ImGui pushes the label as ID and names the button "-".
- **tests**: none

### 21. Output fps increment step button

- **id**: `output-fps-plus`
- **input**: left-click
- **path**: `Output fps/+`
- **precondition**: always
- **effect**: Increments s_qpro_fps by step 1 (step_fast 10 when Ctrl-clicked), then the clamp to [1,240] runs
- **source**: `src/gui/gui_qpro_panel.cpp:234`
- **notes**: Implicit ImGui::InputInt step button named "+".
- **tests**: none

### 22. "qpro" main tab

- **id**: `qpro-tab-select`
- **input**: left-click
- **path**: `qpro`
- **precondition**: Active backend is "afp_modern" AND App::Global().GetGameProfileSlug() == "iidx33" (PanelDesc.visible = QproTabVisible); otherwise the tab is not emitted at all
- **disabled when**: never (when present)
- **effect**: Selects the qpro MainTab panel, causing Panels::RenderQproTabBody() to be the drawn body for the main tab area
- **source**: `src/gui/panel_registry.cpp:37`
- **notes**: Gate for every other entry on this surface. The tab item itself is created by the generic tab-bar loop from PanelDesc.tab_label; registry entry id is "qpro_view".
- **tests**: `qpro controls explain themselves on hover`, `top bar hides the view switch when only one main panel is active`, `top bar view switch appears for iidx33 and swaps the main panel`
- **audit correction**: Wrong control type, wrong source line and wrong imgui path, plus an incomplete precondition. src/gui/panel_registry.cpp:37 is the .tab_label field of a constexpr PanelDesc table entry, not an interactive control, and there is NO tab bar for main tabs - panel_registry.cpp contains no ImGui calls at all (the only BeginTabBar/BeginTabItem in the GUI is the inspector's, src/gui/gui_inspector.cpp:444-446, which does not carry MainTab panels). The note "the tab item itself is created by the generic tab-bar loop" is therefore wrong. The precondition also omits the app-level boot gate. -> The control is a plain ImGui::Button labelled with PanelDesc::tab_label, emitted by the loop in RenderTopBar: `if (ImGui::Button(tabs[i]->tab_label)) g_main_view = (int)i;` at src/gui/gui_panels.cpp:251, inside ImGui::BeginChild("topbar", ...) at gui_panels.cpp:234. source = src/gui/gui_panels.cpp:251; imgui_path = "##main/topbar/qpro"; input remains left-click. Effect: sets the file-static int g_main_view (declared src/gui/gui_panels.cpp:207) to this panel's index; RenderReadyView then clamps it and calls tabs[view]->draw() (= Panels::RenderQproTabBody) inside ImGui::BeginChild("main_view", ...) at gui_panels.cpp:449-451. Full precondition: App::Global().GetBootState() == App::BootState::Ready, otherwise Build() draws Panels::Setup::RenderView() and no top bar exists at all (src/gui/gui_panels.cpp:468-471); AND ActiveBackendId() == "afp_modern" (kPanelSets match, panel_registry.cpp:104-118); AND GetGameProfileSlug() == "iidx33" (QproTabVisible, panel_registry.cpp:18-20); AND tabs.size() > 1 (gui_panels.cpp:243), which holds here because kModernPanels contributes Renderer + qpro to the MainTab slot. Also note the button is styled active/inactive via ImGuiCol_HeaderActive vs ImGuiCol_FrameBg (gui_panels.cpp:247-250) but is never disabled.

### 23. Scan parts from bm2dx.dll

- **id**: `scan-button`
- **input**: left-click
- **path**: `Scan parts from bm2dx.dll`
- **precondition**: always visible
- **disabled when**: busy (QproExtract::GetStatus().running, passed in as `busy`) OR scan.running - wrapped in ImGui::BeginDisabled(busy \|\| scan.running)
- **effect**: Calls QproExtract::MarkScanRunning() then App::Global().PostCommand(AfpCmd::Wrap(AfpCmd::QproStartScan{})) to start the background bm2dx.dll part scan
- **source**: `src/gui/gui_qpro_panel.cpp:94`
- **tooltip**: yes
- **notes**: While scan.running a "scanning bm2dx.dll ..." TextDisabled is shown SameLine; on failure a red "Scan failed: <err>" line appears (both non-interactive).
- **tests**: `qpro scan button posts the scan command`
- **audit correction**: Incomplete/misleading effect: it only lists the PostCommand, and omits the destructive side effect that lands on the GUI when the scan finishes. That side effect silently discards the user's per-part selection, which changes the meaning of every other part-list entry. -> Effect should also state the completion path: on the first frame after the background scan completes, DrawPartScanList sees scan.done && scan.generation != s_scan_gen (src/gui/gui_qpro_panel.cpp:152-157) and then (a) latches s_scan_gen = scan.generation, (b) replaces s_parts with scan.parts, (c) calls s_checked.assign(s_parts.size(), 1), i.e. RESETS every part checkbox to checked and throws away any manual per-part / per-date-group selection made before the rescan, and (d) calls RebuildGroups() (gui_qpro_panel.cpp:63-78), rebuilding s_groups by source-IFS date (unknown-date bucket "(no source IFS / unknown date)" sorted last, other dates descending). As a knock-on effect the return value of DrawPartScanList flips from -1 (no scan yet, extract sends the whole part set because `if (nsel >= 0) r.part_sel = BuildSelection();` at gui_qpro_panel.cpp:296 is skipped) to the full part count, from which point on the extract button always sends an explicit PartSelection.

### 24. All (select every scanned part)

- **id**: `select-all-parts`
- **input**: left-click
- **path**: `All`
- **precondition**: A scan has completed and s_parts is non-empty (DrawPartScanList returns early at gui_qpro_panel.cpp:161 otherwise)
- **disabled when**: never (not wrapped in BeginDisabled; remains clickable even while an extract is running)
- **effect**: std::ranges::fill(s_checked, 1) - marks every scanned part selected, which changes the "%d / %zu parts selected" count and the PartSelection built by BuildSelection()
- **source**: `src/gui/gui_qpro_panel.cpp:169`
- **notes**: SmallButton placed SameLine after the selected-count text.
- **tests**: `qpro None then All flip the whole part selection`, `qpro group checkbox clears just that date group`, `qpro scan failure is surfaced without a part list`

### 25. None (deselect every scanned part)

- **id**: `select-no-parts`
- **input**: left-click
- **path**: `None`
- **precondition**: A scan has completed and s_parts is non-empty
- **effect**: std::ranges::fill(s_checked, 0) - clears all part selection; nsel becomes 0 which disables the extract button via the any_part gate
- **source**: `src/gui/gui_qpro_panel.cpp:171`
- **notes**: SmallButton placed SameLine after "All".
- **tests**: `qpro None then All flip the whole part selection`, `qpro scan failure is surfaced without a part list`

### 26. Skipped/failed issues list scroll region

- **id**: `issues-list-scroll`
- **input**: scroll
- **path**: `qpro_issues`
- **precondition**: Extraction status has a non-empty issues vector
- **effect**: Scrolls the bordered fixed-height (0,170) child window listing every "[FAIL] ..." / "[skip] ..." line; ImGuiWindowFlags_HorizontalScrollbar means it scrolls horizontally too for long lines
- **source**: `src/gui/gui_qpro_panel.cpp:40`
- **notes**: ImGui::BeginChild("qpro_issues", ImVec2(0,170), border=1, ImGuiWindowFlags_HorizontalScrollbar). Both scrollbars are also drag-scrollable.
- **tests**: none

### 27. Scanned parts list scroll region

- **id**: `parts-list-scroll`
- **input**: scroll
- **path**: `qpro_parts`
- **precondition**: A scan has completed and s_parts is non-empty
- **effect**: Scrolls the bordered fixed-height (0,300) child window containing the per-date part groups; content is clipper-driven so scrolling changes which part checkboxes are instantiated
- **source**: `src/gui/gui_qpro_panel.cpp:175`
- **notes**: ImGui::BeginChild("qpro_parts", ImVec2(0,300), border=1). Also drag-scrollable via its scrollbar.
- **tests**: `qpro group checkbox clears just that date group`, `qpro part groups appear once a scan publishes parts`

### 28. qpro tab body scroll region ("main_view" child window)

- **id**: `qpro-body-scroll` *(audit)*
- **input**: scroll
- **path**: `##main/main_view`
- **precondition**: qpro tab selected (g_main_view points at the qpro PanelDesc); the child is created unconditionally around tabs[view]->draw()
- **effect**: Scrolls the child window that wraps the ENTIRE qpro panel body (ImGui::BeginChild("main_view", ImVec2(0, content_h), 0) - no NoScrollbar flag, so a vertical scrollbar appears whenever the body exceeds content_h). The qpro body is intro text + options + category row + a fixed 300px parts child + extract button + status/issues, which overflows a normal window height, so the extract button, the progress bar and the issues list are only reachable after scrolling this outer region. Mouse wheel over the panel background, or dragging its scrollbar, moves it.
- **source**: `src/gui/gui_panels.cpp:449`
- **notes**: Distinct from the two inner children already inventoried (qpro_parts at gui_qpro_panel.cpp:175 and qpro_issues at gui_qpro_panel.cpp:40). Wheel over qpro_parts scrolls that inner child first and only chains to main_view at its scroll limit, so an automated driver must target the outer region explicitly. content_h = GetContentRegionAvail().y - kStatusStripH - ItemSpacing.y (gui_panels.cpp:447-448).
- **tests**: `qpro controls explain themselves on hover`, `qpro group checkbox clears just that date group`, `splitter drag moves the boundary between the left and centre panes`

### 29. Output fps (numeric text field)

- **id**: `output-fps-text`
- **input**: text-entry
- **path**: `Output fps`
- **precondition**: always
- **effect**: ImGui::InputInt writes s_qpro_fps, then it is clamped to [1,240] with std::max/std::min the same frame; the value is copied into AfpCmd::QproStartExtract::fps on extract
- **source**: `src/gui/gui_qpro_panel.cpp:234`
- **tooltip**: yes
- **notes**: Item width forced to 120 via SetNextItemWidth. Default 60. Typing an out-of-range number is silently clamped on the next frame.
- **tests**: `qpro extract posts the selected options`, `qpro output fps clamps to the accepted range`

