#include "document/animation_entries.h"

#include "document/animation_strings.h"
#include "document/animation_template.h"
#include "document/entries.h"
#include "document/entry_edit.h"
#include "formats/afp_animation.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kAnimationDirectory = "afp";
constexpr std::string_view kScriptDirectory = "afp/bsi";
constexpr std::string_view kShapeDirectory = "geo";
constexpr std::string_view kAnimationList = "afp/afplist.xml";
constexpr std::string_view kEntryName = "afp";
constexpr std::string_view kNameAttribute = "name";
constexpr std::string_view kShapeList = "geo";
constexpr std::string_view kListFile = "afplist.xml";
constexpr std::array<std::string_view, 2> kAnimationDirectories{kAnimationDirectory,
                                                                kScriptDirectory};
constexpr uint8_t kShapeListType = BinaryXml::Type::kU16 | BinaryXml::kArrayFlag;
constexpr std::size_t kLongestName = 52;
constexpr std::size_t kShapeIdBytes = 2;
constexpr char kFirstPrintable = ' ';
constexpr char kLastPrintable = '~';

std::string Text(const std::vector<uint8_t>& value) {
    std::string text(value.begin(), value.end());
    if (!text.empty() && text.back() == '\0') text.pop_back();
    return text;
}

const BinaryXml::Node* NameOf(const BinaryXml::Node& listed) {
    const auto found = std::ranges::find(listed.attributes, kNameAttribute, &BinaryXml::Node::name);
    return found == listed.attributes.end() ? nullptr : &*found;
}

bool IsListing(const BinaryXml::Node& node) {
    return node.name == kEntryName && NameOf(node) != nullptr;
}

std::string ListedName(const BinaryXml::Node& listing) {
    return Text(NameOf(listing)->value);
}

Support::Expected<std::string, std::string> PathOf(std::string_view directory,
                                                   std::string_view logical_name) {
    auto stored = StoredName(directory, logical_name);
    if (!stored) return Support::Unexpected(stored.error());
    auto file_name = Ifs::UnescapeName(*stored);
    if (!file_name) return Support::Unexpected(file_name.error());
    return JoinPath(directory, *file_name);
}

Support::Expected<BinaryXml::Document, std::string> ReadList(const Ifs::Archive& archive) {
    const Ifs::Entry* entry = FindEntry(archive, kAnimationList);
    if (entry == nullptr)
        return Support::Unexpected(std::string(kAnimationList) + " is not in the package");
    return BinaryXml::Read(entry->bytes);
}

Support::Expected<AfpAnimation::Animation, std::string> ReadAt(const Ifs::Archive& archive,
                                                               std::string_view path) {
    const Ifs::Entry* stored = FindEntry(archive, path);
    const Ifs::Entry* script = FindEntry(archive, ScriptPath(path));
    if (stored == nullptr || script == nullptr)
        return Support::Unexpected(std::string(path) + " is not an animation");
    return AfpAnimation::ReadStored(stored->bytes, script->bytes);
}

Support::Expected<void, std::string> CheckName(std::string_view name) {
    if (name.empty()) return Support::Unexpected(std::string("an animation needs a name"));
    if (name.size() > kLongestName) {
        return Support::Unexpected("an animation name is at most " + std::to_string(kLongestName) +
                                   " characters");
    }
    const bool plain = std::ranges::all_of(name, [](char c) {
        return c >= kFirstPrintable && c <= kLastPrintable && c != '/' && c != '\\';
    });
    if (!plain) {
        return Support::Unexpected(
            std::string("an animation name is printable ASCII without slashes"));
    }
    return {};
}

Support::Expected<BinaryXml::Document, std::string> ListFor(const Ifs::Archive& archive,
                                                            const BinaryXml::Document& like_list) {
    if (FindEntry(archive, kAnimationList) != nullptr) return ReadList(archive);
    BinaryXml::Document fresh = like_list;
    fresh.root.children.clear();
    return fresh;
}

