#include "ifs_round_trip_support.h"

#include "formats/binary_xml.h"
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

void Collect(const std::vector<Ifs::Entry>& originals, std::vector<Ifs::Entry>& copies,
             const std::string& prefix, const Walk& walk) {
    for (std::size_t i = 0; i < originals.size(); i++) {
        const Ifs::Entry& original = originals[i];
        const std::string path = prefix.empty() ? original.name : prefix + "/" + original.name;
        if (original.kind == Ifs::EntryKind::Directory) {
            const std::size_t first_child = walk.files->size();
            Collect(original.children, copies[i].children, path, walk);
            AttachImages(original, path, *walk.files, first_child, *walk.problems);
        } else if (original.kind == Ifs::EntryKind::File && original.super_index == 0) {
            FileRef ref{.path = path,
                        .original = &original,
                        .copy = &copies[i],
                        .kind = ContentKind::WholeFile,
                        .image = {},
                        .compressed = false};
            if (LooksLikeBinaryXml(original.bytes)) ref.kind = ContentKind::BinaryXml;
            walk.files->push_back(ref);
        }
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

void Tally(const FileRef& file, TextureImages::Storage storage, Counters& counters) {
    const bool rewritten = file.copy->bytes != file.original->bytes;
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
                if (file.kind == ContentKind::WholeFile) continue;
                TextureImages::Storage storage = TextureImages::Storage::Plain;
                const std::string problem = file.kind == ContentKind::BinaryXml
                                                ? ReencodeBinaryXml(file)
                                                : ReencodeImage(file, storage);
                const std::scoped_lock guard(lock);
                if (!problem.empty()) problems.Add(file.path, problem);
                Tally(file, storage, counters);
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
