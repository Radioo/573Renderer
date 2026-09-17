# PNG and JPEG images could not be added

Status: resolved

The package menu offers to add an image and its file dialog filters for
`*.png *.bmp *.jpg`, but qtbase was built with only `gui` and `widgets`, so Qt
read only BMP, PPM, XBM and XPM. Adding a PNG said it was not an image Qt can
read.

## Acceptance

- A PNG and a JPEG added from the package menu land in the package. Tested in
  `editor_window_tests`, watched failing first with the PNG refused.
- The JPEG plugin and the DLL it needs are deployed next to the editor.
