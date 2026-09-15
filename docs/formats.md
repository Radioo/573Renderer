# r573_formats (src/formats/)

Pure container/codec parsers (stdlib plus tl-expected and hash-library). No
AVS/AFP, no D3D9, no logging -
this module builds and unit-tests standalone (`formats_tests`, CTest label
`ci`), and everything in it is exercised by synthetic fixtures. Real game data
only enters through the `[real]` cases and the `local` / `local_dll` test
executables (docs/local_regression.md).

## DDR .arc container (`ddr_arc.h`)

DDR World (MDX) archive reader. Compressed entries go through the AVS-LZ77
codec below. Format and codec were reverse-engineered from gamemdx.dll /
libavs-win64.dll and verified byte-exact against the real game's own output.

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

## AVS-LZ77 codec (`avs_lz77.h`)

`AvsLz77::Decompress` and `AvsLz77::Compress`, shared by the arc reader and
IFS texture images.

Stream format: 4096-byte sliding window; write position starts at `0xFEE`;
the window pre-history is ZERO-filled (the game allocates the context with
calloc - back-references into untouched window bytes legitimately produce
zeros). One control byte carries 8 flags, consumed LSB-first; flag 1 =
literal byte, flag 0 = match. A match is two bytes forming
`token = (b1 << 8) | b2`: `distance = token >> 4` (12-bit),
`length = (token & 0xF) + 3`, copy source = `(write_pos - distance) & 0xFFF`,
copied byte-by-byte through the window (so overlapping matches repeat
recent output). `distance == 0` is the end-of-stream marker. The
`expected_size` argument stops decompression early once that many bytes are
out; 0 means run to the end-of-stream marker.

The compressor reproduces avs2-core's encoder byte for byte (cstream
operator 1; found through the `avs-cstream-lz77` source strings and the
cstream_create export). It is a greedy binary-tree LZSS encoder:

- Window 4096, longest match 18, a match needs at least 3 bytes. The text
  buffer is 4113 bytes; its first 17 bytes are mirrored after byte 4096 so
  comparisons never wrap.
- One binary search tree per first byte (256 roots). Inserting a position
  walks the tree comparing bytes 1..17 as a signed difference (right when
  `>= 0`), keeps the first strictly longer match on the descent path, and
  replaces the node outright on an 18-byte match. Equal-length ties are
  decided by that walk, not by distance, so the tree must be rebuilt exactly.
- Before the first item, positions `0xFEE-1` down to `0xFEE-18` are
  inserted, then `0xFEE`: that is how matches into the zero pre-history
  arise (five zero bytes compress to `00 01 22 00 00`).
- Each item clamps the match to the remaining lookahead, emits a literal
  when the match is 2 bytes or shorter, and slides the window by the item
  length, deleting the oldest position and inserting the new one. After
  input ends the window keeps sliding without new bytes, and stale bytes
  past the input still take part in tie breaking.
- The stream always ends by appending two zero bytes to the pending group
  and writing that group, even when it holds no items, so input whose item
  count is a multiple of 8 ends in `00 00 00`.
- avs2-core misbehaves on an empty input (its lookahead counter wraps);
  `Compress` returns the plain end-of-stream group `00 00 00` instead, which
  decodes to nothing.

Tests: `tests/formats/avs_lz77_tests.cpp` (`ci`) holds known-answer streams
for literals, zero pre-history, overlap and tree tie breaking, plus
round trips; `tests/local/avs_writer_contract_tests.cpp` (`local_dll`)
compresses the same inputs with avs2-core's own cstream compressor and
requires identical bytes.

## Binary XML (`binary_xml.h`)

`BinaryXml::Type` names the node type ids the code uses (`kVoid`, `kS32`,
`k3S32`, `k4U16`, `kAttribute`, ...); the full table of 56 ids and their sizes
is in `binary_xml_types.cpp`.

`BinaryXml::Read` and `BinaryXml::Write` convert between avs2-core's binary
property format and a `Document` of `Node`s, byte for byte. A node holds its
type byte (the base type id plus `kArrayFlag`), its name, its value bytes as
stored (big-endian, without length prefixes or padding; strings keep their
NUL), its attributes and its children. Values are never decoded or
re-encoded, so strings that are invalid in the declared encoding survive a
round trip.

Layout, as the writer emits it:

