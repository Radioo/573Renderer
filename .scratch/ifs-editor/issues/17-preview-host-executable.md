# Preview host executable

Status: resolved

Blocked by: 13, 15, 16.

## Acceptance

- A separate executable links `r573_afp_host`, serves the protocol, and never links Qt.
- A `local_dll` test launches it, loads an IIDX 33 package from bytes, seeks, renders and opens the shared texture from the test process.

## Comments

2026-09-15: `preview_host` (`src/preview/host/`, `docs/preview_host.md`). `preview_host_process_tests.cpp` passes against IIDX 33: boot, load `graphic/1/title.ifs` from bytes, seek, resize, render, open the shared texture from the test process, and a clean exit when the pipe closes. DLL discovery and loading moved from the family backend into `r573_afp_host` (`EngineDlls`) so the host can share it. Building it exposed that `ComPtr`'s move assignment never compiled (it took the address through its own `operator&`); `com_ptr_tests.cpp` covers the fix.
