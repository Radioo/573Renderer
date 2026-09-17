# Local render-regression net

`tools/local/render_regression.py` automates the standing byte-compare
discipline the refactor uses (the "stash-dance"): run
the reference scenarios, SHA-256 every dumped frame and encoded output, and
compare against a locally blessed baseline. It is the P10 "one command on your
machine validates" precursor - the L3/L4 suites the hosted CI can never run
because it needs the game DLLs, game data, and a real GPU.

## Scenarios

| Name | Needs env var | What it runs | Outputs hashed |
|---|---|---|---|
| `sdvx_select_bg_vi` | `R573_SDVX_DIR` | 12-frame 1080x1920 export of `select_bg_vi.ifs` with `--export-bg 32,64,96 --export-crop 100,200,400,300` (exercises the bg-composite + crop frame path) | 12 `.bgra` + `out.webp` |
| `iidx_02005` | `R573_IIDX_DIR` | 12-frame export of `data/graphic/02005.ifs` (modern draw path, transparent bg) | 12 `.bgra` + `out.webp` |
| `ddr_bg_0009` | `R573_DDR_DIR` | `--ddr-test` on `background_0009.arc`, capture range 0-11 (legacy backend, clip-mask path) | 12 `seq_*.png` |

Scenarios whose env var is unset are SKIPPED with a loud line - never
silently. A scenario with no baseline entry reports `NO BASELINE` and asks for
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

The baseline (`local_baselines/render_regression.json`) is MACHINE-LOCAL and
gitignored: the hashes derive from this machine's game-data version, GPU, and
driver, so they are not portable and must never be committed. Re-bless after:

- an INTENDED rendering change (verify it first against the live game per
  CLAUDE.md - the baseline records intent, it does not define correctness),
- a game-data update under one of the R573 dirs,
- a GPU/driver change that alters output bytes.

Temp render output goes to a `%TEMP%` work dir (deleted afterwards);
`renderer.log` lands in the repo root, which is gitignored.

## Relation to the other nets

- The GPU-less CI recorder gate (`command_stream_tests`, docs/command_stream.md
  step 3) locks the RECORDER's decisions in hosted CI with no DLLs.
- This tool locks the PIXELS + ENCODED BYTES locally with real DLLs.
- The stash-dance (build old -> dump -> pop -> build new -> dump -> SHA) is
  still the recipe when comparing UNCOMMITTED work against a prior tree state;
  this tool replaces it for the common "did my change perturb rendering at
  all" check against the blessed build.

## The local_dll contract suite (P10)

`local_dll_tests` (tests/local/dll_contract_tests.cpp) runs assertions against
the REAL game DLLs and data - the contracts our unit tests and parsers assume.
It carries the ctest label `local_dll`, which the CI workflow excludes
(`ctest -LE local_dll` in build-renderer.yml); every test SKIPs cleanly when
its R573_*_DIR env var is unset, so the target is safe to build everywhere.

Run: `ctest --test-dir build -L local_dll` with R573_IIDX_DIR (and optionally
R573_SDVX_DIR / R573_DDR_DIR / R573_IIDX10_DIR) set.

Contracts covered:
- IIDX 10 screen presets (tests/local/iidx10_screen_data_tests.cpp, R573_IIDX10_DIR):
  every animation a `sprite.animate` clip of a built-in IIDX 10 document names
  exists in its package, and the one frame count the documents bake in - the game
  over document's `length` of 180 - equals the real animation's length. The game
  reads that at runtime, so this is what keeps the built-in documents
  (`src/preset/defaults/`) honest. The game over test names
  `data/graph/sys/gameover` / `GAMEOVER` as a literal because the document cannot:
  that banner is classified chrome in docs/preset_layers.md, so no clip draws it,
  yet its length is where the screen's 180 frames come from. No built-in declares a
  `loop_start` / `loop_end` range, so there is no loop range to check against the
  data; the M8 rewrite of this file dropped a loop over card-in loop ranges that
  had always iterated zero times.
- bm2dx qpro pattern-scan: `QproDll::Read` on the real bm2dx.dll - parses ok,
  >= 447 heads, first head is qp_kihon, every head is a `qp_*.ifs` name.
- game profile auto-detection on the real installs (slug + legacy_afp).
- avs2-core boot + a real IFS parse: DllLoader + AvsFuncs resolve, AVS boots,
  MountFsRoot + MountIfsImage a real IFS at /afp/packages, then
  CountExpectedTextures == CountMatches("texturelist/texture") ==
  GatherMatchAttr("name") count, every name non-empty, ReadAtlasFilters values
  in the D3D range - the PropertyTree wrapper contract against the real
  property engine.
