# Starting to animate a property a depth never set

Status: resolved

Blocked by: 45.

An owned depth could only animate the properties its placements already
carried. Ticket 45 made the matrix and colour groups' resting values known from
the game, which is what a new track needs to start from without changing what
is drawn.

## Acceptance

- An owned depth offers the matrix and colour properties it does not animate
  yet, one per component and in the long encoding, and in a 3D span only the
  colours, since the game does not apply the 2D matrix there.
- Starting one adds a track with one keyframe at the identity on the depth's
  first frame, so the export is unchanged until that keyframe is edited, and
  from then on the game draws what the keyframes say.
- Properties outside those groups are not offered, because their resting values
  are not known from the game yet.
- Tested under `ci`.
