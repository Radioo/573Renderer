# Replacing images with pictures

Status: resolved

Blocked by: 85.

Replacing an image took the chosen file's bytes as the texture blob, so a PNG
put back after editing broke the image.

## Acceptance

- An image's pixels are replaced by a picture of the same size in the entry's
  own storage, and a different size is refused. Tested under `ci`, and seen to
  fail with plain storage or without the size check.
- The package menu replaces an image from a picture, and saving it afterwards
  gives the new picture back. Tested in `editor_window_tests`, and seen to fail
  when images take the raw replace.
