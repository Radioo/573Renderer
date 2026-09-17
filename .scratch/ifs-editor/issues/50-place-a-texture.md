# Placing a package texture as a new character

Status: needs-info

Blocked by: 47.

A depth can only place a character the animation already defines, so a texture
in the package cannot be put on stage unless some shape already draws it.

## What was tried and why it does not work

An `AP2_IMAGE` tag (flags 0, the texture's name as the texture list writes it,
which is how all 140 shipped image tags name theirs) was added to the root and
placed on a new depth. The package loads and renders in the preview host, but
frame 10 is pixel for pixel the same with and without the new depth, for two
different textures. The survey agrees that this is not how shipped data draws a
picture: no shipped placement references an image id at all, while 621469
placements reference shapes. That route was removed rather than shipped.

## What needs finding out

- How afp-core's `AP2_SHAPE` resolves `<export name>_shape<id>` through the
  host geometry callback, and which package entry answers it.
- What a GE2D shape that draws one texture as a quad looks like in the shipped
  `geo/` entries, so the editor can write one.
- What the 140 image tags are used for, if not placement (attachBitmap and the
  morph shape bitmap fill are the candidates in the notes).

The live test for this ticket must compare a render with and without the new
depth, as the removed attempt did; a clean load proves nothing.
