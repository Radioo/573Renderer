#include "ifs_round_trip_support.h"

#include "formats/afp_animation.h"
#include "formats/binary_xml.h"
#include "formats/ge2d_shape.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "formats/texture_images.h"
#include "support/expected.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace RoundTrip {

namespace {

constexpr uint8_t kBinaryXmlMagic = 0xA0;
constexpr std::size_t kBgraBytes = 4;
constexpr const char* kTextureDirectory = "tex";
constexpr const char* kTextureListNode = "texturelist_Exml";
constexpr const char* kAnimationDirectory = "afp";
constexpr const char* kScriptDirectory = "bsi";
constexpr const char* kShapeDirectory = "geo";
constexpr const char* kPackageMagic = "magic";

struct Walk {
    std::vector<FileRef>* files = nullptr;
    Problems* problems = nullptr;
};

void AttachImages(const Ifs::Entry& directory, const std::string& path, std::vector<FileRef>& files,
                  std::size_t first_child, Problems& problems) {
    const auto list_entry =
        std::ranges::find(directory.children, std::string(kTextureListNode), &Ifs::Entry::name);
    if (directory.name != kTextureDirectory || list_entry == directory.children.end()) return;
    const auto doc = BinaryXml::Read(list_entry->bytes);
    const auto list =
        doc ? TextureImages::ReadList(*doc)
            : Support::Expected<TextureImages::List, std::string>(Support::Unexpected(doc.error()));
    if (!list) {
        problems.Add(path, "texturelist: " + list.error());
        return;
    }
    for (const TextureImages::Image& image : list->images) {
        const std::string hashed = Ifs::HashedName(image.name);
        const auto found =
            std::find_if(files.begin() + static_cast<std::ptrdiff_t>(first_child), files.end(),
                         [&](const FileRef& f) { return f.original->name == hashed; });
        if (found == files.end()) {
            problems.Add(path, "no tex entry for image " + image.name);
            continue;
        }
        found->kind = ContentKind::TextureImage;
        found->image = image;
        found->compressed = list->compressed;
    }
}

void AttachAnimations(const Ifs::Entry& directory, std::vector<FileRef>& files,
                      std::size_t first_child) {
    if (directory.name != kAnimationDirectory) return;
    const auto scripts =
        std::ranges::find(directory.children, std::string(kScriptDirectory), &Ifs::Entry::name);
    if (scripts == directory.children.end()) return;
    for (std::size_t i = first_child; i < files.size(); i++) {
        const FileRef& candidate = files[i];
        const bool is_script = std::ranges::any_of(
            scripts->children, [&](const Ifs::Entry& e) { return &e == candidate.original; });
        if (!is_script) continue;
        const auto animation = std::find_if(
            files.begin() + static_cast<std::ptrdiff_t>(first_child), files.end(),
            [&](const FileRef& f) {
                return f.original->name == candidate.original->name &&
                       f.kind == ContentKind::WholeFile &&
                       std::ranges::any_of(directory.children,
                                           [&](const Ifs::Entry& e) { return &e == f.original; });
            });
        if (animation == files.end()) continue;
        files[i].kind = ContentKind::ByteOrderScript;
        animation->kind = ContentKind::Animation;
        animation->script = i;
    }
}

void Collect(const std::vector<Ifs::Entry>& originals, std::vector<Ifs::Entry>& copies,
             const std::string& prefix, const Walk& walk) {
    for (std::size_t i = 0; i < originals.size(); i++) {
        const Ifs::Entry& original = originals[i];
        const std::string path = prefix.empty() ? original.name : prefix + "/" + original.name;
        if (original.kind == Ifs::EntryKind::Directory) {
            const std::size_t first_child = walk.files->size();
            Collect(original.children, copies[i].children, path, walk);
            AttachImages(original, path, *walk.files, first_child, *walk.problems);
            AttachAnimations(original, *walk.files, first_child);
        } else if (original.kind == Ifs::EntryKind::File && original.super_index == 0) {
            FileRef ref{.path = path,
                        .original = &original,
                        .copy = &copies[i],
                        .kind = ContentKind::WholeFile,
                        .image = {},
                        .compressed = false,
                        .script = 0,
                        .shape_order = Ge2dShape::ByteOrder::Big};
            if (LooksLikeBinaryXml(original.bytes)) ref.kind = ContentKind::BinaryXml;
            walk.files->push_back(ref);
        }
    }
}

void AttachShapes(const std::vector<Ifs::Entry>& root, std::vector<FileRef>& files,
                  Problems& problems) {
    const auto magic = std::ranges::find(root, std::string(kPackageMagic), &Ifs::Entry::name);
    const auto shapes = std::ranges::find(root, std::string(kShapeDirectory), &Ifs::Entry::name);
    if (shapes == root.end() || shapes->kind != Ifs::EntryKind::Directory) return;
    if (magic == root.end()) {
        problems.Add(kShapeDirectory, "package has shapes but no magic file");
        return;
    }
    const auto order = Ge2dShape::PackageByteOrder(magic->bytes);
    if (!order) {
        problems.Add(kPackageMagic, order.error());
        return;
    }
    for (FileRef& file : files) {
        const bool is_shape = std::ranges::any_of(
            shapes->children, [&](const Ifs::Entry& e) { return &e == file.original; });
        if (!is_shape) continue;
        file.kind = ContentKind::Shape;
        file.shape_order = *order;
    }
}

std::string ReencodeBinaryXml(FileRef& file) {
    const auto doc = BinaryXml::Read(file.original->bytes);
    if (!doc) return doc.error();
    auto bytes = BinaryXml::Write(*doc);
    if (!bytes) return bytes.error();
    file.copy->bytes = std::move(*bytes);
    return {};
}

std::string ReencodeImage(FileRef& file, TextureImages::Storage& storage) {
    const auto blob = TextureImages::DecodeBlob(file.original->bytes, file.compressed);
    if (!blob) return blob.error();
    storage = blob->storage;
    if (blob->pixels.size() != std::size_t{file.image.width} * file.image.height * kBgraBytes) {
        return "pixel count disagrees with imgrect";
    }
    const auto bgra = TextureImages::PixelsToBgra(file.image.format, blob->pixels);
    if (!bgra) return bgra.error();
    TextureImages::Blob rebuilt{.storage = blob->storage, .pixels = {}};
    auto pixels = TextureImages::BgraToPixels(file.image.format, *bgra);
    if (!pixels) return pixels.error();
    rebuilt.pixels = std::move(*pixels);
    file.copy->bytes = TextureImages::EncodeBlob(rebuilt);
    return {};
}

struct Outcome {
    TextureImages::Storage storage = TextureImages::Storage::Plain;
    bool unswapped_colour = false;
    bool failed = false;
};

std::string ReencodeAnimation(FileRef& file, FileRef& script, Outcome& outcome) {
    const auto animation = AfpAnimation::ReadStored(file.original->bytes, script.original->bytes);
    if (!animation) return "animation does not read: " + animation.error();
    outcome.unswapped_colour = !animation->stored_form.background_colour_swapped;
    auto stored = AfpAnimation::WriteStored(*animation);
    if (!stored) return "animation does not write: " + stored.error();
    file.copy->bytes = std::move(stored->data);
    script.copy->bytes = std::move(stored->script);
    return {};
}

std::string ReencodeShape(FileRef& file) {
    const auto shape = Ge2dShape::Read(file.original->bytes, file.shape_order);
    if (!shape) return "shape does not read: " + shape.error();
    auto bytes = Ge2dShape::Write(*shape, file.shape_order);
    if (!bytes) return "shape does not write: " + bytes.error();
    file.copy->bytes = std::move(*bytes);
    return {};
}

void Tally(const FileRef& file, const std::vector<FileRef>& files, const Outcome& outcome,
           Counters& counters) {
    const TextureImages::Storage storage = outcome.storage;
    const bool rewritten = outcome.failed || file.copy->bytes != file.original->bytes;
    if (file.kind == ContentKind::Animation) {
        const FileRef& script = files[file.script];
        const bool script_rewritten =
            outcome.failed || script.copy->bytes != script.original->bytes;
        counters.animations++;
        counters.animations_rewritten += rewritten ? 1U : 0U;
        counters.scripts_rewritten += script_rewritten ? 1U : 0U;
        if (rewritten || script_rewritten) counters.entries_not_identical.push_back(file.path);
        counters.animations_with_unswapped_colour += outcome.unswapped_colour ? 1U : 0U;
        return;
    }
    if (file.kind == ContentKind::Shape) {
        counters.shapes++;
        counters.shapes_rewritten += rewritten ? 1U : 0U;
        if (rewritten) counters.entries_not_identical.push_back(file.path);
        return;
    }
    if (file.kind == ContentKind::BinaryXml) {
        counters.binary_xml_entries++;
        counters.binary_xml_rewritten += rewritten ? 1U : 0U;
        return;
    }
    counters.images++;
    counters.images_rewritten += rewritten ? 1U : 0U;
    counters.images_lz77 += storage == TextureImages::Storage::Lz77 ? 1U : 0U;
    counters.images_raw += storage == TextureImages::Storage::RawAfterHeader ? 1U : 0U;
    counters.images_plain += storage == TextureImages::Storage::Plain ? 1U : 0U;
}

}

