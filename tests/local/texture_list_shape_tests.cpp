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
#include <optional>
#include <string>
#include <vector>

namespace {

struct Shapes {
    std::map<std::string, std::size_t> images;
    std::map<std::string, std::size_t> rects;
    std::map<std::string, std::size_t> sizes;
    std::map<std::string, std::size_t> counts;
    std::map<std::string, std::size_t> gaps;
    std::size_t files = 0;
    std::size_t lists = 0;
    std::size_t textures = 0;
    std::size_t power_of_two = 0;
    std::size_t inside_the_atlas = 0;
    std::size_t outside_the_atlas = 0;
    std::size_t overlapping = 0;
    std::size_t half_pixel_edges = 0;
};

struct Box {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

bool PowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

std::optional<Box> RectOf(const BinaryXml::Node& image, const std::string& name) {
    for (const BinaryXml::Node& child : image.children) {
        if (child.name != name || child.value.size() != 8) continue;
        return Box{.left = static_cast<int>(BigEndian::ReadU16(child.value, 0)),
                   .right = static_cast<int>(BigEndian::ReadU16(child.value, 2)),
                   .top = static_cast<int>(BigEndian::ReadU16(child.value, 4)),
                   .bottom = static_cast<int>(BigEndian::ReadU16(child.value, 6))};
    }
    return std::nullopt;
}

bool Overlaps(const Box& a, const Box& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

int Gap(const Box& a, const Box& b) {
    const int across = std::max(b.left - a.right, a.left - b.right);
    const int down = std::max(b.top - a.bottom, a.top - b.bottom);
    return std::max(across, down);
}

std::string Bucket(std::size_t count) {
    if (count == 1) return "1";
    if (count <= 4) return "2 to 4";
    if (count <= 16) return "5 to 16";
    if (count <= 64) return "17 to 64";
    return "more than 64";
}

void CountGaps(const std::vector<Box>& boxes, Shapes& shapes) {
    int closest = -1;
    for (std::size_t i = 0; i < boxes.size(); i++) {
        for (std::size_t j = i + 1; j < boxes.size(); j++) {
            if (Overlaps(boxes[i], boxes[j])) {
                shapes.overlapping++;
                continue;
            }
            const int gap = Gap(boxes[i], boxes[j]);
            if (closest < 0 || gap < closest) closest = gap;
        }
    }
    if (closest >= 0) shapes.gaps[std::to_string(closest)]++;
}

void CountAtlas(const BinaryXml::Node& texture, Shapes& shapes) {
    std::optional<Box> size;
    for (const BinaryXml::Node& child : texture.children) {
        if (child.name == "size" && child.value.size() == 4) {
            size = Box{.left = 0,
                       .right = static_cast<int>(BigEndian::ReadU16(child.value, 0)),
                       .top = 0,
                       .bottom = static_cast<int>(BigEndian::ReadU16(child.value, 2))};
        }
    }
    if (!size) return;
    shapes.sizes[std::to_string(size->right) + "x" + std::to_string(size->bottom)]++;
    if (PowerOfTwo(size->right) && PowerOfTwo(size->bottom)) shapes.power_of_two++;

    std::vector<Box> boxes;
    for (const BinaryXml::Node& image : texture.children) {
        if (image.name != "image") continue;
        const std::optional<Box> rect = RectOf(image, "imgrect");
        if (rect) boxes.push_back(*rect);
    }
    shapes.counts[Bucket(boxes.size())]++;
    for (const Box& box : boxes) {
        if (box.left % 2 != 0 || box.right % 2 != 0 || box.top % 2 != 0 || box.bottom % 2 != 0)
            shapes.half_pixel_edges++;
        if (box.right <= 2 * size->right && box.bottom <= 2 * size->bottom) {
            shapes.inside_the_atlas++;
        } else {
            shapes.outside_the_atlas++;
        }
    }
    CountGaps(boxes, shapes);
}

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
        CountAtlas(texture, shapes);
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

    std::cerr << std::format("[atlases] {} of {} are powers of two\n", shapes.power_of_two,
                             shapes.textures);
    std::cerr << std::format("[atlases] {} images inside, {} outside, {} overlapping, {} on a "
                             "half pixel edge\n",
                             shapes.inside_the_atlas, shapes.outside_the_atlas, shapes.overlapping,
                             shapes.half_pixel_edges);
    for (const auto& [bucket, count] : shapes.counts)
        std::cerr << std::format("[atlases] images per atlas {}: {}\n", bucket, count);
    for (const auto& [gap, count] : shapes.gaps)
        std::cerr << std::format("[atlases] closest gap {}: {}\n", gap, count);
    std::size_t shown = 0;
    for (const auto& [size, count] : shapes.sizes) {
        if (shown++ >= 24) break;
        std::cerr << std::format("[atlases] size {}: {}\n", size, count);
    }

    CHECK(shapes.images.size() == 2);
    CHECK(shapes.images["image[name ]{uvrect:39 imgrect:39 }"] == 106370);
    CHECK(shapes.images["image[name ]{imgrect:39 uvrect:39 }"] == 2);
    CHECK(shapes.rects["2 -2 2 -2 "] == 106322);
    CHECK(shapes.rects["0 0 0 0 "] == 50);
}
