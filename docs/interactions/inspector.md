# Inspector

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `properties-slot-visible-checkbox` | "Slot visible" checkbox | checkbox | - | 2 |
| 2 | `render-filter-checkbox` | "Filter (F7)" checkbox | checkbox | yes | 2 |
| 3 | `render-loop-master-checkbox` | "Loop master animation" checkbox | checkbox | yes | 3 |
| 4 | `render-show-mc-names-checkbox` | "Show MC names (F3)" checkbox | checkbox | yes | 2 |
| 5 | `properties-slot-bitmap-combo-open` | Variant-slot bitmap combo (preview shows "(default)" or the overridden bitmap name) | combo-select | yes | 2 |
| 6 | `render-master-scale-slider-drag` | "Master scale" slider (0.25x - 4.00x, format "%.2fx") | drag | - | 1 |
| 7 | `properties-slot-bitmap-tooltip` *(audit)* | Hover tooltip on the variant-slot bitmap widget (combo or text field) | hover | yes | 2 |
| 8 | `render-background-tooltip` | Hover tooltip on the "Background" segmented control | hover | yes | **none** |
| 9 | `render-continuous-loop-tooltip` | Hover tooltip on the "Continuous loop" segmented control | hover | yes | 1 |
| 10 | `render-filter-tooltip` | Hover tooltip on "Filter (F7)" | hover | yes | 2 |
| 11 | `render-loop-master-tooltip` | Hover tooltip on "Loop master animation" | hover | yes | 3 |
| 12 | `render-master-scale-tooltip` | Hover tooltip on the master-scale row | hover | yes | 1 |
| 13 | `render-root-loop-tooltip` | Hover tooltip on the "Loop root" segmented control | hover | yes | 1 |
| 14 | `render-show-mc-names-tooltip` | Hover tooltip on "Show MC names (F3)" | hover | yes | 2 |
| 15 | `render-trim-frames-tooltip` | Hover tooltip on "Trim frames" | hover | yes | 1 |
| 16 | `properties-slot-bitmap-combo-keyboard-nav` *(audit)* | Bitmap combo popup keyboard navigation (arrows / Enter / Escape) | key | - | **none** |
| 17 | `render-master-scale-slider-ctrl-click` | "Master scale" slider keyboard entry | key | - | 1 |
| 18 | `inspector-tab-select` | Inspector tab (Properties / Render / Live / 3D scene / 2D package) | left-click | - | **none** |
| 19 | `properties-play-replay-button` | Play / Replay button (label is "Replay" when this layer is already the playing animation, otherwise "Play") | left-click | - | **none** |
| 20 | `properties-slot-bitmap-combo-default-item` | "(default)" entry inside the bitmap combo | left-click | - | 1 |
| 21 | `properties-slot-bitmap-combo-name-item` | Bitmap name entry inside the bitmap combo | left-click | - | **none** |
| 22 | `render-background-segment` | "Background" segmented control ("default" / "grey" / "black" / "red" / "green" / "blue") | left-click | yes | **none** |
| 23 | `render-continuous-loop-segment` | "Continuous loop" segmented control ("OFF" / "default" / "ON") | left-click | yes | **none** |
| 24 | `render-master-scale-preset-15x` | "1.5x" small button | left-click | yes | 1 |
| 25 | `render-master-scale-reset-1x` | "1.0x" small button | left-click | - | 1 |
| 26 | `render-mc-name-type-segment` | MC name type segmented control ("at clip pos" / "column") | left-click | - | **none** |
| 27 | `render-reset-live-overrides-button` | "Reset live overrides" button | left-click | - | 1 |
| 28 | `render-root-loop-segment` | "Loop root" segmented control ("Auto-hold" / "Force loop") | left-click | yes | **none** |
| 29 | `live-mc-names-list-scroll` | "MC names (N)" list child window | scroll | - | **none** |
| 30 | `properties-slot-bitmap-combo-popup-scroll` | Bitmap combo popup list | scroll | - | **none** |
| 31 | `properties-slot-bitmap-input` | Variant-slot bitmap name text field (hint: "bitmap name (blank = IFS default)") | text-entry | yes | 2 |
| 32 | `render-trim-frames-input` | "Trim frames" integer input | text-entry | yes | 1 |

## Detail

### 1. "Slot visible" checkbox

