#pragma once

#include <catch2/catch_test_macros.hpp>

#include "formats/afp_animation.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace SamplePackage {

inline BinaryXml::Node Attribute(const std::string& name, const std::string& value) {
    return BinaryXml::Node{.type = BinaryXml::Type::kAttribute,
                           .name = name,
                           .value = std::vector<uint8_t>(value.begin(), value.end()),
                           .attributes = {},
                           .children = {}};
}

inline std::vector<uint8_t> TextureListBytes() {
    BinaryXml::Node image{.type = BinaryXml::Type::kVoid,
                          .name = "image",
                          .value = {},
                          .attributes = {Attribute("name", "bg03")},
                          .children = {}};
    image.children.push_back(BinaryXml::Node{.type = BinaryXml::Type::k4U16,
                                             .name = "imgrect",
                                             .value = {0, 0, 0, 128, 0, 0, 0, 64},
                                             .attributes = {},
                                             .children = {}});
    BinaryXml::Node texture{.type = BinaryXml::Type::kVoid,
                            .name = "texture",
                            .value = {},
                            .attributes = {Attribute("format", "argb8888rev")},
                            .children = {image}};
    BinaryXml::Document doc;
    doc.root = BinaryXml::Node{.type = BinaryXml::Type::kVoid,
                               .name = "texturelist",
                               .value = {},
                               .attributes = {Attribute("compress", "avslz")},
                               .children = {texture}};
    return *BinaryXml::Write(doc);
}

inline std::vector<uint8_t> AnimationListBytes() {
    BinaryXml::Node listed{.type = BinaryXml::Type::kVoid,
                           .name = "afp",
                           .value = {},
                           .attributes = {Attribute("name", "intro")},
                           .children = {}};
    BinaryXml::Document doc;
    doc.root = BinaryXml::Node{.type = BinaryXml::Type::kVoid,
                               .name = "afplist",
                               .value = {},
                               .attributes = {},
                               .children = {listed}};
    return *BinaryXml::Write(doc);
}

inline AfpAnimation::Animation SampleAnimation() {
    AfpAnimation::Animation animation;
    animation.container_version = 8;
    animation.magic = {0xB2, 0xD0, 0xC1};
    animation.data_version = 0x200;
    animation.rect = {0, 1920, 0, 1080};
    animation.fps = 60 * 1024;
    animation.strings = {"", "loop"};
    animation.name = 0;
    animation.root.frames = {AfpAnimation::Frame{}, AfpAnimation::Frame{}, AfpAnimation::Frame{}};
    animation.root.labels = {AfpAnimation::Label{.frame = 2, .name = 1}};
    animation.stored_form =
        AfpAnimation::StoredForm{.strings_scrambled = false, .background_colour_swapped = false};
    return animation;
}

inline Ifs::Entry File(const std::string& name, std::vector<uint8_t> bytes) {
    Ifs::Entry entry;
    entry.kind = Ifs::EntryKind::File;
    entry.name = name;
    entry.type = BinaryXml::Type::k3S32;
    entry.stored_size = static_cast<uint32_t>(bytes.size());
    entry.bytes = std::move(bytes);
    return entry;
}

inline Ifs::Entry Directory(const std::string& name, std::vector<Ifs::Entry> children) {
    Ifs::Entry entry;
    entry.kind = Ifs::EntryKind::Directory;
    entry.name = name;
    entry.type = BinaryXml::Type::kVoid;
    entry.children = std::move(children);
    return entry;
}

inline Ifs::Entry InfoEntry() {
    Ifs::Entry entry;
    entry.kind = Ifs::EntryKind::Special;
    entry.name = "_info_";
    entry.special = BinaryXml::Node{.type = BinaryXml::Type::kVoid,
                                    .name = "_info_",
                                    .value = {},
                                    .attributes = {},
                                    .children = {}};
    entry.special.children.push_back(BinaryXml::Node{.type = BinaryXml::Type::kBin,
                                                     .name = "md5",
                                                     .value = std::vector<uint8_t>(16, 0),
                                                     .attributes = {},
                                                     .children = {}});
    entry.special.children.push_back(BinaryXml::Node{.type = BinaryXml::Type::kU32,
                                                     .name = "size",
                                                     .value = std::vector<uint8_t>(4, 0),
                                                     .attributes = {},
                                                     .children = {}});
    return entry;
}

inline Ifs::Archive SampleArchive() {
    const auto stored = AfpAnimation::WriteStored(SampleAnimation());
    REQUIRE(stored.has_value());
    Ifs::Archive archive;
    archive.entries.push_back(File("magic", {'N', 'G', 'P', 'F'}));
    archive.entries.push_back(
        Directory("tex", {File("texturelist_Exml", TextureListBytes()),
                          File(Ifs::HashedName("bg03"), std::vector<uint8_t>(16, 0))}));
    archive.entries.push_back(
        Directory("afp", {File("afplist_Exml", AnimationListBytes()),
                          File(Ifs::HashedName("intro"), stored->data),
                          Directory("bsi", {File(Ifs::HashedName("intro"), stored->script)})}));
    archive.entries.push_back(InfoEntry());
    return archive;
}

inline std::string HashPath(const std::string& logical_name) {
    return *Ifs::UnescapeName(Ifs::HashedName(logical_name));
}

}
