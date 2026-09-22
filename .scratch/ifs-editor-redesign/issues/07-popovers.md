# Popovers for parameter commands

Status: resolved
Blocked by: 06

Stretch, Wiggle, Simplify and Group into sprite open anchored popovers with defaults and a live stage preview; apply is one undo step, cancel changes nothing.

## Acceptance

- No `QInputDialog` remains for these commands.
- Simplify shows how many keyframes will go.

## Comments

A review of the first cut found two Qt traps, both fixed with tests or docs:
the inspector deleted the spin box that was emitting the edit (now
`deleteLater`), and the popover deleted its buttons while iterating the live
child list (now a copied list). Both are recorded in docs/editor.md.
