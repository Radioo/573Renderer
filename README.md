# 573Renderer

> [!CAUTION]
> This project was 100% vibe coded. I don't know how much of it is actually correct, but it works for my use cases. It was made by constantly insisting on doing RE work and making it behave like the real game does. I made attempts to improve code quality, but I have no idea if the project is in a state that could be maintained by a human.

A standalone Windows renderer for Konami arcade animation content. It loads a
game's own engine DLLs (avs2-core / afp-core / afp-utils), mounts the game's IFS
packages and runs the game's animation engine on a D3D9 backend, so scenes
render the way they do on the real game. It can then capture them to video and
image files.

Supported games (one profile entry each, see `docs/game_profiles.md`):

- beatmania IIDX 33 (Sparkle Shower)
- SOUND VOLTEX 7 (NABLA), always rendered at its native 1080x1920 portrait
- DanceDanceRevolution World (legacy AFP 2.13.7)
- GITADORA DELTA
- jubeat (T44), native 1080x1920 portrait at 60 Hz

What it does:

- A live preview GUI (ImGui) with the features of the AFP debug viewer: seek,
  pause, labels, sub-layer visibility, variant slots and filters.
- Video export that finds loop points from afp's own state, to AVIF, WebM
  (VP9/AV1), WebP, MP4 (H.264 / HEVC-with-alpha) and PNG sequences.
- The IIDX Q-pro avatar extractor, which turns the full part library into
  web-ready assets.
- A headless CLI that does the same things, for scripts.

## Build

```
build.bat
```

The build uses CMake presets and vcpkg in manifest mode with a pinned
toolchain. `docs/build.md` has the details, including the ffmpeg feature set
and the x265-alpha overlay port.

`build32.bat` builds the 32-bit renderer for the 32-bit games. It exports
through `bin/573Encoder.exe`, which `build.bat` builds, so run both
(`docs/x86_build.md`).

## Editing a scene preset

Every shipped screen is a JSON timeline document: tracks of clips on a frame
axis. A document round-trips, so you can export one, edit it outside the app
and run it:

```
573Renderer.exe --preset-export-json iidx11 iidx11-mode-select mode.json
573Renderer.exe --preset-validate mode.json
573Renderer.exe --preset-test <game-dir> out.png 120 --preset-json mode.json --preset-option mode=EXPERT
```

A document dropped in `presets/<build>/<id>.json` next to the exe shows up in
the Screens list beside the built-in ones. `docs/preset_document.md` has the
schema, the command catalog and the registry rules, and `docs/cli.md` has the
CLI flags.

## Looking at a 2D package

This renders every animation and every named cell of a pre-AFP IIDX package to
one PNG each, so you can classify a layer by what it draws instead of by its
name:

```
573Renderer.exe --gc2d-sheet "<iidx-red-dir>/data/graph/sys/mselect" screenshots/sheet_red_mselect 4
```

The third argument is how many frames to sample per animation. A layer has to
go through this step before it can go into a scene preset:
`docs/preset_layers.md`.

## Judging interpolated animation (SDVX 1)

This shows every SDVX 1 clip twice, side by side: once as the game draws it and
once with poses interpolated between the authored frames at the monitor's
refresh rate. Each verdict is saved as soon as you give it.

```
bin\573Renderer32.exe --judge "<sdvx1-dir>"
```

`docs/judge.md` covers the keys, the verdict file and how the interpolation
works.

## The IFS editor

A separate Qt 6 application for editing the animations in an IFS package. It
shows the package, a library of characters, a stage previewed through the
game's own engine, and a timeline with keyframes and a graph editor.

```bash
editor\build.bat
```

It builds into `build-editor/` with its own vcpkg feature, so building the
renderer never builds Qt. `build-editor/ifs_editor_shot.exe` opens the same
window off-screen and writes a PNG; visual changes are checked that way
(`docs/editor.md`).

## Tests and verification

```
ctest --test-dir build -L ci          # hosted-CI suite (no game data needed)
ctest --test-dir build -L local_dll   # real-DLL contract suite (needs R573_*_DIR env vars)
python tools/local/render_regression.py   # 3-game byte-compare net (machine-local baselines)
```

Every quality rule is enforced by a script or a test; see `docs/gates.md`.

## Reading a table out of a game DLL

`tools/local/afp_builtin_names.py` writes `src/formats/afp_script_names_data.h`
from afp-core's own builtin name table, which supplies the call names shown in
the script editor. Run it from `tools/local` (a uv project, so `pefile` is
already there):

```bash
uv run afp_builtin_names.py --dll <afp-core.dll> --names 0x... --index 0x... --blocks 0x...
```

`docs/formats.md` explains how to find the three addresses in a newer DLL, and
`docs/local_regression.md` lists what to run afterwards.

## Documentation

The code has no comments by policy. All reverse-engineering knowledge and
design rationale lives in `docs/`. Start at `docs/README.md`.
