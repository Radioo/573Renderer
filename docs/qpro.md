# qpro (IIDX Q-pro avatar) extractor

Knowledge captured from `src/qpro/qpro_dll.*`, `src/qpro/qpro_scan.*`, `src/qpro/qpro_extract.*`,
`src/qpro/qpro_internal.h`, `src/qpro/qpro_walk.*`, `src/qpro/qpro_composite.cpp`,
`src/qpro/qpro_composite_debug.cpp`, plus `src/ifs_inspect.*` (IFS/texturelist inspection shared
with the general renderer).

TU layout of the extractor itself (split out of a single 966-line
qpro_extract.cpp when it approached the 1000-line gate):

| TU | Holds |
|---|---|
| `qpro/qpro_extract.cpp` | the `Run` orchestrator, the category sweep (MountCategorySweep / SweepOneItem / RunCategorySweep / placeholders), the two JSON writers, and the status plumbing that backs `qpro/qpro_status.h` |
| `qpro/qpro_detail.cpp` | the shared `detail` primitives every qpro TU uses: texturelist parse + atlas lookup, hue-scope rects, the WIC PNG writer, still-AVIF write, IfsPath/Stem, ReadPiece, RenderFrame/WarmUpFrames, the LayerJob category map (`CompositeJobs`), and `QproLimit` |
| `qpro/qpro_body.cpp` | everything body-specific: the 520x704 canvas gate, qp_main2 mount, per-body piece re-pointing (`SwapBodyPieces`), the body pass, and the `BodyOne` debug driver |
| `qpro/qpro_debug.cpp` | the screenshot-dumping single-item drivers (`BackOne`, and `HeadOne`/`HandOne`/`HairOne`/`FaceOne` over one shared `CategoryOne`) |

`qpro/qpro_status.h` lets the body pass and the sweep share one progress/issue
channel. The mutex, `Status` and the done/total counters stay private to
qpro_extract.cpp, and the other TUs reach them only through `PublishProgress` /
`Note` / `NoteIssue` / `BumpDone` / `DoneCount` / `TotalCount`.

### Shared composite primitives

Five near-identical copies of the item-clip attach walk (hand real + hand
debug + head debug + generic item + back) were merged into one
`AttachClipStream(pkg, sid, layer, clip_name, log_tag)` in qpro_detail.cpp.
It resolves the layer's head mc, looks the clip's stream up once (falling
back from `item_clip` to `layer`, which the generic item path needed), then
attaches + invalidates down the `dir 6` same-name sibling chain, capped at
64. It returns `{head_mc, attached, have_stream, data_id}` so the callers
keep their own follow-ups: the back path wants `head_mc` even when nothing
attached, and the generic item path falls back to an atlas texture-swap when
`have_stream` is false. The two debug callers pass a log tag; passing none
logs nothing. The lookup can sit outside the per-sibling loop because it is
a pure `(pkg, name)` query.

Three hand-maintained "hide everything except..." arrays (kNonHand,
kNonHead, kHideForBody) were replaced by
`HideAllLayersExcept(sid, keep)` over `kAllAvatarLayers`. This changes the
order the hides are issued in (the old arrays each had their own order), and
the byte-compare below shows the change has no effect. It only ever hides
and never sets a kept layer visible, which is why `HideAllButQproBg` in
qpro_back.cpp stays separate (it forces `qpro_bg` visible).

`WriteStillFrameWithDump` + `AvifPathToPng` replace three copies of the
"write the still AVIF, and if `g_clip_dump_raw` also drop the same pixels
as .png next to it" block.

All of the above was checked with a QPRO_LIMIT=2 extraction over all six
categories at 520x704 (`--qpro-parts hand,back,head,hair,face,body`), which
produces 20 files; every one is byte-identical before and after. That run is
the only byte-level check that covers the qpro pipelines (the
render-regression net in docs/render_regression.md does not touch them), so
re-run it after changing anything in this directory.

