# Shared widgets and gating

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `vsplitter-double-click-no-reset` *(audit)* | Double-clicking a splitter bar | double-click | - | **none** |
| 2 | `inspector-tab-reorder-blocked` | Dragging an inspector tab to reorder it | drag | - | **none** |
| 3 | `overlay-move-resize-collapse-blocked` | Dragging / resizing / collapsing the loading overlay window | drag | - | **none** |
| 4 | `segmented-wrap-on-pane-resize` *(audit)* | Segmented control row re-flowing when the host pane is narrowed | drag | - | **none** |
| 5 | `vsplitter-drag` | Vertical splitter bar (InvisibleButton) | drag | - | **none** |
| 6 | `vsplitter-drag-left-instance` | Left splitter between the IFS picker pane and the centre scene pane | drag | - | 1 |
| 7 | `vsplitter-drag-right-instance` | Right splitter between the centre scene pane and the inspector pane | drag | - | **none** |
| 8 | `section-header-no-interaction` | Gui::SectionHeader (icon + label + suffix + Separator) | hover | - | **none** |
| 9 | `segmented-item-hover-highlight` | Segmented control segment button hover/press feedback | hover | - | **none** |
| 10 | `segmented-last-item-tooltip` | Segmented control: hovering the LAST segment surfaces the caller's tooltip | hover | yes | **none** |
| 11 | `vsplitter-hover-cursor` | Splitter hover cursor | hover | - | **none** |
| 12 | `inspector-tab-keyboard-activate` *(audit)* | Inspector tab item reached via keyboard navigation | key | - | **none** |
| 13 | `main-tab-button-keyboard-activate` *(audit)* | Main view tab button in the top bar, reached via keyboard navigation | key | - | **none** |
| 14 | `segmented-item-keyboard-activate` | Segmented segment button via keyboard navigation | key | - | **none** |
| 15 | `vsplitter-keyboard-activate` | Splitter InvisibleButton reached via keyboard nav | key | - | **none** |
| 16 | `inspector-tab-gc2d` | Inspector tab "2D package" | left-click | - | **none** |
| 17 | `inspector-tab-live-ddr` | Inspector tab "Live" (DDR backend) | left-click | - | **none** |
| 18 | `inspector-tab-live-modern` | Inspector tab "Live" (modern backend) | left-click | - | **none** |
| 19 | `inspector-tab-properties` | Inspector tab "Properties" | left-click | - | **none** |
| 20 | `inspector-tab-render-ddr` | Inspector tab "Render" (DDR backend variant) | left-click | - | **none** |
| 21 | `inspector-tab-render-modern` | Inspector tab "Render" (modern backend variant) | left-click | - | **none** |
| 22 | `inspector-tab-scene3d-ddr` | Inspector tab "3D scene" (DDR backend) | left-click | - | **none** |
| 23 | `inspector-tab-scene3d-standalone` | Inspector tab "3D scene" (scene3d backend) | left-click | - | **none** |
| 24 | `inspector-tab-select` | Inspector tab item (one per InspectorTab panel) | left-click | - | **none** |
| 25 | `main-tab-button-click` | Main view tab button in the top bar (one per MainTab panel) | left-click | - | **none** |
| 26 | `main-tab-qpro` | Main tab "qpro" | left-click | - | **none** |
| 27 | `main-tab-renderer` | Main tab "Renderer" | left-click | - | **none** |
| 28 | `mc-name-type-segmented-show-gate` *(audit)* | Segmented instance "##mc_name_type" is conditionally hidden | left-click | - | **none** |
| 29 | `overlay-click-through` | Clicking anywhere on the loading overlay | left-click | - | **none** |
| 30 | `panel-set-unknown-backend` | Effect of an unrecognised active backend id on every tab on this surface | left-click | - | **none** |
| 31 | `segmented-active-item-click-noop` | Segmented control: the currently selected segment | left-click | - | **none** |
| 32 | `segmented-current-out-of-range` *(audit)* | Segmented control when *current is outside [0, count) | left-click | - | **none** |
| 33 | `segmented-item-click` | Segmented control segment button (one per item) | left-click | - | **none** |
| 34 | `vsplitter-press-no-move` | Splitter click without moving the mouse | left-click | - | **none** |
| 35 | `main-view-body-child-scroll` *(audit)* | Main view body host child ("main_view") that renders the selected MainTab panel | scroll | - | **none** |
| 36 | `overlay-scroll-blocked` | Scrolling over the loading overlay | scroll | - | **none** |
| 37 | `overlay-visibility-gate` | Loading overlay appearance / disappearance | window-message | - | n/a |

## Detail

### 1. Double-clicking a splitter bar

- **id**: `vsplitter-double-click-no-reset` *(audit)*
- **input**: double-click
- **path**: `##main / main_view / ##split_l (or ##split_r)`
- **precondition**: Mouse over either splitter rect (kSplitterW = 6.0f wide, gui_layout_constants.h:12) and double-clicked
- **disabled when**: always inert: VSplitter reads only ImGui::IsItemActive() (gui_splitter.cpp:11), ImGui::IsItemHovered() (:12) and ImGui::GetIO().MouseDelta.x (:28). There is no IsMouseDoubleClicked / IsMouseClicked handling anywhere in the file.
- **effect**: Nothing beyond the ordinary press feedback: the bar turns ImGuiCol_SeparatorActive and the ResizeEW cursor is forced for the frames the button is held. Pane widths are NOT reset to kPaneLeftDefault (300) / kPaneRightDefault (340) (gui_layout_constants.h:9-10). Because left_w / right_w are function-local statics (gui_panels.cpp:387-388) that are never persisted and never reset, the only way back to the defaults is restarting the process.
- **source**: `src/gui/gui_splitter.cpp:28`
- **notes**: Double-click-to-reset is the standard splitter idiom in most tools, so its absence is a real user-facing gap and belongs in the inventory alongside the other deliberately-recorded no-op gestures (inspector-tab-reorder-blocked, overlay-move-resize-collapse-blocked).
- **tests**: none

