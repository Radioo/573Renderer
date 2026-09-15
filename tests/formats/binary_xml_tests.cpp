#include <catch2/catch_test_macros.hpp>

#include "formats/binary_xml.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<uint8_t> FromHex(const std::string& hex) {
    std::vector<uint8_t> out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::vector<uint8_t> Bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

std::vector<uint8_t> Text(const std::string& s) {
    std::vector<uint8_t> out(s.begin(), s.end());
    out.push_back(0);
    return out;
}

BinaryXml::Node Element(uint8_t type, std::string name, std::vector<uint8_t> value = {}) {
    BinaryXml::Node node;
    node.type = type;
    node.name = std::move(name);
    node.value = std::move(value);
    return node;
}

BinaryXml::Node Attribute(std::string name, const std::string& text) {
    return Element(BinaryXml::kAttributeType, std::move(name), Text(text));
}

const std::string kVoidRoot = "a04200ff000000080104df4d39feff0000000000";

const std::string kTexturelistLike =
    "a042807f00000044010be6af79eb7ab1bb8e402e08a34cb5deae380107e6af79eb7a802e06af4df29b902e04ce"
    "6caa4504e2efeafe0105bb29aca82e04ce6caa4506ebbdeaa390fefefefeff00000048000000066176736c7a00"
    "00000000000c6172676238383838726576000000000774657830303000000000000408000400000000056267"
    "303300000000000000080002000300040005";

const std::string kByteWordSlots =
    "a04200ff0000003c0105e31d39e0030198fe04019cfe0601a0fe3401a4fe1101a8fe0201acfe0b01b0fe0301b4"
    "fe0301b8fe0501bcfe1e01c0fe0a01c4fe0f01c8fefeff0000003801018005fffe1020000000070000000368690"
    "000060000001234000000000001fffffffe0000000300000003deadbe003ff8000000000000";

const std::string kLongNames =
    "a045a05f000000880b44612e622d632e4e6174747220776974682073706163652e407a0180236e6e6e6e6e6e6e"
    "6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e"
    "6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e6e"
    "6e6e6efefeff00000000000018000000027800000000000002320000000000000231000000";

BinaryXml::Document ByteWordSlotsDocument() {
    BinaryXml::Document doc;
    doc.root = Element(1, "slots");
    auto& c = doc.root.children;
    c.push_back(Element(3, "a", {0x01}));
    c.push_back(Element(4, "b", {0xFF, 0xFE}));
    c.push_back(Element(6, "c", {0, 0, 0, 7}));
    c.push_back(Element(52, "d", {0x01}));
    c.push_back(Element(17, "e", {0x10, 0x20}));
    c.push_back(Element(2, "f", {0x80}));
    c.push_back(Element(11, "g", Text("hi")));
    c.push_back(Element(3, "h", {0x05}));
    c.push_back(Element(3, "i", {0x06}));
    c.push_back(Element(5, "j", {0x12, 0x34}));
    c.push_back(Element(30, "k", {0, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0xFE, 0, 0, 0, 3}));
    c.push_back(Element(10, "l", {0xDE, 0xAD, 0xBE}));
    c.push_back(Element(15, "m", {0x3F, 0xF8, 0, 0, 0, 0, 0, 0}));
    return doc;
}

}

TEST_CASE("Read a void root with a sixbit name") {
    const auto doc = BinaryXml::Read(FromHex(kVoidRoot));
    REQUIRE(doc.has_value());
    CHECK(doc->signature == BinaryXml::kSixBitNames);
    CHECK(doc->encoding == 0);
    CHECK(doc->root.name == "root");
    CHECK(doc->root.type == 1);
    CHECK(doc->root.value.empty());
    CHECK(doc->root.children.empty());
}

TEST_CASE("Write a void root with a sixbit name") {
    BinaryXml::Document doc;
    doc.root = Element(1, "root");
    const auto bytes = BinaryXml::Write(doc);
    REQUIRE(bytes.has_value());
    CHECK(*bytes == FromHex(kVoidRoot));
}

