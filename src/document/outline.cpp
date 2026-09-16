#include "document/outline.h"

#include "document/entries.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "formats/texture_images.h"
#include "support/expected.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kMagic = "magic";
constexpr std::string_view kVersion = "version.xml";
constexpr std::string_view kConverterVersion = "cversion";
constexpr std::string_view kTextureDirectory = "tex";
constexpr std::string_view kTextureList = "texturelist.xml";
constexpr std::string_view kAnimationDirectory = "afp";
constexpr std::string_view kAnimationList = "afplist.xml";
constexpr std::string_view kScriptDirectory = "bsi";
constexpr std::string_view kShapeDirectory = "geo";
constexpr std::string_view kAnimationListRoot = "afplist";
constexpr std::string_view kAnimationListEntry = "afp";
constexpr std::string_view kNameAttribute = "name";

Role RoleFor(Ifs::EntryKind kind, std::string_view parent, std::string_view name) {
    if (kind != Ifs::EntryKind::File) return Role::Unknown;
    if (parent.empty()) {
        if (name == kMagic) return Role::PackageMagic;
        if (name == kVersion || name == kConverterVersion) return Role::PackageVersion;
        return Role::Unknown;
    }
    if (parent == kTextureDirectory)
        return name == kTextureList ? Role::TextureList : Role::Texture;
    if (parent == kAnimationDirectory)
        return name == kAnimationList ? Role::AnimationList : Role::Animation;
    if (parent == JoinPath(kAnimationDirectory, kScriptDirectory)) return Role::ByteOrderScript;
    if (parent == kShapeDirectory) return Role::Shape;
    return Role::Unknown;
}

std::vector<Node> Walk(const std::vector<Ifs::Entry>& entries, std::string_view parent) {
    std::vector<Node> nodes;
    nodes.reserve(entries.size());
    for (const Ifs::Entry& entry : entries) {
        const auto unescaped = Ifs::UnescapeName(entry.name);
        const std::string name = unescaped ? *unescaped : entry.name;
        Node node{.kind = entry.kind,
                  .role = RoleFor(entry.kind, parent, name),
                  .name = name,
                  .stored_name = entry.name,
                  .path = JoinPath(parent, name),
                  .stored_size = entry.stored_size,
                  .time = entry.time,
                  .super_index = {},
                  .children = {}};
        if (entry.super_index != 0) node.super_index = entry.super_index;
        if (entry.kind == Ifs::EntryKind::Directory)
            node.children = Walk(entry.children, node.path);
        nodes.push_back(std::move(node));
    }
    return nodes;
}

const Ifs::Entry* FindStored(const std::vector<Ifs::Entry>& entries, std::string_view stored_name) {
    const auto found = std::ranges::find(entries, stored_name, &Ifs::Entry::name);
    return found == entries.end() ? nullptr : &*found;
}

Node* FindNode(std::vector<Node>& nodes, std::string_view path) {
    for (Node& node : nodes) {
        if (node.path == path) return &node;
        if (node.kind != Ifs::EntryKind::Directory) continue;
        if (Node* found = FindNode(node.children, path); found != nullptr) return found;
    }
    return nullptr;
}

const Node* FindNode(const std::vector<Node>& nodes, std::string_view path) {
    for (const Node& node : nodes) {
        if (node.path == path) return &node;
        if (node.kind != Ifs::EntryKind::Directory) continue;
        if (const Node* found = FindNode(node.children, path); found != nullptr) return found;
    }
    return nullptr;
}

std::string AttributeText(const BinaryXml::Node& node, std::string_view name) {
    for (const BinaryXml::Node& attribute : node.attributes) {
        if (attribute.name != name) continue;
        std::string text(attribute.value.begin(), attribute.value.end());
        if (!text.empty() && text.back() == '\0') text.pop_back();
        return text;
    }
    return {};
}

Support::Expected<BinaryXml::Document, std::string>
ListDocument(const std::vector<Ifs::Entry>& entries, std::string_view logical_name) {
    const auto escaped = Ifs::EscapeName(logical_name);
    if (!escaped) return Support::Unexpected(escaped.error());
    const Ifs::Entry* entry = FindStored(entries, *escaped);
    if (entry == nullptr)
        return Support::Unexpected(std::string(logical_name) + " is not in the package");
    return BinaryXml::Read(entry->bytes);
}

}

std::string_view RoleName(Role role) {
    switch (role) {
    case Role::PackageMagic:
        return "package magic";
    case Role::PackageVersion:
        return "package version";
    case Role::TextureList:
        return "texture list";
    case Role::Texture:
        return "texture";
    case Role::AnimationList:
        return "animation list";
    case Role::Animation:
        return "animation";
    case Role::ByteOrderScript:
        return "byte order script";
    case Role::Shape:
        return "shape";
    case Role::Unknown:
        break;
    }
    return "entry";
}

