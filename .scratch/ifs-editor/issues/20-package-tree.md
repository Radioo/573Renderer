# Package tree and inspector

Status: resolved

Blocked by: 19.

## Acceptance

- Opening an IFS reads it with `Ifs::Read` and shows its entries as a tree, with directories, files, special nodes and super-image references distinguished.
- Selecting an entry shows its details in the inspector: kind, stored size, time, and for a texture its format and pixel size, for an animation its frame count and labels.
- Animations and textures are recognised by where they sit in the package, not by guessing from bytes.
- The tree building and the inspector fields are computed by code with no Qt types in it, tested under `ci`.
