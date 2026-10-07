# Animations in the round trip gate

Status: resolved

Blocked by: 07, 08.

Extend the local round trip gate to animations.

## Acceptance

- Every `afp/<name>` entry is restored with its bsi, read into the model, written, converted back to the stored byte order, and placed into the re-encoded archive together with its generated bsi.
- The gate fails on any difference in the decoded model between the original and the round-tripped animation, naming the archive, animation and tag.
- It reports how many animations and bsi scripts come out byte-identical.

## Comments

2026-09-15: All 29110 IIDX 33 animations decode equal after a round trip and come out byte-identical with their scripts. The gate names the first differing tag and lists non-identical animations. An injected placement depth change failed all 84 affected animations.
