#include "editor_thumbnail.h"

#include <QtGlobal>

#include <cstddef>

namespace Editor {

QImage Thumbnail(std::span<const uint8_t> bgra, int width, int height, QSize into) {
    if (width <= 0 || height <= 0 || into.isEmpty()) return {};
    const auto needed = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (bgra.size() < needed) return {};
    const QImage borrowed(bgra.data(), width, height, QImage::Format_ARGB32);
    const QImage made = borrowed.scaled(into, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return made.constBits() == borrowed.constBits() ? made.copy() : made;
}

}
