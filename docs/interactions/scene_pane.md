# Scene pane

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `sublayer-visibility-checkbox` | Sublayer visibility checkbox | checkbox | - | 2 |
| 2 | `layer-row-double-click-play` | Layer row (double-click to play/replay) | double-click | yes | 4 |
| 3 | `add-slot-tooltip` | Add button hover tooltip | hover | yes | 3 |
| 4 | `layer-row-tooltip` | Layer row hover tooltip | hover | yes | 4 |
| 5 | `add-slot-button-keyboard-activate` *(audit)* | Add button activated with the keyboard (Space / Enter) | key | yes | 3 |
| 6 | `add-slot-input-editing-keys` *(audit)* | New variant slot text box editing keymap, and Enter having no effect | key | - | 3 |
| 7 | `layer-row-keyboard-toggle` *(audit)* | Layer row activated with the keyboard (Space / Enter) | key | yes | 4 |
| 8 | `scene-filter-input-editing-keys` *(audit)* | Clip filter text box editing keymap and mouse text selection | key | - | 3 |
| 9 | `scene-keyboard-nav-focus` *(audit)* | Keyboard nav cursor over the scene pane items | key | - | **none** |
| 10 | `scene-tree-keyboard-arrow-expand` *(audit)* | Left / Right arrow on a nav-focused tree node (layer row or sublayer node) | key | - | **none** |
| 11 | `sublayer-node-keyboard-toggle` *(audit)* | Sublayer tree node activated with the keyboard (Space / Enter) | key | - | **none** |
| 12 | `sublayer-vis-checkbox-keyboard-toggle` *(audit)* | Sublayer visibility checkbox activated with the keyboard (Space / Enter) | key | - | 2 |
| 13 | `add-slot-button` | Add (register variant slot) button | left-click | yes | 3 |
| 14 | `layer-row-expand` | Layer row expand/collapse arrow | left-click | yes | 4 |
| 15 | `layer-row-select` | Layer row (click on the label, not the arrow) | left-click | yes | 4 |
| 16 | `sublayer-node-expand` | Sublayer tree node expand/collapse arrow | left-click | - | **none** |
| 17 | `sublayer-node-select` | Sublayer tree node (click on the label) | left-click | - | **none** |
| 18 | `unresolved-slot-selectable` | Unresolved / unmatched variant slot row | left-click | - | **none** |
| 19 | `scene-scroll-child` | Scene tree scroll region | scroll | - | 10 |
| 20 | `scene-empty-no-ifs-gate` | Empty-state message when no IFS is selected | state-change | - | n/a |
| 21 | `scene-empty-no-layers-gate` | Empty-state message when afplist.xml lists no layers | state-change | - | n/a |
| 22 | `scene-selection-reset-on-ifs-change` | Implicit selection reset when the user picks a different IFS elsewhere | state-change | - | n/a |
| 23 | `add-slot-input` | New variant slot path text box (hint "add slot by clip path, e.g. coin") | text-entry | - | 3 |
| 24 | `scene-filter-input` | Clip filter text box (hint "find clip...") | text-entry | - | 3 |

## Detail

### 1. Sublayer visibility checkbox

