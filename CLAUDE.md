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

# A MISSING LIBRARY OR QT MODULE IS SOMETHING TO INSTALL, NEVER A REASON TO PIVOT

"It is not installed" / "that plugin is not deployed" / "vcpkg did not build it" is the START of
the job, not a finding. **Install it**: add the port to `vcpkg.json` (the `editor` feature for Qt
things), `find_package` its component, link it, and deploy its plugins with
`r573_deploy_qt_plugin`. Then use it. Rebuilding vcpkg takes minutes and that is fine.
- **NEVER hand-roll a replacement for something a Qt module already does.** No painting icons by
  hand because Qt SVG is not linked, no writing an image codec, no re-implementing a widget, no
  "I will approximate the paths with QPainter". If Qt (or another maintained library) ships it,
  add the dependency and use the real thing.
- The same goes for platform plugins: if a plugin is missing from the build, check whether the port
  ships it and enable it; only when the port genuinely does not build it (vcpkg's qtbase has no
  `qoffscreen.dll`) is that a real constraint - say so, and pick the next real option, never a
  hand-rolled stand-in.
- **Say what was installed and why** in the reply. Do not silently reimplement.
- Burned (2026-09-20): building the editor UI I needed the design's stroke icons, found no Qt SVG in
  the build, and started writing a QPainter icon painter instead of adding `qtsvg` to `vcpkg.json`.
  The user was rightly angry. `qtsvg` + `Qt6::Svg` + the `qsvg`/`qsvgicon` plugins took one edit and
  renders the design's own SVG paths exactly.

# THE EDITOR'S LOOK IS CHECKED BY LOOKING AT IT

The IFS editor has a design plan (a Design canvas artboard set; the link lives in
`.scratch/ifs-editor-redesign/design.local.md`, which is gitignored). **Matching it is a hard
requirement, not a nice-to-have.** Behaviour tests cannot see a wrong colour, a wrong font or a
wrong layout, so:
- **Take a screenshot and LOOK at it after every visual change.** `build-editor/ifs_editor_shot.exe`
  builds the real window on the native Windows platform, off-screen and without stealing focus, and
  writes a PNG into `screenshots/` (gitignored). Read the PNG back and compare it with the artboard
  before saying anything is done.
- Read the artboard for the exact tokens (colours, px sizes, fonts, spacing) rather than guessing;
  they live in the canvas's `project/*.dc.html` files. `editor/src/editor_theme.h` holds the ones
  already lifted out.
- Never report a UI change as finished without having seen it.
- **Every colour pair must clear 4.5:1.** Text the reader has to squint at is a
  defect, not a style choice. The contrast gate (`editor/tests/editor_contrast_window_tests.cpp`
  for the editor, `tests/gui/contrast_tests.cpp` for the renderer's ImGui interface) measures
  every word the interface paints and fails the check gate below the WCAG AA bar. When adding a
  colour, run those cases; when painting text somewhere the widget walk cannot see (a delegate,
  a custom paintEvent), add the pair to the painted-pairs case in the same change.

# THE EDITOR WINDOW NEVER BLOCKS

Nothing that takes more than an instant may run on the window's thread: reading
or writing a file, parsing or encoding a package, decoding an animation or an
image, talking to the preview host, exporting. It goes on the window's thread pool
through `Editor::Jobs::Start`, and the window says what it is doing while it runs.
- **Every wait has a state.** A panel shows what it is loading, never an empty list
  that the reader cannot tell from "there is nothing here". The Busy page
  (`Editor::Busy`) covers document-wide work and carries a progress bar, a detail
  line and a Stop button where the work can be stopped.
- **Never show an empty state before the load has started.** Switch to the loading
  state first, then start the job.
- **Coalesce, never drop.** When a request arrives while the same kind of work is in
  flight, keep the latest and run it when the current one finishes. Dropping it
  leaves the screen showing something that is no longer true.
- Anything the tests need to wait for must be visible through `Window::Loading`.

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

# Resolutions

When testing or debugging, ALWAYS use a proper render resolution for the game so that the content is not getting cut off.
- SDVX - 1080x1920
- IIDX 9 to 19 - 640x480
- IIDX 20 to 29 - 1280x720
- IIDX 30 and newer - 1920x1080
- DDR - 1280x720
- GD - 3840x2160
- JB (T44) - 1080x1920

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default labels: needs-triage, needs-info, ready-for-agent, ready-for-human, wontfix. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
