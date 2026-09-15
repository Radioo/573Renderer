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

- Every codec is a pure function over byte spans in the stdlib-only `r573_formats` module, tested with synthetic fixtures under the `ci` label.
- A local test under the `local` label runs the round trip over every IFS below `R573_IIDX_DIR/data`, reporting the entry-level pass count and the whole-file identity count.
- A local test under the `local_dll` label compares the binary XML and LZ77 writers against avs2-core's own writers on the same inputs.

### Out of scope

- Restoring hashed entry names to logical names, MD5 name hashing for new entries, atlas packing, PNG import: those arrive with the first feature that adds new entries.
- Any UI.

## Tickets

- `issues/01-avs-lz77-module.md`
- `issues/02-binary-xml.md`
- `issues/03-ifs-container.md`
- `issues/04-texture-images.md`
- `issues/05-round-trip-gate.md`
- `issues/06-avs2-writer-comparison.md`