- **id**: `sublayer-visibility-checkbox`
- **input**: checkbox
- **path**: `scene_scroll/<layer idx>/<node idx…>/##vis`
- **precondition**: Parent layer row is open AND the node's subtree matches the filter (SubtreeMatches); initial checked state comes from the matching entry in state.GetSublayerOverrides(active), defaulting to true when no override exists
- **effect**: ImGui::Checkbox("##vis", &visible) returning true calls state.SetSublayerOverride(active, node.path, visible), recording a per-IFS visibility override for that clip path.
- **source**: `src/gui/gui_scene_panel.cpp:96`
- **notes**: ONE entry for a control drawn in the recursive loop RenderSceneNode over status.mc_tree children (line 126-127 and 190-191); each level pushes ImGui::PushID(idx).
- **tests**: `3D scene model list toggles visibility and blend mode`, `scene pane child visibility checkbox records a sublayer override`
- **audit correction**: The effect stops at 'recording a per-IFS visibility override', which reads as UI-only bookkeeping. The override actually drives clip visibility in the live render and is persisted across runs. -> ImGui::Checkbox("##vis", &visible) returning true calls state.SetSublayerOverride(active, node.path, visible) (line 96), stored per-IFS in IfsCatalog (src/state/ifs_catalog.cpp:41). ModernRuntime::ApplySublayerOverrides then pushes every override to the engine with McControl::SetClipVisible (src/game_runtime_modern.cpp:189-194), so unchecking actually hides that clip in the rendered output. The set is reloaded from the saved config at startup via App::Global().SetSublayerOverride in src/boot.cpp:193, so the toggle survives a restart.

### 2. Layer row (double-click to play/replay)

- **id**: `layer-row-double-click-play`
- **input**: double-click
- **path**: `scene_scroll/<idx>/##layer`
- **precondition**: Row is visible after filtering AND ImGui::IsItemHovered() AND ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
- **effect**: state.PostCommand(AfpCmd::Wrap(AfpCmd::SwitchAnimation{.name = name, .label = ""})) queues the AFP SwitchAnimation command for the render thread, which switches the playing animation (or replays it if it is already playing).
- **source**: `src/gui/gui_scene_panel.cpp:181`
- **tooltip**: yes
- **notes**: Same looped row. The first click of the double-click also fires layer-row-select, so a double-click both selects and plays.
- **tests**: `properties tab Play posts SwitchAnimation for an idle layer`, `properties tab Replay posts SwitchAnimation for the playing layer`, `scene pane double-click on a layer posts SwitchAnimation`, `scene pane filter narrows the layer list`

### 3. Add button hover tooltip

- **id**: `add-slot-tooltip`
- **input**: hover
- **path**: `Add`
- **precondition**: ImGui::IsItemHovered() on the Add button (the last submitted item at that point)
- **effect**: ImGui::SetTooltip(...) explains that the entry registers a clip path as a variant slot, that the render thread probes it next frame, and that unresolved slots remain listed under the playing layer until they resolve.
- **source**: `src/gui/gui_scene_panel.cpp:221`
- **tooltip**: yes
- **tests**: `scene pane Add ignores an empty slot name`, `scene pane Add refuses a duplicate slot path`, `scene pane Add registers a variant slot`

### 4. Layer row hover tooltip

- **id**: `layer-row-tooltip`
- **input**: hover
- **path**: `scene_scroll/<idx>/##layer`
- **precondition**: Row is visible after filtering AND ImGui::IsItemHovered()
- **effect**: ImGui::SetTooltip(...) shows a tooltip explaining that a single click selects the layer and a double-click plays / replays it.
- **source**: `src/gui/gui_scene_panel.cpp:184`
- **tooltip**: yes
- **notes**: One tooltip, drawn once per looped layer row.
- **tests**: `properties tab Play posts SwitchAnimation for an idle layer`, `properties tab Replay posts SwitchAnimation for the playing layer`, `scene pane double-click on a layer posts SwitchAnimation`, `scene pane filter narrows the layer list`

### 5. Add button activated with the keyboard (Space / Enter)

- **id**: `add-slot-button-keyboard-activate` *(audit)*
- **input**: key
- **path**: `Add`
- **precondition**: Nav cursor is on the Add button
- **disabled when**: never (the add itself is a no-op when buf is empty because of the `&& buf[0] != 0` short-circuit at line 203)
- **effect**: NavActivate makes ImGui::Button return true, running the identical dedupe-scan / VariantSlot push_back / buffer-clear path as a mouse click. Tab from the ##new_slot field lands here directly, so the whole add-slot flow is reachable without the mouse.
- **source**: `src/gui/gui_scene_panel.cpp:203`
- **tooltip**: yes
- **notes**: Tab out of ##new_slot deactivates the text field first (no ImGuiInputTextFlags_AllowTabInput at line 201), so a tab character is never inserted.
- **tests**: `scene pane Add ignores an empty slot name`, `scene pane Add refuses a duplicate slot path`, `scene pane Add registers a variant slot`

