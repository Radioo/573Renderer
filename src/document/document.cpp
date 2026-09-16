#include "document/document.h"

#include "document/entries.h"
#include "document/entry_edit.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
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

Support::Expected<void, std::string> File::RemoveImage(std::string_view name) {
    auto removed = Document::RemoveImage(archive_, name);
    if (!removed) return Support::Unexpected(removed.error());
    outline_ = Outline::Build(archive_);
    return {};
}

}
