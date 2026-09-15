# Preview host protocol (`r573_preview_protocol`)

The IFS editor never loads the game DLLs itself (ADR 0005). It drives a
preview host process through this protocol: FlatBuffers messages over a
Windows named pipe (ADR 0007).

## Messages

`src/preview/preview_host.fbs` defines both directions. A request is a
`RequestMessage` holding one of:

| Request | Fields |
|---|---|
| `Boot` | game install directory, build slug |
| `LoadPackage` | package name, animation name, the IFS bytes, whether this replaces the loaded package |
| `SelectAnimation` | animation name |
| `Seek` | frame |
| `Resize` | viewport width and height |
| `Render` | none |

A reply is a `ReplyMessage` holding `Done`, `Loaded` (frame count and labels),
`Frame` (the shared texture handle as a u64, its size, the frame drawn) or
`Failure` (a message). Replies are read with
`flatbuffers::GetRoot<PreviewProtocol::ReplyMessage>`, since the schema's root
type is the request. A receiver checks untrusted buffers with the generated
verifiers before reading them.

## Channel (`preview/preview_channel.h`)

Each message travels as a little-endian u32 length and then the message.
`PreviewChannel::Send` and `Receive` move whole messages over a pipe opened
for overlapped I/O, so a read can time out (`CancelIoEx` stops the pending
read). Messages above 256 MB are refused on both sides, so a corrupt length
never allocates. A peer that closed the pipe is reported as "the pipe was
closed".

- `Listen(name)` creates the single server instance of `\.\pipe\<name>` and
  rejects remote clients; `Accept(server, timeout)` waits for the client.
- `Connect(name, timeout)` retries while the pipe does not exist yet or is
  busy, so the editor can start the host and connect straight away.
- `Client::Call(request, name)` sends one request and waits for its reply. On
  a closed pipe or a timeout, the error names the request that was never
  answered, and `LastRequest()` keeps it, so the editor can say which edit
  crashed or hung the host.

Tests: `tests/preview/preview_channel_tests.cpp` (`ci`) sends a 5 MB package
request and a labelled reply over a real named pipe inside the test process,
and checks the timeout, a closed peer, a refused oversize length and the
client naming the unanswered request.

## The host executable (`preview_host`)

`preview_host <pipe name>` links `r573_afp_host` and `r573_preview_protocol`
and nothing from the renderer's app or GUI. It creates the pipe, waits for
the editor, then answers one request at a time until the pipe closes, when it
unloads and exits with code 0.

`PreviewHost::Session` (`src/preview/host/`) handles the requests:

| Request | What the host does | Reply |
|---|---|---|
| `Boot` | resolves the build slug to its AFP config and game profile, finds and loads the DLLs under the install (`EngineDlls`), boots avs2-core, creates a hidden window and a D3D9Ex device at the profile's render size, boots afp-core | `Done` |
| `LoadPackage` | `LoadPackageFromMemory` the first time, `ReloadPackageFromMemory` after that, so the playhead survives an edit | `Loaded` |
| `SelectAnimation` | `SwitchAnimation` | `Loaded` |
| `Seek` | `SeekFrame` on the root clip | `Done` |
| `Resize` | drops the shared texture so the next render makes one at the new size | `Done` |
| `Render` | draws one frame at the game's render size, copies it into the shared texture (scaled when the viewport size differs) | `Frame` |

`Loaded` carries the root clip's frame count and labels read back from
afp-core. `Frame` carries the root clip's current frame. A request the host
cannot serve gets a `Failure` naming what went wrong. Any request other than
`Boot` gets one until a boot succeeds.

Test: `tests/local/preview_host_process_tests.cpp` (`local_dll`) starts
`preview_host.exe`, boots IIDX 33, sends `graphic/1/title.ifs` as bytes,
checks the frame count and `loop` label, seeks to frame 300, resizes to
640x360, renders, opens the shared texture on its own D3D9Ex device, then
closes the pipe and checks that the host exits with code 0.
