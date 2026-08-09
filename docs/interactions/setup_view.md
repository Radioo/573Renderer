# Setup view

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `game-profile-combo-open` | Game profile combo (open/close the dropdown) | combo-select | yes | 1 |
| 2 | `game-profile-select-auto` | 'Auto (...)' entry in the game profile combo | combo-select | - | **none** |
| 3 | `game-profile-select-row` | Profile row in the game profile combo (one per GameProfile::All() entry) | combo-select | - | **none** |
| 4 | `render-preset-combo-open` | Render resolution preset combo (open/close) | combo-select | yes | 1 |
| 5 | `render-preset-select-custom` | 'Custom' row in the render preset combo | combo-select | - | **none** |
| 6 | `render-preset-select-row` | Resolution preset row in the render preset combo | combo-select | - | **none** |
| 7 | `setup-window-scrollbar-drag` *(audit)* | '##setup' window vertical scrollbar grab | drag | - | **none** |
| 8 | `error-banner-region` | Last-attempt-failed error banner | hover | - | **none** |
| 9 | `game-profile-combo-tooltip` | Game profile combo hover tooltip | hover | yes | 1 |
| 10 | `render-fps-tooltip` | Frame rate field hover tooltip | hover | yes | 2 |
| 11 | `render-preset-combo-tooltip` | Render resolution combo hover tooltip | hover | yes | 1 |
| 12 | `combo-popup-dismiss` *(audit)* | Combo popup dismissal (both '##game_profile' and '##render_preset') | key | - | 1 |
| 13 | `setup-card-nav-activate` *(audit)* | Keyboard activation of the focused setup control | key | - | 20 |
| 14 | `setup-card-nav-focus-move` *(audit)* | Keyboard focus traversal across the setup card items | key | - | 20 |
| 15 | `browse-game-dir` | Browse... button (game directory) | left-click | - | 3 |
| 16 | `extract-arc-button` | 'Extract .arc files...' button | left-click | - | 4 |
| 17 | `extract-customize-button` | 'Extract customize images...' button | left-click | - | 4 |
| 18 | `load-button` | Load button | left-click | - | 5 |
| 19 | `render-fps-quick-preset` | Quick frame-rate preset button (30 / 60 / 120 / 144) | left-click | - | **none** |
| 20 | `render-fps-step-minus` | InputInt step-down button ('-') on the frame rate field | left-click | - | **none** |
| 21 | `render-fps-step-plus` | InputInt step-up button ('+') on the frame rate field | left-click | - | **none** |
| 22 | `game-profile-popup-scroll` | Game profile combo popup list (scroll) | scroll | - | 1 |
| 23 | `render-preset-popup-scroll` *(audit)* | Render resolution preset combo popup list (scroll) | scroll | - | 1 |
| 24 | `setup-window-scroll` | Setup full-viewport window background (scroll) | scroll | - | **none** |
| 25 | `game-dir-input` | Game directory text field | text-entry | - | 1 |
| 26 | `numeric-field-text-editing` *(audit)* | Numeric text editing inside ##render_fps / ##render_w / ##render_h | text-entry | - | 1 |
| 27 | `render-fps-input` | Frame rate InputInt text field | text-entry | yes | 2 |
| 28 | `render-height-input` | Render height InputInt | text-entry | - | 1 |
| 29 | `render-width-input` | Render width InputInt | text-entry | - | 2 |

## Detail

### 1. Game profile combo (open/close the dropdown)

- **id**: `game-profile-combo-open`
- **input**: combo-select
- **path**: `##setup/setup_card/##game_profile`
- **precondition**: always
- **effect**: Opens the ImGui combo popup listing 'Auto (...)' plus every GameProfile::All() entry. Preview text is either the auto label ('Auto (matches: <name>)' when GameProfile::AutoDetect(g_dir_buf) succeeds, otherwise 'Auto (no match - select a profile; boot requires one)') or the selected profile's name. Opening/closing alone changes no state.
- **source**: `src/gui/gui_setup_view.cpp:121`
- **tooltip**: yes
- **notes**: Item width -FLT_MIN (full card width). Tooltip is attached to this item - see entry game-profile-combo-tooltip.
- **tests**: `setup view controls explain themselves on hover`

