# Inspector widget

Status: resolved
Blocked by: 04

The inspector panel draws the sections: header with thumbnail, depth, character, span and baked or keyed badge; Keyframe this depth or Detach; scrubbable numeric fields; colour swatches; blend dropdown; filters list; stopwatch and keyframe diamonds for owned depths; Keyframes section with the embedded ease editor.

## Acceptance

- Every edit the old table made is still reachable and goes through the same edit targets.
- The modal ease dialog is gone.

## Comments

The Anchor row landed later, with the matrix mapping its offset needs
(docs/document.md). The Keyframes section with the embedded ease editor landed
after that, and the modal ease dialog is gone (`Editor::EaseEditor`,
`Window::RefreshEaseSection`, `Window::ApplySelectedKeysEase`), and so did the
Content section: the character row with its Replace button, the blend dropdown
over `Document::BlendModes`, the masking number and the filters list with add
and remove for an owned depth. Everything the design asked of this panel is in
place; the Raw placement fields section still carries every other field.
