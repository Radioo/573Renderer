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

Uncompressed format ids seen in `TexUpload` (same afp-utils table): `0x01`
i4 (1 bpp), `0x0E` rgb888 (3 bpp), `0x10`/`0x20` (a/x)rgb8888 (4 bpp),
`0x1E` la88, `0x1F` rgb565 (2 bpp).
