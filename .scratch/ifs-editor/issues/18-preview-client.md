# Client for the preview host

Status: resolved

Blocked by: none.

The editor side of the protocol, with no Qt in it.

## Acceptance

- Starting a client launches `preview_host` on a pipe name unique to the process, connects, and reports a host that never connects.
- One call per request: boot, load or reload a package from bytes, select an animation, seek, resize, render. Replies come back as plain structs (frame count and labels, the shared texture handle and frame).
- A `Failure` reply becomes an error carrying the host's message and the request name; a dead or hung host becomes an error naming the request that was outstanding.
- Destroying the client closes the pipe, waits for the host to exit and kills it if it does not.
- A `local_dll` test drives a real host through the client: boot, load, seek, resize, render, open the shared texture, then shut down.
