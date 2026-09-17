# Matrix and colour updates reset what they do not carry

Status: ready-for-agent

Blocked by: 44.

Read in the IIDX 33 afp-core placement parser (the function that logs
`place oblect class[%s] can not defined.`): before parsing, the matrix temps are
set to the 4x4 identity and the colour temps to multiply (1, 1, 1, 1), add 0.
The parsed fields overwrite only the parts they carry. Then:

- with `0x4` and no `0x4000000`, the whole matrix is copied onto the object;
- with `0x4` and `0x4000000`, tx and ty are copied (tz and the 3x3 only when
  their own fields are present);
- with `0x8`, both colours are copied onto the object.

So an update with `0x4` that carries only a translation resets scale and
rotation to identity, and one with `0x8` that carries only a multiply resets the
add colour to zero. Nothing the update leaves out is held. Scale and short
scale fill the same matrix slots, as do rotate and short rotate, multiply and
packed multiply, and add and packed add; the later-parsed field wins.

The keyframe model treats every property as held between keyframes, which is
the opposite. Export shows it: easing a translation while a scale is held at
anything but 1 writes the in-between frames with the translation, `0x4` and no
scale, and the game draws them at scale 1.

## Acceptance

- The document can replay a depth's placements with the game's rule and say
  what the game applies on each frame, as pure data the tests use.
- A frame that writes any part of a group writes every part of that group whose
  value is not the identity, so what the game shows on every exported frame is
  what the keyframes say.
- Own records what the game shows: on a frame that resets a group, a property
  of that group it does not carry is keyed at its identity, and such a key is
  not written back unless the frame carried it.
- Unedited spans still come back byte for byte, which the `local` survey checks.
- Tested under `ci` by comparing the replayed game state of exported spans with
  the sampled keyframes on every frame.
