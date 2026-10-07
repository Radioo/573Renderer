# Preview host protocol

Status: resolved

Blocked by: none. Transport and encoding decided in ADR 0007: FlatBuffers messages, each prefixed with its length, over a Windows named pipe.

## Acceptance

- One schema defines the requests (boot a target build, load or reload a package from bytes, select an animation, seek to a frame, set the viewport size, render) and the replies (shared texture handle and frame, frame count and labels, errors naming the request).
- A small channel sends and receives whole messages over a pipe handle, with a read timeout, and reports a closed or broken pipe as an error.
- The client reports a host crash or hang together with the last request it sent.
- Unit tests round-trip every message through a real anonymous or named pipe inside one test process, under `ci`.

## Comments

2026-09-15: schema `src/preview/preview_host.fbs`, `PreviewChannel` and `Client` in `r573_preview_protocol` (`docs/preview_host.md`); `preview_protocol_tests` (`ci`) passes. The flatbuffers port needed `VCPKG_HOST_TRIPLET` in the presets so CMake finds `flatc`.
