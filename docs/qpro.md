# qpro (IIDX Q-pro avatar) extractor

Knowledge captured from `src/qpro_dll.*`, `src/qpro_scan.*`, `src/qpro_extract.*`,
`src/qpro_internal.h`, `src/qpro_walk.*`, `src/qpro_composite.cpp`,
`src/qpro_composite_debug.cpp`, plus `src/ifs_inspect.*` (IFS/texturelist inspection shared
with the general renderer).

## 1. Part discovery: the bm2dx.dll static arrays (QproDll)

`QproDll::Read` is a PURE STATIC PE read of `<game>/modules/bm2dx.dll` - no boot, no loader.
It survives DLL updates because it pattern-scans rather than using fixed addresses:

- Each qpro part record is 16 bytes: qword 0 = VA of an ASCII ifs-name string
  (`qp_*.ifs`), qword 1 = flags (kept: low byte; sanity bound: flag < 0x10000).
- A record is valid iff the string pointer resolves (PE section VA -> file offset), the string
  starts with `qp_`, ends with `.ifs`, length >= 7, printable ASCII, <= 96 chars.
- Records are grouped into RUNS: candidate VAs that chain at +16 strides; a run must start
  where no record exists at VA-16 and be at least 16 entries long (kMinRun) to be kept. This
  rejects random string-pointer pairs elsewhere in the image.
- Six category arrays exist in bm2dx (index = part id, 16-byte stride). Category is classified
  from the ifs stem suffix: `_body`, `_hand`, `_face`, `_hair`, `_head`, `_bg` (the `_bg`
  suffix = the Back category). Entries within a category are ordered by VA, which reproduces
  the game's index = id ordering.
- SUFFIX-DIGIT quirk: a few parts carry a numeric variant suffix on the category tag - the
  Elpis character ships two heads as `qp_elpis_head1` / `qp_elpis_head2`. A trailing digit run
  is stripped before suffix matching; without this the two Elpis heads were dropped, the head
  array shortened by 2 and every later head id shifted down by 2 (our head_427 was the
  gallery's head_429). Verified against bm2dx.dll that ONLY those 2 heads are affected.
- Failure mode "no qpro arrays found (DLL layout changed?)" fires only when ALL six categories
  come back empty.
- Output JSON key names (2dx_qpro.json): bodyParts / handParts / faceParts / hairParts /
  headParts / backParts; output-stem prefixes: body / hand / face / hair / head / back.

Part IFS files live at `<game_dir>/data/graphic/<ifs>`.

## 2. Part scan + selection (qpro_scan)

The scan stamps each part with its source IFS's on-disk modified date ("YYYY-MM-DD", local
time) - the date is the GROUP KEY because a game update rewrites only its own IFS files, so
parts from one update share a date. The GUI groups by date so the user can render only the
parts from a recent update.

- Selection is applied at RENDER time only: unselected parts are skipped (existing outputs
  untouched) but STILL appear in the full 2dx_qpro.json manifest, and are still mounted for
  the cheap animated-ness detection that feeds the full qpro_videos.json.
- `PartSelection`: an EMPTY per-category vector means "all selected" (default before any scan).
- Scan runs synchronously on the render thread (`RunScan`); the GUI flips running=true
  immediately via `MarkScanRunning` for feedback before the render thread picks it up.
  `generation` bumps per completed scan so the panel re-seeds its checkboxes.
- Uses C++20 `std::chrono::clock_cast<system_clock>` (MSVC) to convert file_clock times.

## 3. The qp_motion COMPOSITE - the current render architecture

Every qpro part (hair/hand/face/head/back; body via texture swap) renders THROUGH the avatar:
mount `qp_main2.ifs`, play its `qp_motion` clip, then attach or texture-swap the item onto the
matching avatar layer. This supersedes the standalone-clip extractor. Key rules, all learned
the hard way:

### 3.1 MOUNT-ONCE (re-mount faults)

