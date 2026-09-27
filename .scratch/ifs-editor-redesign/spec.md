# IFS editor redesign

Status: done

The IFS editor (`ifs_editor.exe`, Qt 6 Widgets, ADR 0003) gets a new window
layout and a new way of reaching its commands. Its document model, preview
host, formats and every edit it already makes stay as they are; this spec
changes where the user finds them and how they are asked for. The approved
design is a design canvas whose link is kept out of git in
`design.local.md` beside this spec (ten boards: a baked depth chosen, an owned depth with keyframes selected,
graph mode, editing a clip in context, owning a depth, the start screen,
command search, the drift review, the left dock tabs and the command map).
Vocabulary is `CONTEXT.md`'s.

## Problem Statement

The editor can do almost everything the content needs, and it is miserable to
use. Nearly every command lives in a right-click menu whose items change with
where the click landed: the timeline menu alone has 28 entries, and the lane,
library, package and inspector menus add more. About twenty commands then stop
and ask for a number in a `QInputDialog`: which depth to put something on,
which frame a range ends on, how many frames apart a wiggle goes, what
percentage to stretch by. The user has to know the depth numbers and frame
numbers by heart, and has no way to see the effect before typing.

The inspector is a two-column table of every placement field the format has,
twenty-six rows for one placement, most of them empty, named after the format
("Packed multiply colour", "Short rotate skew", "Unknown data") and shown in
stored units, so a depth in the middle of a 1920 by 1080 stage reads
`19200, 10800`. Nothing says which fields this frame set and which it carried
over from an earlier placement.

Whether a depth is baked data or authored content is invisible until an edit
is refused, and refusals arrive after the fact as a line in the status bar.
Owning a depth needs a project, which needs `File > New project` first. Clips
are picked from a combo box above the timeline, the graph hides behind a dock
tab, the transport has no buttons, and modes such as motion sketch are ticks in
the Playback menu. The package tree lists the IFS entries (`_info_`, `magic`,
`bsi`, `afplist`) at the same level as the animations the user came to edit,
with their names cut off.

## Solution

A window built around one rule: the user chooses the thing first, and the
commands for it appear next to it. No command asks for a depth or frame number
in a dialog. Where a command needs a number, it takes a sensible default from
what is chosen (the chosen depths, the playhead, the work area, the first free
depth) and the result can then be adjusted directly. Where a command genuinely
needs parameters (wiggle, simplify, time-stretch), it opens a small popover
beside its button with a live preview on the stage.

The window, from the design:

- **Top bar**: the menus (File, Edit, Depth, Keyframe, Clip, View, Playback),
  undo, redo and a history button, a command search field (Ctrl+K), the file's
  name and unsaved state, a project chip, an Export to IFS button carrying the
  count of authored content changed since the last export, and Save.
- **Left**: the Package panel with three tabs (Animations with thumbnails and
  frame count, stage size and rate; Images as a thumbnail grid with a detail
  area; Files, the raw IFS entry tree) and, below it, the Library of the open
  animation's characters as thumbnail tiles with use counts and type filters.
- **Centre**: the stage. A stage bar holds the clip breadcrumb (file,
  animation, root, and each clip entered), the overlay toggles (rulers,
  snapping, onion skin, motion path, outlines, background colour), the zoom
  control, fit, and saving pictures. A tool strip holds Select, Anchor, Pan,
  Zoom and Motion sketch. Below the stage, the selection bar lists what is
  chosen and its commands.
- **Bottom**, under the left panel and the stage: the timeline. Its header
  carries the transport, the frame number (click to type one), the time, the
  current label, the work area, the Timeline and Graph switch and the zoom.
  Rows list the front depth first, each with eye, lock and solo switches, the
  depth number, the character's type and name, and a badge for authored
  content. Bars are coloured by character type and marked on every frame where
  a placement changes the depth. A lane of frame scripts runs above the rows.
