# Script source

Status: ready-for-agent

Blocked by: 32.

## Acceptance

- A script an authored depth owns keeps its source text in the project, and
  export compiles it to bytecode.
- What the source language is and which calls it can make is decided from what
  the target build's afp-core accepts, and written down before any of it is
  exposed.
- A script in baked data is still shown as an instruction list and is not
  turned into source automatically.
- Tested under `ci` over the model.
