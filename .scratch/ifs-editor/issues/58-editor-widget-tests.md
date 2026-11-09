# Testing the editor's widgets

Status: resolved

Blocked by: 54.

The viewport handles, the timeline selection and the ease curve were only
checked by building the editor.

## Acceptance

- A test executable drives the timeline, viewport and curve editor with mouse
  events on Qt's minimal platform and checks what they select and emit.
- The local gate builds the editor and runs those tests.
- The tests are shown to fail when the behaviour they cover is broken.
