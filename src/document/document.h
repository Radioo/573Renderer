#pragma once

#include "document/outline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

class File {
public:
    [[nodiscard]] static Support::Expected<File, std::string> Open(std::span<const uint8_t> bytes);

    [[nodiscard]] const std::vector<Node>& Nodes() const { return outline_.Nodes(); }

    [[nodiscard]] const std::vector<std::string>& Problems() const { return outline_.Problems(); }

    [[nodiscard]] Support::Expected<Details, std::string> Describe(std::string_view path) const {
        return outline_.Describe(archive_, path);
    }

    [[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string> Encode() const;

    [[nodiscard]] Support::Expected<AfpAnimation::Animation, std::string>
    ReadAnimation(std::string_view path) const;

    [[nodiscard]] Support::Expected<void, std::string>
    WriteAnimation(std::string_view path, const AfpAnimation::Animation& animation);

    [[nodiscard]] Support::Expected<void, std::string>
    AddEntry(std::string_view directory, std::string_view logical_name, std::vector<uint8_t> bytes);

    [[nodiscard]] Support::Expected<void, std::string> ReplaceEntry(std::string_view path,
                                                                    std::vector<uint8_t> bytes);

    [[nodiscard]] Support::Expected<void, std::string> RemoveEntry(std::string_view path);

    [[nodiscard]] Support::Expected<void, std::string>
    AddImage(std::string_view name, uint32_t width, uint32_t height, std::span<const uint8_t> bgra);

    [[nodiscard]] Support::Expected<void, std::string> RemoveImage(std::string_view name);

private:
    Ifs::Archive archive_;
    Outline outline_;
};

}
