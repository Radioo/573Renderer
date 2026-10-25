# Playing the animation

Status: resolved

Blocked by: 23.

The editor can only be scrubbed a frame at a time. An animation editor whose
animation cannot be watched is not one, and every judgement a user makes about
easing, timing and a loop needs to be made at speed rather than frame by frame.

The rate is in the animation, not in a setting: the header's `fps` is
`s32 / 1024` when the header flags carry `0x2`, and a float otherwise. Measured
over IIDX 33: 29110 animations, all of them fixed point, landing on 60, 30,
29.97 and 15, and none outside a rate anything could play at.

## Acceptance

- Play and pause, with looping the user can turn off, reachable from a menu and
  from the space bar.
- Playback runs at the animation's own rate, and an animation whose stored rate
  is not a rate anything could play at falls back rather than stalling or
  running away.
- Where playback is and when it ends comes from the frame count afp reported,
  never from comparing rendered frames.
- The timeline playhead follows playback.
- The inspector is not rebuilt per frame while playing, since reading the
  animation back for every frame is what a scrub can afford and playback cannot.
- Playback stops when the animation changes, when the document closes and when
  an edit is made.
- The rate, the fallback and the stepping are model functions tested under `ci`,
  with the shipped distribution measured by a `local` survey.