- avs2-core writers (tests/local/avs_writer_contract_tests.cpp, R573_IIDX_DIR):
  avs2-core 2.17's cstream compressor (operator 1, resolved by ordinal in the
  test) must produce the same bytes as `AvsLz77::Compress` on synthetic
  inputs from 1 byte to 300 KB, after a known-answer check that the DLL path
  really compresses. Its binary property writer (`property_create`,
  `property_insert_read`, `property_part_write` by ordinal) must write back
  exactly what `BinaryXml::Write` produced for a document holding every
  storable type and arrays, in both name forms.

The suite has caught two real defects: (1) DDR auto-detection
relied on "mdx" appearing in the install PATH - the live install moved to
a folder without it and detection silently failed; AutoDetect now falls back to
looking for the profile's own GAME DLL (bm2dx.dll / soundvoltex.dll /
gamemdx.dll / gdxg.dll) in the same candidate dirs DiscoverDllDir probes, which
is the install's real identity rather than a folder-name heuristic. (2)
IfsInspect::AtlasFilter carried never-populated width/height fields
(always 0) - removed.

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

Before trusting a pass, the gate was run with a one-byte pixel corruption
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

`tests/local/document_edit_tests.cpp` is the proof that an edit made through
the editor's own writers is what afp-core ends up running. It opens
`graphic/1/title.ifs` as a `Document::File`, loads it in a real preview host,
then moves the placement live at frame 300 on the first depth that covers it and
adds a label `edited` at frame 100. After `WriteAnimation` and `Encode` the
package is loaded again with the reload flag, and the check is afp-core's own
answer: the frame count is still 840 and the labels come back as `edited` at 100
and `loop` at 240, in frame order. Pixels are never compared; the engine's label
list is the ground truth.

The same case then undoes the edit through `Document::History` and loads the
restored document again, which is the proof that undo reaches the engine and not
just the model: afp-core goes back to the one `loop` label at 240, and the
history reports the document as saved again because the undo landed back on the
depth it was opened at.

A second case in the same file adds a camera on frame 300 with `AddCamera`, sets
its projection centre and focal length, reloads and renders. afp-core takes the
package, keeps the frame count and renders the frame, and the tag reads back
with the focal length that was set. What it deliberately does not check is the
picture: a camera tag updates a stored camera without making it the one the
movie draws with, so the frame is unchanged, and asserting on pixels here would
be asserting on something the tag does not control. `Core/afp_format.md` in the
notes repo has the reader, the camera list and which function picks the active
camera.

## Own and detach against the install (`local` label)

`placement_span_survey_tests` (tests/local/placement_span_survey_tests.cpp)
walks every animation in the install, and does two jobs. It measures what a
per-frame placement actually carries over a span, which is what the shape of
`Document::AuthoredDepth` was decided from, and it then owns every span and
detaches it again and requires the clip to come back identical.

It owns spans in the root and in every sprite, and reports the two separately.
On IIDX 33 it owns 209251 root spans and 610965 sprite spans, 99.2% of the
826810 in the install, and gets every one of them back identical. The rest are
refused for a stated reason: filters (4737), a character swapped mid-span (1640)
and deformation curves (217). Before tickets 44 and 45 it refused another 69174
spans whose updates used different control bits.

For every span it owns it also replays the shipped placements with the game's
rule (`Document::ReplayDepth`) and compares each frame with what the keyframes
say (`Document::KeyedState`): 35291707 root frames and 93222908 sprite frames,
with no disagreement. That comparison is what shows own records what the game
draws, including the 135000 or so updates that reset a matrix part and the
375758 that reset a colour.

That second half is the proof behind ticket 30's byte-for-byte line, and it is
what found the three things own was dropping: the non-presence flag bits, the
extended flag word and the frames whose update sets no property. Each showed up
as a count of differing spans, and the test reports which member of the
placement differed so the next fix does not have to be guessed. It is a `local`
test because it needs the install and takes minutes; `document_tests` covers the
same operations on clips built in code.

## Where an animation's content lives (`local` label)

`afp_clip_nesting_survey_tests` (tests/local/afp_clip_nesting_survey_tests.cpp)
counts how much of every animation sits in its root and how much in its
sprites, where sprite definitions sit among the root's frames, how sprite label
tables are ordered, and how many sprites have cameras, labels and export names.
It is what made sprites the next thing the editor had to reach: on IIDX 33, 69%
of all placements are inside sprites. It is also what found that 145900 of
171786 sprite definitions sit inside root frame 0, which is the reason
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

