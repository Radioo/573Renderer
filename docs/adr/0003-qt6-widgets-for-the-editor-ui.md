---
status: linking superseded by ADR-0009
---

# Qt 6 Widgets for the IFS editor UI

The IFS editor is a separate executable in this repo, built on Qt 6 Widgets with Qt Advanced Docking System. The renderer keeps Dear ImGui. The editor is meant for public use and needs large trees, property grids, text entry, high DPI and saved dock layouts. Qt has all of these, and ImGui would need months of custom work for them; the timeline and curve editor are custom in either toolkit. WinUI 3 was rejected because its XAML compiler is tied to MSBuild and it lacks editor controls. A C# front end was rejected because it splits the document model across two languages, and a custom UI because it means building a toolkit before the editor. Qt's LGPL terms require dynamic linking, so the editor has its own CMake preset with a dynamic vcpkg triplet and the shared code is built again for it.