### 2. Dragging an inspector tab to reorder it

- **id**: `inspector-tab-reorder-blocked`
- **input**: drag
- **path**: `##inspector_tabs`
- **precondition**: Inspector tab bar visible
- **disabled when**: always: BeginTabBar is created with only ImGuiTabBarFlags_FittingPolicyResizeDown, without ImGuiTabBarFlags_Reorderable, so tabs cannot be dragged; there is also no close button (no p_open passed to BeginTabItem)
- **effect**: Nothing. Tab order is fixed by the descriptor array order in panel_registry.cpp, and tabs shrink to fit rather than scrolling.
- **source**: `src/gui/gui_inspector.cpp:444`
- **notes**: Recorded so the inventory covers the gestures that a tab bar normally offers but this one does not.
- **tests**: none

### 3. Dragging / resizing / collapsing the loading overlay window

- **id**: `overlay-move-resize-collapse-blocked`
- **input**: drag
- **path**: `##loading_overlay`
- **precondition**: Overlay visible
- **disabled when**: always: ImGuiWindowFlags_NoTitleBar \| NoResize \| NoMove \| NoCollapse \| NoSavedSettings are all set, and SetNextWindowPos/Size are re-applied every frame
- **effect**: Nothing. There is no title bar to grab or double-click, no resize grip, no collapse arrow, and no close button; window geometry is pinned to the main viewport work area each frame.
- **source**: `src/gui/gui_loading_overlay.cpp:115`
- **notes**: Covers the usual window gestures (title-bar drag, edge drag, double-click-to-collapse) in one entry since they share a single cause.
- **tests**: none

### 4. Segmented control row re-flowing when the host pane is narrowed

- **id**: `segmented-wrap-on-pane-resize` *(audit)*
- **input**: drag
- **path**: `<caller id> / <i> / <items[i]>`
- **precondition**: A Segmented control is drawn in the inspector pane and the user drags ##split_r (or the OS window edge) so that ImGui::GetContentRegionAvail().x drops below the next segment's width
- **disabled when**: the first segment (i == 0) never wraps, because the SameLine / NewLine block at gui_widgets.cpp:15-18 is guarded by `if (i > 0)`; so with an extremely narrow pane a long first label is clipped rather than moved
- **effect**: For each i > 0, ImGui::SameLine() is issued and then ImGui::NewLine() if GetContentRegionAvail().x < CalcTextSize(items[i]).x + 2*FramePadding.x (gui_widgets.cpp:14-18), so the segments break onto additional rows and the row's height grows, pushing every widget below it down. Worst case is "##bg_color" with 6 items (gui_inspector.cpp:268-270), which can occupy up to 6 rows at kPaneRightMin (280).
- **source**: `src/gui/gui_widgets.cpp:17`
- **notes**: Interacts with the tooltip quirk already recorded as segmented-last-item-tooltip: the tooltip is anchored to the LAST segment, so after wrapping the only tooltip-bearing segment moves to a different visual row (bottom-left rather than far-right). The inventory mentions wrapping only inside a precondition string and has no entry for the resize-driven behaviour itself.
- **tests**: none

### 5. Vertical splitter bar (InvisibleButton)

- **id**: `vsplitter-drag`
- **input**: drag
- **path**: `<id> (the caller's string, e.g. "##split_l" / "##split_r") inside window "##main"`
- **precondition**: Left mouse pressed on the splitter rect (width x height as passed by the caller) and held; ImGui::IsItemActive() is true
- **disabled when**: the frame's delta is rejected entirely when *left_w + dx < left_min OR *right_w - dx < right_min; the move is all-or-nothing, there is no partial clamp, so at a limit the bar stops following the cursor
- **effect**: Each frame, dx = ImGui::GetIO().MouseDelta.x is added to *left_w and subtracted from *right_w, resizing the two adjacent panes. Returns true while dragging. No command is posted and nothing is persisted; the widths live in the caller's statics.
- **source**: `src/gui/gui_splitter.cpp:27`
- **notes**: Only the left mouse button works (ImGui::InvisibleButton defaults to ImGuiButtonFlags_MouseButtonLeft). Vertical mouse motion is ignored; only MouseDelta.x is read.
- **tests**: none
- **audit correction**: Wrong imgui_path. It states the splitter is submitted "inside window \"##main\"". It is not: RenderRendererView (gui_panels.cpp:386), which contains both VSplitter calls at :405 and :416, is only ever reached through the panel registry as tabs[view]->draw() at gui_panels.cpp:450, which sits between BeginChild("main_view", ...) at :449 and EndChild() at :451. A test engine path of "##main/##split_l" will not resolve. -> imgui_path should be "##main / main_view / <id>", e.g. "##main"/"main_view"/"##split_l". The same child prefix applies to the bare "##split_l" / "##split_r" paths on vsplitter-drag-left-instance, vsplitter-drag-right-instance, vsplitter-hover-cursor, vsplitter-press-no-move and vsplitter-keyboard-activate.

