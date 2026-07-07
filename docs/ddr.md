# DDR World (AFP 2.13.7) subsystem

Knowledge captured from `src/afp_ddr.h`, `src/afp_ddr_boot.cpp`, `src/afp_ddr_render.h`,
`src/afp_ddr_render.cpp`, `src/afp_ddr_test.*`, `src/arc_extract.*`, `src/customize_extract.*`.
The .arc container, AVS-LZ77 codec and DXT block decode are documented in
`docs/formats.md` - not repeated here.

## 1. What the DdrAfp module is

`DdrAfp` boots DDR World's legacy `libafp-win64.dll` (AFP 2.13.7) + `libafputils-win64.dll` +
`libavs-win64.dll`, wires the `DdrRender` D3D9 backend, mounts an IFS package and drives the
per-frame loop. It is the DDR-native analog of the modern `AfpManager`, mirroring two gamemdx
functions:

- boot: the gamemdx boot function
- package load / BM2D layer attach: the gamemdx package-load function and the
  BM2D layer-attach function

All afp/afpu functions are resolved BY NAME from the DDR DLLs (readable exports, unlike the
mangled avs ones).

## 2. Boot sequence (mirrors the gamemdx boot function)

Order matters:

1. Resolve libafp + libafputils exports by name.
2. `afp_ext_command(9, &ver)` returns the AFP version string.
3. `DdrRender::Init(device, w, h)` builds the render-params + afpu-config callback structs;
   `SetTexBindResolver(afpu_get_texture_bind_id)` wires texture-id resolution.
4. `afp_boot(render_params)`.
5. `afp_set_stream_max_nr(2048)`.
6. `afp_set_policy(1)` (env `DDR_POLICY` overrides).
7. `afp_system_set_attribute(mask, value)` - see section 2.1.
8. `afpu_boot(nullptr, afpu_config)`.
9. `afpu_set_afp_render_params(render_params)` + `afpu_set_render_params(afpu_config)`.
10. `afpu_system_set_parameter(1, 4096)`.
11. `afpu_system_set_attributes(mask, value)` - see section 2.2.

Do NOT call `afp_render_init()` at boot. In AFP 2.13.7 it calls
`afp_render_pp_start_single` (allocates the 4 primitive pools) and leaves them allocated for the
mode-2 render pattern (init -> do_render(.,2) -> render_finish). The self-contained mode 3 path
(`afp_do_render(dt,3,0)` + `afp_do_display`) brackets its own `pp_start_single` /
`pp_end_single`; calling render_init first makes mode 3's pp_start assert "already allocated"
(afp-render.c:316). gamemdx's boot also omits it.

### 2.1 afp_system_set_attribute - the mask-bit story (CRITICAL)

`afp_system_set_attribute(mask, value)` is 2-ARG: it does
`global = value | (global & ~mask)`. `afp_boot` defaults that global to 0x800.

- The gamemdx boot function (disasm-verified `mov edx,10h; mov ecx,edx`) sets
  (0x10,0x10) then (8,8) -> 0x818.
- gamemdx ALSO sets bit 0 via the lazy mask-system init function
  (`afp_system_set_attribute(1,1)`, then `(0x20,0)`), giving the runtime value **0x819**.
- Bit 0 gates the type-0 MASK pass in `afp_draw_frame_play_data` and the leaf/container draw
  functions: with bit 0 set, afp emits a
  PER-NODE mask (each clip clipped to its OWN bbox) instead of a single bg_root frame-mask
  bounding every child. Without bit 0 the bg_root frame-mask (bbox 123..1154 for
  background_0009) clipped the rotating bg_line bundles, cutting their L/R as they swung past.
  This is the game's real mechanism.
- The renderer replicates 0x819 exactly: (16,16), (8,8), (1,1).
- `DDR_SYS_ATTR=<hex>` diagnostic env overrides to an absolute value (clears all, sets that).

