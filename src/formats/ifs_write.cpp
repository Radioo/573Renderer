#include "formats/ifs_archive.h"

#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_layout.h"
#include "support/expected.h"

#include <md5.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ifs {

namespace {

constexpr uint32_t kDataAlignment = 16;
constexpr const char* kInfoMd5 = "md5";
constexpr const char* kInfoSize = "size";

using Offsets = std::unordered_map<const Entry*, uint32_t>;

struct PlacedData {
    Offsets offsets;
    std::vector<uint8_t> bytes;
};

std::array<uint8_t, Detail::kMd5Size> Md5(std::span<const uint8_t> bytes) {
    MD5 md5;
    md5.add(bytes.data(), bytes.size());
    std::array<uint8_t, Detail::kMd5Size> digest{};
    md5.getHash(digest.data());
    return digest;
}

void CollectLocalFiles(const std::vector<Entry>& entries, std::vector<const Entry*>& out) {
    for (const Entry& entry : entries) {
        if (entry.kind == EntryKind::File && entry.image == 0) out.push_back(&entry);
        if (entry.kind == EntryKind::Directory) CollectLocalFiles(entry.children, out);
    }
}

bool StoredLayoutFits(const std::vector<const Entry*>& files) {
    std::vector<const Entry*> by_offset = files;
    for (const Entry* file : files) {
        if (file->bytes.size() != file->stored_size) return false;
    }
    std::ranges::sort(by_offset, {}, &Entry::stored_offset);
    for (std::size_t i = 1; i < by_offset.size(); i++) {
        const uint64_t previous_end =
            uint64_t{by_offset[i - 1]->stored_offset} + by_offset[i - 1]->stored_size;
        if (previous_end > by_offset[i]->stored_offset) return false;
    }
    return true;
}

PlacedData PlaceFiles(const Archive& archive) {
    std::vector<const Entry*> files;
    CollectLocalFiles(archive.entries, files);
    PlacedData placed;
    uint32_t data_size = 0;
    if (StoredLayoutFits(files)) {
        data_size = archive.stored_data_size;
        for (const Entry* file : files) {
            placed.offsets[file] = file->stored_offset;
            data_size = std::max(data_size, file->stored_offset + file->stored_size);
        }
    } else {
        std::vector<uint32_t> sizes;
        sizes.reserve(files.size());
        for (const Entry* file : files)
            sizes.push_back(static_cast<uint32_t>(file->bytes.size()));
        const Detail::Layout layout = Detail::PackLargestFirst(sizes);
        for (std::size_t i = 0; i < files.size(); i++)
            placed.offsets[files[i]] = layout.offsets[i];
        data_size = layout.data_size;
    }
    placed.bytes.assign(data_size, 0);
    for (const Entry* file : files) {
        std::ranges::copy(file->bytes,
                          placed.bytes.begin() + static_cast<std::ptrdiff_t>(placed.offsets[file]));
    }
    return placed;
}

std::vector<uint8_t> BigEndianBytes(std::initializer_list<uint32_t> values) {
    std::vector<uint8_t> out;
    for (const uint32_t v : values)
        BigEndian::AppendU32(out, v);
    return out;
}

BinaryXml::Node InfoNode(const Entry& entry, const PlacedData& placed) {
    BinaryXml::Node node = entry.special;
    const std::array<uint8_t, Detail::kMd5Size> digest = Md5(placed.bytes);
    for (BinaryXml::Node& child : node.children) {
        if (child.name == kInfoMd5 && child.type == Detail::kBinType) {
            child.value.assign(digest.begin(), digest.end());
        } else if (child.name == kInfoSize && child.type == Detail::kU32Type) {
            child.value = BigEndianBytes({static_cast<uint32_t>(placed.bytes.size())});
        }
    }
    return node;
}

BinaryXml::Node ManifestNode(const Entry& entry, const PlacedData& placed, bool at_root) {
    if (entry.kind == EntryKind::Special) {
        return at_root && entry.name == Detail::kInfoName ? InfoNode(entry, placed) : entry.special;
    }
    BinaryXml::Node node;
    node.type = entry.type;
    node.name = entry.name;
    if (entry.kind == EntryKind::Directory) {
        if (entry.type == Detail::kS32Type) node.value = BigEndianBytes({static_cast<uint32_t>(entry.time)});
        for (const Entry& child : entry.children)
            node.children.push_back(ManifestNode(child, placed, false));
        return node;
    }
    const auto found = placed.offsets.find(&entry);
    const uint32_t offset = found != placed.offsets.end() ? found->second : entry.stored_offset;
    const uint32_t size =
        found != placed.offsets.end() ? static_cast<uint32_t>(entry.bytes.size()) : entry.stored_size;
    node.value = entry.type == Detail::kThreeS32Type
                     ? BigEndianBytes({offset, size, static_cast<uint32_t>(entry.time)})
                     : BigEndianBytes({offset, size});
    node.children = entry.extra_nodes;
    return node;
}

}

Support::Expected<std::vector<uint8_t>, std::string> Write(const Archive& archive) {
    const PlacedData placed = PlaceFiles(archive);
    BinaryXml::Document manifest;
    manifest.signature = archive.manifest_signature;
    manifest.encoding = archive.manifest_encoding;
    manifest.root.type = archive.root_type;
    manifest.root.name = Detail::kRootName;
    for (const Entry& entry : archive.entries)
        manifest.root.children.push_back(ManifestNode(entry, placed, true));
    const auto kbin = BinaryXml::Write(manifest);
    if (!kbin) return Support::Unexpected("IFS manifest: " + kbin.error());

    const bool with_md5 = (archive.flags & kFlagManifestMd5) != 0;
    const auto header_end =
        static_cast<uint32_t>(Detail::kHeaderSize + (with_md5 ? Detail::kMd5Size : 0));
    const uint32_t data_offset =
        Detail::AlignTo(header_end + static_cast<uint32_t>(kbin->size()), kDataAlignment);

    std::vector<uint8_t> out;
    out.reserve(data_offset + placed.bytes.size());
    BigEndian::AppendU32(out, Detail::kSignature);
    BigEndian::AppendU16(out, archive.flags);
    BigEndian::AppendU16(out, static_cast<uint16_t>(archive.flags ^ Detail::kFlagComplement));
    BigEndian::AppendU32(out, archive.time);
    BigEndian::AppendU32(out, std::max(archive.tree_size, Detail::TreeSize(manifest)));
    BigEndian::AppendU32(out, data_offset);
    out.resize(header_end, 0);
    out.insert(out.end(), kbin->begin(), kbin->end());
    out.resize(data_offset, 0);
    if (with_md5) {
        const auto region = std::span(out).subspan(header_end, data_offset - header_end);
        const std::array<uint8_t, Detail::kMd5Size> digest = Md5(region);
        std::ranges::copy(digest, out.begin() + static_cast<std::ptrdiff_t>(Detail::kHeaderSize));
    }
    out.insert(out.end(), placed.bytes.begin(), placed.bytes.end());
    return out;
}

}
