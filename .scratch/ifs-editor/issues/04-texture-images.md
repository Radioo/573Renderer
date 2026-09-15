# Texture list and image blobs

Status: ready-for-agent

Blocked by: 01, 02.

Read `tex/texturelist.xml` into textures and images, decode image blobs to BGRA, and encode them back.

## Acceptance

- `argb8888rev` pixels decode and encode losslessly.
- `avslz` blobs keep their size header; blobs stored raw under an `avslz` list stay raw on write.
- Unsupported pixel formats are reported with the format name.