### 2. 'Auto (...)' entry in the game profile combo

- **id**: `game-profile-select-auto`
- **input**: combo-select
- **path**: `##setup/setup_card/##game_profile/Auto (matches: <name>) | Auto (no match - select a profile; boot requires one)`
- **precondition**: game profile combo popup is open
- **effect**: state.SetGameProfileSlug("") (clears the explicit override so EffectiveSetupSlug falls back to GameProfile::AutoDetect(g_dir_buf)), then PersistSetup(state, g_dir_buf) -> SetGameDir if changed + App::SaveCurrentSettings(). Indirectly re-gates the qpro render preset and the DDR-only extractor tools.
- **source**: `src/gui/gui_setup_view.cpp:122`
- **notes**: Label text is built into auto_label at line 100-106, so the exact string depends on whether auto-detection matched.
- **tests**: none

### 3. Profile row in the game profile combo (one per GameProfile::All() entry)

- **id**: `game-profile-select-row`
- **input**: combo-select
- **path**: `##setup/setup_card/##game_profile/<profile name>`
- **precondition**: game profile combo popup is open
- **effect**: state.SetGameProfileSlug(profiles[i].slug) then PersistSetup(state, g_dir_buf) -> SetGameDir if changed + App::SaveCurrentSettings(). The currently selected row calls SetItemDefaultFocus() so the popup opens focused on it.
- **source**: `src/gui/gui_setup_view.cpp:128`
- **notes**: ONE entry describing a row drawn in a loop over GameProfile::All(); there is one Selectable per registered profile, labelled with profiles[i].name.
- **tests**: none

### 4. Render resolution preset combo (open/close)

- **id**: `render-preset-combo-open`
- **input**: combo-select
- **path**: `##setup/setup_card/##render_preset`
- **precondition**: always
- **effect**: Opens the preset popup. Preview shows kPresets[shown_idx].label; shown_idx is re-derived from the current render size whenever the size changed since last frame (falls back to 'Custom' when no preset matches). Opening alone changes no state.
- **source**: `src/gui/gui_setup_view.cpp:226`
- **tooltip**: yes
- **notes**: Item width -120.
- **tests**: `setup view controls explain themselves on hover`

### 5. 'Custom' row in the render preset combo

- **id**: `render-preset-select-custom`
- **input**: combo-select
- **path**: `##setup/setup_card/##render_preset/Custom`
- **precondition**: render preset popup is open
- **effect**: Sets shown_idx to the Custom index only. Because kPresets[Custom].w == 0 the `if (kPresets[i].w > 0)` branch is skipped, so NO SetRenderSize and NO SaveCurrentSettings happen - the user is expected to type into the width/height InputInts below.
- **source**: `src/gui/gui_setup_view.cpp:230`
- **notes**: Same Selectable call site as the loop rows, but the w==0 sentinel makes it a no-op selection.
- **tests**: none

### 6. Resolution preset row in the render preset combo

- **id**: `render-preset-select-row`
- **input**: combo-select
- **path**: `##setup/setup_card/##render_preset/3840x2160 (GITADORA 4K) | 1920x1080 (IIDX) | 1280x720 (DDR) | 640x480 (legacy 4:3) | 1080x1920 (SDVX / jubeat portrait) | 720x1280 (SDVX old-era) | 520x704 (qpro avatar)`
- **precondition**: render preset popup is open; the '520x704 (qpro avatar)' row is additionally hidden unless EffectiveSetupSlug(state) == "iidx33" (or unless it happens to be the currently shown preset)
- **disabled when**: never (rows are skipped/hidden rather than disabled)
- **effect**: Sets shown_idx to the clicked row and, when the preset has w>0, calls state.SetRenderSize(w,h), updates the cached last_rw/last_rh, and PersistSetup() (SetGameDir if changed + App::SaveCurrentSettings()). Selected row calls SetItemDefaultFocus().
- **source**: `src/gui/gui_setup_view.cpp:230`
- **notes**: ONE entry for the loop over kPresets; the visibility gate at line 228 (`if (i == kQproIdx && !show_qpro_preset && i != shown_idx) continue;`) hides only the qpro row. The 'Custom' row is listed separately because its effect differs.
- **tests**: none

### 7. '##setup' window vertical scrollbar grab