The whole numbers are the point. The scale is a `/ 1024` divisor taken from the
notes, and a wrong divisor would give 29110 ragged fractions rather than four
exact rates, so the survey is what turns a documented constant into a checked
one. Run it against another build before trusting playback there, because the
count of animations storing a float is the thing that could change.

## The texture list shape (`local` label)

`texture_list_shape_tests` (tests/local/texture_list_shape_tests.cpp) reads every
`tex/texturelist.xml` in the install and pins the shape an `image` node has, so
the editor can write a new one that matches. It also measures how the images sit
in their atlas, which is where the editor's packer got its rules: 11433 of 12522
atlases are powers of two on both sides, no image falls outside its atlas or
overlaps another, no coordinate is odd, and every atlas holding more than one
image has a pair touching with no gap. That last one is why the editor packs
tight and puts the guard pixel inside the image. It is what found that an image
carries `uvrect` as well as `imgrect`, and what the inset between them is: of
106372 images, 106370 order the children `uvrect` then `imgrect` and 2 the other
way, and 106322 have `uvrect` inset one pixel inside `imgrect` while 50 have the
two equal.

Writing an image without that `uvrect` would have looked correct in every test
that only reads back what the editor wrote, which is exactly why the shape is
measured against the shipped data instead.

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

The proof that such a shape draws is in `local_dll_tests`:
`image_shape_live_tests.cpp` places the largest image of `graphic/1/title.ifs`
on a new depth over frames 0 to 20 with `PlaceImage`, loads the package before
and after in a real preview host at 1920x1080, and reads frame 10 back. Some
pixels inside the image's rectangle at the stage origin must change and none
outside it. Ending the depth at frame 5 instead makes it fail with no pixel
changed, which is how the check was shown to see the drawing. This is a
content-drawn check between two different packages, not a playback state check.

## The script walker over every script in the install (`local` label)

`afp_script_survey_tests` (tests/local/afp_script_survey_tests.cpp) reads every
`AP2_DO_ACTION` tag and every placement clip action in all 6146 IFS files,
walks the bytecode with `AfpScript::Read`, and requires two things: that no
script fails to read, and that every one of them writes back byte for byte. It
also pins the numbers the repo notes record, so a reader change that quietly
loses a call shows up as a count that moved.

Result on IIDX 33 (2026-09-16): 6146 files, 463562 scripts, none unreadable,
none rewritten. The opcodes are the eight the notes list, and the calls are all
on one built-in object, `aeplib`: `aep_set_set_frame` 413915,
`aep_set_rect_mask` 55808, `deepGotoAndPlay` 9966, `aep_set_frame_control`
6236, `gotoAndPlay` 3323, `stop` 263, `deepStop` 117, `gotoAndStop` 20.

The gate earned its keep immediately: the first run reported 442107 scripts
rewritten, which is how the padding after `END` was found.

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
`SharedTexture::Reader` on a second device, which is exactly what the editor
does with a frame the preview host rendered. The `local_dll` host process case
runs the same reader against a real rendered frame and requires it to be more
than zeros, so a host that answers with an empty texture fails.

## The real-data format cases ([real] tag)

`formats_tests` carries five cases tagged `[real]` that decode REAL game
data instead of synthetic fixtures: `tests/formats/model3d_real_tests.cpp`
(the IIDX 18 mode_bg scene: inz manifest + gcz tiles + xfile models, keyed
by `R573_IIDX18_DIR`) and four package sweeps in
`tests/formats/sysidx_tests.cpp` (`R573_IIDX17_DIR` sirius index
invariants, `R573_IIDX13_DIR` blowfish texture path, `R573_IIDX11_DIR`
unencrypted red packages, `R573_IIDX09_DIR` big-endian 9th-style chunks).
Each SKIPs cleanly when its env var is unset or the directory is missing,
so they are registered with ctest everywhere (they show as Skipped in CI)
and only assert on a machine with the dumps. They were previously hidden
`[.real]` tags, which Catch's test discovery never registers - a manual-only
path that silently returned green without data; the SKIP form replaced it
so a data-less run is visibly a skip, not a pass.

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
reviewed by eye, which is the other half of the check the tool cannot make. A non-zero exit means at least one
preset was built from a screen's INIT and misses the per-frame update that drives
its models: see `docs/game_profiles.md` and the `game-scene-preset` skill.
