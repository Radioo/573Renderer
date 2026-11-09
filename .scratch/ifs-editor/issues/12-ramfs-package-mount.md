# Mount IFS bytes from memory

Status: resolved

Blocked by: the ramfs write, flag and mount details being documented for the target build.

## Acceptance

- The engine registers avs2-core's `ramfs` driver after boot when the build exports it, and maps IFS bytes held in memory onto a `ramfs` mount.
- That file mounts with `imagefs`, and its entries list and read back through avs2-core equal to the bytes written.
- Writing the same name again replaces the bytes, and the mount reflects the new content after a remount.
- Builds whose ordinal table lacks the `ramfs` exports report that the in-memory path is unavailable instead of failing boot.
- A `local_dll` test proves all of the above against the target build.

## Comments

2026-09-15: RE showed `ramfs` maps host memory as a single `image.bin` rather than storing written files, so the acceptance wording changed from "writes" to "maps" and "writing the same name again" became unmount and mount the new bytes. `AvsManager::MountMemoryIfs` / `UnmountMemoryIfs` (`docs/afp_host.md`); `memory_ifs_mount_tests.cpp` passes against IIDX 33's avs2-core, including a remount of bytes rebuilt by `Ifs::Write`.