- **id**: `setup-window-scrollbar-drag` *(audit)*
- **input**: drag
- **path**: `##setup/#SCROLLY`
- **precondition**: the leading Dummy spacer (line 412) plus the auto-sized 'setup_card' child exceed the viewport work area height, so ImGui draws a scrollbar on the '##setup' window
- **disabled when**: never - the window is created without ImGuiWindowFlags_NoScrollbar (line 404-406), unlike the two children which both pass NoScrollbar
- **effect**: Dragging (or click-jumping on) the scrollbar track scrolls the setup window vertically. Purely view scroll, no state change. Distinct interactive item from the wheel scroll already listed as setup-window-scroll.
- **source**: `src/gui/gui_setup_view.cpp:404`
- **notes**: Most likely to appear at the small end of the enforced minimum client size (kMinClientW/kMinClientH, src/gui/gui_window.cpp WM_GETMINMAXINFO) since the card content is roughly 560px tall, which is also the threshold used by the Dummy spacer at line 412.
- **tests**: none

### 8. Last-attempt-failed error banner

- **id**: `error-banner-region`
- **input**: hover
- **path**: `##setup/setup_card/setup_err`
- **precondition**: BootState == Failed AND GetBootError() is non-empty
- **disabled when**: never (non-interactive)
- **effect**: No effect. Read-only child window 'setup_err' (Borders\|AutoResizeY, NoScrollbar) showing 'Last attempt failed:' plus the wrapped boot error text. Listed because it is a distinct visibility gate the user can observe/hit-test but not activate.
- **source**: `src/gui/gui_setup_view.cpp:62`
- **notes**: NoScrollbar and AutoResizeY, so long error text expands the banner rather than scrolling it.
- **tests**: none

### 9. Game profile combo hover tooltip

- **id**: `game-profile-combo-tooltip`
- **input**: hover
- **path**: `##setup/setup_card/##game_profile`
- **precondition**: pointer hovering the '##game_profile' combo (IsItemHovered after EndCombo)
- **effect**: ImGui::SetTooltip shows a 6-line explanation of what the profile selects: per-game DLL ordinal tables and boot-call gating, why Konami's per-game afp-core.dll builds share export names but differ in ordinal mapping, that auto-detect keys off the game directory path, and that this control overrides a wrong detection.
- **source**: `src/gui/gui_setup_view.cpp:136`
- **tooltip**: yes
- **tests**: `setup view controls explain themselves on hover`
- **audit correction**: Precondition 'pointer hovering the ##game_profile combo' is incomplete in a way that changes observed behaviour: IsItemHovered() with default flags goes through IsWindowContentHoverable, which returns false while a popup owns nav focus. So once the combo popup is open, hovering the combo header shows NO tooltip. -> Precondition: pointer hovering the '##game_profile' combo header AND no combo/popup is currently open (default IsItemHovered lacks ImGuiHoveredFlags_AllowWhenBlockedByPopup, so an open popup suppresses the tooltip).

### 10. Frame rate field hover tooltip

- **id**: `render-fps-tooltip`
- **input**: hover
- **path**: `##setup/setup_card/##render_fps`
- **precondition**: pointer hovering the '##render_fps' InputInt (IsItemHovered immediately after it)
- **effect**: ImGui::SetTooltip explains that this is the preview render/advance rate, that animation speed is unchanged because dt = 1/fps, that it is purely smoothness vs CPU, that 60 matches DDR's authored rate while 120 is the historical default, and that video export uses its own fps in the Export panel.
- **source**: `src/gui/gui_setup_view.cpp:153`
- **tooltip**: yes
- **notes**: Tooltip binds to the InputInt, not to the quick-preset buttons.
- **tests**: `setup view controls explain themselves on hover`, `setup view fps commits once, on edit completion`
- **audit correction**: The note 'Tooltip binds to the InputInt, not to the quick-preset buttons' understates the hover target. IsItemHovered() after an InputInt with step != 0 tests the group rect, so the hover region is the text box PLUS the '-' and '+' buttons. (The claim about the quick-preset buttons is correct - they are separate items submitted after the tooltip call.) -> Precondition: pointer hovering ANY part of the ##render_fps InputInt group - the text box or either step button - because InputScalar's BeginGroup/EndGroup makes the group the last item. Not raised over the 30/60/120/144 buttons, and not raised while a combo popup is open.

