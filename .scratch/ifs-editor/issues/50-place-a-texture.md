# Placing a package texture as a new character

Status: resolved

Blocked by: 47.

A depth can only place a character the animation already defines, so a texture
in the package cannot be put on stage unless some shape already draws it.

## What was tried first and why it does not work

An `AP2_IMAGE` tag (flags 0, the texture's name as the texture list writes it,
which is how all 140 shipped image tags name theirs) was added to the root and
placed on a new depth. The package loads and renders in the preview host, but
frame 10 is pixel for pixel the same with and without the new depth, for two
different textures. No shipped placement references an image id at all, while
621469 placements reference shapes. That route was removed.

## What was found

- afp-core formats `<header name>_shape<id>` for an `AP2_SHAPE` tag; afp-utils
  reads `geo/<afplist name>_shape<id>` for each id in the animation's
  `afplist.xml` `geo` array, and the header name always equals the listed name.
- A shipped textured quad is the image's `uvrect` size from (0, 0), with atlas
  UVs on the same corners, draw flags `0x3`, and a shape tag word of 2.
- What the 140 image tags are for is still open; placing them is not it.

## Acceptance

- The document adds a shape for a package image (GE2D file, `geo` array entry,
  shape tag) and can place it on a new depth in one step, changing nothing when
  a step fails. Tested under `ci`.
- Adding a depth in the editor offers the package's images, and shapes are
  labelled with the image they draw.
- A `local_dll` test renders a frame with and without the new depth and
  requires pixels to change inside the quad and nowhere else.
