# A package's first animation, and the converter's exports

Status: resolved

Blocked by: 62.

A new animation needed an animation in the same package to copy, so a package
with only images could never get one, and a new animation dropped the
`aeplibset` and `aep_mask_dummy` helpers and the self export every shipped
animation carries.

## Acceptance

- What surrounds the content of a shipped animation is measured over the
  install and written down.
- A new animation keeps the template's helper sprites, the shapes they place
  with their shape files, and exports an empty self sprite, in name order, with
  later frames starting after the definitions. Tested under `ci`.
- The template can come from another package; a package without an animation
  list, `afp` or `geo` directory gets them. Tested under `ci`, and under `local`
  with a real IIDX 33 image-only package whose new animation loads in afp-core
  and draws an image placed on it.
- The editor asks for another IFS when the package has no animation. Covered by
  the window tests.
- Found on the way: list files were stored under an MD5 name like their
  siblings. Fixed and tested.

## The self export

- Nothing in IIDX 33 reads the self export sprite (every caller of afp-core's
  symbol lookup was read, and neither bm2dx nor afp-utils imports the attach
  exports), so root edits leave it as it was. This is written down, not
  changed.
- A new animation's export is inserted in afp-core's case-folded order, and a
  name that folds to a kept export is refused. Tested under `ci` with a name
  byte order would have put first.
