#include "document/image_shape.h"

#include "document/animation_strings.h"
#include "document/entries.h"
#include "document/entry_edit.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ge2d_shape.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kShapeDirectory = "geo";
constexpr std::string_view kAnimationList = "afp/afplist.xml";
constexpr std::string_view kTextureList = "tex/texturelist.xml";
constexpr std::string_view kVersion = "version.xml";
constexpr std::string_view kMeshShapes = "mesh";
constexpr uint32_t kShapeVersion = 0x00010000;
constexpr uint32_t kShapeValue = 0x00010100;
constexpr uint32_t kMeshFlags = 0x20;
constexpr uint8_t kTriangles = 4;
constexpr uint8_t kTexturedDraw = 0x3;
constexpr uint8_t kNoTexture = 0xFF;
constexpr uint16_t kTexturedShapeWord = 2;
constexpr uint16_t kLastCharacterId = 0xFFFE;
constexpr uint8_t kGeoType = BinaryXml::Type::kU16 | BinaryXml::kArrayFlag;
constexpr std::size_t kRectBytes = 8;
constexpr std::size_t kSizeBytes = 4;

uint32_t FloatBits(double value) {
    return std::bit_cast<uint32_t>(static_cast<float>(value));
}

std::string Text(const std::vector<uint8_t>& value) {
    std::string text(value.begin(), value.end());
    if (!text.empty() && text.back() == '\0') text.pop_back();
    return text;
}

std::string AttributeText(const BinaryXml::Node& node, std::string_view name) {
    for (const BinaryXml::Node& attribute : node.attributes) {
        if (attribute.name == name) return Text(attribute.value);
    }
    return {};
}

const BinaryXml::Node* ChildNamed(const BinaryXml::Node& node, std::string_view name) {
    const auto found = std::ranges::find(node.children, name, &BinaryXml::Node::name);
    return found == node.children.end() ? nullptr : &*found;
}

Support::Expected<BinaryXml::Document, std::string> ReadList(const Ifs::Archive& archive,
                                                             std::string_view path) {
    const Ifs::Entry* entry = FindEntry(archive, path);
    if (entry == nullptr) return Support::Unexpected(std::string(path) + " is not in the package");
    return BinaryXml::Read(entry->bytes);
}

std::optional<ImageArea> AreaIn(const BinaryXml::Node& texture, std::string_view image) {
    const BinaryXml::Node* size = ChildNamed(texture, "size");
    if (size == nullptr || size->value.size() != kSizeBytes) return std::nullopt;
    for (const BinaryXml::Node& node : texture.children) {
        if (node.name != "image" || AttributeText(node, "name") != image) continue;
        const BinaryXml::Node* uvrect = ChildNamed(node, "uvrect");
        if (uvrect == nullptr || uvrect->value.size() != kRectBytes) return std::nullopt;
        ImageArea area{.atlas_width = BigEndian::ReadU16(size->value, 0),
                       .atlas_height = BigEndian::ReadU16(size->value, 2),
                       .uvrect = {}};
        for (std::size_t i = 0; i < area.uvrect.size(); i++)
            area.uvrect.at(i) = BigEndian::ReadU16(uvrect->value, 2 * i);
        return area;
    }
    return std::nullopt;
}

Support::Expected<ImageArea, std::string> FindImage(const Ifs::Archive& archive,
                                                    std::string_view image) {
    const auto list = ReadList(archive, kTextureList);
    if (!list) return Support::Unexpected(list.error());
    for (const BinaryXml::Node& texture : list->root.children) {
        if (texture.name != "texture") continue;
        auto area = AreaIn(texture, image);
        if (area) return *area;
    }
    return Support::Unexpected(std::string(image) + " is not an image of this package");
}

bool MeshPackage(const Ifs::Archive& archive) {
    const auto version = ReadList(archive, kVersion);
    if (!version) return false;
    const BinaryXml::Node* shapes = ChildNamed(version->root, "shapetype");
    return shapes != nullptr && Text(shapes->value) == kMeshShapes;
}

