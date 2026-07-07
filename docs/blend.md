# r573_render / Blend (src/render/blend_map.h)

The afp SetLayer blend-mode -> D3D9 blend-state mapping, shared by BOTH render
backends. Before this module the table existed twice (modern
`AfpD3D9::SetLayer` in afp_d3d9_callbacks.cpp and DDR `Cb_SetBlend` in
afp_ddr_render.cpp) and the additive-mode set existed a third time
(SubmitGeometry's coverage-shader gate) - and project history shows blend fixes
were applied to only one copy at a time (bg_0028's mode-8 stars were fixed on
the DDR side first). Now a single change covers both backends.

## Provenance

RE'd from the game's own SetLayer blend dispatch (bm2dx.dll).
The afp blend-mode enum is shared across afp generations - the modern afp-core
SetLayer callback and DDR's libafp 2.13.7 set_blend callback both use it. To
re-find it after a DLL update: locate the game's SetLayer/set_blend callback in
the render-callback table and read its switch over the mode argument; every
case sets BLENDOPALPHA=MAX + SRCBLENDALPHA=ONE + DESTBLENDALPHA=ONE plus the
per-mode color op/src/dst below.

## The mapping (Blend::MapAfpMode)

| afp mode      | meaning     | BLENDOP     | SRCBLEND | DESTBLEND   |
|---------------|-------------|-------------|----------|-------------|
| 0, unknown    | normal      | ADD         | SRCALPHA | INVSRCALPHA |
| 4, 8, 0x4F    | additive    | ADD         | SRCALPHA | ONE         |
| 5             | lighten/max | MAX         | SRCALPHA | ONE         |
| 3             | multiply    | ADD         | ZERO     | SRCCOLOR    |
| 6             | darken/min  | MIN         | SRCALPHA | ONE         |
| 9, 0x46       | subtractive | REVSUBTRACT | SRCALPHA | ONE         |

`Blend::kAlphaCoverage` is the alpha-channel policy every mode shares:
BLENDOPALPHA=MAX, SRCBLENDALPHA=ONE, DESTBLENDALPHA=ONE - max(src.a, dst.a)
preserves coverage where ADD would saturate overlapping draws to 255 and wash
out exported straight-alpha edges.

`Blend::IsAdditiveAfpMode` is the {4, 8, 0x4F} membership test. Consumers: the
mode rows above, and modern SubmitGeometry's additive-coverage pixel-shader
gate (the exported-alpha fix for black-background glow sprites - see
docs/d3d9_backend.md).

The constants (`kOpAdd` ... `kFactorInvSrcAlpha`) carry the numeric values of
the corresponding D3DBLENDOP_* / D3DBLEND_* enums so the gated lib does not
include d3d9.h; tests/render/blend_map_tests.cpp includes the real d3d9.h and
locks each constant to its SDK value.

## Backend-specific extras (NOT in the shared table, by design)

- Modern multiply (mode 3) adds an alpha-weighting texture-stage-1 program on
  top of the shared ZERO/SRCCOLOR row (final = dst * lerp(white, tex*diffuse,
  eff_a)) so straight-alpha sprites do not over-darken (the bg_air "blue oval"
  fix). DDR keeps the plain multiply - its content is game-verified without
  the weighting. See afp_d3d9_callbacks.cpp SetLayer.
- Modern `SetBlend` (afp-core stream-level callback) uses a DIFFERENT enum
  (0=normal, 1=additive, 2=multiply, 3=screen, 4=premultiplied-add) and is NOT
  this table - do not unify them.

## Verification

Unit tests lock every row + the SDK enum values. The dedup itself was verified
byte-identical on both backends: modern 12-frame dump (IIDX 33 02005.ifs) and
DDR `--ddr-test` PNGs for background_0001 / 0009 / 0028 (0028 = the mode-8
additive stars) - all SHA256-equal before vs after the rewiring.
