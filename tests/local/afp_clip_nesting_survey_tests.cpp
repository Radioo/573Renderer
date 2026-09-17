#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/env.h"

#include <algorithm>
#include <array>
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
#include <utility>
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
    std::map<std::string, std::size_t> export_order;
    std::map<std::string, std::size_t> control_bits;
    std::map<std::string, std::size_t> resets;
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

std::string Folded(std::string text) {
    std::ranges::transform(text, text.begin(), [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return text;
}

std::string ExportOrder(const AfpAnimation::Animation& animation) {
    if (animation.exports.size() < 2) return "fewer than two";
    std::vector<std::string> names;
    names.reserve(animation.exports.size());
    for (const AfpAnimation::Export& exported : animation.exports) {
        names.push_back(exported.name < animation.strings.size() ? animation.strings[exported.name]
                                                                 : std::string());
    }
    std::vector<std::string> folded;
    folded.reserve(names.size());
    for (const std::string& name : names)
        folded.push_back(Folded(name));
    const bool by_folded = std::ranges::is_sorted(folded);
    const bool by_bytes = std::ranges::is_sorted(names);
    const bool by_tag = std::ranges::is_sorted(animation.exports, {}, &AfpAnimation::Export::tag);
    std::string order = by_folded ? "folded name" : "not folded name";
    order += by_bytes ? ", byte name" : ", not byte name";
    order += by_tag ? ", tag" : ", not tag";
    return order;
}

bool CarriesMatrix(const AfpAnimation::Placement& placement) {
    return placement.scale || placement.rotate_skew || placement.translation ||
           placement.short_scale || placement.short_rotate_skew;
}

bool CarriesColour(const AfpAnimation::Placement& placement) {
    return placement.multiply_colour || placement.add_colour || placement.packed_multiply_colour ||
           placement.packed_add_colour;
}

void CountControlBits(const AfpAnimation::Container& clip, Counts& counts) {
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr) continue;
        const std::string kind = (placement->flags & 0x1U) != 0 ? "update" : "create";
        const bool matrix = CarriesMatrix(*placement);
        const bool use_matrix = (placement->flags & 0x4U) != 0;
        const bool colour = CarriesColour(*placement);
        const bool use_colour = (placement->flags & 0x8U) != 0;
        counts.control_bits[std::format("{} matrix {} / 0x4 {}", kind, matrix ? "yes" : "no",
                                        use_matrix ? "set" : "clear")]++;
        counts.control_bits[std::format("{} colour {} / 0x8 {}", kind, colour ? "yes" : "no",
                                        use_colour ? "set" : "clear")]++;
    }
}