Support::Expected<void, std::string> ListShape(Ifs::Archive& archive, std::string_view animation,
                                               uint16_t id) {
    auto list = ReadList(archive, kAnimationList);
    if (!list) return Support::Unexpected(list.error());
    const auto listed = std::ranges::find_if(list->root.children, [&](const BinaryXml::Node& n) {
        return n.name == "afp" && AttributeText(n, "name") == animation;
    });
    if (listed == list->root.children.end())
        return Support::Unexpected(std::string(animation) + " is not in the animation list");
    auto geo = std::ranges::find(listed->children, std::string("geo"), &BinaryXml::Node::name);
    if (geo == listed->children.end()) {
        listed->children.push_back(BinaryXml::Node{
            .type = kGeoType, .name = "geo", .value = {}, .attributes = {}, .children = {}});
        geo = listed->children.end() - 1;
    }
    BigEndian::AppendU16(geo->value, id);
    auto written = BinaryXml::Write(*list);
    if (!written) return Support::Unexpected(written.error());
    return ReplaceEntry(archive, kAnimationList, std::move(*written));
}

void EnsureShapeDirectory(Ifs::Archive& archive) {
    if (FindEntry(archive, kShapeDirectory) != nullptr) return;
    Ifs::Entry directory;
    directory.kind = Ifs::EntryKind::Directory;
    directory.name = std::string(kShapeDirectory);
    directory.type = BinaryXml::Type::kVoid;
    directory.time = static_cast<int32_t>(archive.time);
    for (const Ifs::Entry& sibling : archive.entries) {
        if (sibling.kind != Ifs::EntryKind::Directory) continue;
        directory.type = sibling.type;
        directory.time = sibling.time;
        break;
    }
    archive.entries.push_back(std::move(directory));
}

Support::Expected<AfpAnimation::Animation, std::string>
ReadAnimationAt(const Ifs::Archive& archive, std::string_view animation_path) {
    const std::string script_path = ScriptPath(animation_path);
    const Ifs::Entry* stored = FindEntry(archive, animation_path);
    const Ifs::Entry* script = script_path.empty() ? nullptr : FindEntry(archive, script_path);
    if (stored == nullptr || script == nullptr)
        return Support::Unexpected(std::string(animation_path) + " is not an animation");
    return AfpAnimation::ReadStored(stored->bytes, script->bytes);
}

Support::Expected<void, std::string> WriteShape(Ifs::Archive& archive, std::string_view animation,
                                                uint16_t id, std::string_view image) {
    const auto area = FindImage(archive, image);
    if (!area) return Support::Unexpected(area.error());
    const Ifs::Entry* magic = FindEntry(archive, "magic");
    if (magic == nullptr) return Support::Unexpected(std::string("the package has no magic"));
    const auto order = Ge2dShape::PackageByteOrder(magic->bytes);
    if (!order) return Support::Unexpected(order.error());
    auto bytes = Ge2dShape::Write(ImageQuad(image, *area, MeshPackage(archive)), *order);
    if (!bytes) return Support::Unexpected(bytes.error());
    EnsureShapeDirectory(archive);
    auto added = AddEntry(archive, kShapeDirectory, std::format("{}_shape{}", animation, id),
                          std::move(*bytes));
    if (!added) return Support::Unexpected(added.error());
    return ListShape(archive, animation, id);
}

}

Ge2dShape::Shape ImageQuad(std::string_view image, const ImageArea& area, bool mesh_package) {
    const double width = (area.uvrect[1] - area.uvrect[0]) / 2.0;
    const double height = (area.uvrect[3] - area.uvrect[2]) / 2.0;
    const double left = area.uvrect[0] / (2.0 * area.atlas_width);
    const double right = area.uvrect[1] / (2.0 * area.atlas_width);
    const double top = area.uvrect[2] / (2.0 * area.atlas_height);
    const double bottom = area.uvrect[3] / (2.0 * area.atlas_height);
    Ge2dShape::Shape shape;
    shape.unread_version = kShapeVersion;
    shape.unread_value = kShapeValue;
    shape.flags = mesh_package ? kMeshFlags : 0;
    if (mesh_package) {
        shape.rect = std::array<uint32_t, 4>{FloatBits(0), FloatBits(width), FloatBits(0),
                                             FloatBits(height)};
    }
    shape.vertices = {{FloatBits(0), FloatBits(0)},
                      {FloatBits(width), FloatBits(0)},
                      {FloatBits(0), FloatBits(height)},
                      {FloatBits(width), FloatBits(height)}};
    shape.uvs = {{FloatBits(left), FloatBits(top)},
                 {FloatBits(right), FloatBits(top)},
                 {FloatBits(left), FloatBits(bottom)},
                 {FloatBits(right), FloatBits(bottom)}};
    shape.texture_names = {std::string(image)};
    shape.primitives = {Ge2dShape::Primitive{.kind = kTriangles,
                                             .draw_flags = kTexturedDraw,
                                             .texture = 0,
                                             .second_texture = kNoTexture,
                                             .unread_bytes = {},
                                             .colour = {},
                                             .indices = {0, 1, 2, 2, 1, 3}}};
    return shape;
}

