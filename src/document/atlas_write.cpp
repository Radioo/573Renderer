#include "document/atlas_write.h"

#include "document/atlas.h"
#include "document/entries.h"
#include "document/entry_edit.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/texture_images.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kTextureDirectory = "tex";
constexpr std::string_view kTextureList = "texturelist.xml";
constexpr std::string_view kTextureListStored = "texturelist_Exml";
constexpr uint16_t kUvInset = 2;
constexpr uint32_t kGuard = 1;
constexpr std::size_t kBgraBytes = 4;

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

BinaryXml::Node PlacedImage(const AtlasPlacement& placed) {
    const auto left = static_cast<uint16_t>(2 * placed.x);
    const auto right = static_cast<uint16_t>(2 * (placed.x + placed.width));
    const auto top = static_cast<uint16_t>(2 * placed.y);
    const auto bottom = static_cast<uint16_t>(2 * (placed.y + placed.height));
    BinaryXml::Node image{.type = BinaryXml::Type::kVoid,
                          .name = "image",
                          .value = {},
                          .attributes = {},
                          .children = {}};
    SetAttribute(image, "name", placed.name);
    image.children.push_back(
        Words("uvrect", BinaryXml::Type::k4U16,
              {static_cast<uint16_t>(left + kUvInset), static_cast<uint16_t>(right - kUvInset),
               static_cast<uint16_t>(top + kUvInset), static_cast<uint16_t>(bottom - kUvInset)}));
    image.children.push_back(Words("imgrect", BinaryXml::Type::k4U16, {left, right, top, bottom}));
    return image;
}

Ifs::Entry* TextureListEntry(Ifs::Archive& archive) {
    Ifs::Entry* textures = FindEntry(archive, kTextureDirectory);
    if (textures == nullptr) return nullptr;
    const auto found =
        std::ranges::find(textures->children, std::string(kTextureListStored), &Ifs::Entry::name);
    return found == textures->children.end() ? nullptr : &*found;
}

Support::Expected<void, std::string> WriteImageEntries(Ifs::Archive& archive, const Atlas& atlas,
                                                       std::span<const LoadedImage> images,
                                                       const std::string& format, bool compressed) {
    for (std::size_t i = 0; i < atlas.images.size(); i++) {
        const AtlasPlacement& placed = atlas.images[i];
        if (images[i].width != placed.width || images[i].height != placed.height)
            return Support::Unexpected(placed.name + " does not match the cell it was given");
        auto pixels = TextureImages::BgraToPixels(format, images[i].bgra);
        if (!pixels) return Support::Unexpected(pixels.error());
        const TextureImages::Blob blob{.storage = compressed ? TextureImages::Storage::Lz77
                                                             : TextureImages::Storage::Plain,
                                       .pixels = std::move(*pixels)};
        std::vector<uint8_t> bytes = TextureImages::EncodeBlob(blob);
        auto stored = StoredName(kTextureDirectory, placed.name);
        if (!stored) return Support::Unexpected(stored.error());
        const std::string path = std::string(kTextureDirectory) + "/" + *stored;
        if (FindEntry(archive, path) != nullptr) {
            auto again = ReplaceEntry(archive, path, std::move(bytes));
            if (!again) return Support::Unexpected(again.error());
            continue;
        }
        auto added = AddEntry(archive, kTextureDirectory, placed.name, std::move(bytes));
        if (!added) return Support::Unexpected(added.error());
    }
    return {};
}

}

LoadedImage WithGuardRing(const LoadedImage& image) {
    LoadedImage out{
        .width = image.width + (2 * kGuard), .height = image.height + (2 * kGuard), .bgra = {}};
    out.bgra.resize(static_cast<std::size_t>(out.width) * out.height * kBgraBytes);
    for (uint32_t y = 0; y < out.height; y++) {
        const uint32_t source_y = std::clamp(y, kGuard, image.height + kGuard - 1) - kGuard;
        for (uint32_t x = 0; x < out.width; x++) {
            const uint32_t source_x = std::clamp(x, kGuard, image.width + kGuard - 1) - kGuard;
            const std::size_t from =
                ((static_cast<std::size_t>(source_y) * image.width) + source_x) * kBgraBytes;
            const std::size_t to = ((static_cast<std::size_t>(y) * out.width) + x) * kBgraBytes;
            std::copy_n(image.bgra.begin() + static_cast<std::ptrdiff_t>(from), kBgraBytes,
                        out.bgra.begin() + static_cast<std::ptrdiff_t>(to));
        }
    }
    return out;
}

Support::Expected<void, std::string> WriteAtlas(Ifs::Archive& archive, std::string_view atlas_name,
                                                const Atlas& atlas,
                                                std::span<const LoadedImage> images) {
    if (images.size() != atlas.images.size())
        return Support::Unexpected(std::string("the atlas and its pixels do not match"));
    Ifs::Entry* list = TextureListEntry(archive);
    if (list == nullptr)
        return Support::Unexpected(std::string(kTextureList) + " is not in the package");
    auto document = BinaryXml::Read(list->bytes);
    if (!document) return Support::Unexpected(document.error());

    const auto standing =
        std::ranges::find_if(document->root.children, [atlas_name](const BinaryXml::Node& node) {
            return node.name == "texture" && AttributeText(node, "name") == atlas_name;
        });
    const auto like =
        std::ranges::find(document->root.children, std::string("texture"), &BinaryXml::Node::name);
    if (like == document->root.children.end())
        return Support::Unexpected(std::string(kTextureList) + " lists no texture to copy");

    BinaryXml::Node texture = standing != document->root.children.end() ? *standing : *like;
    const std::string format = AttributeText(texture, "format");
    texture.children.clear();
    SetAttribute(texture, "name", atlas_name);
    texture.children.push_back(
        Words("size", BinaryXml::Type::k2U16,
              {static_cast<uint16_t>(atlas.width), static_cast<uint16_t>(atlas.height)}));
    for (const AtlasPlacement& placed : atlas.images)
        texture.children.push_back(PlacedImage(placed));

    if (standing != document->root.children.end()) {
        *standing = std::move(texture);
    } else {
        document->root.children.push_back(std::move(texture));
    }

    const bool compressed = AttributeText(document->root, "compress") == "avslz";
    auto written = BinaryXml::Write(*document);
    if (!written) return Support::Unexpected(written.error());
    auto replaced =
        ReplaceEntry(archive, std::string(kTextureDirectory) + "/" + std::string(kTextureList),
                     std::move(*written));
    if (!replaced) return Support::Unexpected(replaced.error());

    return WriteImageEntries(archive, atlas, images, format, compressed);
}

}
