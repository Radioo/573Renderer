#pragma once

#include <QImage>
#include <QSize>

#include <cstdint>
#include <span>

namespace Editor {

[[nodiscard]] QImage Thumbnail(std::span<const uint8_t> bgra, int width, int height, QSize into);

}
