# Animation settings and the drawn background

Status: resolved

Blocked by: 62.

An animation's stage size, frame rate and background could not be seen or
changed, and nothing said what afp-core does with them.

## Acceptance

- The inspector shows the stage size, frame rate, background colour and
  whether the colour is used when no depth is chosen, and each takes an edit in
  the form the header stores, refusing values that do not fit. Tested under
  `ci`.
- What afp-core does with them is read from the DLLs and written down, with
  what bm2dx does and does not ask for.
- The preview host can draw the background, off by default as in the game; with
  it on, the stage size and colour decide what is filled. Tested under `local`,
  and the test was seen to fail with the host option doing nothing.
- The editor offers the option in the Playback menu and remembers it.
