# Render into a shared texture

Status: resolved

Blocked by: 14.

## Acceptance

- The library renders a frame at a given size into a D3D9Ex render target texture created with a shared handle, without a visible window.
- A second D3D9Ex device (in a test, standing in for another process) opens the handle and reads back the same pixels.
- Resizing recreates the texture and hands out the new handle.

## Comments

2026-09-15: `SharedFrame::Create` / `Copy` in `r573_afp_host` (`docs/afp_host.md`). `shared_frame_tests` (label `local`) fills the host's offscreen target, copies it, opens the handle on a second D3D9Ex device and reads the fill colour back; a resize hands out a new handle. The window stays hidden. The copy waits for the GPU through a lockable 1x1 surface; without that wait the reader saw zeros.