### 2.2 afpu_system_set_attributes

Also 2-ARG (a 1-arg call passed a garbage value that polluted the afputils global governing
shape/stream interpretation). The gamemdx boot function, disasm-verified, calls:

- `afpu_system_set_attributes(4, 0)` - CLEARS bit 2 (`xor edx,edx; lea ecx,[rdx+4]`). An earlier
  renderer version used (4,4) which SETS bit 2 - a real divergence from the game, though it did
  not affect the bg_0009 bg_line dip.
- `(8, 8)` and `(16, 16)` set bits 3 and 4.

`DDR_AFPU_ATTR=<hex>` clears all flags then sets that absolute value (diagnostic).

## 3. Package load (LoadIfs)

1. Mount the .ifs via avs imagefs at `/afp/packages` (AvsManager::MountIfs mounts BOTH `/data`
   as dir fs and `/afp/packages` as file imagefs). Loading a second package requires umounting
   both first. `afpu_ngp_read_data` COPIES clip defs into afputils, so a preloaded companion's
   clips stay registered after its mounts are gone (enough for cross-package refs to resolve).
2. `afpu_ngp_read_data(pkg_name, "/afp/packages", 0)` -> data_id (gamemdx pattern).
3. `afpu_do_create_stream_all(data_id, 1)` creates all streams - but creating streams only loads
   the animation DATA. To DISPLAY it, a LAYER must be instantiated from the package's root
   stream and added to the layer list afp_do_render/afp_do_display iterate (the gamemdx BM2D
   layer-attach function).
4. Preferred layer source: `afpu_get_afp_info_at_package(info, data_id, clip_name)` - it
   strcmp-matches the 3rd arg against clip names. `info+24` = stream id, `info+16` = path ptr.
5. `afp_layer_create_with_property(stream_id, path, 0, nullptr)` -> layer id; validate with
   `afp_id_is_valid(5, layer_id)` (>= 0 = valid).

### 3.1 afputils package table (direct memory layout)

The package table (in libafputils):

- table index = `(data_id >> 15) & 0xFF`; each slot is a pointer to a package record.
- record `+0x14` = clip count (u16).
- record `+0x50` = pointer to clip array, 40-byte entries:
  - clip `+16` = name ptr
  - clip `+32` = stream id
- record `+0x60` (96) = strdup'd package name (stored there by `afpu_new_package`). This
  offset backs the afp_hook `PkgName()` data_id -> "background_NNNN" resolver used against
  the live game.
- clip[0] is the root clip (normally `bg_root` for backgrounds).

### 3.2 Layer attributes and diagnostics

- gamemdx's BM2D layer-attach calls `afp_layer_set_attribute(layer, 0x200, 0x200)` on
  backgrounds. Default create flags are 0x1000001F and do NOT include 0x200; bit 0x200 gates a
  per-node transform step in the advance. It does NOT affect background_0009's bg_line
  "swipe" dip at frame ~360; its broader effect is unverified, so the renderer leaves it OFF
  by default (`DDR_LAYER_ATTR=0x200` to enable).
- The LIVE game (observed via afp_hook) makes one bg-setup call the renderer does not:
  `afp_layer_mc_refer(layer, "/")` right after create. Its prologue function only sets the
  "current layer" globals, but the "/" branch may realize the play tree.
  `DDR_MC_REFER=1` mirrors it.
- `DDR_ROOT_CLIP=<name>` overrides which package clip becomes the played layer (default
  clip[0] = bg_root) - lets a child clip (e.g. bg_line_a) play in isolation.
- `DDR_EXTRA_LAYERS=1` creates + plays a layer for every other clip in the package so the afp
  layer table holds many advancing layers, mimicking the real game's busy multi-layer scene
  (the real game advances priority groups 0,4,5,1,2,3 with dozens of layers; the renderer advances
  bg_root alone). Display stays bg_root-only, so it changes only the ADVANCE context.

