# Export

Status: ready-for-agent

Blocked by: 30, 31.

## Acceptance

- Export writes a project's authored content into its IFS as baked data: one
  placement per frame per authored depth, sampled from the keyframes.
- Export is deterministic. Exporting the same project twice produces the same
  IFS bytes, proved by a test rather than asserted.
- Export never disturbs baked data the project does not own, and the round trip
  gate still passes on an untouched install.
- The project's authored content is written into and read back from the
  manifest, so what is owned survives closing the editor. Ticket 30 holds it
  only in memory on purpose, because the persisted form has to agree with what
  export writes.
- Tested under `ci` over the model.