- **id**: `properties-slot-visible-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/Properties/Slot visible`
- **precondition**: Properties tab active, selection is a child/clip (kind != Kind::Layer), and FindSlotFor(cfg, sel) found a App::VariantSlot in state.MutConfig(ActiveIfs()).slots whose path equals sel.path or sel.name.
- **effect**: Writes App::VariantSlot::visible directly in the mutable IfsConfig held by App::State (state.MutConfig(active)). No command is posted and no settings save happens here; the flag is consumed by the variant-slot apply path.
- **source**: `src/gui/gui_inspector.cpp:80`
- **notes**: Shown under the "Variant slot" caption; the caption gains "(unresolved)" when !slot.is_valid (read-only text, line 78).
- **tests**: `selected clip with a slot exposes the slot controls`, `unresolved slots are listed under the playing layer`

### 2. "Filter (F7)" checkbox

- **id**: `render-filter-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/Render/Filter (F7)`
- **precondition**: Render tab active with the afp_modern backend (DrawFilterMcNameRows is only called from RenderRenderTabModern).
- **effect**: Toggles ov.filter_enabled on the local LiveOverrides copy and sets changed=true; state.ApplyLiveOverridesDelta(before, ov) at line 341 pushes it to the backend (afp-core set-filter, ord 0x032, on the active stream).
- **source**: `src/gui/gui_inspector.cpp:282`
- **tooltip**: yes
- **tests**: `ddr render tab offers background and reset only`, `render tab filter checkbox toggles the live override`

### 3. "Loop master animation" checkbox

- **id**: `render-loop-master-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/Render/Loop master animation`
- **precondition**: Render tab active with the afp_modern backend (RenderRenderTabModern). Not drawn in the DDR render tab.
- **effect**: App::Global().SetLoopMaster(loop) followed by App::SaveCurrentSettings() - persists immediately.
- **source**: `src/gui/gui_inspector.cpp:167`
- **tooltip**: yes
- **notes**: Value is read fresh each frame from App::Global().GetLoopMaster().
- **tests**: `ddr render tab offers background and reset only`, `inspector tabs switch the visible body`, `render tab loop-master checkbox persists to state`

### 4. "Show MC names (F3)" checkbox

- **id**: `render-show-mc-names-checkbox`
- **input**: checkbox
- **path**: `##inspector_tabs/Render/Show MC names (F3)`
- **precondition**: Render tab active with the afp_modern backend.
- **effect**: Toggles ov.show_mc_names and sets changed=true; ApplyLiveOverridesDelta at line 341 enables child enumeration (afp_mc_enumerate_children, ord 0x079). Turning it on also reveals the ##mc_name_type segmented row below it and, when the type is "column", the MC names list in the Live tab (line 437).
- **source**: `src/gui/gui_inspector.cpp:292`
- **tooltip**: yes
- **tests**: `inspector render tab controls all explain themselves`, `render tab MC-names checkbox reveals the name-type segmented`

### 5. Variant-slot bitmap combo (preview shows "(default)" or the overridden bitmap name)

- **id**: `properties-slot-bitmap-combo-open`
- **input**: combo-select
- **path**: `##inspector_tabs/Properties/##bitmap`
- **precondition**: Properties tab active, selection is a child with a matching VariantSlot, AND cfg.bitmap_names is NOT empty (when it is empty DrawSlotBitmapInput draws a text field instead).
- **effect**: ImGui::BeginCombo("##bitmap", preview) opens the dropdown popup listing "(default)" plus every name in cfg.bitmap_names; opening alone mutates nothing. Item width is set to -FLT_MIN (full width).
- **source**: `src/gui/gui_inspector.cpp:42`
- **tooltip**: yes
- **notes**: Tooltip is emitted by the IsItemHovered() check at line 86-87 in DrawSlotProperties, which fires for whichever of the combo/input was drawn last; it explains that this swaps the clip's bitmap and that "(default)" drops the override and replays the master because afp has no restore-authored call.
- **tests**: `slot bitmap combo picks an override and restores the default`, `slot bitmap falls back to a text field with no listed bitmaps`

### 6. "Master scale" slider (0.25x - 4.00x, format "%.2fx")

- **id**: `render-master-scale-slider-drag`
- **input**: drag
- **path**: `##inspector_tabs/Render/##master_scale`
- **precondition**: Render tab active with the afp_modern backend.
- **effect**: On any change: App::Global().SetMasterScale(scale) then App::SaveCurrentSettings().
- **source**: `src/gui/gui_inspector.cpp:243`
- **notes**: Item width -124.0f so it leaves room for the two SmallButtons on the same line. Clicking anywhere on the track also jumps the value (standard SliderFloat grab behaviour).
- **tests**: `render tab master-scale slider sets an arbitrary scale`

