# Previewing a sprite on its own

Status: resolved

Blocked by: 40.

With a sprite selected the viewport still shows the root animation, because the
preview host can only load a package-level animation by name.

## What the DLL does

afp-core exports a call that makes a movie clip show one symbol of its
animation in place of what it was showing, Flash's `attachMovie` by linkage
name: ordinal `0x6d`, `(mc_id, lib)`, and ordinal `0x89`, `(mc_id, stream_id,
lib)` for a symbol from another loaded animation. The symbol is looked up by
name, case-insensitively, through the animation's export table by binary
search, then its import table. Nothing exported takes a character id. To find
it again, xref the string `afp_play_work_attach_movie_as2`: its one reference is
the attach routine, and that routine's two exported callers are the two
ordinals. Read on IIDX 34 and confirmed on IIDX 33, where the routine and both
ordinals are the same.

## Acceptance

- The host protocol gains a request to show one exported symbol of the loaded
  animation, which binds ordinal `0x6d` and attaches the name onto the root
  movie clip, and a request to go back to the whole animation.
- The editor sends it when a sprite with an export name is picked, so the
  viewport, the timeline and playback all follow the sprite, and it goes back to
  the root when the root is picked.
- A sprite with no export name is shown by giving it a preview-only export name
  in the bytes handed to the host, never in the document, keeping the export
  table in the case-insensitive order the lookup needs.
- A `local_dll` test attaches a shipped sprite by name and checks afp reports
  the sprite's frame count, never by comparing pixels.
