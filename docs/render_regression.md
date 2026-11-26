# Render regression harness (tools/render_regress)

## What it is

`make render-regress` boots every supported game headlessly, captures a fixed
set of frames, hashes them, and compares the hashes with a recorded golden
set. The whole suite takes about 3 seconds with `--jobs 4`.

`make render-regress-update` re-records the goldens. Run it only when a render
change is intended, and look at the affected PNGs under
`screenshots/regress/` before accepting the new hashes.

## Why it exists

Checking whether game X still renders used to mean launching the renderer with
`--screenshot-frames N` and letting a `timeout` kill it. The app does not exit
after the last screenshot, so every check waited out the full timeout (150s)
to capture one frame that was ready after 2. Four games meant roughly ten
minutes per regression pass, so the pass kept getting skipped or done halfway.

Most of the fix is `--exit-after-frames`, which already existed: with it a
case finishes in ~2.5s. The harness wraps that up so the comparison is one
command and the reference values live in the repo instead of in a transcript.

## Adding / editing cases

`tools/render_regress/cases.json` holds only case data that does not depend on
the machine, and is committed. Game locations live in
`tools/render_regress/game_dirs.json`, which is gitignored: copy
`game_dirs.example.json` to it and fill in your own paths. Absolute paths from
a dev machine must never land in a tracked file, because they leak the owner's
disk layout and make the tool work on exactly one machine. A case with no
configured dir, or a dir that does not exist, is reported SKIP, not FAIL.

- `arch` picks `573Renderer32.exe` (x86) or `573Renderer.exe` (x64).
- `ifs_candidates` are paths relative to the case's game dir. The list is
  ordered by preference, but the file actually used is the one with the newest
  mtime among those that exist, whatever its place in the list. IIDX ships
  the same asset under `data/graphic/0/` and `data/graphic/1/`; those are
  versioned data drops and the newer one is the live asset.
- `frames` are the frames to capture; the run exits at `max(frames) + 5`.
- Optional `render_size` and `animation` map to the matching CLI flags.

Use `title.ifs` for IIDX cases, not `gmframe*.ifs`. It is shorter and a more
involved animation, so it exercises more of the render path per second of
test time.

## What it does and does not catch

It compares whole-frame SHA-256, so it catches any pixel change and cannot
tell a meaningful regression from an intended one; you make that call at
`--update` time. It is a tripwire for a refactor that silently changes
rendering, which is the main risk when one backend is shared across
generations (the DDR/legacy backend now serves DDR World, IIDX 20 and IIDX 24
at the same time).

It is deliberately not wired into `tools/checks.sh` or CI, because it needs
the game dumps, which exist only on the dev machine.
