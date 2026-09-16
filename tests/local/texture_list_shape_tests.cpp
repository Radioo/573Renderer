#include <catch2/catch_test_macros.hpp>

#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
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
#include <map>
#include <string>
#include <vector>

namespace {

struct Shapes {
    std::map<std::string, std::size_t> images;
    std::map<std::string, std::size_t> rects;
    std::size_t files = 0;
    std::size_t lists = 0;
    std::size_t textures = 0;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string Shape(const BinaryXml::Node& node) {
    std::string out = node.name + "[";
    for (const BinaryXml::Node& attribute : node.attributes)
        out += attribute.name + " ";
    out += "]{";
    for (const BinaryXml::Node& child : node.children)
        out += child.name + ":" + std::to_string(child.type) + " ";
    return out + "}";
}

void CountRects(const BinaryXml::Node& image, Shapes& shapes) {
    const BinaryXml::Node* uv = nullptr;
    const BinaryXml::Node* rect = nullptr;
    for (const BinaryXml::Node& child : image.children) {
        if (child.name == "uvrect" && child.value.size() == 8) uv = &child;
        if (child.name == "imgrect" && child.value.size() == 8) rect = &child;
    }
    if (uv == nullptr || rect == nullptr) return;
    std::string difference;
    for (std::size_t i = 0; i < 4; i++) {
        const auto inset = static_cast<int>(BigEndian::ReadU16(uv->value, 2 * i));
        const auto whole = static_cast<int>(BigEndian::ReadU16(rect->value, 2 * i));
        difference += std::to_string(inset - whole) + " ";
    }
    shapes.rects[difference]++;
}

void CountList(const BinaryXml::Document& document, Shapes& shapes) {
    shapes.lists++;
    for (const BinaryXml::Node& texture : document.root.children) {
        if (texture.name != "texture") continue;
        shapes.textures++;
        for (const BinaryXml::Node& image : texture.children) {
            if (image.name != "image") continue;
            shapes.images[Shape(image)]++;
            CountRects(image, shapes);
        }
    }
}

void Walk(const std::string& dir, Shapes& shapes) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        const auto archive = Ifs::Read(ReadAll(entry.path()));
        if (!archive) continue;
        shapes.files++;
        const auto tex = std::ranges::find(archive->entries, std::string("tex"), &Ifs::Entry::name);
        if (tex == archive->entries.end()) continue;
        const auto list =
            std::ranges::find(tex->children, std::string("texturelist_Exml"), &Ifs::Entry::name);
        if (list == tex->children.end()) continue;
        const auto document = BinaryXml::Read(list->bytes);
        if (!document) continue;
        CountList(*document, shapes);
    }
}

}

TEST_CASE("Every image in the install carries a uvrect inset inside its imgrect") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Shapes shapes;
    Walk(dir, shapes);

    std::cerr << std::format("[texture lists] {} files, {} lists, {} textures\n", shapes.files,
                             shapes.lists, shapes.textures);
    for (const auto& [shape, count] : shapes.images)
        std::cerr << std::format("[texture lists] image {}: {}\n", shape, count);
    for (const auto& [difference, count] : shapes.rects) {
        std::cerr << std::format("[texture lists] uvrect minus imgrect {}: {}\n", difference,
                                 count);
    }

    CHECK(shapes.images.size() == 2);
    CHECK(shapes.images["image[name ]{uvrect:39 imgrect:39 }"] == 106370);
    CHECK(shapes.images["image[name ]{imgrect:39 uvrect:39 }"] == 2);
    CHECK(shapes.rects["2 -2 2 -2 "] == 106322);
    CHECK(shapes.rects["0 0 0 0 "] == 50);
}