### 6. New variant slot text box editing keymap, and Enter having no effect

- **id**: `add-slot-input-editing-keys` *(audit)*
- **input**: key
- **path**: `##new_slot`
- **precondition**: ##new_slot is active
- **effect**: Same full ImGui editing keymap and Escape-reverts behaviour as the filter box (no flags passed at line 201). Notably ImGuiInputTextFlags_EnterReturnsTrue is NOT set and the return value is discarded, so pressing Enter after typing a clip path does NOT register the slot: the only paths that add are clicking or nav-activating the Add button at line 203. Focus here also raises io.WantTextInput and suppresses all timeline shortcuts (src/gui/gui_timeline.cpp:193).
- **source**: `src/gui/gui_scene_panel.cpp:201`
- **notes**: The 'Enter does nothing' behaviour is the most likely user-visible surprise on this surface and is absent from the inventory's add-slot-input entry.
- **tests**: `scene pane Add refuses a duplicate slot path`, `scene pane Add registers a variant slot`, `scene pane prompts for an IFS while none is active`

### 7. Layer row activated with the keyboard (Space / Enter)

- **id**: `layer-row-keyboard-toggle` *(audit)*
- **input**: key
- **path**: `scene_scroll/<idx>/##layer`
- **precondition**: Nav cursor is on the layer TreeNodeEx AND has_children is true (row is not a Leaf)
- **disabled when**: has_children is false: Leaf \| NoTreePushOnOpen are set at line 166 and imgui_widgets.cpp TreeNodeBehavior skips the whole toggle block for leaves
- **effect**: imgui 1.92.7 TreeNodeBehavior (vendor/vcpkg/buildtrees/imgui/src/v1.92.7-b588f89316.clean/imgui_widgets.cpp:7007) toggles the node when g.NavActivateId == id EVEN THOUGH ImGuiTreeNodeFlags_OpenOnArrow is set, so Space/Enter expands or collapses the layer row. It does NOT select it: the selection at line 178 is guarded by ImGui::IsItemClicked(ImGuiMouseButton_Left), which is mouse-only, and the play gesture at line 181 needs IsMouseDoubleClicked. Keyboard therefore can expand a layer but can never select or play one. The same Space keypress also reaches Panels::HandleShortcuts and toggles playback pause (src/gui/gui_timeline.cpp:201), because that handler is gated only by io.WantTextInput (line 193) and IsKeyPressed ignores key ownership.
- **source**: `src/gui/gui_scene_panel.cpp:175`
- **tooltip**: yes
- **notes**: The Space double-effect (expand row + pause playback) is a real conflict, not a theoretical one: HandleShortcuts is called every frame from RenderTimelineDock (src/gui/gui_timeline.cpp:228) whenever an IFS is loaded.
- **tests**: `properties tab Play posts SwitchAnimation for an idle layer`, `properties tab Replay posts SwitchAnimation for the playing layer`, `scene pane double-click on a layer posts SwitchAnimation`, `scene pane filter narrows the layer list`

### 8. Clip filter text box editing keymap and mouse text selection

