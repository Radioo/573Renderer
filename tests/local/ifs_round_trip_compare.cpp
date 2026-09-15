#include "ifs_round_trip_support.h"

#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/texture_images.h"

#include <cstddef>
#include <string>
#include <vector>

namespace RoundTrip {

namespace {

constexpr const char* kInfoName = "_info_";

bool SameNode(const BinaryXml::Node& a, const BinaryXml::Node& b, bool compare_values) {
    if (a.type != b.type || a.name != b.name) return false;
    if (compare_values && a.value != b.value) return false;
    if (a.attributes.size() != b.attributes.size() || a.children.size() != b.children.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.attributes.size(); i++) {
        if (!SameNode(a.attributes[i], b.attributes[i], compare_values)) return false;
    }
    for (std::size_t i = 0; i < a.children.size(); i++) {
        if (!SameNode(a.children[i], b.children[i], compare_values)) return false;
    }
    return true;
}

bool SameExtraNodes(const Ifs::Entry& a, const Ifs::Entry& b) {
    if (a.extra_nodes.size() != b.extra_nodes.size()) return false;
    for (std::size_t i = 0; i < a.extra_nodes.size(); i++) {
        if (!SameNode(a.extra_nodes[i], b.extra_nodes[i], true)) return false;
    }
    return true;
}

std::string EntryDifference(const Ifs::Entry& a, const Ifs::Entry& b, bool at_root) {
    if (a.kind != b.kind || a.name != b.name || a.type != b.type) return "kind, name or type";
    if (a.time != b.time || a.super_index != b.super_index) return "time or super image";
    if (!SameExtraNodes(a, b)) return "extra nodes";
    if (a.kind == Ifs::EntryKind::Special) {
        const bool derived = at_root && a.name == kInfoName;
        if (!SameNode(a.special, b.special, !derived)) return "special node";
    }
    if (a.super_index != 0 &&
        (a.stored_offset != b.stored_offset || a.stored_size != b.stored_size)) {
        return "super image reference";
    }
    if (a.children.size() != b.children.size()) return "child count";
    return {};
}

void CompareTrees(const std::vector<Ifs::Entry>& a, const std::vector<Ifs::Entry>& b,
                  const std::string& prefix, Problems& problems) {
    if (a.size() != b.size()) {
        problems.Add(prefix.empty() ? "root" : prefix, "entry count differs");
        return;
    }
    for (std::size_t i = 0; i < a.size(); i++) {
        const std::string path = prefix.empty() ? a[i].name : prefix + "/" + a[i].name;
        const std::string difference = EntryDifference(a[i], b[i], prefix.empty());
        if (!difference.empty()) {
            problems.Add(path, difference + " differs");
            continue;
        }
        CompareTrees(a[i].children, b[i].children, path, problems);
    }
}

void CollectBackFiles(const std::vector<Ifs::Entry>& entries, std::vector<const Ifs::Entry*>& out) {
    for (const Ifs::Entry& entry : entries) {
        if (entry.kind == Ifs::EntryKind::Directory) CollectBackFiles(entry.children, out);
        if (entry.kind == Ifs::EntryKind::File && entry.super_index == 0) out.push_back(&entry);
    }
}

std::string ContentDifference(const FileRef& file, const Ifs::Entry& back) {
    if (file.kind == ContentKind::WholeFile) {
        return back.bytes == file.original->bytes ? std::string() : "bytes differ";
    }
    if (file.kind == ContentKind::BinaryXml) {
        const auto a = BinaryXml::Read(file.original->bytes);
        const auto b = BinaryXml::Read(back.bytes);
        if (!a || !b) return "binary xml does not read back";
        const bool same = a->signature == b->signature && a->encoding == b->encoding &&
                          SameNode(a->root, b->root, true);
        return same ? std::string() : "binary xml content differs";
    }
    const auto a = TextureImages::DecodeBlob(file.original->bytes, file.compressed);
    const auto b = TextureImages::DecodeBlob(back.bytes, file.compressed);
    if (!a || !b) return "image does not decode back";
    const bool same = a->storage == b->storage && a->pixels == b->pixels;
    return same ? std::string() : "image pixels differ";
}

}

void CompareArchives(const Ifs::Archive& original, const Ifs::Archive& back,
                     const std::vector<FileRef>& files, Problems& problems) {
    if (original.flags != back.flags || original.time != back.time) {
        problems.Add("header", "flags or time differ");
    }
    if (original.manifest_signature != back.manifest_signature ||
        original.manifest_encoding != back.manifest_encoding ||
        original.root_type != back.root_type) {
        problems.Add("manifest", "signature, encoding or root type differ");
    }
    CompareTrees(original.entries, back.entries, "", problems);
    std::vector<const Ifs::Entry*> back_files;
    CollectBackFiles(back.entries, back_files);
    if (back_files.size() != files.size()) {
        problems.Add("archive", "local file count differs");
        return;
    }
    for (std::size_t i = 0; i < files.size(); i++) {
        const std::string difference = ContentDifference(files[i], *back_files[i]);
        if (!difference.empty()) problems.Add(files[i].path, difference);
    }
}

}
