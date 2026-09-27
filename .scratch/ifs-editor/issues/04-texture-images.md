# Texture list and image blobs

Status: resolved

Blocked by: 01, 02.

Read `tex/texturelist.xml` into textures and images, decode image blobs to BGRA, and encode them back.

## Acceptance

- `argb8888rev` pixels decode and encode losslessly.
- `avslz` blobs keep their size header; blobs stored raw under an `avslz` list stay raw on write.
- Unsupported pixel formats are reported with the format name.

## Comments

2026-09-15: `src/formats/texture_images.{h,cpp}` and `ifs_names.{h,cpp}`; unit tests in `tests/formats/texture_images_tests.cpp`. The raw form (compressed size 0) was confirmed in afp-utils' image reader. The gate re-encodes all 106372 IIDX 33 images byte for byte. The first gate run caught `imgrect` being a `4u16` value rather than a u16 array; a unit test now pins that.
