#pragma once

#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "formats/ge2d_shape.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace Document {

struct ImageArea {
    uint16_t atlas_width = 0;
    uint16_t atlas_height = 0;
    std::array<uint16_t, 4> uvrect{};
};

[[nodiscard]] Ge2dShape::Shape ImageQuad(std::string_view image, const ImageArea& area,
                                         bool mesh_package);

[[nodiscard]] Support::Expected<uint16_t, std::string>
NextCharacterId(const AfpAnimation::Animation& animation);

[[nodiscard]] std::map<uint16_t, std::string> ShapeImages(const Ifs::Archive& archive,
                                                          std::string_view animation_path);

[[nodiscard]] std::map<uint16_t, Box> ShapeBounds(const Ifs::Archive& archive,
                                                  std::string_view animation_path);

[[nodiscard]] Support::Expected<uint16_t, std::string>
AddImageShape(Ifs::Archive& archive, std::string_view animation_path, std::string_view image);

}