### 6. Left splitter between the IFS picker pane and the centre scene pane

- **id**: `vsplitter-drag-left-instance`
- **input**: drag
- **path**: `##split_l`
- **precondition**: The ready/main view is drawn (window "##main", gui_panels.cpp:436) so the three panes and both splitters exist
- **disabled when**: delta rejected while left_w would drop below Gui::kPaneLeftMin (240) or the centre would drop below Gui::kPaneCenterMin (320)
- **effect**: Mutates the static float left_w (gui_panels.cpp:387) that sizes BeginChild("pane_left"); the centre pane width is recomputed next frame by ClampPaneWidths (gui_panels.cpp:396/209). Not persisted across restarts.
- **source**: `src/gui/gui_panels.cpp:405`
- **notes**: The right_w argument is a local copy `c` of center_w, so the splitter's write to it is discarded and the centre width is derived from avail_w - left_w - right_w - 2*splitter each frame.
- **tests**: `splitter drag moves the boundary between the left and centre panes`
- **audit correction**: Incomplete precondition. It says only "The ready/main view is drawn (window \"##main\", gui_panels.cpp:436) so the three panes and both splitters exist". Being in the ready view is not sufficient: the splitters live inside RenderRendererView, which is the draw() of the "renderer_view" MainTab descriptor (panel_registry.cpp:31/:59/:82) and only runs when std::clamp(g_main_view, 0, tabs.size()-1) at gui_panels.cpp:446 selects it. On afp_modern with the iidx33 profile, clicking the "qpro" main tab swaps the body to Panels::RenderQproTabBody and BOTH splitters, all three panes and the timeline dock disappear from the frame. -> Precondition: boot state Ready (gui_panels.cpp:468) AND the selected MainTab panel is "renderer_view" (g_main_view resolves to the renderer descriptor at gui_panels.cpp:446/:450). Add to notes: left_w is a function-local static (gui_panels.cpp:387), so a width set before switching to the qpro tab survives the round trip back to Renderer.

### 7. Right splitter between the centre scene pane and the inspector pane

- **id**: `vsplitter-drag-right-instance`
- **input**: drag
- **path**: `##split_r`
- **precondition**: The ready/main view is drawn (window "##main")
- **disabled when**: delta rejected while the centre would drop below Gui::kPaneCenterMin (320) or right_w below Gui::kPaneRightMin (280)
- **effect**: Mutates the static float right_w (gui_panels.cpp:388) that sizes BeginChild("pane_right") hosting RenderInspectorPane; centre width recomputed by ClampPaneWidths next frame. Not persisted.
- **source**: `src/gui/gui_panels.cpp:416`
- **notes**: Here the left_w argument is the discarded local `c`, so only right_w survives the drag.
- **tests**: none
- **audit correction**: Same incomplete precondition as vsplitter-drag-left-instance: "The ready/main view is drawn (window \"##main\")" omits the requirement that the renderer_view MainTab descriptor is the one currently selected. ##split_r is not submitted at all while the qpro main tab is active. -> Precondition: boot state Ready AND the selected MainTab panel is "renderer_view" (gui_panels.cpp:446/:450, descriptor at panel_registry.cpp:31). right_w is the static at gui_panels.cpp:388 and persists across main-tab switches within a session.

### 8. Gui::SectionHeader (icon + label + suffix + Separator)

- **id**: `section-header-no-interaction`
- **input**: hover
- **path**: `n/a (text-only, no ImGui ID)`
- **precondition**: always, wherever a panel calls SectionHeader
- **disabled when**: always: SectionHeader emits only TextColored / TextUnformatted / TextDisabled / Separator / Spacing, none of which register an activatable ImGui item ID
- **effect**: Nothing. No click target, no tooltip, no collapse. It is not a CollapsingHeader, so the section it titles cannot be expanded or collapsed by the user.
- **source**: `src/gui/gui_widgets.cpp:40`
- **notes**: Recorded explicitly so the surface inventory is provably complete: this widget contributes zero interactions. The optional icon (:41) and suffix (:48) only change what text is drawn.
- **tests**: none

### 9. Segmented control segment button hover/press feedback

- **id**: `segmented-item-hover-highlight`
- **input**: hover
- **path**: `<caller id> / <i> / <items[i]>`
- **precondition**: Mouse over any segment rect
- **effect**: Purely visual: only ImGuiCol_Button and ImGuiCol_Text are pushed (gui_widgets.cpp:21-26), so the default ImGuiCol_ButtonHovered / ImGuiCol_ButtonActive theme colors still paint on hover and press. No state change.
- **source**: `src/gui/gui_widgets.cpp:21`
- **notes**: Consequence of pushing only 2 style colors: the active segment also lights up with the generic hovered color rather than a themed one.
- **tests**: none

### 10. Segmented control: hovering the LAST segment surfaces the caller's tooltip

