#pragma once

#include "document/authored.h"
#include "document/outline.h"
#include "formats/afp_animation.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Document {

enum class EditTarget : uint8_t { None, Placement, Camera, CallArgument, KeyValue };

struct InspectedRow {
    Field field;
    EditTarget edits = EditTarget::None;
};

struct Selection {
    std::optional<uint16_t> depth;
    uint32_t frame = 0;
    const AuthoredDepth* owned = nullptr;
    std::string key_property;
    std::optional<uint32_t> key_frame;
};

[[nodiscard]] std::vector<InspectedRow> InspectFrame(const AfpAnimation::Animation& animation,
                                                     const Selection& selection);

}