Support::Expected<std::vector<uint8_t>, std::string> Listed(const BinaryXml::Document& list,
                                                            const BinaryXml::Document& like_list,
                                                            std::string_view name,
                                                            const std::vector<uint16_t>& shapes) {
    const auto template_listing = std::ranges::find_if(like_list.root.children, IsListing);
    if (template_listing == like_list.root.children.end())
        return Support::Unexpected(std::string("the template's animation list names nothing"));
    const bool taken = std::ranges::any_of(list.root.children, [&](const BinaryXml::Node& node) {
        return IsListing(node) && ListedName(node) == name;
    });
    if (taken) return Support::Unexpected(std::string(name) + " is already an animation here");

    BinaryXml::Document edited = list;
    BinaryXml::Node listing = *template_listing;
    BinaryXml::Node geo{.type = kShapeListType,
                        .name = std::string(kShapeList),
                        .value = {},
                        .attributes = {},
                        .children = {}};
    const auto template_geo =
        std::ranges::find(listing.children, kShapeList, &BinaryXml::Node::name);
    if (template_geo != listing.children.end()) geo.type = template_geo->type;
    listing.children.clear();
    for (const uint16_t id : shapes)
        BigEndian::AppendU16(geo.value, id);
    if (!shapes.empty()) listing.children.push_back(std::move(geo));
    BinaryXml::Node named = *NameOf(listing);
    const bool terminated = !named.value.empty() && named.value.back() == 0;
    named.value.assign(name.begin(), name.end());
    if (terminated) named.value.push_back(0);
    listing.attributes = {std::move(named)};
    edited.root.children.push_back(std::move(listing));
    return BinaryXml::Write(edited);
}

Support::Expected<void, std::string> CopyShapes(Ifs::Archive& archive,
                                                const Ifs::Archive& like_archive,
                                                std::string_view like_name, std::string_view name,
                                                const std::vector<uint16_t>& shapes) {
    if (shapes.empty()) return {};
    auto directory = EnsureDirectory(archive, kShapeDirectory);
    if (!directory) return Support::Unexpected(directory.error());
    for (const uint16_t id : shapes) {
        const auto source = PathOf(kShapeDirectory, std::format("{}_shape{}", like_name, id));
        if (!source) return Support::Unexpected(source.error());
        const Ifs::Entry* shape = FindEntry(like_archive, *source);
        if (shape == nullptr) {
            return Support::Unexpected(
                std::format("the template has no shape file for shape {}", id));
        }
        auto added =
            AddEntry(archive, kShapeDirectory, std::format("{}_shape{}", name, id), shape->bytes);
        if (!added) return Support::Unexpected(added.error());
    }
    return {};
}

Support::Expected<std::string, std::string> ListedNameAt(const BinaryXml::Document& list,
                                                         std::string_view path) {
    for (const BinaryXml::Node& listing : list.root.children) {
        if (!IsListing(listing)) continue;
        std::string name = ListedName(listing);
        const auto listed_path = PathOf(kAnimationDirectory, name);
        if (listed_path && *listed_path == path) return name;
    }
    return Support::Unexpected(std::string(path) + " is not a listed animation");
}

Support::Expected<void, std::string> CheckFree(const BinaryXml::Document& list,
                                               std::string_view name) {
    const std::string folded = FoldedName(name);
    const bool taken = std::ranges::any_of(list.root.children, [&](const BinaryXml::Node& node) {
        return IsListing(node) && FoldedName(ListedName(node)) == folded;
    });
    if (taken) {
        return Support::Unexpected(std::string(name) +
                                   " is already an animation here, ignoring case");
    }
    return {};
}

Support::Expected<std::vector<uint8_t>, std::string>
Relisted(const BinaryXml::Document& list, std::string_view old_name, std::string_view name) {
    BinaryXml::Document edited = list;
    for (BinaryXml::Node& listing : edited.root.children) {
        if (!IsListing(listing) || ListedName(listing) != old_name) continue;
        for (BinaryXml::Node& attribute : listing.attributes) {
            if (attribute.name != kNameAttribute) continue;
            const bool terminated = !attribute.value.empty() && attribute.value.back() == 0;
            attribute.value.assign(name.begin(), name.end());
            if (terminated) attribute.value.push_back(0);
        }
    }
    return BinaryXml::Write(edited);
}

std::vector<uint16_t> ListedShapes(const BinaryXml::Document& list, std::string_view name) {
    std::vector<uint16_t> shapes;
    for (const BinaryXml::Node& listing : list.root.children) {
        if (!IsListing(listing) || ListedName(listing) != name) continue;
        for (const BinaryXml::Node& child : listing.children) {
            if (child.name != kShapeList) continue;
            for (std::size_t at = 0; at + kShapeIdBytes <= child.value.size(); at += kShapeIdBytes)
                shapes.push_back(BigEndian::ReadU16(child.value, at));
        }
    }
    return shapes;
}

void RenameOwnExport(AfpAnimation::Animation& animation, std::string_view old_name,
                     std::string_view name) {
    const std::string folded = FoldedName(old_name);
    const auto own = std::ranges::find_if(animation.exports, [&](const AfpAnimation::Export& one) {
        return FoldedName(StringText(animation, one.name)) == folded;
    });
    animation.name = InternString(animation, name);
    if (own != animation.exports.end()) {
        const uint16_t tag = own->tag;
        animation.exports.erase(own);
        InsertExport(animation, tag, name);
    }
    CompactStrings(animation);
}

