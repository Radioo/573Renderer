# The project file

Status: resolved

Blocked by: 23.

## Acceptance

- A project is a folder holding a text manifest that names the IFS it belongs
  to. The source a project owns arrives with the tickets that add it.
- A project can be created for an open document, opened on its own, and saved,
  and opening one opens its IFS.
- The manifest records the format it was written in, the target build and the
  path of the IFS, and a manifest written in a format the editor does not know
  is refused rather than guessed at.
- Writing the same project twice produces the same manifest bytes.
- An IFS still opens, edits and saves with no project, and closing a project
  leaves the IFS exactly as it was.
- Tested under `ci` over the model.