## 4. Per-frame loop

Per-frame sequence mirrors the gamemdx per-frame render function:
`afp_render_init` (alloc pp pools + render state) -> `afp_do_render(dt, mode, 0)` (advance all
layers via the layer-advance function) -> `afp_render_finish` (free pp pools; render state persists for the
later do_display). Then `afp_do_display(5, layer_id)` draws the single active layer
(self-brackets its own pp pools).

- `DDR_RENDER_MODE=3` selects afp's full "system advance" mode 3, which allocs its own pp
  pools - render_init must be SKIPPED in that mode (double-alloc assert, afp-render.c:316).
- `DDR_RENDER_MODE=2` mirrors the live game: per-priority-group advance
  `afp_do_render(dt, 2, group)` over groups 0..7 every frame. `DDR_DISPLAY_MODE=2` similarly
  drives `afp_do_display(2, group)` over groups 0..7 (priority-sorted) instead of mode 5.
- `DDR_ADV_DT0=1` forces dt=0 on the advance (see 4.1 for why that premise was wrong).

### 4.1 The dt=0 hook artifact (important negative result)

A previous `DDR_FORCE_STEP` mechanism (priming the layer's force-advance byte +0x40 and
sub-frame accumulator +0x56 for a dt=0 step) was REMOVED. It was built on the premise that the
real game advances frame-locked with dt=0 - but that premise was a HOOK ARTIFACT: the afp_hook
logged `afp_do_render`'s dt as 0.0000 because it typed the argument `double` while afp_do_render
takes a `float` (it does `*(float*)&a1`). afp_do_render early-returns on dt <= 0, so the real game
does NOT pass 0 - it passes a real dt. Advance mode / dt is NOT the cause of the bg_0009 dip
.

`DDR_FRAME_LOCK` and `LayerStruct` are kept: FRAME_LOCK proved the dip sits at an INTEGER
subpos (~360), ruling out sub-frame interpolation.

### 4.2 Layer struct (libafp internals)

The afp layer table (indexed by group):

- group = `((layer_id >> 27) & 0xF) - 1` (0 or 1); index = `layer_id & 0xFFFF`; each slot is a
  pointer to the layer struct.
- struct `+28` holds the layer id (validation).
- struct `+40` (0x40 within the id-scheme comments = one byte) = force-advance byte; also the
  monotonic per-render-tick counter is exposed at `afp_layer_get_info` out[14].
- struct `+52` / `+56` = sub-frame time accumulators (floats). `DDR_FRAME_LOCK=1` zeroes them
  after the advance so `afp_do_display` renders the exact integer frame instead of
  interpolating the bg_line mesh at a fractional position.
- struct `+60` = subpos (from the first-time diagnostic log).

### 4.3 Play-work table (nested clip diagnostics)

`DDR_DUMP_CHILD=<clipname>` resolves a named child MC of bg_root and dumps its play-work struct
per captured frame. The play-work table is located via `get_index_from_mc_id` (afp-play-work.c;
re-find via its afp-play-work.c assert string), indexed by `(mc_id & 0xFFFF)`, validated by
`struct+348 == mc_id`. A refer'd mc has group nibble 4 (`(mc_id >> 27) & 0xF == 4`).

## 5. Playhead, labels, loop detection (afp_mc_get_param codes)

The authored loop lives on the MOVIE CLIP, not the layer or stream. `afp_mc_get_param` on the
root mc (obtained via `afp_layer_mc_refer(layer, "")`):

- `0x1010` = current timeline frame (drift-free clock). Wraps back to the "loop" label - the
  wrap IS the authored loop, pixel-exact for root-driven backgrounds
.
- `0x1011` = total frames (the wrap divisor, NOT the loop length).
- `0x1012` = resolve a named label to a frame; returns 0 when found (usage:
  `afp_mc_get_param(mc, 0x1012, name, &frame)`).
