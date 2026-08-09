# Export modal

Every interaction on this surface. Generated inventory, cross-checked by a second
reader against the source (see README.md for method).

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `blend-loop-checkbox` | "Blend loop seam" checkbox | checkbox | yes | 3 |
| 2 | `hw-accel-checkbox` | "HW accel" checkbox (live) | checkbox | yes | 1 |
| 3 | `hw-accel-checkbox-disabled` | "HW accel" checkbox (placeholder, always-unchecked) | checkbox | yes | 1 |
| 4 | `limit-frames-checkbox` | "Limit frames" checkbox | checkbox | yes | 2 |
| 5 | `transparent-bg-checkbox` | "Transparent bg" checkbox | checkbox | yes | 2 |
| 6 | `format-combo-open` | Output format combo box | combo-select | yes | 7 |
| 7 | `res-preset-combo-open` | Output resolution preset combo | combo-select | yes | 3 |
| 8 | `bg-color-picker` | Colour picker popup opened from the background swatch | drag | - | **none** |
| 9 | `bg-color-swatch-dragdrop` *(audit)* | Background colour swatch as drag-and-drop source / target | drag | - | **none** |
| 10 | `modal-scrollbar-drag` *(audit)* | Export modal vertical scrollbar | drag | - | **none** |
| 11 | `modal-titlebar-drag` | Export modal title bar | drag | - | **none** |
| 12 | `bg-color-swatch-hover-tooltip` *(audit)* | Background colour swatch built-in hover tooltip | hover | yes | **none** |
| 13 | `blend-frames-tooltip` | Hover tooltip on "Blend frames" | hover | yes | 2 |
| 14 | `blend-loop-tooltip` | Hover tooltip on "Blend loop seam" | hover | yes | 3 |
| 15 | `crop-pick-tooltip` | Hover tooltip on the Pick region / Picking button | hover | yes | 1 |
| 16 | `format-combo-tooltip` | Hover tooltip on the format combo | hover | yes | 7 |
| 17 | `fps-tooltip` | Hover tooltip on the fps input | hover | yes | 1 |
| 18 | `hw-tooltip-av1-nvenc` | Hover tooltip on HW accel: AV1 via NVENC | hover | yes | 1 |
| 19 | `hw-tooltip-h264-no-nvenc` | Hover tooltip on HW accel: H.264 with no NVENC | hover | yes | 1 |
| 20 | `hw-tooltip-h264-nvenc` | Hover tooltip on HW accel: H.264 via NVENC | hover | yes | 1 |
| 21 | `hw-tooltip-png` | Hover tooltip on HW accel: PNG sequence | hover | yes | 1 |
| 22 | `hw-tooltip-unavailable` | Hover tooltip on HW accel: hardware unavailable | hover | yes | 1 |
| 23 | `hw-tooltip-vp9` | Hover tooltip on HW accel: WebM VP9 | hover | yes | 1 |
| 24 | `hw-tooltip-webp` | Hover tooltip on HW accel: WebP | hover | yes | 1 |
| 25 | `keyframe-tooltip` | Hover tooltip on the keyframe interval input | hover | yes | 1 |
| 26 | `limit-frames-tooltip` | Hover tooltip on "Limit frames" | hover | yes | 2 |
| 27 | `loop-count-tooltip` | Hover tooltip on the loop count input | hover | yes | 1 |
| 28 | `max-frames-tooltip` | Hover tooltip on the duration-preview text next to the frames input | hover | yes | **none** |
| 29 | `open-folder-tooltip` | Hover tooltip on "Open folder" | hover | yes | 3 |
| 30 | `quality-tooltip` | Hover tooltip on the quality slider | hover | yes | 1 |
| 31 | `res-preset-tooltip` | Hover tooltip on the resolution preset combo | hover | yes | 3 |
| 32 | `scale-button-tooltip` | Hover tooltip on a scale-multiplier button | hover | yes | **none** |
| 33 | `stem-input-tooltip` | Hover tooltip on the filename stem field | hover | yes | 3 |
| 34 | `transparent-bg-tooltip` | Hover tooltip on "Transparent bg" | hover | yes | 2 |
| 35 | `escape-clears-active-item` *(audit)* | Escape key while a widget in the modal is active | key | - | **none** |
| 36 | `modal-escape-close` | Escape key while the Export modal has focus | key | - | **none** |
| 37 | `modal-keyboard-nav` | Tab / Shift+Tab / arrow-key navigation between the modal's widgets | key | - | **none** |
| 38 | `numeric-input-text-keymap` *(audit)* | Text-editing keymap inside every InputInt on this surface | key | - | 1 |
| 39 | `stem-input-text-keymap` *(audit)* | Filename stem field text-editing keymap and mouse selection | key | yes | 3 |
| 40 | `advanced-header` | "Advanced" collapsing header | left-click | - | 2 |
| 41 | `bg-color-swatch` | Background colour swatch (ColorEdit3, NoInputs + NoLabel) | left-click | - | **none** |
| 42 | `blend-frames-step-buttons` | Blend frames -/+ step buttons | left-click | - | **none** |
| 43 | `cancel-export-button` | "Cancel" button | left-click | - | 1 |
| 44 | `close-modal-button` | "Close" button | left-click | - | 4 |
| 45 | `crop-clear-button` | "Clear" crop button | left-click | - | 1 |
| 46 | `crop-pick-button` | "Pick region" button | left-click | yes | 1 |
| 47 | `crop-picking-cancel-button` | "Picking..." button (armed state) | left-click | yes | 1 |
| 48 | `format-combo-item` | Format list entry inside the open combo | left-click | - | **none** |
| 49 | `fps-step-buttons` | fps InputInt -/+ step buttons | left-click | - | **none** |
| 50 | `keyframe-step-buttons` | keyframe interval -/+ step buttons | left-click | - | **none** |
| 51 | `loop-count-step-buttons` | Continuous loop count -/+ step buttons | left-click | - | **none** |
| 52 | `max-frames-step-buttons` | frames InputInt -/+ step buttons | left-click | - | **none** |
| 53 | `modal-click-outside` *(audit)* | Click anywhere outside the Export modal (dimmed background, the main render window, any other panel) | left-click | - | **none** |
| 54 | `open-folder-button` | "Open folder" button | left-click | yes | 3 |
| 55 | `res-preset-custom` | "Custom" entry in the resolution combo | left-click | - | **none** |
| 56 | `res-preset-fixed` | Fixed resolution entry in the resolution combo | left-click | - | **none** |
| 57 | `res-preset-native` | "Native (WxH)" entry in the resolution combo | left-click | - | **none** |
| 58 | `scale-button` | Scale-current multiplier button ("x0.5 (WxH)" / "x0.25 (WxH)" / "x0.1 (WxH)") | left-click | yes | **none** |
| 59 | `start-export-button` | "Start export" button | left-click | - | 16 |
| 60 | `bg-color-swatch-context-menu` *(audit)* | Background colour swatch right-click options popup | right-click | - | **none** |
| 61 | `modal-scroll` | Export modal body (scrollable popup content) | scroll | - | **none** |
| 62 | `quality-slider-drag` | quality SliderInt | slider | yes | 1 |
| 63 | `blend-frames-input` | "Blend frames" InputInt (text portion) | text-entry | yes | 2 |
| 64 | `crop-xywh-input` | Crop x / y / w / h InputInt | text-entry | - | 1 |
| 65 | `fps-input` | fps InputInt (text portion) | text-entry | yes | 1 |
| 66 | `keyframe-input` | "keyframe interval" InputInt (text portion) | text-entry | yes | 1 |
| 67 | `loop-count-input` | "Continuous loop count" InputInt (text portion) | text-entry | yes | 1 |
| 68 | `max-frames-input` | "frames" InputInt (max frame count, text portion) | text-entry | - | 1 |
| 69 | `quality-slider-ctrl-click` | quality SliderInt ctrl+click / double-click to type a value | text-entry | - | 1 |
| 70 | `res-height-input` | Output height InputInt | text-entry | - | 1 |
| 71 | `res-width-input` | Output width InputInt | text-entry | - | 1 |
| 72 | `stem-input` | Output filename stem text field | text-entry | yes | 3 |
| 73 | `modal-open` | Export modal window ("Export" popup) | window-message | - | n/a |

