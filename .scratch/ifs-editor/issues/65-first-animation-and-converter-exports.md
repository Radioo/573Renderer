# A package's first animation, and the converter's exports

Status: resolved, with one follow-up open

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

## Open

- The self export sprite is not updated when the root is edited, so a movie that
  attaches this one by name sees the content as it was. Deciding whether the
  editor should rewrite it from the root on every edit, and checking what reads
  it in the game, is the next step.
