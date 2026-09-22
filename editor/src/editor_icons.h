#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

#include <cstdint>

namespace Editor::Icons {

enum class Glyph : uint8_t {
    Undo,
    Redo,
    Clock,
    Search,
    File,
    Folder,
    Upload,
    Save,
    Chevron,
    Preview,
    Plus,
    Minus,
    Cross,
    Dots,
    Frame,
    Eye,
    Lock,
    Solo,
    Rulers,
    Snap,
    Onion,
    Path,
    Background,
    Fit,
    Select,
    Anchor,
    Pan,
    Zoom,
    Sketch,
    First,
    Previous,
    Play,
    Warning,
    Next,
    Last,
    Key,
    Loop,
    Trash,
};

[[nodiscard]] QPixmap Drawn(Glyph glyph, const QColor& colour, int side);

[[nodiscard]] QIcon Of(Glyph glyph, const QColor& colour, int side = 16);

[[nodiscard]] QIcon Toggling(Glyph glyph, const QColor& off, const QColor& on, int side = 16);

}