- **Right**, full height: the Inspector, with History as a second tab.
- **Status bar**: preview host state and render time, stage size and rate, the
  frame, what is chosen, snapping, stage zoom.

## User Stories

### Opening and the start screen

1. As an editor user, I want a start screen with Open IFS, Open project and my recent files, so that I can get back to work in one click.
2. As an editor user, I want each recent file to show a thumbnail, its folder, how many animations it has and whether it has a project, so that I can tell similar files apart.
3. As an editor user, I want to drop an IFS or a project folder anywhere in the window, so that I do not have to go through a file dialog.
4. As an editor user, I want the start screen to show the game install, whether the preview host is running and the target build, so that I know the picture I get will be the game's.
5. As an editor user, I want to change the game install and restart the preview host from the start screen and from the status bar, so that I do not have to find it in a menu.
6. As an editor user without a game install, I want the start screen to tell me that everything except the picture still works, so that I am not put off editing.
7. As an editor user, I want the first animation of a freshly opened package shown straight away, so that opening a file shows something.

### Top bar, saving and exporting

8. As an editor user, I want the file name and whether it has unsaved edits (and how many) in the top bar, so that I never lose work by accident.
9. As an editor user, I want Save as a visible button with Ctrl+S, so that I do not have to open the File menu.
10. As an editor user with a project, I want an Export to IFS button that shows how many owned depths and project images changed since the last export, so that I know when the IFS is behind the project.
11. As an editor user, I want Export disabled with a reason when nothing has changed or no project is open, so that I understand why it does nothing.
12. As an editor user, I want a project chip that names the open project and offers opening its folder and closing it, so that project state is visible without a menu.
13. As an editor user, I want undo and redo buttons whose tooltips name the step, and a history button that opens the History tab, so that I can see what undo will do before pressing it.

### Command search

14. As an editor user, I want Ctrl+K to open a search over every command, so that I can run anything by name without knowing which menu holds it.
15. As an editor user, I want the search to also find depths of the open clip and animations of the package, so that I can jump to them by typing.
16. As an editor user, I want each result to show its shortcut, so that I learn the keys as I go.
17. As an editor user, I want commands that cannot run right now to stay in the results, greyed, with the reason (for example "Select three or more keyframes of a keyed depth"), so that I learn what they need instead of wondering where they went.
18. As an editor user, I want Enter to run the highlighted result and Escape to close the search, so that it is fully keyboard driven.

### Package panel

19. As an editor user, I want the animations of the package listed on their own with a thumbnail, their full name, frame count, stage size and frame rate, so that I can find the one I want without reading entry kinds and byte sizes.
20. As an editor user, I want to filter animations by name, so that a package with dozens of them stays usable.
21. As an editor user, I want to rename an animation in place with F2 or a double-click, so that renaming needs no dialog.
22. As an editor user, I want a row menu on each animation with Open, Rename, Duplicate, Remove, save every frame as PNG, and removing unused definitions with the count shown before I click, so that the package operations live with the animation they act on.
23. As an editor user, I want New animation to add a row with its name ready to type and the open animation's header as the template, so that making one needs no chain of dialogs.
24. As an editor user, I want an Images tab showing every texture image as a thumbnail with its size and atlas, so that I can see the package's pictures.
25. As an editor user, I want an image's detail area to offer Save as PNG and Replace, so that editing a picture in another program and putting it back is two clicks.
26. As an editor user with a project, I want adding an image to offer adding it as a project image, so that it goes into the project's atlas on export.
27. As an editor user, I want a Files tab with the raw IFS entry tree, kinds and sizes, so that I can still replace or remove whole entries when I need to.
28. As an editor user, I want unknown data marked and protected in the Files tab, so that I do not break what the editor does not understand.

### Library

