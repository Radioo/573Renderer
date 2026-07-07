# D3D9 render backend: RE facts and design rationale

Knowledge captured from `src/render_backend.{h,cpp}`, `src/afp_d3d9.cpp`,
`src/afp_d3d9_internal.h`, `src/afp_d3d9_callbacks.cpp`,
`src/afp_d3d9_textures.cpp`, `src/afp_anim.cpp`, `src/afp_packages.cpp`
ahead of the no-comments migration. Texture format ids and
DXT decoding are documented in `docs/formats.md`.

## Module layout

The AfpD3D9 backend is ONE tightly-coupled module split across three TUs only
to satisfy the 1000-line-per-file rule: `afp_d3d9.cpp` (lifecycle + default
state), `afp_d3d9_callbacks.cpp` (draw/state callbacks),
`afp_d3d9_textures.cpp` (texture management). Shared file-static state lives
in `afp_d3d9_internal.h` as C++17 inline variables so exactly one instance
exists regardless of how many TUs include it. That header is NOT public API;
external callers use `render_backend.h`.

## Device and offscreen setup

- The game uses `Direct3DCreate9Ex`; the renderer does too, falling back to
  plain `Direct3DCreate9` if Ex fails. `D3DCREATE_HARDWARE_VERTEXPROCESSING |
  D3DCREATE_MULTITHREADED`, falling back to software VP.
- Present parameters: windowed, `D3DSWAPEFFECT_DISCARD`, backbuffer format
  explicitly `D3DFMT_X8R8G8B8` (so the offscreen RT can pair with it),
  `D3DPRESENT_INTERVAL_IMMEDIATE` (the main loop self-paces to 120 Hz via
  sleep_until; vsync would fight that and lock to monitor refresh),
  `EnableAutoDepthStencil = FALSE` (we attach our own DS to the offscreen RT,
  not the swap chain).
- Offscreen render-target architecture mirrors bm2dx (see
  the bm2dx D3D9 device setup):
  - `backbuffer` = the swap chain's back buffer (what Present shows).
  - `offscreen_rt` = dedicated RT the ENTIRE afp frame is drawn into,
    `D3DFMT_A8R8G8B8` and lockable (export readback needs it). A8R8G8B8 so a
    real alpha channel survives for export; the XRGB backbuffer drops alpha
    during the StretchRect, which only affects the on-screen preview.
  - `depth_stencil` = dedicated `D3DFMT_D24S8` surface attached while drawing
    offscreen. The 8-bit stencil matters: afp's SetMaskRegion uses the
    stencil buffer in the game; without a real stencil the mask system is a
    no-op.
  - Neither surface is multisampled. The GAME runs without MSAA: verified in
    soundvoltex.dll's render-target creation function, where every
    CreateRenderTarget /
    CreateDepthStencilSurface call passes `D3DMULTISAMPLE_NONE`. The visual
    smoothing the real game shows comes from texture filtering (see sampler
    section), NOT polygon-edge AA. Adding MSAA would diverge from the game's
    pipeline.
- Per-frame sequence, matching `bm2dx_RenderFrame` exactly:
  `BeginScene -> SetRenderTarget(offscreen) -> SetDepthStencilSurface ->
  Clear(TARGET|ZBUFFER|STENCIL, clear_color, 1.0f, 0)` (bm2dx's
  vtable[+344] Clear call uses flags = 7) `... afp draws ...
  SetRenderTarget(backbuffer) -> StretchRect(offscreen -> backbuffer,
  D3DTEXF_LINEAR)` (bm2dx uses LINEAR here) `-> EndScene -> Present`.
  Clearing STENCIL every frame is mandatory: the game's SetMaskRegion mask
  ops (0/1/2/3, dispatched against D3DRS_STENCILENABLE) write stencil values
  that would otherwise bleed across frames and clip later scenes ("residue"
  bugs).
- Clear color is stored as a premultiplied-BGRA D3DCOLOR, default
  `ARGB(0,0,0,0)` fully transparent so exports carry real alpha; the Export
  module flips it to an opaque user color for solid backgrounds and back on
  session end. The preview looks the same either way (StretchRect to XRGB
  drops alpha, showing black).
- Screenshot timing contract: the backbuffer snapshot must happen AFTER the
  offscreen->backbuffer StretchRect and BEFORE Present. On a D3D9Ex DISCARD
  chain, capturing after Present reads an already-swapped, undefined frame.
  The offscreen RGBA save (`SaveOffscreenRGBAToPNG`) must run BEFORE
  EndFrame's StretchRect for the same reason (and because only the offscreen
  keeps alpha). `D3DXSaveSurfaceToFileA` writes A8R8G8B8 alpha into the PNG
  alpha channel. `D3DXIFF_PNG = 3` (enum value from d3dx9tex.h). All D3DX9
  entry points are resolved dynamically by probing d3dx9_43.dll down to
  d3dx9_24.dll.
- CPU readback (`ReadOffscreenBGRA`): D3D9's canonical RT readback is
  `GetRenderTargetData` into a `D3DPOOL_SYSTEMMEM` offscreen-plain surface of
  the exact same size/format, then LockRect. Locking the RT directly is not
  allowed without D3DUSAGE_DYNAMIC (undesirable on an RT). Driver pitch may
  exceed width*4 (some align rows to 256 bytes), so rows are repacked to a
  tight stride. The sysmem scratch surface is cached across calls (the export
  loop calls per frame) and must be released before the device in Shutdown.
  D3DFMT_A8R8G8B8 in little-endian memory is byte order B,G,R,A.
- Crop overlay: drawn on the backbuffer after StretchRect, using
  pre-transformed `D3DFVF_XYZRHW | D3DFVF_DIFFUSE` vertices (no matrices).
  D3D9 LINELIST is always 1px (no line-width state), so the 2px border is
  four thin quads. State hygiene: sets what it needs and restores nothing,
  because afp's BeginRender reinstalls the full default state next frame and
  the overlay is the last thing drawn before Present.

## AFP vertex format and vertex shader

- AFP vertex = 24 bytes: `float3 pos, DWORD ARGB diffuse, float2 uv` =
  `D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1`. AFP subtracts 0.5 from each
  position component (see half-pixel section).
- The vertex shader is the EXACT source of the game's shader created by
  bm2dx.dll's vertex-shader creation function:

      vs.1.1
      dcl_position v0
      dcl_color v1
      dcl_texcoord0 v2
      m4x4 oPos, v0, c0
      mul oD0, v1, c4
      mov oT0.xy, v2

  c0..c3 = transposed layer matrix, c4 = per-shape RGBA color multiplier.

## AfpRenderContext (the 128 KB render ctx)

One 0x20000-byte allocation passed to `afp_boot()` / `afpu_render_init()`
plays two roles, mirroring the game's wiring:

1. The first ~200 bytes are the render callback table (function pointers at
   offsets 0x008..0x068 and 0x118..0x130). afp-core and afp-utils read these
   to hook BeginFrame / EndFrame / SubmitGeometry / LayerCommand / etc. See
   the bm2dx callback-table registration.
2. The same pointer is handed back as the "state ctx" during LayerCommand
   case 4. bm2dx stores its D3D device INSIDE the ctx at +0x18000 (aliased
   with a bm2dx global); AFPU vtable methods may read the
   device back out of that offset, so real storage must exist there and must
   be kept in sync with the device pointer passed by argument.

bm2dx's actual struct is 98304 bytes (0x18000); the renderer rounds up to
0x20000 for headroom past +0x18000.

## Render callback table: per-callback contract

Slot numbers refer to afp-core's 35-slot internal callback table; +0xNN
offsets are within the render ctx table.

