# Keyframes and easing

Status: ready-for-agent

Blocked by: 29.

## Acceptance

- A property of an authored depth is a list of keyframes, each on a whole
  frame, with an ease between one keyframe and the next.
- Sampling a property at a frame is a pure function of its keyframes, shared by
  export and by anything that draws the curve, so the two cannot disagree.
- Keyframes can be added, moved, retimed and removed, and a property with one
  keyframe holds that value for the whole range.
- Which eases exist is decided before any are exposed, and each is written down
  with what it computes.
- Tested under `ci` over the model.