- **id**: `segmented-last-item-tooltip`
- **input**: hover
- **precondition**: Mouse rests over the final segment (the last ImGui item Segmented emitted), and the caller follows the Segmented() call with IsItemHovered()+SetTooltip
- **disabled when**: hovering any segment other than the last produces no tooltip, because IsItemHovered() after Segmented() refers only to the final Button pushed inside the loop
- **effect**: ImGui::SetTooltip draws the caller's explanatory tooltip. Existing ones: "##root_loop" explains Auto-hold vs Force loop root driving and that it is persisted (gui_inspector.cpp:189); "##live_cont" explains OFF/default/ON continuous-loop and that it applies on the next stream switch (gui_inspector.cpp:209); "##bg_color" explains the preview background / AFP debug viewer F4 and that it is preview-only (gui_inspector.cpp:274). "##mc_name_type" (gui_inspector.cpp:305) has no tooltip.
- **source**: `src/gui/gui_widgets.cpp:28`
- **tooltip**: yes
- **notes**: Real usability quirk worth flagging to a test: the tooltip is only reachable from the LAST segment ("Force loop", "ON", "blue"), never from the others, because Segmented does not wrap the group in an ItemAdd.
- **tests**: none
- **audit correction**: Record is malformed relative to every other entry: it has no imgui_path field at all (the entry jumps from has_tooltip straight to notes), even though it is anchored to a concrete ImGui item - the final Button of the Segmented loop, which is what ImGui::IsItemHovered() at gui_inspector.cpp:189 / :209 / :274 actually tests. -> Add imgui_path: "<caller id> / <count-1> / <items[count-1]>", i.e. "##root_loop"/1/"Force loop", "##live_cont"/2/"ON", "##bg_color"/5/"blue". ("##mc_name_type"/1/"column" has no tooltip, gui_inspector.cpp:305-306.)

### 11. Splitter hover cursor

- **id**: `vsplitter-hover-cursor`
- **input**: hover
- **path**: `<id> e.g. ##split_l`
- **precondition**: Mouse over the splitter rect, or the splitter is being dragged
- **effect**: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW) for the frame, so the OS/ImGui cursor becomes the horizontal-resize arrow; the drawn 1px line switches color from ImGuiCol_Separator to ImGuiCol_SeparatorHovered.
- **source**: `src/gui/gui_splitter.cpp:13`
- **notes**: Cursor is also forced while active (dragging) even if the pointer leaves the 6px bar.
- **tests**: none

### 12. Inspector tab item reached via keyboard navigation

- **id**: `inspector-tab-keyboard-activate` *(audit)*
- **input**: key
- **path**: `##inspector_tabs / <tab_label>`
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard (src/gui/gui_window.cpp:153), the inspector tab bar exists because CollectActivePanels(PanelSlot::InspectorTab) returned a non-empty list (gui_inspector.cpp:442-443), and nav focus has been moved onto a tab with Tab / arrow keys
- **disabled when**: never while the tab bar is drawn; BeginTabItem is passed no flags, so no ImGuiTabItemFlags_NoTabStop / no disabled state exists on any tab
- **effect**: Space/Enter selects the tab item, exactly as a left-click: BeginTabItem returns true, the pane draws ImGui::Spacing() and then calls the descriptor's draw() function pointer (gui_inspector.cpp:446-449).
- **source**: `src/gui/gui_inspector.cpp:446`
- **notes**: No keyboard entry exists anywhere for the inspector tab bar. Worth a test-engine record because ItemClick and nav-activate take different code paths through ImGui's tab bar.
- **tests**: none

### 13. Main view tab button in the top bar, reached via keyboard navigation

- **id**: `main-tab-button-keyboard-activate` *(audit)*
- **input**: key
- **path**: `##main / topbar / <tab_label> e.g. "##main"/"topbar"/"qpro"`
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard is set at src/gui/gui_window.cpp:153 (the only ConfigFlags line in the whole tree - NavEnableGamepad is NOT set), the tab row is drawn because CollectActivePanels(PanelSlot::MainTab) returned more than one panel (gui_panels.cpp:243), and Tab / arrow keys have moved nav focus onto one of the ImGui::Button items in BeginChild("topbar")
- **disabled when**: never while the row is drawn; ImGui::Button sets neither ImGuiItemFlags_NoTabStop nor ImGuiItemFlags_Disabled, so every tab button is a tab stop and an arrow-nav candidate
- **effect**: Space/Enter activates the Button exactly as a left-click: g_main_view = (int)i (gui_panels.cpp:251), which changes which MainTab descriptor's draw() runs at gui_panels.cpp:450 on the same frame.
- **source**: `src/gui/gui_panels.cpp:251`
- **notes**: The inventory records keyboard activation for Segmented (segmented-item-keyboard-activate) and for the splitter (vsplitter-keyboard-activate) but never for the top-bar tab buttons, even though they are the same ImGui::Button primitive. Unlike the splitter this one is fully usable from the keyboard, because the effect is a plain assignment and not io.MouseDelta.
- **tests**: none

### 14. Segmented segment button via keyboard navigation

- **id**: `segmented-item-keyboard-activate`
- **input**: key
- **path**: `<caller id> / <i> / <items[i]>`
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard is set at startup (src/gui/gui_window.cpp:153), the window has nav focus, and the user has moved nav focus onto the segment with Tab / arrow keys
- **disabled when**: never while keyboard nav is enabled
- **effect**: Space/Enter activates the Button exactly as a left-click does: *current = i, Segmented returns true, caller commits (same effects as segmented-item-click).
- **source**: `src/gui/gui_widgets.cpp:28`
- **notes**: Each segment is an individually nav-focusable item; there is no arrow-key-moves-selection behavior, arrows only move nav focus.
- **tests**: none