Support::Expected<uint16_t, std::string> NextCharacterId(const AfpAnimation::Animation& animation) {
    std::optional<uint16_t> highest;
    const auto note = [&highest](uint16_t id) {
        if (!highest || id > *highest) highest = id;
    };
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) note(sprite->id);
        if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) note(image->id);
        if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) note(shape->id);
    }
    for (const AfpAnimation::Import& imported : animation.imports) {
        for (const AfpAnimation::ImportedAsset& asset : imported.assets)
            note(asset.tag);
    }
    if (!highest) return uint16_t{0};
    if (*highest >= kLastCharacterId)
        return Support::Unexpected(std::string("the animation has no character id left"));
    return static_cast<uint16_t>(*highest + 1);
}

std::map<uint16_t, std::string> ShapeImages(const Ifs::Archive& archive,
                                            std::string_view animation_path) {
    std::map<uint16_t, std::string> images;
    const auto animation = ReadAnimationAt(archive, animation_path);
    const Ifs::Entry* magic = FindEntry(archive, "magic");
    if (!animation || magic == nullptr) return images;
    const auto order = Ge2dShape::PackageByteOrder(magic->bytes);
    if (!order) return images;
    const std::string name = StringText(*animation, animation->name);
    for (const AfpAnimation::Tag& tag : animation->root.tags) {
        const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body);
        if (shape == nullptr) continue;
        const auto stored =
            Ifs::UnescapeName(Ifs::HashedName(std::format("{}_shape{}", name, shape->id)));
        if (!stored) continue;
        const Ifs::Entry* entry = FindEntry(archive, JoinPath(kShapeDirectory, *stored));
        if (entry == nullptr) continue;
        const auto read = Ge2dShape::Read(entry->bytes, *order);
        if (read && read->texture_names.size() == 1) images[shape->id] = read->texture_names[0];
    }
    return images;
}

Support::Expected<uint16_t, std::string>
AddImageShape(Ifs::Archive& archive, std::string_view animation_path, std::string_view image) {
    auto animation = ReadAnimationAt(archive, animation_path);
    if (!animation) return Support::Unexpected(animation.error());
    if (animation->root.frames.empty())
        return Support::Unexpected(std::string("the animation has no frame to define a shape in"));
    const auto id = NextCharacterId(*animation);
    if (!id) return Support::Unexpected(id.error());
    const std::string name = StringText(*animation, animation->name);
    if (name.empty()) return Support::Unexpected(std::string("the animation has no name"));

    Ifs::Archive edited = archive;
    auto shaped = WriteShape(edited, name, *id, image);
    if (!shaped) return Support::Unexpected(shaped.error());
    InsertTag(animation->root, 0,
              AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = kTexturedShapeWord, .id = *id}});
    auto rewritten = AfpAnimation::WriteStored(*animation);
    if (!rewritten) return Support::Unexpected(rewritten.error());
    auto data = ReplaceEntry(edited, animation_path, std::move(rewritten->data));
    if (!data) return Support::Unexpected(data.error());
    auto order = ReplaceEntry(edited, ScriptPath(animation_path), std::move(rewritten->script));
    if (!order) return Support::Unexpected(order.error());
    archive = std::move(edited);
    return *id;
}

}