- **id**: `scene-filter-input-editing-keys` *(audit)*
- **input**: key
- **path**: `##scene_filter`
- **precondition**: ##scene_filter is active (clicked into or nav-activated)
- **effect**: The InputTextWithHint at line 261 carries no flags, so the full ImGui text-edit keymap is live: Left/Right/Home/End and Ctrl+Left/Right caret motion, Shift+motion selection, Ctrl+A select-all, Ctrl+C / Ctrl+X / Ctrl+V clipboard, Ctrl+Z / Ctrl+Y undo-redo, Backspace/Delete (with Ctrl for word delete), click-drag selection and double-click word selection. Every edit takes effect on the same frame because lower_filter is recomputed from filter_buf on line 262 each frame. Escape reverts the buffer to its value at activation and deactivates, which is the fastest way to clear a typed filter (no clear button exists). While the field is active io.WantTextInput is true, which makes Panels::HandleShortcuts return immediately (src/gui/gui_timeline.cpp:193), disabling Space=pause, Left/Right=step and Ctrl+E=export for as long as the filter box has focus.
- **source**: `src/gui/gui_scene_panel.cpp:261`
- **notes**: The inventory's scene-filter-input entry covers only 'typing'; the implicit editing keymap, the Escape revert, and the global-shortcut suppression are all unrecorded.
- **tests**: `scene pane filter narrows the layer list`, `scene pane prompts for an IFS while none is active`, `scene pane reports an IFS with no listed layers`

### 9. Keyboard nav cursor over the scene pane items

- **id**: `scene-keyboard-nav-focus` *(audit)*
- **input**: key
- **path**: `scene_scroll/*`
- **precondition**: io.ConfigFlags has ImGuiConfigFlags_NavEnableKeyboard (set unconditionally at src/gui/gui_window.cpp:153) AND the pane is visible (active IFS, cfg.anim_names non-empty)
- **disabled when**: never; there is no ImGuiItemFlags_NoNav / NoNavDefaultFocus anywhere in this file
- **effect**: Tab / Shift+Tab and the nav arrow keys move the ImGui nav cursor across every item this pane submits: the ##scene_filter box, each layer TreeNodeEx, each ##vis checkbox and sublayer TreeNodeEx, each unresolved-slot Selectable, the ##new_slot box and the Add button. When the focused item is inside the scene_scroll child, ImGui scrolls that child to bring it into view, so keyboard focus movement is a second way to scroll the region beside wheel and scrollbar drag.
- **source**: `src/gui/gui_scene_panel.cpp:267 (BeginChild) with nav enabled at src/gui/gui_window.cpp:153`
- **notes**: Prerequisite entry for the four keyboard entries below. The inventory records only mouse gestures, so the entire keyboard path through this pane is absent.
- **tests**: none

### 10. Left / Right arrow on a nav-focused tree node (layer row or sublayer node)

- **id**: `scene-tree-keyboard-arrow-expand` *(audit)*
- **input**: key
- **path**: `scene_scroll/<idx>/##layer and scene_scroll/<layer idx>/<node idx…>/<node.name>`
- **precondition**: Nav cursor is on a non-leaf TreeNodeEx; Right applies when the node is closed, Left when it is open
- **disabled when**: Leaf nodes (both the has_children==false layer rows and the is_leaf sublayer nodes) never toggle: the block is inside if (!is_leaf)
- **effect**: imgui_widgets.cpp:7023-7033 toggles the node on ImGuiDir_Left when open and ImGuiDir_Right when closed, then cancels the nav move. For sublayer nodes this again routes through IsItemToggledOpen -> state.SetSublayerExpanded at line 123. Critically, the SAME keypress is also consumed by the timeline: src/gui/gui_timeline.cpp:203-204 steps the playback frame by 1 (or by 100 with Shift held, line 202) on Left/Right, gated only by io.WantTextInput. So navigating the clip tree with the arrow keys silently seeks the animation.
- **source**: `src/gui/gui_scene_panel.cpp:107 and src/gui/gui_scene_panel.cpp:175`
- **notes**: Reported as one entry because both TreeNodeEx call sites share the identical imgui behaviour and the identical timeline collision.
- **tests**: none

### 11. Sublayer tree node activated with the keyboard (Space / Enter)

