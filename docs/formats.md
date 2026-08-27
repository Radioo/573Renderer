# r573_formats (src/formats/)

Pure, stdlib-only container/codec parsers. No AVS/AFP, no D3D9, no logging -
this module builds and unit-tests standalone (`formats_tests`, CTest label
`ci`), and everything in it is exercised by synthetic fixtures, never by
Konami data.

## DDR .arc container (`ddr_arc.h`)

DDR World (MDX) archive reader + AVS-LZ77 decompressor. Format and codec
were reverse-engineered from gamemdx.dll / libavs-win64.dll and verified
byte-exact against the real game's own output.

A `.arc` is a flat container, all fields u32 little-endian:

| Offset | Field |
|---|---|
| header +0 | magic `0x19751120` |
| header +4 | version |
| header +8 | entry count |
| header +12 | comp_flag |
| entry +0 | name_offset - NUL-terminated logical path in a packed blob after the table |
| entry +4 | data_offset |
| entry +8 | decomp_size |
| entry +12 | comp_len |

Entries are 16 bytes each, table starts at offset 16. `comp_len >=
decomp_size` means the entry is stored verbatim; otherwise the payload is
AVS-LZ77 compressed. In DDR World the renderable scenes are standard `.ifs`
files wrapped one per arc under `data/arc/bm2d/`; `ExtractFirstIfs`
decompresses the `.ifs` entry for the normal IFS pipeline.

`ReadToc` reads only the file head: header, entry table, and the name blob
up to the smallest `data_offset` (names sit between the table and the first
data blob in well-formed arcs). That keeps a TOC scan across thousands of
arcs cheap - the multi-MB data region is never pulled. Name offsets pointing
past the read head produce a synthetic `<name@0xNNN>` label instead of
failing the whole arc. `ExtractFirstIfs` likewise reads only the matched
entry's byte range, not the whole file.

### AVS-LZ77 codec

4096-byte sliding window; write position starts at `0xFEE`; the window
pre-history is ZERO-filled (the game allocates the context with calloc -
back-references into untouched window bytes legitimately produce zeros).
Stream structure: one control byte carries 8 flags, consumed LSB-first;
flag 1 = literal byte, flag 0 = match. A match is two bytes forming
`token = (b1 << 8) | b2`: `distance = token >> 4` (12-bit),
`length = (token & 0xF) + 3`, copy source = `(write_pos - distance) & 0xFFF`,
copied byte-by-byte through the window (so overlapping matches repeat
recent output). `distance == 0` is the end-of-stream marker. The
`expected_size` argument stops decompression early once that many bytes are
out; 0 means run to the end-of-stream marker.

## DXT / S3TC decode (`dxt_decode.h`)

AFP textures arrive uncompressed (rgb565 / rgb888 / (a)rgb8888 / la88) or
block-compressed (DXT1-5). The renderer's atlas textures are always created
as `D3DFMT_A8R8G8B8` in `TexCreate`, so the DXT path decompresses 4x4
source blocks into BGRA scanlines written directly at the destination
pitch - the same memory layout the row-copy upload path uses, no scratch
buffer. `TexUpload` in `afp_d3d9_textures.cpp` is the sole in-renderer
caller; it sizes the source span via `EncodedSize`.

Format ids are the afp-utils texture-format table (afp-utils.dll,
16-byte entries of name_ptr + id):

| id | format | bytes/block | alpha |
|---|---|---|---|
| 0x17 | dxt1 | 8 | opaque, or 1-bit punchthrough when color0 <= color1 |
| 0x18 | dxt2 | 16 | premultiplied explicit 4-bit (decoded as dxt3) |
| 0x19 | dxt3 | 16 | explicit 4-bit |
| 0x1A | dxt4 | 16 | premultiplied interpolated (decoded as dxt5) |
| 0x1B | dxt5 | 16 | interpolated 8-bit, 3-bit indices |

Decode notes:

- RGB565 endpoints expand 5/6/5 -> 8/8/8 via "shift, then OR the high bits
  back in" so max channel values map to 0xFF rather than 0xF8/0xFC.
- BC1 color block: `color0 > color1` selects the 4-color mode (two
  interpolants at 1/3 and 2/3); `color0 <= color1` selects the 3-color +
  punchthrough mode where palette index 3 is fully transparent black.
