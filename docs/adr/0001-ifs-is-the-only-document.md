---
status: superseded by ADR-0006
---

# The IFS is the editor's only document

The IFS editor opens and saves IFS files directly and has no project file of its own. Authoring helpers such as applying an ease or retiming a range write their result into the package data once, and keep no live curve or setting anywhere else. We chose this over an After Effects style project file that exports to IFS, because a second file drifts out of sync with the IFS it was built into, and because it makes opening a shipped game file the same operation as opening your own work. If the package format turns out unable to store something users need, revisit this with the specific cases in hand rather than adding a sidecar file quietly.
