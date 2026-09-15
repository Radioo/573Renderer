#include <catch2/catch_test_macros.hpp>

#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"

#include <md5.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint8_t kS32 = 6;
constexpr uint8_t kU32 = 7;
constexpr uint8_t kU8 = 3;
constexpr uint8_t kBin = 10;
constexpr uint8_t k3S32 = 30;
constexpr std::size_t kHeaderWithMd5 = 36;

uint32_t ReadU32BE(std::span<const uint8_t> b, std::size_t off) {
    return (static_cast<uint32_t>(b[off]) << 24U) | (static_cast<uint32_t>(b[off + 1]) << 16U) |
           (static_cast<uint32_t>(b[off + 2]) << 8U) | static_cast<uint32_t>(b[off + 3]);
}

std::array<uint8_t, 16> Md5Of(std::span<const uint8_t> bytes) {
    MD5 md5;
    md5.add(bytes.data(), bytes.size());
    std::array<uint8_t, 16> out{};
    md5.getHash(out.data());
    return out;
}

std::vector<uint8_t> Filled(std::size_t size, uint8_t seed) {
    std::vector<uint8_t> out(size);
    for (std::size_t i = 0; i < size; i++)
        out[i] = static_cast<uint8_t>(seed + i);
    return out;
}

BinaryXml::Node Value(uint8_t type, std::string name, std::vector<uint8_t> value) {
    BinaryXml::Node node;
    node.type = type;
    node.name = std::move(name);
    node.value = std::move(value);
    return node;
}

Ifs::Entry InfoEntry() {
    Ifs::Entry info;
    info.kind = Ifs::EntryKind::Special;
    info.name = "_info_";
    info.special = Value(1, "_info_", {});
    info.special.children.push_back(Value(kBin, "md5", std::vector<uint8_t>(16, 0)));
    info.special.children.push_back(Value(kU32, "size", {0, 0, 0, 0}));
    return info;
}

Ifs::Entry FileEntry(std::string name, std::vector<uint8_t> bytes, int32_t time) {
    Ifs::Entry file;
    file.kind = Ifs::EntryKind::File;
    file.name = std::move(name);
    file.type = k3S32;
    file.time = time;
    file.bytes = std::move(bytes);
    return file;
}

Ifs::Archive SampleArchive() {
    Ifs::Archive archive;
    archive.time = 1700000000;
    archive.entries.push_back(InfoEntry());
    Ifs::Entry tex;
    tex.kind = Ifs::EntryKind::Directory;
    tex.name = "tex";
    tex.type = kS32;
    tex.time = 5;
    tex.children.push_back(FileEntry("big", Filled(100, 1), 7));
    archive.entries.push_back(std::move(tex));
    archive.entries.push_back(FileEntry("mid", Filled(20, 50), 8));
    archive.entries.push_back(FileEntry("magic", {'N', 'G', 'P', 'F'}, 9));
    return archive;
}

Ifs::Archive ReadBack(const Ifs::Archive& archive) {
    const auto bytes = Ifs::Write(archive);
    REQUIRE(bytes.has_value());
    auto back = Ifs::Read(*bytes);
    REQUIRE(back.has_value());
    return std::move(*back);
}

const Ifs::Entry& Find(const std::vector<Ifs::Entry>& entries, const std::string& name) {
    for (const Ifs::Entry& e : entries) {
        if (e.name == name) return e;
    }
    FAIL("missing entry " << name);
    return entries.front();
}

}

TEST_CASE("Read rejects files that are not IFS archives") {
    std::vector<uint8_t> stub(256, 0);
    stub[0] = 0x72;
    stub[1] = 0x9B;
    stub[2] = 0x79;
    stub[3] = 0xB1;
    CHECK_FALSE(Ifs::Read(stub).has_value());

    auto bytes = Ifs::Write(SampleArchive());
    REQUIRE(bytes.has_value());
    std::vector<uint8_t> bad_flags = *bytes;
    bad_flags[6] = 0x00;
    CHECK_FALSE(Ifs::Read(bad_flags).has_value());

    const std::vector<uint8_t> short_header(bytes->begin(), bytes->begin() + 10);
    CHECK_FALSE(Ifs::Read(short_header).has_value());
}

TEST_CASE("Write lays out files largest first, filling alignment gaps") {
    const Ifs::Archive back = ReadBack(SampleArchive());
    CHECK(Find(Find(back.entries, "tex").children, "big").stored_offset == 0);
    CHECK(Find(back.entries, "mid").stored_offset == 112);
    CHECK(Find(back.entries, "magic").stored_offset == 100);
    CHECK(back.stored_data_size == 144);
}

