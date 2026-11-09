# A script editor for the IFS editor

Status: needs-triage

## The problem

An IFS carries executable bytecode in three places, and the editor can barely
touch it. A frame's `Action` tag shows as a read-only disassembly or, when it
happens to be one `aeplib` call, as a row of unlabelled argument boxes. A
placement's `clip_actions` show the same way in the inspector. A project-owned
depth gets a bare `QInputDialog` multi-line box with no highlighting, no
completion and no error reporting until you press OK.

None of that is editing code. This spec designs the thing that is.

## What already exists

This is not a green field. The parts below are built, tested, and measured.

| Piece | Where | What it does |
|---|---|---|
| Bytecode reader / writer | `formats/afp_script.h` | `Read` / `Write`, byte-exact |
| Source decompiler | `document/script_source.h` | `ScriptSourceText(animation, bytecode)` |
| Source compiler | `document/script_source.h` | `CompileScript(animation, source)` |
| Known call list | `document/script_source.h` | `ScriptCalls()`, 8 names |
| Call model | `document/library_call.h` | `ReadLibraryCall` / `WriteLibraryCall`, `ScriptListing` |
| Frame script access | `document/clip_edit.h` | `FrameScriptTag`, `EditFrameCallArgument` |
| Corpus survey | `tests/local/afp_script_survey_tests.cpp` | counts every script in an install |

The survey is the important one, because it says how much of the corpus the
language already covers. Over six IIDX 33 packages:

- 13,208 scripts, 0 unreadable, 0 that fail to write back byte for byte
- 12,288 (93%) decompile to source
- 12,288 of those 12,288 recompile to the identical instruction bytes, 0 differ

So for 93% of scripts there is already a lossless source round trip. That is the
foundation the editor should be built on, and it is why this is a UI job far more
than a compiler job.

## What "IDE" means for this language

The language is eight calls:

```
aep_set_frame_control  aep_set_rect_mask  aep_set_set_frame
deepGotoAndPlay        deepStop           gotoAndPlay
gotoAndStop            stop
```

One call per line, `name(argument, argument)`, and an argument is a number, a
`"quoted string"` or the bare word `this`. There are no variables, no control
flow, no expressions, no user functions.

A language that small will not benefit from most of what makes a premium IDE
premium. What it WILL benefit from, and what this spec is really about, is that
**every useful argument is a name that lives in the open file**: a clip
instance name, a frame label, an image name, a frame number. An editor that
knows those names, completes them, and tells you immediately when one does not
exist is worth far more here than folding, refactoring or a debugger.

So the goal is: **a real code editor surface, whose intelligence comes from the
package that is open.**

## The 7%, closed

920 of those 13,208 scripts used not to decompile: the shapes built from
`CALL_FUNCTION`, `STORE_REGISTER`, `SET_MEMBER` and `GOTO_FRAME2`, and the
calls with no trailing `POP`. They all decompile now, and the language is
total: every script in both installs reads back as source and compiles to the
same bytes.

Rather than invent syntax for a shape we had only observed, the language gained
a second layer that mirrors the instructions one for one. A call still reads as
a call; anything else reads as its instructions:

```
push(540, 1, -16382, 1, builtin_0x465)
call_function
store r1
push(r1)
call_method
pop
push(2, 0, r1)
set_member
```

That cannot be a wrong guess about semantics, because it makes no claim about
semantics. A call that leaves its result on the stack is written `keep
gotoAndStop(3)`, and a value with no shorthand is written `item(51,
4059000000000000)`, its push type and its operand bytes. Each shorthand is used
only when re-encoding it reproduces the original item byte for byte, so the
round trip is exact by construction rather than by luck.

The full contract is in `docs/document.md` under Script source.

## The widget

Three real options, all libraries, no hand-rolled text editor.

**1. Qt's own: `QPlainTextEdit` + `QSyntaxHighlighter` + `QCompleter`.**
These are the Qt classes made for exactly this, already linked, LGPL like the
rest of our Qt. We write the highlighting rules and the completion model, which
is language-specific work no library can do for us. A line-number gutter is
about 40 lines against `blockBoundingGeometry`, which is Qt's own documented
approach rather than a reimplementation of anything.

**2. QScintilla** (`qscintilla` 2.14.1 is in our vcpkg). A full editing
component: margins, folding, call tips, autocompletion lists, indicators.
**It is GPL-3.0-or-later.** Linking it makes the editor GPL-3. For a private
tool that may be perfectly fine, but it is your call, not mine, and I will not
quietly add it.

**3. KSyntaxHighlighting** (`kf5syntaxhighlighting`, MIT). A highlighting engine
driven by Kate XML definitions, plus themes. It gives no editor widget, so it
pairs with option 1 and replaces only the highlighter. It would mean authoring a
Kate XML for a language with eight keywords.

**Chosen: 2, QScintilla.** You asked for the more featured widget and said the
licence is not a concern, so the margins, folding, call tips, autocompletion
lists and indicators come from the library and none of it is hand-rolled.

