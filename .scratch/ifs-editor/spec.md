# IFS editor

Status: ready-for-agent

A separate Qt 6 application in this repo that opens, edits and saves IFS files, with an optional project file for authored content and a viewport drawn by the target build's afp-core in a preview host process. Vocabulary is in `CONTEXT.md`; the decisions are ADRs 0002 to 0006.

## Milestones

Each milestone is blocked by the one before it.

1. Container and textures: IFS read and write, the binary XML manifest, AVS-LZ77, texture images, and the round trip gate. No UI.
2. Lossless AFP animation reader and writer for the tags the target build's data uses, through the round trip gate.
3. GE2D shape reader and writer, through the round trip gate.
4. Preview host: the afp host code carved out of `r573_app` into a library, a host executable, IPC, ramfs reload.
5. Editor application shell (Qt 6 Widgets, docking) with the chosen layout: viewport centre, package tree left, inspector right, depth-row timeline below.
6. Placement editing, then library calls and labels, structure edits, the 3D camera.

Reverse engineering of unknown placement flags and blend values runs alongside every milestone.

## Milestone 1: container and textures

### Behaviour

- Reading an IFS yields its header fields, its manifest tree and every entry's bytes, without the game DLLs.
- Writing produces an IFS avs2-core mounts, from that same model.
- A round trip encodes every entry again through the writers, never copying original bytes through (ADR 0002). The gate is that every entry's decoded bytes and the container fields compare equal; whole-file byte identity is a tracked metric that does not block.
- Binary XML trees read and write losslessly, keeping string bytes exactly as stored even when they are not valid in their declared encoding.
- AVS-LZ77 compresses and decompresses; the compressor aims to match avs2-core's output byte for byte.
- Texture images in `argb8888rev` decode to and encode from 8-bit BGRA; `avslz` image blobs keep their header framing, and raw blobs stay raw.

### Seams and tests

- Every codec is a pure function over byte spans in the `r573_formats` module (stdlib, tl-expected, and hash-library for MD5), tested with synthetic fixtures under the `ci` label.
- A local test under the `local` label runs the round trip over every IFS below `R573_IIDX_DIR/data`, reporting the entry-level pass count and the whole-file identity count.
- A local test under the `local_dll` label compares the binary XML and LZ77 writers against avs2-core's own writers on the same inputs.

### Out of scope

- Restoring hashed entry names to logical names, MD5 name hashing for new entries, atlas packing, PNG import: those arrive with the first feature that adds new entries.
- Any UI.

## Milestone 2: AFP animations

### Behaviour

- A stored animation and its byte order script restore to native order the way afp-core's op 8 does, and a native animation stores back with a generated script.
- Restored animations read into a model with no offsets: header, exports, imports, the string table in file order, containers, and typed tags for the seven the target build uses. Anything else stays as bytes and cannot be stored, since nothing says which bytes to swap.
- Writing rebuilds every offset and records every field's byte width; the script comes from those widths with the converter's greedy rule.
- Stored details the restored data cannot show (scrambled strings, the header background colour's byte order) live in the model as an explicit stored form.
- The round trip gate re-encodes every animation and its script, fails on any model difference, and reports byte identity of both.

### Seams and tests

- `AfpByteOrder` and `AfpAnimation` in `r573_formats`, tested with synthetic fixtures under `ci`.
- The `local` round trip gate covers every `afp/<name>` with an `afp/bsi/<name>`.

## Milestone 3: GE2D shapes

### Behaviour

- A shape reads into a model with no offsets, in the byte order the package `magic` file selects, and writes back with every count, offset and size recomputed in the shipped table order.
- Fields with no known meaning keep their values; floats keep their raw bits.
- The round trip gate re-encodes every shape and fails on any model difference.

### Seams and tests

- `Ge2dShape` in `r573_formats`, tested with synthetic fixtures under `ci`.
- The `local` round trip gate covers every `geo/` entry.

## Milestone 4: preview host

### Behaviour

- IFS bytes held in memory mount through avs2-core's `ramfs` and `imagefs`, so a package loads without touching the disk.
- A loaded package reloads under the same name: layers and streams are destroyed first, then the package, then the new bytes load and the animation seeks back to its frame. afp state (frame count, labels, playhead) proves the new content is live, never pixels.
- The afp boot, mount, package, animation and D3D9 draw code the renderer already has moves into an `r573_afp_host` library that takes its session and GPU context explicitly and has no GUI, `App` or window dependency. The renderer links it and renders exactly as before.
- The library renders one frame into an offscreen D3D9Ex texture whose shared handle another process can open.
- A preview host executable boots a target build from its install directory and serves the editor over IPC: load or reload a package from bytes, pick an animation, seek to a frame, set the viewport size, and return the shared texture.

### Seams and tests

- Mount and reload are proved by `local_dll` tests against the target build.
- The carve-out is proved by the existing pixel golden and byte-compare gates staying green.
- The host protocol is tested with the host process launched by a `local_dll` test.

- The host protocol is FlatBuffers over a named pipe (ADR 0007).

## Tickets

- `issues/01-avs-lz77-module.md`
- `issues/02-binary-xml.md`
- `issues/03-ifs-container.md`
- `issues/04-texture-images.md`
- `issues/05-round-trip-gate.md`
- `issues/06-avs2-writer-comparison.md`
- `issues/07-afp-byte-order.md` (milestone 2)
- `issues/08-afp-animation-model.md` (milestone 2)
- `issues/09-afp-round-trip-gate.md` (milestone 2)
- `issues/10-ge2d-shapes.md` (milestone 3)
- `issues/11-ge2d-round-trip-gate.md` (milestone 3)
- `issues/12-ramfs-package-mount.md` (milestone 4)
- `issues/13-package-reload.md` (milestone 4)
- `issues/14-afp-host-library.md` (milestone 4)
- `issues/15-shared-texture-target.md` (milestone 4)
- `issues/16-preview-host-protocol.md` (milestone 4)
- `issues/17-preview-host-executable.md` (milestone 4)
