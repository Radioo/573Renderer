# Local render-regression net

`tools/local/render_regression.py` automates the byte-compare the refactor
checks every render change with (the "stash-dance"): it runs the reference
scenarios, takes the SHA-256 of every dumped frame and encoded output, and
compares them against a locally blessed baseline. It is the precursor of P10's
"one command on your machine validates", covering the L3/L4 suites that hosted
CI can never run because they need the game DLLs, game data and a real GPU.

## Scenarios

| Name | Needs env var | What it runs | Outputs hashed |
|---|---|---|---|
| `sdvx_select_bg_vi` | `R573_SDVX_DIR` | 12-frame 1080x1920 export of `select_bg_vi.ifs` with `--export-bg 32,64,96 --export-crop 100,200,400,300` (exercises the bg-composite + crop frame path) | 12 `.bgra` + `out.webp` |
| `iidx_02005` | `R573_IIDX_DIR` | 12-frame export of `data/graphic/02005.ifs` (modern draw path, transparent bg) | 12 `.bgra` + `out.webp` |
| `ddr_bg_0009` | `R573_DDR_DIR` | `--ddr-test` on `background_0009.arc`, capture range 0-11 (legacy backend, clip-mask path) | 12 `seq_*.png` |

A scenario whose env var is unset is skipped, and the tool prints a line
saying so. A scenario with no baseline entry reports `NO BASELINE` and asks for
a bless.

## Usage

```
set R573_SDVX_DIR=<sdvx7 install>
set R573_IIDX_DIR=<iidx33 install>
set R573_DDR_DIR=<ddr world install>
python tools/local/render_regression.py --bless   (once, on a known-good build)
python tools/local/render_regression.py           (after every render-path change)
```

Exit 0 = every hash matches. Exit 1 = any mismatch / missing output /
renderer failure / nothing ran. `--bless` refuses to record while a scenario
is failing.

## Baseline semantics

The baseline (`local_baselines/render_regression.json`) is machine-local and
gitignored: the hashes depend on this machine's game-data version, GPU and
driver, so they are not portable and must never be committed. Re-bless after:

- an intended rendering change (verify it against the live game first, per
  CLAUDE.md; the baseline records what was intended and does not define what
  is correct),
- a game-data update under one of the R573 dirs,
- a GPU/driver change that alters output bytes.

Temp render output goes to a `%TEMP%` work dir (deleted afterwards);
`renderer.log` lands in the repo root, which is gitignored.

## Relation to the other nets

- The GPU-less CI recorder gate (`command_stream_tests`, docs/command_stream.md
  step 3) locks the recorder's decisions in hosted CI with no DLLs.
- This tool locks the pixels and encoded bytes locally with real DLLs.
- The stash-dance (build old -> dump -> pop -> build new -> dump -> SHA) is
  still the way to compare uncommitted work against a prior tree state; this
  tool replaces it for the common "did my change perturb rendering at all"
  check against the blessed build.

## Running and reading the net

- A scenario that fails with a DLL-load or missing-file error (for example
  "Failed to load libavs-win64.dll error 126") instead of a hash mismatch
  almost always means the install moved or was reorganised. Check the
  R573_*_DIR path before reading your diff, then re-bless against the new
  location.
- For the stash-dance, run `build.bat` from PowerShell (`cmd /c .\build.bat`);
  invoked through a POSIX shell's cmd wrapper its output is swallowed.
- IIDX 33 CLI runs that pass a relative `--ifs` wait for the background IFS
  scan (`WaitForIfsScan`), which takes minutes on a cold disk cache. Give such
  runs a generous timeout (the harness allows the IIDX scenario 600 s) or pass
  an absolute path.
- `--ddr-test` never reaches the modern HSL or additive-coverage shader arms
  (legacy DDR HSL is the separate `Cb_SetFilter` path).

## What the byte-compare cannot see

- Only one known asset uses the modern HSL (YIQ hue) shader path: the IIDX 33
  Q-pro hand `qp_21chronicle_07_hand` (its rainbow blade). The headless Q-pro
  composite of that item writes a near-blank AVIF, so a pixel compare there
  proves nothing. When moving code in the HSL and additive-coverage arms,
  prove equivalence by diffing the lifted device calls against the previous
  source instead.