- `0x1013` = raw loop_count; stays 0 for DDR's gotoAndPlay idle loop, kept only as a
  diagnostic.
- `0x1020` (label by index) returns a numeric token, not text - query-by-name (0x1012) is the
  reliable path.

Every DDR bg root carries the four authored timeline labels `{in, loop, out, end}`
(RE-confirmed).

Things that are NOT the loop length (all proven wrong at some point):

- `afp_stream_get_info` out+2 is the AFP data-format-version word (ALWAYS 512), not a frame
  count. Only the stream bounds are usable: `+16` = width,
  `+18` = height (right-left / bottom-top).
- `afp_layer_get_info` out[14] (= layer struct +0x40) is a monotonic per-render-TICK counter:
  it advances 1 per render call, NOT per content frame (content advances via the float-dt
  accumulator in `afp_advance_play_data`), so it does not give the visible loop period.
- `DdrAfp::LoopFrames()` is legacy/dead: always returns 0, never populated.

`afp_layer_get_info(layer, info, 0)` useful words: info[0], info[6] (wrapmod), info[13]
(subpos), info[14] (tick counter) - used by `DDR_LOG_SUBPOS` (the hooked live game loops
subpos 10..132 for the reference background).

## 6. Playback control ops

- PAUSE / RESUME: `afp_layer_play(layer, rate)` - rate 0 freezes in place, 1 resumes 1x.
  NEVER use `afp_layer_stop` for pause: it is `afp_movie_work_free` - a full teardown +
  reinit-to-frame-0 (RE-confirmed).
- SEEK: `afp_mc_op(mc, 0xF08, frame)` = deep_goto_play - seek to absolute frame + resume,
  recurses into child clips. afp clamps the HIGH end (to total-1); the renderer floors the low
  end at 0. (0xF09 = deep_goto_play_label takes a name directly but resolves to the same frame
  internally.)
- CLIP SWITCH: afp 2.13.7 has NO afp_layer_destroy. The renderer keeps one layer per clip
  (bounded, no leak); parking = `afp_layer_stop` on the old layer (rate goes 0, it stops
  advancing) and re-pointing the displayed layer id, since `afp_do_display(5, id)` draws only
  the active layer. Replaying a cached layer = `afp_layer_stop` (rewinds) then
  `afp_layer_play(layer, 1.0f)`.
- Package clips are DDR's afplist equivalent (e.g. common_shutter's 00_cleared /
  shutter_clear / ...), enumerated from the afputils package table and exposed to the GUI
  Layers panel; SwitchClip is DDR's equivalent of the modern SwitchAnimation.

## 7. Shutdown

Intentionally does NOT call `afp_shutdown` / `afpu_shutdown`: AFP 2.13.7's teardown
access-violates in this standalone wiring (it tears down render/stream state the renderer does
not fully own). Shutdown happens only at process exit where the OS reclaims everything; the
verified `--ddr-test` harness exits cleanly precisely because it skips them. The cached root
stream id is zeroed so a torn-down background cannot report a stale [SIZE].

## 8. DdrRender - the D3D9 render backend (render-params callbacks)

The 2.13.7 analog of afp_d3d9 + the afpu_data struct: implements the named render-params
callback table against the renderer's own device, immediate-mode. Slot offsets in the
render-params struct were RE'd via `afp_set_render_params`' remap:

```
+0x00 config word (0x200)   +0x08 init_frame      +0x10 finish_frame
+0x18 set_mask              +0x20 set_blend       +0x28 set_priority
+0x30 set_filter            +0x38 draw_primitive  +0x40 draw_shape
+0x48 load_matrix (2x3)     +0x50 load_matrix44   +0x58 load_proj44
+0x60 get_screen_size       +0x68 get_near_far    +0x70 set_draw_rect
```

