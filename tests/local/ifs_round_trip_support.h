#pragma once

#include "formats/ifs_archive.h"
#include "formats/texture_images.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace RoundTrip {

struct Counters {
    std::size_t identical_archives = 0;
    std::size_t layouts_repacked_identically = 0;
    std::size_t tree_sizes_recomputed_identically = 0;
    std::size_t binary_xml_entries = 0;
    std::size_t binary_xml_rewritten = 0;
    std::size_t images = 0;
    std::size_t images_rewritten = 0;
    std::size_t images_lz77 = 0;
    std::size_t images_raw = 0;
    std::size_t images_plain = 0;
};

enum class ContentKind : uint8_t { WholeFile, BinaryXml, TextureImage };

struct FileRef {
    std::string path;
    const Ifs::Entry* original = nullptr;
    Ifs::Entry* copy = nullptr;
    ContentKind kind = ContentKind::WholeFile;
    TextureImages::Image image;
    bool compressed = false;
};

struct Problems {
    std::vector<std::string> list;
    void Add(const std::string& where, const std::string& what) {
        list.push_back(where + ": " + what);
    }
};

[[nodiscard]] bool LooksLikeBinaryXml(std::span<const uint8_t> bytes);

[[nodiscard]] std::vector<FileRef> CollectFiles(const Ifs::Archive& original, Ifs::Archive& copy,
                                                Problems& problems);

void ReencodeFiles(std::vector<FileRef>& files, Counters& counters, Problems& problems);

void CompareArchives(const Ifs::Archive& original, const Ifs::Archive& back,
                     const std::vector<FileRef>& files, Problems& problems);

}