29. As an editor user, I want the open animation's characters as thumbnail tiles with their type and how many placements use them, so that I can recognise them by sight.
30. As an editor user, I want to filter the library by images, shapes, clips and unused characters, so that I can find things in a large animation.
31. As an editor user, I want to drag a tile onto the stage to place it where I drop it, so that placing is direct.
32. As an editor user, I want to drag a tile onto the timeline to place it from the frame I drop on, so that I can choose when it appears.
33. As an editor user, I want to drop a tile onto the inspector's Character row to change what the chosen depth shows from the playhead, so that replacing a character is a drag.
34. As an editor user, I want Enter on a tile to place it on the first free depth from the playhead to the end of the clip, so that placing needs no questions.
35. As an editor user, I want a New empty clip button that creates the clip and opens it, with its frame count editable in the inspector, so that making a clip asks nothing up front.
36. As an editor user, I want Duplicate on a clip tile, so that I can make a variant without touching the other places the original is shown.
37. As an editor user, I want a button that removes the unused definitions, with their count, next to the Unused filter, so that clean-up is visible.

### Stage

38. As an editor user, I want the stage shown on a neutral background with its edge drawn and its size and rate named under it, so that I can see where the stage ends.
39. As an editor user, I want a zoom control with the current percentage and a Fit button in the stage bar, so that zoom is not only Ctrl+wheel and Ctrl+0.
40. As an editor user, I want toggles for rulers, snapping, onion skin, motion path, outlines and the background colour in the stage bar, showing whether each is on, so that I do not have to open the View menu to see or change them.
41. As an editor user, I want a Select tool that picks, moves, scales and turns with the existing handles, so that the main job keeps working as it does now.
42. As an editor user, I want an Anchor tool, besides dragging the anchor cross, so that moving the anchor without moving the object is discoverable.
43. As an editor user, I want Pan and Zoom tools, and Space held to pan, so that the stage can be navigated without a middle button.
44. As an editor user, I want Motion sketch to be a tool rather than a menu tick, so that I can see when a drag will record motion.
45. As an editor user, I want to save the frame or the work area as PNG from a button on the stage bar, so that I do not have to find it in the File menu.

### Selection bar

46. As an editor user, I want a bar under the stage naming what is chosen (a depth with its thumbnail and state, several depths, keyframes, or nothing), so that I always know what a command will act on.
47. As an editor user with one depth chosen, I want Fit (to the stage, its width, its height), Centre anchor, Flip, Arrange, Split, Duplicate, Group into clip and Remove on the bar, so that the depth commands are one click away.
48. As an editor user with several depths chosen, I want Align, Spread, Sequence and Remove on the bar, so that group commands appear when they apply.
49. As an editor user, I want commands that cannot apply to stay on the bar, greyed, with a tooltip saying what they need, so that I learn the rule instead of meeting a refusal later.
50. As an editor user, I want Group into clip to use the chosen depths and the work area (or the widest span under the playhead) without asking, and let me adjust the frames in a popover before applying, so that grouping needs no dialogs.
51. As an editor user with keyframes selected, I want the ease presets (Linear, Hold, Easy ease, Ease in, Ease out, Curve), Reverse, Stretch, Wiggle, Simplify, Copy and Delete on the bar, so that keyframe commands are one click away.
52. As an editor user, I want Stretch, Wiggle and Simplify to open a popover with their fields filled with sensible values and a live preview on the stage, so that I can see the result before applying.
53. As an editor user, I want Simplify's popover to say how many keyframes will go at the current tolerance, so that I can tune it.
54. As an editor user, I want Wiggle's popover to show its seed and a button for a new one, so that I can reroll a wiggle I do not like.
55. As an editor user, I want every popover to apply as one undo step and Escape to cancel with nothing changed, so that trying things is safe.

### Timeline