struct Effective {
    std::array<double, 6> matrix{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    std::array<double, 4> multiply{1.0, 1.0, 1.0, 1.0};
    std::array<double, 4> add{0.0, 0.0, 0.0, 0.0};
    bool long_scale = false;
    bool short_scale = false;
};

std::array<double, 4> Unpacked(uint32_t packed) {
    return {static_cast<double>((packed >> 24) & 0xFF) / 255.0,
            static_cast<double>((packed >> 16) & 0xFF) / 255.0,
            static_cast<double>((packed >> 8) & 0xFF) / 255.0,
            static_cast<double>(packed & 0xFF) / 255.0};
}

std::array<double, 4> Scaled(const std::array<int16_t, 4>& colour) {
    return {colour[0] / 255.0, colour[1] / 255.0, colour[2] / 255.0, colour[3] / 255.0};
}

void ApplyMatrix(const AfpAnimation::Placement& placement, Effective& state, bool fresh,
                 Counts& counts) {
    std::array<double, 6> next{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    std::array<bool, 6> carried{};
    if (placement.scale) {
        next[0] = (*placement.scale)[0] / 1024.0;
        next[3] = (*placement.scale)[1] / 1024.0;
        carried[0] = carried[3] = true;
    }
    if (placement.rotate_skew) {
        next[1] = (*placement.rotate_skew)[0] / 1024.0;
        next[2] = (*placement.rotate_skew)[1] / 1024.0;
        carried[1] = carried[2] = true;
    }
    if (placement.translation) {
        next[4] = (*placement.translation)[0];
        next[5] = (*placement.translation)[1];
        carried[4] = carried[5] = true;
    }
    if (placement.short_scale) {
        next[0] = (*placement.short_scale)[0] / 32768.0;
        next[3] = (*placement.short_scale)[1] / 32768.0;
        carried[0] = carried[3] = true;
    }
    if (placement.short_rotate_skew) {
        next[1] = (*placement.short_rotate_skew)[0] / 32768.0;
        next[2] = (*placement.short_rotate_skew)[1] / 32768.0;
        carried[1] = carried[2] = true;
    }
    if (!fresh) {
        const std::array<const char*, 6> names{"scale", "rotate", "rotate", "scale", "translation",
                                               "translation"};
        std::map<std::string, bool> mattered;
        for (std::size_t i = 0; i < 6; i++) {
            if (carried[i]) continue;
            const double identity = (i == 0 || i == 3) ? 1.0 : 0.0;
            if (state.matrix[i] != identity) mattered[names[i]] = true;
        }
        for (const auto& [name, yes] : mattered)
            counts.resets[std::string("2D update resets a held ") + name]++;
        if (mattered.empty()) counts.resets["2D update resets nothing held"]++;
    }
    state.matrix = next;
}

void ApplyColour(const AfpAnimation::Placement& placement, Effective& state, bool fresh,
                 Counts& counts) {
    std::array<double, 4> multiply{1.0, 1.0, 1.0, 1.0};
    std::array<double, 4> add{0.0, 0.0, 0.0, 0.0};
    bool has_multiply = false;
    bool has_add = false;
    if (placement.multiply_colour) {
        multiply = Scaled(*placement.multiply_colour);
        has_multiply = true;
    }
    if (placement.add_colour) {
        add = Scaled(*placement.add_colour);
        has_add = true;
    }
    if (placement.packed_multiply_colour) {
        multiply = Unpacked(*placement.packed_multiply_colour);
        has_multiply = true;
    }
    if (placement.packed_add_colour) {
        add = Unpacked(*placement.packed_add_colour);
        has_add = true;
    }
    if (!fresh) {
        const bool lost_multiply = !has_multiply && state.multiply != std::array<double, 4>{1.0, 1.0, 1.0, 1.0};
        const bool lost_add = !has_add && state.add != std::array<double, 4>{0.0, 0.0, 0.0, 0.0};
        if (lost_multiply) counts.resets["colour update resets a held multiply"]++;
        if (lost_add) counts.resets["colour update resets a held add"]++;
        if (!lost_multiply && !lost_add) counts.resets["colour update resets nothing held"]++;
    }
    state.multiply = multiply;
    state.add = add;
}

void ReplayClip(const AfpAnimation::Container& clip, Counts& counts) {
    std::map<uint16_t, Effective> live;
    std::map<uint16_t, std::pair<bool, bool>> encodings;
    for (const AfpAnimation::Frame& frame : clip.frames) {
        for (uint32_t i = 0; i < frame.tag_count; i++) {
            const std::size_t index = frame.first_tag + i;
            if (index >= clip.tags.size()) break;
            const AfpAnimation::Tag& tag = clip.tags[index];
            if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) {
                live.erase(remove->depth);
                continue;
            }
            const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement == nullptr) continue;
            const bool update = (placement->flags & 0x1U) != 0;
            const auto found = live.find(placement->depth);
            if (update && found == live.end()) continue;
            const bool fresh = !update;
            if (fresh) live[placement->depth] = Effective{};
            Effective& state = live[placement->depth];
            auto& seen = encodings[placement->depth];
            if (placement->scale) seen.first = true;
            if (placement->short_scale) seen.second = true;
            if (placement->scale && placement->short_scale)
                counts.resets["placement carries both scale encodings"]++;
            if (placement->multiply_colour && placement->packed_multiply_colour)
                counts.resets["placement carries both multiply encodings"]++;
            const bool three_d = (placement->flags & 0x04000000U) != 0;
            if ((placement->flags & 0x4U) != 0 && !three_d) ApplyMatrix(*placement, state, fresh, counts);
            if ((placement->flags & 0x4U) != 0 && three_d && !fresh && !placement->translation &&
                (state.matrix[4] != 0.0 || state.matrix[5] != 0.0)) {
                counts.resets["3D update resets a held translation"]++;
            }
            if ((placement->flags & 0x8U) != 0) ApplyColour(*placement, state, fresh, counts);
        }
    }
    for (const auto& [depth, seen] : encodings) {
        if (seen.first && seen.second) counts.resets["depth uses both scale encodings"]++;
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
    counts.export_order[ExportOrder(animation)]++;
    CountSpriteFrames(animation.root, counts);
    CountControlBits(animation.root, counts);
    ReplayClip(animation.root, counts);
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
            CountControlBits(sprite->container, counts);
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
            ReplayClip(sprite->container, counts);
    }
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
    for (const auto& [what, count] : counts.resets)
        std::cerr << std::format("[nesting] reset, {}: {}\n", what, count);
    for (const auto& [bits, count] : counts.control_bits)
        std::cerr << std::format("[nesting] control bits, {}: {}\n", bits, count);
    for (const auto& [order, count] : counts.export_order)
        std::cerr << std::format("[nesting] export table {}: {}\n", order, count);
    for (const auto& [where, count] : counts.sprite_tag_frame)
        std::cerr << std::format("[nesting] sprite definition {}: {}\n", where, count);
    for (const auto& [level, count] : counts.deepest)
        std::cerr << std::format("[nesting] deepest sprite level {}: {}\n", level, count);

    CHECK(counts.animations > 0);
}