GUI: since P17 the qpro tab is registered only for the `iidx33` profile (panel
registry, docs/gui.md 2.0). It reads bm2dx.dll, so it had no use on any other
game, where it used to render anyway.

## 1. Part discovery: the bm2dx.dll static arrays (QproDll)

`QproDll::Read` is a pure static PE read of `<game>/modules/bm2dx.dll`, with no boot and no
loader. It pattern-scans instead of using fixed addresses, so it keeps working across DLL
updates:

- Each qpro part record is 16 bytes: qword 0 = VA of an ASCII ifs-name string
  (`qp_*.ifs`), qword 1 = flags (kept: low byte; sanity bound: flag < 0x10000).
- A record is valid iff the string pointer resolves (PE section VA -> file offset), the string
  starts with `qp_`, ends with `.ifs`, length >= 7, printable ASCII, <= 96 chars.
- Records are grouped into runs: candidate VAs that chain at +16 strides. A run must start
  where no record exists at VA-16 and be at least 16 entries long (kMinRun) to be kept, which
  rejects random string-pointer pairs elsewhere in the image.
- bm2dx has six category arrays (index = part id, 16-byte stride). The category comes from
  the ifs stem suffix: `_body`, `_hand`, `_face`, `_hair`, `_head`, `_bg` (the `_bg`
  suffix = the Back category). Entries within a category are ordered by VA, which reproduces
  the game's index = id ordering.
- Suffix-digit quirk: a few parts carry a numeric variant suffix on the category tag. The
  Elpis character ships two heads as `qp_elpis_head1` / `qp_elpis_head2`. A trailing digit run
  is stripped before suffix matching; without this the two Elpis heads were dropped, the head
  array shortened by 2 and every later head id shifted down by 2 (our head_427 was the
  gallery's head_429). Checked against bm2dx.dll: only those 2 heads are affected.
- The failure "no qpro arrays found (DLL layout changed?)" fires only when all six categories
  come back empty.
- Output JSON key names (2dx_qpro.json): bodyParts / handParts / faceParts / hairParts /
  headParts / backParts; output-stem prefixes: body / hand / face / hair / head / back.

Part IFS files live at `<game_dir>/data/graphic/<ifs>`.

## 2. Part scan + selection (qpro_scan)

The scan stamps each part with its source IFS's on-disk modified date ("YYYY-MM-DD", local
time). The date is the group key: a game update rewrites only its own IFS files, so parts from
one update share a date. The GUI groups by date so the user can render only the parts from a
recent update.

- Selection applies at render time only. Unselected parts are skipped (existing outputs
  untouched) but still appear in the full 2dx_qpro.json manifest, and are still mounted for
  the cheap animated-ness detection that feeds the full qpro_videos.json.
- `PartSelection`: an empty per-category vector means "all selected" (default before any scan).
- The scan runs synchronously on the render thread (`RunScan`); the GUI sets running=true
  immediately via `MarkScanRunning`, so the user sees it start before the render thread picks
  it up. `generation` bumps per completed scan so the panel re-seeds its checkboxes.
- Uses C++20 `std::chrono::clock_cast<system_clock>` (MSVC) to convert file_clock times.

## 3. The qp_motion composite - the current render architecture

Every qpro part (hair/hand/face/head/back; body via texture swap) renders through the avatar:
mount `qp_main2.ifs`, play its `qp_motion` clip, then attach or texture-swap the item onto the
matching avatar layer. This replaces the standalone-clip extractor. Each rule below comes from
a failure the earlier code ran into.

### 3.1 Mount-once (re-mount faults)

