# AFP animation reader and writer

Status: needs-info

Blocked by: 07.

Read native-order AFP animation data into a lossless model and write it back.

## Acceptance

- Header, optional slots, export and import tables, string table, tag containers, frame and label tables and tag records read into structures, including nested sprite containers.
- The seven tags the target build's data uses get typed models: sprite, script, placement, remove, image, shape, camera. Any other tag is kept as its record bytes.
- Placement fields follow afp-core's read order and scales; flag bits whose meaning is unverified keep their bytes and are reported as unknown data.
- Scripts and clip action blocks keep their bytecode as bytes in this ticket.
- Writing rebuilds every table and offset from the model, never by copying original byte ranges.
- Malformed input returns an error instead of reading out of bounds.
