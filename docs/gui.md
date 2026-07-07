# GUI architecture and debug-viewer parity

Knowledge captured from `src/gui/*`. The GUI mirrors
KONAMI's own in-game AFP debug viewer (`CAfpViewerScene` in bm2dx) wherever a control maps to
an engine call - those mappings are engine facts and are listed in section 4.

## 1. Threading model

### 1.1 Dedicated GUI thread (gui_thread)

The GUI runs on its OWN thread, separate from the render thread. Reason: dragging a Win32
window by its title bar (or resizing via an edge) enters a "modal move loop" inside
DefWindowProc that only returns on mouse-up - anything on that thread stalls for the duration,
including the render loop and AFP's per-frame tick. A separate GUI thread isolates that modal
loop: the render thread keeps running at its cadence while the GUI thread is stuck in the drag.

- The two threads communicate EXCLUSIVELY through `App::State` (mutex-guarded). Each thread
  owns its own window, D3D9 device and ImGui context - zero cross-thread ImGui/D3D9 sharing.
- The GUI thread exits when (a) the user closes the GUI window - the renderer then continues
  HEADLESSLY (the GUI is a control panel, not the app) - or (b) `App::Global().ShouldExit()`
  flips (render window closed); main.cpp joins the GUI thread before final cleanup.
- The panel loop is rate-limited to ~60 Hz via Sleep(16) - it does not need the renderer's
  120 Hz and burning a core on an idle ImGui loop is wasteful.
- `GuiThread::Start` is idempotent (atomic exchange); `Stop` flips the local running flag and
  joins, letting the GUI stop selectively without tearing the whole app down.

### 1.2 Request/Status pattern

GUI widgets never call the engine. They post `App::Request` objects (seek_frame, set_paused,
switch_animation, load_new_ifs, start_export, start_qpro_extract, force_replay,
toggle_companion, goto_label, set_game_dir, ...) that the render thread consumes; the render
thread publishes `App::Status` / LiveState / ExportState / LoadProgress snapshots that the GUI
polls once per frame. Background workers (arc/customize extractors, qpro scan) publish into
their own mutex-guarded Status structs polled the same way.

### 1.3 gui_window internals (Win32 + DX9 lost-device dance)

- The modal border-drag loop runs inside DispatchMessage and blocks PumpAndRender until
  mouse-up, so live resize must be serviced from INSIDE the message handler: WM_SIZE queues a
  backbuffer resize AND renders one frame immediately (`RenderFrameLocked`, re-entrancy
  guarded) so the panes track the cursor during the drag. WM_PAINT renders a real frame +
  ValidateRect; WM_ERASEBKGND returns 1 (skip GDI erase, avoids resize flicker).
  SIZE_MINIMIZED reports a 0x0 client that must NOT be reset to.
- `ResetDevice` re-reads the client rect into the present params EVERY time, so a device-lost
  that happens AFTER a resize restores at the current size instead of snapping back to the
  creation size. ImGui's font atlas + buffers live in D3DPOOL_DEFAULT, so
  `ImGui_ImplDX9_InvalidateDeviceObjects` before Reset and `CreateDeviceObjects` after
  (standard DX9 lost-device dance).
- WM_GETMINMAXINFO clamps the min window size to the layout constants (fires during
  CreateWindow while the module global is still null - fine, the constants are static).
- Fonts: ImGui's ProggyClean is ASCII-only; the GUI loads Segoe UI 16px, falling back to
  Consolas 15px, from `%WINDIR%\Fonts` (GetWindowsDirectory, not hardcoded), with glyph ranges
  0x0020-0x00FF, 0x2010-0x2027, 0x2030-0x205E to cover dashes/quotes/ellipsis. Fonts MUST be
  added BEFORE ImGui_ImplDX9_Init (the backend uploads the atlas on init; later fonts render
  blank). The glyph-range array must be static (ImGui keeps the pointer).
- io.IniFilename = nullptr (window layout not persisted). The GUI HWND is exposed via
  `Gui::GetHwnd()` for native dialog parenting.

## 2. Layout

- Layout constants live in one place (`gui_layout_constants.h`) so the two consumers cannot
  drift: WM_GETMINMAXINFO (min client size) and the renderer-tab pane layout (per-pane
  minimums + splitter width). Pane minimums: left 240 (IFS tree), centre 320 (info + export),
  right 300 (layers/variants stack); defaults 320 / 380; splitter 6 px; min client =
  sum + 2 splitters + 80 margin, min height 600.