### 7. Hover tooltip on the variant-slot bitmap widget (combo or text field)

- **id**: `properties-slot-bitmap-tooltip` *(audit)*
- **input**: hover
- **path**: `##inspector_tabs/Properties/##bitmap`
- **precondition**: Properties tab active, selection is a child/clip (kind != Kind::Layer) and FindSlotFor found a matching App::VariantSlot, so DrawSlotProperties runs (line 73). The IsItemHovered() at line 86 follows whichever bitmap widget was just submitted: the ##bitmap combo when cfg.bitmap_names is non-empty (line 82), otherwise the ##bitmap InputTextWithHint (line 84). It is NOT attached to the "Slot visible" checkbox above it (line 80).
- **effect**: ImGui::SetTooltip (lines 87-89) - explains that this swaps the clip's bitmap and that "(default)" drops the override and replays the master so the timeline re-authors the original bitmap, because afp has no restore-authored call.
- **source**: `src/gui/gui_inspector.cpp:86`
- **tooltip**: yes
- **notes**: This is the 9th IsItemHovered/SetTooltip pair in the file (86, 171, 189, 209, 228, 257, 274, 285, 295). The inventory has dedicated hover entries for the other 8 but only flagged this one via has_tooltip:true on properties-slot-bitmap-combo-open / properties-slot-bitmap-input, so the one-entry-per-tooltip rule is violated exactly once. Verified the tooltip still targets the combo itself while its popup is open: ImGui::End() restores g.LastItemData from window_stack_data.ParentLastItemDataBackup (vendor/vcpkg/buildtrees/imgui/src/v1.92.7-b588f89316.clean/imgui.cpp:8230), so EndCombo -> EndPopup -> End does not leave LastItemData pointing at the last Selectable.
- **tests**: `slot bitmap combo picks an override and restores the default`, `slot bitmap falls back to a text field with no listed bitmaps`

### 8. Hover tooltip on the "Background" segmented control

- **id**: `render-background-tooltip`
- **input**: hover
- **path**: `##bg_color/5/blue`
- **precondition**: Pointer over the last segment button ("blue") of the row.
- **effect**: ImGui::SetTooltip - explains this is the AFP debug viewer F4 preview background, that default means transparent, and that it affects the live preview only because export uses its own Background setting.
- **source**: `src/gui/gui_inspector.cpp:274`
- **tooltip**: yes
- **tests**: none

### 9. Hover tooltip on the "Continuous loop" segmented control

- **id**: `render-continuous-loop-tooltip`
- **input**: hover
- **path**: `##live_cont/2/ON`
- **precondition**: Pointer over the last segment button ("ON") of the row.
- **effect**: ImGui::SetTooltip - explains OFF clears the CLayer flag (master reverts to gotoAndStop saturation), default leaves the engine default, ON applies the BG dispatcher's continuous-loop flag sequence (master advances past total_length, sub-clips evolve, required for BG 20), and that it is re-applied on the next stream switch.
- **source**: `src/gui/gui_inspector.cpp:209`
- **tooltip**: yes
- **notes**: Same last-item-hover behaviour as the other Segmented tooltips.
- **tests**: `render tab continuous-loop segmented applies each mode`

### 10. Hover tooltip on "Filter (F7)"

- **id**: `render-filter-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/Render/Filter (F7)`
- **precondition**: Pointer over the Filter checkbox.
- **effect**: ImGui::SetTooltip - explains it toggles the AFP layer filter (debug viewer F7) by calling afp-core set-filter (ord 0x032) on the active stream with filter id 0x80000000\|enable, the same call the scene's CLayer slot-32 wrapper makes.
- **source**: `src/gui/gui_inspector.cpp:285`
- **tooltip**: yes
- **tests**: `ddr render tab offers background and reset only`, `render tab filter checkbox toggles the live override`

### 11. Hover tooltip on "Loop master animation"

- **id**: `render-loop-master-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/Render/Loop master animation`
- **precondition**: Pointer over the Loop master checkbox (ImGui::IsItemHovered()).
- **effect**: ImGui::SetTooltip - explains that reaching the end of the master timeline re-plays from frame 0, useful for short title clips that would otherwise freeze on the last authored frame.
- **source**: `src/gui/gui_inspector.cpp:171`
- **tooltip**: yes
- **notes**: Separate entry per the one-entry-per-tooltip rule.
- **tests**: `ddr render tab offers background and reset only`, `inspector tabs switch the visible body`, `render tab loop-master checkbox persists to state`

