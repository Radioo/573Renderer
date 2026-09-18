#pragma once

#include "document/atlas_write.h"
#include "document/entry_edit.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <map>
#include <optional>
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

    [[nodiscard]] Support::Expected<ImagePixels, std::string>
    ReadImage(std::string_view name) const;

    [[nodiscard]] std::map<uint16_t, std::string>
    ShapeImages(std::string_view animation_path) const;

    [[nodiscard]] std::map<uint16_t, Box> ShapeBounds(std::string_view animation_path) const;

    [[nodiscard]] Support::Expected<uint16_t, std::string>
    AddImageShape(std::string_view animation_path, std::string_view image);

    [[nodiscard]] std::optional<std::vector<uint8_t>> ShapeFile(std::string_view animation_path,
                                                                uint16_t id) const;

    [[nodiscard]] Support::Expected<void, std::string>
    AddShapeFile(std::string_view animation_path, uint16_t id, std::vector<uint8_t> bytes);

    [[nodiscard]] Support::Expected<void, std::string>
    RemoveShapeFile(std::string_view animation_path, uint16_t id);

    [[nodiscard]] Support::Expected<std::string, std::string>
    AddAnimation(std::string_view name, const File& like, std::string_view like_path,
                 uint32_t frames);

    [[nodiscard]] Support::Expected<void, std::string> RemoveAnimation(std::string_view path);

    [[nodiscard]] Support::Expected<std::string, std::string>
    RenameAnimation(std::string_view path, std::string_view name);

    [[nodiscard]] std::optional<std::string> EntryDigest(std::string_view path) const;

    [[nodiscard]] Support::Expected<void, std::string>
    WriteAtlas(std::string_view atlas_name, const Atlas& atlas,
               std::span<const LoadedImage> images);

private:
    Ifs::Archive archive_;
    Outline outline_;
};

}
