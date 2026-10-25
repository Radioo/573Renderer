# Saving package images

Status: resolved

Blocked by: 20.

An image could be added to a package or replaced, but not saved back out, so
editing one in another program meant finding its pixels some other way.

## Acceptance

- An image reads back as the BGRA it was added with, and an unknown name is
  refused. Tested under `ci`, and seen to fail when the blob is not decoded.
- An image whose hashed name starts with a digit can be read and removed.
  Tested under `ci`, and seen to fail first, since the path skipped
  `UnescapeName`.
- The package menu saves an image as a PNG with the same pixels. Tested in
  `editor_window_tests`, and seen to fail with the channels swapped.