TEST_CASE("Write stores the data offset and the zero padded manifest MD5 in the header") {
    const auto bytes = Ifs::Write(SampleArchive());
    REQUIRE(bytes.has_value());
    CHECK(ReadU32BE(*bytes, 0) == 0x6CAD8F89U);
    CHECK(((*bytes)[4] ^ (*bytes)[6]) == 0xFF);
    CHECK(((*bytes)[5] ^ (*bytes)[7]) == 0xFF);
    CHECK(ReadU32BE(*bytes, 8) == 1700000000U);
    const uint32_t data_offset = ReadU32BE(*bytes, 16);
    CHECK(data_offset % 16 == 0);
    REQUIRE(data_offset > kHeaderWithMd5);
    const auto region = std::span(*bytes).subspan(kHeaderWithMd5, data_offset - kHeaderWithMd5);
    const std::array<uint8_t, 16> expected = Md5Of(region);
    CHECK(std::equal(expected.begin(), expected.end(), bytes->begin() + 20));
    CHECK(bytes->size() == data_offset + 144U);
}

TEST_CASE("Write fills _info_ with the size and MD5 of the data region") {
    const auto bytes = Ifs::Write(SampleArchive());
    REQUIRE(bytes.has_value());
    const Ifs::Archive back = ReadBack(SampleArchive());
    const Ifs::Entry& info = Find(back.entries, "_info_");
    REQUIRE(info.kind == Ifs::EntryKind::Special);
    REQUIRE(info.special.children.size() == 2);
    const uint32_t data_offset = ReadU32BE(*bytes, 16);
    const auto data = std::span(*bytes).subspan(data_offset);
    const std::array<uint8_t, 16> md5 = Md5Of(data);
    CHECK(info.special.children[0].value == std::vector<uint8_t>(md5.begin(), md5.end()));
    CHECK(info.special.children[1].value == std::vector<uint8_t>{0, 0, 0, 144});
}

TEST_CASE("Read then write keeps entries and reproduces the file") {
    const auto first = Ifs::Write(SampleArchive());
    REQUIRE(first.has_value());
    const auto archive = Ifs::Read(*first);
    REQUIRE(archive.has_value());
    REQUIRE(archive->entries.size() == 4);
    CHECK(archive->entries[0].name == "_info_");
    CHECK(archive->entries[1].name == "tex");
    CHECK(archive->entries[1].kind == Ifs::EntryKind::Directory);
    CHECK(archive->entries[1].time == 5);
    CHECK(archive->entries[1].children[0].bytes == Filled(100, 1));
    CHECK(archive->entries[3].time == 9);
    const auto second = Ifs::Write(*archive);
    REQUIRE(second.has_value());
    CHECK(*second == *first);
}

TEST_CASE("Write keeps a stored layout while every file still fits it") {
    Ifs::Archive archive = ReadBack(SampleArchive());
    Ifs::Entry& big = archive.entries[1].children[0];
    big.stored_offset = 4;
    archive.entries[2].stored_offset = 104;
    archive.entries[3].stored_offset = 0;
    archive.stored_data_size = 124;
    const Ifs::Archive back = ReadBack(archive);
    CHECK(back.entries[1].children[0].stored_offset == 4);
    CHECK(back.entries[2].stored_offset == 104);
    CHECK(back.entries[3].stored_offset == 0);
    CHECK(back.stored_data_size == 124);
}

TEST_CASE("Write lays files out again once a size no longer matches") {
    Ifs::Archive archive = ReadBack(SampleArchive());
    archive.entries[3].bytes = Filled(8, 3);
    const Ifs::Archive back = ReadBack(archive);
    CHECK(back.entries[1].children[0].stored_offset == 0);
    CHECK(back.entries[3].stored_offset == 100);
    CHECK(back.entries[2].stored_offset == 112);
    CHECK(back.entries[3].bytes == Filled(8, 3));
}

TEST_CASE("Files stored in a super image keep their reference and carry no bytes") {
    Ifs::Archive archive = SampleArchive();
    Ifs::Entry external = FileEntry("pre", {}, 11);
    external.image = 1;
    external.stored_offset = 50;
    external.stored_size = 10;
    external.extra_nodes.push_back(Value(kU8, "i", {1}));
    archive.entries.push_back(std::move(external));
    const Ifs::Archive back = ReadBack(archive);
    const Ifs::Entry& pre = Find(back.entries, "pre");
    CHECK(pre.image == 1);
    CHECK(pre.stored_offset == 50);
    CHECK(pre.stored_size == 10);
    CHECK(pre.bytes.empty());
    CHECK(Find(back.entries, "mid").stored_offset == 112);
}

TEST_CASE("Tree size covers the manifest and keeps a larger stored value") {
    CHECK(ReadBack(SampleArchive()).tree_size == 1104);
    Ifs::Archive roomy = SampleArchive();
    roomy.tree_size = 5000;
    CHECK(ReadBack(roomy).tree_size == 5000);
}