## Detail

### 1. "Blend loop seam" checkbox

- **id**: `blend-loop-checkbox`
- **input**: checkbox
- **path**: `Export/Blend loop seam##exp_blend`
- **precondition**: Advanced expanded
- **disabled when**: busy
- **effect**: Toggles g_blend_loop (posted as r.blend_loop) and reveals/hides the "Blend frames" input on the same line.
- **source**: `src/gui/gui_export_panel.cpp:201`
- **tooltip**: yes
- **notes**: Gate for blend-frames-input.
- **tests**: `export modal blend frame count hides while the seam is off`, `export modal controls all explain themselves`, `export modal loop count and blend seam reach the request`

### 2. "HW accel" checkbox (live)

- **id**: `hw-accel-checkbox`
- **input**: checkbox
- **path**: `Export/HW accel##exp_hw`
- **precondition**: VideoEncoder::HardwareAvailable(MediaSink::HardwareProbeFormat(current_format)) is true AND the format is AVIF, WebM_AV1 or MP4_H264
- **disabled when**: busy
- **effect**: Toggles g_prefer_hw; posted as r.prefer_hardware = g_prefer_hw && hw_applies in the StartExport request.
- **source**: `src/gui/gui_export_panel.cpp:514`
- **tooltip**: yes
- **notes**: Default true.
- **tests**: `export modal disables hardware encode for software-only formats`

### 3. "HW accel" checkbox (placeholder, always-unchecked)

- **id**: `hw-accel-checkbox-disabled`
- **input**: checkbox
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hardware is unavailable OR the format cannot use hardware (WebM_VP9, WebP_Anim, PNG_Sequence, MP4_HEVC_Alpha)
- **disabled when**: always (this variant only exists in the disabled branch)
- **effect**: A separate ImGui::Checkbox bound to a local bool tmp=false is drawn inside BeginDisabled/EndDisabled; clicking it does nothing and g_prefer_hw is untouched.
- **source**: `src/gui/gui_export_panel.cpp:511`
- **tooltip**: yes
- **notes**: Distinct code path from the enabled checkbox; the two share the same ImGui ID.
- **tests**: `export modal disables hardware encode for software-only formats`

### 4. "Limit frames" checkbox

- **id**: `limit-frames-checkbox`
- **input**: checkbox
- **path**: `Export/Limit frames##exp_limit`
- **precondition**: Advanced expanded
- **disabled when**: busy
- **effect**: Toggles g_limit_frames. Turning it on while g_max_frames <= 0 seeds g_max_frames = 60. Controls r.max_frames = g_limit_frames ? g_max_frames : 0 in the StartExport request, and enables/disables the frames input beside it.
- **source**: `src/gui/gui_export_panel.cpp:160`
- **tooltip**: yes
- **tests**: `export modal controls all explain themselves`, `export modal frame limit is applied only while enabled`

### 5. "Transparent bg" checkbox

- **id**: `transparent-bg-checkbox`
- **input**: checkbox
- **path**: `Export/Transparent bg`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Toggles g_bg_transparent, which becomes r.bg_transparent in the StartExport request and also gates the background colour swatch below.
- **source**: `src/gui/gui_export_panel.cpp:489`
- **tooltip**: yes
- **notes**: Default true.
- **tests**: `export modal background toggle and colour reach the request`, `export modal disables the colour picker while the bg stays transparent`

### 6. Output format combo box

- **id**: `format-combo-open`
- **input**: combo-select
- **path**: `Export/##exp_fmt`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: ImGui::BeginCombo opens the format dropdown showing MediaSink::FormatLabel(MediaSink::FromIndex(g_format_idx)).
- **source**: `src/gui/gui_export_panel.cpp:78`
- **tooltip**: yes
- **notes**: Full-width (-FLT_MIN).
- **tests**: `export modal PNG sequence writes a directory path`, `export modal controls all explain themselves`, `export modal disables hardware encode for software-only formats`, `export modal filename and format drive the output path` (+3 more)

### 7. Output resolution preset combo

- **id**: `res-preset-combo-open`
- **input**: combo-select
- **path**: `Export/##exp_sz`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Opens the resolution dropdown; preview text is the currently matched preset label, recomputed by MatchPresetIdx() whenever g_out_w/g_out_h change externally.
- **source**: `src/gui/gui_export_panel.cpp:274`
- **tooltip**: yes
- **notes**: Preceded by a non-interactive ImGui::Text("Output resolution:") at line 272.
- **tests**: `export modal native resolution leaves the size unset`, `export modal resolution preset pins the output size`, `export modal scale button halves the current size`

### 8. Colour picker popup opened from the background swatch

- **id**: `bg-color-picker`
- **input**: drag
- **path**: `Export/##exp_bg_color/picker`
- **precondition**: The ##exp_bg_color picker popup is open
- **disabled when**: g_bg_transparent is true or busy (popup cannot be opened)
- **effect**: ImGui's built-in picker: dragging the saturation/value square and the hue bar, and typing into its RGB/HSV/Hex fields, writes g_bg_rgb in place. Right-clicking inside offers ImGui's copy-as / picker-type options.
- **source**: `src/gui/gui_export_panel.cpp:500`
- **notes**: Contents are ImGui core widgets, not code in this file; ColorEdit3 also accepts an ImGui colour drag-and-drop payload dropped onto the swatch.
- **tests**: none

### 9. Background colour swatch as drag-and-drop source / target

- **id**: `bg-color-swatch-dragdrop` *(audit)*
- **input**: drag
- **path**: `Export/##exp_bg_color`
- **precondition**: Modal open, g_bg_transparent is false; either dragging away from ##exp_bg_color while it is active, or dropping an ImGui colour payload onto it
- **disabled when**: g_bg_transparent is true or busy; also suppressed if ImGuiColorEditFlags_NoDragDrop were set, which it is not here
- **effect**: As a source, ColorButton begins a drag-drop source carrying IMGUI_PAYLOAD_TYPE_COLOR_3F/_4F with the current colour. As a target, ColorEdit4 accepts those payloads and memcpy's them straight into g_bg_rgb, marking the value changed - so a colour dragged from any other ImGui colour widget overwrites the export background colour (and therefore r.bg_r/g/b).
- **source**: `src/gui/gui_export_panel.cpp:500`
- **notes**: Verified in vendored imgui_widgets.cpp:6477-6482 (source) and the ColorEdit4 drag-drop target block that calls AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F/4F). The inventory mentions the drop case only in a free-text note on bg-color-picker; there is no record for it and none at all for the drag-source half.
- **tests**: none

### 10. Export modal vertical scrollbar

- **id**: `modal-scrollbar-drag` *(audit)*
- **input**: drag
- **path**: `Export/#SCROLLY`
- **precondition**: Modal open AND content height exceeds the vp->WorkSize.y - 48 cap set at line 627 (typically Advanced expanded on a short screen)
- **disabled when**: never (the scrollbar is drawn by the window, outside the BeginDisabled(busy) block, so it still works during an export)
- **effect**: Dragging the scrollbar grab scrolls the popup's content region, same as the wheel. No application state changes. Clicking the scrollbar track pages the view.
- **source**: `src/gui/gui_export_panel.cpp:627`
- **notes**: The inventory's modal-scroll covers only the wheel input; the scrollbar is a separate interactive item ImGui submits for the window.
- **tests**: none

### 11. Export modal title bar

- **id**: `modal-titlebar-drag`
- **input**: drag
- **path**: `Export`
- **precondition**: Modal open
- **effect**: No lasting effect: ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, {0.5,0.5}) re-centres the window every frame, so any drag is undone on the next frame.
- **source**: `src/gui/gui_export_panel.cpp:626`
- **notes**: Listed because it is a gesture a user will attempt; the Always condition makes it a no-op.
- **tests**: none

### 12. Background colour swatch built-in hover tooltip

