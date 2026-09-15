#pragma once

#include "formats/binary_xml.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Ifs {

constexpr uint16_t kFlagManifestMd5 = 0x2;
constexpr uint16_t kDefaultFlags = 0x3;

enum class EntryKind : uint8_t { Directory, File, Special };

struct Entry {
    EntryKind kind = EntryKind::File;
    std::string name;
    uint8_t type = 0;
    int32_t time = 0;
    uint8_t image = 0;
    uint32_t stored_offset = 0;
    uint32_t stored_size = 0;
    std::vector<uint8_t> bytes;
    std::vector<BinaryXml::Node> extra_nodes;
    std::vector<Entry> children;
    BinaryXml::Node special;
};

struct Archive {
    uint16_t flags = kDefaultFlags;
    uint32_t time = 0;
    uint32_t tree_size = 0;
    uint32_t stored_data_size = 0;
    uint8_t manifest_signature = BinaryXml::kSixBitNames;
    uint8_t manifest_encoding = 0;
    uint8_t root_type = 1;
    std::vector<Entry> entries;
};

[[nodiscard]] Support::Expected<Archive, std::string> Read(std::span<const uint8_t> bytes);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string> Write(const Archive& archive);

}