- **id**: `sublayer-node-keyboard-toggle` *(audit)*
- **input**: key
- **path**: `scene_scroll/<layer idx>/<node idx…>/<node.name>`
- **precondition**: Nav cursor is on the sublayer TreeNodeEx AND is_leaf is false (NOT (node.enumerated && node.children.empty()))
- **disabled when**: is_leaf is true: Leaf \| NoTreePushOnOpen set at line 103
- **effect**: Same NavActivate toggle path as the layer row (imgui_widgets.cpp:7007). Because ImGui sets ImGuiItemStatusFlags_ToggledOpen regardless of input source, ImGui::IsItemToggledOpen() at line 108 is true, so line 123 runs state.SetSublayerExpanded(node.path, open) exactly as for a mouse arrow click, which in turn drives the render-thread child enumeration (see the correction on sublayer-node-expand). No selection happens, since line 109 requires IsItemClicked.
- **source**: `src/gui/gui_scene_panel.cpp:107`
- **notes**: Keyboard activation is the only gesture in this pane that mutates persistent App::State without any mouse involvement.
- **tests**: none

### 12. Sublayer visibility checkbox activated with the keyboard (Space / Enter)

- **id**: `sublayer-vis-checkbox-keyboard-toggle` *(audit)*
- **input**: key
- **path**: `scene_scroll/<layer idx>/<node idx…>/##vis`
- **precondition**: Nav cursor is on the ##vis checkbox (parent layer row open and the node subtree matches the filter)
- **effect**: ImGui::Checkbox returns true on NavActivate exactly as on a mouse click (ButtonBehavior nav-activate path, imgui_widgets.cpp:696-699), so line 96 runs state.SetSublayerOverride(active, node.path, visible) and the clip is hidden/shown in the live render on the next ApplySublayerOverrides pass.
- **source**: `src/gui/gui_scene_panel.cpp:96`
- **notes**: Also note Space here collides with the timeline pause shortcut (src/gui/gui_timeline.cpp:201), same mechanism as layer-row-keyboard-toggle.
- **tests**: `3D scene model list toggles visibility and blend mode`, `scene pane child visibility checkbox records a sublayer override`

### 13. Add (register variant slot) button

- **id**: `add-slot-button`
- **input**: left-click
- **path**: `Add`
- **precondition**: An IFS is active AND cfg.anim_names is non-empty
- **disabled when**: never (the button is always clickable; the action is a no-op when buf is empty because of the `&& buf[0] != 0` guard)
- **effect**: Scans cfg.slots for an existing entry with the same path; if none exists it constructs an App::VariantSlot with path = buf, default_bitmap = buf, visible = true, is_valid = false and push_backs it into state.MutConfig(active).slots. Then clears buf. The render thread probes the new slot on a later frame; until it resolves it appears via RenderUnresolvedSlots.
- **source**: `src/gui/gui_scene_panel.cpp:203`
- **tooltip**: yes
- **notes**: Button size ImVec2(-FLT_MIN, 0) stretches it to the remaining width.
- **tests**: `scene pane Add ignores an empty slot name`, `scene pane Add refuses a duplicate slot path`, `scene pane Add registers a variant slot`

### 14. Layer row expand/collapse arrow

- **id**: `layer-row-expand`
- **input**: left-click
- **path**: `scene_scroll/<idx>/##layer`
- **precondition**: Row is visible after filtering; the arrow only toggles when the row has children, i.e. the layer is the currently playing animation (name == status.playing_animation) AND status.mc_tree.children is non-empty
- **disabled when**: has_children is false: ImGuiTreeNodeFlags_Leaf \| NoTreePushOnOpen are set, so there is no arrow to click
- **effect**: ImGui::TreeNodeEx("##layer", flags, "%s%s", name, play icon) toggles the node's open state in ImGui storage, which shows/hides the recursive RenderSceneNode children plus RenderUnresolvedSlots below it. Layer open state is NOT written back to App::State (unlike sublayers).
- **source**: `src/gui/gui_scene_panel.cpp:174`
- **tooltip**: yes
- **notes**: ONE entry describing a row drawn in a loop over cfg.anim_names (line 269-270), each wrapped in ImGui::PushID(idx). Flags include OpenOnArrow and SpanAvailWidth; the playing layer gets DefaultOpen and a green text color plus an ICON_PLAY suffix.
- **tests**: `properties tab Play posts SwitchAnimation for an idle layer`, `properties tab Replay posts SwitchAnimation for the playing layer`, `scene pane double-click on a layer posts SwitchAnimation`, `scene pane filter narrows the layer list`
- **audit correction**: Wrong source line. Line 174 holds only the `const bool open =` fragment; the item is submitted on line 175. -> src/gui/gui_scene_panel.cpp:175

