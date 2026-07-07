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
R573_SDVX_DIR / R573_DDR_DIR) set.

Contracts covered:
- bm2dx qpro pattern-scan: `QproDll::Read` on the real bm2dx.dll - parses ok,
  >= 447 heads, first head is qp_kihon, every head is a `qp_*.ifs` name.
- game profile auto-detection on the real installs (slug + legacy_afp).
- avs2-core boot + a real IFS parse: DllLoader + AvsFuncs resolve, AVS boots,
  MountFsRoot + MountIfsImage a real IFS at /afp/packages, then
  CountExpectedTextures == CountMatches("texturelist/texture") ==
  GatherMatchAttr("name") count, every name non-empty, ReadAtlasFilters values
  in the D3D range - the PropertyTree wrapper contract against the real
  property engine.

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
