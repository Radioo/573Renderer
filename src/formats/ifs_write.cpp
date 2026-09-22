#include "formats/ifs_archive.h"

#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_digest.h"
#include "formats/ifs_layout.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace Ifs {

namespace {

constexpr uint32_t kDataAlignment = 16;
constexpr uint64_t kMaxDataBytes = std::numeric_limits<uint32_t>::max() - kDataAlignment;
constexpr const char* kInfoMd5 = "md5";
constexpr const char* kInfoSize = "size";

using BinaryXml::Type::k3S32;

struct Placement {
    const Entry* file = nullptr;
    uint32_t offset = 0;
};

struct PlacedData {
    std::vector<Placement> placements;
    std::vector<uint8_t> bytes;
};

const Placement* FindPlacement(const PlacedData& placed, const Entry* file) {
    const auto found =
        std::ranges::lower_bound(placed.placements, file, std::ranges::less{}, &Placement::file);
    return found != placed.placements.end() && found->file == file ? &*found : nullptr;
}

void CollectLocalFiles(const std::vector<Entry>& entries, std::vector<const Entry*>& out) {
    for (const Entry& entry : entries) {
        if (entry.kind == EntryKind::File && entry.super_index == 0) out.push_back(&entry);
        if (entry.kind == EntryKind::Directory) CollectLocalFiles(entry.children, out);
    }
}

bool StoredLayoutFits(const std::vector<const Entry*>& files) {
    for (const Entry* file : files) {
        if (file->bytes.size() != file->stored_size) return false;
        if (uint64_t{file->stored_offset} + file->stored_size > kMaxDataBytes) return false;
    }
    std::vector<const Entry*> by_offset = files;
    std::ranges::sort(by_offset, {}, &Entry::stored_offset);
    for (std::size_t i = 1; i < by_offset.size(); i++) {
        const uint64_t previous_end =
            uint64_t{by_offset[i - 1]->stored_offset} + by_offset[i - 1]->stored_size;
        if (previous_end > by_offset[i]->stored_offset) return false;
    }
    return true;
}

Support::Expected<Detail::Layout, std::string> LayOut(const Archive& archive,
                                                      const std::vector<const Entry*>& files) {
    if (StoredLayoutFits(files)) {
        Detail::Layout layout;
        layout.data_size = archive.stored_data_size;
        for (const Entry* file : files) {
            layout.offsets.push_back(file->stored_offset);
            layout.data_size = std::max(layout.data_size, file->stored_offset + file->stored_size);
        }
        return layout;
    }
    uint64_t total = 0;
    std::vector<uint32_t> sizes;
    sizes.reserve(files.size());
    for (const Entry* file : files) {
        total += uint64_t{file->bytes.size()} + kDataAlignment;
        if (total > kMaxDataBytes) {
            return Support::Unexpected(std::string("IFS data region would exceed 4 GB"));
        }
        sizes.push_back(static_cast<uint32_t>(file->bytes.size()));
    }
    return Detail::PackLargestFirst(sizes);
}

Support::Expected<PlacedData, std::string> PlaceFiles(const Archive& archive) {
    std::vector<const Entry*> files;
    CollectLocalFiles(archive.entries, files);
    auto layout = LayOut(archive, files);
    if (!layout) return Support::Unexpected(layout.error());
    PlacedData placed;
    placed.bytes.assign(layout->data_size, 0);
    for (std::size_t i = 0; i < files.size(); i++) {
        placed.placements.push_back({.file = files[i], .offset = layout->offsets[i]});
        std::ranges::copy(files[i]->bytes,
                          placed.bytes.begin() + static_cast<std::ptrdiff_t>(layout->offsets[i]));
    }
    std::ranges::sort(placed.placements, std::ranges::less{}, &Placement::file);
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
    const Detail::Digest digest = Detail::Md5(placed.bytes);
    for (BinaryXml::Node& child : node.children) {
        if (child.name == kInfoMd5 && child.type == BinaryXml::Type::kBin) {
            child.value.assign(digest.begin(), digest.end());
        } else if (child.name == kInfoSize && child.type == BinaryXml::Type::kU32) {
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
        if (entry.type == BinaryXml::Type::kS32)
            node.value = BigEndianBytes({static_cast<uint32_t>(entry.time)});
        for (const Entry& child : entry.children)
            node.children.push_back(ManifestNode(child, placed, false));
        return node;
    }
    const Placement* placement = FindPlacement(placed, &entry);
    const uint32_t offset = placement != nullptr ? placement->offset : entry.stored_offset;
    const uint32_t size =
        placement != nullptr ? static_cast<uint32_t>(entry.bytes.size()) : entry.stored_size;
    node.value = entry.type == k3S32
                     ? BigEndianBytes({offset, size, static_cast<uint32_t>(entry.time)})
                     : BigEndianBytes({offset, size});
    node.children = entry.extra_nodes;
    return node;
}

}

Support::Expected<std::vector<uint8_t>, std::string> Write(const Archive& archive) {
    const auto placed = PlaceFiles(archive);
    if (!placed) return Support::Unexpected(placed.error());
    BinaryXml::Document manifest;
    manifest.signature = archive.manifest_signature;
    manifest.encoding = archive.manifest_encoding;
    manifest.root.type = archive.root_type;
    manifest.root.name = Detail::kRootName;
    for (const Entry& entry : archive.entries)
        manifest.root.children.push_back(ManifestNode(entry, *placed, true));
    const auto manifest_bytes = BinaryXml::Write(manifest);
    if (!manifest_bytes) return Support::Unexpected("IFS manifest: " + manifest_bytes.error());

    const bool with_md5 = (archive.flags & kFlagManifestMd5) != 0;
    const auto header_end =
        static_cast<uint32_t>(Detail::kHeaderSize + (with_md5 ? Detail::kMd5Size : 0));
    const uint32_t data_offset =
        Detail::AlignTo(header_end + static_cast<uint32_t>(manifest_bytes->size()), kDataAlignment);

    std::vector<uint8_t> out;
    out.reserve(data_offset + placed->bytes.size());
    BigEndian::AppendU32(out, Detail::kSignature);
    BigEndian::AppendU16(out, archive.flags);
    BigEndian::AppendU16(out, static_cast<uint16_t>(archive.flags ^ Detail::kFlagComplement));
    BigEndian::AppendU32(out, archive.time);
    BigEndian::AppendU32(out, std::max(archive.tree_size, Detail::TreeSize(manifest)));
    BigEndian::AppendU32(out, data_offset);
    out.resize(header_end, 0);
    out.insert(out.end(), manifest_bytes->begin(), manifest_bytes->end());
    out.resize(data_offset, 0);
    if (with_md5) {
        const Detail::Digest digest = Detail::ManifestMd5(out, header_end, data_offset);
        std::ranges::copy(digest, out.begin() + static_cast<std::ptrdiff_t>(Detail::kHeaderSize));
    }
    out.insert(out.end(), placed->bytes.begin(), placed->bytes.end());
    return out;
}

}