### 12. Hover tooltip on the master-scale row

- **id**: `render-master-scale-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/Render/1.5x##scale_sdvx_old`
- **precondition**: Pointer over the "1.5x##scale_sdvx_old" small button - the IsItemHovered() at line 257 follows that button, so the tooltip does NOT appear over the slider or the 1.0x button.
- **effect**: ImGui::SetTooltip - explains that the value multiplies the master stream's transform matrix (default 1.0x) and that 1.5x matches what SDVX 7's BG dispatcher applies to SDVX-I-through-IV-era 720x1280 select_bg variants on a 1080x1920 game.
- **source**: `src/gui/gui_inspector.cpp:257`
- **tooltip**: yes
- **tests**: `render tab master-scale preset buttons set the scale`

### 13. Hover tooltip on the "Loop root" segmented control

- **id**: `render-root-loop-tooltip`
- **input**: hover
- **path**: `##root_loop/1/Force loop`
- **precondition**: Pointer over the LAST segment button of the row - IsItemHovered() after Gui::Segmented refers to the final Button submitted inside it ("Force loop").
- **effect**: ImGui::SetTooltip - explains Auto-hold (game default: mount once, root plays once and holds while nested children run) vs Force loop (ForceReplay plus the continuous-loop flag sequence, needed for one-shot masters like bg_common), and that it applies to the live preview now and to the next non-label export, persisted across restarts.
- **source**: `src/gui/gui_inspector.cpp:189`
- **tooltip**: yes
- **notes**: Because Gui::Segmented leaves LastItemData pointing at its final button, this tooltip only shows when hovering the last segment, not the whole row.
- **tests**: `render tab root-loop segmented switches to force mode`

### 14. Hover tooltip on "Show MC names (F3)"

- **id**: `render-show-mc-names-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/Render/Show MC names (F3)`
- **precondition**: Pointer over the Show MC names checkbox.
- **effect**: ImGui::SetTooltip - explains it enumerates the master's named child clips (afp_mc_enumerate_children ord 0x079), that "at clip pos" draws names over the preview while "column" lists them in the Live tab (matching the debug viewer F3/F6), and that it also feeds the child positions shown in the Scene tree.
- **source**: `src/gui/gui_inspector.cpp:295`
- **tooltip**: yes
- **tests**: `inspector render tab controls all explain themselves`, `render tab MC-names checkbox reveals the name-type segmented`

### 15. Hover tooltip on "Trim frames"

- **id**: `render-trim-frames-tooltip`
- **input**: hover
- **path**: `##inspector_tabs/Render/##live_trim`
- **precondition**: Pointer over the ##live_trim input.
- **effect**: ImGui::SetTooltip - explains 0 = no trim (loop forever via the engine) and >0 = at frame N (rendered frames since the last stream-switch/hot-swap) restart the master via ForceReplay so a loop of exactly N frames can be inspected at full framerate.
- **source**: `src/gui/gui_inspector.cpp:228`
- **tooltip**: yes
- **tests**: `render tab trim input feeds the live overrides`

### 16. Bitmap combo popup keyboard navigation (arrows / Enter / Escape)

- **id**: `properties-slot-bitmap-combo-keyboard-nav` *(audit)*
- **input**: key
- **path**: `##inspector_tabs/Properties/##bitmap (popup)`
- **precondition**: The ##bitmap combo popup is open (BeginCombo at line 42 returned true). io.ConfigFlags \|= ImGuiConfigFlags_NavEnableKeyboard is set at src/gui/gui_window.cpp:153, so keyboard nav is live for this popup.
- **effect**: Up/Down arrows move the nav highlight across the "(default)" Selectable (line 43) and the per-bitmap Selectables (line 50), wrapping because EndPopup issues NavMoveRequestTryWrapping(window, ImGuiNavMoveFlags_LoopY) (imgui.cpp:12485). Enter/Space activates the highlighted Selectable and runs exactly the same branch as a mouse click: for "(default)" slot.bitmap.clear() + bitmap_override=false + PostCommand(ForceReplay); for a name row slot.bitmap=b + bitmap_override=true. Escape closes the popup without touching the slot. ImGui::SetItemDefaultFocus() at line 54 seeds the initial nav focus on the currently selected bitmap row.
- **source**: `src/gui/gui_inspector.cpp:54`
- **notes**: The inventory covers the popup's mouse scroll (properties-slot-bitmap-combo-popup-scroll) but has no keyboard path for it, even though it does carry an equivalent keyboard entry for the master-scale slider. Keyboard nav is enabled process-wide, so the same Space/Enter activation reaches every Button/Checkbox/Segmented button on these tabs once nav-focused.
- **tests**: none

