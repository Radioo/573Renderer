# Reload a package from memory

Status: resolved

Blocked by: 12.

## Acceptance

- A package loads from IFS bytes through the in-memory mount and the normal afp-utils read path, and an animation plays from it.
- Reloading destroys the animation's layers and streams, then the package, then loads the new bytes under the same name and recreates the animation at the frame it was on.
- A `local_dll` test edits an animation with the format writers (for example its frame count or a label), reloads it, and checks the change through afp state, not pixels.
- Reloading many times in one process leaks no package ids and never shows the old content.

## Comments

2026-09-15: `AfpManager::LoadPackageFromMemory` / `ReloadPackageFromMemory` (`docs/afp_host.md`). `afp_reload_tests.cpp` passes against IIDX 33: three rounds of reloading an edited `title` animation (label renamed and moved, written by `AfpAnimation::WriteStored` and `Ifs::Write`) and the original, checking labels and the playhead through afp-core and a constant loaded package count. It uses `graphic/1/title.ifs`; `graphic/0/title.ifs` is next-version preload data that afp-utils 1.2.19 refuses.
