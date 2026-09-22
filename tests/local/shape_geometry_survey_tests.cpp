#include <catch2/catch_test_macros.hpp>

#include "formats/afp_animation.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ge2d_shape.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "support/env.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

struct Counts {
    std::size_t packages = 0;
    std::map<std::string, std::size_t> sizes;
    std::map<std::string, std::size_t> origins;
    std::map<std::string, std::size_t> ids;
    std::map<uint8_t, std::size_t> geo_types;
};

struct Pixels {
    double width = 0;
    double height = 0;
};

struct ImageRects {
    Pixels image;
    Pixels uv;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

const Ifs::Entry* Child(const std::vector<Ifs::Entry>& entries, std::string_view logical) {
    const auto escaped = Ifs::EscapeName(logical);
    if (!escaped) return nullptr;
    const auto found = std::ranges::find(entries, *escaped, &Ifs::Entry::name);
    return found == entries.end() ? nullptr : &*found;
}

std::optional<Pixels> RectPixels(const BinaryXml::Node& image, std::string_view name) {
    for (const BinaryXml::Node& child : image.children) {
        if (child.name != name || child.value.size() != 8) continue;
        const double x0 = BigEndian::ReadU16(child.value, 0);
        const double x1 = BigEndian::ReadU16(child.value, 2);
        const double y0 = BigEndian::ReadU16(child.value, 4);
        const double y1 = BigEndian::ReadU16(child.value, 6);
        return Pixels{.width = (x1 - x0) / 2, .height = (y1 - y0) / 2};
    }
    return std::nullopt;
}

std::string Attribute(const BinaryXml::Node& node, std::string_view name) {
    for (const BinaryXml::Node& attribute : node.attributes) {
        if (attribute.name != name) continue;
        std::string text(attribute.value.begin(), attribute.value.end());
        if (!text.empty() && text.back() == '\0') text.pop_back();
        return text;
    }
    return {};
}

std::map<std::string, ImageRects> ImagesOf(const Ifs::Entry& tex) {
    std::map<std::string, ImageRects> images;
    const Ifs::Entry* list = Child(tex.children, "texturelist.xml");
    if (list == nullptr) return images;
    const auto document = BinaryXml::Read(list->bytes);
    if (!document) return images;
    for (const BinaryXml::Node& texture : document->root.children) {
        for (const BinaryXml::Node& image : texture.children) {
            if (image.name != "image") continue;
            const auto whole = RectPixels(image, "imgrect");
            const auto inset = RectPixels(image, "uvrect");
            if (!whole || !inset) continue;
            images.try_emplace(Attribute(image, "name"), ImageRects{.image = *whole, .uv = *inset});
        }
    }
    return images;
}

std::string Compare(double value, const ImageRects& rects, bool across) {
    const double image = across ? rects.image.width : rects.image.height;
    const double uv = across ? rects.uv.width : rects.uv.height;
    if (value == uv) return "uv";
    if (value == image) return "image";
    if (value == uv * 2) return "2 uv";
    if (value == image * 2) return "2 image";
    return "other";
}

std::string Origin(float min, float max) {
    if (min == 0) return "0";
    if (min == -max) return "centred";
    return "other";
}

float At(const std::array<uint32_t, 2>& point, std::size_t axis) {
    return std::bit_cast<float>(point.at(axis));
}

bool UvsFollowCorners(const Ge2dShape::Shape& shape, const std::array<float, 2>& high) {
    for (std::size_t i = 0; i < shape.vertices.size(); i++) {
        const bool right = At(shape.vertices[i], 0) == high[0];
        const bool bottom = At(shape.vertices[i], 1) == high[1];
        const bool uv_right = At(shape.uvs.at(i), 0) > At(shape.uvs[0], 0);
        const bool uv_bottom = At(shape.uvs.at(i), 1) > At(shape.uvs[0], 1);
        if (right != (i % 2 == 1) || bottom != (i >= 2)) return false;
        if (uv_right != right || uv_bottom != bottom) return false;
    }
    return true;
}

void CountShape(const Ge2dShape::Shape& shape, const std::map<std::string, ImageRects>& images,
                Counts& counts) {
    if (shape.primitives.size() != 1 || shape.texture_names.size() != 1 ||
        shape.vertices.size() != 4 || shape.uvs.size() != 4)
        return;
    if (shape.primitives[0].draw_flags != 0x3) return;
    const auto found = images.find(shape.texture_names[0]);
    if (found == images.end()) return;
    std::array<float, 2> low{INFINITY, INFINITY};
    std::array<float, 2> high{-INFINITY, -INFINITY};
    for (const auto& vertex : shape.vertices) {
        for (std::size_t axis = 0; axis < 2; axis++) {
            const auto value = std::bit_cast<float>(vertex.at(axis));
            low.at(axis) = std::min(low.at(axis), value);
            high.at(axis) = std::max(high.at(axis), value);
        }
    }
    counts.sizes[Compare(high[0] - low[0], found->second, true) + " x " +
                 Compare(high[1] - low[1], found->second, false)]++;
    counts.origins[Origin(low[0], high[0]) + " / " + Origin(low[1], high[1])]++;
    counts.origins[UvsFollowCorners(shape, high) ? "uvs follow the vertex corners"
                                                 : "uvs in another order"]++;
}

std::vector<uint16_t> GeoIds(const BinaryXml::Node& listed, Counts& counts) {
    std::vector<uint16_t> ids;
    for (const BinaryXml::Node& child : listed.children) {
        if (child.name != "geo") continue;
        counts.geo_types[child.type]++;
        for (std::size_t at = 0; at + 1 < child.value.size(); at += 2)
            ids.push_back(BigEndian::ReadU16(child.value, at));
    }
    return ids;
}

std::map<uint16_t, uint16_t> ShapeTags(const Ifs::Entry& afp, const std::string& name,
                                       Counts& counts) {
    std::map<uint16_t, uint16_t> ids;
    const auto stored = std::ranges::find(afp.children, Ifs::HashedName(name), &Ifs::Entry::name);
    const Ifs::Entry* bsi = Child(afp.children, "bsi");
    if (stored == afp.children.end() || bsi == nullptr) {
        counts.ids["no afp entry"]++;
        return ids;
    }
    const auto script = std::ranges::find(bsi->children, Ifs::HashedName(name), &Ifs::Entry::name);
    if (script == bsi->children.end()) return ids;
    const auto animation = AfpAnimation::ReadStored(stored->bytes, script->bytes);
    if (!animation) {
        counts.ids["unreadable: " + animation.error()]++;
        return ids;
    }
    const std::string header =
        animation->name < animation->strings.size() ? animation->strings[animation->name] : "";
    counts.ids[header == name ? "header name is the listed name" : "header name differs"]++;
    const std::size_t leading =
        animation->root.frames.empty() ? 0 : animation->root.frames.front().first_tag;
    const std::size_t first_end =
        animation->root.frames.empty() ? 0 : leading + animation->root.frames.front().tag_count;
    std::optional<uint16_t> previous;
    for (std::size_t index = 0; index < animation->root.tags.size(); index++) {
        const auto* shape = std::get_if<AfpAnimation::Shape>(&animation->root.tags[index].body);
        if (shape == nullptr) continue;
        ids[shape->id] = shape->unread_word;
        if (index < leading) {
            counts.ids["shape tag leading"]++;
        } else if (index < first_end) {
            counts.ids["shape tag in frame 0"]++;
        } else {
            counts.ids["shape tag after frame 0"]++;
        }
        if (previous && *previous > shape->id) counts.ids["shape tags out of id order"]++;
        previous = shape->id;
    }
    return ids;
}

void CountIds(const std::map<uint16_t, uint16_t>& words, const std::vector<uint16_t>& listed,
              bool twice, Counts& counts) {
    const std::string where = twice ? " (name listed twice)" : " (name listed once)";
    std::set<uint16_t> tags;
    for (const auto& [id, word] : words)
        tags.insert(id);
    const std::set<uint16_t> geo(listed.begin(), listed.end());
    counts.ids[(tags == geo ? "shape tags equal geo ids" : "shape tags differ from geo ids") +
               where]++;
    if (!std::ranges::is_sorted(listed)) counts.ids["geo ids unsorted" + where]++;
    if (geo.size() != listed.size()) counts.ids["geo ids repeated" + where]++;
}

void CountPackage(const Ifs::Archive& archive, Counts& counts) {
    const Ifs::Entry* magic = Child(archive.entries, "magic");
    const Ifs::Entry* afp = Child(archive.entries, "afp");
    const Ifs::Entry* geo = Child(archive.entries, "geo");
    const Ifs::Entry* tex = Child(archive.entries, "tex");
    if (magic == nullptr || afp == nullptr || geo == nullptr || tex == nullptr) return;
    const auto order = Ge2dShape::PackageByteOrder(magic->bytes);
    const Ifs::Entry* list = Child(afp->children, "afplist.xml");
    if (!order || list == nullptr) return;
    const auto document = BinaryXml::Read(list->bytes);
    if (!document) return;
    counts.packages++;
    const std::map<std::string, ImageRects> images = ImagesOf(*tex);
    std::map<std::string, std::size_t> seen;
    for (const BinaryXml::Node& listed : document->root.children)
        seen[Attribute(listed, "name")]++;
    for (const BinaryXml::Node& listed : document->root.children) {
        const std::string name = Attribute(listed, "name");
        const auto geo_nodes =
            std::ranges::count(listed.children, std::string("geo"), &BinaryXml::Node::name);
        counts.ids[std::format("listed {} with {} geo arrays", seen[name] > 1 ? "twice" : "once",
                               geo_nodes)]++;
        const std::vector<uint16_t> ids = GeoIds(listed, counts);
        const std::map<uint16_t, uint16_t> words = ShapeTags(*afp, name, counts);
        CountIds(words, ids, seen[name] > 1, counts);
        for (const uint16_t id : ids) {
            const auto stored = std::ranges::find(
                geo->children, Ifs::HashedName(std::format("{}_shape{}", name, id)),
                &Ifs::Entry::name);
            if (stored == geo->children.end()) continue;
            const auto shape = Ge2dShape::Read(stored->bytes, *order);
            if (!shape) continue;
            CountShape(*shape, images, counts);
            const auto word = words.find(id);
            if (word == words.end() || shape->primitives.empty()) continue;
            counts.ids[std::format("shape tag word {:#x} with header flags {:#x} draw flags {:#x}",
                                   word->second, shape->flags, shape->primitives[0].draw_flags)]++;
        }
    }
}

}

TEST_CASE("Shipped textured shapes relate their quad to the image they draw") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data/graphic")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        const auto archive = Ifs::Read(ReadAll(entry.path()));
        if (archive) CountPackage(*archive, counts);
    }

    std::cerr << std::format("[shape geometry] {} packages\n", counts.packages);
    for (const auto& [what, count] : counts.sizes)
        std::cerr << std::format("[shape geometry] size {}: {}\n", what, count);
    for (const auto& [what, count] : counts.origins)
        std::cerr << std::format("[shape geometry] origin {}: {}\n", what, count);
    for (const auto& [what, count] : counts.ids)
        std::cerr << std::format("[shape geometry] {}: {}\n", what, count);
    for (const auto& [type, count] : counts.geo_types)
        std::cerr << std::format("[shape geometry] geo node type {}: {}\n", type, count);
    CHECK(counts.packages > 0);
}