### 15. Splitter InvisibleButton reached via keyboard nav

- **id**: `vsplitter-keyboard-activate`
- **input**: key
- **path**: `<id> e.g. ##split_l`
- **precondition**: ImGuiConfigFlags_NavEnableKeyboard (src/gui/gui_window.cpp:153) and nav focus landed on the invisible button
- **disabled when**: resize is effectively impossible this way: the delta source is io.MouseDelta.x, which stays 0 during keyboard activation
- **effect**: Holding Space marks the item active, which turns the bar the SeparatorActive color and forces the ResizeEW cursor, but the pane widths never change.
- **source**: `src/gui/gui_splitter.cpp:11`
- **notes**: Worth knowing for the ImGui test engine: driving the splitter by ItemClick alone will not resize anything, the test must inject mouse movement.
- **tests**: none

### 16. Inspector tab "2D package"

- **id**: `inspector-tab-gc2d`
- **input**: left-click
- **path**: `##inspector_tabs / 2D package`
- **precondition**: ActiveBackendId() == "scene3d" AND Gc2dHost::Active() (Gc2dTabVisible, panel_registry.cpp:26)
- **disabled when**: hidden whenever the 2D package host is inactive, and for every backend other than scene3d
- **effect**: Draws Panels::Gc2dPanel::Render.
- **source**: `src/gui/panel_registry.cpp:92`
- **notes**: Only panel in the registry gated on Gc2dHost.
- **tests**: none

### 17. Inspector tab "Live" (DDR backend)

- **id**: `inspector-tab-live-ddr`
- **input**: left-click
- **path**: `##inspector_tabs / Live`
- **precondition**: ActiveBackendId() == "afp_ddr"
- **disabled when**: absent for the scene3d backend
- **effect**: Draws Panels::RenderLiveTab.
- **source**: `src/gui/panel_registry.cpp:69`
- **notes**: Separate descriptor from the modern one even though id, label and draw match.
- **tests**: none

### 18. Inspector tab "Live" (modern backend)

- **id**: `inspector-tab-live-modern`
- **input**: left-click
- **path**: `##inspector_tabs / Live`
- **precondition**: ActiveBackendId() == "afp_modern"
- **disabled when**: absent for the scene3d backend
- **effect**: Draws Panels::RenderLiveTab.
- **source**: `src/gui/panel_registry.cpp:51`
- **notes**: Same draw function as the DDR Live tab.
- **tests**: none

### 19. Inspector tab "Properties"

- **id**: `inspector-tab-properties`
- **input**: left-click
- **path**: `##inspector_tabs / Properties`
- **precondition**: App::Global().ActiveBackendId() == "afp_modern"
- **disabled when**: absent for the afp_ddr and scene3d backends (not in their panel sets)
- **effect**: Draws Panels::RenderPropertiesTab in the inspector pane.
- **source**: `src/gui/panel_registry.cpp:41`
- **notes**: visible predicate is nullptr, so backend membership is the only gate.
- **tests**: none

### 20. Inspector tab "Render" (DDR backend variant)

- **id**: `inspector-tab-render-ddr`
- **input**: left-click
- **path**: `##inspector_tabs / Render`
- **precondition**: ActiveBackendId() == "afp_ddr"
- **disabled when**: absent for afp_modern and scene3d backends
- **effect**: Draws Panels::RenderRenderTabDdr.
- **source**: `src/gui/panel_registry.cpp:64`
- **notes**: Because the label is identical to the modern variant, a UI test must key off the active backend to know which body it is exercising.
- **tests**: none

### 21. Inspector tab "Render" (modern backend variant)

- **id**: `inspector-tab-render-modern`
- **input**: left-click
- **path**: `##inspector_tabs / Render`
- **precondition**: ActiveBackendId() == "afp_modern"
- **disabled when**: absent for afp_ddr (which has its own "Render" descriptor) and for scene3d (no Render tab at all)
- **effect**: Draws Panels::RenderRenderTabModern.
- **source**: `src/gui/panel_registry.cpp:46`
- **notes**: Same id "render" and same visible label as the DDR entry but a different draw function, hence a separate entry.
- **tests**: none

### 22. Inspector tab "3D scene" (DDR backend)

- **id**: `inspector-tab-scene3d-ddr`
- **input**: left-click
- **path**: `##inspector_tabs / 3D scene`
- **precondition**: ActiveBackendId() == "afp_ddr" AND Scene3dHost::Active() (Scene3dTabVisible, panel_registry.cpp:22)
- **disabled when**: hidden whenever the 3D scene host is not active, even on the DDR backend
- **effect**: Draws Panels::Scene3dPanel::Render.
- **source**: `src/gui/panel_registry.cpp:74`
- **notes**: The tab can appear and disappear mid-session as Scene3dHost::Active() flips; ImGui will then move selection to a neighbouring tab.
- **tests**: none

### 23. Inspector tab "3D scene" (scene3d backend)

- **id**: `inspector-tab-scene3d-standalone`
- **input**: left-click
- **path**: `##inspector_tabs / 3D scene`
- **precondition**: ActiveBackendId() == "scene3d" AND Scene3dHost::Active()
- **disabled when**: hidden when the 3D scene host is inactive; with the gc2d tab also hidden the inspector tab bar is not drawn at all (gui_inspector.cpp:443)
- **effect**: Draws Panels::Scene3dPanel::Render.
- **source**: `src/gui/panel_registry.cpp:87`
- **notes**: Second declaration of the same panel, in the scene3d set.
- **tests**: none