### 15. Layer row (click on the label, not the arrow)

- **id**: `layer-row-select`
- **input**: left-click
- **path**: `scene_scroll/<idx>/##layer`
- **precondition**: Row is visible after filtering AND ImGui::IsItemToggledOpen() is false for this click (clicking the arrow selects nothing)
- **effect**: Scene::Select({.kind = Scene::Selection::Kind::Layer, .path = name, .name = name}) stores the selection in the file-local g_selection, which Scene::Current() exposes to other panels and which makes this row render with ImGuiTreeNodeFlags_Selected.
- **source**: `src/gui/gui_scene_panel.cpp:178`
- **tooltip**: yes
- **notes**: Same looped row as layer-row-expand; separate entry because selection and expansion are distinct gestures guarded against each other by IsItemToggledOpen().
- **tests**: `properties tab Play posts SwitchAnimation for an idle layer`, `properties tab Replay posts SwitchAnimation for the playing layer`, `scene pane double-click on a layer posts SwitchAnimation`, `scene pane filter narrows the layer list`

### 16. Sublayer tree node expand/collapse arrow

- **id**: `sublayer-node-expand`
- **input**: left-click
- **path**: `scene_scroll/<layer idx>/<node idx…>/<node.name>`
- **precondition**: Node is rendered (subtree matches filter) AND the node is not a leaf, i.e. NOT (node.enumerated && node.children.empty())
- **disabled when**: is_leaf is true (node.enumerated && no children): Leaf \| NoTreePushOnOpen flags are set so no arrow exists
- **effect**: ImGui::TreeNodeEx(node.name.c_str(), flags) toggles open state; when ImGui::IsItemToggledOpen() is true the code calls state.SetSublayerExpanded(node.path, open) to persist expansion in App::State, and when open it recurses into node.children and calls TreePop().
- **source**: `src/gui/gui_scene_panel.cpp:107`
- **notes**: Recursive looped row. A non-empty filter forces DefaultOpen on every node, so a manual collapse is overridden while filtering.
- **tests**: none
- **audit correction**: The effect is materially understated and therefore wrong about what expanding does. It claims SetSublayerExpanded merely 'persists expansion in App::State'. Expansion is in fact the ONLY trigger for the render thread to enumerate that node's children, so the entry hides a cross-thread AFP side effect. -> ImGui::TreeNodeEx(node.name.c_str(), flags) toggles open state; when ImGui::IsItemToggledOpen() is true, line 123 calls state.SetSublayerExpanded(node.path, open), which App::State forwards to IfsCatalog::SetSublayerExpanded (src/state/app_state.cpp:70, src/state/ifs_catalog.cpp:70-78) - a GLOBAL path list, not per-IFS unlike the visibility overrides. The render thread reads that list at src/render_live.cpp:261 (App::Global().GetSublayerExpanded() passed into BuildSubLayerTree) and BuildSubLayerLevel only resolves a node id via afp_mc_get_id_by_path and recurses into afp_mc_enumerate_children for paths present in the list (src/render_live.cpp:167-182), setting node.enumerated = true only then. So expanding a node causes a live AFP child enumeration, and collapsing stops it. The rebuild is throttled to one every 15 status ticks (src/render_live.cpp:258-260), so the children appear a beat later, and until node.enumerated flips the node is never is_leaf, which is why unexpanded leaves still show an arrow.