### 17. "Master scale" slider keyboard entry

- **id**: `render-master-scale-slider-ctrl-click`
- **input**: key
- **path**: `##inspector_tabs/Render/##master_scale`
- **precondition**: Ctrl+click (or double-click, per ImGui io.ConfigDragClickToInputText) on the ##master_scale slider.
- **effect**: Turns the slider into an inline text field; committing a typed value runs the same SetMasterScale + SaveCurrentSettings path, clamped to [0.25, 4.0].
- **source**: `src/gui/gui_inspector.cpp:243`
- **notes**: Built-in ImGui SliderFloat behaviour, not custom code in this file.
- **tests**: `render tab master-scale slider sets an arbitrary scale`
- **audit correction**: The precondition claims the text-entry mode is reachable by "Ctrl+click (or double-click, per ImGui io.ConfigDragClickToInputText)". Double-click does not enter text input on this widget, and io.ConfigDragClickToInputText is never set in this project. -> Ctrl+click only, or keyboard-nav activation. SliderScalar enters temp text input when `(clicked && g.IO.KeyCtrl) \|\| (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))` (vendor/vcpkg/buildtrees/imgui/src/v1.92.7-b588f89316.clean/imgui_widgets.cpp:3336). io.ConfigDragClickToInputText is set nowhere in src/ (a grep for ConfigFlags/ConfigDragClickToInputText/ConfigWindows across src/ returns only src/gui/gui_window.cpp:153 setting ImGuiConfigFlags_NavEnableKeyboard), and that flag only affects DragXXX widgets anyway, not SliderFloat. Double-clicking the slider just grabs and moves it. The nav path (Enter on a keyboard-focused slider) IS available because NavEnableKeyboard is on.

### 18. Inspector tab (Properties / Render / Live / 3D scene / 2D package)

- **id**: `inspector-tab-select`
- **input**: left-click
- **path**: `##inspector_tabs/<tab_label>`
- **precondition**: Gui::CollectActivePanels(PanelSlot::InspectorTab) returned a non-empty list for the active backend id; the tab bar is skipped entirely when tabs.empty(). Individual tabs additionally require their PanelDesc::visible() gate: "3D scene" needs Scene3dHost::Active(), "2D package" needs Gc2dHost::Active(); Properties/Render/Live have no visible gate but only exist in the backend's panel set.
- **effect**: ImGui::BeginTabItem(tab->tab_label) makes that panel current; on the frame it is selected ImGui::Spacing() runs and tab->draw() is invoked, drawing that panel's body (Panels::RenderPropertiesTab / RenderRenderTabModern / RenderRenderTabDdr / RenderLiveTab / Scene3dPanel::Render / Gc2dPanel::Render). Tab selection state lives in ImGui's tab bar "##inspector_tabs".
- **source**: `src/gui/gui_inspector.cpp:446`
- **notes**: ONE entry describing a row drawn in a loop over the PanelDesc list. Labels come from src/gui/panel_registry.cpp: afp_modern -> "Properties", "Render", "Live"; afp_ddr -> "Render", "Live", "3D scene"; scene3d -> "3D scene", "2D package". Tab bar flags are FittingPolicyResizeDown only, so tabs are not reorderable and have no close buttons.
- **tests**: none

### 19. Play / Replay button (label is "Replay" when this layer is already the playing animation, otherwise "Play")

- **id**: `properties-play-replay-button`
- **input**: left-click
- **path**: `##inspector_tabs/Properties/Play  (or ##inspector_tabs/Properties/Replay)`
- **precondition**: Properties tab active AND Scene::Current().kind != Kind::None AND state.ActiveIfs() is non-empty AND the selection kind is Kind::Layer (otherwise DrawChildProperties runs instead). When the selection is None or no IFS is active the whole body is replaced by the disabled text "Select a layer or clip in the Scene pane." at line 144.
- **effect**: state.PostCommand(AfpCmd::Wrap(AfpCmd::SwitchAnimation{.name = sel.name, .label = ""})) - queues an afp command that switches/restarts the master stream to the selected afplist layer with no label.
- **source**: `src/gui/gui_inspector.cpp:107`
- **notes**: Fixed size ImVec2(110,0). The button label itself changes with is_playing (sel.name == status.playing_animation), which also gates the "%u frames" mono readout and the green "playing" text above it (read-only, lines 99-105).
- **tests**: none