### 24. Inspector tab item (one per InspectorTab panel)

- **id**: `inspector-tab-select`
- **input**: left-click
- **path**: `##inspector_tabs / <tab_label>  (BeginTabBar("##inspector_tabs"), gui_inspector.cpp:444)`
- **precondition**: CollectActivePanels(PanelSlot::InspectorTab) returned a non-empty list; RenderInspectorPane returns early and draws no tab bar when it is empty (gui_inspector.cpp:443)
- **disabled when**: a tab is absent entirely when its descriptor's visible() predicate returns false or its backend set is not active; there is no greyed-out state
- **effect**: ImGui::BeginTabItem selects the tab; on selection the pane draws ImGui::Spacing() then calls the descriptor's draw() function pointer for the rest of the frame.
- **source**: `src/gui/gui_inspector.cpp:446`
- **notes**: ONE entry for the loop over collected InspectorTab descriptors. The individual tabs are enumerated as separate entries below because their preconditions differ.
- **tests**: none

### 25. Main view tab button in the top bar (one per MainTab panel)

- **id**: `main-tab-button-click`
- **input**: left-click
- **path**: `topbar / <tab_label>  (inside BeginChild("topbar"), gui_panels.cpp:234)`
- **precondition**: CollectActivePanels(PanelSlot::MainTab) returned MORE THAN ONE panel (gui_panels.cpp:243). With one visible main panel the button row is not drawn at all.
- **disabled when**: hidden whenever tabs.size() <= 1; clicking the already-active tab re-assigns the same index (harmless no-op)
- **effect**: Sets the file-static g_main_view = i, which selects which MainTab panel body is drawn; the active tab is tinted ImGuiCol_HeaderActive, inactive ones ImGuiCol_FrameBg.
- **source**: `src/gui/gui_panels.cpp:251`
- **notes**: ONE entry for the loop over the collected MainTab descriptors. If a visibility gate later hides the selected tab, g_main_view is clamped back to 0 at gui_panels.cpp:255, so the user is snapped to "Renderer".
- **tests**: none

### 26. Main tab "qpro"

- **id**: `main-tab-qpro`
- **input**: left-click
- **path**: `topbar / qpro`
- **precondition**: Active backend id == "afp_modern" AND App::Global().GetGameProfileSlug() == "iidx33" (QproTabVisible, panel_registry.cpp:18)
- **disabled when**: hidden for every profile other than iidx33 and for every non-afp_modern backend
- **effect**: Selects the main view whose body is Panels::RenderQproTabBody.
- **source**: `src/gui/panel_registry.cpp:36`
- **notes**: This is the only descriptor that makes tabs.size() > 1 for afp_modern, so it is also what causes the top-bar tab row to appear at all. Switching the profile away from iidx33 while qpro is selected trips the g_main_view clamp at gui_panels.cpp:255 and drops the user back to Renderer.
- **tests**: none

### 27. Main tab "Renderer"

- **id**: `main-tab-renderer`
- **input**: left-click
- **path**: `topbar / Renderer`
- **precondition**: Active backend id is afp_modern, afp_ddr or scene3d (this descriptor exists in all three sets) AND at least one other MainTab panel is visible so the button row renders
- **disabled when**: the whole row is hidden when it is the only MainTab panel, which is the case for afp_ddr and scene3d backends and for afp_modern on a non-iidx33 profile
- **effect**: Selects the main view whose body is Panels::RenderRendererView.
- **source**: `src/gui/panel_registry.cpp:31`
- **notes**: Declared three times (panel_registry.cpp:31, :59, :82), once per backend panel set; identical id/slot/draw each time.
- **tests**: none

### 28. Segmented instance "##mc_name_type" is conditionally hidden

- **id**: `mc-name-type-segmented-show-gate` *(audit)*
- **input**: left-click
- **path**: `##inspector_tabs / Render / ##mc_name_type / <i> / <items[i]>`
- **precondition**: The modern Render tab is drawn (RenderRenderTabModern, gui_inspector.cpp:319, which calls DrawFilterMcNameRows at :339) AND the "Show MC names (F3)" checkbox (gui_inspector.cpp:292) has set ov.show_mc_names = true
- **disabled when**: hidden entirely whenever ov.show_mc_names is false: the whole widget lives inside `if (ov.show_mc_names)` at gui_inspector.cpp:302, so there is no greyed-out state, the control simply does not exist. It is also absent on the DDR backend, because RenderRenderTabDdr (gui_inspector.cpp:346) calls only DrawBackgroundRow and DrawResetOverridesRow.
- **effect**: When shown it is drawn between ImGui::Indent(24.0F) / ImGui::Unindent(24.0F) (gui_inspector.cpp:303/:306) and writes straight into ov.mc_name_type; `changed = true` makes RenderRenderTabModern call state.ApplyLiveOverridesDelta(before, ov) at gui_inspector.cpp:341. mc_name_type == 1 ("column") is what makes RenderLiveTab call DrawMcNamesList (gui_inspector.cpp:437); 0 ("at clip pos") draws the names over the preview.
- **source**: `src/gui/gui_inspector.cpp:302`
- **notes**: The inventory's segmented-item-click entry lists gui_inspector.cpp:305 as a caller but records disabled_when as "never", and no entry anywhere captures this per-instance visibility gate. Of the four Segmented call sites this is the only one that can be absent from the UI, and it is the only one that is NOT followed by an IsItemHovered()+SetTooltip pair.
- **tests**: none