56. As an editor user, I want transport buttons (first frame, previous, play, next, last, previous and next change, loop) in the timeline header, so that playback does not depend on knowing the keys.
57. As an editor user, I want to click the frame number and type one, so that Go to frame needs no dialog.
58. As an editor user, I want the time in seconds and the current label next to the frame number, so that I know where I am in the animation's terms.
59. As an editor user, I want the work area's range and a clear button in the header, so that I can see and drop it.
60. As an editor user, I want Trim clip, Extract and Lift buttons on the work area band in the ruler, so that the work area commands sit on the work area.
61. As an editor user, I want a zoom slider with plus and minus buttons, so that timeline zoom is not only keys and Ctrl+wheel.
62. As an editor user, I want the front depth listed first, so that the list reads like the stacking order I see.
63. As an editor user, I want each row to show eye, lock and solo switches, the depth number, the character's type icon and name, so that I can recognise depths without choosing them.
64. As an editor user, I want clicking the eye, lock or solo column header to apply to every row (show everything, unlock everything, end solo), so that Show every hidden depth has a visible home.
65. As an editor user, I want authored content marked with a badge on its row and a coloured edge on its bar, so that I can see which depths are keyed before I try to edit them.
66. As an editor user, I want bars coloured by what they show (image, shape, clip, text), so that the timeline is readable at a glance.
67. As an editor user, I want a tick on a bar at every frame where a placement changes that depth, so that baked content shows where its motion changes, not only keyed content.
68. As an editor user, I want hidden depths' bars drawn hatched and grey, so that hidden is obvious.
69. As an editor user, I want to drag a row up or down to move its span to another depth, with the target shown while I drag, so that changing depth needs no dialog.
70. As an editor user, I want Ctrl+D to duplicate onto the first free depth above and then drag the copy where I want it, so that duplicating needs no dialog.
71. As an editor user, I want Ctrl+V to paste a copied depth on the first free depth at the playhead of whichever clip is open, so that pasting between clips needs no dialog.
72. As an editor user, I want a + Depth button that opens a picker of the animation's characters and the package's images, placing the pick on the first free depth from the playhead, so that adding a depth is visible.
73. As an editor user, I want a lane above the rows showing every frame script, library calls as readable chips, so that I can see where the animation stops and jumps.
74. As an editor user, I want to double-click a script chip to edit it in the inspector, so that scripts are reached from where they happen.
75. As an editor user, I want a camera button on that lane's header to add or remove the camera on the playhead's frame, so that cameras have a visible home.
76. As an editor user, I want to double-click the ruler to add a label there with its name ready to type, so that adding a label needs no dialog.
77. As an editor user, I want to double-click a label's flag to rename it in place and press Delete to remove the one I clicked, so that label edits need no dialog.
78. As an editor user, I want the existing span drags, trims, Shift snapping, label drags and bracket keys to keep working, so that nothing I rely on is lost.
79. As an editor user with an owned depth chosen, I want a twirl on its row that opens its property lanes, so that keyframes are one click away.
80. As an editor user, I want selected keyframes drawn differently from the rest and held keyframes drawn as squares, so that I can read a lane at a glance.
81. As an editor user, I want each property lane's header to show the value at the playhead and previous and next keyframe buttons, so that I can step between keyframes of one property.
82. As an editor user, I want to double-click a lane to add a keyframe there, so that keying is direct.

### Graph mode

83. As an editor user, I want a Timeline and Graph switch in the timeline header (Shift+F3), so that the graph is a mode of the same panel, not a tab I have to find.
84. As an editor user, I want the graph's side column to list the owned depth's properties with a colour and a checkbox each, so that I choose which curves I see.
85. As an editor user, I want Fit all and Fit keys buttons, so that the curves fill the panel.
86. As an editor user, I want to drag a keyframe up or down, sideways, or one way only with Shift, as today, so that the existing graph editing is kept.
87. As an editor user, I want the Bezier ease handles of a segment drawn on the curve and draggable, so that I can shape an ease where I can see it.
88. As an editor user, I want a box around the selected keyframes in the graph, so that I can see what the selection bar will act on.

### Inspector