- **id**: `bg-color-swatch-hover-tooltip` *(audit)*
- **input**: hover
- **path**: `Export/##exp_bg_color`
- **precondition**: Modal open, g_bg_transparent is false (swatch not disabled), hovering ##exp_bg_color
- **disabled when**: g_bg_transparent is true (BeginDisabled at line 498) or busy - ColorButton gates the tooltip on IsItemHovered(ImGuiHoveredFlags_ForTooltip), which returns false for disabled items
- **effect**: ImGui's ColorButton draws its own tooltip because ImGuiColorEditFlags_NoTooltip is NOT among the flags passed at line 501 (only NoInputs \| NoLabel). The tooltip shows a large colour preview plus the float and 8-bit RGB values and the hex code of g_bg_rgb. No application state changes.
- **source**: `src/gui/gui_export_panel.cpp:500`
- **tooltip**: yes
- **notes**: Verified in vendored imgui_widgets.cpp:6490 (ColorButton -> ColorTooltip). The inventory has NO hover entry for this widget at all; it is the only interactive widget in the file whose tooltip comes from ImGui rather than an explicit SetTooltip.
- **tests**: none

### 13. Hover tooltip on "Blend frames"

- **id**: `blend-frames-tooltip`
- **input**: hover
- **path**: `Export/Blend frames##exp_blendN`
- **precondition**: Hovering Blend frames##exp_blendN
- **disabled when**: busy; hidden when g_blend_loop is false
- **effect**: ImGui::SetTooltip explaining crossfade length (default 15), the softness/seam tradeoff, and that 0 means a hard cut at the best frame.
- **source**: `src/gui/gui_export_panel.cpp:215`
- **tooltip**: yes
- **tests**: `export modal blend frame count hides while the seam is off`, `export modal loop count and blend seam reach the request`

### 14. Hover tooltip on "Blend loop seam"

- **id**: `blend-loop-tooltip`
- **input**: hover
- **path**: `Export/Blend loop seam##exp_blend`
- **precondition**: Hovering Blend loop seam##exp_blend
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining the synthesized crossfade for backgrounds that do not loop cleanly, and warning that the real game does not do this.
- **source**: `src/gui/gui_export_panel.cpp:203`
- **tooltip**: yes
- **tests**: `export modal blend frame count hides while the seam is off`, `export modal controls all explain themselves`, `export modal loop count and blend seam reach the request`

### 15. Hover tooltip on the Pick region / Picking button

- **id**: `crop-pick-tooltip`
- **input**: hover
- **path**: `Export/Pick region##crop_pick  or  Export/Picking...##crop_pick`
- **precondition**: Hovering whichever of the two pick buttons is currently drawn
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining that it arms crop-selection mode, that you then click-and-drag on the render window, that Esc cancels, that the dialog reopens once picked, and that only pixels inside the rect are encoded.
- **source**: `src/gui/gui_export_panel.cpp:392`
- **tooltip**: yes
- **notes**: The IsItemHovered at line 391 sits after the if/else, so it covers both button variants; it is evaluated before the Clear button so it does not apply to Clear.
- **tests**: `export modal Picking button disarms crop mode`

### 16. Hover tooltip on the format combo

- **id**: `format-combo-tooltip`
- **input**: hover
- **path**: `Export/##exp_fmt`
- **precondition**: Hovering the ##exp_fmt combo (the closed preview widget)
- **disabled when**: busy
- **effect**: ImGui::SetTooltip describing each format's codec/container, alpha support, hardware-encode availability and browser playback tradeoffs, and noting the quality slider is ignored for PNG sequence.
- **source**: `src/gui/gui_export_panel.cpp:88`
- **tooltip**: yes
- **tests**: `export modal PNG sequence writes a directory path`, `export modal controls all explain themselves`, `export modal disables hardware encode for software-only formats`, `export modal filename and format drive the output path` (+3 more)

### 17. Hover tooltip on the fps input

- **id**: `fps-tooltip`
- **input**: hover
- **path**: `Export/fps##exp_fps`
- **precondition**: Hovering fps##exp_fps
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining it is the output framerate/timebase, that it does not change playback speed (the renderer samples at this cadence), and recommending 120 to avoid skipped frames.
- **source**: `src/gui/gui_export_panel.cpp:116`
- **tooltip**: yes
- **tests**: `export modal fps and quality reach the request`

### 18. Hover tooltip on HW accel: AV1 via NVENC

- **id**: `hw-tooltip-av1-nvenc`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is true AND format is AVIF or WebM_AV1
- **disabled when**: busy
- **effect**: ImGui::SetTooltip describing av1_nvenc as ~50x faster than libaom-av1, noting AVIF alpha stays on software and that it falls back to software if NVENC init fails.
- **source**: `src/gui/gui_export_panel.cpp:477`
- **tooltip**: yes
- **notes**: Fallback else-branch; reachable for AVIF and WebM_AV1.
- **tests**: `export modal disables hardware encode for software-only formats`
- **audit correction**: Incomplete/wrong precondition: line 477 is a bare else, so it also catches hw_available == true with current_format == MP4_HEVC_Alpha (HardwareProbeFormat maps HEVC_Alpha to AVIF, so hw_available can be true), and MP4_HEVC_Alpha fails format_can_use_hw at lines 505-506, meaning the checkbox is in the disabled placeholder branch and the tooltip is unreachable in that case. The entry claims the branch is simply 'reachable'. -> Precondition: hovering HW accel while hw_available is true AND the format is not VP9/WebP/PNG - i.e. AVIF, WebM_AV1 (reachable, checkbox enabled) or MP4_HEVC_Alpha (unreachable, checkbox disabled because format_can_use_hw is false, and IsItemHovered returns false for disabled items).

### 19. Hover tooltip on HW accel: H.264 with no NVENC

- **id**: `hw-tooltip-h264-no-nvenc`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is false AND format is MP4_H264
- **disabled when**: the checkbox is in its disabled placeholder branch in this state, and ImGui::IsItemHovered() returns false for disabled items, so this tooltip is effectively unreachable
- **effect**: ImGui::SetTooltip stating h264_nvenc is unavailable and that this ffmpeg build has no software H.264 encoder.
- **source**: `src/gui/gui_export_panel.cpp:446`
- **tooltip**: yes
- **notes**: Reached via DrawHwAccelTooltip dispatched from the single IsItemHovered at line 516.
- **tests**: `export modal disables hardware encode for software-only formats`

### 20. Hover tooltip on HW accel: H.264 via NVENC

- **id**: `hw-tooltip-h264-nvenc`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is true AND format is MP4_H264
- **disabled when**: busy
- **effect**: ImGui::SetTooltip describing h264_nvenc as fast hardware encode available on most NVIDIA GPUs, opaque only, and required because there is no software H.264 encoder.
- **source**: `src/gui/gui_export_panel.cpp:471`
- **tooltip**: yes
- **notes**: This branch is reachable (the checkbox is enabled in this state).
- **tests**: `export modal disables hardware encode for software-only formats`

### 21. Hover tooltip on HW accel: PNG sequence

- **id**: `hw-tooltip-png`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is true AND format is PNG_Sequence
- **disabled when**: same disabled-item hover caveat
- **effect**: ImGui::SetTooltip stating PNG sequence writes lossless per-frame files through WIC with no encoder, no NVENC and no quality slider.
- **source**: `src/gui/gui_export_panel.cpp:466`
- **tooltip**: yes
- **tests**: `export modal disables hardware encode for software-only formats`

### 22. Hover tooltip on HW accel: hardware unavailable

- **id**: `hw-tooltip-unavailable`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is false and the format is not MP4_H264
- **disabled when**: same disabled-item hover caveat as hw-tooltip-h264-no-nvenc
- **effect**: ImGui::SetTooltip stating hardware acceleration needs an NVIDIA GPU with AV1 encode (RTX 40-series or newer) and an ffmpeg built with nvcodec.
- **source**: `src/gui/gui_export_panel.cpp:450`
- **tooltip**: yes
- **tests**: `export modal disables hardware encode for software-only formats`

### 23. Hover tooltip on HW accel: WebM VP9

