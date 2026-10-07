#pragma once

#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

struct ImagePixels {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgra;
};

[[nodiscard]] Support::Expected<std::string, std::string> StoredName(std::string_view directory,
                                                                     std::string_view logical_name);

[[nodiscard]] Support::Expected<void, std::string> EnsureDirectory(Ifs::Archive& archive,
                                                                   std::string_view path);

[[nodiscard]] Support::Expected<void, std::string> AddEntry(Ifs::Archive& archive,
                                                            std::string_view directory,
                                                            std::string_view logical_name,
                                                            std::vector<uint8_t> bytes);

[[nodiscard]] Support::Expected<void, std::string>
ReplaceEntry(Ifs::Archive& archive, std::string_view path, std::vector<uint8_t> bytes);

[[nodiscard]] Support::Expected<void, std::string> RemoveEntry(Ifs::Archive& archive,
                                                               std::string_view path);

[[nodiscard]] Support::Expected<void, std::string> AddImage(Ifs::Archive& archive,
                                                            std::string_view name, uint32_t width,
                                                            uint32_t height,
                                                            std::span<const uint8_t> bgra);

[[nodiscard]] Support::Expected<void, std::string> RemoveImage(Ifs::Archive& archive,
                                                               std::string_view name);

[[nodiscard]] Support::Expected<ImagePixels, std::string> ReadImage(const Ifs::Archive& archive,
                                                                    std::string_view name);

[[nodiscard]] Support::Expected<void, std::string> ReplaceImage(Ifs::Archive& archive,
                                                                std::string_view name,
                                                                uint32_t width, uint32_t height,
                                                                std::span<const uint8_t> bgra);

}
