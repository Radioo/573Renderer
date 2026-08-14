# 573Renderer

> [!CAUTION]
> This project was 100% vibe coded. I don't know how much of it is actually correct, but it works for my use cases. It was made by constantly insisting of doing RE work and making it behave like the real game does. I made attempts on improving code quality, however I have no idea if the project is in a state that could be maintained by a human.

A standalone Windows renderer for Konami arcade animation content. It loads a
game's own engine DLLs (avs2-core / afp-core / afp-utils), mounts the game's IFS 
packages, and drives the real animation engine against a D3D9 backend so that 
scenes render exactly as they do on the real game - then captures them to modern 
video and image formats.

Supported games (one profile entry each, see `docs/game_profiles.md`):

- beatmania IIDX 33 (Sparkle Shower)
- SOUND VOLTEX 7 (NABLA) - always rendered at its native 1080x1920 portrait
- DanceDanceRevolution World (legacy AFP 2.13.7)
- GITADORA DELTA
- jubeat (T44) - native 1080x1920 portrait at 60 Hz

Capabilities:

- Live preview GUI (ImGui) with the AFP debug-viewer parity features: seek,
  pause, labels, sub-layer visibility, variant slots, filters.
- Video export with afp-state loop detection: AVIF, WebM (VP9/AV1), WebP,
  MP4 (H.264 / HEVC-with-alpha), PNG sequences.
- The IIDX Q-pro avatar extractor (full part library to web-ready assets).
- A headless CLI covering the same flows for automation.

## Build

```
build.bat
```

CMake presets + vcpkg (manifest mode, pinned toolchain). Details, including
the ffmpeg feature set and the x265-alpha overlay port: `docs/build.md`.

## Looking at a 2D package

Render every animation and every named cell of a pre-AFP IIDX package to one
PNG each, so a layer can be classified by what it draws instead of by its name:

```
573Renderer.exe --gc2d-sheet "<iidx-red-dir>/data/graph/sys/mselect" screenshots/sheet_red_mselect 30
```

The third argument is the frame to sample (animations only). This is a required
step before a layer may go into a scene preset: `docs/preset_layers.md`.

## Tests and verification

```
ctest --test-dir build -L ci          # hosted-CI tier (no game data needed)
ctest --test-dir build -L local_dll   # real-DLL contract tier (needs R573_*_DIR env vars)
python tools/local/render_regression.py   # 3-game byte-compare net (machine-local baselines)
```

Every quality rule is machine-enforced; see `docs/gates.md`.

## Documentation

The code is comment-free by policy: all reverse-engineering knowledge and
design rationale lives in `docs/`. Start at `docs/README.md`.
