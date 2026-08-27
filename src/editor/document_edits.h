#pragma once

#include "preset/doc/preset_document.h"

#include <cstdint>

namespace Editor {

enum class Rounding : uint8_t {
    Nearest,
    Floor,
    Ceil,
};

struct FpsPreview {
    int edges = 0;
    int keys = 0;
};

FpsPreview PreviewConvertFps(const Preset::Doc::Document& document, int target_fps);

bool ConvertFps(Preset::Doc::Document& document, int target_fps, Rounding rounding);

}
