# Placement editing

Status: resolved

Blocked by: 23.

## Acceptance

- Selecting a depth row at a frame selects the placement that is live there,
  and the inspector shows its fields with the units the format stores.
- Editable fields: translation, scale, rotate and skew (both the long and the
  short form the placement carries), the 3D matrix, translation z, colour
  multiply and add (both the four-value and the packed form), blend, HSV, the
  rotation origin, the clip depth and the instance name.
- A field the placement does not carry can be added and a field it carries can
  be dropped, because presence is what the format stores; the editor never
  writes a field the target build's afp-core would not read.
- Unknown data (an unknown tag, discarded words, a filter the reader kept as
  bytes) is shown as unknown and cannot be edited.
- The edits are functions over the model in `r573_document`, tested under `ci`,
  including that dropping a field removes exactly its bit and its bytes.