- Every unimplemented slot is filled with a safe no-op returning 0: AFP's bytecode executor
  calls the backend table by slot and `afp_set_render_params` reads the
  ura-render-params slots (a1[35..38]) directly; a NULL slot it reaches would fault. A
  0-returning stub matches afp's own default (`afp_get_ura_render_params` is itself
  `return 0`). Extra args land in registers and are ignored under x64.
- CAVEAT: `afp_set_render_params` substitutes ITS OWN DEFAULT for any NULL
  slot - so stubbing everything with a non-null no-op DEFEATS those defaults
  (property/font/get_bitmap_info/etc.). `DDR_DEFAULT_CB=1` re-nulls slots 0x78..0x110 so afp
  uses its defaults (diagnostic).
- afp-heap allocator callbacks are render_params[35..38]:
  `+0x118` heap ctx (unused), `+0x120` [36] malloc(ctx,size), `+0x128` [37]
  realloc(ctx,ptr,size), `+0x130` [38] free(ctx,ptr). The mapping is installed verbatim by
  the libafp heap-callback installer into afp's heap-callback slots; the realloc is
  reached whenever a block from the afp-heap alloc function is grown
  (e.g. `afp_set_stream_max_nr` growing the stream table
  8 -> 2048). The triple is malloc/realloc/free, NOT malloc/free/notify - that mistake frees
  the stream table inside the grow and crashes.

### 8.1 afpu config struct

gamemdx builds this static struct; afpu_boot/afpu_set_render_params installs
it as an afputils global. Layout RE'd from gamemdx's static struct + afputils'
afpu-heap reader functions (which read malloc@+32, realloc@+40, free@+48):

```
+0x00 create_texture(ctx, w, h, format_idx, a5, a6, a7) -> tex slot id
+0x08 destroy_texture(id)
+0x10 upload_texture(id, fmt, a3, a4, x, y, w, h, pixels)
+0x18 heap ctx     +0x20 malloc(ctx,size)
+0x28 realloc(ctx,ptr,size)   +0x30 free(ctx,ptr)
+0x38 near (float, 1.0)       +0x3C far (float, 9999.0)
```

### 8.2 Texture upload format codes (the gamemdx upload callback)

The upload callback signature + format codes mirror the game's own afpu upload callback:

- fmt 14 = 24-bit, byte order B,G,R (3 bytes/px)
- fmt 16 / 32 = 32-bit B,G,R,A
- fmt 31 = 16-bit RGB565
- fmt 22 = DXT1/BC1 (8 bytes per 4x4 block)
- fmt 26 = DXT5/BC3 (16 bytes per 4x4 block)

The game locks the destination sub-rect [x, y, x+w, y+h] and copies rows. The renderer creates
all textures as A8R8G8B8 (D3D9Ex rejects D3DPOOL_MANAGED - use DYNAMIC + DEFAULT so
LockRect-upload works) and CPU-decodes DXT into the locked rect (block decode itself: see
`docs/formats.md`). `draw_primitive`'s texture id is translated to the created slot via
`afpu_get_texture_bind_id` - exactly how gamemdx's draw callback maps it.

### 8.3 draw_primitive contract (the gamemdx draw_primitive callback)

`draw_primitive(vtx, count, params, ctx)`:

- params[0] = afp primitive type. Exact mapping (from the draw_primitive callback):
  1/3 = line list, 2 = line strip, 4 = tri list, 5 = tri strip, 6 = tri fan, else point list.
  (bg_0009 uses only type 4; line types matter for bg_line_* content.)
- params[1] = vertex format flags: &1 pos.xy, &2 pos.xyz, &4 per-vertex color, &8 uv,
  &0x10 a skipped 2-float field. Source component order per vertex: [uv][skip][color][pos].
- params[2] = afp texture id (0 = untextured shape fill).
- params[4..7] = RGBA modulate color (floats 0..1).
- params[8..11] = a SECOND color, "g_pixelColor" - gamemdx's pixel-color installer
  applies it as a pixel-shader tint ON TOP of the params[4..7] modulate. The renderer
  ignores it (a diagnostic logs any non-white pixcol).
