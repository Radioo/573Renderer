# Animations in the round trip gate

Status: needs-info

Blocked by: 07, 08.

Extend the local round trip gate to animations.

## Acceptance

- Every `afp/<name>` entry is restored with its bsi, read into the model, written, converted back to the stored byte order, and placed into the re-encoded archive together with its generated bsi.
- The gate fails on any difference in the decoded model between the original and the round-tripped animation, naming the archive, animation and tag.
- It reports how many animations and bsi scripts come out byte-identical.
