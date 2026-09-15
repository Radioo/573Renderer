# Local render-regression net

`tools/local/render_regression.py` automates the standing byte-compare
discipline the refactor uses (the "stash-dance"): run
the reference scenarios, SHA-256 every dumped frame and encoded output, and
compare against a locally blessed baseline. It is the P10 "one command on your
machine validates" precursor - the L3/L4 tier the hosted CI can never run
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

## The local_dll contract tier (P10)

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

The tier has caught two real defects: (1) DDR auto-detection
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
   `TextureImages::EncodeBlob` of the result (on all hardware threads). Other
   entries are whole files and stay as they are.
2. Write the copy with `Ifs::Write` and read it back with `Ifs::Read`, which
   also verifies the manifest MD5.
3. Fail on any difference in decoded content against the original: header
   flags and time, manifest signature, encoding and root type, the entry tree
   (kind, name, type, time, super image and its reference, extra nodes,
   special nodes; `_info_` values are derived and only its shape is compared),
   binary XML documents as trees, images as storage form plus pixels, and
   whole files as bytes. Failures name the archive and the full entry path.

Reported without failing: archives that come out byte-identical, entries whose
re-encoded bytes differ from the originals, and, from a second write with every
stored offset and the tree size cleared, how many archives the packer and the
tree size formula reproduce exactly. Progress goes to stderr for every file.

Before trusting a pass, the gate was run with a one-byte pixel corruption
injected into `EncodeBlob` on two real archives: it failed all 66 images.

Result on IIDX 33 (2026-09-15): 6194 files, 48 not IFS; all 6146 archives pass.
10533 binary XML entries and 106371 texture images (106319 LZ77, 50 raw after
the header, 2 in an uncompressed list; one texture list names an image twice)
re-encode byte for byte. 6145 archives are byte-identical; the exception,
`data/sound/16030-p0.ifs`, is 4 bytes shorter than its own data offset and the
writer pads the region. Recomputing from scratch reproduces the tree size in
6145 archives and every file offset in 6062. A full pass takes about 100
minutes on a 16-thread machine.

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