- Renderer tab = three panes (IFS tree | IFS info + export | layers/variants/labels/
  sub-layers/overrides) separated by two draggable `Gui::VSplitter`s. Side-pane widths are
  session-only (deliberately not persisted); the CENTRE pane always absorbs the remainder.
  When the window shrinks below fit, the centre steals width from the right pane first, then
  the left (side panels are tools, centre is content).
- `VSplitter` is built on `ImGui::InvisibleButton` only (hit-tested, layout-advancing,
  IsItemActive/Hovered - no imgui_internal). A drag moves pixels symmetrically between the two
  neighbours, committing only if BOTH stay >= their minimum, so the row stays exactly tiled.
- Views: BootState WaitingForDir/Booting/Failed -> Setup view; Ready -> tabbed main view
  (Renderer, qpro). The loading overlay renders on top of EITHER view from the same
  LoadProgress API.

### 2.1 IFS picker specifics

- The IFS tree is rebuilt from the entry list EVERY frame (linear in entries x depth, fine for
  ~6000 files). ImGui ID GOTCHA: pushing the entry POINTER as the row ID broke clicking -
  the rebuild gives a different pointer each frame, so press (frame N) and release (frame N+1)
  had different IDs and ImGui never registered the click ("nothing happens"). Fix: PushID on
  the full path STRING (stable content, unique) - directory TreeNodes already used the segment
  string, which is why they worked.
