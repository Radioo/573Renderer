#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Document {

enum class ScriptShape : uint8_t { Call, Instructions, Unreadable };

struct ScriptPlace {
    ClipId clip;
    uint32_t frame = 0;
    std::optional<uint16_t> depth;

    friend bool operator==(const ScriptPlace&, const ScriptPlace&) = default;
};

struct ScriptEntry {
    ScriptPlace place;
    std::string clip_name;
    std::string preview;
    ScriptShape shape = ScriptShape::Call;

    friend bool operator==(const ScriptEntry&, const ScriptEntry&) = default;
};

[[nodiscard]] std::vector<ScriptEntry> ScriptsIn(const AfpAnimation::Animation& animation);

[[nodiscard]] std::vector<std::string> CallsIn(const AfpAnimation::Animation& animation);

[[nodiscard]] std::optional<AfpAnimation::Bytecode>
ScriptAt(const AfpAnimation::Animation& animation, const ScriptPlace& place);

}
