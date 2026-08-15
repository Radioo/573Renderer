# General guidelines

- Never use emdashes
- Write code that is easy to read and maintain
- NO COMMENTS in tracked source. The whole repo is comment-free and this is CI-enforced by `tools/ci/check_no_comments.py` (catch-all across .cpp/.h/.py/.cmake/.yml/.json/CMakeLists/.clang-*, exemptions only in `tools/ci/no_comments_exempt.json`). Any new or edited file MUST land comment-free: move unique RE facts and load-bearing design rationale into the relevant `docs/*.md` file in the SAME change (docs/ is the single source of truth). See docs/comment_migration.md.
- Separating concerns into their own independent modules is a good practice
- Any code file cannot be more than 1000 lines long
- Debug artifacts NEVER go in tracked paths. Put all debug screenshots / captured frame sequences under `screenshots/` (gitignored); `*.log`, `*.mp4`, and loose root-level `*.png` are gitignored too. Don't leave scratch files in the repo root or `src/`, and clean up temps before finishing.
- NEVER hardcode per-file / per-background solutions, and NEVER ship a fragile heuristic to guess what the game does. The renderer must reproduce the game's ACTUAL dynamic mechanism (RE it from the DLLs / confirm via the afp_hook on the live game). If the game varies behaviour per file/state in a way we genuinely cannot derive, do NOT bake in a guess - expose it as an explicit option in the UI (and CLI), defaulting to the game's real default, so the user controls it. Verify any render fix against EVERY affected background (e.g. a mask change must keep bg_0001's trail clipped AND bg_0009's bundles uncut), not just the one you were debugging.
- NEVER detect loop points, animation end, "is it done", or ANY playback state by COMPARING RENDERED PIXELS (frame-to-frame or frame-vs-frame0 MAD / bit-identical checks). ALWAYS rely on afp's OWN state as the ground truth: `cur`/`total`/flags from `afp_get_layer_info` (modern avs2) or `afp_mc_get_param` current_frame / labels / loop_count (DDR libafp), plus the finished (`0x20000000`) / wrapped (`0x40000000`) / set_complete (`0x80000000`) latches. Pixel comparison is fragile, content-dependent, and resolution/fps-dependent - it caused the bg_common loop-count bug (a pixel "visible loop" spanned 2 master cycles and doubled the output). loop_count counts afp MASTER CYCLES (`cur >= N*total`). There are two sanctioned pixel comparisons, both documented: the explicit, user-opted-in "Blend loop seam" synthesis (for backgrounds afp exposes no loop for), and the DDR content-loop fallback (a drifting scene's visible loop is a perceptual match that afp state cannot express - see docs/loop.md for why joint playhead phases cannot replace it). Everything else, including the export idle-stop, keys off afp playhead state.
- ALWAYS report progress for any operation that can take more than an instant - it must NEVER look stuck/frozen. Prefer a determinate progress bar when you know the total (e.g. `X / N`); if you don't know the total, show the CURRENT ITEM being processed (the file/dir/frame), not a bare static label. A spinner or animated indeterminate bar alone is not enough - it must be paired with a changing detail (current item, or a climbing count). NEVER show a frozen `0/0`, an empty label, or just "Scanning..." with no detail. This applies to EVERY long-running path: boot phases, directory scans, IFS/package/companion loads, texture loads, video export capture + encode, and the .arc / customize extractors. Background workers publish progress into a status struct (mutex-guarded) that the GUI polls each frame; long render-thread work uses the loading overlay (BeginLoad/UpdateLoadStage/EndLoad). When you add a new operation, wire its progress reporting in the SAME change.
- When updating documentation, don't hesitate to run extra agents to verify it is correct by doing extra RE work around the feature. When you can, leave pointers on how to find this piece of code (do not put raw offsets because they will change with every new game build). If you are unsure about a feature, leave do more RE work and search online to verify it.
- When fixing a bug with AVS/AFP instrumentation, you MUST provide proof of game pseudocode from RE to ensure the fix is correct. Attach it to docs along with a way how to find that code (remember, no raw offsets because they will change on each game version).
- Prefer existing libraries and solutions known to work as opposed to writing code that another library can provide, adding new vcpkg dependencies for this is very welcome

# FIXING A BUG: FAILING TEST FIRST, THEN THE FIX, THEN PROVE IT PASSES

Every bug fix follows this order, with no steps merged or skipped:

1. **Reproduce the bug in a test, and watch it FAIL.** Write the test before
   touching app code, run it, and confirm it fails for the reason the bug
   describes. A test that has never failed proves nothing: it may be asserting
   something that cannot break, or asserting it in a place the bug does not reach.
2. **Fix the app code.**
3. **Run the test again and confirm it now passes**, then run the full gate
   (`bash tools/checks.sh`) so the fix is checked against everything else.

The test is the deliverable, not the scaffolding. It stays, it runs in CI under
the `ci` label, and it is what stops the bug coming back.

