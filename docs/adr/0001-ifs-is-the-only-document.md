---
status: superseded by ADR-0006
---

# The IFS is the editor's only document

The IFS editor opens and saves IFS files directly and has no project file of its own. Authoring helpers such as applying an ease or retiming a range write their result into the package data once, and keep no live curve or setting anywhere else. We chose this over an After Effects style project file that exports to IFS for two reasons: a second file drifts out of sync with the IFS it was built into, and without one, opening a shipped game file is the same operation as opening your own work. If the package format turns out to be unable to store something users need, revisit this decision with the specific cases in hand instead of quietly adding a sidecar file.
