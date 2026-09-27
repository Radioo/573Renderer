#pragma once

#include "formats/ifs_layout.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Ifs::Detail {

using Digest = std::array<uint8_t, kMd5Size>;

[[nodiscard]] Digest Md5(std::span<const uint8_t> bytes);

[[nodiscard]] Digest ManifestMd5(std::span<const uint8_t> file, std::size_t header_end,
                                 std::size_t data_offset);

}