### 17. Sublayer tree node (click on the label)

- **id**: `sublayer-node-select`
- **input**: left-click
- **path**: `scene_scroll/<layer idx>/<node idx…>/<node.name>`
- **precondition**: Node is rendered (subtree matches filter) AND ImGui::IsItemClicked(ImGuiMouseButton_Left) AND ImGui::IsItemToggledOpen() is false
- **effect**: Scene::Select({.kind = Scene::Selection::Kind::Child, .path = node.path, .name = node.name}) sets g_selection; the row then renders with ImGuiTreeNodeFlags_Selected and other panels read it via Scene::Current().
- **source**: `src/gui/gui_scene_panel.cpp:109`
- **notes**: Recursive looped row; distinct gesture from the arrow toggle, guarded by !toggled_open. Non-interactive decorations sit on the same line: a TextDisabled "(x, y)" when FindChildPos finds the child in status.mc_children with have_pos, and a colored "variant" badge when FindSlot matches a valid cfg slot.
- **tests**: none

### 18. Unresolved / unmatched variant slot row

- **id**: `unresolved-slot-selectable`
- **input**: left-click
- **path**: `scene_scroll/<layer idx>/<slot.path>/   <slot.path>`
- **precondition**: Parent (playing) layer row is open AND the slot is not already represented in the tree, i.e. NOT (slot.is_valid && NodeMatchesPath(status.mc_tree, slot.path))
- **effect**: ImGui::Selectable(row, selected) returning true calls Scene::Select({.kind = Scene::Selection::Kind::Child, .path = slot.path, .name = slot.path}), selecting the slot path as if it were a clip child.
- **source**: `src/gui/gui_scene_panel.cpp:142`
- **notes**: ONE entry for a row drawn in a loop over cfg.slots (line 135), each wrapped in ImGui::PushID(slot.path.c_str()); the visible label is the slot path prefixed by three spaces. A TextDisabled "(slot)" / "(unresolved slot)" marker follows on the same line and is not clickable. This row is NOT filtered by the clip filter.
- **tests**: none
- **audit correction**: Incomplete precondition. It states only that the parent layer row is open, but RenderUnresolvedSlots is called at line 192 inside `if (has_children && open)`, so the rows are also gated on the playing layer having a non-empty status.mc_tree.children. -> Precondition: the row is under the currently playing layer (name == status.playing_animation), status.mc_tree.children is non-empty (has_children), that layer row is open, AND the slot is not already represented in the tree (NOT (slot.is_valid && NodeMatchesPath(status.mc_tree, slot.path)), line 136). If the playing layer has no enumerated children the row is a Leaf and no unresolved slot is ever listed, even though cfg.slots is non-empty.

### 19. Scene tree scroll region

- **id**: `scene-scroll-child`
- **input**: scroll
- **path**: `scene_scroll`
- **precondition**: An IFS is active AND cfg.anim_names is non-empty
- **effect**: ImGui::BeginChild("scene_scroll", ImVec2(0, -38.0F), 0) creates a scrollable child holding every layer row and its expanded sublayer tree; mouse wheel / scrollbar drag scrolls it. Purely view-level, no state change.
- **source**: `src/gui/gui_scene_panel.cpp:267`
- **notes**: Height reserves 38px at the bottom for the add-slot row, which stays outside the scroll region.
- **tests**: `properties tab Play posts SwitchAnimation for an idle layer`, `properties tab Replay posts SwitchAnimation for the playing layer`, `scene pane child tree expansion is recorded`, `scene pane child visibility checkbox records a sublayer override` (+6 more)
- **audit correction**: Effect implies general scrolling; the child is submitted with flags = 0, so only vertical scrolling exists. -> ImGui::BeginChild("scene_scroll", ImVec2(0, -38.0F), 0) with no ImGuiWindowFlags_HorizontalScrollbar: mouse wheel and vertical scrollbar drag scroll it, and keyboard nav auto-scrolls to the focused item, but deeply indented sublayer rows are clipped at the right edge with no way to scroll horizontally (Shift+wheel does nothing). Purely view-level, no state change.