89. As an editor user, I want the inspector's header to show the chosen depth's thumbnail, depth number, character name and id, size, span and whether it is baked or keyed, so that I know what I am editing.
90. As an editor user, I want the placement's values grouped into Transform, Appearance, Masking, Character and Script, so that I find a value by what it does.
91. As an editor user, I want position and anchor in stage pixels, scale in percent and angles in degrees, so that I do not have to convert stored units in my head.
92. As an editor user, I want each value marked as set on this frame or carried from an earlier one, and the frame that set it named, so that I know which placement an edit will change.
93. As an editor user, I want previous and next change buttons in the inspector, so that I can step through the placements of the chosen depth.
94. As an editor user, I want to drag a value's label left and right to scrub it, and type into its field, so that small adjustments are quick.
95. As an editor user, I want a link toggle between scale's two values, so that scaling can keep the aspect.
96. As an editor user, I want colours shown as a swatch, a hex value and an alpha, with a click on the swatch opening a colour picker, so that picking a colour is not hidden in a right-click menu.
97. As an editor user, I want blend shown as a named dropdown, so that I do not have to know the stored number.
98. As an editor user, I want the filters listed with an add button and a remove button on each, so that filters are edited where they are shown.
99. As an editor user, I want the depth a placement masks up to shown as its own row, so that masking is visible.
100. As an editor user, I want the Character row to show a thumbnail and a Replace menu and to accept a library drop, so that changing what a depth shows is direct.
101. As an editor user, I want the raw placement fields, including unknown data, kept in a collapsed section with their count, so that nothing the format holds is hidden but it does not crowd the common values.
102. As an editor user, I want unknown data shown read-only and marked as kept byte for byte, so that I know it will survive a save.
103. As an editor user with nothing chosen on the root timeline, I want the animation's settings (stage size, frame rate, background colour and whether it is used) and the frame's camera, so that the inspector is never empty.
104. As an editor user inside a clip with nothing chosen, I want the clip's frame count, its export name, where it is shown and Duplicate and Ungroup buttons, so that clip settings have a home.
105. As an editor user, I want a baked depth's inspector to offer "Keyframe this depth", so that owning is a visible, named action.
106. As an editor user with an owned depth chosen, I want a stopwatch beside each property it does not animate yet and a keyframe diamond beside each it does, so that starting to animate and keying on this frame work as in After Effects.
107. As an editor user, I want values that come from keyframes coloured differently from fixed ones, so that I can see what is animated.
108. As an editor user, I want a Keyframes section when keyframes are selected, naming how many on which property and frames, with the ease curve drawn, preset buttons and the four Bezier numbers, so that easing needs no modal dialog.
109. As an editor user with a Filters keyframe selected, I want its filters as rows with add and remove, so that filter keyframes are editable where they are shown.
110. As an editor user, I want an owned depth's inspector to offer Detach to baked, so that giving the content back is as visible as owning it.
111. As an editor user, I want the Script section to show a library call as the call and one editable row per argument, and anything else as instructions, so that scripts are edited in the inspector.
112. As an editor user with an owned depth, I want its script source edited in the Script section, with an empty source handing it back to the baked script, so that script editing needs no text dialog.
113. As an editor user, I want a field that refuses a value to say why next to the field and go back to the document's value, so that a bad value never looks accepted.

### Owning, detaching and projects

114. As an editor user without a project, I want Keyframe this depth to explain what owning does and offer to create a project at a suggested path I can change, so that owning is one step instead of three.
115. As an editor user, I want later owning to use the same project without asking, so that the question comes once.
116. As an editor user, I want owning to be lossless and to say so, so that I trust that nothing on screen will change.
117. As an editor user, I want commands that an owned depth refuses (split, remove, group, trim the clip, centre the anchor and the rest) greyed with the reason and the way out (detach first), so that I am not surprised by a refusal.
118. As an editor user opening a project whose IFS changed, I want one sheet listing every difference with a choice per row, choices for all rows at once, and a way to look at each difference on the stage, so that I decide once instead of answering a dialog per entry.
119. As an editor user, I want Decide later to leave the project as it is without applying anything, so that I can look around first.