### 11. Render resolution combo hover tooltip

- **id**: `render-preset-combo-tooltip`
- **input**: hover
- **path**: `##setup/setup_card/##render_preset`
- **precondition**: pointer hovering the '##render_preset' combo (IsItemHovered after EndCombo)
- **effect**: ImGui::SetTooltip explains that this is the native screen size of the game whose IFSes are being loaded, and that the user can pick a preset or type a custom WxH in the fields below.
- **source**: `src/gui/gui_setup_view.cpp:243`
- **tooltip**: yes
- **tests**: `setup view controls explain themselves on hover`
- **audit correction**: Same incompleteness as the profile combo tooltip: the IsItemHovered() at line 243 does not fire while the '##render_preset' popup is open, because default hover flags reject hover blocked by an open popup. -> Precondition: pointer hovering the '##render_preset' combo header AND no popup is open. The tooltip is unreachable while the user is browsing the preset rows.

### 12. Combo popup dismissal (both '##game_profile' and '##render_preset')

- **id**: `combo-popup-dismiss` *(audit)*
- **input**: key
- **path**: `##setup/setup_card/##game_profile | ##setup/setup_card/##render_preset`
- **precondition**: either combo popup is open
- **effect**: Escape, or a left-click anywhere outside the popup rectangle, closes the popup with NO selection applied - so no SetGameProfileSlug / SetRenderSize / SaveCurrentSettings runs. Distinct from clicking a row. Standard ImGui BeginPopup behaviour, not written in this file, but it is the only way to back out of an opened combo.
- **source**: `src/gui/gui_setup_view.cpp:121`
- **notes**: Second call site is line 226. While a popup is open the parent window's items stop reporting hovered (IsWindowContentHoverable rejects popup-blocked hover), which is why the combo tooltips at lines 136/243 do not fire while their own popup is open.
- **tests**: `setup view controls explain themselves on hover`

### 13. Keyboard activation of the focused setup control

- **id**: `setup-card-nav-activate` *(audit)*
- **input**: key
- **path**: `##setup/setup_card`
- **precondition**: keyboard nav focus is on a card item (ImGuiConfigFlags_NavEnableKeyboard, src/gui/gui_window.cpp:153)
- **disabled when**: the focused item is inside a BeginDisabled block (lines 289, 334, 375)
- **effect**: Space or Enter activates the focused item, running exactly the same code path as a left-click: Browse... opens the Win32 folder picker, a quick-fps button sets fps and persists, Load posts App::Cmd::BootGame, an extractor button opens its picker and calls Start(). On a combo, Space/Enter opens the popup, arrow keys move between rows (opening on the row that called SetItemDefaultFocus at line 132 / 239), Enter commits the row, Escape closes without applying. On a text/numeric field, Enter enters edit mode and Escape reverts the value.
- **source**: `src/gui/gui_window.cpp:153`
- **notes**: This is what makes SetItemDefaultFocus() at lines 132 and 239 observable - the inventory mentions those calls only as a note on the mouse-click entries.
- **tests**: `keyboard activation runs the same path as a click`, `keyboard nav moves focus between setup controls`, `scenario: first run picks a game, a profile and a resolution then boots`, `setup view Browse adopts the picked folder` (+16 more)

### 14. Keyboard focus traversal across the setup card items

- **id**: `setup-card-nav-focus-move` *(audit)*
- **input**: key
- **path**: `##setup/setup_card`
- **precondition**: always - io.ConfigFlags has ImGuiConfigFlags_NavEnableKeyboard set at src/gui/gui_window.cpp:153, so keyboard nav is live on this view
- **disabled when**: items wrapped in BeginDisabled (Load at line 375 when booting or the dir buffer is empty; the two extractor buttons at lines 289/334 while their job runs) are skipped by nav
- **effect**: Tab / Shift+Tab and the arrow keys move the ImGui nav cursor between the interactive items of the card in submission order: ##gamedir -> Browse... -> ##game_profile -> ##render_fps (group incl. -/+) -> 30/60/120/144 -> ##render_preset -> ##render_w -> ##render_h -> Load -> Extract .arc files... -> Extract customize images... Moving focus alone changes no state; ImGui auto-scrolls the '##setup' window to keep the focused item visible.
- **source**: `src/gui/gui_window.cpp:153`
- **notes**: Entirely absent from the inventory, which documents mouse input only. Inside an active text field Tab is consumed by the text edit and moves to the next inputable item.
- **tests**: `keyboard activation runs the same path as a click`, `keyboard nav moves focus between setup controls`, `scenario: first run picks a game, a profile and a resolution then boots`, `setup view Browse adopts the picked folder` (+16 more)