Support::Expected<void, std::string> MoveShapes(Ifs::Archive& archive,
                                                const std::vector<uint16_t>& shapes,
                                                std::string_view old_name, std::string_view name) {
    for (const uint16_t id : shapes) {
        const auto from = PathOf(kShapeDirectory, std::format("{}_shape{}", old_name, id));
        if (!from) return Support::Unexpected(from.error());
        const Ifs::Entry* shape = FindEntry(archive, *from);
        if (shape == nullptr) continue;
        std::vector<uint8_t> bytes = shape->bytes;
        auto gone = RemoveEntry(archive, *from);
        if (!gone) return Support::Unexpected(gone.error());
        auto added = AddEntry(archive, kShapeDirectory, std::format("{}_shape{}", name, id),
                              std::move(bytes));
        if (!added) return Support::Unexpected(added.error());
    }
    return {};
}

struct Unlisted {
    std::string name;
    std::vector<uint16_t> shapes;
    std::vector<uint8_t> list;
};

Support::Expected<Unlisted, std::string> Unlist(const BinaryXml::Document& list,
                                                std::string_view path) {
    Unlisted unlisted;
    BinaryXml::Document edited = list;
    std::vector<BinaryXml::Node>& listings = edited.root.children;
    for (auto it = listings.begin(); it != listings.end();) {
        if (!IsListing(*it)) {
            ++it;
            continue;
        }
        const std::string name = ListedName(*it);
        const auto listed_path = PathOf(kAnimationDirectory, name);
        if (!listed_path || *listed_path != path) {
            ++it;
            continue;
        }
        unlisted.name = name;
        for (const BinaryXml::Node& child : it->children) {
            if (child.name != kShapeList) continue;
            for (std::size_t at = 0; at + kShapeIdBytes <= child.value.size(); at += kShapeIdBytes)
                unlisted.shapes.push_back(BigEndian::ReadU16(child.value, at));
        }
        it = listings.erase(it);
    }
    if (unlisted.name.empty())
        return Support::Unexpected(std::string(path) + " is not a listed animation");
    auto written = BinaryXml::Write(edited);
    if (!written) return Support::Unexpected(written.error());
    unlisted.list = std::move(*written);
    return unlisted;
}

Support::Expected<void, std::string> CheckNotImported(const Ifs::Archive& archive,
                                                      const BinaryXml::Document& list,
                                                      std::string_view removed,
                                                      std::string_view edit) {
    for (const BinaryXml::Node& listing : list.root.children) {
        if (!IsListing(listing) || ListedName(listing) == removed) continue;
        const std::string name = ListedName(listing);
        const auto path = PathOf(kAnimationDirectory, name);
        if (!path) return Support::Unexpected(path.error());
        const auto animation = ReadAt(archive, *path);
        if (!animation) return Support::Unexpected(animation.error());
        const bool imports =
            std::ranges::any_of(animation->imports, [&](const AfpAnimation::Import& imported) {
                return StringText(*animation, imported.movie) == removed;
            });
        if (imports) {
            return Support::Unexpected(name + " imports " + std::string(removed) +
                                       ", so it cannot be " + std::string(edit));
        }
    }
    return {};
}

}

Support::Expected<std::string, std::string>
AddAnimation(Ifs::Archive& archive, std::string_view name, const Ifs::Archive& like_archive,
             std::string_view like_path, uint32_t frames) {
    auto named = CheckName(name);
    if (!named) return Support::Unexpected(named.error());
    if (frames == 0) return Support::Unexpected(std::string("an animation needs a frame"));
    const auto like = ReadAt(like_archive, like_path);
    if (!like) return Support::Unexpected(like.error());
    const auto like_list = ReadList(like_archive);
    if (!like_list) return Support::Unexpected(like_list.error());
    const auto list = ListFor(archive, *like_list);
    if (!list) return Support::Unexpected(list.error());
    const auto made = EmptyLike(*like, name, frames);
    if (!made) return Support::Unexpected(made.error());
    auto listed = Listed(*list, *like_list, name, made->shapes);
    if (!listed) return Support::Unexpected(listed.error());
    auto path = PathOf(kAnimationDirectory, name);
    if (!path) return Support::Unexpected(path.error());
    auto written = AfpAnimation::WriteStored(made->animation);
    if (!written) return Support::Unexpected(written.error());

    Ifs::Archive edited = archive;
    for (const std::string_view directory : kAnimationDirectories) {
        auto ensured = EnsureDirectory(edited, directory);
        if (!ensured) return Support::Unexpected(ensured.error());
    }
    auto shapes =
        CopyShapes(edited, like_archive, StringText(*like, like->name), name, made->shapes);
    if (!shapes) return Support::Unexpected(shapes.error());
    auto data = AddEntry(edited, kAnimationDirectory, name, std::move(written->data));
    if (!data) return Support::Unexpected(data.error());
    auto order = AddEntry(edited, kScriptDirectory, name, std::move(written->script));
    if (!order) return Support::Unexpected(order.error());
    auto relisted = FindEntry(edited, kAnimationList) != nullptr
                        ? ReplaceEntry(edited, kAnimationList, std::move(*listed))
                        : AddEntry(edited, kAnimationDirectory, kListFile, std::move(*listed));
    if (!relisted) return Support::Unexpected(relisted.error());
    archive = std::move(edited);
    return *path;
}

