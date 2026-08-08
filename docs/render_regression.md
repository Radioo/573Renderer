# Render regression harness (tools/render_regress)

## What it is

`make render-regress` boots every supported game headlessly, captures a fixed
set of frames, hashes them, and compares against a recorded golden set. The
whole suite is about 3 seconds with `--jobs 4`.

`make render-regress-update` re-records the goldens. Run it ONLY when a render
change is intended, and eyeball the affected PNGs under
`screenshots/regress/` before accepting the new hashes.

## Why it exists

Ad-hoc verification of "does game X still render" was being done by launching
the renderer with `--screenshot-frames N` and letting a `timeout` kill it. The
app does not exit after the last screenshot, so every single check burned the
full timeout (150s) to capture one frame that was ready after 2. Four games
meant roughly ten minutes per regression pass, which is why it kept getting
skipped or half-done.

The fix is mostly `--exit-after-frames`, which already existed: with it a case
finishes in ~2.5s. The harness wraps that up so the comparison is one command
and the reference values live in the repo instead of in a transcript.

## Adding / editing cases

`tools/render_regress/cases.json` holds ONLY machine-independent case data
and is committed. Game locations live in `tools/render_regress/game_dirs.json`,
which is GITIGNORED - copy `game_dirs.example.json` to it and fill in your own
paths. Absolute paths from a dev machine must never land in a tracked file:
they leak the owner's disk layout and make the tool work on exactly one box.
A case with no configured or no existing dir is reported SKIP, not FAIL.

- `arch` picks `573Renderer32.exe` (x86) or `573Renderer.exe` (x64).
- `ifs_candidates` are paths RELATIVE to the case's game dir. The list is
  ordered by preference, but selection is by NEWEST FILE MTIME among the ones
  that exist, not by list order. IIDX ships
  the same asset under `data/graphic/0/` and `data/graphic/1/`; those are
  versioned data drops and the newer one is the live asset.
- `frames` are the frames to capture; the run exits at `max(frames) + 5`.
- Optional `render_size` and `animation` map to the matching CLI flags.

**Use `title.ifs` for IIDX cases, not `gmframe*.ifs`** - it is both shorter and
a more involved animation, so it exercises more of the render path per second
of test time.

## What it does and does not catch

It compares whole-frame SHA-256, so it catches any pixel change and cannot
tell a meaningful regression from an intended one - that judgement is yours at
`--update` time. It is a tripwire for "this refactor silently changed
rendering", which is exactly the risk when one backend is shared across
generations (the DDR/legacy backend now serves DDR World, IIDX 20 and IIDX 24
simultaneously).

It is deliberately NOT wired into `tools/checks.sh` or CI: it needs the game
dumps, which exist only on the dev machine.
