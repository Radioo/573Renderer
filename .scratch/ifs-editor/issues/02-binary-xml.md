# Binary XML tree reader and writer

Status: resolved

Blocked by: the binary XML format being documented from avs2-core.

Read a binary XML document into a node tree (name, type, value bytes, attributes, children) and write a tree back.

## Acceptance

- Every node type avs2-core defines reads and writes, including arrays and attributes.
- Compressed and uncompressed name forms both read; the writer picks the form avs2-core picks.
- A document read and written again is byte-identical, including strings whose bytes are invalid in the declared encoding.
- Malformed input returns an error instead of reading out of bounds.

## Comments

2026-09-15: `src/formats/binary_xml*.{h,cpp}`; known-answer tests in `tests/formats/binary_xml_tests.cpp`; avs2-core writes back our output byte for byte in `tests/local/avs_writer_contract_tests.cpp`. Real game documents are exercised by the round trip gate (05).
