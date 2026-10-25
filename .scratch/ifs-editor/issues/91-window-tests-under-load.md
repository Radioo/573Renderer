# Window tests failing on a busy machine

Status: resolved

Blocked by: 19.

The window tests failed now and then right after a build, with scripts that
never finished, and passed when run again.

## Acceptance

- The failure is reproduced on purpose: with every core kept busy by other
  processes, the suite failed four runs out of four, each time a script that
  opens a file dialog or renders through the host running past the ten second
  bound.
- With the bound at a minute, the same four loaded runs pass, and no wait in the
  suite depends on the bound being short.