- Per-vertex color is a float holding the packed D3DCOLOR (0xAARRGGBB); gamemdx recovers it
  with cvttss2si (float -> int truncation). The radiating bg_line meshes carry per-vertex
  colors (white AND dark blue); dropping vcol flattened them all to the white modulate.
  The pipeline color = texture x vertex_color x modulate (component-wise).
- Positions are screen-space with a -0.5 half-pixel bias (as gamemdx applies). afp emits final
  screen-pixel coords (the bg fill is the quad (0,0)-(1280,720)); it never calls load_proj44
  for these backgrounds, so the renderer uses XYZRHW vertices (rhw=1) and sets up the
  screen->clip ortho itself (equivalent to D3DXMatrixOrthoOffCenterLH(0,w,h,0,0,1)).

### 8.4 set_mask (game-faithful, stateless)

afp brackets a clip's content with set_mask calls: type 0 (MASK) begins the mask, type 1
(COLOR) the masked content, type 2 (END) finishes; (x,y,w,h) come from the node's computed
bbox. The real game's set_mask callback (gamemdx) is a STATELESS rect scissor:
type 1 clips to [x, y, x+w, y+h]; type 0/2 reset to full screen. No stack, no intersection - an
earlier intersecting-stack implementation ANDed every child with the bg_root bbox and cut
background_0009's rotating bundles. With system-attribute bit 0 set
(section 2.1) afp emits these per node, so each clip is bounded by its OWN bbox.

Between type 0 and type 1, afp draws the mask SHAPE as a white quad. On the real game that goes
to the stencil/mask buffer (invisible to color); the immediate-mode path has no such redirect,
so color draws are skipped while the mask is being written (`g_in_mask_write` honoured in the
draw callback). `DDR_NO_MASK=1` ignores masks entirely (diagnostic; unclips bg_0001's trail).

A mask/filter state is force-reset at the start of every frame as a defence against any
unbalanced MASK/COLOR/END bracket across frames (afp resets masks per layer subtree).

### 8.5 set_blend (shared bm2dx enum)

The afp blend-mode enum is shared across afp versions; the mapping is RE'd from the bm2dx
blend-mode mapping function (mirrored by the modern AfpD3D9::SetLayer):

- 0 = normal alpha (SRCALPHA / INVSRCALPHA, ADD) - also the default
- 3 = multiply (ZERO / SRCCOLOR, ADD)
- 4, 8, 0x4F = additive (SRCALPHA / ONE, ADD). bg_0028's sparkling stars use mode 8: the
  sprite is a star on black, additive drops the black. Falling mode 8 through to straight
  alpha is exactly what produced black boxes around the stars.
- 5 = max / lighten (SRCALPHA / ONE, MAX)
- 6 = min / darken (SRCALPHA / ONE, MIN)
- 9, 0x46 = subtractive (SRCALPHA / ONE, REVSUBTRACT)

The alpha channel uses separate-alpha blending ONE/ONE with BLENDOP MAX (coverage) so
overlapping/additive draws do not wash out the exported alpha.

### 8.6 set_filter - the HSL color filter (id 100/101)

For filter id 100 or 101 the payload a3 is a float*: a3[1] = hue (degrees), a3[2] = saturation
(percent), a3[3] = "value" (percent); gamemdx's filter-payload builder normalises hue/=360,
sat/=100, value/=100, so they are signed OFFSETS. en (a2) toggles the filter: afp brackets each
recoloured draw with en=1 ... en=0. Stubbing this callback left every bg_0009 line white (the
line textures are white; the HSL adjust of the white draw color IS the line's final color).

The third param is HSL LIGHTNESS, not HSV value - exact to the byte: white + bg_0009's
[hue -136, sat 38, value -69] (dh=-0.378, ds=+0.38, dl=-0.69) -> HSL(224, 0.38, 0.31) ->
#31416d, the exact real game pixel. HSV with the same numbers gives #31394f (the bug this
replaced). Implementation: rgb -> HSL, hue += dh (wraps), sat += ds (clamp), lightness += dl
(clamp), HSL -> rgb; alpha preserved.

### 8.7 set_draw_rect

A genuine no-op in gamemdx: its render-params template shares one no-op stub
between set_priority and set_draw_rect. afp still passes a rect, the game ignores it; clipping
is done via set_mask. The renderer ignores it too.

### 8.8 get_near_far

Returns near=1.0 / far=10000.0 by default; `DDR_NEARFAR=near,far` overrides (was a diagnostic
for whether bg_line was 3D-projected; call count is logged to confirm afp uses depth at all).

### 8.9 D3D9 gotchas encoded here

- `SetTransform` must never receive a NULL matrix - D3D9 dereferences it and faults inside
  d3d9.dll. Use identity until afp supplies one.
- The renderer's shared BeginFrame binds the MODERN afp vertex/pixel shaders (g_afp_vs with its
  own ortho in c0..c3); the DDR path drives fixed-function, so init_frame clears both shaders
  (otherwise SetTransform is ignored and the stale shader's wrong-sized ortho scales
  everything) and pins the full viewport.