### 15. Browse... button (game directory)

- **id**: `browse-game-dir`
- **input**: left-click
- **path**: `##setup/setup_card/Browse...`
- **precondition**: always
- **effect**: Calls NativeDialog::BrowseForFolder(Gui::GetHwnd(), g_dir_buf) which opens the Win32 modal folder picker rooted at the current buffer. If a non-empty path is returned it is copied into g_dir_buf (clamped to 1023 chars) and PersistSetup() runs: state.SetGameDir(path) + App::SaveCurrentSettings().
- **source**: `src/gui/gui_setup_view.cpp:82`
- **notes**: Button size ImVec2(-FLT_MIN, 0) so it fills the remaining row width.
- **tests**: `scenario: first run picks a game, a profile and a resolution then boots`, `setup view Browse adopts the picked folder`, `setup view Browse keeps the current folder when cancelled`

### 16. 'Extract .arc files...' button

- **id**: `extract-arc-button`
- **input**: left-click
- **path**: `##setup/setup_card/Extract .arc files...`
- **precondition**: EffectiveSetupSlug(state) == "ddrworld" (the whole Tools section is only drawn for the DDR WORLD profile, line 428) AND no extraction already running
- **disabled when**: ArcExtract::GetStatus().running is true - ImGui::BeginDisabled at line 289
- **effect**: Opens NativeDialog::BrowseForFolder(Gui::GetHwnd(), g_dir_buf); if a folder is picked, calls ArcExtract::Start(picked) which kicks off the background recursive .arc unpack into '<folder>_extracted'. Progress is then polled each frame into the ProgressBar below (indeterminate 'Scanning... N found' plus current file while total_arcs == 0, determinate 'done/total' plus 'Extracting: <file>' afterwards), and a Done/Failed summary with the output dir once finished.
- **source**: `src/gui/gui_setup_view.cpp:290`
- **notes**: Button size ImVec2(200, 0). The ProgressBar and result text (lines 296-322) are display-only, not interactive.
- **tests**: `setup view arc extractor does nothing when the picker is cancelled`, `setup view arc extractor runs over the picked folder`, `setup view hides the DDR extractors for other profiles`, `setup view offers the DDR extractors for ddrworld`

### 17. 'Extract customize images...' button

- **id**: `extract-customize-button`
- **input**: left-click
- **path**: `##setup/setup_card/Extract customize images...`
- **precondition**: EffectiveSetupSlug(state) == "ddrworld" (Tools section gate at line 428) AND no customize extraction already running
- **disabled when**: CustomizeExtract::GetStatus().running is true - ImGui::BeginDisabled at line 334
- **effect**: Opens NativeDialog::BrowseForFolder(Gui::GetHwnd(), g_dir_buf); if a folder is picked, calls CustomizeExtract::Start(picked), which unpacks appeal boards / characters / lane art into a 'customize_assets' folder, renamed by id and losslessly optimised. Progress polls into the ProgressBar (indeterminate 'Scanning... N found' + current item while total == 0, else 'done/total' + 'Processing: <item>'), followed by a Done/Failed/'No customize .arc files found' summary with byte savings and output dir.
- **source**: `src/gui/gui_setup_view.cpp:335`
- **notes**: Button size ImVec2(220, 0). Progress/result text at lines 341-369 is display-only.
- **tests**: `setup view arc extractor does nothing when the picker is cancelled`, `setup view customize extractor runs over the picked folder`, `setup view hides the DDR extractors for other profiles`, `setup view offers the DDR extractors for ddrworld`

### 18. Load button

