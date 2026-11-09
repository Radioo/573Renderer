#pragma once

#include "document/atlas.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

struct LoadedImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgra;

    friend bool operator==(const LoadedImage&, const LoadedImage&) = default;
};

[[nodiscard]] LoadedImage WithGuardRing(const LoadedImage& image);

[[nodiscard]] Support::Expected<void, std::string> WriteAtlas(Ifs::Archive& archive,
                                                              std::string_view atlas_name,
                                                              const Atlas& atlas,
                                                              std::span<const LoadedImage> images);

}