- `LayerCommand` fires for no known content (docs/command_stream.md); verify a
  change to it by source diff plus a no-perturbation byte-compare.

## The local_dll contract suite (P10)

`local_dll_tests` (tests/local/dll_contract_tests.cpp) checks the contracts
our unit tests and parsers assume against the real game DLLs and data. It
carries the ctest label `local_dll`, which the CI workflow excludes
(`ctest -LE local_dll` in build-renderer.yml); every test skips cleanly when
its R573_*_DIR env var is unset, so the target is safe to build everywhere.

Run: `ctest --test-dir build -L local_dll` with R573_IIDX_DIR (and optionally
R573_SDVX_DIR / R573_DDR_DIR / R573_IIDX10_DIR) set.

Contracts covered:
- IIDX 10 screen presets (tests/local/iidx10_screen_data_tests.cpp, R573_IIDX10_DIR):
  every animation a `sprite.animate` clip of a built-in IIDX 10 document names
  exists in its package, and the one frame count the documents bake in (the game
  over document's `length` of 180) equals the real animation's length. The game
  reads that length at runtime, so this test is what keeps the built-in
  documents (`src/preset/defaults/`) in step with the data. The game over test names
  `data/graph/sys/gameover` / `GAMEOVER` as a literal because the document cannot:
  that banner is classified chrome in docs/preset_layers.md, so no clip draws it,
  yet its length is where the screen's 180 frames come from. No built-in declares a
  `loop_start` / `loop_end` range, so there is no loop range to check against the
  data; the M8 rewrite of this file dropped a loop over card-in loop ranges that
  had always iterated zero times.
- SDVX 1 TXP2 clips (tests/local/ddr_txp2_live_tests.cpp, R573_SDVX1_DIR): the
  32-bit `ddr_local_tests` target (built only by `build32.bat`, it links the
  renderer's `r573_app`) boots SDVX 1's own libavs / libafp, loads
  `data/graphics/bg_space.bin` through the TXP2 path, and requires the four clip
  names and a readable playhead on each one, the first clip included
  (docs/x86_build.md, "SDVX 1 (BOOTH)").
- SDVX 1 package reloads (tests/local/ddr_txp2_reload_live_tests.cpp, R573_SDVX1_DIR):
  `ddr_reload_local_tests` loads every package in turn through `DdrAfp::LoadTxp2`
  and plays it for 30 frames (docs/x86_build.md, "A package's bytes outlive its
  streams").
- SDVX 1 re-placement survey (tests/local/judge_succession_survey_tests.cpp,
  R573_SDVX1_DIR): `judge_succession_survey_tests` prints how new objects relate to
  removed ones over every clip (docs/judge.md, "Objects that are replaced instead
  of moved").
- SDVX 1 interpolation (tests/local/judge_interp_live_tests.cpp, R573_SDVX1_DIR): the
  32-bit `judge_local_tests` target boots SDVX 1's libafp with the judge's detours,
  opens `bg_space.bin` / `bg_space` on two layers and, over ten logic steps, requires
  the authored layer to draw the same pixels at 0 and at half a step after the
  advance while the interpolated layer draws different ones every time
  (docs/judge.md).
- bm2dx qpro pattern-scan: `QproDll::Read` on the real bm2dx.dll parses ok,
  finds >= 447 heads, the first head is qp_kihon, and every head is a
  `qp_*.ifs` name.
- game profile auto-detection on the real installs (slug + legacy_afp).
- avs2-core boot + a real IFS parse: DllLoader + AvsFuncs resolve, AVS boots,
  MountFsRoot + MountIfsImage a real IFS at /afp/packages, then
  CountExpectedTextures == CountMatches("texturelist/texture") ==
  GatherMatchAttr("name") count, every name non-empty, ReadAtlasFilters values
  in the D3D range. This is the PropertyTree wrapper contract checked against
  the real property engine.
- avs2-core writers (tests/local/avs_writer_contract_tests.cpp, R573_IIDX_DIR):
  avs2-core 2.17's cstream compressor (operator 1, resolved by ordinal in the
  test) must produce the same bytes as `AvsLz77::Compress` on synthetic
  inputs from 1 byte to 300 KB, after a known-answer check that the DLL path
  really compresses. Its binary property writer (`property_create`,
  `property_insert_read`, `property_part_write` by ordinal) must write back
  exactly what `BinaryXml::Write` produced for a document holding every
  storable type and arrays, in both name forms.

The suite has caught two real defects. (1) DDR auto-detection relied on
"mdx" appearing in the install path; the live install moved to a folder
without it and detection silently failed. AutoDetect now falls back to
looking for the profile's own game DLL (bm2dx.dll / soundvoltex.dll /
gamemdx.dll / gdxg.dll) in the same candidate dirs DiscoverDllDir probes,
which identifies the install by its contents instead of its folder name. (2)
IfsInspect::AtlasFilter carried width/height fields that were never filled
in (always 0); they were removed.

Not covered here (needs a D3D device + full AFP boot): afp-core playhead /
stream semantics. Those stay on the render-regression net above.

## The IFS round trip gate (`local` label)

`ifs_round_trip_tests` (tests/local/ifs_round_trip_tests.cpp) runs every
`.ifs` under `R573_IIDX_DIR/data` through the IFS editor's writers. It needs no
DLL and carries the ctest label `local`, which neither CI nor `checks.sh`
selects (both run `-L ci`), because a full pass reads the whole install and
recompresses every texture.

```
set R573_IIDX_DIR=<iidx33 install>
ctest --test-dir build -L local --output-on-failure
```

Per archive (sources in `tests/local/ifs_round_trip_{tests,reencode,compare}.cpp`):

1. Read the archive and make a copy. In the copy, every binary XML entry is
   replaced by `BinaryXml::Write(BinaryXml::Read(bytes))`, and every texture
   image is decoded, converted to BGRA and back, and replaced by
   `TextureImages::EncodeBlob` of the result (on all hardware threads). Every
   `afp/<name>` that has an `afp/bsi/<name>` is read with
   `AfpAnimation::ReadStored` and written back with `WriteStored`, replacing
   both the animation and its byte order script. In a package with a `magic`
   file, every `geo/` shape is read and written with `Ge2dShape` in the byte
   order that file selects. Other entries are whole files and stay as they
   are.
2. Write the copy with `Ifs::Write` and read it back with `Ifs::Read`, which
   also verifies the manifest MD5.
3. Fail on any difference in decoded content against the original: header
   flags and time, manifest signature, encoding and root type, the entry tree
   (kind, name, type, time, super image and its reference, extra nodes,
   special nodes; `_info_` values are derived and only its shape is compared),
   binary XML documents as trees, images as storage form plus pixels,
   animations as their decoded models (naming the first differing tag, down
   through nested sprites), shapes as their decoded models, and whole files
   as bytes. Failures name the archive and the full entry path.

Reported without failing: archives that come out byte-identical, entries whose
re-encoded bytes differ from the originals (animations, scripts and shapes are
also listed by name), and, from a second write with every
stored offset and the tree size cleared, how many archives the packer and the
tree size formula reproduce exactly. Progress goes to stderr for every file.

To show the gate can fail, it was run with a one-byte pixel corruption
injected into `EncodeBlob` on two real archives: it failed all 66 images. The
same was done for the newer checks on three archives: a placement depth
changed in the animation writer failed all 84 affected animations, naming the
tag, and one vertex bit flipped in the shape writer failed all 2013 shapes.

Result on IIDX 33 (2026-09-15): 6194 files, 48 not IFS; all 6146 archives pass.
10533 binary XML entries and 106371 texture images (106319 LZ77, 50 raw after
the header, 2 in an uncompressed list; one texture list names an image twice)
re-encode byte for byte, and so do all 29110 animations with their byte
order scripts (3852 of them store the header background colour unswapped) and
all 273660 GE2D shapes. 6145 archives are byte-identical; the exception,
`data/sound/16030-p0.ifs`, is 4 bytes shorter than its own data offset and the
writer pads the region. Recomputing from scratch reproduces the tree size in
6145 archives and every file offset in 6062. A full pass takes about 100
minutes on a 16-thread machine.

## The edit loop against a real host (`local_dll` label)

`tests/local/document_edit_tests.cpp` checks that an edit made through the
editor's own writers is what afp-core ends up running. It opens
`graphic/1/title.ifs` as a `Document::File`, loads it in a real preview host,
then moves the placement live at frame 300 on the first depth that covers it and
adds a label `edited` at frame 100. After `WriteAnimation` and `Encode` the
package is loaded again with the reload flag, and the test asks afp-core: the
frame count is still 840 and the labels come back as `edited` at 100 and `loop`
at 240, in frame order. Pixels are never compared; the engine's label list is
the ground truth.

The same case then undoes the edit through `Document::History` and loads the
restored document again, which shows that undo reaches the engine as well as
the model: afp-core goes back to the one `loop` label at 240, and the history
reports the document as saved again because the undo landed back on the depth
it was opened at.

A second case in the same file adds a camera on frame 300 with `AddCamera`, sets
its projection centre and focal length, reloads and renders. afp-core takes the
package, keeps the frame count and renders the frame, and the tag reads back
with the focal length that was set. It does not check the picture: a camera tag
updates a stored camera without making it the one the movie draws with, so the
frame is unchanged and its pixels say nothing about the tag. `Core/afp_format.md` in the
notes repo has the reader, the camera list and which function picks the active
camera.

## Own and detach against the install (`local` label)

`placement_span_survey_tests` (tests/local/placement_span_survey_tests.cpp)
walks every animation in the install. It measures what a per-frame placement
carries over a span, which is what the shape of `Document::AuthoredDepth` was
decided from, and then it owns every span, detaches it again and requires the
clip to come back identical.

It owns spans in the root and in every sprite, and reports the two separately.
On IIDX 33 it owns all 826810 spans in the install, 211152 in roots and 615658
in sprites, and gets every one of them back identical. Before ticket 70 it
refused the 269 whose updates change deformation curves, and 94 of those also
mixed updates with and without an extended word. Before tickets 44 and 45 it
refused another 69174 spans whose updates used different control bits, before
ticket 55 another 1640 that swap their character mid-span, before ticket 56
every span whose updates change its filters (4737, which hid 52 of the curve
spans), and before ticket 61 the 600 whose filter list changes its kinds or
table lengths.

For every span it owns it also replays the shipped placements with the game's
rule (`Document::ReplayDepth`) and compares each frame with what the keyframes
say (`Document::KeyedState`): 35623952 root frames and 93902985 sprite frames,
with no disagreement. A full run reads every IFS under `data` (about 36 GB)
and takes around half an hour, printing only at the end. This comparison shows
that own records what the game draws, including the 135000 or so updates that
reset a matrix part and the 375758 that reset a colour.

The comparison backs ticket 30's byte-for-byte line, and it found the three
things own was dropping: the non-presence flag bits, the extended flag word and
the frames whose update sets no property. Each showed up as a count of
differing spans, and the test reports which member of the placement differed,
so the next fix starts from that member. It is a `local` test because it needs
the install and takes minutes; `document_tests` covers the same operations on
clips built in code.

## What an animation header holds besides its content (`local` label)

`afp_header_survey_tests` (tests/local/afp_header_survey_tests.cpp) reads every
animation under `data` and counts what surrounds the content, which is what a
new animation copies. Over IIDX 33's 29110 animations:

- Every file exports `aep_mask_dummy` and `aeplibset`, both one-frame sprites
  identical in every file: `aeplibset` places the `aeplib` import (character 2)
  at depth 0, `aep_mask_dummy` places a shape at depth 1.
- 29103 export a sprite under the movie's own name, never placed on the root;
  28945 of those are exactly the root without its sprite and shape definitions,
  with the same frame count in all 29103. Three exports is the most common count
  (23209 files).
- Imports are `aeplib` with the asset `__Packages.aeplib` (27286) or `aeplib`
  (1824) at tag 2, and the import initializers are always word 0 with `(2, 0)`.
- 1914 stage sizes, most often 260x350 (4992) and 1920x1080 (2811); 25258 files
  store the background colour byte swapped and 3852 do not, and every string
  table is scrambled; 27554 backgrounds are opaque black; 4194 roots carry
  script labels.

A second case checks how `graphic/1/title.ifs` lays those definitions out:
exports in name order (`aep_mask_dummy` 6, `aeplibset` 3, `title` 335), and root
frame 0 opening with sprite 3, the solid shape 5 and sprite 6. The survey case
reads every IFS, so a full run takes a few minutes.

## How updates use filters (`local` label)

`filter_span_survey_tests` (tests/local/filter_span_survey_tests.cpp) looks at
every span whose later placements carry a filter list, which is what own used to
refuse. Over IIDX 33 there are 4755 of them, and every one has filters on its
first placement too. 3285 carry filters on every update and keep the same kinds
of filter as the first placement while their numbers change, 264 do the same on
only some updates, and 1206 change the list itself (in this count a lookup table
whose contents change also counts as a change). Across updates the colour matrix
entries that move are 0, 1, 2, 5, 6, 7, 10, 11 and 12, the RGB rows, in about
354000 updates each, entries 4, 9 and 14 (the offsets) in 8220, and the HSV
values in 100552. Update lists are one colour matrix (246960), one with HSV
(109421), or one to three lookups (109466). These counts are why filters became
a stepped track and own stopped refusing them.

## How spans use deformation curves (`local` label)

`curve_span_survey_tests` (tests/local/curve_span_survey_tests.cpp) looks at
every span whose placements carry a curve set. Over IIDX 33's 6146 files there
are 1050 such spans, and every one has its first set on the create; 269 have
later sets. All 10460 sets have their slots packed from 0, with one curve
(10212), two (246) or three (2), and 4 to 61 points. Of the 9410 later sets,
122 repeat the first set, 7058 keep its slots and point counts and change the
values, and 2230 also change a slot's flags. None names a slot past the first
set's count or gives a slot more points than the first set did, which is the
rule `Document::CheckCurvesFit` enforces.

## Where an animation's content lives (`local` label)

`afp_clip_nesting_survey_tests` (tests/local/afp_clip_nesting_survey_tests.cpp)
counts how much of every animation sits in its root and how much in its
sprites, where sprite definitions sit among the root's frames, how sprite label
tables are ordered, and how many sprites have cameras, labels and export names.
It is why sprites were the next thing the editor had to reach: on IIDX 33, 69%
of all placements are inside sprites. It also found that 145900 of 171786
sprite definitions sit inside root frame 0, which is why
`Document::RemoveFrame` keeps definitions. It also checks the order of every
export table: all of them are in case-folded name order, and 1239 are in that
order but not in byte order, which is the order the game's symbol lookup
searches. Run it on a new build before assuming any of this still holds.

## The frame rate an animation asks for (`local` label)

`afp_fps_survey_tests` (tests/local/afp_fps_survey_tests.cpp) reads every
animation in the install and reports how its header stores a frame rate and what
rate that comes to, which is where `Document::FrameRate` got its rule. On IIDX 33
it reports 29110 animations, every one of them storing the rate as fixed point,
landing on 60, 30, 29.97 and 15, and none outside a rate anything could play at.

The scale is a `/ 1024` divisor taken from the notes. A wrong divisor would
give 29110 ragged fractions instead of four exact rates, so the whole numbers
confirm the documented constant. Run it against another build before trusting
playback there, because the count of animations storing a float could change.

## The texture list shape (`local` label)

`texture_list_shape_tests` (tests/local/texture_list_shape_tests.cpp) reads every
`tex/texturelist.xml` in the install and pins the shape an `image` node has, so
the editor can write a new one that matches. It also measures how the images sit
in their atlas, which is where the editor's packer got its rules: 11433 of 12522
atlases are powers of two on both sides, no image falls outside its atlas or
overlaps another, no coordinate is odd, and every atlas holding more than one
image has a pair touching with no gap. That last rule is why the editor packs
tight and puts the guard pixel inside the image. The test also found that an
image carries `uvrect` as well as `imgrect`, and what the inset between them is: of
106372 images, 106370 order the children `uvrect` then `imgrect` and 2 the other
way, and 106322 have `uvrect` inset one pixel inside `imgrect` while 50 have the
two equal.

An image written without that `uvrect` would pass every test that only reads
back what the editor wrote, so the shape is measured against the shipped data.

## Shipped shapes and what a new one must look like (`local` label)

`shape_geometry_survey_tests` (tests/local/shape_geometry_survey_tests.cpp)
reads every package under `data/graphic` with an `afp/afplist.xml`, every
`geo/` file its `geo` arrays list, and the animation that owns them. Over the
IIDX 33 install (2120 packages, 27878 listed animations):

- All 219739 single-texture `0x3` quads whose image is in the package's own
  list are exactly the image's `uvrect` size in pixels, with their minimum
  corner at (0, 0), and their UVs sit on the same corners as the vertices.
- The leading word of an `AP2_SHAPE` tag is 2 on every textured shape (`0x3`
  and `0x43`) and 0 on every solid one (`0x9`).
- Shape tags are in id order in every animation, 21858 of them before frame 0
  and 248469 inside root frame 0.
- Every animation's header name equals its `afplist.xml` name. The `geo`
  array is always a u16 array (type 69). For the 27550 names listed once it
  holds exactly the ids of the shape tags, sorted and without repeats. The 164
  names listed twice have one listing whose array repeats ids (163 of them out
  of order) but names the same set, and one listing with no array.

Those are the rules `ImageQuad` and `AddImageShape` follow.

`local_dll_tests` checks that such a shape draws:
`image_shape_live_tests.cpp` places the largest image of `graphic/1/title.ifs`
that fits in 400x400 on a new depth over frames 0 to 20 with `PlaceImage`, loads
the package before and after in a real preview host at 1920x1080, and reads
frame 10 back. Some pixels must change, every changed pixel must touch the
outline `Document::StageOutlines` works out for the new depth, and the changed
area must span at least half the outline each way. A second case gives the
placement a scale, a skew, a move and a rotation origin first, which checks the
outline formula against afp-core's own drawing. Ending the depth at frame 5
makes the first fail with no pixel changed, and leaving the origin out of the
formula makes the second fail with 2324 pixels outside, which shows that both
checks can fail. These checks compare drawn content between two different
packages; they do not read playback state from pixels.

`preview_host_process_tests.cpp` also checks that `Frame` reports the
1920x1080 stage of IIDX 33.

## The script walker over every script in the install (`local` label)

`afp_script_survey_tests` (tests/local/afp_script_survey_tests.cpp) reads every
`AP2_DO_ACTION` tag and every placement clip action in all 6146 IFS files,
walks the bytecode with `AfpScript::Read`, and requires that every script reads
and writes back byte for byte. It also pins the numbers the repo notes record,
so a reader change that drops a call shows up as a count that moved.

Result on IIDX 33 (2026-09-16): 6146 files, 463562 scripts, none unreadable,
none rewritten. The opcodes are the eight the notes list, and the calls are all
on one built-in object, `aeplib`: `aep_set_set_frame` 413915,
`aep_set_rect_mask` 55808, `deepGotoAndPlay` 9966, `aep_set_frame_control`
6236, `gotoAndPlay` 3323, `stop` 263, `deepStop` 117, `gotoAndStop` 20.

The first run reported 442107 scripts rewritten, which is how the padding after
`END` was found.

It also measures the source language both ways: every script is turned into
source and compiled again, and the bytes are compared with the original. All
463562 scripts here, and all 211761 in SDVX 7's 935
files, read back and recompile identically, and the test asserts all three
counts as well as printing them. Point `R573_IIDX_DIR` at an SDVX install
to run it over that corpus instead; the two counts baked in are IIDX 33's, so
the script and call totals report a mismatch there while the source columns
stay exact.

## The document model against a shipped package (`local` label)

`document_outline_local_tests` (tests/local/document_outline_tests.cpp) opens
`R573_IIDX_DIR/data/graphic/1/title.ifs`, builds a `Document::Outline` and
checks the parts a synthetic package cannot prove: that the outline reports no
problems for a real package, that the animation listed as `title` is named and
described with 840 frames and the `loop` label at frame 240, and that the first
texture describes as `argb8888rev` with a non-zero width. It needs no DLL.

## The shared texture reader (`local` label)

`shared_texture_tests` (tests/local/shared_texture_tests.cpp) needs a GPU but no
game: it fills an offscreen render target on one D3D9Ex device, publishes it
with `SharedFrame::Copy`, and reads the pixels back through
`SharedTexture::Reader` on a second device, the same steps the editor takes
with a frame the preview host rendered. The `local_dll` host process case
runs the same reader against a real rendered frame and requires it to be more
than zeros, so a host that answers with an empty texture fails.

## The real-data format cases ([real] tag)

`formats_tests` carries five cases tagged `[real]` that decode real game
data instead of synthetic fixtures: `tests/formats/model3d_real_tests.cpp`
(the IIDX 18 mode_bg scene: inz manifest + gcz tiles + xfile models, keyed
by `R573_IIDX18_DIR`) and four package sweeps in
`tests/formats/sysidx_tests.cpp` (`R573_IIDX17_DIR` sirius index
invariants, `R573_IIDX13_DIR` blowfish texture path, `R573_IIDX11_DIR`
unencrypted red packages, `R573_IIDX09_DIR` big-endian 9th-style chunks).
Each one skips cleanly when its env var is unset or the directory is missing,
so they are registered with ctest everywhere (they show as Skipped in CI)
and only assert on a machine with the dumps. They used to be hidden
`[.real]` tags, which Catch's test discovery never registers, so they could
only be run by hand and silently passed without data. With SKIP, a run
without data is reported as skipped.

## The AFP builtin name table

`tools/local/afp_builtin_names.py` reads afp-core's own builtin name table out
of the DLL and writes `src/formats/afp_script_names_data.h`, which is what lets
the script language spell `getInstanceAtDepth` instead of `builtin_0x465`:

```bash
uv run afp_builtin_names.py --dll "<game>/modules/afp-core.dll" --names 0x... --index 0x... --blocks 0x...
```

Run it from `tools/local`, which is a uv project so `pefile` is there without
installing anything. The three addresses are the blob of names, the slot table
and the block table, read off the name lookup in a fresh IDB; `docs/formats.md`
says how to find that function without an offset, and what the tables mean. The
tool writes nothing unless the nine names that were known by hand come back
right, so a wrong address leaves the header alone.

The table describes the bytecode format and does not change per build, so it
only needs rereading when a DLL adds names. After rereading it, run the script
survey below over both installs: a name that changed id would show up there as a
script that no longer recompiles to its own bytes.

## Every script reads back as source

`tests/local/afp_script_survey_tests.cpp` has one case per install
(`R573_IIDX_DIR` and `R573_SDVX_DIR`). Each reads every script in every `.ifs`,
turns it into source, compiles that source again and compares the bytes:

```bash
R573_IIDX_DIR=<iidx33 install> R573_SDVX_DIR=<sdvx7 install> ./build/afp_script_survey_tests.exe
```

Both cases require every script to be sourced, every one to recompile, and none
to differ. IIDX 33 is 6146 files and 463562 scripts, SDVX 7 is 935 files and
211761 scripts, and the IIDX case also re-measures the opcode and call counts
the notes record. Every change to the source language must pass this gate,
so a shape that loses a byte fails here before it reaches a package somebody
edited.

## Scene preset sweep

`tools/local/preset_sweep.py` renders every built-in preset of a build, at every
marker and in every option state, and fails if any of them shows a model whose
transform never changes:

```bash
python tools/local/preset_sweep.py iidx11 <iidx-red-dir> --frames 120
```

It takes the preset list from the renderer's own document dump
(`--preset-dump-defaults`, shared with the CI gates through
`tools/ci/preset_dump.py`; `--dump <dir>` reuses an existing one), so it needs a
built `bin/573Renderer.exe`. Shot names come from the document: one per marker
(rendered at the marker frame plus 60) and one per choice of the first option,
passed as `--preset-option <option-id>=<label>`. It prints the current preset and
state as it goes, and writes one PNG per state into `screenshots/` (named
`<preset>-state<N>-<marker>.png` when the preset has options) so every state can be
reviewed by eye, since the tool cannot judge the pictures itself. A non-zero
exit means at least one preset was built from a screen's init and misses the
per-frame update that drives its models: see `docs/game_profiles.md` and the
`game-scene-preset` skill.