TEST_CASE("Read attributes, arrays and nesting") {
    const auto doc = BinaryXml::Read(FromHex(kTexturelistLike));
    REQUIRE(doc.has_value());
    CHECK(doc->encoding == 0x80);
    const BinaryXml::Node& root = doc->root;
    CHECK(root.name == "texturelist");
    REQUIRE(root.attributes.size() == 1);
    CHECK(root.attributes[0].name == "compress");
    CHECK(root.attributes[0].value == Text("avslz"));
    REQUIRE(root.children.size() == 1);
    const BinaryXml::Node& texture = root.children[0];
    REQUIRE(texture.attributes.size() == 2);
    CHECK(texture.attributes[0].name == "format");
    CHECK(texture.attributes[1].name == "name");
    CHECK(texture.attributes[1].value == Text("tex000"));
    REQUIRE(texture.children.size() == 2);
    CHECK(texture.children[0].type == (BinaryXml::kArrayFlag | 5));
    CHECK(texture.children[0].value == std::vector<uint8_t>{0x08, 0x00, 0x04, 0x00});
    CHECK(texture.children[1].children[0].name == "uvrect");
}

TEST_CASE("Write packs byte and word values into shared slots") {
    const auto bytes = BinaryXml::Write(ByteWordSlotsDocument());
    REQUIRE(bytes.has_value());
    CHECK(*bytes == FromHex(kByteWordSlots));
}

TEST_CASE("Write sorts attributes by name and supports long names") {
    BinaryXml::Document doc;
    doc.signature = BinaryXml::kByteNames;
    doc.encoding = 0xA0;
    doc.root = Element(11, "a.b-c", Text("x"));
    doc.root.attributes.push_back(Attribute("z", "1"));
    doc.root.attributes.push_back(Attribute("attr with space", "2"));
    doc.root.children.push_back(Element(1, std::string(100, 'n')));
    const auto bytes = BinaryXml::Write(doc);
    REQUIRE(bytes.has_value());
    CHECK(*bytes == FromHex(kLongNames));
}

TEST_CASE("Read then write reproduces every fixture byte for byte") {
    for (const std::string& hex : {kVoidRoot, kTexturelistLike, kByteWordSlots, kLongNames}) {
        const std::vector<uint8_t> original = FromHex(hex);
        const auto doc = BinaryXml::Read(original);
        REQUIRE(doc.has_value());
        const auto again = BinaryXml::Write(*doc);
        REQUIRE(again.has_value());
        CHECK(*again == original);
    }
}

TEST_CASE("String bytes invalid in the declared encoding survive a round trip") {
    BinaryXml::Document doc;
    doc.encoding = 0x80;
    doc.root = Element(11, "s", {0x83, 0xFF, 0x81, 0x00});
    const auto bytes = BinaryXml::Write(doc);
    REQUIRE(bytes.has_value());
    const auto back = BinaryXml::Read(*bytes);
    REQUIRE(back.has_value());
    CHECK(back->root.value == std::vector<uint8_t>{0x83, 0xFF, 0x81, 0x00});
}

TEST_CASE("Read rejects malformed documents") {
    const std::vector<uint8_t> good = FromHex(kTexturelistLike);

    std::vector<uint8_t> bad_magic = good;
    bad_magic[0] = 0xA1;
    CHECK_FALSE(BinaryXml::Read(bad_magic).has_value());

    std::vector<uint8_t> bad_complement = good;
    bad_complement[3] = 0x00;
    CHECK_FALSE(BinaryXml::Read(bad_complement).has_value());

    std::vector<uint8_t> truncated = good;
    truncated.pop_back();
    CHECK_FALSE(BinaryXml::Read(truncated).has_value());

    std::vector<uint8_t> bad_type = good;
    bad_type[8] = 0x80;
    CHECK_FALSE(BinaryXml::Read(bad_type).has_value());

    std::vector<uint8_t> bad_node_length = good;
    bad_node_length[7] = 0x40;
    CHECK_FALSE(BinaryXml::Read(bad_node_length).has_value());

    CHECK_FALSE(BinaryXml::Read(Bytes("\xA0")).has_value());
}

TEST_CASE("Write rejects names the chosen name form cannot hold") {
    BinaryXml::Document doc;
    doc.root = Element(1, "has space");
    CHECK_FALSE(BinaryXml::Write(doc).has_value());

    doc.root = Element(1, std::string(37, 'a'));
    CHECK_FALSE(BinaryXml::Write(doc).has_value());

    doc.root = Element(1, "");
    CHECK_FALSE(BinaryXml::Write(doc).has_value());
}

TEST_CASE("Write rejects duplicate attribute names") {
    BinaryXml::Document doc;
    doc.root = Element(1, "root");
    doc.root.attributes.push_back(Attribute("a", "1"));
    doc.root.attributes.push_back(Attribute("a", "2"));
    CHECK_FALSE(BinaryXml::Write(doc).has_value());
}
