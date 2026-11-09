# Adding and removing animations

Status: resolved

Blocked by: 50.

A package's animations could be edited but a new one could not be made, and
removing an animation from the package tree left its byte order script, its list
entry and its shapes behind, so the package no longer loaded.

## Acceptance

- A new animation copies the header and imports of one already in the package,
  starts with the requested number of empty frames and is listed without a
  `geo` array. Names that are taken, empty, too long for afp-utils, or not
  printable ASCII are refused. Tested under `ci`.
- afp-core loads the new animation with that frame count and draws an image
  placed on it inside its outline. Tested under `local`, and the load was seen to
  fail with the list update removed.
- Removing an animation removes its file, script, list entries and listed shapes
  together, and is refused while another animation imports it. Tested under
  `ci`; the package still loads its other animation afterwards under `local`.
- The package tree offers both, and removal is refused while the project owns
  depths in that animation.
