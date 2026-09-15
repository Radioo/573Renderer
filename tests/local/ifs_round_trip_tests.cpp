#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "formats/texture_images.h"
#include "support/env.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint8_t kBinaryXmlMagic = 0xA0;
constexpr std::size_t kProgressEvery = 250;
constexpr std::size_t kBgraBytes = 4;
constexpr const char* kTextureDirectory = "tex";
constexpr const char* kTextureListNode = "texturelist_Exml";

std::vector<uint8_t> ReadWholeFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> FindArchives(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> out;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
        if (item.is_regular_file() && item.path().extension() == ".ifs") out.push_back(item.path());
    }
    std::ranges::sort(out);
    return out;
}

bool SameNode(const BinaryXml::Node& a, const BinaryXml::Node& b) {
    if (a.type != b.type || a.name != b.name || a.value != b.value) return false;
    if (a.attributes.size() != b.attributes.size() || a.children.size() != b.children.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.attributes.size(); i++) {
        if (!SameNode(a.attributes[i], b.attributes[i])) return false;
    }
    for (std::size_t i = 0; i < a.children.size(); i++) {
        if (!SameNode(a.children[i], b.children[i])) return false;
    }
    return true;
}

bool SameEntries(const std::vector<Ifs::Entry>& a, const std::vector<Ifs::Entry>& b,
                 std::string& where) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++) {
        const Ifs::Entry& x = a[i];
        const Ifs::Entry& y = b[i];
        where = x.name;
        if (x.kind != y.kind || x.name != y.name || x.type != y.type || x.time != y.time ||
            x.image != y.image || x.bytes != y.bytes || x.extra_nodes.size() != y.extra_nodes.size()) {
            return false;
        }
        if (x.kind == Ifs::EntryKind::Special && !SameNode(x.special, y.special)) return false;
        if (x.image != 0 && (x.stored_offset != y.stored_offset || x.stored_size != y.stored_size)) {
            return false;
        }
        if (!SameEntries(x.children, y.children, where)) return false;
    }
    return true;
}

bool LooksLikeBinaryXml(const std::vector<uint8_t>& bytes) {
    return bytes.size() >= 4 && bytes[0] == kBinaryXmlMagic &&
           (bytes[1] == BinaryXml::kSixBitNames || bytes[1] == BinaryXml::kByteNames) &&
           (bytes[2] ^ bytes[3]) == 0xFF;
}

void CheckBinaryXmlEntries(const std::vector<Ifs::Entry>& entries, const std::string& archive,
                           std::size_t& documents) {
    for (const Ifs::Entry& entry : entries) {
        CheckBinaryXmlEntries(entry.children, archive, documents);
        if (entry.kind != Ifs::EntryKind::File || !LooksLikeBinaryXml(entry.bytes)) continue;
        INFO(archive << " entry " << entry.name);
        const auto doc = BinaryXml::Read(entry.bytes);
        REQUIRE(doc.has_value());
        const auto again = BinaryXml::Write(*doc);
        REQUIRE(again.has_value());
        CHECK(*again == entry.bytes);
        documents++;
    }
}

struct ImageJob {
    TextureImages::Image image;
    bool compressed = false;
    const std::vector<uint8_t>* bytes = nullptr;
};

struct TextureTally {
    std::size_t images = 0;
    std::size_t lz77 = 0;
    std::size_t raw = 0;
    std::size_t plain = 0;
};

const Ifs::Entry* FindChild(const Ifs::Entry& directory, const std::string& name) {
    for (const Ifs::Entry& child : directory.children) {
        if (child.name == name) return &child;
    }
    return nullptr;
}

void CollectTextureDirectories(const std::vector<Ifs::Entry>& entries, std::vector<const Ifs::Entry*>& out) {
    for (const Ifs::Entry& entry : entries) {
        if (entry.kind != Ifs::EntryKind::Directory) continue;
        if (entry.name == kTextureDirectory && FindChild(entry, kTextureListNode) != nullptr) out.push_back(&entry);
        CollectTextureDirectories(entry.children, out);
    }
}