- Header: `A0`, signature (`0x42` sixbit names, `0x45` byte-string names),
  encoding byte, its complement, big-endian u32 node section length, node
  section, big-endian u32 data section length, data section.
- Node section: an element is its type byte, its name, one `0x2E` byte plus a
  name per attribute, its child elements, then `0xFE`; `0xFF` follows the
  root. The section is padded to 4 with zeros and its length includes the
  padding.
- Sixbit names: a length byte (1 to 36) and the characters of
  `0-9 : A-Z _ a-z` as 6-bit indices packed MSB first. Byte-string names: a
  length byte `n + 63` for 1 to 64 bytes, or a big-endian u16
  `0x8000 + n - 65` up to 4096 bytes, then the raw bytes.
- Attributes are written sorted by name bytes, and the reader sorts them the
  same way, because avs2-core pairs attribute values with names in sorted
  order.
- Data section: each element's value, then its attributes' values, then its
  children, recursively. `s8`/`u8`/`bool` values share 4-byte slots (a new
  slot is appended when the running byte count is 0, later bytes fill it even
  after other data was appended); `s16`/`u16`/`2s8`/`2u8`/`2b` share slots of
  two words the same way. `bin`, `str`, attribute values and every array are a
  u32 byte length, the bytes and zero padding to 4. Every other type is its
  fixed size, zero padded to 4. `void` and the array marker type 47 have no
  data.

The format was reversed from avs2-core's property reader and writer (source
strings `property-read-binary`/`property-write-binary`, the node type name
table next to them).

Tests: `tests/formats/binary_xml_tests.cpp` (`ci`) holds known-answer
documents for nesting, attributes, arrays, byte and word slots and long names,
plus malformed input; `tests/local/avs_writer_contract_tests.cpp`
(`local_dll`) has avs2-core read our output for every storable type in both
name forms and write it back, requiring identical bytes.

## IFS archives (`ifs_archive.h`)

`Ifs::Read` turns an IFS file into an `Archive`; `Ifs::Write` turns an
`Archive` back into a file avs2-core's `imagefs` driver mounts. The archive
keeps the header flags and time, the stored tree size, the manifest's binary
XML signature and encoding, and a tree of `Entry` values in manifest order:

- Directory: an `s32` node (its time) or a `void` node (the header time).
- File: a `3s32` node (offset, size, time) or a `2s32` node (offset, size).
  Local files carry their bytes; a file whose `u8` child `i` is non-zero
  lives in a `_super_` image (`Entry::super_index`), keeps its stored offset
  and size, and carries no bytes. Other child nodes of a file are kept
  verbatim.
- Special: every other node, kept verbatim. That covers `_info_`, `_super_`
  and any name the game's directory listing skips (a leading `_` followed by
  anything other than `A`-`H`, `_` or a digit), plus nodes with attributes or
  types imagefs rejects.

Header, big-endian: `6C AD 8F 89`, u16 flags, u16 NOT flags, u32 time, u32
tree size, u32 data offset, then a 16-byte MD5 when flags has `0x2`. The
manifest follows the header and the data region starts at the data offset.

`Read` verifies that MD5 the way avs2-core's mount does: over the region from
the end of the header to the data offset, with any bytes missing from a
truncated file counted as zeros (`ifs_digest.h`). A mismatch is an error,
because the game refuses to mount such a file.

What `Write` recomputes rather than copies:

- File placement. When every local file still has its stored size and the
  stored ranges do not overlap, files keep their stored offsets and the data
  region keeps at least its stored length (this reproduces shipped files,
  including the packer's gap filling and files with no end padding).
  Otherwise the files are packed the way the game's main packer does it:
  largest first (ties in manifest order), each placed at the first zero gap
  where a 4-aligned start fits, else appended at the 16-aligned end; the
  region ends 16-aligned. Offsets are checked in 64 bits: a stored layout
  whose ranges would pass 4 GB is laid out again, and a data region that would
  pass 4 GB makes `Write` return an error.
- `_info_` at the root: its `md5` child becomes the MD5 of the data region and
  its `size` child the region length.
- Data offset: the manifest end aligned to 16, with zero padding.
- Header MD5 (flag `0x2`): the MD5 of the manifest region from the end of the
  header to the data offset, padding included, which is what avs2-core
  verifies.
- Tree size: the larger of the stored value and the size the manifest needs:
  `52 * nodes + large values + 630` for sixbit names, or
  `56 * nodes + large values + 630 + per-name bytes` for byte-string names,
  rounded with `(size + 8) & ~7`. Large values are those longer than 4 bytes,
  counted rounded to 2 for `bin` and to 4 otherwise; per-name bytes are each
  name length (at least 8) rounded to 4. avs2-core only needs the value to be
  big enough.

Entries keep their stored node names. `Ifs::IsSpecialName` holds the listing
rule above. `Ifs::EscapeName` (`ifs_names.h`) maps
one path component to its node name the way imagefs does: letters stay, a
leading digit gains a `_`, `_` doubles, and ` $+-.:@~` become `_A` to `_H`;
any other character is refused (bytes of `0x80` and above make avs2-core read
outside its table). `Ifs::HashedName` is the escaped lowercase hex MD5 of a
logical name, which is how packages name their `tex/` images and animations.

The format was reversed from avs2-core's `imagefs` driver (source string
`vfs-driver-imagefs.c`, the driver descriptor carrying the magic
`0xA94BEE7C`, and the mount function referencing `/imgfs`, `_super_` and
`broken filetree: bad data offset(%x<%x)`). MD5 comes from the hash-library
vcpkg port.

Tests: `tests/formats/ifs_archive_tests.cpp` (`ci`) covers the packer, the
header MD5, `_info_`, stored layout reuse, repacking, super image files and the
tree size; `tests/local/ifs_round_trip_tests.cpp` (`local`, `R573_IIDX_DIR`) runs
every IFS in the install through `Read`, `Write` and `Read` again, requires
identical entries and binary XML entries that re-encode byte for byte, and
reports how many files come out byte-identical.

## Texture images (`texture_images.h`)

`TextureImages::ReadList` reads `tex/texturelist.xml` into images: name, the
texture's pixel format, and the size from `imgrect` (a `4u16` value, type 39,
holding x0, x1, y0, y1 in half pixels), plus whether the list's `compress` attribute is
`avslz`. An image's bytes live in the `tex/` entry named by
`Ifs::HashedName(image name)`.

`DecodeBlob` / `EncodeBlob` handle the three storage forms afp-utils' image
reader accepts:

- list not `avslz`: the entry is the pixels;
- `avslz`, compressed size non-zero: big-endian u32 uncompressed size, u32
  compressed size, then an AVS-LZ77 stream;
- `avslz`, compressed size zero: the same header, then the pixels as they
  are.

`EncodeBlob` keeps the storage form it decoded, and recompresses with
`AvsLz77::Compress`, which reproduces the game files byte for byte.

`PixelsToBgra` / `BgraToPixels` convert stored pixels to 8-bit BGRA and back.
Only `argb8888rev` is implemented, which is every texture IIDX 33 ships; its
bytes already are B, G, R, A (the renderer's texture callback copies them
straight into `D3DFMT_A8R8G8B8`). Other formats return an error naming the
format.

Tests: `tests/formats/texture_images_tests.cpp` (`ci`); the round trip gate
(`tests/local/ifs_round_trip_tests.cpp`) decodes and re-encodes every texture
image in the install.

## AFP byte order scripts (`afp_byte_order.h`)

IIDX 33 stores every animation (`afp/<name>`) big-endian, with its string
table scrambled, next to a byte order script (`afp/bsi/<name>`). afp-core
restores both in `afp_ext_command` op 8; `AfpByteOrder` mirrors that routine
and its inverse.

A script is an array of little-endian u16 words ending at the word `0x0000`.
Each word holds a type in bits 13-15, loops in bits 7-12 and a skip in bits
0-6. The cursor starts at byte 0. A word first advances it by `skip * 2` bytes.
Type 0 then advances it by `loops * 256` more bytes; types 1, 2 and 3 reverse
`loops + 1` consecutive elements of 2, 4 or 8 bytes; types 4 to 7 are fatal
(`unknown byte order data type(%d).`).

- `ReadScript` decodes a script into `Swap` runs (offset, element size,
  count). `WriteScript` encodes runs the way KONAMI's converter did: adjacent
  elements of one size merge into a run no matter which field they belong to,
  a run splits at 64 elements or a change of size, a gap of up to 254 bytes
  goes into the swap word's skip, a longer gap becomes one type 0 word
  (`gap >> 8` in loops, the rest in skip), and the script ends right after the
  last swapped element. Gaps above 16382 bytes are split into several type 0
  words; no IIDX 33 file has one, so that part does not reproduce a known
  converter output.