Re-mounting qp_main2 per item faults the afp; the batch died on the 2nd item's remount.
The batch mounts qp_main2 once (per body pass and once for the whole hand/face/hair/head/back
sweep) and per item swaps only the companion package, reusing texture slots (free the item's
companion textures and rewind the slot cursor after each item). `RenderItemComposite` has two
modes: OWN (pre_sid == 0, fresh one-off mount) and MOUNTED (reuse the caller's qp_main2).

### 3.2 Detach-before-unload

Unloading a companion whose item clip is still attached to the avatar faults the afp (it killed
the batch on the 2nd item and the one-off tests at cleanup). Always call
`AfpManager::DestroyCurrentStream` (destroy the master tree, detaching the mounted clips)
before `UnloadCompanion`. The next item plays qp_motion again from the start.

### 3.3 Clip attach mechanism (bm2dx afp_mc_attach_stream)

The game's item-clip mount call chain (bm2dx, ending at afp_mc_attach_stream) is
reproduced as:

1. `afpu_afp_get_info_in_package` (libafputils-for-modern: afpu ordinal 0x062,
   `GetFunc(0x062, "afpu_afp_get_info_in_package")`) with the item's clip name; the item clip
   stream/data id = `(u32)info[3]`.
2. `afp_mc_get_id_by_path(sid, layer)` resolves the avatar layer mc; walk the same-name
   sibling chain via `afp_mc_get_relative_id(mc, 6)` (dir 6 = next same-name sibling) as the
   game does, attaching to every sibling.
3. `afp_mc_attach_stream(mc, data_id)` (afp ordinal 0x6e = the clip-mount).
4. `afp_mc_get(mc, 0x101E, 1)` finalizes the attach.

Clip parts (head/hand/face) attach their clip. Static parts (hair/back) have no clip
(get_afp_info returns -2), so their atlas region is texture-swapped onto the avatar layer's
default bitmap instead (the game's mechanism for clip-less parts, same as the body pass).
Using the layer name for the clip lookup and skipping the atlas fallback made every hair/back
export show the avatar's default part; that is fixed.

### 3.4 Freeze-play (0xF08 semantics)

`AfpManager::SeekFrame(g_afp, 0)` deep-goto-plays the master (afp op 0xF08, deep-syncs child
frames via the afp-core deep-sync routine), which freezes the whole avatar at the frame-0 rest
pose. Then only the target part's own timeline is advanced:

- `afp_mc_control_frame(part_mc, 0xF08, K)` is the real frame setter (deep_goto_play);
  property 0x1010 is a getter. The op's refer gate must be satisfied first with
  `afp_stream_control(6, part_mc)`.
- Rendering without ticking the master (`afp_do_update`) keeps the avatar still while the
  part steps through frames 0..total-1.
- The part's bounded total = property 0x1011 read via `afp_mc_set(part_mc, 0x1011, &t)`
  (afp_mc_set doubles as a property reader here). This is the part's own cycle length, not
  qp_motion's ~11792-frame idle cycle. A static item collapses to one frame. The batch caps
  clip totals at 600 frames.
- Clips mounted into an already-playing qp_motion start frozen at cur=0. Neither
  afp_do_update nor a master deep-goto reaches them, so each mounted layer mc must be driven
  with 0xF08 directly (confirmed by the eagle-head composite diagnostic).

### 3.5 The shared canvas

Every part is exported on the full composite frame (the 520x704 render), so all AVIFs have the
same size and stacking them at (0,0) assembles the avatar with no alignment step. Reference
measurements: head x[206..420] y[82..210], hand_l/umbrella x[187..470] y[66..362], hand_r
x[50..333]. An older fixed hand-window crop (kHandCompLX=168, kHandCompRX=44, kHandCompY=56,
308x324; _r mirrors _l about the 520-wide frame centre, because items attach at the avatar
wrist, a fixed point per side) and a shared window (kSharedX=36, kSharedY=18, 448x376) remain
in qpro_composite.cpp for the one-shot tools.

Body parts require the 520x704 render size (the qpro avatar preset); the batch fails all
bodies with a GUI issue if the render is any other size.

### 3.6 Avatar layout (qp_main2 / qp_motion)

All 21 top-level clips of qp_motion, in the draw order the game also uses:

```
qpro_bg, qp_cat_1, qp_cat_2, qp_cat_3, qp_head_b_neutral, qp_hair_b, qp_body_b,
qp_arm_r_upper, qp_arm_r_lower, qp_hand_r_neutral, qp_leg_r_lower, qp_leg_r_upper,
qp_leg_l_lower, qp_leg_l_upper, qp_body_f, qp_face_neutral, qp_hair_f,
qp_head_f_neutral, qp_arm_l_upper, qp_arm_l_lower, qp_hand_l_neutral
```

One composite render shows exactly one layer (all others hidden via
`McControl::SetClipVisible`, which walks the dir-6 same-name chain like the game). With all 21
hidden, the character composite is present but invisible and only a co-present qp_bg stream
paints, which matches the game's two-scene render (bm2dx) with the character suppressed.

Category -> composite jobs (avatar layer, item clip, atlas fallback, hue effect):

- Head: qp_head_f_neutral / qp_head_b_neutral (clips; atlas qp_head_f / qp_head_b) -> _f, _b
- Hair: qp_hair_f / qp_hair_b layers, clips qp_hair_f_neutral / qp_hair_b_neutral (atlas
  qp_hair_f / qp_hair_b) -> _f, _b
- Hand: qp_hand_l_neutral / qp_hand_r_neutral (hue effects qp_hand_l / qp_hand_r) -> _l, _r
- Face: qp_face_neutral -> single layer
- Back: qpro_bg layer, item clip/atlas qp_bg -> single layer

Body is not a composite job. It is a multi-clip torso/limb bitmap re-point
(kBodyClips = qp_body_f, qp_body_b, qp_arm_r_upper/lower, qp_arm_l_upper/lower,
qp_leg_r_upper/lower, qp_leg_l_upper/lower), after which qp_motion renders at frame 0
(SeekFrame(0) gives the same frozen pose every composite part uses, so body + head + hands
align).

This is the game's own body-equip mechanism. For each piece clip the game resolves the piece
bitmap from the companion package by name (afp-utils image lookup, ord 0x046
-> image-to-stream-args, ord 0x04c) and re-points the clip's whole sibling chain to that bitmap
via afp_play_work_load_image (afp-core ord 0x088) + invalidate (mc prop 0x101E), walking
siblings with afp_mc_get_relative_id(mc, 6). The clip therefore takes the companion bitmap's
own size and anchor. `SwapClipBitmapFromCompanion` (see engine_binding.md) reproduces this;
`SwapBodyPieces` calls it per clip after qp_motion is switched in.

A body piece must never be blitted from the companion atlas into a fixed qp_main2 atlas slot.
That earlier approach only rendered correctly when the companion piece matched the qp_main2
piece dimensions. When a body ships an empty placeholder piece (e.g. a bloomer/gym body whose
qp_body_b is a blank 76x82 = no back element) the blit overwrote only that sub-region of the
shared atlas and left the previous body's larger piece (e.g. a vampire cape) showing through;
when a piece was larger than the qp_main2 slot (e.g. an outfit whose qp_leg_l_lower is wider) it
overflowed into the neighbouring packed sub-image. The bitmap re-point avoids both failures
because each clip carries its own geometry and nothing is shared across bodies.

The hand-swap diagnostic tool (kHandSwap = qp_hand_l, qp_hand_l2, qp_hand_r, qp_hand_r2) still
uses the atlas-blit path. The shipping hand extraction uses the item-clip composite job instead.

## 4. Backgrounds (Back category) and native fps

In the game, backgrounds are independent of the avatar, but they are composited for position:

- A background IFS mounted standalone has qp_bg as the master, advancing at its own authored
  rate. Once attached to the avatar's qpro_bg layer it follows the 60fps master, and its
  native rate can no longer be observed. So `ProbeBackNativeFps` measures the rate
  standalone (tick `afp_do_update(1/fps)` N=40 times, read playhead advance from afp state;
  native = adv/N*fps) and must run before qp_main2 is mounted (it umounts /afp/packages).
- `RenderBackComposite` attaches qp_bg to qpro_bg (correct avatar-relative transform), then
  ticks in real time with a scaled dt: `dt = (1/fps) * (native_fps / 60)`. The attached clip
  follows the 60fps master, so ticking raw 1/fps would run a 30fps bg at 2x.
  Plain freeze-play (0xF08 on the qpro_bg mc) was wrong for backs: it moved only that mc's own
  playhead and left nested sub-clips frozen (broken, non-looping bg). Only afp_do_update
  advances the whole tree.
- Re-hide the other avatar layers on every tick (the master keeps advancing the hidden avatar).
- Loop end comes from afp state, never pixels: a one-shot reaches its last frame
  (cur >= total-1, and the playhead stays there); a looper wraps (loop_count property 0x1013
  climbs past the start). Capture stops on either.
- `RenderBackRealtime` renders a back standalone (its own master, native rate): real-time
  sample at 1/fps for exactly one loop, so a 30fps bg is held for 2 output frames at 60fps =
  native speed.

## 5. Static vs animated classification (afp data, not pixels)

A clip is animated iff it has visual display-list commands after frame 0, read straight from
afp-core's parsed movie definition (never a pixel compare):

- Per movie def: def+16 = frame table, def+8 = 24-byte command pool.
- frame table +4 = frame count; +16 = u32 offset to the per-frame packed index array.
- Per frame f, packed u32 at `(frame_table + idx_off + 4*f)`: command count = bits >> 20,
  pool start index = bits & 0xFFFFF.
- Each command's opcode = `((*(u32)cmd >> 1) & 0x3FF)`. Opcodes that change the display:
  0x7F (place/move), 0x80 (remove), 0x88. A fully static clip has commands only at frame 0
  (initial placement) -> count 0.
- The mc must be "refered" first: `afp_stream_control(6, mc_id)` sets the +0x800 gate the
  resolver checks. The walk reads raw afp-core structs, so all of it is SEH-guarded (-2 on
  fault; any negative = "treat as animated, never drop a real animation").
- Re-find on a DLL bump: the afp-core mc_id -> work resolver, reached from the
  XCd229cc000072 ordinal-0x72 dispatcher; the work -> def resolver; structure
  from `afp_set_play_data_frame_core`.
- Live-tree walk: work+72 = first child, child+88 = next sibling (offsets from
  `afp_mc_tick_advance_one`'s child walk). Summing over the whole live node tree catches an
  animated sprite placed once at frame 0 whose motion lives in an unnamed child (invisible to
  EnumerateChildClips), e.g. back_49 / qp_lane_v_1p_bg, a 390-frame lane whose scroll lives
  in an unnamed child.

Loop-seam handling: a looping part's final authored frame duplicates frame 0, which causes a
one-frame stutter at the seam. This is detected from afp state (0xF08 to the last frame, tick
a few times, read 0x1010; if the playhead wrapped below total-1 the clip loops) and the
redundant frame is dropped (emit total-1 frames).

## 6. Hue scope (the rainbow-hand fix)

Effect hands (rainbow blade/ring) are one afp node with two fills: the effect bitmap
(qp_hand_l / qp_hand_r) + a static base fill (qp_hand_l2 / qp_hand_r2, e.g. the gold sword
hilt / shuriken). afp merges them into one draw with the hue filter active on both; the live
game hue-shifts only the effect. To reproduce this, the HSV shader is gated per fragment on
the effect bitmap's UV rect in the atlas:

- `ScopeHueToImage` (region 1): restrict the hue filter to the effect bitmap's atlas rect
  (u0..u1, v0..v1 = pixel rect / atlas size, via `AfpD3D9::SetHsvScopeRect`).
- The left hand needs region 2 (`SetHsvScopeRect2` on qp_hand_l2): the afp merge keeps only
  the shuriken's saturation and drops the ring's hue, so region 1 recovers the ring's hue at
  qp_hand_l while region 2 applies the draw's sat at qp_hand_l2.
- Only the hand category has the dual scope plumbing. An effect hand is detected by
  `CountWithPrefix(texlist, base) > 1` (an animation-variant base like qp_hand_r + qp_hand_r2
  exists).
- In the one-shot ClipOne, the effect bitmap name = clip name minus its trailing `_<state>`
  segment (qp_hand_r_neutral -> qp_hand_r).
- On by default; `--qpro-no-hue-scope` (SetHueScopeEnabled(false)) reverts to afp-literal
  (whole draw hue-shifted).
- The mounted item clip draws from the companion atlas, so scope against the companion
  texlist/slot base, not the avatar's.
- HandComposite (full-avatar afp 0x6e clip-mount) cannot un-merge the draw: the avatar z-sort
  merges more and leaks hsv-mono, so it is kept as a diagnostic only. The shipped fix is the
  per-node reconstruction in afp_d3d9_callbacks.cpp + this scoping.

Wide held items (umbrellas etc.): `AfpD3D9::SetHandRenderShift(true)` pushes the hand render
right so the whole item is in-frame; the capture then re-anchors the crop at the content's
true left (= min(render x offset, content bbox left)) so a normal hand keeps the natural
framing while a wide item is cut on the right, like the game's container
(SetHandRenderShift keeps its left in-frame). qp_iris_hand's 268px umbrella exceeds the 212
slot and is cut on the right, matching the game.

## 7. Hand canvas widths (walk-path constants, proven from game data)

The old clip-walk path used per-category design canvases (still in qpro_walk.h):

- back 342x502; head 262x352; hair 262x352; face 150x158 (face is the only soft canvas:
  faces that really are bigger, 182x182 / 254x216, are kept at native size and only smaller
  content is padded up; all other categories hard-crop).
- Hand: height 352; right width 212 (the widest hand item, qp_25nyah, fills it exactly);
  left width = right width. Game data proves the two are symmetric: qp_main2's default left and
  right hand slots are byte-identical (`afp/bsi/x_qp_hand_l_neutral` ==
  `x_qp_hand_r_neutral`, same md5 08380dfc..., both reference geos "5 10 15"), so the
  avatar's two hand slots are mirror copies and the _l container must equal the _r container.
  The hand-made gallery's old _l=162 was an under-crop. For each new game version, re-check
  by comparing the two slot bsi files (identical => keep _l == _r).
- Body canvas: 520x704 full frame; legacy tight crop was (167,74) 260x352.

## 8. Front/back pairs and occlusion policy (final)

Head and hair are front/back pairs (qp_head_f/b, qp_hair_f/b), exported as head_<id>_f /
head_<id>_b etc. Each layer is exported as its own raw afp animation with no occlusion baked
in. The back's appear/disappear (e.g. qp_27eagle1_head's mask-synced fade: qp_head_b fades in
~frame 420, gone by 599) is the game's own timeline; the game composites _f over _b at draw
time, so the consumer (webui) does the final layering. An interim exporter-side
`_b.alpha *= 1 - _f.alpha` attenuation was removed because it cut up animated backs.

mask_type proof (the direct RE confirmation):
afp_mc_get property 0x103A per node = mask_type; 0 = ordinary drawn layer, non-zero = an afp
clip-mask (its color gated out by the render walk at afp_play_work_draw_sub).
The eagle qp_head_b reads 0 on every node, so its black silhouette is hidden only because
qp_head_f is drawn over it (occlusion), never by a clip-mask. The `ProbeLayerMaskType` helper
in qpro_extract.cpp runs this check for any clip.

"Composed placeholder" pairs: an all-transparent static front region (qp_head_f / qp_hair_f
bitmap alpha <= 8 everywhere) means the visible art only assembles in the clip. This is
detected from the structure of the bitmap, never from playback pixels. A composed front still
does not justify skipping a pair's static fallback entirely: qp_pitapat_hair has no afp clips
at all, and its qp_hair_b is the back hair while qp_hair_f is a 1009-byte empty placeholder.

Composed/effect heads (e.g. qp_27eagle1_head's assembling mask) extract via the head's
qp_head_f/b_neutral clips driven in the composite; the standalone atlas crop wrongly dumps
the raw (transient) bitmap.

## 9. Alpha and pixel-format rules

- AFP renders premultiplied, and the gallery consumer expects straight alpha (an a=0 export
  that made a burst disappear showed this). `UnpremultiplyBGRA` divides RGB back out by alpha
  (skipping a=0 and a=255).
- Additive glows carry alpha = max(rgb) from the additive-coverage shader (g_afp_add_ps), so
  unpremultiply recovers the straight glow color (rgb/coverage) without hue skew and black
  stays transparent (see docs/d3d9_backend.md "Additive-coverage shader").
- `ReadPiece` (atlas crop) already returns straight-alpha pixels, which compare directly with
  unpremultiplied clip frames.
- texturelist.xml imgrect values are halved for pixel coords: x = r[0]/2, y = r[2]/2,
  w = (r[1]-r[0])/2, h = (r[3]-r[2])/2 (imgrect stores doubled units).

## 10. fps derivation (walk path)

`AfpFpsFromSteps(steps, frames, fallback)`: the engine steps the timeline 1/120 s per update,
so afp_fps = 120 / (update-steps per advanced frame), k clamped to [1,12]. qp_bg and character
_neutral clips mix 60fps and 30fps; encoding a 30fps clip at a fixed 60 plays it 2x too fast.
This only works when the clip is the master. An attached clip's playhead follows the 60fps
master instead of its own rate, so per-clip fps detection inside the composite was wrong. It
was replaced by these rules: composited parts are avatar-synced (60fps) and encode each
authored frame 1:1 at the output fps; backgrounds use the standalone pre-probe (section 4).

## 11. Batch outputs and no-skip guarantees

- The output root is always a `qpro_assets` subfolder of the user-picked dir (as the
  customize extractor does), never loose files in the picked folder.
- Animated parts emit a fixed trio (no format knob): transparent WebM VP9
  (Chrome/Edge/Firefox) + HEVC-with-alpha MP4 hvc1 (Safari/WebKit, which plays neither
  VP9-alpha nor AV1-alpha) + a static AVIF poster (first frame). Static parts are a lone AVIF
  still. Quality: AVIF = libaom CRF 40 (kQproAvifQuality, GPU AV1 preferred for color with
  libaom fallback for the gray8 alpha), VP9 CRF 32, WebP quality 85.
- No skips: every expected layer file must exist after a part renders. A missing source IFS
  or empty layer produces a transparent full-canvas (520x704) placeholder, so the gallery has
  no gaps and placeholders line up with real renders. A post-check fills a placeholder for any
  layer file still absent after the composite (animated parts always leave the .avif poster,
  so absence = failure).
- Every skip or failure goes onto a live Issue list (label + ifs + reason) that the GUI
  shows, so none passes silently.
- `2dx_qpro.json` = the full part manifest (all parts, selected or not).
- `qpro_videos.json` = which output stems are animated (key = output-file stem, e.g.
  "head_284_b"; statics absent). It is complete regardless of render selection: detection runs
  for every part (detect_only pass for unselected ones), and the result is unioned with a full
  directory scan of existing .webm/.mp4 files in qpro_assets (so videos from earlier runs are
  never dropped). The dir scan uses a private error_code + non-throwing increment + no
  per-file stat, so one locked/unreadable entry cannot cut it short.
- `QPRO_LIMIT=<n>` env caps items per category for previews (0 = all).
- Progress counts every part in the selected categories (even unselected ones are mounted for
  detection), so the bar reflects the work actually done.
- The effect-bg blank issue (qp_*_bg lone-master corruption) is solved by the co-present
  composite render. The historical diagnostic `BackComposite` traces the afp-core matrix-stack
  index (u16) and table ptr: effect bgs (x_qp_bg_fire/ring baked into the root timeline)
  strand the index at +2 (0 -> 2 on frame 0, then stable) with a non-null table, which is a
  real missing pop.

## 12. IFS inspection helpers (ifs_inspect, shared with the renderer)

- Variant-slot discovery pipeline: read `/afp/packages/tex/texturelist.xml` (bitmap dictionary:
  texturelist/texture/image@name) + `/afp/packages/afp/afplist.xml` (animation roots:
  afplist/afp@name), then probe `afp_mc_get_id_by_path(stream, name)` for each animation name,
  each bitmap name (authors often name clips after their bitmap, which catches child slots like
  coin / paseli / m_paseli that are not afplist roots), and a conservative list of well-known
  Konami clip names (coin, paseli, e_amu, copylight, m_paseli, m_ketai, cardless, btn_*,
  text_*, chara, bg, fg, lang_*). Only resolvable paths become slots. There is no per-IFS
  hardcoding: every AFP game exposes its scene as paths resolvable against the stream.
- default_bitmap seeding uses the clip path itself (title.ifs-style clips like "coin" render
  the same-named bitmap as their authored default); the user can correct it via free text.
- `LoadDictionary` must rebuild bitmap_names/anim_names from scratch each call (IfsConfig is
  cached across reloads; appending would duplicate entries N times). Slots are kept on purpose
  (user config survives reload; ProbeSlots dedups).
- The two-level texturelist walk (atlas -> image) uses `property_search` with
  starting_node = the texture node for relative children, the same pattern afp-utils'
  layoutlist/group/layout walker uses.
- `CountExpectedTextures` counts texturelist/texture atlas blocks; each maps 1:1 to one
  AfpD3D9::TexCreate call at package load, which makes it the denominator for the load
  progress bar (0 = unknown -> indeterminate bar).
- Per-atlas sampler filters (`ReadAtlasFilters`): SDVX BG IFSes (select_bg_iii.ifs etc.)
  declare `mag_filter="nearest" / min_filter="nearest"` on the BG atlas because the atlas
  packs unrelated sub-images next to each other. LINEAR filtering bleeds the neighbouring
  sub-image's first column in at a UV-rect edge, which is the visible 1-pixel seam at exactly
  screen x=540 on BG 3. The game honours these strings, and the renderer must apply them as
  D3D9 sampler state (nearest = D3DTEXF_POINT = 1, linear = D3DTEXF_LINEAR = 2, missing = 0 =
  leave default). Multi-stage BG atlases use NEAREST; small UI atlases use LINEAR with 1-unit
  UV insets. The AFPU texture loader (afp-utils) walks texturelist forward exactly once during
  package open, so declaration order == TexCreate-call order (the N-th filter entry applies to
  the N-th created texture). The old behaviour (global ANISOTROPIC/LINEAR from the settings
  publisher) broke BG atlases.
- Locale companions: IIDX ships each scene IFS as base + optional overlays `<stem>_j` /
  `_a` / `_k` (.ifs; Japanese / Asian / Korean). bm2dx hardcodes these paths in a static table
  (the per-scene locale-path table function); the tool derives them from the naming
  convention on disk (companions may not be mounted yet, so these are plain filesystem
  checks, not AVS). The suffix list can be extended (e.g. a future `_e`).

## Head ids: outputs are named by the game id (dense DLL index)

The game id for a qpro part is the dense index into the current DLL's part
array; that is the value stored in the player profile and sent on the wire.
Checked in-game on Sparkle Shower: save id 286 renders dense[286]
qp_arena_head (a holed space would have rendered dense[284]
qp_27eagle1_head). The head array is append-only across versions (checked
across data versions 2024110500 -> 20260629: 401 -> 447 heads, all appends).