- **id**: `hw-tooltip-vp9`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is true AND format is WebM_VP9
- **disabled when**: same disabled-item hover caveat (VP9 never satisfies hw_applies)
- **effect**: ImGui::SetTooltip stating VP9 has no hardware encoder path on NVIDIA desktop GPUs and suggesting AVIF or WebM-AV1.
- **source**: `src/gui/gui_export_panel.cpp:456`
- **tooltip**: yes
- **tests**: `export modal disables hardware encode for software-only formats`

### 24. Hover tooltip on HW accel: WebP

- **id**: `hw-tooltip-webp`
- **input**: hover
- **path**: `Export/HW accel##exp_hw`
- **precondition**: Hovering HW accel while hw_available is true AND format is WebP_Anim
- **disabled when**: same disabled-item hover caveat
- **effect**: ImGui::SetTooltip stating libwebp_anim is software only because VP8 has no consumer-GPU encoder.
- **source**: `src/gui/gui_export_panel.cpp:461`
- **tooltip**: yes
- **tests**: `export modal disables hardware encode for software-only formats`

### 25. Hover tooltip on the keyframe interval input

- **id**: `keyframe-tooltip`
- **input**: hover
- **path**: `Export/keyframe interval##exp_keyint`
- **precondition**: Hovering keyframe interval##exp_keyint
- **disabled when**: busy or control hidden
- **effect**: ImGui::SetTooltip explaining 0 = auto (one keyframe per second), that larger values shrink the file for static scenes, that a value >= the total frame count gives a single keyframe, and that PNG and WebP ignore it.
- **source**: `src/gui/gui_export_panel.cpp:142`
- **tooltip**: yes
- **tests**: `export modal keyframe interval is offered only for video formats`

### 26. Hover tooltip on "Limit frames"

- **id**: `limit-frames-tooltip`
- **input**: hover
- **path**: `Export/Limit frames##exp_limit`
- **precondition**: Hovering Limit frames##exp_limit
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining that ON stops the export after a fixed captured-frame count (default 60) while OFF runs to the master animation's natural end-of-timeline, plus use cases.
- **source**: `src/gui/gui_export_panel.cpp:162`
- **tooltip**: yes
- **tests**: `export modal controls all explain themselves`, `export modal frame limit is applied only while enabled`

### 27. Hover tooltip on the loop count input

- **id**: `loop-count-tooltip`
- **input**: hover
- **path**: `Export/Continuous loop count##exp_loops`
- **precondition**: Hovering Continuous loop count##exp_loops
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining how many detected loops to capture, that recording continues past each loop boundary, and that "Limit frames" still caps it.
- **source**: `src/gui/gui_export_panel.cpp:195`
- **tooltip**: yes
- **tests**: `export modal loop count and blend seam reach the request`

### 28. Hover tooltip on the duration-preview text next to the frames input

- **id**: `max-frames-tooltip`
- **input**: hover
- **path**: `Export/(~%.2fs at %d fps)`
- **precondition**: Hovering the "(~%.2fs at %d fps)" TextDisabled item
- **disabled when**: !g_limit_frames or busy (the TextDisabled is inside the BeginDisabled block, and disabled items do not report hover)
- **effect**: ImGui::SetTooltip explaining the encoder receives exactly this many frames before auto-finalising, counted post-capture.
- **source**: `src/gui/gui_export_panel.cpp:183`
- **tooltip**: yes
- **notes**: The IsItemHovered() at line 183 runs AFTER ImGui::EndDisabled() at line 182, so "last item" is the seconds-preview TextDisabled, not the frames InputInt - hovering the input itself shows nothing.
- **tests**: none

### 29. Hover tooltip on "Open folder"

- **id**: `open-folder-tooltip`
- **input**: hover
- **path**: `Export/Open folder`
- **precondition**: Hovering the Open folder button (phase Done, path non-empty)
- **effect**: ImGui::SetTooltip stating it shows the exported file in Explorer.
- **source**: `src/gui/gui_export_panel.cpp:549`
- **tooltip**: yes
- **tests**: `export modal reveals the finished file`, `status strip Open folder reveals the finished export`, `status strip hides the reveal button unless an export finished`

### 30. Hover tooltip on the quality slider

- **id**: `quality-tooltip`
- **input**: hover
- **path**: `Export/##exp_q`
- **precondition**: Hovering ##exp_q
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining the 0-100 scale, default 60, and how it maps to libaom CRF / NVENC CQP / libvpx CRF per format.
- **source**: `src/gui/gui_export_panel.cpp:128`
- **tooltip**: yes
- **tests**: `export modal fps and quality reach the request`

### 31. Hover tooltip on the resolution preset combo

- **id**: `res-preset-tooltip`
- **input**: hover
- **path**: `Export/##exp_sz`
- **precondition**: Hovering ##exp_sz
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining that smaller output shrinks file size and browser decode cost, that "Native" follows the Setup render size, and that fixed presets pin the pixel dimensions.
- **source**: `src/gui/gui_export_panel.cpp:288`
- **tooltip**: yes
- **tests**: `export modal native resolution leaves the size unset`, `export modal resolution preset pins the output size`, `export modal scale button halves the current size`

### 32. Hover tooltip on a scale-multiplier button

- **id**: `scale-button-tooltip`
- **input**: hover
- **path**: `Export/x0.5 (%dx%d)##exp_scl_x0.5##exp_scl  (and the x0.25 / x0.1 variants)`
- **precondition**: Hovering one of the three scale buttons
- **disabled when**: busy
- **effect**: ImGui::SetTooltip with that button's per-entry .tip text explaining the resulting size class and that aspect ratio is preserved.
- **source**: `src/gui/gui_export_panel.cpp:356`
- **tooltip**: yes
- **notes**: ONE entry covering 3 distinct tooltip texts, one per loop iteration.
- **tests**: none

### 33. Hover tooltip on the filename stem field

- **id**: `stem-input-tooltip`
- **input**: hover
- **path**: `Export/##exp_stem`
- **precondition**: Hovering ##exp_stem and the field is not disabled
- **disabled when**: busy (disabled items do not report hover)
- **effect**: ImGui::SetTooltip explaining that the field is a stem only (extension chosen by format), that it auto-regenerates on IFS/animation switch, that relative paths resolve against the renderer working directory, and that for PNG sequence the stem becomes a folder containing frame_NNNNNN.png.
- **source**: `src/gui/gui_export_panel.cpp:66`
- **tooltip**: yes
- **tests**: `export modal PNG sequence writes a directory path`, `export modal filename and format drive the output path`, `export modal opens with a stem derived from the active IFS`

### 34. Hover tooltip on "Transparent bg"

- **id**: `transparent-bg-tooltip`
- **input**: hover
- **path**: `Export/Transparent bg`
- **precondition**: Hovering the Transparent bg checkbox
- **disabled when**: busy
- **effect**: ImGui::SetTooltip explaining that ON exports a real alpha channel and OFF composites a solid colour under the animation for viewers that handle animated AVIF transparency poorly.
- **source**: `src/gui/gui_export_panel.cpp:491`
- **tooltip**: yes
- **tests**: `export modal background toggle and colour reach the request`, `export modal disables the colour picker while the bg stays transparent`

### 35. Escape key while a widget in the modal is active

- **id**: `escape-clears-active-item` *(audit)*
- **input**: key
- **path**: `Export`
- **precondition**: Modal open AND g.ActiveId != 0 (a text field is being edited, a slider is being dragged, or a button is held)
- **effect**: NavUpdateCancelRequest takes the 'g.ActiveId != 0' branch and calls ClearActiveID(): an in-progress InputText edit is reverted to its pre-edit value, a slider drag is released, a held button is released. The popup is NOT closed and no export state changes.
- **source**: `src/gui/gui_export_panel.cpp:628`
- **notes**: This is the real Escape behaviour on this surface and replaces the inventory's modal-escape-close, which claims Escape dismisses the popup. Verified in vendored imgui.cpp NavUpdateCancelRequest (the ActiveId branch, and the popup-closing branch that explicitly excludes ImGuiWindowFlags_Modal).
- **tests**: none

### 36. Escape key while the Export modal has focus