### 8.10 Render diagnostics (env vars)

All DDR_* debug switches, kept for future dip-hunting:

- `DDR_DUMP_FRAME=N` - per-draw logging (bbox, uv, blend, vcol, truncation) on frame N; frame
  summary lines for +-60 frames around it.
- `DDR_DUMP_DRAW=N` - on the dump frame, log every vertex of the Nth draw.
- `DDR_DUMP_TEX=1` - on the dump frame, save every live atlas to screenshots/atlas_NN.png via
  dynamically-resolved D3DXSaveTextureToFileA (tries d3dx9_43 down to d3dx9_30).
- `DDR_ONLY_DRAW=N` / `DDR_DRAW_MIN` / `DDR_DRAW_MAX` - render only selected draws per frame
  (bisect which draw covers/retracts geometry).
- `DDR_TRACK_BARS=1` - log geometry of the tall center bar draws (vertex count in [90,99]):
  first body quad corners + width/length/angle of the width edge, to separate rotation from
  uniform scale during the bg_0009 pinch (tri-list corners are buf[0..2] and buf[5]).
- `DDR_LINE_ALPHA=<f>` - force the bpls2_line strip draws (atlas v in [0.105, 0.120]) to a given
  alpha. The real game draws these via draw_shape with VARYING alpha (some ~0.298); faint white
  over the bg reads as dark blue. Diagnostic, not a fix.

## 9. --ddr-test smoke harness (afp_ddr_test.cpp)

Self-contained end-to-end test: loads ONLY DDR's libavs/libafp/libafputils, boots AVS + AFP,
decompresses a given .arc to its inner .ifs (`DdrArc::ExtractFirstIfs`), loads + plays it,
renders N frames at 1280x720 (the DDR bg authoring size) and writes the final frame to PNG.
Mirrors how the game wires AVS+AFP - no game DLL involved. Return codes identify the failed
stage (2 = DLL load, 3 = avs resolve, 4 = AVS boot, 5 = D3D init, 6 = AFP boot, 8 = LoadIfs).

- Installs a vectored exception handler that logs faulting address, module and module-relative
  offset (mappable straight to an IDA address) plus the bad data pointer.
- `SetDllDirectoryA(modules_dir)` so libafp's imports (libavs + d3dx9_43) resolve.
- `DDR_PRELOAD_ARC=<path>` loads a companion/common package FIRST (the live game loads
  common_background_v3.ifs alongside each background) so a scene IFS's refs to persistent
  common clips resolve. Best effort.
