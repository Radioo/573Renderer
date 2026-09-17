# Choosing what a new depth places

Status: resolved

Blocked by: 27.

Adding a depth always placed character 0, so a new depth could only be pointed
at something by typing a character number into the inspector afterwards.

An animation's placeable characters are the sprites, images and shapes defined
in its root, and the characters it imports from other animations. They share
one id space.

## Acceptance

- The document lists an animation's placeable characters by id, each labelled
  with what it is: a sprite with its export name, an image with its texture
  name, a shape, or an import with its asset and the animation it comes from.
- Adding a depth asks which of them to place, and an animation that defines
  nothing placeable says so rather than placing character 0.
- Tested under `ci`.
