#include <catch2/catch_test_macros.hpp>

#include "formats/ddr_arc.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace {

void PushU32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFFU));
    v.push_back(static_cast<uint8_t>((x >> 8U) & 0xFFU));
    v.push_back(static_cast<uint8_t>((x >> 16U) & 0xFFU));
    v.push_back(static_cast<uint8_t>((x >> 24U) & 0xFFU));
}

void PushBytes(std::vector<uint8_t>& v, const std::string& s) {
    for (const char c : s)
        v.push_back(static_cast<uint8_t>(c));
}

std::vector<uint8_t> AbcLz77Stream() {
    return {0x07, 'A', 'B', 'C', 0x00, 0x33, 0x00, 0x00};
}

std::vector<uint8_t> BuildTwoEntryArc() {
    const std::string name0 = "scene/a.ifs";
    const std::string name1 = "extra/b.bin";
    const std::vector<uint8_t> comp = AbcLz77Stream();
    const std::string stored_payload = "STOREDDATA";

    const uint32_t names_off0 = 48;
    const auto names_off1 = static_cast<uint32_t>(names_off0 + name0.size() + 1);
    const auto data_off0 = static_cast<uint32_t>(names_off1 + name1.size() + 1);
    const auto data_off1 = static_cast<uint32_t>(data_off0 + comp.size());

    std::vector<uint8_t> arc;
    PushU32(arc, DdrArc::kMagic);
    PushU32(arc, 2);
    PushU32(arc, 2);
    PushU32(arc, 1);

    PushU32(arc, names_off0);
    PushU32(arc, data_off0);
    PushU32(arc, 9);
    PushU32(arc, static_cast<uint32_t>(comp.size()));

    PushU32(arc, names_off1);
    PushU32(arc, data_off1);
    PushU32(arc, static_cast<uint32_t>(stored_payload.size()));
    PushU32(arc, static_cast<uint32_t>(stored_payload.size()));

    PushBytes(arc, name0);
    arc.push_back(0);
    PushBytes(arc, name1);
    arc.push_back(0);

    arc.insert(arc.end(), comp.begin(), comp.end());
    PushBytes(arc, stored_payload);
    return arc;
}

std::string AsString(const std::vector<uint8_t>& v) {
    std::string s;
    for (const uint8_t b : v)
        s.push_back(static_cast<char>(b));
    return s;
}

}

TEST_CASE("Lz77Decompress literals only") {
    const std::vector<uint8_t> src = {0x07, 'A', 'B', 'C'};
    CHECK(AsString(DdrArc::Lz77Decompress(src, 0)) == "ABC");
}

TEST_CASE("Lz77Decompress back-reference expands window match") {
    CHECK(AsString(DdrArc::Lz77Decompress(AbcLz77Stream(), 0)) == "ABCABCABC");
}

TEST_CASE("Lz77Decompress reads zero pre-history") {
    const std::vector<uint8_t> src = {0x00, 0x06, 0x41};
    const std::vector<uint8_t> expected = {0, 0, 0, 0};
    CHECK(DdrArc::Lz77Decompress(src, 0) == expected);
}

TEST_CASE("Lz77Decompress stops at expected_size") {
    const std::vector<uint8_t> src = {0x1F, 'A', 'B', 'C', 'D', 'E'};
    CHECK(AsString(DdrArc::Lz77Decompress(src, 3)) == "ABC");
}

TEST_CASE("Lz77Decompress stops at zero-distance end marker") {
    const std::vector<uint8_t> src = {0x01, 'X', 0x00, 0x00, 'Y', 'Z'};
    CHECK(AsString(DdrArc::Lz77Decompress(src, 0)) == "X");
}

TEST_CASE("ParseToc reads header and entries") {
    const std::vector<uint8_t> arc = BuildTwoEntryArc();
    DdrArc::Toc toc;
    REQUIRE(DdrArc::ParseToc(arc, toc));
    CHECK(toc.version == 2);
    CHECK(toc.comp_flag == 1);
    REQUIRE(toc.entries.size() == 2);
    CHECK(toc.entries[0].name == "scene/a.ifs");
    CHECK(toc.entries[1].name == "extra/b.bin");
    CHECK_FALSE(toc.entries[0].stored());
    CHECK(toc.entries[1].stored());
    CHECK(toc.HasIfs());
}

TEST_CASE("ParseToc rejects bad input") {
    DdrArc::Toc toc;
    std::vector<uint8_t> arc = BuildTwoEntryArc();
    arc[0] ^= 0xFFU;
    CHECK_FALSE(DdrArc::ParseToc(arc, toc));

    const std::vector<uint8_t> tiny = {0x20, 0x11, 0x75, 0x19};
    CHECK_FALSE(DdrArc::ParseToc(tiny, toc));

    std::vector<uint8_t> truncated = BuildTwoEntryArc();
    truncated.resize(20);
    CHECK_FALSE(DdrArc::ParseToc(truncated, toc));
}

TEST_CASE("DecompressEntry handles stored and compressed entries") {
    const std::vector<uint8_t> arc = BuildTwoEntryArc();
    DdrArc::Toc toc;
    REQUIRE(DdrArc::ParseToc(arc, toc));
    CHECK(AsString(DdrArc::DecompressEntry(arc, toc.entries[0])) == "ABCABCABC");
    CHECK(AsString(DdrArc::DecompressEntry(arc, toc.entries[1])) == "STOREDDATA");

    DdrArc::Entry bad = toc.entries[1];
    bad.data_offset = static_cast<uint32_t>(arc.size());
    CHECK(DdrArc::DecompressEntry(arc, bad).empty());
}

TEST_CASE("ReadToc and ExtractFirstIfs work from disk") {
    const std::vector<uint8_t> arc = BuildTwoEntryArc();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "r573_formats_test.arc";
    {
        std::ofstream f(path, std::ios::binary);
        REQUIRE(f.is_open());
        for (const uint8_t b : arc)
            f.put(static_cast<char>(b));
    }

    DdrArc::Toc toc;
    REQUIRE(DdrArc::ReadToc(path.string(), toc));
    REQUIRE(toc.entries.size() == 2);
    CHECK(toc.entries[0].name == "scene/a.ifs");

    std::string inner;
    const std::vector<uint8_t> ifs = DdrArc::ExtractFirstIfs(path.string(), inner);
    CHECK(inner == "scene/a.ifs");
    CHECK(AsString(ifs) == "ABCABCABC");

    std::filesystem::remove(path);
    DdrArc::Toc missing;
    CHECK_FALSE(DdrArc::ReadToc(path.string(), missing));
}
