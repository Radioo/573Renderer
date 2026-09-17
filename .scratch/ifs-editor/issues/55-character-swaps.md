# Owning a depth that swaps its character

Status: resolved

Blocked by: 42.

1640 spans in IIDX 33 change the character they show partway through, and own
refused them.

## Acceptance

- A span that swaps its character is owned with a Character track keyed where
  the swaps are, and detaches byte for byte; a span that does not swap keeps
  its character on the baked create placement. Tested under `ci`.
- Character, Clip depth and Blend only hold: other eases are refused when set
  and when written. Tested under `ci`.
- The install survey owns the 1640 spans and still detaches every owned span
  exactly, with no frame disagreeing with the game.