- BC2 alpha: 16 x 4-bit explicit values, expanded x17 (0x11) to 8-bit.
- BC3 alpha: two endpoints + 16 x 3-bit indices packed little-endian into
  the remaining 6 bytes; `a0 > a1` gives 6 interpolants, otherwise 4
  interpolants plus hard 0x00 and 0xFF entries.
- DXT2/DXT4 (premultiplied) are decoded as DXT3/DXT5: the renderer's blend
  state expects straight alpha, the visual difference is negligible for AFP
  atlases in practice, and the in-game equivalent applies the same
  simplification.
- Tail blocks at the right/bottom edges are clipped to the surface size.
- Truncated source data aborts the decode without writing partial blocks.

Why this exists: SDVX's older `select_bg_iii.ifs` (and similar) package
most atlas content as DXT5. Without the decoder every dxt-encoded slot
stayed at the D3D create-time default (transparent black) - the historical
"missing assets" symptom.

### The afp-utils texture-format id table (re-read from the DLL)

Read directly out of IIDX 33 `modules/afp-utils.dll`. How to re-find it on
a new DLL version: string-search `argb8888rev` (or `rgb565rev`, `ati2n` -
names unique to this table), take the DATA xref to the string, then walk
BACKWARDS to the table base. The base is named by a descriptor record that
also carries the entry count - here `{..., "compress", 0x180021100, 61}` at
`0x1800216c0`. Entry layout is **`[id(4), pad(4), name_ptr(8)]`**, 16 bytes,
ids ascending from 0. Anchor the layout on the FIRST entry: it must read
`id = 0`. (Misreading the layout as `[name_ptr, id]` shifts every name by
one slot and produces a plausible-looking but wrong table - that mistake
was made and caught here.)

| id | name | id | name | id | name |
|---|---|---|---|---|---|
| 0x00 | a4 | 0x0D | rgba4444 | 0x1A | **dxt5** |
| 0x01 | a8 | 0x0E | **rgb888** | 0x1B | la88 |
| 0x02 | i4 | 0x0F | rgbx8888 | 0x1C | al44 |
| 0x03 | i8 | 0x10 | **rgba8888** | 0x1D | al26 |
| 0x04 | l4 | 0x11 | p4 | 0x1E | al88 |
| 0x05 | l8 | 0x12 | p8 | 0x1F | **argb4444** |
| 0x06 | la44 | 0x13 | argb1555 | 0x20 | **argb8888rev** |
| 0x07 | la62 | 0x14 | xrgb8888 | 0x21 | ati2n |
| 0x08 | rgba2222 | 0x15 | argb8888 | 0x22 | rgba4444rev |
| 0x09 | rgb332 | 0x16 | **dxt1** | 0x23 | rgb565rev |
| 0x0A | rgbx5551 | 0x17 | dxt2 | 0x24 | rgba5551rev |
| 0x0B | rgb565 | 0x18 | **dxt3** | 0x3C | xrgb8888rev |
| 0x0C | rgba5551 | 0x19 | dxt4 | 0x3D | xrgb1555 |

Bold = an id the renderer actually decodes today, and all of those now
agree with the table: `0x0E` rgb888 (3 bpp), `0x10` rgba8888 (4 bpp),
`0x1F` argb4444 (our 4x4-bit-nibble x17 decode IS argb4444, 2 bpp), `0x20`
argb8888rev (4 bpp), and the DXT block.

The id space is shared across engine generations, not per-game: the DDR
decoder (libafp 2.13.7, `afp_ddr_textures.cpp`) was derived independently
from DDR RE and uses `0x16` -> DXT1 and `0x1A` -> DXT5, which matches this
table exactly on both.

### The DXT ids were off by one until this was checked

`Dxt::kFmtDxt1..kFmtDxt5` used to be `0x17..0x1B`. They are now
`0x16..0x1A`, matching the table. The old numbering made `0x1B` (really
`la88`) decode as a DXT format and left real `dxt1`/`dxt2` falling through
to the 4-bpp raw default.

