#include "formats/ifs_archive.h"

#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_digest.h"
#include "formats/ifs_layout.h"
#include "formats/ifs_names.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Ifs {

namespace {

constexpr std::size_t kTimeSlot = 8;
constexpr const char* kImageNodeName = "i";

using BinaryXml::Type::k2S32;
using BinaryXml::Type::k3S32;
using BinaryXml::Type::kS32;
using BinaryXml::Type::kVoid;

struct ReadContext {
    std::span<const uint8_t> data;
    int32_t header_time = 0;
};

Support::Expected<Entry, std::string> ConvertNode(const BinaryXml::Node& node,
                                                  const ReadContext& ctx);

Support::Expected<std::vector<Entry>, std::string>
ConvertChildren(const std::vector<BinaryXml::Node>& nodes, const ReadContext& ctx) {
    std::vector<Entry> entries;
    entries.reserve(nodes.size());
    for (const BinaryXml::Node& node : nodes) {
        auto entry = ConvertNode(node, ctx);
        if (!entry) return Support::Unexpected(entry.error());
        entries.push_back(std::move(*entry));
    }
    return entries;
}

Support::Expected<void, std::string> FillFile(const BinaryXml::Node& node, const ReadContext& ctx,
                                              Entry& file) {
    const std::size_t needed = node.type == k3S32 ? 12 : 8;
    if (node.value.size() != needed) {
        return Support::Unexpected("file node " + node.name + " has a malformed value");
    }
    file.kind = EntryKind::File;
    file.stored_offset = BigEndian::ReadU32(node.value, 0);
    file.stored_size = BigEndian::ReadU32(node.value, 4);
    file.time = node.type == k3S32 ? static_cast<int32_t>(BigEndian::ReadU32(node.value, kTimeSlot))
                                   : ctx.header_time;
    file.extra_nodes = node.children;
    const auto image = std::ranges::find_if(file.extra_nodes, [](const BinaryXml::Node& child) {
        return child.name == kImageNodeName && child.type == BinaryXml::Type::kU8;
    });
    if (image != file.extra_nodes.end() && image->value.size() == 1)
        file.super_index = image->value[0];
    if (file.super_index != 0) return {};
    const std::size_t end = std::size_t{file.stored_offset} + file.stored_size;
    if (end > ctx.data.size()) {
        return Support::Unexpected("file " + node.name + " lies outside the data region");
    }
    const auto first = ctx.data.begin() + static_cast<std::ptrdiff_t>(file.stored_offset);
    file.bytes.assign(first, first + static_cast<std::ptrdiff_t>(file.stored_size));
    return {};
}

Support::Expected<Entry, std::string> ConvertNode(const BinaryXml::Node& node,
                                                  const ReadContext& ctx) {
    Entry entry;
    entry.name = node.name;
    entry.type = node.type;
    const bool plain = node.attributes.empty() && !IsSpecialName(node.name);
    if (plain && (node.type == kVoid || (node.type == kS32 && node.value.size() == 4))) {
        entry.kind = EntryKind::Directory;
        entry.time = node.type == kS32 ? static_cast<int32_t>(BigEndian::ReadU32(node.value, 0))
                                       : ctx.header_time;
        auto children = ConvertChildren(node.children, ctx);
        if (!children) return Support::Unexpected(children.error());
        entry.children = std::move(*children);
        return entry;
    }
    if (plain && (node.type == k2S32 || node.type == k3S32)) {
        if (auto filled = FillFile(node, ctx, entry); !filled) {
            return Support::Unexpected(filled.error());
        }
        return entry;
    }
    entry.kind = EntryKind::Special;
    entry.special = node;
    return entry;
}

}

Support::Expected<Archive, std::string> Read(std::span<const uint8_t> bytes) {
    if (bytes.size() < Detail::kHeaderSize) {
        return Support::Unexpected(std::string("file shorter than an IFS header"));
    }
    if (BigEndian::ReadU32(bytes, 0) != Detail::kSignature) {
        return Support::Unexpected(std::string("not an IFS file"));
    }
    Archive archive;
    archive.flags = BigEndian::ReadU16(bytes, 4);
    if ((archive.flags ^ BigEndian::ReadU16(bytes, 6)) != Detail::kFlagComplement) {
        return Support::Unexpected(std::string("IFS flags and their complement disagree"));
    }
    archive.time = BigEndian::ReadU32(bytes, 8);
    archive.tree_size = BigEndian::ReadU32(bytes, 12);
    const std::size_t data_offset = BigEndian::ReadU32(bytes, 16);
    const std::size_t header_end =
        Detail::kHeaderSize + ((archive.flags & kFlagManifestMd5) != 0 ? Detail::kMd5Size : 0);
    if (bytes.size() < header_end || data_offset < header_end) {
        return Support::Unexpected(std::string("IFS header is truncated or inconsistent"));
    }
    if ((archive.flags & kFlagManifestMd5) != 0) {
        const Detail::Digest digest = Detail::ManifestMd5(bytes, header_end, data_offset);
        if (!std::equal(digest.begin(), digest.end(), bytes.begin() + Detail::kHeaderSize)) {
            return Support::Unexpected(std::string("IFS manifest MD5 does not match its header"));
        }
    }
    const std::size_t manifest_end = std::min(data_offset, bytes.size());
    auto manifest = BinaryXml::Read(bytes.subspan(header_end, manifest_end - header_end));
    if (!manifest) return Support::Unexpected("IFS manifest: " + manifest.error());
    if (manifest->root.name != Detail::kRootName) {
        return Support::Unexpected(std::string("IFS manifest root is not imgfs"));
    }
    archive.manifest_signature = manifest->signature;
    archive.manifest_encoding = manifest->encoding;
    archive.root_type = manifest->root.type;
    const std::span<const uint8_t> data =
        data_offset < bytes.size() ? bytes.subspan(data_offset) : std::span<const uint8_t>{};
    archive.stored_data_size = static_cast<uint32_t>(data.size());
    const ReadContext ctx{.data = data, .header_time = static_cast<int32_t>(archive.time)};
    auto entries = ConvertChildren(manifest->root.children, ctx);
    if (!entries) return Support::Unexpected(entries.error());
    archive.entries = std::move(*entries);
    return archive;
}

}