### 29. Clicking anywhere on the loading overlay

- **id**: `overlay-click-through`
- **input**: left-click
- **path**: `##loading_overlay`
- **precondition**: Overlay visible (progress.active)
- **disabled when**: always inert as a target: ImGuiWindowFlags_NoInputs means the overlay never captures mouse or keyboard, and NoBringToFrontOnFocus keeps it from stealing focus
- **effect**: The click passes straight through to whatever widget is underneath, so the user can still operate the UI behind the dimmed veil. The overlay itself has no button, no cancel, and no way to dismiss it.
- **source**: `src/gui/gui_loading_overlay.cpp:118`
- **notes**: There is deliberately no Cancel control for an in-flight load on this surface.
- **tests**: none

### 30. Effect of an unrecognised active backend id on every tab on this surface

- **id**: `panel-set-unknown-backend`
- **input**: left-click
- **path**: `n/a (nothing is submitted)`
- **precondition**: App::Global().ActiveBackendId() matches none of "afp_modern", "afp_ddr", "scene3d" in kPanelSets
- **disabled when**: in that state there is nothing to click: CollectActivePanels leaves `out` cleared, so the top-bar tab row is skipped (tabs.size() <= 1) and RenderInspectorPane returns before creating the tab bar
- **effect**: No main-tab buttons and no inspector tab bar are drawn at all; the inspector pane renders empty.
- **source**: `src/gui/panel_registry.cpp:112`
- **notes**: The loop breaks on the first matching set (panel_registry.cpp:122), so exactly one panel set is ever live; the callers keep the vector in a function-local static and refill it every frame.
- **tests**: none

### 31. Segmented control: the currently selected segment

- **id**: `segmented-active-item-click-noop`
- **input**: left-click
- **path**: `<caller id> / <i> / <items[i]> where i == *current`
- **precondition**: i == *current, i.e. the segment is the active one (drawn with ImGuiCol_HeaderActive background and ImGuiCol_CheckMark text)
- **disabled when**: always inert as far as state goes: the `&& !active` guard suppresses the assignment, so Segmented returns false and the caller never runs its setter / SaveCurrentSettings
- **effect**: Button reports pressed, but *current is not written and `changed` stays false, so no App::State setter and no settings save runs. Only the transient ImGuiCol_ButtonActive press flash is visible.
- **source**: `src/gui/gui_widgets.cpp:28`
- **notes**: Separate entry because this is a real gate: re-clicking the active segment is deliberately a no-op, which also means no redundant App::SaveCurrentSettings() write for "##root_loop".
- **tests**: none

### 32. Segmented control when *current is outside [0, count)

- **id**: `segmented-current-out-of-range` *(audit)*
- **input**: left-click
- **path**: `<caller id> / <i> / <items[i]>`
- **precondition**: The caller passes a *current value that no segment matches
- **disabled when**: n/a - every segment stays clickable; the only difference is that none is drawn active
- **effect**: `const bool active = (i == *current)` (gui_widgets.cpp:19) is false for every i, so all segments are painted with ImGuiCol_FrameBg / ImGuiCol_TextDisabled and none carries the ImGuiCol_HeaderActive + ImGuiCol_CheckMark treatment - the control shows no selection at all. The `&& !active` guard at :28 then lets ANY segment click through, so the first click always writes. Callers differ: "##bg_color" pre-clamps (`ov.bg_color_index < -1 \|\| > 4 ? 0 : idx+1`, gui_inspector.cpp:267) so a bad stored value silently displays as "default" while the stored value stays bad until clicked; "##mc_name_type" passes &ov.mc_name_type raw (gui_inspector.cpp:305) with no clamp, so a stored value other than 0 or 1 leaves the control blank while RenderLiveTab's `ov.mc_name_type == 1` test (gui_inspector.cpp:437) treats it as "at clip pos".
- **source**: `src/gui/gui_widgets.cpp:19`
- **notes**: Segmented never validates or clamps *current itself, and it has no assert; the widget silently renders a selection-less row. Only 2 of the 4 call sites defend against it, so this is a real state the UI can enter, not a hypothetical.
- **tests**: none

### 33. Segmented control segment button (one per item)

- **id**: `segmented-item-click`
- **input**: left-click
- **path**: `<caller id> / <i> / <items[i]>  (PushID(id) at gui_widgets.cpp:10, PushID(i) at :27, Button(items[i]) at :28) e.g. "##root_loop"/0/"Auto-hold"`
- **precondition**: The calling panel row is drawn. Segments are laid out with SameLine and wrap to a NewLine when GetContentRegionAvail().x < item width, so every segment is always reachable.
- **disabled when**: never (every segment is a live ImGui::Button; only the write is suppressed, see segmented-active-item-click-noop)
- **effect**: Sets *current = i and returns true from Gui::Segmented, which makes the caller commit its state. Known callers: gui_inspector.cpp:184 "##root_loop" -> App::Global().SetRootLoopMode(Hold\|Force) + App::SaveCurrentSettings(); gui_inspector.cpp:205 "##live_cont" -> ov.continuous_loop_mode = idx-1, changed=true; gui_inspector.cpp:270 "##bg_color" -> ov.bg_color_index = idx-1, changed=true; gui_inspector.cpp:305 "##mc_name_type" -> ov.mc_name_type = idx, changed=true.
- **source**: `src/gui/gui_widgets.cpp:28`
- **notes**: ONE entry describing the whole loop over items (gui_widgets.cpp:13-34). Concrete instantiations in the app: "##root_loop" {Auto-hold, Force loop}, "##live_cont" {OFF, default, ON}, "##bg_color" {default, grey, black, red, green, blue}, "##mc_name_type" {at clip pos, column}.
- **tests**: none