Why it was invisible: DXT2/DXT3 share one block layout (explicit alpha) and
DXT4/DXT5 share the other (interpolated alpha), and the decoder only
branches on those two families. Under the old off-by-one, real `dxt3`
(0x18) landed on our "dxt2" label -> explicit alpha -> correct anyway, and
real `dxt5` (0x1A) landed on "dxt4" -> interpolated -> correct anyway. Since
dxt3 and dxt5 are what the assets actually use, output was right by
accident. Only `0x16`/`0x17`/`0x19`/`0x1B` were wrong, and nothing in the
net uses them - confirmed empirically: the whole 14-case byte-compare net
is byte-identical before and after the renumber.

`FormatBpp` in afp_d3d9_textures.cpp now asks `Dxt::IsDxtFormat` instead of
carrying its own `0x18..0x1B` list, so the two ranges cannot drift apart
again (they already had: `IsDxtFormat` said `0x17..0x1B`, `FormatBpp` said
`0x18..0x1B`).

### Still unverified (do not "fix" on the table alone)

- `0x01`: we splat 1 bpp to all four channels; the table says `a8`. Same
  bpp, different semantics (alpha-only vs luminance splat).
- `0x1E`: we treat it as a 1 bpp splat; the table says `al88`, which is
  2 bpp. If a real asset ever uses it the stride would be wrong - but no
  covered asset does, so there is nothing to verify against.
- DDR `0x1F`: `afp_ddr_textures.cpp` decodes it as RGB565 while the table
  (and the modern decoder) say argb4444. The modern side is the one that
  matches; the DDR branch is unexercised by the net, so it is recorded here
  rather than changed blind.

To settle any of these, get ground truth rather than guessing: instrument
the live game through the afp hook and log `(format, w, h, byte_count)` for
a texture known to use the id - the byte count divided by w*h gives the bpp
directly.

## AES-256-CBC-CTS (`aes.h`)

Decrypt-only AES-256 in CBC mode with ciphertext stealing, used by the IIDX 17
(SIRIUS) encrypted sprite packages (`system.idr`, `N.gcr`). Ciphertext stealing
matters: 136 of the 141 encrypted files have a length that is not a multiple of
16, and the format carries no padding, so the plaintext is exactly as long as
the ciphertext.

`DecryptCbcCts(key, iv, cipher, out, err)` takes the 32-byte key and the 16-byte
IV separately; the caller splits them off the file (the IV is the first 16 bytes,
the ciphertext is the rest). Key derivation is game-specific and lives with the
package loader, not here.

The tail follows NIST CBC-CS3 / Kerberos CTS: with a partial last block of `d`
bytes, `P_n = D(C_{n-1})[0:d] XOR C_n`, and `P_{n-1} = D(C_n || D(C_{n-1})[d:16])
XOR C_{n-2}`.

The S-box is generated at first use rather than shipped as a literal table:
multiplicative inverses come from a log/antilog pair over GF(2^8) with generator
3, then the standard affine transform. The one trap is `a == 1`, where
`255 - log[a]` is 255 while the antilog table only fills 0..254 - the exponent
has to be reduced modulo 255. Getting that wrong corrupts exactly two S-box
entries, which leaves most blocks decrypting correctly and looks like a chaining
bug rather than a cipher bug. The NIST SP 800-38A CBC-AES256 vector in
`tests/formats/aes_tests.cpp` catches it immediately.

## DirectX .x models (`xfile.h`, `xfile_binary.h`)

`XFile::Parse` takes a whole `.x` file. The header's third field selects the
encoding: `txt ` is parsed directly by the recursive-descent reader in
`xfile.cpp`, and `bin ` is first transcoded to the text form by
`XFile::BinaryToText`, then re-entered through the same `Parse`. Anything else
(`tzip`, `bzip`) is rejected.

Transcoding rather than writing a second parser is deliberate: the two encodings
are the same object graph, and the text reader is the one covered by tests. The
transcoder invents the separators the binary form omits (it emits `value;` after
every integer and float, which the reader treats as whitespace since every list
carries its own length), drops GUIDs, and formats floats with a
shortest-round-trip conversion so model coordinates survive the trip.

## `.inz` scene manifest and the texture atlas (`inz.h`)