- slot 0 `BeginRender`: per frame. Calls afpu's `set_screen_rect(int[4]{0,0,
  w,h})` EVERY frame; that function lives at a DIFFERENT afpu offset per
  version (IIDX afp-utils 1.2.19 = +0x18550, SDVX 1.2.26 = +0x199B0), set
  per game profile; calling the wrong offset on SDVX lands inside afpu's
  matrix-transform helper and access-violates. Then installs the default
  render states (see below), resets the mask-write latch, disables scissor,
  binds the fallback texture.
- slot 1 `EndRender`: reinstalls default render states so the next frame
  starts clean regardless of PushLayer/SetBlend/SetMaskRegion. bm2dx runs its
  default-state install function at BOTH BeginFrame and EndFrame.
  Diagnostic counters read from afp-utils globals: afpu's `afpu_render_info`
  shows "PRIMITIVE (A+B) -> drawn" where A and B are two afp-utils shape-push
  counters and drawn is its drawn-shape counter. A+B = shapes the AFPU scene
  walker pushed
  through draw_shape dispatch; the gap between (A+B) and drawn is filtered
  INSIDE AFPU, not in our SubmitGeometry/LayerCommand (verified: our reject
  counters stay 0).
- slot 3 = ctx +0x028 `SetLayer(blend_mode, zero, hsv_desc)`: afp-core's
  HSV-filter apply callback. See blend-mode and HSV sections. Fires 1:1
  immediately before each filtered draw (confirmed), so latch-then-consume
  in SubmitGeometry is exact.
  - hsv_desc sentinel: when NO filter is active, afp passes its
    `g_afp_cur_hsv_filter` global (set to -1 by afp-core's render-begin
    function) which surfaces as a non-canonical high
    pointer (~0x7ffffffffffffffc), NOT null. A plain null check lets it
    through and the deref faults; the AV was swallowed by the render SEH
    wrapper, whose unwind skipped afp-core's matrix-stack pop function
    and stranded the world-matrix index at +2, blanking
    every subsequently-extracted qpro (effect bgs `qp_*_bg` fire/ring
    trigger it). Treat any pointer >= 0x800000000000 as "no descriptor",
    as the game's own backend does.
  - The afp reuses ONE HSV work struct and mutates it in place per node, so
    by draw time `*hsv_desc` holds the LAST node's values. SetLayer
    snapshots the 16-byte descriptor VALUE at call time to recover the
    FIRST node's filter (the left-hand ring hue that the merge otherwise
    drops).
- slot 4 `DrawPrimitive(prim_info)`: afp-core slot +0x028; afpu mediates it
  to afpu's set_screen_rect helper. Intentionally empty: geometry actually
  reaches D3D9 via
  SubmitGeometry / LayerCommand.
- slot 5 `SetBlend(blend_mode, flags, extra)`: second, simpler blend enum
  (see blend table B). Blend mode is NOT carried per-draw; it sticks from
  the most recent SetBlend.
- slot 6 `SetMask`: unused; masking is handled by SetMaskRegion (below).
- ctx +0x038 `SubmitGeometry(a1, vert_count, geo_data, tex_ptr)`: the
  draw_vg path; see decode pipeline section.
- ctx +0x040 `LayerCommand(cmd, a2, a3, a4)`: NOT a SubmitGeometry variant.
  Matches `bm2dx_AfpCB_LayerCommand`. Bit 0 of
  `cmd` is an enable flag (clear = whole command is a no-op); sub-command =
  `(cmd >> 1) & 0x3FF`, cases 0..5:
  - 0 DrawLines (bm2dx's line-list drawer), 1 DrawLineStrip
    (bm2dx's line-strip drawer): bm2dx-specific custom
    primitives using bm2dx's own
    shader state; stubbed (scream-once diagnostic). AFPU titles observed so
    far do not emit them.
  - 2 DrawTextured (`bm2dx_LayerCmd2_DrawTextured`): uses bm2dx's own
    TexIdMap + VS constants; stubbed. If draws get dropped here, port it
    against the g_textures slot table.
  - 3 nested dispatch: call `(*a4)->vtbl[+176](*a4, &cmd, a2, a3, a4)`.
  - 4 THE TEXTURE-BIND PATH: the ONLY place AFPU receives a device pointer,
    therefore how package-atlas textures actually bind and draw. `a4` is an
    AFPU-owned effect object; call vtbl[+168](a4, state_ctx, device, &cmd,
    a2, a3), then vtbl[+176](a4, state_ctx, device), then vtbl[+184](a4,
    state_ctx, device). The state ctx is the AfpRenderContext pointer
    (device mirrored at +0x18000). Without case 4, package textures never
    appear on screen.
  - 5: `a4` is a (fn_ptr, this) pair; call `a4[0](a4[1], &cmd, a2, a3)`.
  - bm2dx reissues its pixel/vertex shaders after the dispatch; the
    renderer skips that because its shader state is simpler and is
    reasserted in BeginRender.
- ctx +0x018 `SetMaskRegion(mode, layer, x, y, w, h)`: mask/clip command
  from the afp-utils command buffer. `mode` is the afp-core set_mask OP
  (`afp_backend_call_set_mask` @ afp-core). Three phases per
  clip-mask node, gated by `node+0x24 & 0x60000000` in
  `afp_play_work_draw_sub` @ afp-core:
  - op 0 BEGIN: the matte SHAPE is about to be drawn. Latch mask-write so
    SubmitGeometry skips the silhouette quads; keep scissor full.
  - op 1 USE: matte done, masked CONTENT follows. Clear the latch and
    scissor content to the mask node's bbox [x, y, x+w, y+h].
  - op 2 END: pop the clip; disable scissor.
  Stateless, exactly like the DDR path's Cb_SetMask.
- ctx +0x050 `SetShapeMatrix(mat2d)`: called before each draw_vg with the
  shape's mat2d (afp-core's shape-matrix notify call) and after
  with identity (the identity notify call). AFP applies the
  mat2d to vertex data INTERNALLY before
  calling SubmitGeometry - this callback is a NOTIFICATION only; applying
  the transform again double-transforms the vertices. Correct
  implementation: no-op.
- ctx +0x058 `SetMatrix` / PushMatrix (bm2dx table +88): reference is
  `bm2dx_AfpCB_PushMatrix`, which does exactly two
  things: non-null matrix -> `D3DXMatrixTranspose` into the layer-matrix
  slot at ctx+0x184C8; null -> rebuild the default ortho via bm2dx's
  default-ortho builder. It does NOT decompose to 2D and does
  NOT upload to the shader; the upload to c0..c3 happens per-draw inside
  `bm2dx_DrawPrimitiveFromGeo` (see the layer matrix flow section). History:
  an earlier
  implementation decomposed the matrix as a 2x3 affine, which broke
  flags=0xA 3D sub-clips (fcombo post-frame-60 FULL COMBO reveal): draws
  were submitted but transformed to a bogus clip-space location and nothing
  rasterized. Mirroring bm2dx (plain transpose) fixed it. AFP hands over
  the FINAL world*projection matrix.
- slot 11 `InvalidateBlend`: no-op stub (reset blend state).
- slot 12 `GetScreenSize(x, y, w, h)`: returns (0, 0, screen_w, screen_h).
- slot 13 `GetNearFar`: returns near 0.01, far 1000.0.
- slot 14 `FindTexture(name, data)`: afp calls this during stream creation
  to find external bitmaps for shapes (name is a shape name like
  "1p_fullcombo_shape5"). Returning 0 = "not found"; those shapes lose
  their geo data.

## Default render states (InstallDefaultRenderStates)

Exact match to bm2dx's default-state install function, run at
BOTH BeginRender and EndRender (gap analysis item 14). Render states from the
game disassembly:
ZENABLE=FALSE, ZWRITEENABLE=FALSE, CULLMODE=NONE, ALPHABLENDENABLE=TRUE,
LIGHTING=FALSE, SRGBWRITEENABLE=FALSE, ALPHATESTENABLE=FALSE,
FOGENABLE=FALSE, SPECULARENABLE=FALSE, COLORVERTEX=TRUE, CLIPPING=TRUE,
SEPARATEALPHABLENDENABLE=TRUE, SRCBLEND=SRCALPHA, DESTBLEND=INVSRCALPHA,
STENCILENABLE=FALSE, STENCILWRITEMASK=0xFF, STENCILMASK=0xFF,
STENCILFAIL/ZFAIL=KEEP, SCISSORTESTENABLE=FALSE (undoes SetMaskRegion op 1).
Texture stage 0: COLOROP=MODULATE(TEXTURE, DIFFUSE),
ALPHAOP=MODULATE(TEXTURE, DIFFUSE); stage 1 disabled, texture 1 null.

Alpha-channel blend (verified against the IDA decomp of the blend-mode apply
function in bm2dx.dll): EVERY blend-mode case uses
`BLENDOPALPHA = 5 (MAX)`, `SRCBLENDALPHA = ONE`, `DESTBLENDALPHA = ONE`.
Earlier renderer code used BLENDOPALPHA=SUBTRACT(2) + SRCBLENDALPHA=SRCALPHA,
which corrupts the alpha plane on every draw and was the root cause of the
"ghost rectangle on non-black background" artifact. MAX preserves per-pixel
coverage: overlapping alpha-blended sprites reach max(src.a, dst.a) instead
of saturating to 255 via addition, so consumers compositing the exported
AVIF do not wash out rectangle edges.

### Sampler state: game-verified

The static-RE read of soundvoltex.dll's sampler-state install function
suggested
MIP/MIN=ANISOTROPIC, MAG=LINEAR, MAXANISOTROPY=16, ADDRESSU/V=CLAMP. That
was partially a mis-RE. The values below are verified empirically on the
SDVX 7 NABLA game by reading per-draw sampler state during BG3 booth tile
rendering:

    MAGFILTER     = LINEAR      (2)
    MINFILTER     = ANISOTROPIC (3)
    MIPFILTER     = NONE        (0)  (no mip chain exists, so MIP=NONE;
                                      not ANISOTROPIC)
    MAXANISOTROPY = default (1)      (never raised; 16 was wrong)
    ADDRESSU      = WRAP        (1)  (not CLAMP - see below)
    ADDRESSV      = WRAP        (1)

WRAP is the seam-prevention key: AFPU's UV authoring already insets each
sub-image's UVs by half a texel (via the imgrect/uvrect delta in
texturelist.xml), so a WRAP-mode bilinear tap at the inset UV stays INSIDE
the sub-image and never actually wraps. CLAMP clamps to the placement edge,
which IS where the next packed sub-image lives in the atlas - hence the
cross-sub-image bleed the old CLAMP + no-inset state produced.

Captured entry (BG3 booth tile draw, sub-image join at screen y=1046.5):

    [DrawUP #39 / draw_seq=1327] prim=4 count=2 stride=24 vc=6
      bounds=[(164.500,828.500)..(866.500,1046.500)]
      stage0: tex=... MAG=2 MIN=3 MIP=0 ADDR_U=1 ADDR_V=1 MAXANISO=0
      v[0]: pos=(164.5000, 828.5000) uv=(0.00049, 0.85400)
      v[5]: pos=(866.5000, 1046.5000) uv=(0.34326, 0.96045)

Note positions at integer+0.5 (D3D9 half-pixel rule) and uv = 0.5/1024 =
0.00049 (texel-center inset baked into AFPU's uvrect at load time).

The texturelist.xml `mag_filter`/`min_filter` strings are a red herring for
sampler state: the real game does NOT honour them as D3D9 sampler settings.
(The per-texture filter queue plumbing still exists - see the atlas filter
FIFO section - but the SubmitGeometry consumer is disabled.)

Both sampler stages 0 and 1 get the same install so cross-stage state bleed
cannot reintroduce CLAMP/aniso-mip artifacts on multi-texture draws (e.g.
the 2-texture `lrp` shader at a1[3] set up by soundvoltex.dll's two-texture
shader install function).

The game's only "smoothing" is this texture filtering (LINEAR mag on scaled
BGs like BG3's 1.5x upsample and rotated sprites); no MSAA anywhere.

## Layer matrix flow

- Storage mirrors bm2dx's ctx+0x184C8 (the bm2dx layer-matrix global): 16
  floats, transposed for the VS `m4x4 oPos, v0, c0` row-dot layout.
- Default matrix = D3DXMatrixOrthoOffCenterRH(l=0, r=w, b=h, t=0, zn=-1,
  zf=1) then transposed, matching bm2dx's default-ortho builder
  exactly (zf=1.0 and zn=-1.0 in bm2dx, confirmed in IDA).
  Transposed result:

      row 0: [ 2/w,   0,    0,   -1  ]
      row 1: [ 0,   -2/h,   0,    1  ]
      row 2: [ 0,     0,  -0.5,  0.5 ]
      row 3: [ 0,     0,    0,    1  ]

- bm2dx only rebuilds the ortho on its boot/init path; the renderer ALSO
  resets it in every InstallDefaultRenderStates to guard against stale
  matrices after scene transitions. This is safe because the authoritative
  upload is per-draw.
- Per-draw upload: SubmitGeometry issues `SetVertexShaderConstantF(0,
  matrix, 4)` immediately before DrawPrimitiveUP, mirroring
  `bm2dx_DrawPrimitiveFromGeo` (its vtable[+752]
  call). Rationale: many draws can occur between matrix updates, and other
  state paths (InstallDefaultRenderStates at EndFrame etc.) stomp c0..c3;
  per-draw upload keeps the constant in lockstep. SetMatrix additionally
  uploads immediately so LayerCommand case-4 draws (AFPU's own vtable draws,
  which bypass our SubmitGeometry) still get the freshest transform.
- VS c4 default = (1,1,1,1); SubmitGeometry overwrites it per shape.
- Qpro hand shift: a horizontal render offset (SetHandRenderShift, 0.30 *
  screen width when on) is applied by nudging clip-space x-translation
  (matrix row0.w += 2*offset/w) in both BuildDefaultMatrix and SetMatrix.
  Purpose: a wide held item (e.g. qp_iris_hand's umbrella, quad running to
  x=-75) would clip off the LEFT edge; shifting right + re-anchoring the
  crop at the content's true left means the RIGHT gets cut like the game.
  Inert (0) for all other paths.

## SubmitGeometry decode pipeline

Order of operations: reject -> mask-skip -> texture bind -> shape color ->
vertex build (flag decode + half-pixel snap) -> UV inset -> prim map ->
matrix upload -> HSV/additive shader selection -> DrawPrimitiveUP -> shader
reset.

- Rejects: null device/geo_data; vert_count <= 0 or > 10000; prim_count <= 0
  after decode. Each has a diagnostic counter; expected identity per frame:
  `shapes - draws == rejected_device + rejected_vc + zero_prim +
  layer_cmd_calls - case4_calls` (case 4 may still produce visible output).
- Mask-write suppression: while the g_in_mask_write latch is set (between
  set_mask op 0 and op 1), the incoming geometry is the clip-mask matte
  SILHOUETTE. The real game routes it to the clip/coverage path and NEVER
  paints it to color - afp-core forces the matte's color to RGBA(0,0,0,0)
  (matte-color zeroing site).
  This backend has no stencil matte, so it must SKIP
  those draws entirely or they paint an opaque black shape (the qp_27eagle2
  qp_head_b black silhouette bug). Driven purely by the afp set_mask op
  stream (mask_type baked in node+0x24 top bits), so a normal visible back
  layer (mask_type 0, e.g. qp_rupika_head's qp_head_b) is never bracketed.
  No id/name hardcoding. The latch is also reset every BeginRender: if a
  degenerate subtree never emits set_mask(op 2), a latched frame would
  otherwise blank everything (mirrors the DDR path's per-frame reset).
- geo_data layout (DWORD indexed): geo[0] = prim_type, geo[1] = vertex
  flags, geo[2] = tex_ref, geo[4..7] = RGBA float shape color (uploaded to
  VS c4; e.g. 1.0, 1.0, 1.0, 0.204). a1 = the packed vertex stream.

### tex_ref encoding

Following what bm2dx does (see afp-utils.dll's texture-record resolver):

- `tex_ref == 0`: untextured solid-color fill. Stage 0 is fixed-function
  MODULATE(texture, diffuse), so a WHITE texture must be bound for the
  diffuse to pass through. Binding null makes the driver sample (0,0,0,0)
  and every solid fill becomes an opaque BLACK box (that was the
  qp_lane_v_*_bg bug: white note-hit explosion flashes rendered as black
  rectangles). A 2x2 all-white fallback texture is bound instead; being
  uniformly white, out-of-range fill UVs still sample white.
- `(tex_ref & 0x78000000) == 0x08000000`: valid AFPU tex_id, encoded as
  `tag(bit 27) | (pkg_idx << 15) | slot_in_pkg` (pkg_idx read as
  `(tex_ref >> 15) & 0xFF`, slot as `tex_ref & 0x7FFF`). The CORRECT bind
  path is to ask afp-utils to resolve the 32-byte texture record for the
  tex_id and read `record[+4]`, which is whatever value OUR TexCreate
  returned at package-load time (= the g_textures slot index). This is
  exactly what bm2dx does via `afpuloc_get_texture_data_size` - misnamed in
  the API; it is the texture-record resolver, not a size query (afp-utils).
  afp-utils exports are name-obfuscated (e.g. `XE592acd000042` for ordinal
  0x42), so GetProcAddress by readable name fails; the resolved pointer is
  plumbed in from AfpuFuncs (SetAfpuTexSlotResolver). Resolver returns 0 if
  the package is gone or the slot was never populated -> fallback texture.
- Any other non-zero tex_ref: legacy direct-slot encoding, `(tex_ref &
  0xFFFF) + 1` selects a g_textures slot directly. Not used once boot IFSes
  are loaded (everything goes through the AFPU encoding); kept for
  hand-crafted tests and the external-image path.

`ResolveTexture` mirrors the same encoding for out-of-band consumers
(offscreen inspection / capture).

### Vertex flag decode

The packed source stream `a1` is consumed in this per-vertex field order,
gated by geo[1] flag bits:

    0x08: u, v          (2 floats; else UVs come from geo+4 uv_data)
    0x10: skip 2 floats
    0x04: color         (1 DWORD ARGB)
    0x02: x, y, z       (3 floats)
    else if 0x01: x, y  (2 floats, z = 0)

Vertex cap 256 per draw. Default color 0xFFFFFFFF.

### Half-pixel grid snap

Discovered via VertProbe: OUR AFPU hands vertex coords inconsistently -
some integer-aligned (x=540.0, y=777.0, y=1047.0, y=1317.0), others already
at integer+0.5 (y=568.5, y=871.5, x=1079.5). The real game probe shows ALL
booth-tile vertices consistently at integer+0.5 (539.5, 568.5, 1316.5)
because the real game's vertex half-pixel adjust function subtracts 0.5 from
every coord and the real game's
AFPU input is uniformly integer. Since our input is mixed, a
uniform -0.5 would produce mixed output; instead each coord is snapped to
the nearest integer-0.5:

    snap(v) = floor(v + 0.5) - 0.5
    540.0  -> 539.5   568.5 -> 568.5   1079.5 -> 1079.5   1317.0 -> 1316.5

Empirical result: the vertical seam at x=540 drops to 1.1 (real game 1.25);
horizontal seams at y=777/1317 lose their static 1-pixel stripe because the
boundary moves to y=1316.5 and pixel ownership transitions cleanly.

### Per-quad quarter-texel UV inset

Known divergence being mitigated: the real game's AFPU emits the BG3 booth as
single full-width un-mirrored quads (probe: base0a is one 6-vert quad, AFP
x=[0..720] = screen x=[0..1080], u=[0.70312..0.87891]). OUR AFPU emits the
SAME booth as four mirrored half-quads (24-vert TRIANGLELIST) whose
left/right halves share the centerline vertex at u=0.70312 - EXACTLY the
boundary between two adjacent packed sub-images in tex001 (texel column
1440 is the first column of an unrelated stage's content). With LINEAR
filtering, the centerline pixel samples 100 percent of texel 1440 (bad
content) -> a 1-pixel bright vertical streak at screen x=540 the real game
does not show. The same mechanism produces stable 1-pixel horizontal seams
at booth tile boundaries (y=777/1047/1317/1587) where v sits on a packed
atlas boundary.

WHY AFPU subdivides differently for us vs the real game is un-RE'd. Candidate
causes: (1) the real game installs an AFPU-side render callback table via
`XE592acd000070` (an afp-utils global) that we do not; (2) scene
state (set_flag, mc_id frame) at a different point changes shape batching.

Mitigation until that is RE'd: inset every quad's UV bbox extremes inward
by 1/4 texel toward the quad's UV center. That moves the centerline u from
0.70312 (atlas col 1440.0) to 0.70312 - 0.25/2048 = 0.70300 (col 1439.75),
still inside our sub-image's last column. Worst-case on-screen displacement
= 0.5 / (2048 * uvspan) * screen_width - sub-pixel. 1/4 texel was picked
empirically: 0.5 texel killed the seam but visibly cooled the mid-booth
body (+0.5 per-region mean diff); 0.25 keeps the seam fix (v_seam 18.6 ->
5.3, h_y1317 21.6 -> 3.2, measured at frame 400 vs game_reference.png on
the y=200..1730 BG crop) and lowers overall mean diff 3.91 -> 3.88.
Only TRIANGLELIST (prim_type 4) draws with vert_count % 6 == 0 are inset
(the multi-quad merged meshes); other prim types use AFP-authored UVs
verbatim and already match the real game. Quads with UV span < 2 texels in a
dimension are skipped (insetting would invert the gradient).

Related trade study (per-texture NEAREST vs real game sampler): applying
texturelist's NEAREST for tex001 kills the x=540 vertical seam cleanly but
exposes booth-tile horizontal boundaries as sharp 1-pixel dark stripes at
y=777/1047/1317 (the atlas texel content genuinely has darker edge rows).
Using the game's real sampler state everywhere smooths the horizontal
boundaries to barely visible (matches the real game's measured 7/30/4 diffs)
but leaves the x=540 centerline at an 18-unit diff (real game: 1). The
real game achieves BOTH because its UVs never sample across the u=0.70312
sub-image boundary. game-sampler mode gives the lower overall diff
(mean 8.6 across 1080x1690 BG3-area pixels) and is the active default; the
per-texture override stays disabled until the AFPU subdivision divergence
is fixed.

### Primitive type mapping

Matches the game's `bm2dx_DrawPrimitiveFromGeo`:

    afp 1, 3 -> D3DPT_LINELIST      prim_count = n / 2
    afp 2    -> D3DPT_LINESTRIP     prim_count = n - 1
    afp 4    -> D3DPT_TRIANGLELIST  prim_count = n / 3
    afp 5    -> D3DPT_TRIANGLESTRIP prim_count = n - 2
    afp 6    -> D3DPT_TRIANGLEFAN   prim_count = n - 2
    default  -> TRIANGLESTRIP       prim_count = n - 2

## Blend modes

### Table A: SetLayer blend_mode (shared bm2dx `SetLayer` enum)

Alpha channel for EVERY mode: SEPARATEALPHABLENDENABLE=TRUE,
SRCBLENDALPHA=ONE, DESTBLENDALPHA=ONE, BLENDOPALPHA=MAX (verified: every
case in bm2dx's blend-mode apply function uses this). Color
channel per mode:

    0            Normal:      ADD,          SRCALPHA, INVSRCALPHA
    4, 8, 0x4F   Additive:    ADD,          SRCALPHA, ONE
                 (mode 8 = ADDITIVE is the shared bm2dx enum also used by
                  DDR set_blend; additive draws additionally bind the
                  coverage pixel shader and switch SRCBLEND to ONE - see
                  additive shader section)
    5            Max/Lighten: MAX,          SRCALPHA, ONE
    3            Multiply:    ADD,          ZERO,     SRCCOLOR
                 alpha-weighted via texture stage 1 (see below)
    6            Min/Darken:  MIN,          SRCALPHA, ONE
    9, 0x46      Subtractive: REVSUBTRACT,  SRCALPHA, ONE
    other        fall back to Normal

Multiply (kind=3) detail: a plain dst*src.rgb multiply IGNORES source
alpha, so a straight-alpha sprite's transparent/semi-transparent texels
still multiply the destination and over-darken (bg_air's blue radial glow
became a hard dark ellipse - the SDVX bg_bpls3 "blue oval"). Fix: final =
dst * lerp(white, tex*diffuse, eff_a). Texture stage 1 blends the modulated
color toward white by its own alpha: TEXTUREFACTOR=0xFFFFFFFF, stage 1
COLOROP=BLENDCURRENTALPHA with COLORARG1=CURRENT, COLORARG2=TFACTOR,
ALPHAOP=SELECTARG1(CURRENT). a=0 texels resolve to 1.0 (multiply no-op) and
the falloff attenuates correctly, matching how the game (same afp-core,
same kind=3) composites it. Stage 1 is reset to DISABLE on every SetLayer
call so a multiply draw's program never leaks into later draws.

### Table B: SetBlend blend_mode

    0  Normal:                 SRCALPHA, INVSRCALPHA
    1  Additive (glows):       SRCALPHA, ONE
    2  Multiply:               DESTCOLOR, ZERO
    3  Screen:                 ONE, INVSRCCOLOR
    4  Additive premultiplied: ONE, ONE
    other -> Normal

## HSV filter (afp filter id 100/101) and shaders

Ground truth: the game's blade filter shader was
dumped off the LIVE GPU and disassembled with `fxc /dumpbin`; its parameters
are g_addColor / g_hsvShift / g_contrast / g_colorMatrix / toneCurveMap. It
is a YIQ (Rec601) HUE ROTATION, NOT the saturation-preserving
"HSVPixelShader" older docs claimed: it rotates chroma in the YIQ plane and
clamps, so it DESATURATES as it rotates (max sat at 0 deg, dipping to ~0.75
at 180 deg). Verified against 119 captured in-game blade frames: mean sat
error 0.003 (vs 0.152 for the wrong HLS shader), mean hue error 6.4 deg.

Shader math (ps_2_b, compiled at device init):

    angle = frac(hue_deg / 360 + 0.5) * 2*pi - pi     (wrap to [-pi, pi),
                                                       exactly as the game)
    mag   = sat_scale * value_scale
    Y     = dot(rgb, (0.299, 0.587, 0.114))
    I     = dot(rgb, (0.595716, -0.274453, -0.321263))
    Q     = dot(rgb, (0.211456, -0.522591, 0.311135))
    I2    = I*cos(angle)*mag - Q*sin(angle)*mag
    Q2    = I*sin(angle)*mag + Q*cos(angle)*mag
    r     = Y + 0.9563*I2 + 0.6210*Q2
    g     = Y - 0.2721*I2 - 0.6474*Q2
    b     = Y - 1.1070*I2 + 1.7046*Q2
    out   = saturate(rgb') * diffuse + g_addColor

Constant layout: c0 = g_addColor (0 here). c1 = g_hsvShift =
`(hue_deg, sat/100 + 1, value/100 + 1, _)` - rotation angle from x,
magnitude from y*z, so a NEGATIVE descriptor sat/value weakens the rotation
(this is the left-hand desaturation). c2 = region-1 UV-space scope rect
(umin, umax, vmin, vmax). c3/c4 = a SECOND filter + rect for region 2;
scope2 = (0,0,0,0) collapses region 2 (k2 = 0).

afp 16-byte HSV descriptor layout: byte 0 = filter id (100/101), byte 3 =
mono flag, u16 at +2 = valid (must be 1), f32 at +4 = hue degrees, +8 =
sat percent, +12 = value percent.

Feed direction: the GAME uploads `360 - descriptor_hue`; the renderer
reproduces that: c1 = (360 - hue, sat*0.01 + 1, value*0.01 + 1, 0).

Two-region rationale: the afp merges the
left hand's ring (hue node) and shuriken (sat node) into ONE draw and by
draw time the shared work struct holds only the LAST node's descriptor.
SetLayer's 16-byte value snapshot preserves the FIRST node; region 1
(c1/c2 = qp_hand_l rect) applies the captured ring hue, region 2 (c3/c4 =
qp_hand_l2 rect) applies the draw-time shuriken sat. Single-node parts
(right-hand blade) feed identical filters and a zero scope2, leaving the
blade path unchanged. The scope rect also keeps a static base fill sharing
a sprite with the animated effect fill (gold sword hilt next to the rainbow
blade, qp_hand_r2) unfiltered.

Sampling while the HSV shader is active: NEAREST (POINT mag+min), matching
the qpro atlas's texturelist (mag/min = nearest); LINEAR would blend the
rainbow stripes. Restored to LINEAR after the draw.

The dormant bm2dx colorize mode (g_mul = 0, which whites the blade) is not
used.

## Additive-coverage shader (export alpha fix)

Problem: additive sprites (note-hit explosions in qp_lane_v_*_bg) are
authored as an opaque (alpha=255) glow on solid black. A straight-alpha
AVIF export would show the black as an opaque black box (consumers use
STRAIGHT alpha - an a=0 export made the burst disappear entirely).

Shader (bound only for SetLayer additive modes 4/8/0x4F, when the HSV
shader is not active):

    c   = tex2D(s0, uv) * diffuse
    g   = c.rgb * c.a          (premultiplied glow)
    cov = max(g.r, g.g, g.b)   (coverage; MAX not luminance so saturated
                                BLUE gets full alpha and is not washed out)
    out = float4(g, cov)

Paired with SRCBLEND=ONE (premultiplied add) for the color channel and the
common MAX alpha blend (alpha = max(cov, dst)). The later UnpremultiplyBGRA
in the export path recovers the straight glow color (rgb / cov) with no hue
skew; on the consumer this composites approximately as glow-over-anything.
Black background -> a=0 (transparent); glow -> a > 0. Dead ends that were
tried and reverted (see also memory notes): plain MAX -> black box;
luminance alpha -> desaturated; straight-unpremult-of-luminance -> cyan;
premultiplied-additive a=0 with premultiplied output -> invisible. State is
reset (shader null, SRCBLEND=SRCALPHA) right after the draw.

## Texture management

- TexCreate(ctx, w, h): creates a `D3DUSAGE_DYNAMIC D3DFMT_A8R8G8B8
  D3DPOOL_DEFAULT` texture, assigns the next slot (1..255; slot 0 unused),
  records dimensions. Returns the slot index - this return value is what
  AFPU stores in the texture record at +4 and later hands back via the
  resolver. Dynamic-pool-default textures CAN be LockRect'd despite GPU
  residency (used by ReadTexturePixels). AFP calls TexCreate once per
  texture slot during `afpu_package_open_streams` (one per atlas in
  texturelist.xml) - a dominant cost of a fresh IFS load, so it also ticks
  the GUI load-progress counter.
- TexUpload(tex_id, format, w, h, x_off, y_off, src_w, src_h, pixels):
  format ids come from the afp-utils texture-format name table in
  afp-utils.dll (16-byte entries: name_ptr + id). Full table
  and DXT details: `docs/formats.md`. Ids handled here:

      0x01 i4/lum-4bit (1 bpp, splatted to all 4 channels)
      0x0E rgb888 (3 bpp, alpha forced 0xFF, RB swapped into BGRA)
      0x10 argb8888/rgba8888 (4 bpp straight copy)
      0x18/0x19/0x1A/0x1B dxt2/dxt3/dxt4/dxt5 (block compressed; routed
           to the Dxt decoder with the destination pitch)
      0x1E la88 (treated as 1 bpp splat)
      0x1F rgb565 id, but the upload path decodes it as 4x4-bit ARGB
           nibbles scaled by 17 (2 bpp)
      0x20 argb8888/xrgb8888 (4 bpp; IIDX's everything-format)
      default: 4 bpp copy

- TexDestroy(tex_id): honoured even for persistent slots - AFPU's own
  teardown for persistent packages (common.ifs etc.) at full shutdown calls
  it explicitly; the persistent boundary only guards the brute-force
  ResetAllTextures sweep. Before releasing, the texture is unbound from
  stage 0 (so D3D9 drops its own reference and the destroy is final) and
  the cached last-uploaded pointer is cleared - otherwise the fallback path
  keeps using a freed D3D9 texture and the driver renders stale content
  (looks like leftovers of the previous IFS after hot-swap).
- Persistent boundary: MarkPersistentBoundary() freezes every slot
  allocated so far (called once, right after LoadBootIfses). ResetAllTextures()
  releases only slots >= the boundary plus the stage-0 binding, so
  common.ifs / gameparts.ifs textures survive hot-swap exactly like in the
  game. g_next_tex_slot is deliberately NOT reset - AFP keeps handing out
  increasing slot ids internally.
- LoadExternalImageSlot(path): WIC-decodes a loose PNG/JPEG into a fresh
  slot (converted to 32bpp BGRA = D3DFMT_A8R8G8B8 memory order) and uploads
  via TexUpload format 0x10, so the slot behaves exactly like an AFP atlas
  slot for the resolver; only the pixel source differs. Used by the SDVX
  submonitor batch path to attach loose subbg_*.png frames to afp clip
  layers via the legacy tex_ref = slot-1 path. These slots are scene-owned
  (dropped on hot-swap) and consume no filter-queue entry in the steady
  state (queue empty after mount). COM init is balanced only when this call
  actually initialized it (RPC_E_CHANGED_MODE tolerated).

### Atlas filter FIFO contract

- API: EnqueueAtlasFilter(mag, min) pushes one (mag, min) pair per
  `<texture>` element parsed from the IFS's texturelist.xml, BEFORE the AFP
  package starts loading. TexCreate pops the queue head on every call and
  stashes the pair in per-slot storage.
- THE INVARIANT: AFPU walks texturelist.xml forward exactly once during
  package open (afp-utils.dll's texturelist walker), so the
  N-th TexCreate call
  corresponds 1:1 to the N-th `<texture>` in declaration order. This is why
  a FIFO works and name/size matching would not: matching by W/H is fragile
  because SDVX BG IFSes can declare two atlases with identical dimensions.
- Values are raw D3DTEXF_* (1 = POINT/nearest, 2 = LINEAR, 0 = leave the
  global default installed by InstallDefaultRenderStates). If the queue is
  exhausted (boot IFSes whose texturelist was not pre-parsed, hand-crafted
  tests, external image slots), the slot's filter stays 0.
- ClearAtlasFilterQueue() resets queue + head; called on every BeginLoad so
  a hot-swap cannot bleed stale entries onto the next IFS's textures.
- Producers: MountAndLoadIfs (scene IFSes), LoadBootIfses (boot IFSes),
  LoadCompanion (companion IFSes) - all via IfsInspect::ReadAtlasFilters.
- Current status: the queue is still populated and recorded per slot, but
  the SubmitGeometry consumer (per-bind SetSamplerState override) is
  DISABLED because the real-game probe proved it does NOT honour
  texturelist filters as D3D9 sampler state (see the sampler section and
  the NEAREST-vs-real game trade study). The plumbing is kept so a future RE
  session can flip modes.
- Historical context for why it was built: BG3 (select_bg_iii.ifs) declares
  its BG atlas tex001 mag/min = "nearest" because the atlas packs unrelated
  sub-images adjacently; global LINEAR produced the well-known 1-pixel
  vertical seam at screen x=540 where a fragment near a UV-rect edge
  bilinear-blends into the neighbour sub-image's first column.

## Anim / movie-clip control (afp_anim.cpp)

- Refer gate: before afp_mc_set / afp_mc_control work on a clip, its mc
  work must be "refered": resolve root with
  `afp_mc_get_id_by_path(stream_id, "")` ("" = root movie clip) then
  `afp_stream_control(6, mc_id)`; otherwise the mc ops return -4. Mirrors
  BM2D::CMovieClip::SetFrameLabel. `afp_mc_get_id_by_path` itself sets the
  refer bit on the CHILD it resolves.
- afp_mc_set codes (afp-core ordinal 0x072; afp_mc_GET ordinal 0x073 stubs
  the label codes to -2 - the SET entry point carries the real ones):

      0x101F  label count (out int)
      0x1020  label name + frame by index (out const char**, out int);
              returns 0 on success
      0x1008  child world position (out float[2] = screen x, y)
      0x1010  current frame: *(u16*)(work + 0x76); FREEZES at the clip's
              stop()
      0x1011  whole-clip total frame count (afp-core's total-frame-count
              helper)
      0x1013  loop_count: *(u16*)(work + 0x104), incremented only on
              wrap-to-0

  Verified live in IIDX 33 afp-core: the 0x1010, 0x1011, and 0x1013 handlers.
  Handlers return -1 on a null out-pointer, so always pass valid locals.
- afp_mc_control ops:
  - `0xF09` = deep goto-play LABEL (recursive: root + children). Same
    mechanism the IIDX AFP debug viewer (CAfpViewerScene) drives with
    F11/RETURN.
  - `0xF08` = deep goto-play FRAME (`afp_mc_deep_goto_play`): clamps
    frame >= total to total-1 (afp-core's frame-clamp helper)
    then deep-syncs child frames (the child-frame deep-sync routine).
    The frame is UNSIGNED inside afp, so
    low-clamp to 0 before calling. The int-frame overload reuses the same
    ordinal 0x071 export pointer (varargs; the r8 vararg is truncated to
    u32).
- Pause/resume = playback SPEED, not the per-clip stop bit: pause ->
  `afp_stream_set_speed(stream, 0.0)` (afp-core's per-stream tick,
  freezes advancement when speed*dt <= 0), resume -> 1.0;
  then stamp stream flag bit 0 (mask=1, val=1) exactly as bm2dx's
  pause/resume flag-stamp helpers do. Both
  act on the stream/layer work (afp-core's stream/layer work resolver),
  so the bounded mc playhead the loop detector watches is
  untouched.
- Child enumeration: `afp_mc_enumerate_children` (afpu ordinal 0x079),
  flags=3 = recurse + full "parent/child" paths, fills a scratch buffer
  with header {u16 written @+0, u16 total @+2, char* name @ +8 + 8*i}.
  Iterate min(written, total) (safe on truncation, rc -5); a 4096-byte
  buffer matches the game scene's own F3 scratch. The bulk enumerate runs
  under SEH: afp faults when a scene is mid-recreating dynamic nested clips
  (sel_all's per-second timer digits) - on fault, keep the last-good list.
  Per-child position/playhead reads also fault on dynamically re-created
  nested clips (sel_all's /timer_usr/timer_10_usr recreate as the timer
  counts; a path resolved one frame is a stale work-struct the next), so
  callers that only need names skip them.
- afp_get_layer_info (afp-core XCd229cc000046 = ordinal 0x46) fills a
  60-byte info struct from the layer's play_work (use a 64-byte buffer for
  headroom). Layout facts:
  - dword at +4 = lifecycle flags. Bit `0x20000000` = "stream finished its
    scripted timeline" latch (set the instant the tick would advance past
    the last authored frame; fires for one-shot clips like exp00.ifs's
    10-tick explosion). Bit `0x40000000` = "timeline wrapped to frame 0"
    latch (only for content with an explicit loop instruction at the
    tail). Bit `0x80000000` = set by `afp_set_complete` when the timeline's
    actionscript declares the
    animation done. None of the three is ever cleared by afp-core, so no
    stale-bit false positives after destroy+play (fresh layers start
    clean). The end-of-animation detector accepts any of `0xE0000000`.
  - w[12] = total frames, w[13] = current frame.
  - w[14] (byte offset 56) = layerobj+60, the root-frame advance-event
    counter: afp-core's per-stream tick does `++*(DWORD*)(a1 + 60)` ONLY
    when the root mc's
    current frame changed this tick. Constant across consecutive
    post-update polls = afp ticked but did not advance = clip is
    stopped/frozen. FPS-independent (counts afp advance events, not wall
    ticks).
  - CAVEAT: some SDVX animations (e.g. hantei.ifs "perfect" judgement)
    never call afp_set_complete and keep an invariant flag word; there is
    no reliable afp-core completion signal for those - the export must be
    bounded by max-frames. The layer struct has live frame state at
    offsets afp_get_layer_info does not expose; reading them directly
    would need a per-game offset table.

## Package / stream lifecycle (afp_packages.cpp, afp_anim.cpp)

### Loading

- Scene package load chain: `afpu_ngp_read_local(pkg_name, mount, 0)` ->
  pkg_id (0x30000000-range id; the package id comes from ngp_read_local,
  NOT from `afpu_ngp_mounttable_load`), then
  `afpu_package_open_streams(pkg_id)` which triggers TexCreate for each
  atlas. Package name = IFS basename (title.ifs -> "title").
- Play ONLY the master clip. bm2dx's CLayer__CreateForPackage (bm2dx.dll)
  calls afp_stream_play(master) ONCE per package; sub-clips
  (x_core / x_full / x_combo / x_exp / x_particle / x_rise_line for
  fcombo00) are PlaceObject children inside the master's timeline. Playing
  them standalone spawns duplicate instances at the stream root (0,0) with
  no inherited transform ("FULL COMBO stuck in upper left", mispositioned
  explosions, post-animation residue).
- Master-name candidates, in order: (1) pkg_hint = IFS basename; (2)
  bm2dx-style names "title", "1p_fullcombo", "2p_fullcombo"; (3) "top" -
  the afp/flash root-clip convention; GITADORA plays scene IFSes via the
  "top" anim (RE: gdxg's scene-play caller invokes gdxg's
  stream-play helper with args `("...ifs", "top")`);
  tried before the afplist order so root-"top" packages do not mis-pick
  the first afplist entry (e.g. afp_result_10_c's "rec_diff_mark" badge);
  (4) every name from /afp/packages/afp/afplist.xml in order - crucial for
  SDVX IFSes whose master is not named after the basename (hantei.ifs's
  master is "perfect"; without the fallback nothing plays and every
  exported frame is transparent).
- `afpu_afp_get_info_in_package` (afpu ordinal 0x062) fills a 64-byte
  descriptor: info[2] = afp-data blob pointer, info[3] = data id; then
  `afp_stream_play(data_id, data_ptr, 0, 0)` returns the stream id
  (negative or 0xFFFFFFFD = failure).
- Post-play stamps: flag 512 marks the MASTER as "main playable" (bm2dx
  sets it only on the master, CLayer__CreateForPackage a5==1 branch;
  PlaceObject sub-streams do not get it). `afp_stream_set_speed(1.0)`
  (some streams ship paused at speed 0). Flag bit 0 = "timeline
  advancing": GITADORA's gdxg sets `afp_set_flag_mask(stream, 1, 1)` (RE:
  gdxg's stream-play helper, GITADORA delta boot docs);
  without bit 0 the timeline
  never advances and every frame redraws the collapsed frame-0 state (all
  verts at the shape anchor, zero area) - the GITADORA blank-render bug.
  SwitchAnimation sets bits 0+9 together (mask/value 513); harmless on
  IIDX/SDVX whose streams already advance (bm2dx sets bit 0 via its
  kick-stream helper). Historical: on the ORIGINAL
  LoadPackages path flag 1 had no effect on fcombo's frame-60 blank and
  was left out there (512 only).
- The "in" label trigger belongs to the fade_inout layer, not fcombo's
  master: in bm2dx's fade-in trigger function, a1[5] is the fade_inout
  layer (loaded
  from a separate locale IFS) and THAT receives gotoAndPlay("in"); fcombo
  plays from frame 0 as authored.
- Single-bitmap fallback A: `afp_stream_play_bitmap_by_name` (afp-core
  ordinal 0x021) for IFSes that contain one bitmap and no animation
  (sub_customize_bg001.ifs shape).
- Single-bitmap path B (SDVX Type 2 / CSprite system BGs; mirrors
  soundvoltex.dll's CSprite bitmap-stream builder):
  1. afpu ordinal 0x046 `afpu_image_lookup(out64, pkg_id, name)` - fills a
     64-byte image-info (atlas refs, UVs, dims); 0 = hit, nonzero
     (typically -2) = miss.
  2. afpu ordinal 0x04C `afpu_image_to_stream_args(out40, info64)` - pure
     transform into the 40-byte stream-args.
  3. afp-core ordinal 0x022 `afp_image_stream_create(args40, 0)` -
     allocates a 704-byte slot from the global stream pool, stamps it as a
     single-bitmap stream (flag 0x400), returns the stream id.
  This path does NOT consult afplist.xml - the bitmap name is looked up
  directly in the package's image table.

### create_level / priorities (persistent vs scene)

- `afp_stream_play` stores the current create-level (an afp-core global set
  via afp_set_create_level) into `stream->+36` at
  creation.
- LoadBootIfses brackets with create_level=1 -> persistent streams;
  LoadPackages / SwitchAnimation / PlayBitmapAnimation bracket with
  create_level=0 -> scene streams. Invariant: create_level == 0 outside of
  LoadBootIfses.
- Boot-time persistent IFS set mirrors bm2dx's CSSDLoader__BootstrapBaseIfsSet
  (IIDX 33): 13 graphic-bootstrap entries in load order -
  common, common_j, gameparts, gameparts_j, graph, graph_j, mdata, dlbg,
  sub_common, sub_premium_area, sub_premium_area_j, qp_main2 (mounted
  WITHOUT the /1/ prefix in the game), led. The game loads these at
  priority 0 (its own bookkeeping); they survive every scene transition and
  provide clips/textures that scene IFSes reference via PlaceObject +
  afp_mc_get_id_by_path. Without them the renderer paints partial content
  and stale orphans after hot-swap.
- imagefs mounts refuse Windows absolute paths - the source must resolve
  through avs_fs_open, so the host data dir is first mounted as a VFS
  fs-root and IFS images are mounted from VFS paths.

### Teardown (hot-swap)

Matched to what bm2dx actually does (full decomp walk in
the bm2dx scene-teardown path):

- bm2dx destroys streams via its CLayerTable: each CLayer dtor walks an
  MC-instance tree calling `afp_stream_destroy(5, child_sid, 0)` per node,
  catching implicit child streams spawned by PlaceObject tags. Destroying
  only explicitly-played streams leaks every implicit child (symptom:
  title.ifs character art + logo keep painting after swap).
- Clean equivalent without a CLayerTable: afp-core's built-in priority
  filter. `afp_stream_destroy(type=8, priority=0, schedule=0)` destroys
  every stream whose `stream->+36` == 0 - i.e. every scene stream including
  implicit children - without touching persistent (level 1) streams.
  Mirrors bm2dx's CBM2D_Tables__PurgeByOwner(0) + CLayer dtor chain using
  afp-core's own index.
- afp_stream_destroy types used: 5 = destroy by stream id (schedule 0 =
  immediate); 8 = destroy by priority equal. Destroy-by-id cascades the
  whole master tree, dropping afp_mc_attach_stream children, so a
  companion package can be freed without a dangling attached clip
  (detach-before-unload).
- Table B (bitmap streams from afp_stream_play_bitmap_*) is NOT walked by
  the type=8 sweep. Table B's live count is an afp-core global;
  check it for survivors if content keeps drawing.
- `afp_get_layers_by_nr(category, buf, n)` enumerates Table A streams,
  filtering by `stream->+14 == category`.
- Package data teardown: `afpu_package_control(6, pkg_id)` walks the
  package's 8 internal record tables and cascades TexDestroy for every
  texture record. It does NOT destroy streams (afp-core's job, handled by
  the sweep).
- Order matters: companions are unloaded FIRST so their (create_level=0)
  streams are caught by the same type=8 sweep while their package records
  are released by package_control(6); doing it after the sweep leaves
  their streams orphaned for a frame.
- Backstop after all of the above: ResetAllTextures() wipes scene slots
  even if AFPU never fired TexDestroy for some; a leftover draw then lands
  on the white fallback - visible but unambiguous, never the previous
  IFS's content.

### Companions (locale overlay IFSes)

- Mount sequence mirrors LoadBootIfses: MountFsRoot for the companion's
  parent dir under a private VFS alias, MountIfsImage, afpu_ngp_read_local,
  then EnqueueAtlasFilter for its texturelist, then
  afpu_package_open_streams (fires TexCreate for its atlases, making its
  bitmaps live).
- Companions are loaded at the normal scene level (create_level=0), NOT
  persistent: they are overlay packages whose streams live and die with
  the base; the scene sweep catches them.
- Name-shadowing mechanism: IIDX companions share bitmap NAMES with the
  base (title.ifs::coin vs title_j.ifs::coin). Last-loaded wins in AFPU's
  bitmap lookup, so loading title_j after the base overlays the Japanese
  coin; dropping the companion package makes the base's bitmap
  authoritative again.
- Unload = reverse: `afpu_package_control(6, pkg_id)` (cascades TexDestroy),
  then umount the image mount, then the fs-root mount.
- ForceReplay exists because a companion load/unload requires re-binding
  the master (destroy + play); SwitchAnimation short-circuits on
  same-name, so the stashed name is cleared first to force the path.

### Misc stream facts

- Invalid/no-stream sentinel used throughout: 0xFFFFFFFC.
- afp_stream_play failure returns negative or 0xFFFFFFFD.
- afp_stream_control(5, sid) is issued before destroy-by-id on the
  explicit-teardown path (bm2dx CPrimBase::ReleaseStream pattern).
- afp_set_flag_mask = afp-core ordinal 0x037; afp_stream_control = ordinal
  0x015.

## Open issues recorded in the code

- AFPU mesh-subdivision divergence vs the real game (BG3 booth: 24-vert
  mirrored half-quads for us, single 6-vert quads on the real game). The UV
  inset + half-pixel snap are mitigations, not the mechanism. Leads:
  afp-utils `XE592acd000070` render-callback-table install
  (an afp-utils global), or scene-state differences in shape batching.
- FCOMBO frame-60 blank (IIDX): draws continue past frame 60 (AFPU
  per-frame count rises ~6 -> ~12) but the screen goes blank; geo[1]
  switches 0x9 (XY+UV) -> 0xA (XYZ+UV). Forcing z=0 and opaque-red color
  did not bring the geometry back - it is not being rasterized. Need
  `bm2dx_DrawPrimitiveFromGeo` to show how the game's vertex
  packer routes the
  flags=0xA path. Symptoms match "draws land in a different render
  target / state".
- LayerCommand cases 0/1/2 (bm2dx custom primitives) are stubbed; a
  scream-once diagnostic (with the caller mapped back to its
  afp-core/afp-utils offset) fires if any content ever exercises them.
- No completion signal for flat-flag SDVX animations (hantei "perfect");
  export must be frame-bounded by the user.

## Vertex snap + UV inset (Render::DecodeAfpVertices / InsetQuadUvs)

Preserved from afp_d3d9_callbacks.cpp SubmitGeometry when the logic was
extracted into the unit-tested r573_render vertex-math module. The original
in-code rationale follows.

### Half-pixel grid snap

```
        // Half-pixel grid snap. Empirically discovered via
        // VertProbe: AFPU hands us vertex coords inconsistently -
        // some at integer-aligned values (e.g. x=540.0, y=1317.0,
        // y=777.0, y=1047.0), others at integer+0.5 (y=568.5,
        // y=871.5, x=1079.5). The real game's probe shows all
        // booth-tile vertices CONSISTENTLY at integer+0.5 (e.g.
        // 539.5, 568.5, 1316.5) because the real game's vertex
        // half-pixel adjust function subtracts 0.5 from every
        // coord, and the real game's AFPU input is uniformly at integer.
        //
        // Our AFPU input is mixed, so a uniform -0.5 subtraction
        // produces mixed output too. Instead we snap each coord
        // to the nearest integer-0.5 with `floor(v + 0.5) - 0.5`:
        //   540.0   -> floor(540.5)-0.5  = 539.5  (snap down)
        //   568.5   -> floor(569.0)-0.5  = 568.5  (unchanged)
        //   1079.5  -> floor(1080.0)-0.5 = 1079.5 (unchanged)
        //   1317.0  -> floor(1317.5)-0.5 = 1316.5 (snap down)
        //
        // This gives consistent half-pixel alignment for all
        // coords and matches the real game's vertex grid. Empirical
        // result: v-seam at x=540 drops to 1.1 (real game 1.25),
        // h-seams at y=777/1317 lose their static 1-pixel stripe
        // because the y=1317 boundary moves to y=1316.5 and pixel
        // ownership transitions cleanly across the integer
        // boundary instead of straddling it.
        auto snap = [](float v) { return std::floor(v + 0.5f) - 0.5f; };