- **id**: `load-button`
- **input**: left-click
- **path**: `##setup/setup_card/Load`
- **precondition**: g_dir_buf is non-empty AND BootState != Booting
- **disabled when**: BootState == App::BootState::Booting OR the game directory buffer is empty (g_dir_buf[0] == '\0') - ImGui::BeginDisabled wraps the button
- **effect**: Builds App::Cmd::BootGame{game_dir = g_dir_buf}, fills render_width/render_height from state.GetRenderSize(), posts it via state.PostCommand(std::move(r)), then clears the previous error with state.SetBootError({}). While booting a 'Booting...' disabled label is shown on the same line.
- **source**: `src/gui/gui_setup_view.cpp:376`
- **notes**: Button size ImVec2(160, 32). The BeginDisabled gate is at line 375.
- **tests**: `keyboard activation runs the same path as a click`, `scenario: first run picks a game, a profile and a resolution then boots`, `setup view Load is disabled while no directory is set`, `setup view Load posts BootGame with the typed directory` (+1 more)

### 19. Quick frame-rate preset button (30 / 60 / 120 / 144)

- **id**: `render-fps-quick-preset`
- **input**: left-click
- **path**: `##setup/setup_card/30##fps_30 | 60##fps_60 | 120##fps_120 | 144##fps_144`
- **precondition**: always
- **effect**: Sets fps to the button's value and marks changed, so the shared tail runs: clamp to [1,1000], state.SetRenderFps(value), PersistSetup() (SetGameDir if changed + App::SaveCurrentSettings()).
- **source**: `src/gui/gui_setup_view.cpp:169`
- **notes**: ONE entry for a row drawn in a loop over kQuick = {30,60,120,144}; each button is labelled '<n>##fps_<n>' and sized to the width of the text '144' plus 2x FramePadding.x.
- **tests**: none

### 20. InputInt step-down button ('-') on the frame rate field

- **id**: `render-fps-step-minus`
- **input**: left-click
- **path**: `##setup/setup_card/##render_fps/-`
- **precondition**: always (InputInt was given step=5, so ImGui draws the -/+ buttons)
- **effect**: Decrements fps by 5 (or by the fast step 30 while Ctrl is held), marks changed, then clamp to [1,1000] + state.SetRenderFps() + PersistSetup() (SaveCurrentSettings). Click-and-hold auto-repeats.
- **source**: `src/gui/gui_setup_view.cpp:152`
- **notes**: Rendered by ImGui::InputInt because step (5) and step_fast (30) are non-zero.
- **tests**: none
- **audit correction**: has_tooltip is false. ImGui::InputInt with a non-zero step wraps the text box AND both step buttons in a BeginGroup/EndGroup (imgui_widgets.cpp InputScalar, whose in-source comment states the group exists so the caller can query item data), so the IsItemHovered() at line 153 covers the group rect. Hovering the '-' button therefore DOES raise the frame-rate tooltip. -> has_tooltip: true - the tooltip at lines 153-158 fires over the whole ##render_fps group, including this step button.

### 21. InputInt step-up button ('+') on the frame rate field

- **id**: `render-fps-step-plus`
- **input**: left-click
- **path**: `##setup/setup_card/##render_fps/+`
- **precondition**: always (InputInt was given step=5)
- **effect**: Increments fps by 5 (or by the fast step 30 with Ctrl held), marks changed, then clamp to [1,1000] + state.SetRenderFps() + PersistSetup() (SaveCurrentSettings). Click-and-hold auto-repeats.
- **source**: `src/gui/gui_setup_view.cpp:152`
- **tests**: none
- **audit correction**: has_tooltip is false, for the same reason as the '-' button: the IsItemHovered() at line 153 tests the EndGroup rect emitted by InputScalar, which encloses the text box plus both step buttons. -> has_tooltip: true - hovering the '+' button raises the frame-rate tooltip.

### 22. Game profile combo popup list (scroll)

- **id**: `game-profile-popup-scroll`
- **input**: scroll
- **path**: `##setup/setup_card/##game_profile`
- **precondition**: game profile combo popup is open AND the profile list is taller than the popup's default height
- **effect**: Scrolls the combo popup's item list. No state change.
- **source**: `src/gui/gui_setup_view.cpp:121`
- **notes**: Standard ImGui combo popup scrolling; the popup holds 1 + GameProfile::All().size() Selectables.
- **tests**: `setup view controls explain themselves on hover`