`Inz::Parse` reads the LZSS-inflated `.inz` text that ships beside the `.xz`
models of an IIDX 10-18 model scene. `[image_file]` lists the `.gcz` slices in
order; `[pattern_list]` maps an authoring `.bmp` path to a rect `x,y,w,h`.

Those rects are coordinates in a VIRTUAL ATLAS, not in the `.gcz` that carries
them, and the atlas is a fixed grid of **256x256** tiles, one tile per row.
That grid is not in any file - the game hardcodes it when it constructs the
manifest object - so `Scene3d` owns the `Inz::AtlasGrid` constant and `Inz` only
does the arithmetic. Slice `i` becomes the whole tile at grid cell
`(i % tiles_per_row, i / tiles_per_row)`; a `.gcz` smaller than the tile
occupies its top-left corner and the rest of the tile stays transparent black.

### The packer's green filler is colour keyed away

`Scene3d::ScatterSlice` (`src/scene3d/atlas.cpp`) expands a `.gcz` slice and
then replaces every texel whose ARGB is exactly `0x0000FF00` with transparent
black before writing it into the tile. That is the game's own step, not a
cosmetic clean-up: IIDX 10 blits each slice with

```c
D3DXLoadSurfaceFromMemory(
    tile_surface, NULL, &dst_rect,
    gcz_payload, 25 /* D3DFMT_A1R5G5B5 */, 2 * span_width, NULL, &src_rect,
    0xFFFFFFFF /* D3DX_DEFAULT filter */, 0x0000FF00 /* ColorKey */);
```

and D3DX replaces colour-key matches with `0x00000000`. A1R5G5B5 `0x03E0`
expands to `A=0, R=0, G=0xFF, B=0`, which is the packer's fill colour, so the
unused space around the packed patterns becomes black rather than green. An
OPAQUE green texel expands to `0xFF00FF00`, does not match, and is left alone.
The game's only other `D3DXLoadSurfaceFromMemory` call (the GDI font upload)
passes `ColorKey = 0`, so the key belongs to the model-scene atlas alone and
`Gc2d` must not copy it.

This is load-bearing because mesh UVs are NOT clamped to their pattern rect.
IIDX 10's `samurai` maps the torii crossbeam undersides with the 8x8 `red.bmp`
swatch at `v` up to `1.13`, so the baked `v'` runs past the swatch into the
filler. Keyed, the sampler blends red toward black and the beams get the dark
shadow the game shows; unkeyed, it blends red toward `(0,255,0)` and the gates
grow bright green bands. `samurai` is the only 3D scene in the IIDX 10 and 11
installs whose atlas contains any such texel, so no other scene changed by a
single pixel. RE detail and how to re-find the blit in a new build:
`IIDX/model_scene_texture_atlas.md` in the notes repo. Covered by
`tests/formats/scene3d_atlas_tests.cpp`.

`Inz::ResolveRegion` turns a material's `TextureFilename` into the tile index
plus the scale/bias that maps the mesh's own `0..1` UVs onto the pattern's
sub-rect of that tile:

```
tile   = (x / tile_w) + tiles_per_row * (y / tile_h)
u' = (x % tile_w) / tile_w + u * (w / tile_w)
v' = (y % tile_h) / tile_h + v * (h / tile_h)
```

`Scene3d` bakes that transform into the chunk vertices at load time, which is
what the game does too (it rewrites the cloned mesh's vertex buffer once, then
draws each material subset with the whole tile bound). Chunks are therefore
grouped per MATERIAL, not per tile: two materials can share a tile and still
need different UV transforms.

Names are matched with the extension removed on both sides: the manifest drops a
trailing dotted 4-character suffix, the material drops a trailing `.bmp`.

Why it matters: when every pattern is exactly one full tile the transform is the
identity, which is why all three IIDX 18 scenes rendered correctly without it.
IIDX 10 packs several small swatches into one tile (`music`'s single 24x8
`0.gcz` holds three 8x8 flat colours) in four of its six scenes, and IIDX 17's
`boss_st` has one such material. Without the transform those materials sample
the whole tile - the "bright cyan panels" symptom. RE evidence, the full
per-scene survey, and how to re-find the code in a new build:
`IIDX/model_scene_texture_atlas.md` in the notes repo.
