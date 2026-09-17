# Live preview while dragging on stage, and fast reloads

Status: resolved

Blocked by: 52.

Dragging an object on stage moved only its outline; the picture changed on
release. Every edit also reloaded the whole package in the host, 4.7 seconds for
`graphic/1/title.ifs`, and every reload leaked 8 texture slots of 1024.

## Acceptance

- The viewport reports drags while they happen as well as when they end. The
  widget tests cover both.
- An unfinished drag renders a preview from a copy of the document and leaves
  the document and the undo history alone; the finished one commits one step.
  A window test covers the document side (seen to fail when previews commit),
  and a local window test drives the real host through a preview and back
  (seen to fail when the preview renders the unedited document).
- The host keeps a package's textures in their own package and reloads only
  the rest; a reload of `title.ifs` takes about 170 ms and draws identically.
  Tested under `local`, and seen to fail with the textures left out.
- Releasing scene textures rewinds the texture slot counter. Tested under
  `local`: the reload test failed with 9, 17, 33, 49 slots before the fix.