- `Restore(stored, script)` does what op 8 does. Data whose first u32 passes
  the little-endian magic test (`(u32 ^ 0xC1D0B2FF) & 0x7F7F7F00 == 0`), or
  whose first three bytes are the old `PAF` magic (`50 46 41` or `D0 C6 C1`),
  is not swapped; otherwise the byte-swapped u32 must pass
  `& 0x7F7F7F00 == 0x41503200` (`??? this is not afp data`) and the script is
  applied. The script is only read in that case, so native data never needs a
  valid one. `PAF` data returns there. For `AP2` data with a data version (u16
  at +8) other than 1, a string table starting with `0x80` is unscrambled by
  subtracting `128 + i` from byte `i`; any other non-zero first byte is fatal
  (`afp data string buffer unusual`). The result says whether the table was
  scrambled.
- `Store(native, swaps, scramble)` is the inverse: scramble a plain table if
  asked (refused for data version 1, whose tables `Restore` never
  unscrambles), then apply the swaps.

Finders in afp-core: the swap routine references `no change byte order info`,
`unknown byte order data type(%d).` and `??? this is not afp data`; the string
routine references `afp data string buffer unusual`; the op 8 dispatcher is the
export whose switch logs `%s(%d) unknown command`.

Tests: `tests/formats/afp_byte_order_tests.cpp` (`ci`).

## AFP animations (`afp_animation.h`)

`AfpAnimation::Read` turns restored (native byte order, plain strings)
animation data into an `Animation` that keeps no offsets, and `Write` builds
the data again from the model while recording the byte width of every u16 and
u32 it emits. `ReadStored` / `WriteStored` wrap both with `AfpByteOrder`, so
they take and return the stored bytes and the script.

What the model holds:

- Header fields, exports, imports and the import initializer section
  (`u16, u16 count`, then entries of `u16 tag, u16 frame, u32 code offset,
  u32 code length`; entries with bytecode are refused as not modelled).
- The string table in file order, including strings nothing references
  (`aep_dummy` in almost every IIDX 33 file). Every Str field is an index into
  it. A Str is written as `(off & 0xFFFC) | (off >> 16)` and read as
  `(v & 0xFFFC) | ((v & 3) << 16)`.
- Containers: labels, script labels (container flag `0x4`), frames as
  (first tag, tag count) from the `first | count << 20` entry, and tags.
  Container flags `0x1` and `0x2` add header fields whose layout is
  unverified, so they are refused.
- Typed tags: `DEFINE_SPRITE` (121, only the flags 1, offset 8 form),
  `DO_ACTION` (122), `PLACE_OBJECT` (127), `REMOVE_OBJECT` (128), `IMAGE`
  (131), `SHAPE` (132) and `PLACE_CAMERA` (136). Any other tag is kept as its
  record bytes. The long tag record form (odd 22-bit size) is refused.
- Placements follow afp-core's read order: flag word, depth, end frame, the
  optional extended flag word, character, ratio, name, clip depth, blend,
  align to 4, 2D matrix parts, colours in both forms, the clip action block,
  the filter list, origin, origin z, host geometry id, short matrix forms,
  class name, align to 4, translation z, 3x3 matrix, HSV, then the extended
  fields (discarded words, curve set, colour controller, grid controller). A
  field is present when its optional is set, so presence bits never disagree
  with the data; `flags` and `extended_flags` hold only the bits that add no
  bytes. `extended_flags` being set is what writes the extended flag word, so
  `Write` refuses extended fields without it. The extended controller record
  (`0x40`) is refused. afp-core's reader is found by the strings
  `AFP_UNUSED_DEPTH used` and `place oblect class[%s] can not defined.`.
- Bytecode stays bytes: the `0xFF` marker, flags, the optional string list,
  then the code up to the end of its record. AP2 operands are big-endian in
  both forms, so code is never swapped.
- Filters: colour matrix (type 6, 84 bytes, or 88 with HSV) and lookup
  (`0x67`, u16 length at +6 counted from +8, table from +12) are typed; any
  other filter is kept as bytes. `Write` refuses a typed filter whose type
  byte is wrong and an unknown filter that is empty or would read back as a
  typed one, so every filter reads back as what was written.