### Clips

120. As an editor user, I want to open a clip by double-clicking its bar on the timeline or its object on the stage, so that entering a clip is direct.
121. As an editor user, I want the breadcrumb above the stage to show the path from the root to the open clip, each part clickable, and Escape to go up one level, so that I always know where I am and can get back.
122. As an editor user, I want to choose between editing a clip in context, with the root dimmed around it, and on its own, so that I can see a clip where it is used or by itself.
123. As an editor user inside a clip, I want the timeline, library highlight, inspector and selection bar to be the clip's, so that everything follows where I am.
124. As an editor user, I want a Back to root button on the selection bar inside a clip, so that leaving is visible.
125. As an editor user, I want the clip's export name edited in the inspector, so that naming an export needs no dialog.

### Feedback

126. As an editor user, I want a short notice after an edit with a visible result (paste bringing shapes along, unused definitions removed, clips restarting after a trim), with Undo on it, so that I see what happened and can take it back.
127. As an editor user, I want notices to go away by themselves and to be dismissable, so that they never pile up.
128. As an editor user, I want preview host errors in the status bar with one dialog per new error, as today, so that a failing host does not flood me with dialogs.
129. As an editor user, I want the status bar to show the host's render time for a frame, so that I notice when previews get slow.

### Keys and layout

130. As an editor user, I want every existing shortcut kept, so that my habits still work.
131. As an editor user, I want every command still listed in the Edit, Depth, Keyframe, Clip, View and Playback menus with its shortcut, so that menus remain a full index.
132. As an editor user, I want the default layout (package and library left, stage centre, timeline under both, inspector right) restored for a saved layout from before the redesign, so that I see the new window on first start.
133. As an editor user, I want panels still movable and closable and View > Panels to bring them back, so that I can arrange the window my way.

## Implementation Decisions

- **What stays.** The document model, the preview host and its protocol, the
  formats, the history, the project and every edit keep their behaviour. No
  edit is added or removed by this spec; commands move and lose their dialogs.
  Where a dialog disappears, the edit it fed is called with the default the
  spec names, then adjusted through a direct gesture or a field.
- **Window layout.** The dock manager's default layout becomes: Package and
  Library stacked left, the stage as the central widget, the timeline docked
  bottom under the left area and the stage, the Inspector and History tabbed on
  the right at full height. The docks layout version is raised, so any saved
  state from before falls back to the new default.
- **Command registry (new module, the one new interface).** Every command the
  editor offers is registered once with a stable id, a label (which may name
  the target, as today's "Split depth 9 at frame 478"), a group, a default
  shortcut and an availability function returning either available or
  unavailable with a reason sentence. Menus, the selection bar, the command
  search, row menus and keyboard shortcuts are all views over the registry and
  never decide availability themselves. A command that is unavailable is shown
  greyed with its reason, never hidden, except in the selection bar where a
  command belongs to a selection kind (depth, depths, keyframes, nothing) and
  only that kind's commands appear. Running a command that turns out to be
  refused by the document still reports the document's reason, as today.
- **Availability reasons come from the document where the document knows
  them.** Checks the document already makes (owned depth refusals, work area
  needed, a depth with nothing on the playhead) are exposed as pure checks the
  registry calls before offering the command, so the greyed reason and the
  refusal message are the same sentence.
- **Defaults that replace dialogs.** Placing a character: first free depth by
  the existing next-free-depth rule, from the playhead to the clip's last
  frame. Duplicate: first free depth above. Paste a depth: first free depth at
  the playhead of the clip on screen. Moving a span to another depth: a row
  drag onto the target row. Group into clip: the chosen depths, over the work
  area or else the widest span under the playhead, adjustable in a popover.
  New animation: a new row named after the open animation with a suffix, name
  selected for typing, frame count and header copied from the open animation,
  and the existing ask-for-another-IFS path kept only for a package with no
  animation. New empty clip: the open clip's frame count. Labels: a name typed
  in place on the ruler. Go to frame: the frame field.
- **Popovers for parameter commands.** Stretch, Wiggle, Simplify, Group into
  clip, and the owning explanation are anchored popovers, not dialogs. While a
  popover is open its current values are previewed through the existing stage
  preview path (the change applied to a copy of the document and rendered at
  the current frame, coalesced to the latest value), and applying commits one
  undo step through the existing edit path. Cancel leaves the document and the
  history untouched.
- **Inspector model (document module, extended).** The row list the window
  shows becomes sections of rows produced by the document, so the window stays
  free of format knowledge. Each row carries: its section, its label, its
  display value in display units, whether this frame set it or it was carried
  and from which frame, whether it is keyed on this frame, animated, or not
  animated (for owned depths), and the existing edit target the window
  switches on. Unknown data and the format's raw fields (ratio, name, class
  name, translation z, 3D matrix, HSV, the packed colours, the short twins,
  origin as stored, geometry) are rows of a Raw section, collapsed by default.