- `DDR_CAPTURE_FROM/TO=a,b` dumps every frame in [a,b] to `<outdir>/seq_%04d.png` in one boot.
- `DDR_SELFDIFF=1` (+ `_BASE`, `_PERIOD`): one-run visible-loop scan - snapshot a baseline
  frame, then MAD-compare frames at every period multiple; a true visible loop is the smallest
  multiple of the afp authored period whose MAD is ~0 (the root only realigns at multiples of
  its period). NOTE: this is a legacy pixel-based diagnostic; the sanctioned loop signal is afp
  state (section 5).
- Screenshots are queued so they land after EndFrame's offscreen -> backbuffer blit, before
  Present.

## 10. Time scale

`DdrAfp::SetTimeScale(s)` multiplies the per-frame dt fed to afp (GameProfile::time_scale).
This is a CALIBRATION for an unresolved ~6 percent rate divergence vs the real game. Set once at
boot; 1.0 = real-time. `DDR_TIME_SCALE` env overrides.

## 11. Batch .arc extractor (arc_extract.*)

Standalone utility, no game boot needed (depends only on DdrArc + the standard library):
recursively finds every DDR World .arc under a folder and unpacks each entry (decompressing
AVS-LZ77 as needed - codec in `docs/formats.md`) into a sibling `<folder>_extracted` directory,
mirroring the source tree with each archive's contents grouped in a folder named after it
(so multi-entry archives never collide with neighbours).

Design points:

- Detached worker thread; progress published into a mutex-guarded Status the GUI polls each
  frame. `Start` is guarded by an atomic compare-exchange so double-starts are no-ops.
- Two passes: pass 1 enumerates .arc files for an accurate total, reporting a live counter +
  the current path every 512 walked entries (total unknown -> GUI shows an indeterminate
  "Scanning..." bar that never looks frozen); pass 2 unpacks with a determinate bar.
- A 0-byte entry is legitimately empty; a non-zero-size entry decompressing to nothing is a
  decode failure and is skipped.
- Output name collisions within an archive get `_2`, `_3`, ... suffixes.

## 12. DDR customize image extractor (customize_extract.*)

Works directly on RAW game data (e.g. `data/arc/custom`): unpacks each customize .arc itself
(each holds a single PNG; the .png entry is found by extension, falling back to the only
entry), renames by id and losslessly re-optimises into a `customize_assets` tree.

Filename mapping (the .arc stem -> category dir / output name; `<id>` has leading zeros
stripped, so appeal_board_0041_result -> 41.png):

```
appeal_board_<id>_result.arc  -> appeal_boards/<id>.png        (the _result variant)
character_<id>_1p.arc         -> characters/<id>_left.png      (base, NOT _result)
character_<id>_2p.arc         -> characters/<id>_right.png
lane_single_<id>.arc          -> lane_backgrounds_sp/<id>.png
lane_double_<id>.arc          -> lane_backgrounds_dp/<id>.png
lane_cover_single_<id>.arc    -> lane_covers_sp/<id>.png
lane_cover_double_<id>.arc    -> lane_covers_dp/<id>.png
```

Lossless PNG minify: keeps the 4 critical chunks (IHDR/PLTE/IDAT/IEND) plus tRNS (the one
ancillary chunk that carries pixel transparency for palette/grayscale); drops every other
ancillary chunk (cHRM, bKGD, gAMA, sRGB, iCCP, pHYs, tEXt, zTXt, iTXt, tIME, ...); merges the
split IDAT chunks into one. The compressed pixel stream is byte-identical (no re-deflate), so
output is always <= input with zero quality loss - the game's own deflate is already optimal,
the win is per-chunk overhead. Decoders are required to ignore absent ancillary chunks. The
PNG chunk CRC covers type + data. Same worker-thread + two-pass-progress pattern as the .arc
extractor; the scanner never descends into its own `customize_assets` output.
