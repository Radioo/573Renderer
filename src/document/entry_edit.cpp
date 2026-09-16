#include "document/entry_edit.h"

#include "document/entries.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/texture_images.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::array<std::string_view, 4> kHashedDirectories{"tex", "afp", "afp/bsi", "geo"};

uint8_t TypeOfSiblings(const std::vector<Ifs::Entry>& siblings) {
    for (const Ifs::Entry& entry : siblings) {
        if (entry.kind == Ifs::EntryKind::File) return entry.type;
    }
    return BinaryXml::Type::k3S32;
}

}

Support::Expected<std::string, std::string> StoredName(std::string_view directory,
                                                       std::string_view logical_name) {
    if (logical_name.empty()) return Support::Unexpected(std::string("an entry needs a name"));
    if (std::ranges::find(kHashedDirectories, directory) != kHashedDirectories.end()) {
        return Ifs::HashedName(logical_name);
    }
    return Ifs::EscapeName(logical_name);
}

Support::Expected<void, std::string> AddEntry(Ifs::Archive& archive, std::string_view directory,
                                              std::string_view logical_name,
                                              std::vector<uint8_t> bytes) {
    std::vector<Ifs::Entry>* siblings = &archive.entries;
    if (!directory.empty()) {
        Ifs::Entry* parent = FindEntry(archive, directory);
        if (parent == nullptr || parent->kind != Ifs::EntryKind::Directory)
            return Support::Unexpected(std::string(directory) + " is not a directory here");
        siblings = &parent->children;
    }
    auto stored = StoredName(directory, logical_name);
    if (!stored) return Support::Unexpected(stored.error());
    if (std::ranges::find(*siblings, *stored, &Ifs::Entry::name) != siblings->end()) {
        return Support::Unexpected(JoinPath(directory, logical_name) +
                                   " is already in the package");
    }

    Ifs::Entry entry;
    entry.kind = Ifs::EntryKind::File;
    entry.name = *stored;
    entry.type = TypeOfSiblings(*siblings);
    entry.time = static_cast<int32_t>(archive.time);
    entry.stored_size = static_cast<uint32_t>(bytes.size());
    entry.bytes = std::move(bytes);
    siblings->push_back(std::move(entry));
    return {};
}

Support::Expected<void, std::string> ReplaceEntry(Ifs::Archive& archive, std::string_view path,
                                                  std::vector<uint8_t> bytes) {
    Ifs::Entry* entry = FindEntry(archive, path);
    if (entry == nullptr || entry->kind != Ifs::EntryKind::File)
        return Support::Unexpected(std::string(path) + " is not a file in the package");
    if (entry->super_index != 0)
        return Support::Unexpected(std::string(path) + " lives in a super image");
    entry->bytes = std::move(bytes);
    entry->stored_size = static_cast<uint32_t>(entry->bytes.size());
    return {};
}

Support::Expected<void, std::string> RemoveEntry(Ifs::Archive& archive, std::string_view path) {
    const std::size_t slash = path.rfind('/');
    const std::string_view directory =
        slash == std::string_view::npos ? std::string_view() : path.substr(0, slash);
    const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);

    std::vector<Ifs::Entry>* siblings = &archive.entries;
    if (!directory.empty()) {
        Ifs::Entry* parent = FindEntry(archive, directory);
        if (parent == nullptr || parent->kind != Ifs::EntryKind::Directory)
            return Support::Unexpected(std::string(directory) + " is not a directory here");
        siblings = &parent->children;
    }
    const auto gone = std::ranges::remove_if(*siblings, [&](const Ifs::Entry& entry) {
        const auto unescaped = Ifs::UnescapeName(entry.name);
        return (unescaped ? *unescaped : entry.name) == name;
    });
    if (gone.begin() == siblings->end())
        return Support::Unexpected(std::string(path) + " is not in the package");
    siblings->erase(gone.begin(), gone.end());
    return {};
}

namespace {

constexpr std::string_view kTextureDirectory = "tex";
constexpr std::string_view kTextureList = "texturelist.xml";
constexpr std::string_view kTextureListStored = "texturelist_Exml";
constexpr uint16_t kUvInset = 2;
constexpr uint32_t kSmallestImage = 2;
constexpr std::size_t kBgraBytes = 4;

BinaryXml::Node* FirstTexture(BinaryXml::Node& root) {
    for (BinaryXml::Node& node : root.children) {
        if (node.name == "texture") return &node;
    }
    return nullptr;
}

std::string AttributeText(const BinaryXml::Node& node, std::string_view name) {
    for (const BinaryXml::Node& attribute : node.attributes) {
        if (attribute.name != name) continue;
        std::string text(attribute.value.begin(), attribute.value.end());
        if (!text.empty() && text.back() == 0) text.pop_back();
        return text;
    }
    return {};
}

void SetAttribute(BinaryXml::Node& node, std::string_view name, std::string_view text) {
    for (BinaryXml::Node& attribute : node.attributes) {
        if (attribute.name != name) continue;
        attribute.value.assign(text.begin(), text.end());
        return;
    }
    node.attributes.push_back(BinaryXml::Node{.type = BinaryXml::Type::kAttribute,
                                              .name = std::string(name),
                                              .value = {text.begin(), text.end()},
                                              .attributes = {},
                                              .children = {}});
}

BinaryXml::Node Words(std::string_view name, uint8_t type, const std::vector<uint16_t>& values) {
    std::vector<uint8_t> bytes;
    for (const uint16_t value : values)
        BigEndian::AppendU16(bytes, value);
    return BinaryXml::Node{.type = type,
                           .name = std::string(name),
                           .value = std::move(bytes),
                           .attributes = {},
                           .children = {}};
}

BinaryXml::Node ImageNode(std::string_view name, uint32_t width, uint32_t height) {
    const auto right = static_cast<uint16_t>(2 * width);
    const auto bottom = static_cast<uint16_t>(2 * height);
    BinaryXml::Node image{.type = BinaryXml::Type::kVoid,
                          .name = "image",
                          .value = {},
                          .attributes = {},
                          .children = {}};
    SetAttribute(image, "name", name);
    image.children.push_back(Words("uvrect", BinaryXml::Type::k4U16,
                                   {kUvInset, static_cast<uint16_t>(right - kUvInset), kUvInset,
                                    static_cast<uint16_t>(bottom - kUvInset)}));
    image.children.push_back(Words("imgrect", BinaryXml::Type::k4U16, {0, right, 0, bottom}));
    return image;
}

Ifs::Entry* TextureListEntry(Ifs::Archive& archive) {
    Ifs::Entry* textures = FindEntry(archive, kTextureDirectory);
    if (textures == nullptr) return nullptr;
    const auto found =
        std::ranges::find(textures->children, std::string(kTextureListStored), &Ifs::Entry::name);
    return found == textures->children.end() ? nullptr : &*found;
}

}