- **Display units.** Translation and origin are shown in stage pixels using
  afp-core's `/20` scale (documented in the notes repository's afp format
  page). Scale is shown in percent. Colours are shown as hex and alpha percent.
  Values are converted back with the same scale on edit, so a typed value lands
  on the stored grid; a value that does not land exactly on it is rounded and
  the field shows the stored result.
- **Rotation and skew.** The format stores the linear part as the scale pair
  and the rotate skew pair. The inspector shows Rotation and Skew in degrees
  derived from those four numbers, and editing either rebuilds the four
  through the existing reshape arithmetic used by the stage's turn handle. An
  unedited placement is never rewritten, so opening and saving stays lossless.
  The stored pairs stay editable as they are in the Raw section.
- **Timeline order.** Rows are listed front depth first. The data model and
  every depth number stay as they are; only the drawing order changes.
- **Timeline widget.** Gains: eye, lock and solo switches with column header
  toggles; a type icon and character name per row; the authored badge; bars
  coloured by character type; change ticks from the existing depth marks; a
  row drag to another depth; the frame scripts lane with a camera button; the
  work area band with Trim, Extract and Lift; double-click to add a label and
  to rename one; a header with transport, frame field, time, label, work area,
  mode switch and zoom slider. The graph becomes a mode of the same panel
  rather than a second dock.
- **Graph.** Gains a property list with colours and checkboxes, Fit all and
  Fit keys, and draggable Bezier handles on a segment whose ease is a curve,
  writing the same four numbers the ease editor writes.
- **Ease editing.** The modal ease dialog is replaced by the same curve editor
  embedded in the inspector's Keyframes section. The curve maths and presets
  are reused from the document.
- **Stage.** Gains the stage bar (breadcrumb, overlay toggles, zoom control,
  fit, picture saving) and the tool strip. The Anchor tool drags the anchor
  cross only. Pan and Zoom tools reuse the existing pan and zoom. The Motion
  sketch tool replaces the Playback menu tick and uses the existing sketch
  recording.
- **Clips.** The clip combo box is removed; the breadcrumb and the existing
  "show a clip on its own" path replace it. In context editing is new: the
  viewport shows the root with everything but the open clip dimmed and the
  clip's bounds drawn, and stage gestures map through the clip's placement
  on the root at the playhead. When the clip is not placed on the root at the
  playhead, in context falls back to on its own and says so.
- **Owning without a prior project.** Keyframe this depth, with no project
  open, creates the project at a path next to the IFS that the popover shows
  and lets the user change, then owns the depth, as two history steps named
  as today.
- **Drift review.** The per-entry drift questions at project open become one
  sheet built from the same drift report; applying writes every choice to the
  manifest at once, as today's answers do one by one. Decide later writes
  nothing and leaves the project's state as it is.
