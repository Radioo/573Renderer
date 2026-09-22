# Command registry

Status: resolved

Every command is registered once with an id, a label, a shortcut and a refusal check. The menus (File, Edit, Depth, Keyframe, Clip, View, Playback) are views over it; a refused command is greyed in its menu with the reason as its tooltip, and running it anyway reports the reason and changes nothing.

## Acceptance

- `Editor::Commands` (object name `commands`) holds every command; menus list every command.
- No two commands share a shortcut; every shortcut from before the redesign still runs a command.
- Refusals are one sentence shared by the registry and the command body.
- Window tests run commands by id and check refusals through the registry.