- **id**: `modal-escape-close`
- **input**: key
- **path**: `Export`
- **precondition**: Modal open and no ImGui item is capturing text input
- **effect**: ImGui's built-in popup close-on-escape closes the "Export" popup; no command is posted and no export state changes. An in-flight export keeps running (phase stays Capturing/Encoding) because closing does not cancel.
- **source**: `src/gui/gui_export_panel.cpp:628`
- **notes**: Handled by ImGui core, not by code in this file; there is no explicit IsKeyPressed in gui_export_panel.cpp.
- **tests**: none
- **audit correction**: Hallucinated effect: Escape does NOT close this modal. ImGui's NavUpdateCancelRequest closes a popup only when the top popup is NOT ImGuiWindowFlags_Modal, and BeginPopupModal (line 628) forces that flag; p_open is nullptr so there is also no title-bar close button. Verified in vendor/vcpkg/buildtrees/imgui/src/v1.92.7-b588f89316.clean/imgui.cpp (NavUpdateCancelRequest: the popup branch is guarded by !(g.OpenPopupStack.back().Window->Flags & ImGuiWindowFlags_Modal)). The entry's precondition also wrongly implies Escape works whenever no text field is capturing input. -> Escape has NO closing effect on the Export modal. Its only effect is ClearActiveID(): it reverts an in-progress text edit or releases an active slider/button (see the new escape-clears-active-item record). The modal can be dismissed only by the Close button (line 570), the Start export button (line 562), or the Pick region path (line 663). Replace modal-escape-close with escape-clears-active-item.

### 37. Tab / Shift+Tab / arrow-key navigation between the modal's widgets

- **id**: `modal-keyboard-nav`
- **input**: key
- **path**: `Export`
- **precondition**: Modal open
- **disabled when**: widgets inside the BeginDisabled(busy) block (everything except Start/Cancel/Close) are skipped while an export is Capturing or Encoding
- **effect**: Moves ImGui keyboard focus between the widgets listed below; activating a focused widget with Space/Enter has the same effect as clicking it.
- **source**: `src/gui/gui_export_panel.cpp:628`
- **notes**: ImGui built-in nav; no custom key handling exists in this file.
- **tests**: none

### 38. Text-editing keymap inside every InputInt on this surface