std::string CheckImage(const ImageJob& job, TextureImages::Storage& storage) {
    const auto blob = TextureImages::DecodeBlob(*job.bytes, job.compressed);
    if (!blob) return job.image.name + ": " + blob.error();
    storage = blob->storage;
    if (blob->pixels.size() != std::size_t{job.image.width} * job.image.height * kBgraBytes) {
        return job.image.name + ": pixel count disagrees with imgrect";
    }
    const auto bgra = TextureImages::PixelsToBgra(job.image.format, blob->pixels);
    if (!bgra) return job.image.name + ": " + bgra.error();
    const auto pixels = TextureImages::BgraToPixels(job.image.format, *bgra);
    if (!pixels || *pixels != blob->pixels) return job.image.name + ": BGRA conversion does not round trip";
    if (TextureImages::EncodeBlob(*blob) != *job.bytes) return job.image.name + ": blob does not re-encode byte for byte";
    return {};
}

std::vector<std::string> CheckTextures(const Ifs::Archive& archive, TextureTally& tally) {
    std::vector<std::string> problems;
    std::vector<const Ifs::Entry*> directories;
    CollectTextureDirectories(archive.entries, directories);
    std::vector<ImageJob> jobs;
    for (const Ifs::Entry* directory : directories) {
        const auto doc = BinaryXml::Read(FindChild(*directory, kTextureListNode)->bytes);
        if (!doc) {
            problems.push_back("texturelist: " + doc.error());
            continue;
        }
        const auto list = TextureImages::ReadList(*doc);
        if (!list) {
            problems.push_back("texturelist: " + list.error());
            continue;
        }
        for (const TextureImages::Image& image : list->images) {
            const Ifs::Entry* entry = FindChild(*directory, Ifs::HashedName(image.name));
            if (entry == nullptr) {
                problems.push_back(image.name + ": no tex entry under its hashed name");
                continue;
            }
            jobs.push_back({.image = image, .compressed = list->compressed, .bytes = &entry->bytes});
        }
    }
    std::mutex lock;
    std::for_each(std::execution::par, jobs.begin(), jobs.end(), [&](const ImageJob& job) {
        TextureImages::Storage storage = TextureImages::Storage::Plain;
        std::string problem = CheckImage(job, storage);
        const std::scoped_lock guard(lock);
        tally.images++;
        tally.lz77 += storage == TextureImages::Storage::Lz77 ? 1U : 0U;
        tally.raw += storage == TextureImages::Storage::RawAfterHeader ? 1U : 0U;
        tally.plain += storage == TextureImages::Storage::Plain ? 1U : 0U;
        if (!problem.empty()) problems.push_back(std::move(problem));
    });
    return problems;
}

}

TEST_CASE("Every IFS in the install survives a round trip through our writers") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    const std::vector<std::filesystem::path> archives = FindArchives(std::filesystem::path(dir) / "data");
    REQUIRE(!archives.empty());

    std::size_t not_ifs = 0;
    std::size_t identical = 0;
    std::size_t documents = 0;
    TextureTally textures;
    std::vector<std::string> differing;
    for (std::size_t n = 0; n < archives.size(); n++) {
        const std::filesystem::path& path = archives[n];
        const std::string label = std::filesystem::relative(path, dir).generic_string();
        if (n % kProgressEvery == 0) {
            std::fprintf(stderr, "[ifs round trip] %zu / %zu %s\n", n, archives.size(), label.c_str());
        }
        const std::vector<uint8_t> original = ReadWholeFile(path);
        const auto archive = Ifs::Read(original);
        if (!archive && archive.error() == "not an IFS file") {
            not_ifs++;
            continue;
        }
        INFO(label);
        REQUIRE(archive.has_value());
        const auto written = Ifs::Write(*archive);
        REQUIRE(written.has_value());
        const auto back = Ifs::Read(*written);
        REQUIRE(back.has_value());
        CHECK(back->flags == archive->flags);
        CHECK(back->time == archive->time);
        std::string where;
        if (!SameEntries(archive->entries, back->entries, where)) {
            FAIL_CHECK("entries differ after a round trip, first at " << where);
        }
        CheckBinaryXmlEntries(archive->entries, label, documents);
        const std::vector<std::string> texture_problems = CheckTextures(*archive, textures);
        for (const std::string& problem : texture_problems) {
            FAIL_CHECK(problem);
        }
        if (*written == original) {
            identical++;
        } else {
            differing.push_back(label);
        }
    }
    std::fprintf(stderr, "[ifs round trip] %zu archives, %zu not IFS, %zu byte-identical, %zu differ, %zu binary xml entries\n",
                 archives.size(), not_ifs, identical, differing.size(), documents);
    for (std::size_t i = 0; i < std::min<std::size_t>(differing.size(), 20); i++) {
        std::fprintf(stderr, "[ifs round trip] differs: %s\n", differing[i].c_str());
    }
}
