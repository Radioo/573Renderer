# Derive the live IIDX graphic folder from bm2dx.dll

Status: ready-for-agent

Blocked by: none.

IIDX 25 and later keep their 2D packages in `data/graphic/0/` and `data/graphic/1/`. Only one of them belongs to the installed game; the other holds the previous version's leftovers and preload data for the next version, which the installed afp-utils can refuse (IIDX 33's afp-utils 1.2.19 rejects `graphic/0/title.ifs` with `afpstr version[1.18.0] is too new[>1.12.255]`). File dates must never decide.

bm2dx.dll decides with a compiled-in string: one helper joins a package name with the constant `"/N/"` (IIDX 33: `"/1/"`) unless the package is top level, and the loader mounts `/data/graphic/` plus that path with no fallback to the other folder. The digit alternates by version: 24=0, 25=1, 26=0, 27=1, 28=0, 29=1, 30=0, 31=1, 32=0, 33=1. IIDX 20 to 23 have no numbered folders.

Finders:

- Any build: the DLL holds exactly one null-terminated `/N/title.ifs` string with a single digit; cross-check with `/data/info/N/music_data.bin` existing in the install.
- x64 builds, IIDX 25 to 33: the helper matches `48 83 EC 38 4D 85 C0 74 ?? 48 85 C9 74 ?? 45 84 C9 74 ?? 4D 8B C8 4C 8D 05 ?? ?? ?? ?? 48 83 C4 38 48 FF 25 ?? ?? ?? ?? 4C 89 44 24 20 4C 8D 0D ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? FF 15` exactly once. The `lea r9` at match+45 (displacement at match+48, next instruction at match+52) points at the 4-byte string `"/N/\0"`; the digit is at target+1.

Today the renderer hardcodes `graphic/1/` in `AfpManager::LoadBootIfses` (`kBootIfses` in `src/afp_packages.cpp`), which is only right for odd versions, and the content scan and IIDX test defaults can pick either folder.

## Acceptance

- A small module reads bm2dx.dll's bytes and returns the folder digit using both finders where they apply; if they disagree, or nothing matches on a build that has numbered folders, it returns an error instead of a guess. Builds without numbered folders report that.
- Every place the renderer picks IIDX graphic packages (boot package list, content scan, any title.ifs default) uses the derived folder.
- Unit tests under `ci` cover a synthetic PE with the string, with the signature, with both disagreeing, and with neither.
- A `local_dll` test checks the derived digit against the IIDX installs that are present (at least 33 -> 1 and 32 -> 0).
- The mechanism and finders are documented in `docs/`, and `bash tools/checks.sh` passes.
