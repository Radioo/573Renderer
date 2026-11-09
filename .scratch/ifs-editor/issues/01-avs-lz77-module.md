# AVS-LZ77 module with compressor

Status: resolved

Move the AVS-LZ77 decoder out of the DDR arc reader into its own `r573_formats` module shared by the arc reader and IFS texture images, and add a compressor.

## Acceptance

- One module owns both directions; the arc reader uses it and keeps its behaviour.
- Decompressing the compressor's output returns the input for empty, short, repetitive, incompressible and window-boundary inputs.
- The compressor reproduces avs2-core's match choices, verified by fixtures whose expected bytes come from the documented algorithm, and later by ticket 06 against the DLL.

## Comments

2026-09-15: `src/formats/avs_lz77.{h,cpp}` holds both directions; the arc reader uses it. Known-answer and round trip tests in `tests/formats/avs_lz77_tests.cpp`; byte-identical to avs2-core in `tests/local/avs_writer_contract_tests.cpp`.
