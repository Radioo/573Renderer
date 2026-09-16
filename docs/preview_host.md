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
| `ShowSymbol` | the export name of a symbol in the loaded animation |

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
| `ShowSymbol` | `AttachSymbol`: attaches the named symbol onto the root movie clip, so the root clip now plays that symbol from frame 0 | `Loaded` |

`Loaded` carries the root clip's frame count and labels read back from
afp-core. `Frame` carries the root clip's current frame. A request the host
cannot serve gets a `Failure` naming what went wrong. Any request other than
`Boot` gets one until a boot succeeds.

Test: `tests/local/preview_host_process_tests.cpp` (`local_dll`) starts
`preview_host.exe`, boots IIDX 33, sends `graphic/1/title.ifs` as bytes,
checks the frame count and `loop` label, seeks to frame 300, resizes to
640x360, renders, opens the shared texture on its own D3D9Ex device, then
closes the pipe and checks that the host exits with code 0.

## The client (`PreviewClient::Host`)

`src/preview/preview_client.h` is the editor's half of the protocol and has no
Qt in it, so it is testable and reusable outside the application.

`Host::Start(Options)` picks a pipe name unique to the process and the call
(`\.\pipe\r573_preview_<pid>_<n>`), starts `preview_host.exe` with
`CREATE_NO_WINDOW`, then connects. A host that never connects is killed and
reported as an error, so a broken build cannot leave a process behind. The
destructor closes the pipe, gives the host five seconds to exit and terminates
it if it does not.

One method per request: `Boot`, `LoadPackage`, `SelectAnimation`, `Seek`,
`Resize`, `Render`, `ShowSymbol`. Each builds its FlatBuffer, calls through
`PreviewChannel::Client` and decodes the reply into a plain struct
(`Loaded{frame_count, labels}`, `Frame{shared_handle, width, height, frame}`).
A `Failure` reply becomes an error carrying the request name and the host's
message; a hung or dead host becomes a channel error naming the request that
was outstanding, which `LastRequest()` also keeps.

`LoadPackage` takes a `reload` flag rather than deciding for itself, because
only the caller knows whether this package is already open in the host.

Test: the `local_dll` case in `tests/local/preview_host_process_tests.cpp`
drives a real host through the client end to end, and two smaller cases check
that a refused request surfaces the host's message with the request name, and
that starting against a missing executable fails instead of hanging.

## Reading the frame back (`SharedTexture::Reader`)

`src/preview/shared_texture.h` is how a process that is not the host turns a
`Frame` reply into pixels. `Reader::Create()` makes its own D3D9Ex device on
the desktop window; `Read(handle, width, height)` opens the host's texture on
that device, copies it into a `D3DPOOL_SYSTEMMEM` surface with
`GetRenderTargetData`, locks it and returns tightly packed BGRA rows. The
opened texture and the staging surface are kept until the handle or the size
changes, so scrubbing a timeline re-opens nothing.

The host's own flush is what makes this correct: `SharedFrame::Copy` blocks on
a one-pixel lockable render target after the `StretchRect`, so by the time the
editor gets the reply the shared texture holds the finished frame. A reader
that skips that flush sees an empty texture, which is what happens if a test
fills the shared texture directly instead of going through `SharedFrame::Copy`.

Tests: `tests/local/shared_texture_tests.cpp` (`local`) draws a known colour on
one device and reads it back on another, and the `local_dll` host process test
reads the real rendered frame and requires it to be more than zeros.

## Showing one symbol on its own

`ShowSymbol` is how the editor previews a sprite by itself. afp-core has a call
that makes a movie clip show a symbol of its animation in place of what it was
showing, Flash's `attachMovie` by linkage name: `AfpFuncs::afp_mc_attach_movie`,
bound at `0x06d`, `int (int mc_id, const char *lib)`. `AfpManager::AttachSymbol`
calls it on the root movie clip (`afp_mc_get_id_by_path(stream, "")`). Because
every other request already reads and drives the root clip, nothing else
changes: `Loaded` reports the symbol's frame count and labels, `Seek` moves
through the symbol's frames, and `Render` reports the symbol's frame. Selecting
or reloading the animation puts the whole animation back.

The game finds the symbol by name only, never by character id, and it looks the
name up in the animation's export table by binary search with ASCII letters
folded to lower case, then in the import table. So only a symbol with an export
name can be shown this way; `Document::PreviewSymbolFor` covers the rest by
handing the host bytes that carry an extra export name. The call is read from
the IIDX 33 and IIDX 34 afp-core, where it is the same routine at the same
number; to find it again, xref the log string `afp_play_work_attach_movie_as2`,
whose one reference is the attach routine, and whose two exported callers are
`0x06d` and `0x089` (the second takes a stream id too, to take the symbol from
another loaded animation). A build that exports by name is bound as
`afp_mc_attach_movie`, which is the name the routine's AS3 twin logs, but no
such build has been checked.

Test: `tests/local/sprite_preview_tests.cpp` (`local_dll`) loads IIDX 33's
`graphic/1/title.ifs`, picks an exported sprite whose frame count differs from
the root's, shows it and checks afp-core reports the sprite's frame count and
label count, seeks into it, renders and checks the frame, goes back to the
whole animation, shows it again by its name in upper case (the lookup folds
case, so this has to work), and checks a name nobody exports is refused. A
second case shows a sprite with no export name: the name is refused against the
shipped bytes and accepted once the host has the preview bytes, and afp-core
reports that sprite's single frame rather than the root's 840.
