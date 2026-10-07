# Labels and library calls

Status: resolved

Blocked by: 23.

## Acceptance

- Labels on the timeline ruler can be added at a frame, renamed, moved and
  removed, and the string table keeps only the strings the animation uses.
- A script that is one aeplib call with constant arguments is recognised and
  edited as that call and its arguments, never as bytecode text; anything else
  is shown as an instruction list and left alone.
- The recognised call set comes from what the target build's aeplib exposes
  and what its data uses, recorded in the repo notes, not guessed.
- Reading a library call out of bytecode and writing it back is a pure
  function tested under `ci`, including that an unrecognised script survives a
  round trip byte for byte.