### 23. Render resolution preset combo popup list (scroll)

- **id**: `render-preset-popup-scroll` *(audit)*
- **input**: scroll
- **path**: `##setup/setup_card/##render_preset`
- **precondition**: the '##render_preset' combo popup is open AND the row list is taller than the popup box (ImGui clamps a combo popup to at most 8 visible items by default; kPresets holds 8 rows, or 7 when the qpro row is hidden)
- **effect**: Scrolls the combo popup's Selectable list. No state change. The inventory has this entry for the game-profile combo (game-profile-popup-scroll) but omits the exact same behaviour for the render-preset combo.
- **source**: `src/gui/gui_setup_view.cpp:226`
- **notes**: Row count comes from kPresets (lines 192-201) minus the qpro row when EffectiveSetupSlug(state) != "iidx33" (gate at line 228).
- **tests**: `setup view controls explain themselves on hover`

### 24. Setup full-viewport window background (scroll)

- **id**: `setup-window-scroll`
- **input**: scroll
- **path**: `##setup`
- **precondition**: always (window is created every frame while the app is in the setup/pre-boot view)
- **effect**: Scrolls the ImGui window '##setup' vertically when the centred 640px card plus the leading vertical Dummy spacer exceed the viewport work area. Purely view scroll; no state change. Window is created with NoTitleBar\|NoResize\|NoMove\|NoCollapse\|NoBringToFrontOnFocus so it cannot be dragged, resized or collapsed.
- **source**: `src/gui/gui_setup_view.cpp:404`
- **notes**: The inner child 'setup_card' (line 417) uses ImGuiChildFlags_AutoResizeY + ImGuiWindowFlags_NoScrollbar, so the card itself never scrolls; only the outer window does.
- **tests**: none

### 25. Game directory text field

- **id**: `game-dir-input`
- **input**: text-entry
- **path**: `##setup/setup_card/##gamedir`
- **precondition**: always
- **effect**: Typing edits the 1024-byte g_dir_buf. On every edit (InputText returns true) PersistSetup() runs: if the text differs from App::State::GameDir() it calls state.SetGameDir(buf), then App::SaveCurrentSettings() writes settings to disk. The buffer also feeds GameProfile::AutoDetect(), the Load button's enable gate, and the initial folder of all three Browse dialogs.
- **source**: `src/gui/gui_setup_view.cpp:78`
- **notes**: Standard ImGui text-field editing applies: click to focus, double-click/drag to select, Ctrl+A/C/V/X/Z, arrow/Home/End keys, Enter to deactivate, Escape to revert. Field width is -120 (leaves room for the Browse button). Buffer is re-synced from App::State by SyncDirBufFromState() only when the state value changes (line 43).
- **tests**: `setup view Load posts BootGame with the typed directory`

### 26. Numeric text editing inside ##render_fps / ##render_w / ##render_h

- **id**: `numeric-field-text-editing` *(audit)*
- **input**: text-entry
- **path**: `##setup/setup_card/##render_fps | ##setup/setup_card/##render_w | ##setup/setup_card/##render_h`
- **precondition**: the field is active (clicked into, or Enter pressed on it with nav focus)
- **effect**: The full ImGui text keymap applies inside all three numeric fields: click to place the caret, double-click / drag to select, Ctrl+A / Ctrl+C / Ctrl+V / Ctrl+X / Ctrl+Z, Home/End/arrows, Enter to deactivate, Escape to revert to the value the field held on activation. InputScalar adds ImGuiInputTextFlags_CharsDecimal \| CharsScientific, so letters and most punctuation are rejected on keystroke and filtered out of pasted text. ImGuiInputTextFlags_AutoSelectAll (passed at lines 152, 261, 267) preselects the whole value on activation, so the first character typed replaces it entirely.
- **source**: `src/gui/gui_setup_view.cpp:152`
- **notes**: The inventory documents this keymap only for the ##gamedir InputText (note on game-dir-input). Combined with the per-keystroke return value of InputInt (see the corrections), Escape does NOT undo the writes already pushed to App::State and settings.json by the intermediate keystrokes - it only restores the on-screen text.
- **tests**: `setup view custom resolution inputs clamp and apply`

