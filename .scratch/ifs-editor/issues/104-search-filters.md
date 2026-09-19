# Searching the package tree and the library

Status: resolved

A package with hundreds of entries, or an animation with many characters, had
no way to find a name other than scrolling.

## Acceptance

- The package tree and the library each have a search box that hides the rows
  whose names do not contain the text, ignoring case, keeping the folders
  around a match and everything inside a matching folder, and staying applied
  when an edit refills the tree. Tested in `editor_widget_tests` and
  `editor_window_tests`, and seen to fail without the reapply after a refill,
  with case-sensitive matching, or without either folder rule.