Re-mounting qp_main2 per item FAULTS the afp - the batch died on the 2nd item's remount.
The batch mounts qp_main2 ONCE (per body pass and once for the whole hand/face/hair/head/back
sweep) and per item swaps only the companion package, reusing texture slots (free the item's
companion textures and rewind the slot cursor after each item). `RenderItemComposite` has two
modes: OWN (pre_sid == 0, fresh one-off mount) and MOUNTED (reuse the caller's qp_main2).

### 3.2 DETACH-BEFORE-UNLOAD

Unloading a companion whose item clip is still attached to the avatar FAULTS the afp (killed
the batch on the 2nd item and the one-off tests at cleanup). Always
`AfpManager::DestroyCurrentStream` (destroy the master tree, detaching the mounted clips)
BEFORE `UnloadCompanion`. The next item re-plays qp_motion fresh.

### 3.3 Clip attach mechanism (bm2dx afp_mc_attach_stream)

The game's item-clip mount call chain (bm2dx, ending at afp_mc_attach_stream) is
reproduced as:

1. `afpu_afp_get_info_in_package` (libafputils-for-modern: afpu ordinal 0x062,
   `GetFunc(0x062, "afpu_afp_get_info_in_package")`) with the ITEM's clip name; the item clip
   stream/data id = `(u32)info[3]`.
2. `afp_mc_get_id_by_path(sid, layer)` resolves the avatar layer mc; walk the same-name
   sibling chain via `afp_mc_get_relative_id(mc, 6)` (dir 6 = next same-name sibling), exactly
   like the game, attaching to EVERY sibling.
3. `afp_mc_attach_stream(mc, data_id)` (afp ordinal 0x6e = the clip-mount).
4. `afp_mc_get(mc, 0x101E, 1)` finalizes the attach.

Clip parts (head/hand/face) attach their clip. STATIC parts (hair/back) have NO clip -
get_afp_info returns -2 - so their atlas region is TEXTURE-SWAPPED onto the avatar layer's
default bitmap instead (the game's mechanism for clip-less parts, same as the body pass).
Using the layer name for the clip lookup AND skipping the atlas fallback is what made every
hair/back export the avatar's DEFAULT part (fixed).

### 3.4 FREEZE-PLAY (0xF08 semantics)

`AfpManager::SeekFrame(g_afp, 0)` deep-goto-plays the master (afp op 0xF08, deep-syncs child
frames via the afp-core deep-sync routine) freezing the WHOLE avatar at the
frame-0 rest pose. Then
ONLY the target part's own timeline is advanced:

- `afp_mc_control_frame(part_mc, 0xF08, K)` is the real frame SETTER (deep_goto_play);
  property 0x1010 is a GETTER. The op's refer gate must be satisfied first with
  `afp_stream_control(6, part_mc)`.
- Rendering WITHOUT ticking the master (`afp_do_update`) keeps the avatar a statue while the
  part steps through frames 0..total-1.
- The part's bounded total = property 0x1011 read via `afp_mc_set(part_mc, 0x1011, &t)`
  (afp_mc_set doubles as a property reader here); this is the part's intrinsic cycle length,
  NOT qp_motion's ~11792-frame idle cycle. A static item collapses to one frame. The batch
  caps clip totals at 600 frames.
- Clips mounted into an already-playing qp_motion start FROZEN at cur=0 - neither afp_do_update
  nor a master deep-goto reaches them; each mounted layer mc must be 0xF08-driven directly
  (confirmed by the eagle-head composite diagnostic).

### 3.5 The SHARED CANVAS

Every part is exported on the FULL composite frame (the 520x704 render), so all AVIFs are
identically sized and overlay-able: stack them at (0,0) and the assembled avatar comes out
with zero alignment. Reference measurements: head x[206..420] y[82..210],
hand_l/umbrella x[187..470] y[66..362], hand_r x[50..333]. An older fixed hand-window crop
(kHandCompLX=168, kHandCompRX=44, kHandCompY=56, 308x324; _r mirrors _l about the 520-wide
frame centre - items attach at the avatar WRIST, a fixed point per side) and a shared window
(kSharedX=36, kSharedY=18, 448x376) survive in qpro_composite.cpp for the one-shot tools.

Body parts REQUIRE the 520x704 render size (the qpro avatar preset); the batch fails all
bodies with a GUI issue if the render is any other size.

### 3.6 Avatar layout (qp_main2 / qp_motion)

All 21 top-level clips of qp_motion (also the draw order used by the game;
):

```
qpro_bg, qp_cat_1, qp_cat_2, qp_cat_3, qp_head_b_neutral, qp_hair_b, qp_body_b,
qp_arm_r_upper, qp_arm_r_lower, qp_hand_r_neutral, qp_leg_r_lower, qp_leg_r_upper,
qp_leg_l_lower, qp_leg_l_upper, qp_body_f, qp_face_neutral, qp_hair_f,
qp_head_f_neutral, qp_arm_l_upper, qp_arm_l_lower, qp_hand_l_neutral
```

One composite render shows EXACTLY ONE layer (all others hidden via
`McControl::SetClipVisible`, which walks the dir-6 same-name chain like the game). Hiding all
21 leaves the character composite present-but-invisible - only a co-present qp_bg stream
paints, which is exactly the game's two-scene render (bm2dx) with the
character suppressed.

Category -> composite jobs (avatar layer, item clip, atlas fallback, hue effect):

- Head: qp_head_f_neutral / qp_head_b_neutral (clips; atlas qp_head_f / qp_head_b) -> _f, _b
- Hair: qp_hair_f / qp_hair_b layers, clips qp_hair_f_neutral / qp_hair_b_neutral (atlas
  qp_hair_f / qp_hair_b) -> _f, _b
- Hand: qp_hand_l_neutral / qp_hand_r_neutral (hue effects qp_hand_l / qp_hand_r) -> _l, _r
- Face: qp_face_neutral -> single layer
- Back: qpro_bg layer, item clip/atlas qp_bg -> single layer

Body is not a composite job: it is a multi-clip torso/limb bitmap re-point
(kBodyClips = qp_body_f, qp_body_b, qp_arm_r_upper/lower, qp_arm_l_upper/lower,
qp_leg_r_upper/lower, qp_leg_l_upper/lower), then qp_motion renders at frame 0 (SeekFrame(0) =
the SAME frozen pose every composite part uses, so body + head + hands align).

This mirrors the game's own body-equip mechanism exactly. For each piece clip the game
resolves the piece bitmap FROM THE COMPANION package by name (afp-utils image lookup, ord 0x046
-> image-to-stream-args, ord 0x04c) and re-points the clip's whole sibling chain to that bitmap
via afp_play_work_load_image (afp-core ord 0x088) + invalidate (mc prop 0x101E), walking
siblings with afp_mc_get_relative_id(mc, 6). The clip therefore adopts the companion bitmap's
OWN size and anchor. `SwapClipBitmapFromCompanion` (see engine_binding.md) reproduces this;
`SwapBodyPieces` calls it per clip after qp_motion is switched in.

A body piece must never be blitted from the companion atlas into a fixed qp_main2 atlas slot.
That earlier approach only rendered correctly when the companion piece matched the qp_main2
piece dimensions. When a body ships an EMPTY placeholder piece (e.g. a bloomer/gym body whose
qp_body_b is a blank 76x82 = no back element) the blit overwrote only that sub-region of the
shared atlas and left the PREVIOUS body's larger piece (e.g. a vampire cape) showing through;
when a piece was LARGER than the qp_main2 slot (e.g. an outfit whose qp_leg_l_lower is wider) it
overflowed into the neighbouring packed sub-image. The bitmap re-point has neither failure mode
because each clip carries its own geometry and nothing is shared across bodies.

The hand-swap diagnostic tool (kHandSwap = qp_hand_l, qp_hand_l2, qp_hand_r, qp_hand_r2) still
uses the atlas-blit path; the shipping hand extraction is the item-clip composite job, not that
tool.

## 4. Backgrounds (Back category) and native fps

Backgrounds are independent of the avatar in the game, but composite for POSITION:

- A background IFS mounted STANDALONE has qp_bg as the MASTER, advancing at its OWN authored
  rate. Once attached to the avatar's qpro_bg layer it is SLAVED to the 60fps master - its
  native rate becomes unobservable. Therefore `ProbeBackNativeFps` measures the rate
  standalone (tick `afp_do_update(1/fps)` N=40 times, read playhead advance from afp state;
  native = adv/N*fps) and MUST run before qp_main2 is mounted (it umounts /afp/packages).
- `RenderBackComposite` attaches qp_bg to qpro_bg (correct avatar-relative transform), then
  real-time ticks with SCALED dt: `dt = (1/fps) * (native_fps / 60)` - because the attached
  clip follows the 60fps master, ticking raw 1/fps would run a 30fps bg at 2x.
  Pure freeze-play (0xF08 on the qpro_bg mc) was WRONG for backs: it moved only that mc's own
  playhead and left NESTED sub-clips frozen (broken, non-looping bg); only afp_do_update
  advances the whole tree.
- Re-hide the other avatar layers EVERY tick (the master keeps advancing the hidden avatar).
- Loop end is pure afp state, never pixels: a ONE-SHOT reaches its last frame
  (cur >= total-1, playhead STICKS); a LOOPER wraps (loop_count property 0x1013 climbs past
  the start). Capture stops on either.
- `RenderBackRealtime` renders a back standalone (its own master, native rate): real-time
  sample at 1/fps for exactly one loop, so a 30fps bg is held for 2 output frames at 60fps =
  native speed.

## 5. STATIC vs ANIMATED classification (afp data, not pixels)

A clip is ANIMATED iff it has visual display-list commands AFTER frame 0, read straight from
afp-core's parsed movie definition (never a pixel compare):

- Per movie def: def+16 = frame table, def+8 = 24-byte command pool.
- frame table +4 = frame count; +16 = u32 offset to the per-frame packed index array.
- Per frame f, packed u32 at `(frame_table + idx_off + 4*f)`: command count = bits >> 20,
  pool start index = bits & 0xFFFFF.
- Each command's opcode = `((*(u32)cmd >> 1) & 0x3FF)`. Display-CHANGING opcodes: 0x7F
  (place/move), 0x80 (remove), 0x88. A truly static clip has commands only at frame 0
  (initial placement) -> count 0.
- The mc must be "refered" first: `afp_stream_control(6, mc_id)` sets the +0x800 gate the
  resolver checks. Reads raw afp-core structs, so the whole walk is SEH-guarded (-2 on fault;
  any negative = "treat as animated, never drop a real animation").
- Re-find on a DLL bump: the afp-core mc_id -> work resolver, reached from the
  XCd229cc000072 ordinal-0x72 dispatcher; the work -> def resolver; structure
  from `afp_set_play_data_frame_core`.
- LIVE-TREE walk: work+72 = first child, child+88 = next sibling (offsets from
  `afp_mc_tick_advance_one`'s child walk). Summing over the whole live node tree catches an
  animated sprite placed ONCE at frame 0 whose motion lives in an UNNAMED child (invisible to
  EnumerateChildClips) - e.g. back_49 / qp_lane_v_1p_bg, a 390-frame lane whose scroll lives
  in an unnamed child.

Loop-seam handling: a LOOPING part's final authored frame duplicates frame 0, causing a
one-frame stutter at the seam. Detected via afp state (0xF08 to the last frame, tick a few
times, read 0x1010; if the playhead WRAPPED below total-1 the clip loops) and the redundant
frame is dropped (emit total-1 frames).

## 6. Hue scope (the rainbow-hand fix)

Effect hands (rainbow blade/ring) are ONE afp node with two fills: the effect bitmap
(qp_hand_l / qp_hand_r) + a static base fill (qp_hand_l2 / qp_hand_r2, e.g. the gold sword
hilt / shuriken). afp merges them into ONE draw with the hue filter active on both; the live
game hue-shifts only the effect. Reproduction: gate the HSV shader per fragment on the effect
bitmap's UV rect in the atlas:

- `ScopeHueToImage` (region 1): restrict the hue filter to the effect bitmap's atlas rect
  (u0..u1, v0..v1 = pixel rect / atlas size, via `AfpD3D9::SetHsvScopeRect`).
- LEFT hand needs region 2 (`SetHsvScopeRect2` on qp_hand_l2): the afp merge keeps only the
  shuriken's SATURATION and drops the ring's hue; region 1 recovers the ring's hue at
  qp_hand_l while region 2 applies the draw's sat at qp_hand_l2
.
- Only the HAND category has the dual scope plumbing. Detection of an effect hand:
  `CountWithPrefix(texlist, base) > 1` (an animation-variant base like qp_hand_r + qp_hand_r2
  exists).
- In the one-shot ClipOne, the effect bitmap name = clip name minus its trailing `_<state>`
  segment (qp_hand_r_neutral -> qp_hand_r).
- Default ON; `--qpro-no-hue-scope` (SetHueScopeEnabled(false)) reverts to afp-literal
  (whole draw hue-shifted).
- The mounted item clip draws from the COMPANION atlas, so scope against the companion
  texlist/slot base, not the avatar's.
- HandComposite (full-avatar afp 0x6e clip-mount) is a DEAD END for un-merging: the avatar
  z-sort merges MORE and leaks hsv-mono; kept as a diagnostic only. The shipped fix is the
  per-node reconstruction in afp_d3d9_callbacks.cpp + this scoping.

Wide held items (umbrellas etc.): `AfpD3D9::SetHandRenderShift(true)` pushes the HAND render
right so the whole item is in-frame; the capture then re-anchors the crop at the content's
TRUE left (= min(render x offset, content bbox left)) so a normal hand keeps the natural
framing while a wide item is cut on the RIGHT - exactly like the game's container
(SetHandRenderShift keeps its left in-frame). qp_iris_hand's 268px umbrella exceeds the 212
slot and is cut right, matching the game.

## 7. Hand canvas widths (walk-path constants, PROVEN from game data)

The old clip-walk path used per-category design canvases (still in qpro_walk.h):

- back 342x502; head 262x352; hair 262x352; face 150x158 (face is the only SOFT canvas:
  genuinely-bigger faces 182x182 / 254x216 are kept at native size, only smaller content is
  padded up; all other categories hard-crop).
- Hand: height 352; RIGHT width 212 (the widest hand item, qp_25nyah, fills it exactly);
  LEFT width = RIGHT width. PROVEN symmetric from game data: qp_main2's default LEFT and
  RIGHT hand slots are BYTE-IDENTICAL - `afp/bsi/x_qp_hand_l_neutral` ==
  `x_qp_hand_r_neutral` (same md5 08380dfc..., both reference geos "5 10 15") - so the
  avatar's two hand slots are mirror copies and the _l container MUST equal the _r container.
  The hand-made gallery's old _l=162 was an under-crop. Future-proof: re-verify per game
  version by cmp-ing the two slot bsi files (identical => keep _l == _r).
- Body canvas: 520x704 full frame; legacy tight crop was (167,74) 260x352.

## 8. Front/back pairs and occlusion policy (FINAL)

Head and hair are front/back pairs (qp_head_f/b, qp_hair_f/b), exported as head_<id>_f /
head_<id>_b etc. POLICY: each layer is exported as its OWN raw afp animation with NO
occlusion baking. The back's appear/disappear (e.g. qp_27eagle1_head's mask-synced fade:
qp_head_b fades in ~frame 420, gone by 599) is the game's own timeline; the game composites
_f over _b at draw time, so the consumer (webui) does the final layering. An interim
exporter-side `_b.alpha *= 1 - _f.alpha` attenuation was REMOVED - it cut up animated backs.

mask_type proof (the direct RE confirmation):
afp_mc_get property 0x103A per node = mask_type; 0 = ordinary drawn layer, non-zero = an afp
clip-mask (its color gated out by the render walk at afp_play_work_draw_sub).
The eagle qp_head_b reads 0 on EVERY node, so its black silhouette is hidden purely by
qp_head_f drawn over it (occlusion), never a clip-mask. The `ProbeLayerMaskType` helper in
qpro_extract.cpp runs this check for any clip.

"Composed placeholder" pairs: an all-transparent static FRONT region (qp_head_f / qp_hair_f
bitmap alpha <= 8 everywhere) means the visible art only assembles in the clip - detected
STRUCTURALLY from the bitmap, never from playback pixels. But a composed front is NO reason
to skip a pair's static fallback entirely: qp_pitapat_hair has NO afp clips at all - its
qp_hair_b IS the back hair while qp_hair_f is a 1009-byte empty placeholder.

Composed/effect heads (e.g. qp_27eagle1_head's assembling mask) extract via the head's
qp_head_f/b_neutral clips driven in the composite - the standalone atlas crop wrongly dumps
the raw (transient) bitmap.

## 9. Alpha and pixel-format rules

- AFP renders PREMULTIPLIED; the gallery consumer expects STRAIGHT alpha - proven when an
  a=0 export made a burst disappear. `UnpremultiplyBGRA` divides RGB back out by alpha
  (skipping a=0 and a=255).
- Additive glows carry alpha = max(rgb) from the additive-coverage shader (g_afp_add_ps), so
  unpremultiply recovers the straight glow color (rgb/coverage) without hue skew and black
  stays transparent (see memory doc renderer573_additive_alpha).
- `ReadPiece` (atlas crop) returns STRAIGHT-alpha pixels already - directly comparable to
  unpremultiplied clip frames.
- texturelist.xml imgrect values are HALVED for pixel coords: x = r[0]/2, y = r[2]/2,
  w = (r[1]-r[0])/2, h = (r[3]-r[2])/2 (imgrect stores doubled units).

## 10. fps derivation (walk path)

`AfpFpsFromSteps(steps, frames, fallback)`: the engine steps the timeline 1/120 s per update,
so afp_fps = 120 / (update-steps per advanced frame), k clamped to [1,12]. qp_bg and character
_neutral clips mix 60fps and 30fps; encoding a 30fps clip at a fixed 60 plays it 2x too fast.
IMPORTANT LIMIT: this only works when the clip is the MASTER. An ATTACHED clip's playhead
follows the 60fps master, not its own rate - per-clip fps detection inside the composite was
wrong and was replaced by: composited parts are avatar-synced (60fps), encode each authored
frame 1:1 at the output fps; backgrounds use the standalone pre-probe (section 4).

## 11. Batch outputs and no-skip guarantees

- Output root is always a `qpro_assets` subfolder of the user-picked dir (mirrors the
  customize extractor's convention) - never loose files in the picked folder.
- ANIMATED parts emit a fixed trio (no format knob): transparent WebM VP9
  (Chrome/Edge/Firefox) + HEVC-with-alpha MP4 hvc1 (Safari/WebKit, which plays neither
  VP9-alpha nor AV1-alpha) + a static AVIF poster (first frame). STATIC parts are a lone AVIF
  still. Quality: AVIF = libaom CRF 40 (kQproAvifQuality, GPU AV1 preferred for color with
  libaom fallback for the gray8 alpha), VP9 CRF 32, WebP quality 85.
- NEVER SKIP: every expected layer file must exist after a part renders. A missing source IFS
  or empty layer produces a transparent FULL-CANVAS (520x704) placeholder so the gallery never
  has gaps and placeholders overlay-align with real renders. An airtight post-check fills a
  placeholder for any layer file absent after the composite (animated parts always leave the
  .avif poster, so absence = failure).
- Every skip/failure is pushed onto a live Issue list (label + ifs + reason) the GUI shows -
  a skip must never pass silently.
- `2dx_qpro.json` = the full part manifest (all parts, selected or not).
- `qpro_videos.json` = which output stems are ANIMATED (key = output-file stem, e.g.
  "head_284_b"; statics absent). Complete regardless of render selection: detection runs for
  EVERY part (detect_only pass for unselected ones), then unioned with a full directory scan
  of existing .webm/.mp4 files in qpro_assets (so prior runs' videos are never dropped). The
  dir scan uses a private error_code + non-throwing increment + no per-file stat so one
  locked/unreadable entry cannot truncate it.
- `QPRO_LIMIT=<n>` env caps items per category for previews (0 = all).
- Progress counts EVERY part in the selected categories (even unselected ones are mounted for
  detection), so the bar reflects real work.
- The effect-bg blank issue (qp_*_bg lone-master corruption) is solved by the co-present
  composite render; the historical diagnostic `BackComposite` traces the afp-core matrix-stack
  index (u16) and table ptr: effect bgs
  (x_qp_bg_fire/ring baked into the root timeline) strand the index at +2 (0 -> 2 on frame 0,
  then stable) with a NON-NULL table - a genuine missing pop
.

## 12. IFS inspection helpers (ifs_inspect, shared with the renderer)

- Variant-slot discovery pipeline: read `/afp/packages/tex/texturelist.xml` (bitmap dictionary:
  texturelist/texture/image@name) + `/afp/packages/afp/afplist.xml` (animation roots:
  afplist/afp@name), then probe `afp_mc_get_id_by_path(stream, name)` for each animation name,
  each bitmap name (authors often name clips after their bitmap - catches child slots like
  coin / paseli / m_paseli that are not afplist roots), and a conservative list of well-known
  Konami clip names (coin, paseli, e_amu, copylight, m_paseli, m_ketai, cardless, btn_*,
  text_*, chara, bg, fg, lang_*). Only resolvable paths become slots. No per-IFS hardcoding -
  every AFP game exposes its scene as paths resolvable against the stream.
- default_bitmap seeding uses the clip path itself (title.ifs-style clips like "coin" render
  the same-named bitmap as their authored default); the user can correct via free text.
- `LoadDictionary` MUST rebuild bitmap_names/anim_names from scratch each call (IfsConfig is
  cached across reloads; appending would duplicate entries N times). Slots are intentionally
  kept (user config survives reload; ProbeSlots dedups).
- The two-level texturelist walk (atlas -> image) uses `property_search` with
  starting_node = the texture node for relative children - the same pattern afp-utils'
  layoutlist/group/layout walker uses.
- `CountExpectedTextures` counts texturelist/texture atlas blocks; each maps 1:1 to one
  AfpD3D9::TexCreate call at package load - the correct denominator for the load progress bar
  (0 = unknown -> indeterminate bar).
- Per-atlas sampler filters (`ReadAtlasFilters`): SDVX BG IFSes (select_bg_iii.ifs etc.)
  declare `mag_filter="nearest" / min_filter="nearest"` on the BG atlas because the atlas
  packs unrelated sub-images adjacently; LINEAR filtering bilinear-bleeds the neighbour
  sub-image's first column at a UV-rect edge - the visible 1-pixel seam at exactly screen
  x=540 on BG 3. The game honours these strings; the renderer must apply them as
  D3D9 sampler state (nearest = D3DTEXF_POINT = 1, linear = D3DTEXF_LINEAR = 2, missing = 0 =
  leave default). Multi-stage BG atlases use NEAREST; small UI atlases use LINEAR with 1-unit
  UV insets. Ordering guarantee: the AFPU texture loader (afp-utils) walks
  texturelist FORWARD exactly once during package open, so declaration order == TexCreate-call
  order (the N-th filter entry applies to the N-th created texture). The old behaviour
  (global ANISOTROPIC/LINEAR from the settings publisher) broke BG atlases.
- Locale companions: IIDX ships each scene IFS as base + optional overlays `<stem>_j` /
  `_a` / `_k` (.ifs; Japanese / Asian / Korean). bm2dx hardcodes these paths in a static table
  (the per-scene locale-path table function); the tool derives them from the
  naming convention on disk
  (companions may not be mounted yet, so plain filesystem checks, not AVS). The suffix list is
  extensible (e.g. a future `_e`).

## Head ids: outputs are named by the GAME id (dense DLL index)

The game id for a qpro part is the DENSE index into the current DLL's part
array; that is the value stored in the player profile and sent on the wire.
Verified in-game on Sparkle Shower: save id 286 renders dense[286]
qp_arena_head (a holed space would have rendered dense[284]
qp_27eagle1_head). The head array is append-only across versions (verified
across data versions 2024110500 -> 20260629: 401 -> 447 heads, pure appends).