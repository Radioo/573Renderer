# Graph mode

Status: resolved
Blocked by: 08

Property list with colours and checkboxes, Fit all and Fit keys, draggable Bezier handles writing the ease numbers.

Landed: the property list with a colour and a checkbox a track, several tracks
drawn at once, `graph.fit_all` and `graph.fit_keys` on the panel and in the View
menu, and draggable bezier handles writing the ease through
`Window::ApplyGraphEase`. Widget tests cover the colours, the two fits and the
handle drag; a window test covers the list, the checkboxes and the Fit buttons.
Documented in docs/editor.md.