## Where it appears

One widget, `Editor::ScriptEditor`, used in three places:

1. **Frame scripts.** The Inspector's Script section, which today shows argument
   boxes, becomes the editor. Clicking a mark in the scripts lane already brings
   the Inspector forward and seeks to the frame.
2. **Placement scripts.** The same section, when a depth is selected and its
   placement carries `clip_actions`.
3. **Owned depths.** Replaces the `QInputDialog` text box that
   `depth.edit_script` opens today.

A fourth surface is worth considering and I would like your view: a **dockable
Scripts panel** listing every script in the open animation (frame, clip, depth,
first line), so you can see and move between them without hunting marks. It is
the thing that would make it feel like an IDE rather than an inspector field.

## Features, in the order I would build them

Tier 1, the editor is not credible without these:

- Syntax highlighting: call names, strings, numbers, `this`, and unknown words
- Line numbers and the current-line highlight
- Compile as you type, with the error underlined on its line and the message in
  a status strip, not in a modal after you press OK
- Completion of call names, triggered on typing and on Ctrl+Space
- Save on focus loss or Ctrl+Enter, through `EditAnimation` so undo covers it

Tier 2, the part that is actually valuable here:

- Completion of ARGUMENTS from the open package: frame labels for
  `gotoAndPlay`, clip instance names for the mask and frame-control calls, image
  names where an image is meant
- A signature hint under the caret showing the argument names of the call being
  typed
- Marking an argument that names something the package does not contain, the way
  a typo in an identifier is marked in a real IDE
- Ctrl+click an argument that names a clip or a label to jump to it in the
  timeline

Tier 3, nice, not needed to ship:

- The Scripts panel above
- Find and replace across the animation's scripts
- A diff view against the original bytes before saving
- Extending the language (option B)

## Editing model and safety

The rules that keep this from destroying files:

- The editor edits SOURCE. Compiling is the only way bytes are produced.
- A script that does not decompile is shown read-only, and says why. Replacing it
  wholesale is a separate, explicit action with a confirmation, never a side
  effect of typing.
- Nothing is written until it compiles. A file is never left holding a half
  parsed script.
- Every write goes through `EditAnimation`, so history, undo and the dirty
  marker behave exactly as they do for a placement field.
- `WriteLibraryCall`'s guarantee holds: a call written back unchanged produces
  the same bytes. The survey case that proves it stays in CI.
- The window never blocks. Compiling 8 calls is microseconds, so it runs inline,
  but the completion model is built from the clip view job, off the window's
  thread, like every other panel's data.

## Theming and the look

Highlighting means new colours, and this repo fails the check gate on any text
pair under 4.5:1. So:

- The token colours are theme tokens in `editor_theme.h`, not literals in the
  highlighter
- Every one of them goes into the painted-pairs case in
  `editor_contrast_window_tests.cpp` in the same change that adds it
- The editor uses `Theme::MonoFamily()`, like every other code-ish surface
- I will screenshot it and look at it before saying it is done, per the repo rule

## Testing

- **Document layer**, no Qt: the compiler and decompiler already have tests;
  extend them for any new completion sources (`ScriptNames(animation)` returning
  the labels, clip names and images an argument may name)
- **Widget tests**: highlighting produces the expected token colours for a known
  line; the completer offers the expected names after a prefix; an error line is
  marked; the gutter numbers match the line count
- **Window tests**: opening a frame script shows its source; typing a valid
  script and committing writes it and it reads back; typing an invalid one
  refuses and leaves the file untouched; undo restores
- **Corpus**: the existing local survey already round-trips every script in an
  install. It stays the backstop, and I will re-run it against IIDX 33 and
  SDVX 7 before and after.

## Staged delivery, and where it stands

1. **Done.** `Editor::ScriptEditor`: QScintilla, a custom lexer over our
   grammar, the gutter, the caret line, wrapping, the Compile and Revert
   buttons and the status strip.
2. **Done.** Wired into the Inspector's Script section. Every script opens,
   read-write, because the language is total now.
3. **Done.** Call-name completion, fed from `Document::ScriptCalls`,
   `ScriptInstructions` and `ScriptWords` so it cannot drift from the compiler.
4. **Done.** Inside a quoted argument the list offers the package's own frame
   labels and instance names, and a quoted name the package does not hold is
   painted as wrong.
5. **Not built.** Placement scripts (`clip_actions`) and the project-owned depth
   editor still use the older surfaces.
6. **Not built.** The dockable Scripts panel and the full window on the left of
   the canvas.

## What I am not building unless you ask

A debugger, breakpoints or stepping. Refactoring. A second language. Editing the
bytecode directly as hex. Any of it is possible; none of it is worth it for
eight calls unless you tell me otherwise.

## The questions, answered

1. The 7%: **B**, and further. The language is total over both installs.
2. The widget: **QScintilla**.
3. The Scripts panel: still open. The Inspector section is what exists today.
4. **An explicit Compile button**, plus Ctrl+Return.
