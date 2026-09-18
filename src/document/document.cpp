#include "document/document.h"

#include "document/entries.h"
#include "formats/ifs_digest.h"
#include "document/atlas.h"
#include "document/atlas_write.h"
#include "document/entry_edit.h"
#include "document/image_shape.h"
#include "document/animation_entries.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <map>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

Support::Expected<void, std::string> ReplaceBytes(Ifs::Archive& archive, std::string_view path,
                                                  std::vector<uint8_t> bytes) {
    Ifs::Entry* entry = FindEntry(archive, path);
    if (entry == nullptr) return Support::Unexpected(std::string(path) + " is not in the package");
    if (entry->super_index != 0)
        return Support::Unexpected(std::string(path) + " lives in a super image");
    entry->bytes = std::move(bytes);
    entry->stored_size = static_cast<uint32_t>(entry->bytes.size());
    return {};
}

}

Support::Expected<File, std::string> File::Open(std::span<const uint8_t> bytes) {
    auto archive = Ifs::Read(bytes);
    if (!archive) return Support::Unexpected(archive.error());
    File file;
    file.archive_ = std::move(*archive);
    file.outline_ = Outline::Build(file.archive_);
    return file;
}

Support::Expected<std::vector<uint8_t>, std::string> File::Encode() const {
    return Ifs::Write(archive_);
}

Support::Expected<AfpAnimation::Animation, std::string>
File::ReadAnimation(std::string_view path) const {
    const std::string script_path = ScriptPath(path);
    const Ifs::Entry* animation = FindEntry(archive_, path);
    const Ifs::Entry* script = script_path.empty() ? nullptr : FindEntry(archive_, script_path);
    if (animation == nullptr || script == nullptr) {
        return Support::Unexpected(std::string(path) + " has no byte order script at " +
                                   script_path);
    }
    return AfpAnimation::ReadStored(animation->bytes, script->bytes);
}

Support::Expected<void, std::string>
File::WriteAnimation(std::string_view path, const AfpAnimation::Animation& animation) {
    const std::string script_path = ScriptPath(path);
    if (script_path.empty())
        return Support::Unexpected(std::string(path) + " is not an animation path");
    auto stored = AfpAnimation::WriteStored(animation);
    if (!stored) return Support::Unexpected(stored.error());
    auto written = ReplaceBytes(archive_, path, std::move(stored->data));
    if (!written) return Support::Unexpected(written.error());
    auto scripted = ReplaceBytes(archive_, script_path, std::move(stored->script));
    if (!scripted) return Support::Unexpected(scripted.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::AddEntry(std::string_view directory,
                                                    std::string_view logical_name,
                                                    std::vector<uint8_t> bytes) {
    auto added = Document::AddEntry(archive_, directory, logical_name, std::move(bytes));
    if (!added) return Support::Unexpected(added.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::ReplaceEntry(std::string_view path,
                                                        std::vector<uint8_t> bytes) {
    auto replaced = Document::ReplaceEntry(archive_, path, std::move(bytes));
    if (!replaced) return Support::Unexpected(replaced.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::RemoveEntry(std::string_view path) {
    auto removed = Document::RemoveEntry(archive_, path);
    if (!removed) return Support::Unexpected(removed.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::AddImage(std::string_view name, uint32_t width,
                                                    uint32_t height,
                                                    std::span<const uint8_t> bgra) {
    auto added = Document::AddImage(archive_, name, width, height, bgra);
    if (!added) return Support::Unexpected(added.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::WriteAtlas(std::string_view atlas_name,
                                                      const Atlas& atlas,
                                                      std::span<const LoadedImage> images) {
    auto written = Document::WriteAtlas(archive_, atlas_name, atlas, images);
    if (!written) return Support::Unexpected(written.error());
    outline_ = Outline::Build(archive_);
    return {};
}

std::optional<std::string> File::EntryDigest(std::string_view path) const {
    const Ifs::Entry* entry = FindEntry(archive_, path);
    if (entry == nullptr || entry->kind != Ifs::EntryKind::File) return std::nullopt;
    const Ifs::Detail::Digest digest = Ifs::Detail::Md5(entry->bytes);
    std::string out;
    out.reserve(digest.size() * 2);
    for (const uint8_t byte : digest) {
        out += "0123456789abcdef"[byte >> 4U];
        out += "0123456789abcdef"[byte & 0xFU];
    }
    return out;
}

Support::Expected<ImagePixels, std::string> File::ReadImage(std::string_view name) const {
    return Document::ReadImage(archive_, name);
}

Support::Expected<void, std::string> File::ReplaceImage(std::string_view name, uint32_t width,
                                                        uint32_t height,
                                                        std::span<const uint8_t> bgra) {
    auto replaced = Document::ReplaceImage(archive_, name, width, height, bgra);
    if (!replaced) return Support::Unexpected(replaced.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::RemoveImage(std::string_view name) {
    auto removed = Document::RemoveImage(archive_, name);
    if (!removed) return Support::Unexpected(removed.error());
    outline_ = Outline::Build(archive_);
    return {};
}

std::map<uint16_t, std::string> File::ShapeImages(std::string_view animation_path) const {
    return Document::ShapeImages(archive_, animation_path);
}

std::map<uint16_t, Box> File::ShapeBounds(std::string_view animation_path) const {
    return Document::ShapeBounds(archive_, animation_path);
}

Support::Expected<uint16_t, std::string> File::AddImageShape(std::string_view animation_path,
                                                             std::string_view image) {
    auto added = Document::AddImageShape(archive_, animation_path, image);
    if (!added) return Support::Unexpected(added.error());
    outline_ = Outline::Build(archive_);
    return *added;
}

std::optional<std::vector<uint8_t>> File::ShapeFile(std::string_view animation_path,
                                                    uint16_t id) const {
    return ShapeFileBytes(archive_, animation_path, id);
}

Support::Expected<void, std::string> File::AddShapeFile(std::string_view animation_path,
                                                        uint16_t id, std::vector<uint8_t> bytes) {
    auto added = Document::AddShapeFile(archive_, animation_path, id, std::move(bytes));
    if (!added) return Support::Unexpected(added.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::RemoveShapeFile(std::string_view animation_path,
                                                           uint16_t id) {
    auto removed = Document::RemoveShapeFile(archive_, animation_path, id);
    if (!removed) return Support::Unexpected(removed.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<void, std::string> File::RemoveAnimation(std::string_view path) {
    auto removed = Document::RemoveAnimation(archive_, path);
    if (!removed) return Support::Unexpected(removed.error());
    outline_ = Outline::Build(archive_);
    return {};
}

Support::Expected<std::string, std::string> File::RenameAnimation(std::string_view path,
                                                                  std::string_view name) {
    auto renamed = Document::RenameAnimation(archive_, path, name);
    if (!renamed) return Support::Unexpected(renamed.error());
    outline_ = Outline::Build(archive_);
    return *renamed;
}

Support::Expected<std::string, std::string> File::AddAnimation(std::string_view name,
                                                               const File& like,
                                                               std::string_view like_path,
                                                               uint32_t frames) {
    auto added = Document::AddAnimation(archive_, name, like.archive_, like_path, frames);
    if (!added) return Support::Unexpected(added.error());
    outline_ = Outline::Build(archive_);
    return *added;
}

}