- Auto-expand is keyed off the WHOLE tree size (<= 20 files), not per-subtree: a tiny install
  (GITADORA's single deeply-nested data/afp_result_10_c.ifs) auto-opens, while a real
  SDVX/IIDX install stays collapsed so small subfolders don't pop open when a scan finishes
  (the old per-subtree <= 20 rule expanded every one of them).
- Active-row highlight compares the FULL absolute path (Status::current_ifs_path), not the
  basename - installs have same-named IFSes across subdirs.
- The .ifs directory scan runs on a background thread (boot flips to Ready before it
  finishes); the pane shows the scan's live status line instead of "none found" while running.
- File leaves post `load_new_ifs` requests carrying `from_arc` for DDR .arc-wrapped IFSes.

### 2.2 Variant editor semantics (afp engine facts)

- "(default)" cannot un-override a clip: `afp_play_work_load_bitmap` (afp-core)
  looks its name argument up in the bitmap dictionary and only REBINDS -
  every call tears down the prior fragment and builds a new one keyed on the given name; there
  is no "restore authored" path. So "(default)" drops the override locally and posts
  `force_replay` so the master stream's timeline re-authors the clip's original bitmap (other
  latched slots re-apply next frame, so only that slot visually reverts).
- Slots are probed from afplist/texturelist plus common names; unresolved user-added slots are
  probed by the render thread next frame.

### 2.3 Layers panel

- The layer list = afplist.xml <afp> entries; KONAMI's own AFP debug viewer labels this exact
  list "[LAYERNUM]" and instantiates the pick via CreateAfpLayer - hence the "Layers" name.
  Clicking the already-playing row is a valid replay (the dispatcher routes same-name picks
  through ForceReplay).
- DDR backgrounds have no afplist (single root clip), so this panel early-outs in DDR mode;
  the seek controls were therefore relocated OUT of this panel into the tab body, gated on a
  loaded stream id instead (Status.stream_id != the 0xFFFFFFFC sentinel; for DDR the non-
  sentinel value is DdrAfp::LayerId()).
- "Loop root" combo encodes a game mechanism: the real game's generic decision for a
  scene-BG root that reaches its end is HOLD (mount once, afp's own tick reaches its natural
  terminal branch while nested children free-run - the game default; fixes the select_bg
  o_kazari5 ornament snapping back every 140-frame root cycle) vs FORCE loop (re-drive the
  root via ForceReplay + the continuous-loop flag sequence - needed for one-shot masters like
  bg_common). The renderer mounts via afplist and cannot read the dispatcher's per-BG choice
  at runtime, so this is an explicit user control defaulting to the game default (Hold),
  persisted in settings.ini.
- "Master scale": SDVX I-IV-era 720x1280 IFSes (select_bg_booth, select_bg_ii, _4bg, ...)
  need x1.5 to fill 1080x1920 - the real game does this per-BG via the BG entry's payload+28
  float; the GUI exposes it as a slider with a 1.5x preset since the preview has no BG-id
  notion.

## 3. Loading overlay and progress rules

- Full-screen dimmed overlay with `ImGuiWindowFlags_NoInputs` - hit-testing suppressed, so
  drawing it AFTER the regular view is enough to make everything non-interactive (no per-
  widget disabling).
- Determinate bar when a fraction is known (e.g. textures_expected pre-counted from
  texturelist.xml); otherwise an indeterminate marquee PLUS a changing detail line (current
  item or climbing count) so it never looks frozen. Edge-triggered logging records overlay
  ON/OFF transitions in renderer.log without per-frame spam.
- The same two-pass pattern is used by the arc/customize extractor panels: pass 1 (scan,
  total unknown) shows an animated bar + "Scanning... N found" + the current path; pass 2
  shows done/total. This implements the project-wide progress rule (see CLAUDE.md).

## 4. Debug-viewer parity controls (engine-call mappings)

The live controls mirror KONAMI's CAfpViewerScene key bindings. Each GUI control drives the
exact engine call the scene's key does:

| Control (scene key) | Engine call |
| --- | --- |
| [TIME] seek slider + step buttons (LEFT/RIGHT; SHIFT = 100-step) | absolute-frame seek via afp_mc_control 0xF08 (deep_goto_play) on the master mc; seeking force-pauses, exactly like the scene |
| Paused checkbox (RETURN+SHIFT) | stream playback speed 0 / 1 via afp_stream_set_speed, re-applied each frame by the render thread |
| Background cycle (F4) | preview RT clear color: default transparent -> grey -> black -> red -> green -> blue -> wrap (kBgPresets); live preview only, export has its own bg |
| Filter toggle (F7) | afp-core set-filter ord 0x032 on the active stream, filter id 0x80000000 OR enable - the same call the scene's CLayer slot-32 wrapper makes |
| Show MC names (F3 DISP MC) | afp_mc_enumerate_children ord 0x079 on the master; name-type (F6): 0 = draw each name at its clip position over the preview, 1 = fixed column list |
| [TIME] readout | the BOUNDED mc playhead (work+0x76 via afp_mc_set 0x1010), shown in every mode exactly like the IIDX debug viewer |
| [LOOPS] readout | OUR detected backward-wrap tally since the clip/label last (re)started - NOT afp's work+0x104; the debug scene shows no loop_count |
| [SIZE] readout | afp_get_layer_info word[8]/word[9] |
| [LBL NO]/[LBL NM] | current label name + index n/N from the label list |
| raw cur/total readout | free-running stream counter afp_get_layer_info w[13]/w[12] - climbs past total under the continuous-loop dance (diagnostic) |
| [FILE INFO] block | afp-utils package metadata; version triplets unpack as major=(v>>16)&0xFFFF, minor=(v>>8)&0xFF, patch=v&0xFF (matches the scene's printf). Locale is intentionally omitted - it is the real game's runtime region via a bm2dx CRT call, not derivable from afp/avs |

Other live-control semantics:

- Continuous loop mode input: -1 = explicit OFF (clear the CLayer flag; master reverts to
  gotoAndStop saturation), 0 = leave engine default, 1 = explicit ON (apply the BG-dispatcher
  flag sequence - master keeps advancing past total_length while sub-clips evolve naturally;
  what BG 20 needs). Re-applied on the NEXT stream switch.
- Trim frames: >0 restarts the master via ForceReplay at rendered frame N since the last
  switch (watch an exact-length loop at full framerate).
- The whole seek block is disabled while an export is capturing: a seek would corrupt the
  export's loop-wrap counter and a pause would freeze the capture (the render thread also
  hard-ignores such requests during capture; greying just makes it visible).
- Step buttons WRAP around mc_total (matching afp's own wrap-to-0), while the slider clamps.
- DDR gating: DDR's afp 2.13.7 exports none of the modern-only knobs (continuous-loop flag
  dance, trim ForceReplay, afp_set_filter, bulk child-enumerate), so those widgets hide in
  DDR mode; Background cycle, Reset and the live readout stay (self-gated on have_* flags).
- Sub-layers panel: per-sub-clip visibility checkboxes over the active layer's named child
  tree. Expansion is LAZY - opening a node writes its path into a State expand-set the render
  thread reads to enumerate children on demand (dynamic timer subtrees only walked if opened).
  ImGui IDs use the SIBLING INDEX, not the path: afp clips can have same-named siblings
  (several `txt_usr`) whose paths collide, and ImGui asserts on duplicate row IDs; the path
  stays the afp toggle key, so same-named siblings toggle as a group - matching the game
  (SetClipVisible walks the dir-6 same-name chain). Absent override = authored visible.
  DDR-gated (no bulk child-enumerate in 2.13.7).

## 5. Export panel notes (encoder facts)

- Form state is file-scope statics so fps/quality/format choices survive tab switches and IFS
  reloads.
- Format facts baked into the tooltips: AVIF = AV1 dual-stream with an auxiliary alpha plane
  (plays in img tags with transparency; color stream NVENC on RTX 40+, alpha stream stays
  software - tiny bitstream, NVENC has no alpha path); WebM VP9 = yuva420p single stream
  (video-tag transparency; software only - NO NVIDIA desktop GPU ships a VP9 encoder); WebM
  AV1 = NVENC-capable AND video-tag smooth but OPAQUE only; WebP anim = alpha + smooth img-tag
  playback everywhere, ~2-3x larger; PNG sequence = lossless frame_NNNNNN.png folder via WIC.
  video-tag playback beats animated img for long clips because Blink clamps animated-image
  frame durations to 10 ms.
- Keyframe interval (`g_keyframe_interval`): shown only when
  `MediaSink::UsesKeyframeInterval(current_format)` (all codec formats, not PNG/WebP). 0 =
  auto (one keyframe/sec); a larger value or one >= the frame count forces a single keyframe
  for a much smaller file on static/scrolling scenes. The inline hint shows "(auto: 1/sec)" or
  "(every N frames)". Feeds `Request::export_keyframe_interval` -> encoder `gop_size`. See
  docs/media_formats.md "Keyframe interval".
- H.264 export REQUIRES h264_nvenc on this ffmpeg build (no libx264/openh264 compiled in).
  av1_nvenc needs Ada (RTX 40) or newer; h264_nvenc is on most NVENC GPUs. HW probe is
  format-specific.
- "Blend loop seam" is the ONLY sanctioned pixel-compare feature: a user-opted-in synthesized
  crossfade for backgrounds with no clean authored loop (e.g. select_bg_iv's one-shot _4bg).
  The real game does NOT do this.
- The whole form is disabled during capture so a mid-capture click cannot mutate values
  already snapshotted into the export session; the crop rect is snapshotted at start for the
  same reason. Crop has two entry paths (numeric inputs + an armed pick-drag on the render
  window mirrored back into App::CropRect); numeric edits write state only on change to avoid
  racing the drag handler.
- Filename stem auto-regenerates on IFS/animation change only (not per frame).
- Output resolution (0,0) = "native at submit time"; a two-static-state machine
  (shown_idx + last dims) keeps a user "Custom" pick from being auto-reverted by the
  auto-match next frame. Same pattern in Setup's render-resolution widget.

## 6. Setup view

- Game profile combo: Konami ships per-game builds of afp-core.dll sharing the export-name
  scheme but DIVERGING in ordinal-to-function mapping (e.g. IIDX 33 vs SDVX 7); the profile
  selects ordinals + boot-call gating. Auto-detect matches known substrings in the game dir
  path; the Auto label previews what it would pick.
- Render fps: controls the live loop tick rate AND per-tick dt (dt = 1/fps) so animation speed
  is unchanged at any rate. DDR content is authored at 60; IIDX/SDVX previews historically ran
  at 120.
- Render resolution presets encode native sizes: 3840x2160 (GITADORA DELTA, 4K@60),
  1920x1080, 1280x720 (IIDX 17), 1080x1920 / 720x1280 (SDVX portrait), 520x704 (IIDX qpro
  avatar canvas), 1024x768 (jubeat saucer - the reason hardcoded 1080p broke: AFP's
  GetScreenSize callback must match the offscreen RT shape or layout coords land wrong).
  Values clamp to 64..8192 (floor protects D3D9, ceiling stops accidental giant RTs).
- Settings persist to settings.ini on every change (atomic tempfile + rename, so per-keystroke
  saves are safe).
- The setup screen also hosts the standalone Tools: the batch .arc extractor and the customize
  image extractor (see docs/ddr.md sections 11-12), both worker-thread + polled-Status.

## 7. qpro tab

- Drives the qpro extractor (docs/qpro.md): category checkboxes compose with the per-part
  date-grouped scan selection (a part renders iff category on AND part checked). A scan that
  selects nothing blocks extract; no scan = all parts.
- Body parts need the 520x704 render size (Setup preset) - the tab shows a warning otherwise;
  other categories extract at any size.
- The skip/failure Issue list renders prominently with a copy-to-clipboard button - a skip
  must never pass silently.
- Uses ImGuiListClipper for the per-part checkbox lists (thousands of rows).