### 20. "(default)" entry inside the bitmap combo

- **id**: `properties-slot-bitmap-combo-default-item`
- **input**: left-click
- **path**: `##inspector_tabs/Properties/##bitmap (popup)/(default)`
- **precondition**: The bitmap combo popup is open (BeginCombo returned true).
- **effect**: slot.bitmap.clear(); slot.bitmap_override = false; then App::Global().PostCommand(AfpCmd::Wrap(AfpCmd::ForceReplay{})) so the timeline re-authors the original bitmap.
- **source**: `src/gui/gui_inspector.cpp:43`
- **notes**: Rendered selected when slot.bitmap is empty and !slot.bitmap_override.
- **tests**: `slot bitmap combo picks an override and restores the default`

### 21. Bitmap name entry inside the bitmap combo

- **id**: `properties-slot-bitmap-combo-name-item`
- **input**: left-click
- **path**: `##inspector_tabs/Properties/##bitmap (popup)/<bitmap name>`
- **precondition**: The bitmap combo popup is open; one row exists per entry in cfg.bitmap_names.
- **effect**: slot.bitmap = b; slot.bitmap_override = true. No command is posted from this branch (the override is picked up by the variant-slot apply path).
- **source**: `src/gui/gui_inspector.cpp:50`
- **notes**: ONE entry for a row drawn in a loop over cfg.bitmap_names. The currently selected row calls ImGui::SetItemDefaultFocus() so the popup opens scrolled to it.
- **tests**: none

### 22. "Background" segmented control ("default" / "grey" / "black" / "red" / "green" / "blue")