- **id**: `numeric-input-text-keymap` *(audit)*
- **input**: key
- **path**: `Export/fps##exp_fps, Export/keyframe interval##exp_keyint, Export/frames##exp_maxf, Export/Continuous loop count##exp_loops, Export/Blend frames##exp_blendN, Export/##exp_w, Export/##exp_h, Export/x##crop_x, Export/y##crop_y, Export/w##crop_w, Export/h##crop_h`
- **precondition**: Modal open and one of the InputInt text boxes is active: fps##exp_fps, keyframe interval##exp_keyint, frames##exp_maxf, Continuous loop count##exp_loops, Blend frames##exp_blendN, ##exp_w, ##exp_h, x##crop_x, y##crop_y, w##crop_w, h##crop_h
- **disabled when**: busy; plus each field's own gate (frames##exp_maxf needs g_limit_frames, Blend frames##exp_blendN needs g_blend_loop, keyframe interval needs MediaSink::UsesKeyframeInterval, all Advanced fields need the header expanded)
- **effect**: Each InputInt is an InputText underneath, so it accepts the same editing keymap as the stem field (selection, clipboard, undo, word motions, double/triple-click select, drag-select). ImGui::InputScalar unconditionally ORs ImGuiInputTextFlags_AutoSelectAll into the flags, so clicking into ANY of these fields selects the whole value for overtyping - not just the four that pass the flag explicitly. None of them use EnterReturnsTrue, so the value is parsed and applied on every keystroke: an intermediate value is written and clamped mid-typing (typing 800 into ##exp_w momentarily sets g_out_w to 64 via the [64,8192] clamp at line 312 while the visible text still reads 8). Escape reverts the field's text and clears the active item without closing the modal.
- **source**: `src/gui/gui_export_panel.cpp:113`
- **notes**: ONE entry covering all 11 InputInt text boxes. AutoSelectAll-is-always-on verified at vendored imgui_widgets.cpp:3804; the inventory presents AutoSelectAll as a distinguishing property of ##exp_w/##exp_h/crop only.
- **tests**: `export modal crop inputs reach the request`

### 39. Filename stem field text-editing keymap and mouse selection

- **id**: `stem-input-text-keymap` *(audit)*
- **input**: key
- **path**: `Export/##exp_stem`
- **precondition**: Modal open and ##exp_stem is the active item (clicked into or Tab-focused)
- **disabled when**: busy (the whole block is inside BeginDisabled(busy) at line 640)
- **effect**: ImGui::InputText is submitted with no flags at line 64, so the full editing keymap is live on g_stem_buf: left/right/up/down, Home/End, Ctrl+left/right by word, shift+any of those to extend the selection, Ctrl+A select all, Ctrl+C/X/V clipboard (via the platform clipboard handler), Ctrl+Z / Ctrl+Y undo-redo, Delete/Backspace (Ctrl+Backspace deletes a word), plus double-click to select a word, triple-click to select the line, and click-drag to select a range. Enter or Tab deactivates and commits; Escape reverts the buffer to its pre-edit contents. Every keystroke writes g_stem_buf directly, so the value is committed continuously (no EnterReturnsTrue).
- **source**: `src/gui/gui_export_panel.cpp:64`
- **tooltip**: yes
- **notes**: The inventory's stem-input entry records the field but only as generic 'text-entry'; the audit checklist calls out that text fields accept the full editing keymap. Escape here is consumed by the active item and does NOT dismiss the modal (see the modal-escape-close correction).
- **tests**: `export modal PNG sequence writes a directory path`, `export modal filename and format drive the output path`, `export modal opens with a stem derived from the active IFS`

### 40. "Advanced" collapsing header

- **id**: `advanced-header`
- **input**: left-click
- **path**: `Export/Advanced`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Expands/collapses the Advanced section. While collapsed, DrawKeyframeIntervalControl, DrawFrameLimitControls, DrawLoopControls and DrawCrop are not drawn at all, so none of their widgets exist and close_for_pick stays false.
- **source**: `src/gui/gui_export_panel.cpp:648`
- **notes**: Gate for every Advanced-section entry below. Collapsing also shrinks the auto-resized popup.
- **tests**: `export modal controls all explain themselves`, `export modal stays inside a short window and keeps its footer`

### 41. Background colour swatch (ColorEdit3, NoInputs + NoLabel)

- **id**: `bg-color-swatch`
- **input**: left-click
- **path**: `Export/##exp_bg_color`
- **precondition**: Modal open AND g_bg_transparent is false
- **disabled when**: g_bg_transparent is true (ImGui::BeginDisabled at line 498) or busy
- **effect**: Clicking the swatch opens ImGui's colour picker popup bound to g_bg_rgb[3]; the values become r.bg_r/r.bg_g/r.bg_b in the StartExport request.
- **source**: `src/gui/gui_export_panel.cpp:500`
- **notes**: Item width 160. Default colour {0.13, 0.14, 0.17}.
- **tests**: none

### 42. Blend frames -/+ step buttons

- **id**: `blend-frames-step-buttons`
- **input**: left-click
- **path**: `Export/Blend frames##exp_blendN/-  and  Export/Blend frames##exp_blendN/+`
- **precondition**: Advanced expanded AND g_blend_loop is true (step=1, step_fast=5)
- **disabled when**: busy; hidden when g_blend_loop is false
- **effect**: Adjusts g_blend_frames by 1 per click, or 5 with Ctrl held, and clamps to [0,240].
- **source**: `src/gui/gui_export_panel.cpp:212`
- **tests**: none
- **audit correction**: has_tooltip is wrong (false): the IsItemHovered() at line 214 covers the entire InputScalar group for Blend frames##exp_blendN, including its -/+ buttons and label. -> has_tooltip: true - hovering the -/+ buttons shows the blend-frames tooltip from line 215. (By contrast max-frames-step-buttons is correctly false, because the IsItemHovered() at line 183 follows the seconds-preview TextDisabled, not the InputInt group.)

### 43. "Cancel" button

- **id**: `cancel-export-button`
- **input**: left-click
- **path**: `Export/Cancel`
- **precondition**: Export phase is Capturing or Encoding (busy)
- **disabled when**: not busy (the button is replaced by "Start export")
- **effect**: Posts App::Cmd::CancelExport{} via state.PostCommand; the modal stays open.
- **source**: `src/gui/gui_export_panel.cpp:565`
- **notes**: Size 120x0, occupies the same slot as Start export.
- **tests**: `export modal offers Cancel while a capture runs`

### 44. "Close" button

- **id**: `close-modal-button`
- **input**: left-click
- **path**: `Export/Close`
- **precondition**: Modal open (always available, including while busy - it is outside the BeginDisabled block)
- **effect**: ImGui::CloseCurrentPopup(). Does not cancel a running export and posts no command.
- **source**: `src/gui/gui_export_panel.cpp:570`
- **notes**: Size 90x0, SameLine after Start/Cancel. The status text or progress bar follows on the same line.
- **tests**: `ctrl plus E opens the export modal`, `export modal Close dismisses without posting a command`, `export modal Pick region arms crop mode, closes, then reopens`, `export modal controls all explain themselves`

### 45. "Clear" crop button

- **id**: `crop-clear-button`
- **input**: left-click
- **path**: `Export/Clear##crop_clear`
- **precondition**: Advanced expanded
- **disabled when**: busy
- **effect**: Calls state.SetCropRect({}) and state.SetCropPickMode(false), zeroes the local xywh array and sets changed=true (which immediately re-posts an all-zero rect via SetCropRect).
- **source**: `src/gui/gui_export_panel.cpp:399`
- **notes**: Size 70x0, drawn SameLine after the pick button. Has no tooltip of its own.
- **tests**: `export modal crop Clear resets the rect`

### 46. "Pick region" button

- **id**: `crop-pick-button`
- **input**: left-click
- **path**: `Export/Pick region##crop_pick`
- **precondition**: Advanced expanded AND state.GetCropPickMode() is false
- **disabled when**: busy
- **effect**: Calls state.SetCropPickMode(true) and returns close_for_pick=true, which sets g_reopen_after_pick=true and calls ImGui::CloseCurrentPopup() so the user can drag a rectangle on the render window; the modal reopens automatically once crop pick mode clears.
- **source**: `src/gui/gui_export_panel.cpp:386`
- **tooltip**: yes
- **notes**: Size 110x0. The actual click-and-drag rectangle selection on the render window is implemented outside this file.
- **tests**: `export modal Pick region arms crop mode, closes, then reopens`

### 47. "Picking..." button (armed state)

- **id**: `crop-picking-cancel-button`
- **input**: left-click
- **path**: `Export/Picking...##crop_pick`
- **precondition**: Advanced expanded AND state.GetCropPickMode() is true
- **disabled when**: busy
- **effect**: Calls state.SetCropPickMode(false), disarming crop-selection mode without closing the modal.
- **source**: `src/gui/gui_export_panel.cpp:381`
- **tooltip**: yes
- **notes**: Same 110x0 slot as "Pick region", drawn with pushed orange ImGuiCol_Button / ImGuiCol_ButtonHovered colours.
- **tests**: `export modal Picking button disarms crop mode`

### 48. Format list entry inside the open combo

- **id**: `format-combo-item`
- **input**: left-click
- **path**: `Export/##exp_fmt/<MediaSink::FormatLabel(f)>`
- **precondition**: ##exp_fmt combo is open
- **disabled when**: busy
- **effect**: ImGui::Selectable sets g_format_idx = i. That immediately changes current_format for the rest of the frame, which drives: the shown extension suffix, whether the keyframe-interval control exists (MediaSink::UsesKeyframeInterval), whether HW accel is applicable, which HW tooltip branch is used, the VideoEncoder::HardwareAvailable probe, and r.format in the posted App::Cmd::StartExport request.
- **source**: `src/gui/gui_export_panel.cpp:82`
- **notes**: ONE entry for a loop over i in [0, MediaSink::kFormatCount) = 7 rows: AVIF, WebM_VP9, WebM_AV1, WebP_Anim, PNG_Sequence, MP4_H264, MP4_HEVC_Alpha (media/media_format.h:8-17). The currently selected row also gets ImGui::SetItemDefaultFocus().
- **tests**: none

### 49. fps InputInt -/+ step buttons

- **id**: `fps-step-buttons`
- **input**: left-click
- **path**: `Export/fps##exp_fps/-  and  Export/fps##exp_fps/+`
- **precondition**: Modal open (step=1, step_fast=10 are non-zero so ImGui draws the buttons)
- **disabled when**: busy
- **effect**: Decrements/increments g_fps by 1 per click, or by 10 while Ctrl is held; result clamped to [1,240].
- **source**: `src/gui/gui_export_panel.cpp:113`
- **notes**: ImGui-generated buttons inside InputScalar; they also auto-repeat when held.
- **tests**: none
- **audit correction**: has_tooltip is wrong (false). ImGui::InputScalar with a non-zero step wraps the text box, the -/+ buttons and the visible label in BeginGroup()/EndGroup(); EndGroup re-emits the whole group as the last item (ItemAdd(group_bb, 0, ...)), so the IsItemHovered() at line 115 is true when the cursor is over the - or + button or the 'fps' label, not just the text box. Verified at vendored imgui_widgets.cpp:3812 and imgui.cpp EndGroup (ItemSize/ItemAdd on group_bb). -> has_tooltip: true - hovering the -/+ step buttons (or the 'fps' label) shows the same fps tooltip from line 116, because IsItemHovered() after InputInt refers to the whole InputScalar group.

### 50. keyframe interval -/+ step buttons

- **id**: `keyframe-step-buttons`
- **input**: left-click
- **path**: `Export/keyframe interval##exp_keyint/-  and  Export/keyframe interval##exp_keyint/+`
- **precondition**: Same as keyframe-input (step=1, step_fast=30)
- **disabled when**: busy or the control is hidden for this format
- **effect**: Adjusts g_keyframe_interval by 1 per click, or 30 with Ctrl held; clamped to [0,100000].
- **source**: `src/gui/gui_export_panel.cpp:139`
- **tests**: none
- **audit correction**: has_tooltip is wrong (false) for the same reason as fps-step-buttons: the IsItemHovered() at line 141 tests the whole InputScalar group emitted by EndGroup, which includes the -/+ buttons and the 'keyframe interval' label. -> has_tooltip: true - hovering the -/+ buttons shows the keyframe tooltip from line 142.

### 51. Continuous loop count -/+ step buttons

- **id**: `loop-count-step-buttons`
- **input**: left-click
- **path**: `Export/Continuous loop count##exp_loops/-  and  Export/Continuous loop count##exp_loops/+`
- **precondition**: Advanced expanded (step=1, step_fast=5)
- **disabled when**: busy
- **effect**: Adjusts g_loop_count by 1 per click, or 5 with Ctrl held, and clamps to [1,1000].
- **source**: `src/gui/gui_export_panel.cpp:192`
- **tests**: none
- **audit correction**: has_tooltip is wrong (false): the IsItemHovered() at line 194 covers the entire InputScalar group for Continuous loop count##exp_loops, including its -/+ buttons and label. -> has_tooltip: true - hovering the -/+ buttons shows the loop-count tooltip from line 195.

### 52. frames InputInt -/+ step buttons

- **id**: `max-frames-step-buttons`
- **input**: left-click
- **path**: `Export/frames##exp_maxf/-  and  Export/frames##exp_maxf/+`
- **precondition**: Advanced expanded AND g_limit_frames is true (step=1, step_fast=10)
- **disabled when**: !g_limit_frames or busy
- **effect**: Adjusts g_max_frames by 1 per click, or 10 with Ctrl held; clamped to [1,100000].
- **source**: `src/gui/gui_export_panel.cpp:175`
- **tests**: none

### 53. Click anywhere outside the Export modal (dimmed background, the main render window, any other panel)

- **id**: `modal-click-outside` *(audit)*
- **input**: left-click
- **path**: `Export`
- **precondition**: Modal open
- **effect**: Nothing happens. BeginPopupModal forces ImGuiWindowFlags_Modal, and ImGui blocks hovering of every window behind the modal, so the click never reaches the underlying panel; ClosePopupsOverWindow keeps modal popups open (ClosePopupsExceptModals stops at the first modal). Unlike a normal popup, this modal cannot be dismissed by clicking away - only Close, Start export, or the Pick region path close it.
- **source**: `src/gui/gui_export_panel.cpp:628`
- **notes**: A gesture every user attempts, and one of only three ways the modal could plausibly be dismissed - the inventory covers title-bar drag and (incorrectly) Escape but never click-outside. Verified in vendored imgui.cpp: the modal blocking comment 'Modal windows prevents mouse from hovering behind them' and the ClosePopupsOverWindow / ClosePopupsExceptModals modal test.
- **tests**: none

### 54. "Open folder" button

- **id**: `open-folder-button`
- **input**: left-click
- **path**: `Export/Open folder`
- **precondition**: Export phase is Done AND ex.output_path is non-empty
- **disabled when**: never (the button simply is not drawn unless the precondition holds)
- **effect**: Calls NativeDialog::RevealInFileManager(ex.output_path) to show the exported file in Explorer.
- **source**: `src/gui/gui_export_panel.cpp:547`
- **tooltip**: yes
- **notes**: Drawn under the green "done - <path>" TextWrapped.
- **tests**: `export modal reveals the finished file`, `status strip Open folder reveals the finished export`, `status strip hides the reveal button unless an export finished`

### 55. "Custom" entry in the resolution combo

- **id**: `res-preset-custom`
- **input**: left-click
- **path**: `Export/##exp_sz/Custom`
- **precondition**: ##exp_sz combo open
- **disabled when**: busy
- **effect**: Selects the custom index; if g_out_w or g_out_h is still <= 0 it seeds them with the current render size (rw, rh) so the width/height inputs below become editable with sane values.
- **source**: `src/gui/gui_export_panel.cpp:277`
- **notes**: Handled by the i == custom_idx branch of ApplyPresetSelection (lines 239-243).
- **tests**: none

### 56. Fixed resolution entry in the resolution combo

- **id**: `res-preset-fixed`
- **input**: left-click
- **path**: `Export/##exp_sz/<preset label>`
- **precondition**: ##exp_sz combo open
- **disabled when**: busy
- **effect**: Sets g_out_w/g_out_h to the preset's literal pixel dimensions, pinning the export size regardless of the render size.
- **source**: `src/gui/gui_export_panel.cpp:277`
- **notes**: ONE entry for a loop over 5 fixed rows: 1920x1080, 1280x720, 640x480, 1080x1920, 720x1280 (declared at lines 253-258).
- **tests**: none

### 57. "Native (WxH)" entry in the resolution combo

- **id**: `res-preset-native`
- **input**: left-click
- **path**: `Export/##exp_sz/Native (%dx%d)`
- **precondition**: ##exp_sz combo open
- **disabled when**: busy
- **effect**: ApplyPresetSelection with i==0 sets g_out_w = 0 and g_out_h = 0, meaning the export tracks state.GetRenderSize() at capture time (r.width/r.height posted as 0).
- **source**: `src/gui/gui_export_panel.cpp:277`
- **notes**: Label is built with snprintf from the live render size (rw, rh), defaulting to 1920x1080 when GetRenderSize returns <= 0.
- **tests**: none

### 58. Scale-current multiplier button ("x0.5 (WxH)" / "x0.25 (WxH)" / "x0.1 (WxH)")

- **id**: `scale-button`
- **input**: left-click
- **path**: `Export/x0.5 (%dx%d)##exp_scl_x0.5##exp_scl  (and the x0.25 / x0.1 variants)`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Sets g_out_w/g_out_h to lroundf(current displayed size * mul), clamped to [64,8192], pinning an explicit custom output size.
- **source**: `src/gui/gui_export_panel.cpp:352`
- **tooltip**: yes
- **notes**: ONE entry for a loop over the 3-element kScales array (lines 325-338). The visible label is rebuilt each frame with the resulting pixel size baked in; the ID suffix is "##exp_scl_" + the original label including its own "##exp_scl". Preceded by TextDisabled("Scale current x:").
- **tests**: none

### 59. "Start export" button

- **id**: `start-export-button`
- **input**: left-click
- **path**: `Export/Start export`
- **precondition**: Export phase is Idle, Done or Failed (i.e. NOT busy)
- **disabled when**: busy (replaced by the Cancel button)
- **effect**: Builds an App::ExportRequest from every control on this surface - output_path = MediaSink::MakeOutputPath(g_stem_buf, current_format), fps, quality, keyframe_interval, max_frames (0 unless Limit frames), loop_count, blend_loop, blend_frames, bg_transparent, bg_r/g/b, width/height, crop_x/y/w/h from state.GetCropRect(), format index, prefer_hardware = g_prefer_hw && hw_applies - then posts App::Cmd::StartExport{req} via state.PostCommand and calls ImGui::CloseCurrentPopup().
- **source**: `src/gui/gui_export_panel.cpp:556`
- **notes**: Size 120x0. hw_applies is recomputed here independently of DrawBackgroundAndHw (lines 557-560).
- **tests**: `export modal PNG sequence writes a directory path`, `export modal background toggle and colour reach the request`, `export modal crop inputs reach the request`, `export modal custom resolution inputs clamp` (+12 more)

### 60. Background colour swatch right-click options popup

- **id**: `bg-color-swatch-context-menu` *(audit)*
- **input**: right-click
- **path**: `Export/##exp_bg_color/context`
- **precondition**: Modal open, g_bg_transparent is false, right-clicking ##exp_bg_color
- **disabled when**: g_bg_transparent is true or busy (disabled items do not accept clicks)
- **effect**: ColorEdit4 calls OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight) on the colour button because ImGuiColorEditFlags_NoOptions is not passed. The popup lets the user switch the colour-edit display mode (RGB / HSV / Hex) and the picker type (hue bar + SV rect vs hue wheel + SV triangle). It writes only ImGui's global g.ColorEditOptions - g_bg_rgb and the export request are untouched.
- **source**: `src/gui/gui_export_panel.cpp:500`
- **notes**: Verified in vendored imgui_widgets.cpp: the OpenPopupOnItemClick("context", ...) immediately after the ColorButton("##ColorButton", ...) call inside ColorEdit4. The inventory only mentions right-click *inside* the picker popup (bg-color-picker notes), not right-click on the swatch itself, which is a separate popup.
- **tests**: none