```

### Per-quad UV bbox inset (quarter-texel inward)

```
    // Per-quad UV bbox inset (quarter-texel inward).
    //
    // The real game's AFPU emits the booth as single full-width
    // un-mirrored quads (BG3 base0a is one 6-vert quad spanning
    // x=[0..720] AFP-space = screen x=[0..1080] with
    // u=[0.70312..0.87891]). Our AFPU emits the SAME booth as
    // four mirrored half-quads (24-vert TRIANGLELIST), with each
    // pair of left/right halves sharing the centerline vertex at
    // u=0.70312. That u value lies EXACTLY on the boundary between
    // two adjacent packed sub-images in tex001 - texel column 1440
    // is the first column of an UNRELATED stage's content. With
    // LINEAR addressing, the centerline pixel samples 100% of texel
    // 1440 (bad content); neighbouring pixels sample the last column
    // of OUR tile (good content). Result: a 1-pixel-wide bright
    // vertical streak at screen x=540 that the real game doesn't show.
    //
    // The same mechanism explains a stable 1-pixel horizontal seam
    // at every booth tile boundary (y=777/1047/1317/1587) where the
    // v coord sits at a packed-atlas boundary value.
    //
    // We don't know WHY AFPU subdivides the mesh differently for us
    // vs the real game (candidates: the real game installs an AFPU
    // render-callback table via XE592acd000070 we don't, or
    // scene-state differences change shape batching). Without
    // fixing the subdivision, the safest mitigation is to inset
    // every quad's UV bbox extremes inward by 1/4 texel toward the
    // quad's UV centre. That moves the centerline u from 0.70312
    // (atlas col 1440.0) to 0.70312 - 0.25/2048 = 0.70300 (atlas
    // col 1439.75) - still inside OUR sub-image's last column. The
    // worst-case on-screen displacement is 0.5 / (2048 * uvspan) *
    // screen_width pixels - sub-pixel.
    //
    // 1/4 texel was picked empirically: 0.5 texel killed the seam
    // but the larger UV shift visibly cooled the mid-booth body
    // (+0.5 to per-region mean diff). 0.25 keeps the seam fix
    // (v_seam 18.6 -> 5.3, h_y1317 21.6 -> 3.2, all measured at
    // f400 vs game_reference.png on the y=200..1730 BG crop) and
    // also pulls the overall mean diff down (3.91 -> 3.88).
    //
    // Only TRIANGLELIST (prim_type 4) draws get insetting because
    // those are the multi-quad merged meshes. Other prim types
    // (LINESTRIP, TRIANGLESTRIP, etc.) use AFP-authored UVs verbatim
    // - they're typically single-quad draws that already match the
    // the real game's output exactly.
    //
    // To revisit when the AFPU mesh-subdivision divergence is RE'd:
    //  - probe README has the real game's actual per-draw vertex
    //    dump for BG3 (single 6-vert quads).
    //  - render_backend.cpp's VertProbe LOG block (just below) is
    //    the live counterpart that dumps our own per-draw vertices
    //    for any draw whose X bbox crosses x=540.
```