- **Notices.** A non-modal notice with an optional Undo replaces status bar
  messages that report a successful edit's side effects. Status bar messages
  remain for host state and for errors.
- **Start screen.** Shown in the central area when no IFS is open. Recent files
  come from the settings the editor already keeps, extended with a list of
  recent paths and whether each had a project.
- **UI words.** The UI keeps calling a clip other than the root a "sprite", as
  the current editor and the library do, and says "Keyframe this depth" and
  "Detach to baked" for own and detach, and "KEYED" and "BAKED" for authored
  content and baked data. `CONTEXT.md` gains the terms selection bar, command
  and notice, and notes these UI words against its own terms.
- **Dependencies.** No new dependency is expected: Qt Widgets and the
  Advanced Docking System already present cover popovers, the tool strip and
  the start screen.
- **Documentation.** `docs/editor.md` is rewritten for the new window in the
  same changes that move each command, with a section per area as today.

## Testing Decisions

- **A good test drives the editor the way a user does and checks what the
  user would see or what the document now holds.** It never reaches into
  private widget state or asserts on layout pixels. Every behaviour moved by
  this spec keeps a test that fails when the behaviour is removed; each new
  test is checked by breaking the behaviour it covers and watching it fail,
  as the existing suites record.
- **Seam 1, the window (existing, primary).** `editor_window_tests` builds the
  whole window with no preview host and drives it. The command registry is the
  new handle on it: a case runs a command by id, reads its availability and
  reason, and reads which command ids the selection bar lists for a selection.
  Cases that today pick a context menu item and answer a `QInputDialog` are
  rewritten to run the command by id (or perform the direct gesture) and check
  the default the spec names. Cases that exercise refusals check the greyed
  reason and that running the command anyway changes nothing.
- **Seam 2, the document (existing).** The document test suite covers the new
  pure pieces: the sectioned inspector rows (sections, display units and their
  round trip, set or carried with the setting frame, keyed state, the Raw
  section keeping every field), the rotation and skew derivation and rebuild
  (including that an unedited placement round trips unchanged), and the
  availability checks the registry calls. These need no window and no game
  install.
- **Seam 3, the widgets (existing).** `editor_widget_tests` drives the
  timeline, viewport and graph with synthetic events for the new gestures: row
  switches and column header toggles, dragging a row to another depth, the
  ruler double-click and in-place label edit, the work area band buttons, the
  mode switch, graph Bezier handles, dropping a library tile on the inspector
  Character row, and the stage tools.
- **Live tests (existing, local only).** The live window tests keep covering
  what needs the preview host: popover previews changing the picture and
  Cancel restoring it, in context clip editing drawing the dimmed root, motion
  sketch as a tool.
- **Shortcut table.** One window test lists every registered command's
  shortcut and fails on a duplicate, and checks every shortcut in the current
  editor's documentation is still registered.
- **Prior art.** The `Script` steps and package helpers in the window test
  support, the mouse helpers and sample scene in the widget test support, the
  document tests' sample package, and the preview-dependent cases that skip
  without `R573_IIDX_DIR`.

## Out of Scope

- New edits the editor does not already make. The redesign moves existing
  commands; features such as a speed graph, several documents open at once, or
  text characters come separately.
- Changes to the preview host, its protocol, the formats, the round trip or
  export.
- Theming beyond the dark, square-cornered style the design shows; light mode.
- Localising the UI.
- The renderer's own ImGui GUI (Timeline Studio), which is a different
  application.

## Further Notes

- The design canvas's stage shows labelled blocks where the preview host's
  picture goes, and its character and animation names other than `title` are
  made up.
- The command map board of the canvas lists every current command and its new
  home; it is the checklist for "nothing lost" while implementing.
- The migration is large enough to be split into issues per area (shell and
  registry first, then inspector model, timeline, selection bar and popovers,
  stage bar and tools, clips, package and library, start screen, drift sheet,
  notices), each keeping the full gate green, rather than one change.
