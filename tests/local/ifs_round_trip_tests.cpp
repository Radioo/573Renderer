#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/ifs_archive.h"
#include "ifs_round_trip_support.h"
#include "support/env.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kListedDifferences = 20;

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

void CollectLocalFiles(std::vector<Ifs::Entry>& entries, std::vector<Ifs::Entry*>& out) {
    for (Ifs::Entry& entry : entries) {
        if (entry.kind == Ifs::EntryKind::Directory) CollectLocalFiles(entry.children, out);
        if (entry.kind == Ifs::EntryKind::File && entry.super_index == 0) out.push_back(&entry);
    }
}

void MeasureRecomputedLayout(const Ifs::Archive& original, RoundTrip::Counters& counters) {
    Ifs::Archive forced = original;
    forced.tree_size = 0;
    std::vector<Ifs::Entry*> forced_files;
    CollectLocalFiles(forced.entries, forced_files);
    for (Ifs::Entry* file : forced_files)
        file->stored_offset = 0;
    const auto written = Ifs::Write(forced);
    if (!written) return;
    auto back = Ifs::Read(*written);
    if (!back) return;
    counters.tree_sizes_recomputed_identically += back->tree_size == original.tree_size ? 1U : 0U;
    Ifs::Archive original_copy = original;
    std::vector<Ifs::Entry*> expected;
    CollectLocalFiles(original_copy.entries, expected);
    std::vector<Ifs::Entry*> actual;
    CollectLocalFiles(back->entries, actual);
    const bool same =
        std::ranges::equal(expected, actual, [](const Ifs::Entry* a, const Ifs::Entry* b) {
            return a->stored_offset == b->stored_offset;
        });
    counters.layouts_repacked_identically += same ? 1U : 0U;
}

bool CheckArchive(const std::vector<uint8_t>& original, const Ifs::Archive& archive,
                  RoundTrip::Counters& counters, RoundTrip::Problems& problems) {
    Ifs::Archive copy = archive;
    std::vector<RoundTrip::FileRef> files = RoundTrip::CollectFiles(archive, copy, problems);
    RoundTrip::ReencodeFiles(files, counters, problems);
    const auto written = Ifs::Write(copy);
    if (!written) {
        problems.Add("archive", "write failed: " + written.error());
        return false;
    }
    const auto back = Ifs::Read(*written);
    if (!back) {
        problems.Add("archive", "written archive does not read back: " + back.error());
        return false;
    }
    RoundTrip::CompareArchives(archive, *back, files, problems);
    MeasureRecomputedLayout(archive, counters);
    return *written == original;
}

void PrintSummary(std::size_t archives, std::size_t not_ifs, const RoundTrip::Counters& c,
                  const std::vector<std::string>& differing) {
    std::cerr << std::format("[ifs round trip] {} files, {} not IFS, {} archives byte-identical\n",
                             archives, not_ifs, c.identical_archives);
    std::cerr << std::format(
        "[ifs round trip] {} binary xml entries re-encoded, {} not byte-identical\n",
        c.binary_xml_entries, c.binary_xml_rewritten);
    std::cerr << std::format("[ifs round trip] {} images re-encoded ({} lz77, {} raw after header, "
                             "{} plain), {} not byte-identical\n",
                             c.images, c.images_lz77, c.images_raw, c.images_plain,
                             c.images_rewritten);
    std::cerr << std::format("[ifs round trip] layout recomputed identically in {} archives, tree "
                             "size in {}\n",
                             c.layouts_repacked_identically, c.tree_sizes_recomputed_identically);
    for (std::size_t i = 0; i < std::min(differing.size(), kListedDifferences); i++)
        std::cerr << std::format("[ifs round trip] not byte-identical: {}\n", differing[i]);
}

}

TEST_CASE("Every IFS in the install survives a round trip through our writers") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    const std::vector<std::filesystem::path> archives =
        FindArchives(std::filesystem::path(dir) / "data");
    REQUIRE(!archives.empty());

    std::size_t not_ifs = 0;
    RoundTrip::Counters counters;
    std::vector<std::string> differing;
    for (std::size_t n = 0; n < archives.size(); n++) {
        const std::string label = std::filesystem::relative(archives[n], dir).generic_string();
        std::cerr << std::format("[ifs round trip] {} / {} {}\n", n + 1, archives.size(), label);
        const std::vector<uint8_t> original = ReadWholeFile(archives[n]);
        const auto archive = Ifs::Read(original);
        if (!archive && archive.error() == "not an IFS file") {
            not_ifs++;
            continue;
        }
        INFO(label);
        REQUIRE(archive.has_value());
        RoundTrip::Problems problems;
        const bool identical = CheckArchive(original, *archive, counters, problems);
        for (const std::string& problem : problems.list)
            FAIL_CHECK(label << " " << problem);
        counters.identical_archives += identical ? 1U : 0U;
        if (!identical) differing.push_back(label);
    }
    PrintSummary(archives.size(), not_ifs, counters, differing);
}