std::vector<Field> Fields(const Details& details) {
    std::vector<Field> fields;
    fields.push_back(Field{.name = "Name", .value = details.name});
    fields.push_back(Field{.name = "Path", .value = details.path});
    if (details.stored_name != details.name)
        fields.push_back(Field{.name = "Stored as", .value = details.stored_name});
    fields.push_back(Field{.name = "Kind", .value = std::string(RoleName(details.role))});
    fields.push_back(Field{.name = "Stored size", .value = std::to_string(details.stored_size)});
    fields.push_back(Field{.name = "Time", .value = std::to_string(details.time)});
    if (details.super_index) {
        fields.push_back(
            Field{.name = "Super image", .value = std::to_string(*details.super_index)});
    }
    if (details.texture) {
        fields.push_back(Field{.name = "Format", .value = details.texture->format});
        fields.push_back(Field{.name = "Pixels",
                               .value = std::to_string(details.texture->width) + " x " +
                                        std::to_string(details.texture->height)});
    }
    if (!details.animation) return fields;
    fields.push_back(
        Field{.name = "Frames", .value = std::to_string(details.animation->frame_count)});
    fields.push_back(
        Field{.name = "Depths", .value = std::to_string(details.animation->depths.size())});
    std::string labels;
    for (const AnimationLabel& label : details.animation->labels) {
        if (!labels.empty()) labels += ", ";
        labels += label.name + " at " + std::to_string(label.frame);
    }
    fields.push_back(Field{.name = "Labels", .value = labels});
    return fields;
}

void Outline::NameTextures(const Ifs::Archive& archive) {
    const Ifs::Entry* textures = FindStored(archive.entries, kTextureDirectory);
    if (textures == nullptr) return;
    const auto document = ListDocument(textures->children, kTextureList);
    const auto list = document ? TextureImages::ReadList(*document)
                               : Support::Expected<TextureImages::List, std::string>(
                                     Support::Unexpected(document.error()));
    if (!list) {
        problems_.push_back(std::string(kTextureList) + ": " + list.error());
        return;
    }
    for (const TextureImages::Image& image : list->images) {
        const auto unescaped = Ifs::UnescapeName(Ifs::HashedName(image.name));
        if (!unescaped) continue;
        const std::string path = JoinPath(kTextureDirectory, *unescaped);
        Node* node = FindNode(nodes_, path);
        if (node == nullptr) {
            problems_.push_back("no tex entry for image " + image.name);
            continue;
        }
        node->name = image.name;
        textures_.emplace(
            path,
            TextureDetails{.format = image.format, .width = image.width, .height = image.height});
    }
}

void Outline::NameAnimations(const Ifs::Archive& archive) {
    const Ifs::Entry* animations = FindStored(archive.entries, kAnimationDirectory);
    if (animations == nullptr) return;
    const auto document = ListDocument(animations->children, kAnimationList);
    if (!document) {
        problems_.push_back(std::string(kAnimationList) + ": " + document.error());
        return;
    }
    if (document->root.name != kAnimationListRoot) {
        problems_.push_back(std::string(kAnimationList) + " is rooted at " + document->root.name);
        return;
    }
    for (const BinaryXml::Node& listed : document->root.children) {
        if (listed.name != kAnimationListEntry) continue;
        const std::string name = AttributeText(listed, kNameAttribute);
        const auto unescaped = Ifs::UnescapeName(Ifs::HashedName(name));
        if (!unescaped) continue;
        Node* node = FindNode(nodes_, JoinPath(kAnimationDirectory, *unescaped));
        if (node == nullptr) {
            problems_.push_back("no afp entry for animation " + name);
            continue;
        }
        node->name = name;
        Node* script =
            FindNode(nodes_, JoinPath(JoinPath(kAnimationDirectory, kScriptDirectory), *unescaped));
        if (script != nullptr) script->name = name;
    }
}

Outline Outline::Build(const Ifs::Archive& archive) {
    Outline outline;
    outline.nodes_ = Walk(archive.entries, "");
    outline.NameTextures(archive);
    outline.NameAnimations(archive);
    return outline;
}

Support::Expected<Details, std::string> Outline::Describe(const Ifs::Archive& archive,
                                                          std::string_view path) const {
    const Node* node = FindNode(nodes_, path);
    if (node == nullptr) return Support::Unexpected(std::string(path) + " is not in the package");
    Details details{.role = node->role,
                    .name = node->name,
                    .stored_name = node->stored_name,
                    .path = node->path,
                    .stored_size = node->stored_size,
                    .time = node->time,
                    .super_index = node->super_index,
                    .texture = {},
                    .animation = {}};
    if (node->role == Role::Texture) {
        const auto found = textures_.find(node->path);
        if (found == textures_.end()) {
            return Support::Unexpected(node->path + " is not listed in " +
                                       std::string(kTextureList));
        }
        details.texture = found->second;
        return details;
    }
    if (node->role != Role::Animation) return details;
    const Ifs::Entry* entry = FindEntry(archive, node->path);
    const std::string script_path = ScriptPath(node->path);
    const Ifs::Entry* script = script_path.empty() ? nullptr : FindEntry(archive, script_path);
    if (entry == nullptr || script == nullptr)
        return Support::Unexpected(node->path + " has no byte order script at " + script_path);
    const auto animation = AfpAnimation::ReadStored(entry->bytes, script->bytes);
    if (!animation) return Support::Unexpected(node->path + ": " + animation.error());
    AnimationDetails read{.frame_count = static_cast<uint32_t>(animation->root.frames.size()),
                          .labels = {},
                          .depths = DepthRows(animation->root)};
    for (const AfpAnimation::Label& label : animation->root.labels) {
        const std::string name =
            label.name < animation->strings.size() ? animation->strings[label.name] : std::string();
        read.labels.push_back(AnimationLabel{.name = name, .frame = label.frame});
    }
    details.animation = std::move(read);
    return details;
}

}
