# General guidelines

- Never use em-dashes.
- NO COMMENTS in tracked source. CI enforces it (`tools/ci/check_no_comments.py`, exemptions only in `tools/ci/no_comments_exempt.json`). Put RE facts and design rationale in the relevant `docs/*.md` file in the SAME change; docs/ is the single source of truth. See docs/comment_migration.md.
- No code file may be longer than 1000 lines.
- Debug artifacts NEVER go in tracked paths. Debug screenshots and captured frames go under `screenshots/` (gitignored); `*.log`, `*.mp4` and loose root-level `*.png` are gitignored too. Clean up temps before finishing.
- NEVER hardcode per-file / per-background solutions, and NEVER ship a fragile heuristic to guess what the game does. Reproduce the game's ACTUAL dynamic mechanism (RE it from the DLLs, confirm with the afp_hook on the live game). If the game varies behaviour in a way we genuinely cannot derive, expose it as an explicit option in the UI and CLI, defaulting to the game's real default. Verify a render fix against EVERY affected background, not just the one you were debugging.
- NEVER detect loop points, animation end or any playback state by COMPARING RENDERED PIXELS. Use afp's own state: `cur`/`total`/flags from `afp_get_layer_info` (modern avs2) or `afp_mc_get_param` current_frame / labels / loop_count (DDR libafp), plus the finished (`0x20000000`) / wrapped (`0x40000000`) / set_complete (`0x80000000`) latches. loop_count counts afp MASTER CYCLES (`cur >= N*total`). The only sanctioned pixel comparisons are the user-opted-in "Blend loop seam" synthesis and the DDR content-loop fallback (see docs/loop.md).
- ALWAYS report progress for anything that takes more than an instant. A determinate bar when the total is known (`X / N`), otherwise the CURRENT ITEM being processed. A spinner alone, a frozen `0/0` or a bare "Scanning..." is not enough. Background workers publish into a mutex-guarded status struct the GUI polls; long render-thread work uses the loading overlay (BeginLoad/UpdateLoadStage/EndLoad). Wire progress for a new operation in the SAME change.
- Documentation leaves pointers for re-finding code in a new game build, never raw offsets. When unsure about a feature, do more RE and search online before writing it down. A fix to AVS/AFP instrumentation needs the game's pseudocode from RE in the docs as proof.

# A MISSING LIBRARY OR QT MODULE IS SOMETHING TO INSTALL, NEVER A REASON TO PIVOT

"It is not installed" / "that plugin is not deployed" / "vcpkg did not build it" is the START of
the job, not a finding. **Install it**: add the port to `vcpkg.json` (the `editor` feature for Qt
things), `find_package` its component, link it, and deploy its plugins with
`r573_deploy_qt_plugin`. Then use it. Rebuilding vcpkg takes minutes and that is fine.
- **NEVER hand-roll a replacement for something a Qt module or maintained library already does.**
  Only a port that genuinely does not ship the thing (vcpkg's qtbase has no `qoffscreen.dll`) is a
  real constraint: say so, and pick the next real option.
- **Say what was installed and why** in the reply.
- **An icon set is a library too. NEVER type SVG path data by hand.** Vendor the real files from a
  maintained set (the editor uses Lucide under `editor/icons/`, licence beside them).

# THE EDITOR'S LOOK IS CHECKED BY LOOKING AT IT

Behaviour tests cannot see a wrong colour, a wrong font or a wrong layout, so:
- **Take a screenshot and LOOK at it after every visual change.** `build-editor/ifs_editor_shot.exe`
  builds the real window off-screen and writes a PNG into `screenshots/`. Never report a UI change
  as finished without having seen it. `editor/src/editor_theme.h` holds the colour and size tokens.
- **Looking is a check for breakage, not a new task.** If a behaviour fix shifts something by a few
  pixels, say so in one line of the reply and STOP. Do not start pixel matching unless the user
  asked for a visual change.
- **Every colour pair must clear 4.5:1.** The contrast gate (`editor/tests/editor_contrast_window_tests.cpp`
  for the editor, `tests/gui/contrast_tests.cpp` for the renderer's ImGui interface) measures every
  word the interface paints. When adding a colour, run those cases; when painting text the widget
  walk cannot see (a delegate, a custom paintEvent), add the pair to the painted-pairs case.

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
  flight, keep the latest and run it when the current one finishes.
- Anything the tests need to wait for must be visible through `Window::Loading`.

# FIXING A BUG: FAILING TEST FIRST, THEN THE FIX, THEN PROVE IT PASSES

1. **Reproduce the bug in a test, and watch it FAIL** for the reason the bug describes.
2. **Fix the app code.**
3. **Run the test again and confirm it now passes**, then run the full gate (`bash tools/checks.sh`).

The test stays and runs in CI under the `ci` label.

**Prefer a UNIT test that needs no game install, no GPU and no window.** A 2D package is a handful
of `SysIdx::Cell` and `SysIdx::Record` values, so build the failing case in code. When behaviour is
per-pixel, express the pipeline once as pure data that BOTH the renderer and the test consume (see
`GcAnim::FactorsFor` and `GcAnim::TexelDiscarded`).

**Verify the test can actually see the bug**: run it against a known-broken input and confirm it
fails there. **Reproduce the user's exact path** (same settings, entry point and format); a headless
test that takes a path the UI cannot take never executes the branch the user hits.

`editor\build.bat <targets>` builds only those targets, which works while `ifs_editor.exe` is running.

# Resolutions

When testing or debugging, ALWAYS use a proper render resolution for the game so that the content is not getting cut off.
- SDVX - 1080x1920
- IIDX 9 to 19 - 640x480
- IIDX 20 to 29 - 1280x720
- IIDX 30 and newer - 1920x1080
- DDR - 1280x720
- GD - 3840x2160
- JB (T44) - 1080x1920

# Agent docs

- Issues and specs live as markdown files under `.scratch/<feature>/`. See `docs/agents/issue-tracker.md`.
- Single-context domain docs: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
