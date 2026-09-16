# Export drift

Status: ready-for-agent

Blocked by: 32.

## Acceptance

- Export records the digest of every entry it wrote into the manifest.
- Opening a project whose IFS has entries that no longer match those digests
  reports them, per entry, rather than silently overwriting or silently
  keeping.
- For each such entry the user chooses between keeping the IFS version, which
  detaches the authored content in it, and exporting again.
- Tested under `ci` over the model.