### 20. Empty-state message when no IFS is selected

- **id**: `scene-empty-no-ifs-gate`
- **input**: state-change
- **precondition**: state.ActiveIfs() is empty
- **disabled when**: An IFS is active
- **effect**: RenderScenePane draws Gui::SectionHeader(ICON_SCENE, "Scene", nullptr) plus a TextDisabled hint to select an IFS on the left, then returns early. Every interaction listed above is hidden in this state.
- **source**: `src/gui/gui_scene_panel.cpp:241`
- **notes**: Visibility gate entry, no user input of its own. Recorded because it gates the whole surface.

### 21. Empty-state message when afplist.xml lists no layers

- **id**: `scene-empty-no-layers-gate`
- **input**: state-change
- **precondition**: An IFS is active AND cfg.anim_names.empty()
- **disabled when**: cfg.anim_names is non-empty
- **effect**: Draws the Scene section header with the counts suffix then TextDisabled("No layers listed in afplist.xml.") and returns early, hiding the filter box, the scroll region and the add-slot row.
- **source**: `src/gui/gui_scene_panel.cpp:254`
- **notes**: Visibility gate entry. The header suffix built at line 249 reports layer / bitmap / slot counts and is non-interactive.

### 22. Implicit selection reset when the user picks a different IFS elsewhere

- **id**: `scene-selection-reset-on-ifs-change`
- **input**: state-change
- **precondition**: state.ActiveIfs() differs from the static s_last_active captured on the previous frame
- **disabled when**: Active IFS unchanged
- **effect**: Scene::Reset() clears g_selection to a default Selection{}, so no layer or clip row renders as selected after the user switches IFS in another panel.
- **source**: `src/gui/gui_scene_panel.cpp:236`
- **notes**: Not a widget: a side effect of a user action taken on another surface, included because it changes this pane's interactive state.

### 23. New variant slot path text box (hint "add slot by clip path, e.g. coin")

- **id**: `add-slot-input`
- **input**: text-entry
- **path**: `##new_slot`
- **precondition**: An IFS is active AND cfg.anim_names is non-empty
- **effect**: Types into the static buf[128] read by the adjacent Add button. Typing alone changes nothing else; the buffer is cleared to empty only when Add is pressed.
- **source**: `src/gui/gui_scene_panel.cpp:201`
- **notes**: Width is SetNextItemWidth(-72.0F), leaving room for the Add button. Buffer is a function-local static, so it survives IFS switches.
- **tests**: `scene pane Add refuses a duplicate slot path`, `scene pane Add registers a variant slot`, `scene pane prompts for an IFS while none is active`

### 24. Clip filter text box (hint "find clip...")

- **id**: `scene-filter-input`
- **input**: text-entry
- **path**: `##scene_filter`
- **precondition**: An IFS is active (state.ActiveIfs() non-empty) AND cfg.anim_names is non-empty
- **effect**: Types into the static filter_buf[128]. The text is lowercased into lower_filter and used to filter rows: RenderLayerRow returns early for layers whose lowercased name does not contain the filter unless the playing layer's mc_tree subtree matches (SubtreeMatches); RenderSceneNode returns early for nodes whose subtree does not match. A non-empty filter also adds ImGuiTreeNodeFlags_DefaultOpen to every sublayer node. No App::Command is posted and no state setter runs.
- **source**: `src/gui/gui_scene_panel.cpp:261`
- **notes**: Full-width (SetNextItemWidth(-FLT_MIN)). Filter state is a function-static buffer, so it persists across IFS switches even though Scene::Reset() clears the selection.
- **tests**: `scene pane filter narrows the layer list`, `scene pane prompts for an IFS while none is active`, `scene pane reports an IFS with no listed layers`