Support::Expected<void, std::string> RemoveAnimation(Ifs::Archive& archive, std::string_view path) {
    const auto list = ReadList(archive);
    if (!list) return Support::Unexpected(list.error());
    auto unlisted = Unlist(*list, path);
    if (!unlisted) return Support::Unexpected(unlisted.error());
    auto free = CheckNotImported(archive, *list, unlisted->name, "removed");
    if (!free) return Support::Unexpected(free.error());

    Ifs::Archive edited = archive;
    auto data = RemoveEntry(edited, path);
    if (!data) return Support::Unexpected(data.error());
    auto order = RemoveEntry(edited, ScriptPath(path));
    if (!order) return Support::Unexpected(order.error());
    auto relisted = ReplaceEntry(edited, kAnimationList, std::move(unlisted->list));
    if (!relisted) return Support::Unexpected(relisted.error());
    for (const uint16_t id : unlisted->shapes) {
        const auto shape = PathOf(kShapeDirectory, std::format("{}_shape{}", unlisted->name, id));
        if (!shape) return Support::Unexpected(shape.error());
        if (FindEntry(edited, *shape) == nullptr) continue;
        auto gone = RemoveEntry(edited, *shape);
        if (!gone) return Support::Unexpected(gone.error());
    }
    archive = std::move(edited);
    return {};
}

Support::Expected<std::string, std::string>
RenameAnimation(Ifs::Archive& archive, std::string_view path, std::string_view name) {
    auto named = CheckName(name);
    if (!named) return Support::Unexpected(named.error());
    const auto list = ReadList(archive);
    if (!list) return Support::Unexpected(list.error());
    const auto old_name = ListedNameAt(*list, path);
    if (!old_name) return Support::Unexpected(old_name.error());
    if (*old_name == name) return std::string(path);
    if (FoldedName(*old_name) != FoldedName(name)) {
        auto free = CheckFree(*list, name);
        if (!free) return Support::Unexpected(free.error());
    }
    auto imported = CheckNotImported(archive, *list, *old_name, "renamed");
    if (!imported) return Support::Unexpected(imported.error());
    auto animation = ReadAt(archive, path);
    if (!animation) return Support::Unexpected(animation.error());
    RenameOwnExport(*animation, *old_name, name);
    auto written = AfpAnimation::WriteStored(*animation);
    if (!written) return Support::Unexpected(written.error());
    auto listed = Relisted(*list, *old_name, name);
    if (!listed) return Support::Unexpected(listed.error());
    auto new_path = PathOf(kAnimationDirectory, name);
    if (!new_path) return Support::Unexpected(new_path.error());

    Ifs::Archive edited = archive;
    auto data = RemoveEntry(edited, path);
    if (!data) return Support::Unexpected(data.error());
    auto order = RemoveEntry(edited, ScriptPath(path));
    if (!order) return Support::Unexpected(order.error());
    auto added = AddEntry(edited, kAnimationDirectory, name, std::move(written->data));
    if (!added) return Support::Unexpected(added.error());
    auto script = AddEntry(edited, kScriptDirectory, name, std::move(written->script));
    if (!script) return Support::Unexpected(script.error());
    auto shapes = MoveShapes(edited, ListedShapes(*list, *old_name), *old_name, name);
    if (!shapes) return Support::Unexpected(shapes.error());
    auto relisted = ReplaceEntry(edited, kAnimationList, std::move(*listed));
    if (!relisted) return Support::Unexpected(relisted.error());
    archive = std::move(edited);
    return *new_path;
}

}
