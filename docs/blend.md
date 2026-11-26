# r573_render / Blend (src/render/blend_map.h)

This module maps the afp SetLayer blend mode to a D3D9 blend state, and both render
backends use it. Before it existed, the table was written out twice (modern
`AfpD3D9::SetLayer` in afp_d3d9_callbacks.cpp and DDR `Cb_SetBlend` in
afp_ddr_render.cpp), and the set of additive modes a third time (the coverage-shader
gate in SubmitGeometry). Project history shows blend fixes landing in one copy at a
time (bg_0028's mode-8 stars were fixed on the DDR side first). One change now covers
both backends.

## Provenance

The mapping was RE'd from the game's own SetLayer blend dispatch (bm2dx.dll). The afp
blend-mode enum is the same across afp generations: the modern afp-core SetLayer
callback and DDR's libafp 2.13.7 set_blend callback both use it. To re-find it after a
DLL update, locate the game's SetLayer/set_blend callback in the render-callback table
and read its switch over the mode argument. Every case sets BLENDOPALPHA=MAX +
SRCBLENDALPHA=ONE + DESTBLENDALPHA=ONE, plus the per-mode color op/src/dst below.

## The mapping (Blend::MapAfpMode)

| afp mode      | meaning     | BLENDOP     | SRCBLEND | DESTBLEND   |
|---------------|-------------|-------------|----------|-------------|
| 0, unknown    | normal      | ADD         | SRCALPHA | INVSRCALPHA |
| 4, 8, 0x4F    | additive    | ADD         | SRCALPHA | ONE         |
| 5             | lighten/max | MAX         | SRCALPHA | ONE         |
| 3             | multiply    | ADD         | ZERO     | SRCCOLOR    |
| 6             | darken/min  | MIN         | SRCALPHA | ONE         |
| 9, 0x46       | subtractive | REVSUBTRACT | SRCALPHA | ONE         |

`Blend::kAlphaCoverage` is the alpha-channel policy all modes share:
BLENDOPALPHA=MAX, SRCBLENDALPHA=ONE, DESTBLENDALPHA=ONE. max(src.a, dst.a) keeps
coverage intact, where ADD would saturate overlapping draws to 255 and wash out the
edges of a straight-alpha export.

`Blend::IsAdditiveAfpMode` tests membership of {4, 8, 0x4F}. It is used by the mode
rows above and by the additive-coverage pixel-shader gate in modern SubmitGeometry
(the exported-alpha fix for glow sprites on a black background, see
docs/d3d9_backend.md).

The constants (`kOpAdd` ... `kFactorInvSrcAlpha`) carry the numeric values of the
matching D3DBLENDOP_* / D3DBLEND_* enums, so the gated lib does not have to include
d3d9.h. tests/render/blend_map_tests.cpp includes the real d3d9.h and pins each
constant to its SDK value.

## Backend-specific extras (not in the shared table, by design)

- Modern multiply (mode 3) adds an alpha-weighting program on texture stage 1 on top
  of the shared ZERO/SRCCOLOR row (final = dst * lerp(white, tex*diffuse, eff_a)), so
  straight-alpha sprites do not over-darken (the bg_air "blue oval" fix). DDR keeps the
  plain multiply, because its content was verified against the game without the
  weighting. See SetLayer in afp_d3d9_callbacks.cpp.
- Only the modern backend has the additive-coverage pixel shader. `CompileAfpShaders`
  runs from afp_d3d9.cpp alone, and the legacy AFP 2.13.7 backend's `Cb_SetBlend`
  (afp_ddr_render.cpp) binds no pixel shader, so its additive modes keep the plain
  `kAlphaCoverage` MAX alpha. This leaves an open defect: on that backend, a
  transparent-background export of an opaque-black additive sprite (the IIDX 18/19
  note-hit explosions) writes a black rectangle over the sprite's bounding box. The
  shader has not been ported to the legacy backend yet; the fix is the same
  `max(r, g, b)` coverage described in docs/d3d9_backend.md.
- Modern `SetBlend` (the afp-core stream-level callback) uses a different enum
  (0=normal, 1=additive, 2=multiply, 3=screen, 4=premultiplied-add) and does not use
  this table. Keep the two separate.

## Verification

Unit tests pin every row and the SDK enum values. Merging the copies was verified as
byte-identical on both backends: a modern 12-frame dump (IIDX 33 02005.ifs) and DDR
`--ddr-test` PNGs for background_0001 / 0009 / 0028 (0028 has the mode-8 additive
stars) all had the same SHA256 before and after the rewiring.