### 61. Export modal body (scrollable popup content)

- **id**: `modal-scroll`
- **input**: scroll
- **path**: `Export`
- **precondition**: Modal open AND content height exceeds vp->WorkSize.y - 48 (e.g. the "Advanced" header is expanded on a short screen)
- **effect**: Scrolls the popup window's content region; no application state changes.
- **source**: `src/gui/gui_export_panel.cpp:627`
- **notes**: SetNextWindowSizeConstraints caps the height, so the auto-resized popup becomes scrollable once content overflows.
- **tests**: none

### 62. quality SliderInt

- **id**: `quality-slider-drag`
- **input**: slider
- **path**: `Export/##exp_q`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Drags g_quality over [0,100], displayed as "quality %d". Feeds r.quality in the StartExport request.
- **source**: `src/gui/gui_export_panel.cpp:126`
- **tooltip**: yes
- **notes**: Full remaining width; drawn SameLine after the fps input.
- **tests**: `export modal fps and quality reach the request`

### 63. "Blend frames" InputInt (text portion)

- **id**: `blend-frames-input`
- **input**: text-entry
- **path**: `Export/Blend frames##exp_blendN`
- **precondition**: Advanced expanded AND g_blend_loop is true
- **disabled when**: busy; hidden entirely when g_blend_loop is false
- **effect**: On change, clamps g_blend_frames to [0,240]; posted as r.blend_frames.
- **source**: `src/gui/gui_export_panel.cpp:212`
- **tooltip**: yes
- **notes**: Item width 100, drawn SameLine after the checkbox.
- **tests**: `export modal blend frame count hides while the seam is off`, `export modal loop count and blend seam reach the request`

### 64. Crop x / y / w / h InputInt