**Prefer a UNIT test that needs no game install, no GPU and no window.** A
2D package is a handful of `SysIdx::Cell` and `SysIdx::Record` values, so build
the failing case in code. Anything that needs a real game directory cannot run in
CI and will rot. When behaviour is per-pixel, express the pipeline once as pure
data that BOTH the renderer and the test consume (see `GcAnim::FactorsFor` and
`GcAnim::TexelDiscarded`) so the test cannot drift from what the screen does.

**Verify the test can actually see the bug.** Before trusting a pass, run the
check against a known-broken input and confirm it fails there. Burned repeatedly
(2026-08-14, the export rectangle): a check scanned a single pixel column, then a
later one scanned rows for horizontal steps a full-width band cannot have. Neither
could fail, both reported success, and the user was told "fixed" three times while
the bug was untouched.

**Reproduce the user's exact path before claiming a fix.** Same settings, same
entry point, same format. That same bug survived because every headless test
passed a transparent background, a path the UI cannot take for that backend, so
the broken branch was never executed once.

# A SCENE PRESET IS A CLEAN BACKGROUND CAPTURE - NEVER A SCREEN REPLICA

Scene presets exist for ONE reason: to capture the cool BACKGROUND ANIMATION a game screen
draws, on its own. **NO UI ELEMENT MAY EVER BE VISIBLE IN A PRESET.** No titles, no
INFORMATION bars, no TIME REMAIN, no difficulty rows, no song lists, no player panels, no
prompts, no frames, no instruction text. There is no "show UI" toggle and there must never be
one again: a toggle turns shipping chrome into a user problem instead of a bug.

- **Never classify a layer from its NAME or from the disassembly.** KONAMI's `MUSIC_IN` is the
  entire music-select UI frame; `EXPERT_IN` is the course-select frame. Names lie.
- **Before ANY 2D layer goes into a preset, render it and LOOK at it:**
  `573Renderer.exe --gc2d-sheet <package-dir> <out-dir> <samples>` writes SEVERAL PNGs per
  animation, spread across its length, plus one per named cell. Then write the verdict into
  `docs/preset_layers.md`. This is enforced by `tools/ci/check_preset_layers.py`: a layer with
  no row, or a row that says `chrome`, fails the build.
- **One frame is not a classification.** A layer can be clean early and bring chrome in later.
  RED's `COURSE_DECIDE` is a plain blue flash at frame 30 and carries SELECT KEY MODE plus both
  option strips by frame 119, so a single-frame look called it clean and the caption turned up
  in a preset render afterwards. Look at every sample, and if a layer's chrome lives in the SAME
  cell as its art (RED's `EXDECIDE` has its caption printed into the hexagon field), the layer
  cannot be cleaned and does not go in a preset at all.
- **The init is not the truth - find the per-frame update.** A screen's init often sets a model
  transform that its update overwrites every frame. IIDX RED music select inits the emblem at
  (-0.58,-0.5,0) and then `sub_41C620` moves it to (-0.1,0,-0.276) on every single frame. Trace
  the update before writing any transform into a preset.
- **When a preset render is finished, LOOK at the PNG and ask "is any chrome visible?"** - not
  "did it crash". Shipping a preset whose render is 90 percent UI, having looked at that exact
  PNG, is how this rule came to exist.
- **A SCREEN IS A SEQUENCE, NOT A POSE. Reproduce the whole thing.** Screens fade in, warp in,
  settle, then change again on input or a timer. A preset that carries only the settled pose is
  the LAST FRAME of the screen, not the screen. Every state goes in `docs/preset_states.md` with
  where it came from in the game, and `tools/ci/check_preset_states.py` fails the build in both
  directions: a documented state the preset does not expose, or a state in the preset with no row.
  Phases advance on the frame counter because the game advances them; options are chosen because
  the game chooses them. A state that genuinely cannot be built yet goes in the gaps table, which
  prints on every build, so the gap is visible rather than silently absent.
- **Never let the preset FORMAT decide what the truth is.** When a screen does something the
  format cannot hold, extend the format. Do not encode the part that fits and move on. Burned
  (2026-08-14): the attract screen plays a 291-frame warp-in, rotating and zooming, with the
  models hidden before it, and the RE brief said so in the state list right below the settled
  table. The preset shipped as the settled loop alone because one pose was all the format held.
- Burned (2026-08-14): `iidx11-music-select` shipped as the whole MUSIC SELECT frame with no
  background at all, and the same class of miss put chrome in eight other presets. The user had
  already said this once, about IIDX 10 mode select, and it was fixed for that one screen
  instead of for the concept.

# Resolutions

When testing or debugging, ALWAYS use a proper render resolution for the game so that the content is not getting cut off.
- SDVX - 1080x1920
- IIDX 9 to 19 - 640x480
- IIDX 20 to 29 - 1280x720
- IIDX 30 and newer - 1920x1080
- DDR - 1280x720
- GD - 3840x2160
- JB (T44) - 1080x1920