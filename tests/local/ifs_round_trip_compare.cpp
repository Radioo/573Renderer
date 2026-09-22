#include "ifs_round_trip_support.h"

#include "formats/afp_animation.h"
#include "formats/binary_xml.h"
#include "formats/ge2d_shape.h"
#include "formats/ifs_archive.h"
#include "formats/texture_images.h"

#include <cstddef>
#include <format>
#include <string>
#include <variant>
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

const char* TagKind(const AfpAnimation::Tag& tag) {
    if (std::holds_alternative<AfpAnimation::Sprite>(tag.body)) return "sprite";
    if (std::holds_alternative<AfpAnimation::Action>(tag.body)) return "action";
    if (std::holds_alternative<AfpAnimation::Placement>(tag.body)) return "placement";
    if (std::holds_alternative<AfpAnimation::Remove>(tag.body)) return "remove";
    if (std::holds_alternative<AfpAnimation::Image>(tag.body)) return "image";
    if (std::holds_alternative<AfpAnimation::Shape>(tag.body)) return "shape";
    if (std::holds_alternative<AfpAnimation::Camera>(tag.body)) return "camera";
    return "unknown tag";
}

std::string ContainerDifference(const AfpAnimation::Container& a, const AfpAnimation::Container& b,
                                const std::string& path) {
    if (a.labels != b.labels) return path + " labels";
    if (a.script_labels != b.script_labels) return path + " script labels";
    if (a.frames != b.frames) return path + " frames";
    if (a.tags.size() != b.tags.size()) return path + " tag count";
    for (std::size_t i = 0; i < a.tags.size(); i++) {
        if (a.tags[i] == b.tags[i]) continue;
        std::string tag = std::format("{}/tag {} ({})", path, i, TagKind(a.tags[i]));
        const auto* sprite_a = std::get_if<AfpAnimation::Sprite>(&a.tags[i].body);
        const auto* sprite_b = std::get_if<AfpAnimation::Sprite>(&b.tags[i].body);
        if (sprite_a != nullptr && sprite_b != nullptr && sprite_a->id == sprite_b->id)
            return ContainerDifference(sprite_a->container, sprite_b->container, tag);
        return tag;
    }
    return {};
}

std::string TopLevelDifference(const AfpAnimation::Animation& a, const AfpAnimation::Animation& b) {
    if (a.strings != b.strings) return "string table";
    if (a.exports != b.exports) return "exports";
    if (a.imports != b.imports) return "imports";
    if (a.import_initializers != b.import_initializers) return "import initializers";
    if (a.stored_form != b.stored_form) return "stored form";
    return "header";
}

std::string AnimationDifference(const Ifs::Entry& original, const Ifs::Entry& original_script,
                                const Ifs::Entry& back, const Ifs::Entry& back_script) {
    const auto a = AfpAnimation::ReadStored(original.bytes, original_script.bytes);
    const auto b = AfpAnimation::ReadStored(back.bytes, back_script.bytes);
    if (!a || !b) return "animation does not read back";
    if (*a == *b) return {};
    std::string where = ContainerDifference(a->root, b->root, "root");
    if (where.empty()) where = TopLevelDifference(*a, *b);
    return "animation differs at " + where;
}

std::string ContentDifference(const FileRef& file, const Ifs::Entry& back) {
    if (file.kind == ContentKind::WholeFile) {
        return back.bytes == file.original->bytes ? std::string() : "bytes differ";
    }
    if (file.kind == ContentKind::Shape) {
        const auto a = Ge2dShape::Read(file.original->bytes, file.shape_order);
        const auto b = Ge2dShape::Read(back.bytes, file.shape_order);
        if (!a || !b) return "shape does not read back";
        return *a == *b ? std::string() : "shape differs";
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
        if (files[i].kind == ContentKind::ByteOrderScript) continue;
        const std::string difference =
            files[i].kind == ContentKind::Animation
                ? AnimationDifference(*files[i].original, *files[files[i].script].original,
                                      *back_files[i], *back_files[files[i].script])
                : ContentDifference(files[i], *back_files[i]);
        if (!difference.empty()) problems.Add(files[i].path, difference);
    }
}

}