- **id**: `crop-xywh-input`
- **input**: text-entry
- **path**: `Export/x##crop_x, Export/y##crop_y, Export/w##crop_w, Export/h##crop_h`
- **precondition**: Advanced expanded
- **disabled when**: busy
- **effect**: Editing any of the four sets changed=true; at the end of DrawCrop the four values are floored at 0 and written via state.SetCropRect(App::CropRect{x,y,w,h}). The rect is later read back with state.GetCropRect() into r.crop_x/y/w/h for the StartExport request.
- **source**: `src/gui/gui_export_panel.cpp:425`
- **notes**: ONE entry for a loop over the 4-element labels array (line 421). step=0 so no -/+ buttons; ImGuiInputTextFlags_AutoSelectAll. Field width is computed from available width / 4. Preceded by a non-interactive TextUnformatted("crop").
- **tests**: `export modal crop inputs reach the request`

### 65. fps InputInt (text portion)

- **id**: `fps-input`
- **input**: text-entry
- **path**: `Export/fps##exp_fps`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Writes g_fps, then clamped to [1,240] every frame. Feeds r.fps in the StartExport request and the "(~X.XXs at N fps)" seconds estimate next to the frame limit.
- **source**: `src/gui/gui_export_panel.cpp:113`
- **tooltip**: yes
- **notes**: Item width 120.
- **tests**: `export modal fps and quality reach the request`

### 66. "keyframe interval" InputInt (text portion)

- **id**: `keyframe-input`
- **input**: text-entry
- **path**: `Export/keyframe interval##exp_keyint`
- **precondition**: Advanced expanded AND MediaSink::UsesKeyframeInterval(current_format) is true
- **disabled when**: busy; the whole control is hidden (not merely disabled) for formats where UsesKeyframeInterval is false
- **effect**: Writes g_keyframe_interval, clamped to [0,100000]; posted as r.keyframe_interval. A SameLine TextDisabled shows "(auto: 1/sec)" when 0 or "(every N frames)" otherwise.
- **source**: `src/gui/gui_export_panel.cpp:139`
- **tooltip**: yes
- **notes**: Item width 120.
- **tests**: `export modal keyframe interval is offered only for video formats`

### 67. "Continuous loop count" InputInt (text portion)

- **id**: `loop-count-input`
- **input**: text-entry
- **path**: `Export/Continuous loop count##exp_loops`
- **precondition**: Advanced expanded
- **disabled when**: busy
- **effect**: On change, clamps g_loop_count to [1,1000]; posted as r.loop_count in the StartExport request.
- **source**: `src/gui/gui_export_panel.cpp:192`
- **tooltip**: yes
- **notes**: Item width 120. Note the clamp only runs when the InputInt returns true (inside the if), unlike fps/keyframe which clamp unconditionally.
- **tests**: `export modal loop count and blend seam reach the request`

### 68. "frames" InputInt (max frame count, text portion)

- **id**: `max-frames-input`
- **input**: text-entry
- **path**: `Export/frames##exp_maxf`
- **precondition**: Advanced expanded AND g_limit_frames is true
- **disabled when**: !g_limit_frames (ImGui::BeginDisabled at line 172) or busy
- **effect**: Writes g_max_frames, clamped to [1,100000]; posted as r.max_frames when Limit frames is on. A SameLine TextDisabled shows the derived duration "(~%.2fs at %d fps)".
- **source**: `src/gui/gui_export_panel.cpp:175`
- **notes**: Item width 120.
- **tests**: `export modal frame limit is applied only while enabled`

### 69. quality SliderInt ctrl+click / double-click to type a value

- **id**: `quality-slider-ctrl-click`
- **input**: text-entry
- **path**: `Export/##exp_q`
- **precondition**: Ctrl+click (or double-click) on ##exp_q
- **disabled when**: busy
- **effect**: ImGui converts the slider into an inline text input; typing a number sets g_quality, clamped to [0,100] by the slider.
- **source**: `src/gui/gui_export_panel.cpp:126`
- **notes**: ImGui built-in slider behaviour (no ImGuiSliderFlags_NoInput passed).
- **tests**: `export modal fps and quality reach the request`
- **audit correction**: Wrong effect/precondition: double-click does not open the slider's inline text input. SliderScalar enters temp-input only on Ctrl+click or on keyboard activation with ImGuiActivateFlags_PreferInput; double-click-to-input is DragScalar behaviour, not SliderScalar. Verified at vendored imgui_widgets.cpp:3330-3337 ('Tabbing or Ctrl+Click on Slider turns it into an input box'; the condition is (clicked && g.IO.KeyCtrl) \|\| (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))). -> Precondition: Ctrl+click on ##exp_q, or Tab/arrow-focus the slider and press Enter (keyboard activate prefers input). Plain double-click does nothing beyond a normal click-drag. Effect otherwise unchanged: an inline text field writes g_quality, clamped to [0,100].

### 70. Output height InputInt

- **id**: `res-height-input`
- **input**: text-entry
- **path**: `Export/##exp_h`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: Same as the width field: clamps both to [64,8192] and writes g_out_w/g_out_h. Between the two fields is a non-interactive "x" and a trailing TextDisabled("(width x height, pixels)").
- **source**: `src/gui/gui_export_panel.cpp:308`
- **notes**: No stepper buttons; ImGuiInputTextFlags_AutoSelectAll.
- **tests**: `export modal custom resolution inputs clamp`

### 71. Output width InputInt

- **id**: `res-width-input`
- **input**: text-entry
- **path**: `Export/##exp_w`
- **precondition**: Modal open
- **disabled when**: busy
- **effect**: On change, both width and height are clamped to [64,8192] and written to g_out_w/g_out_h, which also re-derives the preset combo selection (MatchPresetIdx) on the next frame. Displays the effective width (g_out_w if > 0, else the render width).
- **source**: `src/gui/gui_export_panel.cpp:302`
- **notes**: step=0 and step_fast=0, so there are no -/+ buttons. ImGuiInputTextFlags_AutoSelectAll means a single click selects the whole value for overtyping. Typing here also commits the height value (both are written together at lines 314-315).
- **tests**: `export modal custom resolution inputs clamp`

### 72. Output filename stem text field

- **id**: `stem-input`
- **input**: text-entry
- **path**: `Export/##exp_stem`
- **precondition**: Modal open
- **disabled when**: busy (export phase is Capturing or Encoding) - wrapped in ImGui::BeginDisabled(busy)
- **effect**: ImGui::InputText writes directly into the file-scope buffer g_stem_buf[512]. Consumed at export start by MediaSink::MakeOutputPath(g_stem_buf, current_format). The buffer is auto-regenerated by MaybeRegenerateStem() from MediaSink::DeriveExportStem(state.ActiveIfs(), state.GetStatus().playing_animation) whenever the active IFS or playing animation changes, which overwrites whatever the user typed.
- **source**: `src/gui/gui_export_panel.cpp:64`
- **tooltip**: yes
- **notes**: Width is content-avail minus the extension label width. The trailing extension text (or "/" for PNG sequence) at line 75 is a non-interactive TextDisabled.
- **tests**: `export modal PNG sequence writes a directory path`, `export modal filename and format drive the output path`, `export modal opens with a stem derived from the active IFS`

### 73. Export modal window ("Export" popup)

- **id**: `modal-open`
- **input**: window-message
- **path**: `Export`
- **precondition**: Panels::Export::RequestOpen() was called by another panel this frame (sets g_open_requested), or g_reopen_after_pick was armed by "Pick region" and state.GetCropPickMode() has returned to false
- **effect**: ImGui::OpenPopup("Export") then BeginPopupModal("Export", nullptr, ImGuiWindowFlags_AlwaysAutoResize); window is force-recentred on the main viewport every frame (SetNextWindowPos ImGuiCond_Always) and constrained to width exactly 620 and height <= vp->WorkSize.y - 48. When the popup is not open RenderModal() returns immediately and nothing below is drawn.
- **source**: `src/gui/gui_export_panel.cpp:628`
- **notes**: Gate for every other entry in this surface: all of them require this modal to be open. The reopen-after-crop-pick path is at lines 616-619.