- **id**: `render-background-segment`
- **input**: left-click
- **path**: `##bg_color/<index>/default | grey | black | red | green | blue (inside ##inspector_tabs/Render)`
- **precondition**: Render tab active. Drawn by both RenderRenderTabModern (line 337) and RenderRenderTabDdr (line 352), i.e. for the afp_modern AND afp_ddr backends. Out-of-range stored values (bg_color_index < -1 or > 4) display as "default".
- **disabled when**: Clicking the already-active segment is a no-op (Gui::Segmented's `&& !active` guard).
- **effect**: Sets ov.bg_color_index = idx - 1 on the local LiveOverrides copy and changed=true; state.ApplyLiveOverridesDelta(before, ov) then runs (line 341 in the modern tab, line 354 in the DDR tab).
- **source**: `src/gui/gui_inspector.cpp:270`
- **tooltip**: yes
- **notes**: ONE entry for the six-button segmented row drawn in Gui::Segmented's loop.
- **tests**: none

### 23. "Continuous loop" segmented control ("OFF" / "default" / "ON")

- **id**: `render-continuous-loop-segment`
- **input**: left-click
- **path**: `##live_cont/<index>/OFF | ##live_cont/<index>/default | ##live_cont/<index>/ON (inside ##inspector_tabs/Render)`
- **precondition**: Render tab active with the afp_modern backend.
- **disabled when**: Clicking the already-active segment is a no-op (Gui::Segmented's `&& !active` guard).
- **effect**: Sets ov.continuous_loop_mode = idx - 1 on the local LiveOverrides copy and sets changed=true; at the end of RenderRenderTabModern (line 341) state.ApplyLiveOverridesDelta(before, ov) is called with the before/after pair.
- **source**: `src/gui/gui_inspector.cpp:205`
- **tooltip**: yes
- **notes**: ONE entry for the three-button segmented row. Index mapping: displayed idx = mode + 1, so OFF=-1, default=0, ON=+1.
- **tests**: none

### 24. "1.5x" small button

- **id**: `render-master-scale-preset-15x`
- **input**: left-click
- **path**: `##inspector_tabs/Render/1.5x##scale_sdvx_old`
- **precondition**: Render tab active with the afp_modern backend; drawn SameLine after the 1.0x button.
- **effect**: App::Global().SetMasterScale(1.5F) then App::SaveCurrentSettings().
- **source**: `src/gui/gui_inspector.cpp:253`
- **tooltip**: yes
- **tests**: `render tab master-scale preset buttons set the scale`

### 25. "1.0x" small button

- **id**: `render-master-scale-reset-1x`
- **input**: left-click
- **path**: `##inspector_tabs/Render/1.0x##scale_reset`
- **precondition**: Render tab active with the afp_modern backend; drawn SameLine after the master-scale slider.
- **effect**: App::Global().SetMasterScale(1.0F) then App::SaveCurrentSettings().
- **source**: `src/gui/gui_inspector.cpp:248`
- **tests**: `render tab master-scale preset buttons set the scale`

### 26. MC name type segmented control ("at clip pos" / "column")

- **id**: `render-mc-name-type-segment`
- **input**: left-click
- **path**: `##mc_name_type/<index>/at clip pos | ##mc_name_type/<index>/column (inside ##inspector_tabs/Render)`
- **precondition**: Render tab active with the afp_modern backend AND ov.show_mc_names is true - the row is hidden entirely otherwise (`if (ov.show_mc_names)` at line 302). Drawn indented by 24px.
- **disabled when**: Clicking the already-active segment is a no-op (Gui::Segmented's `&& !active` guard).
- **effect**: Writes directly into ov.mc_name_type (0 = at clip pos, 1 = column) and sets changed=true; ApplyLiveOverridesDelta at line 341 applies it. Selecting "column" (1) is also what makes the Live tab's MC names list appear.
- **source**: `src/gui/gui_inspector.cpp:305`
- **notes**: ONE entry for the two-button segmented row. This is the only Segmented row in the file with no tooltip.
- **tests**: none

### 27. "Reset live overrides" button

- **id**: `render-reset-live-overrides-button`
- **input**: left-click
- **path**: `##inspector_tabs/Render/Reset live overrides`
- **precondition**: Render tab active. Drawn by both RenderRenderTabModern (line 343) and RenderRenderTabDdr (line 355).
- **effect**: state.SetLiveOverrides(App::State::LiveOverrides{}) - replaces the whole live-override struct with a default-constructed one (clears continuous_loop_mode, trim_frames, bg_color_index, filter_enabled, show_mc_names, mc_name_type). Does not touch the persisted loop-master / root-loop / master-scale settings.
- **source**: `src/gui/gui_inspector.cpp:312`
- **tests**: `render tab reset button clears every live override`
- **audit correction**: The effect enumerates the fields a default-constructed LiveOverrides clears and gets the list wrong: it omits the `paused` field entirely, and it implies bg_color_index resets to a cleared/zero value. -> App::State::LiveOverrides (src/state/live_controls.h:26-34) has SEVEN fields: continuous_loop_mode(0), trim_frames(0), bg_color_index(-1), mc_name_type(0), filter_enabled(false), paused(false), show_mc_names(false). state.SetLiveOverrides(LiveOverrides{}) assigns the whole struct after ClampOverrides (src/state/live_controls.cpp:75-79), so the button ALSO clears `paused` - a preview paused from the transport resumes when "Reset live overrides" is clicked, which is a side effect the current entry does not mention. And bg_color_index resets to -1, i.e. the "default" (transparent) Background segment, not 0 ("grey").

### 28. "Loop root" segmented control ("Auto-hold" / "Force loop")

- **id**: `render-root-loop-segment`
- **input**: left-click
- **path**: `##root_loop/<index>/Auto-hold  |  ##root_loop/<index>/Force loop  (PushID("##root_loop") then PushID(i) around each Button, inside ##inspector_tabs/Render)`
- **precondition**: Render tab active with the afp_modern backend.
- **disabled when**: Clicking the already-active segment is swallowed: Gui::Segmented only assigns when `ImGui::Button(...) && !active` (src/gui/gui_widgets.cpp:28), so the current segment is effectively inert.
- **effect**: App::Global().SetRootLoopMode(idx == 1 ? RootLoopMode::Force : RootLoopMode::Hold) then App::SaveCurrentSettings().
- **source**: `src/gui/gui_inspector.cpp:184`
- **tooltip**: yes
- **notes**: ONE entry for the two-button segmented row (Gui::Segmented loops over kItems). Each segment is a real ImGui::Button, so each is independently hoverable/clickable.
- **tests**: none

### 29. "MC names (N)" list child window

- **id**: `live-mc-names-list-scroll`
- **input**: scroll
- **path**: `##inspector_tabs/Live/mc_names_list`
- **precondition**: Live tab active AND ov.show_mc_names is true AND ov.mc_name_type == 1 ("column") - gate at line 437 - AND status.mc_children is non-empty (early return at line 411).
- **effect**: Scrolls the bordered BeginChild("mc_names_list", ImVec2(0,140), 1) region, which lists one mono-font text row per enumerated child clip (name plus "(x, y)" when c.have_pos). Read-only: no widget inside is clickable.
- **source**: `src/gui/gui_inspector.cpp:415`
- **notes**: Fixed 140px height with border; rows come from a loop over status.mc_children. Also drag-scrollable via ImGui's scrollbar. The rest of the Live tab (Live state section line 360, File info section line 386) is pure text with no interactive widgets, though File info is itself gated on live.have_file_info (line 436) and the Live state block falls back to "(no active layer info)" when neither live.have_layer_info nor live.have_mc_playhead is set.
- **tests**: none

### 30. Bitmap combo popup list

- **id**: `properties-slot-bitmap-combo-popup-scroll`
- **input**: scroll
- **path**: `##inspector_tabs/Properties/##bitmap (popup)`
- **precondition**: The bitmap combo popup is open and cfg.bitmap_names is long enough to exceed the popup height.
- **effect**: Scrolls the ImGui combo popup child window; no state change.
- **source**: `src/gui/gui_inspector.cpp:48`
- **notes**: Standard ImGui combo popup scrolling; the list length is data-driven (one row per bitmap name).
- **tests**: none

### 31. Variant-slot bitmap name text field (hint: "bitmap name (blank = IFS default)")

- **id**: `properties-slot-bitmap-input`
- **input**: text-entry
- **path**: `##inspector_tabs/Properties/##bitmap`
- **precondition**: Properties tab active, selection is a child with a matching VariantSlot, AND cfg.bitmap_names IS empty (no enumerated bitmap list for this IFS).
- **effect**: On every edit: slot.bitmap = buf; slot.bitmap_override = (buf[0] != '\0'). The field is seeded each frame from slot.bitmap into a 128-byte stack buffer, so typed text longer than 127 chars is truncated.
- **source**: `src/gui/gui_inspector.cpp:66`
- **tooltip**: yes
- **notes**: Same "##bitmap" id as the combo (mutually exclusive branches). Shares the bitmap-swap tooltip at line 86-87. Item width -FLT_MIN.
- **tests**: `slot bitmap combo picks an override and restores the default`, `slot bitmap falls back to a text field with no listed bitmaps`

### 32. "Trim frames" integer input

- **id**: `render-trim-frames-input`
- **input**: text-entry
- **path**: `##inspector_tabs/Render/##live_trim`
- **precondition**: Render tab active with the afp_modern backend.
- **effect**: ImGui::InputInt("##live_trim", &ov.trim_frames, 0, 0) writes the typed integer into the local LiveOverrides copy and sets changed=true; state.ApplyLiveOverridesDelta(before, ov) runs at line 341.
- **source**: `src/gui/gui_inspector.cpp:225`
- **tooltip**: yes
- **notes**: Step and step_fast are both 0, so ImGui draws NO -/+ buttons; the only input is keyboard text entry (Enter/focus-loss commits). Item width fixed at 120px.
- **tests**: `render tab trim input feeds the live overrides`
- **audit correction**: The notes say the field's "only input is keyboard text entry (Enter/focus-loss commits)". The commit semantics are wrong: InputInt here commits on EVERY keystroke, not on Enter/focus loss. The entry also omits that the value is clamped before it lands in state, and that clicking the field selects all of it. -> ImGui::InputInt("##live_trim", &ov.trim_frames, 0, 0) -> InputScalar with p_step == NULL (step/step_fast are 0, imgui_widgets.cpp:3929), which calls InputText WITHOUT ImGuiInputTextFlags_EnterReturnsTrue (explicitly unsupported, asserted at imgui_widgets.cpp:3788) and sets value_changed = DataTypeApplyFromText(...) on any returned edit (imgui_widgets.cpp:3825). So ov.trim_frames is written and state.ApplyLiveOverridesDelta(before, ov) at line 341 fires per character typed, not on Enter or focus loss. Escape reverts the field to its value at activation and ALSO returns true (imgui_widgets.cpp:5263-5271), so the revert propagates. InputScalar adds ImGuiInputTextFlags_AutoSelectAll (imgui_widgets.cpp:3804), so clicking into the field selects the whole value and the first keystroke replaces it. A typed negative survives in the local copy but is clamped by ClampOverrides (src/state/live_controls.cpp:65, o.trim_frames = std::max(o.trim_frames, 0)) inside ApplyLiveOverridesDelta before it reaches live_overrides_. The "step and step_fast are both 0, so ImGui draws NO -/+ buttons" part of the entry is correct (has_step_buttons = (p_step != NULL), imgui_widgets.cpp:3806).

