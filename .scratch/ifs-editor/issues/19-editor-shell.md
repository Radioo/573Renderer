# Editor application shell

Status: resolved

Blocked by: 18.

## Acceptance

- A Qt 6 Widgets executable in `editor/`, built by its own CMake project and preset against a dynamic vcpkg triplet, linking the shared format and protocol sources from `src/`.
- Dockable panels arranged as viewport centre, package tree left, inspector right, timeline bottom, using Qt Advanced Docking System; the layout and window geometry are saved and restored.
- The application starts the preview host on the install the user picks and shuts it down on exit; a host failure shows in the window without losing the open document.
- `bash tools/checks.sh` still passes for the renderer build, and the editor project builds from a clean configure.