### 34. Splitter click without moving the mouse

- **id**: `vsplitter-press-no-move`
- **input**: left-click
- **path**: `<id> e.g. ##split_r`
- **precondition**: Mouse down on the splitter rect
- **effect**: IsItemActive() becomes true so the line is drawn with ImGuiCol_SeparatorActive and VSplitter returns true to the caller for that frame; because MouseDelta.x == 0 the `if (d != 0.0F)` guard skips the resize, so no widths change.
- **source**: `src/gui/gui_splitter.cpp:16`
- **notes**: Neither call site uses the bool return value, so a bare press has visual effect only.
- **tests**: none

### 35. Main view body host child ("main_view") that renders the selected MainTab panel

- **id**: `main-view-body-child-scroll` *(audit)*
- **input**: scroll
- **path**: `##main / main_view`
- **precondition**: Boot state is Ready so RenderReadyView runs (gui_panels.cpp:468-469), and CollectActivePanels(PanelSlot::MainTab) returned a non-empty list (gui_panels.cpp:444-445)
- **disabled when**: the child is skipped entirely when tabs is empty, which is exactly the unrecognised-backend case; no scroll flags are cleared, so wheel scrolling is always live when the body overflows content_h
- **effect**: BeginChild("main_view", ImVec2(0, content_h), 0) at gui_panels.cpp:449 hosts the panel body; content_h = GetContentRegionAvail().y - kStatusStripH (28) - ItemSpacing.y. It is created with no ImGuiWindowFlags, so the mouse wheel scrolls it and a scrollbar appears whenever the selected panel's body is taller than content_h. Which descriptor's draw() runs is decided by a SECOND independent clamp, `int view = std::clamp(g_main_view, 0, (int)tabs.size() - 1)` at gui_panels.cpp:446, distinct from the `g_main_view = 0` reset at gui_panels.cpp:255.
- **source**: `src/gui/gui_panels.cpp:449`
- **notes**: The inventory's main-tab-button-click documents the write to g_main_view and the :255 clamp but never the host child, its scrollability, or the second clamp at :446 that actually dispatches draw(). This child is also why the vsplitter imgui_path in the inventory is wrong (see corrections).
- **tests**: none

### 36. Scrolling over the loading overlay

- **id**: `overlay-scroll-blocked`
- **input**: scroll
- **path**: `##loading_overlay`
- **precondition**: Overlay visible and the pointer is over the viewport
- **disabled when**: always: NoInputs prevents the overlay from taking the wheel, and its contents are placed with absolute SetCursorScreenPos rather than flowing, so there is nothing to scroll
- **effect**: The wheel event reaches the window underneath instead; the overlay never scrolls.
- **source**: `src/gui/gui_loading_overlay.cpp:118`
- **notes**: Spinner (:125), title text (:136), stage text (:141), progress bar (:145) and detail line (:101) are all non-interactive drawings; the indeterminate bar and spinner animate off ImGui::GetTime() with no input.
- **tests**: none

### 37. Loading overlay appearance / disappearance

- **id**: `overlay-visibility-gate`
- **input**: window-message
- **path**: `##loading_overlay`
- **precondition**: progress.active is true (App::LoadProgress published by a background/boot worker); Render() returns immediately at gui_loading_overlay.cpp:109 when inactive
- **disabled when**: progress.active == false, in which case no window is submitted at all
- **effect**: A full-viewport window "##loading_overlay" is submitted at ImGui::GetMainViewport()->WorkPos/WorkSize with 0.75 background alpha, covering the whole UI. Crossing the active edge also emits a LOG("Gui", "Loading overlay ON/OFF ...") line via LogEdge.
- **source**: `src/gui/gui_loading_overlay.cpp:107`
- **notes**: Not a user widget, but it is the gate that governs every other overlay entry, and it is what the user perceives when they trigger any long operation. Resizing the host OS window re-reads WorkPos/WorkSize the next frame, so the overlay always refills the viewport and the spinner/label/bar re-centre (gui_loading_overlay.cpp:111-122).
- **audit correction**: Incomplete precondition: it only names progress.active and implies the overlay belongs to the ready UI. Panels::LoadingOverlay::Render(progress) is called at gui_panels.cpp:474, AFTER the if/else at :468-472, so it runs identically over the Setup view (Panels::Setup::RenderView) and over the ready view. progress is snapshotted once per frame at gui_panels.cpp:466 by App::Global().GetLoadProgress() - it is a polled state struct, not a window message, which also makes the entry's input value "window-message" misleading. -> Precondition: App::Global().GetLoadProgress().active is true (polled each frame at gui_panels.cpp:466), in EITHER boot state - the overlay is submitted last at gui_panels.cpp:474 and therefore veils the Setup view during boot as well as the ready view during an IFS / texture load. Input should be "state-poll" rather than "window-message".