bool LooksLikeBinaryXml(std::span<const uint8_t> bytes) {
    return bytes.size() >= 4 && bytes[0] == kBinaryXmlMagic &&
           (bytes[1] == BinaryXml::kSixBitNames || bytes[1] == BinaryXml::kByteNames) &&
           (bytes[2] ^ bytes[3]) == 0xFF;
}

std::vector<FileRef> CollectFiles(const Ifs::Archive& original, Ifs::Archive& copy,
                                  Problems& problems) {
    std::vector<FileRef> files;
    Collect(original.entries, copy.entries, "", Walk{.files = &files, .problems = &problems});
    AttachShapes(original.entries, files, problems);
    return files;
}

void ReencodeFiles(std::vector<FileRef>& files, Counters& counters, Problems& problems) {
    std::mutex lock;
    std::atomic<std::size_t> next{0};
    std::atomic<bool> worker_failed{false};
    const auto work = [&]() noexcept {
        try {
            for (std::size_t i = next++; i < files.size(); i = next++) {
                FileRef& file = files[i];
                if (file.kind == ContentKind::WholeFile ||
                    file.kind == ContentKind::ByteOrderScript)
                    continue;
                Outcome outcome;
                std::string problem;
                if (file.kind == ContentKind::BinaryXml) {
                    problem = ReencodeBinaryXml(file);
                } else if (file.kind == ContentKind::Animation) {
                    problem = ReencodeAnimation(file, files[file.script], outcome);
                } else if (file.kind == ContentKind::Shape) {
                    problem = ReencodeShape(file);
                } else {
                    problem = ReencodeImage(file, outcome.storage);
                }
                const std::scoped_lock guard(lock);
                outcome.failed = !problem.empty();
                if (outcome.failed) problems.Add(file.path, problem);
                Tally(file, files, outcome, counters);
            }
        } catch (...) {
            worker_failed = true;
        }
    };
    {
        const unsigned count = std::max(1U, std::thread::hardware_concurrency());
        std::vector<std::jthread> workers;
        workers.reserve(count);
        for (unsigned i = 0; i < count; i++)
            workers.emplace_back(work);
    }
    if (worker_failed) problems.Add("archive", "a re-encoding worker threw an exception");
}

}