Bytes that must be zero (alignment, string padding, tag padding) are checked
on read, and so are bytes no field accounts for, such as a clip action block
or filter list longer than its events or filters. Bytes with no known meaning (`unread_*` fields, filter heads, the
colour controller's colour) are kept as values.

Writing uses one layout: the 56-byte header plus the 4-byte import
initializer slot when there is one, exports, import headers, import entries,
the import initializer section, the root container (header, labels, script
labels, frames, tags with no gaps), then the string table, which must start
with the empty string. Every tag's
size includes its padding to 4. Offsets and sizes are all recomputed.

Unknown tags and unknown filters write fine in native order, but nothing
says which of their bytes to swap, so `Write` returns an error in
`Native::swaps` and `WriteStored` refuses them instead of guessing.

Two things in the stored form are not in the restored data, so the model
carries them in `StoredForm`:

- `strings_scrambled`: whether the table was scrambled.
- `background_colour_swapped`: the four background colour bytes at +28 are
  swapped as a u32 in most IIDX 33 files and left alone in the rest (every
  animation of the numbered song packages and `qp_*` packages at the top of
  `data/graphic`). Both forms restore to the same bytes. `ReadStored` sets it
  from whether the script swaps +28.

Tests: `tests/formats/afp_animation_tests.cpp` (`ci`); the round trip gate
(`tests/local/ifs_round_trip_tests.cpp`) reads and rewrites every animation in
the install.

## GE2D shapes (`ge2d_shape.h`)

`geo/<animation>_shape<N>` files hold the meshes `AP2_SHAPE` tags draw.
afp-core only builds that name (`%s_shape%d`, `can not find geo id [%s]`);
afp-utils loads, swaps and draws the shapes.

Byte order comes from the package, never the shape: `PackageByteOrder` reads
the package's 4-byte `magic` file. afp-utils swaps the shapes only when those
bytes are `NGPF`; for `FPGN` it does not swap, and for any other value it logs
`ngp data magic error[%x]` and does not swap either, so everything but `NGPF`
means little-endian shapes. `Read` and `Write` take that order.

Layout (offsets from the file start, 0 for an absent table):

| Offset | Field |
|---|---|
| +0 | u32 magic `GE2D` |
| +4, +8 | u32 values with no known reader, kept as `version` and `unread_value` |
| +12 | u32 file size |
| +16 | u32 flags; `0x4` adds the rect |
| +20..+28 | u16 counts: vertices, UVs, vertex colours, texture names, primitives |
| +30 | u16 with no known reader |
| +32..+48 | u32 table offsets in the same order |
| +52 | rect, 4 floats (min x, max x, min y, max y), with flag `0x4` |

Vertices and UVs are float pairs, vertex colours 4 raw bytes, the name table
u32 offsets of NUL-terminated names. A primitive is 16 bytes: kind, draw
flags, two texture indices (one byte each), u16 index count, 2 bytes with no
known reader, 4 colour bytes, u32 index array offset. The swap routine swaps
every u16/u32/float field and the index arrays, and leaves the colour table,
the primitive's bytes +0..+3 and +6..+11, the names and padding alone; the
model keeps those as bytes, so a shape reads the same in both orders.

`Shape` keeps no counts, offsets or size, and floats as raw bits. Because the
model drops the layout, `Read` refuses anything the layout would carry beyond
the tables: a non-zero offset for an empty table or index array, tables that
overlap or share bytes, and non-zero bytes that no table, name or index array
covers (padding included). `Write` lays
the file out as the converter did: header, rect, name offset table, names
(each padded with zeros to `(length + 4) & ~3`), vertices, UVs, vertex
colours, primitives, index arrays (each padded to `(2 * count + 3) & ~3`);
an empty table or index array gets offset 0.
No IIDX 33 shape has vertex colours, so their place after the UVs is the one
order that fits both the shipped files and afp-utils' own copy routine.

Finders in afp-utils: the swap routine holds the only `0x47453244` immediate
and asserts with `afpu-swap-data.c`; the package test is next to
`ngp data magic error[%x]`; the size and copy routines are
`afp_bin_geo_calc_size` and `afp_bin_geo_copy`.

Tests: `tests/formats/ge2d_shape_tests.cpp` (`ci`); the round trip gate reads
and rewrites every shape in the install.

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
