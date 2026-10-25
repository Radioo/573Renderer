# Structure edits

Status: resolved

Blocked by: 24.

## Acceptance

- A depth can be added over a frame range with a chosen character and removed
  again, with the placement and remove tags landing on the right frames.
- Frames can be added to and removed from a clip, with every tag index and
  label frame that follows moved with them.
- Entries of the IFS can be added, replaced and removed, including a texture
  image, with the manifest and the texture list kept in step.
- Nothing here invents a name: a new entry is stored under the hash of the
  logical name the user gave it.
- Tested under `ci` over the model, with the round trip gate still passing on
  an untouched install.
