# GE2D shape reader and writer

Status: resolved

Blocked by: 03.

Read `geo/<animation>_shape<N>` files into a model with no offsets and write them back, in the byte order the package's `magic` file selects.

## Acceptance

- The package `magic` file decides the byte order: `NGPF` bytes mean big-endian shapes, `FPGN` bytes mean little-endian; anything else is an error.
- Header, rect, vertices, UVs, vertex colours, texture names, primitives and index arrays read into structures. Floats keep their raw bits. Fields with no known meaning keep their values.
- Writing rebuilds every count, offset and the size field in the shipped table order, with the shipped padding.
- Malformed input returns an error instead of reading out of bounds.

## Comments

2026-09-15: `Ge2dShape` (`src/formats/ge2d_shape.*`). Following afp-utils, only `NGPF` selects big-endian; `FPGN` and every other magic mean no swap. `Read` refuses bytes and offsets the model would drop.
