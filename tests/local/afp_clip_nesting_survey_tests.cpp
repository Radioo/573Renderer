#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
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
#include <variant>
#include <vector>

namespace {

struct Counts {
    std::size_t animations = 0;
    std::size_t with_sprites = 0;
    std::size_t root_placements = 0;
    std::size_t sprite_placements = 0;
    std::size_t sprites = 0;
    std::size_t animated_sprites = 0;
    std::size_t root_one_frame = 0;
    std::size_t mostly_nested = 0;
    std::size_t root_cameras = 0;
    std::size_t sprite_cameras = 0;
    std::size_t sprite_labels = 0;
    std::size_t exported_sprites = 0;
    std::map<std::string, std::size_t> sprite_label_order;
    std::map<std::string, std::size_t> sprite_tag_frame;
    std::map<std::size_t, std::size_t> deepest;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::size_t Placements(const AfpAnimation::Container& clip) {
    return static_cast<std::size_t>(
        std::ranges::count_if(clip.tags, [](const AfpAnimation::Tag& tag) {
            return std::holds_alternative<AfpAnimation::Placement>(tag.body);
        }));
}

std::size_t Cameras(const AfpAnimation::Container& clip) {
    return static_cast<std::size_t>(
        std::ranges::count_if(clip.tags, [](const AfpAnimation::Tag& tag) {
            return std::holds_alternative<AfpAnimation::Camera>(tag.body);
        }));
}

std::string LabelOrder(const AfpAnimation::Animation& animation,
                       const AfpAnimation::Container& clip) {
    const auto text = [&animation](const AfpAnimation::Label& label) {
        return label.name < animation.strings.size() ? animation.strings[label.name]
                                                     : std::string();
    };
    const bool by_name = std::ranges::is_sorted(
        clip.labels, [&text](const AfpAnimation::Label& a, const AfpAnimation::Label& b) {
            return text(a) < text(b);
        });
    const bool by_frame = std::ranges::is_sorted(clip.labels, {}, &AfpAnimation::Label::frame);
    const std::string flag = (animation.flags & 0x8) != 0 ? "flag 0x8" : "no flag";
    if (by_name && by_frame) return flag + ", both";
    if (by_name) return flag + ", by name";
    if (by_frame) return flag + ", by frame";
    return flag + ", neither";
}

void CountSpriteFrames(const AfpAnimation::Container& root, Counts& counts) {
    for (std::size_t index = 0; index < root.tags.size(); index++) {
        if (!std::holds_alternative<AfpAnimation::Sprite>(root.tags[index].body)) continue;
        std::string where = "in no frame";
        for (std::size_t frame = 0; frame < root.frames.size(); frame++) {
            const AfpAnimation::Frame& owner = root.frames[frame];
            if (index >= owner.first_tag && index < owner.first_tag + owner.tag_count) {
                where = frame == 0 ? "in frame 0" : "in a later frame";
                break;
            }
        }
        counts.sprite_tag_frame[where]++;
    }
}

void CountLabelOrder(const AfpAnimation::Animation& animation, Counts& counts) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr || sprite->container.labels.size() < 2) continue;
        counts.sprite_label_order[LabelOrder(animation, sprite->container)]++;
    }
}

std::size_t CountSprites(const AfpAnimation::Container& clip, std::size_t level, Counts& counts,
                         std::size_t& nested) {
    std::size_t deepest = level;
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr) continue;
        counts.sprites++;
        if (sprite->container.frames.size() > 1) counts.animated_sprites++;
        counts.sprite_cameras += Cameras(sprite->container);
        counts.sprite_labels += sprite->container.labels.size();
        nested += Placements(sprite->container);
        deepest = std::max(deepest, CountSprites(sprite->container, level + 1, counts, nested));
    }
    return deepest;
}

bool ExportsSprite(const AfpAnimation::Animation& animation, const AfpAnimation::Export& exported) {
    return std::ranges::any_of(animation.root.tags, [&exported](const AfpAnimation::Tag& tag) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        return sprite != nullptr && sprite->id == exported.tag;
    });
}

void CountAnimation(const AfpAnimation::Animation& animation, Counts& counts) {
    counts.animations++;
    CountLabelOrder(animation, counts);
    CountSpriteFrames(animation.root, counts);
    const std::size_t root = Placements(animation.root);
    std::size_t nested = 0;
    const std::size_t deepest = CountSprites(animation.root, 0, counts, nested);
    counts.deepest[deepest]++;
    counts.root_placements += root;
    counts.root_cameras += Cameras(animation.root);
    counts.exported_sprites += static_cast<std::size_t>(std::ranges::count_if(
        animation.exports, [&animation](const AfpAnimation::Export& exported) {
            return ExportsSprite(animation, exported);
        }));
    counts.sprite_placements += nested;
    if (nested > 0) counts.with_sprites++;
    if (animation.root.frames.size() <= 1) counts.root_one_frame++;
    if (nested > root) counts.mostly_nested++;
}

void CountNodes(const Document::File& file, const std::vector<Document::Node>& nodes,
                Counts& counts) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) {
            const auto animation = file.ReadAnimation(node.path);
            if (animation) CountAnimation(*animation, counts);
        }
        CountNodes(file, node.children, counts);
    }
}

}

TEST_CASE("How much of a shipped animation lives inside its sprites") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        auto file = Document::File::Open(ReadAll(entry.path()));
        if (!file) continue;
        files++;
        CountNodes(*file, file->Nodes(), counts);
    }

    std::cerr << std::format(
        "[nesting] {} files, {} animations, {} with sprite placements, {} mostly nested, {} with a "
        "one frame root\n",
        files, counts.animations, counts.with_sprites, counts.mostly_nested, counts.root_one_frame);
    std::cerr << std::format(
        "[nesting] {} root placements, {} sprite placements, {} sprites, {} animated sprites\n",
        counts.root_placements, counts.sprite_placements, counts.sprites, counts.animated_sprites);
    std::cerr << std::format(
        "[nesting] {} root cameras, {} sprite cameras, {} sprite labels, {} exported sprites\n",
        counts.root_cameras, counts.sprite_cameras, counts.sprite_labels, counts.exported_sprites);
    for (const auto& [order, count] : counts.sprite_label_order)
        std::cerr << std::format("[nesting] sprite labels {}: {}\n", order, count);
    for (const auto& [where, count] : counts.sprite_tag_frame)
        std::cerr << std::format("[nesting] sprite definition {}: {}\n", where, count);
    for (const auto& [level, count] : counts.deepest)
        std::cerr << std::format("[nesting] deepest sprite level {}: {}\n", level, count);

    CHECK(counts.animations > 0);
}
