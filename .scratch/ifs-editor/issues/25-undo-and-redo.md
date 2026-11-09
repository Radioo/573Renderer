# Undo and redo

Status: resolved

Blocked by: 23.

## Acceptance

- Every edit goes through one place that records it with a name a user
  recognises ("Move depth 4", "Add label loop").
- Undo and redo restore the document and run the reload loop, so the viewport
  follows.
- The stack holds documents rather than widget state, and its size is bounded.
- Tested under `ci` by comparing models after an edit, an undo and a redo,
  with no window involved.