Support::Expected<void, std::string> AddImage(Ifs::Archive& archive, std::string_view name,
                                              uint32_t width, uint32_t height,
                                              std::span<const uint8_t> bgra) {
    if (width < kSmallestImage || height < kSmallestImage)
        return Support::Unexpected(std::string("an image is at least two pixels on a side"));
    if (bgra.size() != static_cast<std::size_t>(width) * height * kBgraBytes)
        return Support::Unexpected(std::string("the pixels do not match the size given"));
    Ifs::Entry* list = TextureListEntry(archive);
    if (list == nullptr)
        return Support::Unexpected(std::string(kTextureList) + " is not in the package");
    auto document = BinaryXml::Read(list->bytes);
    if (!document) return Support::Unexpected(document.error());
    BinaryXml::Node* like = FirstTexture(document->root);
    if (like == nullptr)
        return Support::Unexpected(std::string(kTextureList) + " lists no texture to copy");

    const auto listed = TextureImages::ReadList(*document);
    if (!listed) return Support::Unexpected(listed.error());
    if (std::ranges::find(listed->images, std::string(name), &TextureImages::Image::name) !=
        listed->images.end()) {
        return Support::Unexpected(std::string(name) + " is already in the texture list");
    }

    const std::string format = AttributeText(*like, "format");
    auto pixels = TextureImages::BgraToPixels(format, bgra);
    if (!pixels) return Support::Unexpected(pixels.error());

    BinaryXml::Node texture = *like;
    texture.children.clear();
    SetAttribute(texture, "name", name);
    texture.children.push_back(
        Words("size", BinaryXml::Type::k2U16,
              {static_cast<uint16_t>(width), static_cast<uint16_t>(height)}));
    texture.children.push_back(ImageNode(name, width, height));
    document->root.children.push_back(std::move(texture));

    auto written = BinaryXml::Write(*document);
    if (!written) return Support::Unexpected(written.error());
    auto replaced =
        ReplaceEntry(archive, JoinPath(kTextureDirectory, kTextureList), std::move(*written));
    if (!replaced) return Support::Unexpected(replaced.error());

    const TextureImages::Blob blob{.storage = listed->compressed ? TextureImages::Storage::Lz77
                                                                 : TextureImages::Storage::Plain,
                                   .pixels = std::move(*pixels)};
    return AddEntry(archive, kTextureDirectory, name, TextureImages::EncodeBlob(blob));
}

Support::Expected<void, std::string> RemoveImage(Ifs::Archive& archive, std::string_view name) {
    Ifs::Entry* list = TextureListEntry(archive);
    if (list == nullptr)
        return Support::Unexpected(std::string(kTextureList) + " is not in the package");
    auto document = BinaryXml::Read(list->bytes);
    if (!document) return Support::Unexpected(document.error());

    bool found = false;
    for (BinaryXml::Node& texture : document->root.children) {
        if (texture.name != "texture") continue;
        const auto gone =
            std::ranges::remove_if(texture.children, [&](const BinaryXml::Node& node) {
                return node.name == "image" && AttributeText(node, "name") == name;
            });
        if (gone.begin() == texture.children.end()) continue;
        texture.children.erase(gone.begin(), gone.end());
        found = true;
    }
    if (!found) return Support::Unexpected(std::string(name) + " is not in the texture list");

    const auto empty =
        std::ranges::remove_if(document->root.children, [](const BinaryXml::Node& n) {
            if (n.name != "texture") return false;
            return std::ranges::none_of(n.children,
                                        [](const BinaryXml::Node& c) { return c.name == "image"; });
        });
    document->root.children.erase(empty.begin(), empty.end());

    auto written = BinaryXml::Write(*document);
    if (!written) return Support::Unexpected(written.error());
    auto replaced =
        ReplaceEntry(archive, JoinPath(kTextureDirectory, kTextureList), std::move(*written));
    if (!replaced) return Support::Unexpected(replaced.error());
    return RemoveEntry(archive, JoinPath(kTextureDirectory, Ifs::HashedName(name)));
}

}