### 27. Frame rate InputInt text field

- **id**: `render-fps-input`
- **input**: text-entry
- **path**: `##setup/setup_card/##render_fps`
- **precondition**: always
- **effect**: Typing a number and committing sets changed=true; the value is then clamped to [1,1000], state.SetRenderFps(fps) runs and PersistSetup() persists settings (SetGameDir if changed + App::SaveCurrentSettings()). Flag ImGuiInputTextFlags_AutoSelectAll means clicking into the field selects the whole value.
- **source**: `src/gui/gui_setup_view.cpp:152`
- **tooltip**: yes
- **notes**: Item width 120. Controls preview render/advance rate only (dt = 1/fps); video export fps is separate.
- **tests**: `setup view controls explain themselves on hover`, `setup view fps commits once, on edit completion`
- **audit correction**: Effect says 'Typing a number and committing sets changed=true'. ImGui::InputInt is called WITHOUT ImGuiInputTextFlags_EnterReturnsTrue (line 152 passes only AutoSelectAll), so InputScalar returns true on EVERY keystroke that changes the buffer, not on commit. Every intermediate value is therefore clamped, applied and written to disk. -> Each keystroke returns changed=true: fps is clamped to [1,1000], state.SetRenderFps(fps) runs and PersistSetup() calls App::SaveCurrentSettings(), which writes the settings file. Typing '144' over the preselected value applies and persists 1, then 14, then 144 (three disk writes). Escape afterwards restores the displayed text but does not roll back the already-persisted value. Also, the SetNextItemWidth(120) at line 151 is the width of the whole InputInt GROUP; the editable box is 120 minus two step buttons (2 * (GetFrameHeight() + ItemInnerSpacing.x)).

### 28. Render height InputInt

- **id**: `render-height-input`
- **input**: text-entry
- **path**: `##setup/setup_card/##render_h`
- **precondition**: always
- **effect**: On commit, h_changed is true: both width and height are clamped to [64,8192], state.SetRenderSize(w,h) runs and PersistSetup() persists (SetGameDir if changed + App::SaveCurrentSettings()). ImGuiInputTextFlags_AutoSelectAll selects the whole value on click.
- **source**: `src/gui/gui_setup_view.cpp:267`
- **notes**: step/step_fast are 0, so no -/+ buttons. Item width 100. Changing either field also makes the preset combo re-derive shown_idx next frame (line 218).
- **tests**: `setup view custom resolution inputs clamp and apply`
- **audit correction**: Effect says 'On commit, h_changed is true'. Same issue as the width field - line 267 passes only ImGuiInputTextFlags_AutoSelectAll, so there is no commit gate. -> Every keystroke sets h_changed and runs the shared tail: clamp both axes to [64,8192], state.SetRenderSize(w_val, h_val), PersistSetup() -> App::SaveCurrentSettings(). Intermediate digits are applied and persisted (e.g. typing '1080' applies 64, 64, 108, 1080).

### 29. Render width InputInt

- **id**: `render-width-input`
- **input**: text-entry
- **path**: `##setup/setup_card/##render_w`
- **precondition**: always
- **effect**: On commit, w_changed is true: both width and height are clamped to [64,8192], state.SetRenderSize(w,h) runs and PersistSetup() persists (SetGameDir if changed + App::SaveCurrentSettings()). ImGuiInputTextFlags_AutoSelectAll selects the whole value on click.
- **source**: `src/gui/gui_setup_view.cpp:261`
- **notes**: step and step_fast are 0, so ImGui draws NO -/+ buttons on this field. Item width 100.
- **tests**: `setup view custom resolution inputs clamp and apply`, `setup view render size commits once, on edit completion`
- **audit correction**: Effect says 'On commit, w_changed is true'. There is no EnterReturnsTrue flag at line 261, so InputInt returns true on every keystroke. -> Every keystroke sets w_changed: w_val and h_val are clamped to [64,8192], state.SetRenderSize(w_val, h_val) runs and PersistSetup() writes settings immediately. Typing '1920' over the AutoSelectAll-preselected value applies and persists 64 (clamp of 1), 64 (clamp of 19), 192, then 1920 - four SetRenderSize calls and four settings writes, and the intermediate clamps are real state changes, not display-only.

