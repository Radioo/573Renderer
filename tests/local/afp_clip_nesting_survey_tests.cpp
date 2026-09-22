#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "document/placement_effect.h"
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
#include <optional>
#include <set>
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
    std::map<std::string, std::size_t> characters;
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

struct Carried {
    bool scale = false;
    bool rotate = false;
    bool translation = false;
    bool multiply = false;
    bool add = false;
};

Carried CarriedParts(const AfpAnimation::Placement& placement) {
    return Carried{
        .scale = placement.scale || placement.short_scale,
        .rotate = placement.rotate_skew || placement.short_rotate_skew,
        .translation = placement.translation.has_value(),
        .multiply = placement.multiply_colour || placement.packed_multiply_colour,
        .add = placement.add_colour || placement.packed_add_colour,
    };
}

bool Moved(const Document::AppliedState& before, const Document::AppliedState& after,
           std::size_t first, std::size_t second) {
    return before.matrix.at(first) != after.matrix.at(first) ||
           before.matrix.at(second) != after.matrix.at(second);
}

void CountMatrixResets(const AfpAnimation::Placement& placement, const Carried& carried,
                       const Document::AppliedState& before, const Document::AppliedState& after,
                       Counts& counts) {
    if ((placement.flags & 0x04000000U) != 0) {
        if (!carried.translation && Moved(before, after, 4, 5))
            counts.resets["3D update resets a held translation"]++;
        return;
    }
    const bool scale = !carried.scale && Moved(before, after, 0, 3);
    const bool rotate = !carried.rotate && Moved(before, after, 1, 2);
    const bool translation = !carried.translation && Moved(before, after, 4, 5);
    if (scale) counts.resets["2D update resets a held scale"]++;
    if (rotate) counts.resets["2D update resets a held rotate"]++;
    if (translation) counts.resets["2D update resets a held translation"]++;
    if (!scale && !rotate && !translation) counts.resets["2D update resets nothing held"]++;
}

void CountColourResets(const Carried& carried, const Document::AppliedState& before,
                       const Document::AppliedState& after, Counts& counts) {
    const bool multiply = !carried.multiply && before.multiply != after.multiply;
    const bool add = !carried.add && before.add != after.add;
    if (multiply) counts.resets["colour update resets a held multiply"]++;
    if (add) counts.resets["colour update resets a held add"]++;
    if (!multiply && !add) counts.resets["colour update resets nothing held"]++;
}

void NoteEncodings(const AfpAnimation::Placement& placement, std::pair<bool, bool>& seen,
                   Counts& counts) {
    if (placement.scale) seen.first = true;
    if (placement.short_scale) seen.second = true;
    if (placement.scale && placement.short_scale)
        counts.resets["placement carries both scale encodings"]++;
    if (placement.multiply_colour && placement.packed_multiply_colour)
        counts.resets["placement carries both multiply encodings"]++;
}

struct Replay {
    std::map<uint16_t, Document::AppliedState> live;
    std::map<uint16_t, std::pair<bool, bool>> encodings;
};

void ReplayTag(const AfpAnimation::Tag& tag, Replay& replay, Counts& counts) {
    if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) {
        replay.live.erase(remove->depth);
        return;
    }
    const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
    if (placement == nullptr) return;
    const bool update = (placement->flags & 0x1U) != 0;
    if (update && !replay.live.contains(placement->depth)) return;
    if (!update) replay.live[placement->depth] = Document::AppliedState{};
    NoteEncodings(*placement, replay.encodings[placement->depth], counts);
    Document::AppliedState& state = replay.live[placement->depth];
    const Document::AppliedState before = state;
    Document::ApplyPlacement(state, *placement);
    if (!update) return;
    const Carried carried = CarriedParts(*placement);
    if ((placement->flags & 0x4U) != 0)
        CountMatrixResets(*placement, carried, before, state, counts);
    if ((placement->flags & 0x8U) != 0) CountColourResets(carried, before, state, counts);
}

void ReplayClip(const AfpAnimation::Container& clip, Counts& counts) {
    Replay replay;
    for (const AfpAnimation::Frame& frame : clip.frames) {
        for (uint32_t i = 0; i < frame.tag_count; i++) {
            const std::size_t index = frame.first_tag + i;
            if (index >= clip.tags.size()) break;
            ReplayTag(clip.tags[index], replay, counts);
        }
    }
    for (const auto& [depth, seen] : replay.encodings) {
        if (seen.first && seen.second) counts.resets["depth uses both scale encodings"]++;
    }
}

std::optional<std::pair<uint16_t, std::string>> Defined(const AfpAnimation::Tag& tag) {
    if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
        return std::pair<uint16_t, std::string>{sprite->id, "sprite"};
    if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body))
        return std::pair<uint16_t, std::string>{image->id, "image"};
    if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body))
        return std::pair<uint16_t, std::string>{shape->id, "shape"};
    return std::nullopt;
}

std::map<uint16_t, std::string> DefinedKinds(const AfpAnimation::Animation& animation,
                                             Counts& counts) {
    std::map<uint16_t, std::string> kinds;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body))
            counts.characters[std::format("image flags {:#x}", image->flags)]++;
        const auto defined = Defined(tag);
        if (!defined) continue;
        counts.characters["defined " + defined->second]++;
        if (kinds.contains(defined->first)) counts.characters["id defined twice"]++;
        kinds[defined->first] = defined->second;
    }
    for (const AfpAnimation::Import& imported : animation.imports) {
        for (const AfpAnimation::ImportedAsset& asset : imported.assets)
            kinds.try_emplace(asset.tag, "import");
    }
    return kinds;
}

void CountPlaced(const AfpAnimation::Container& clip, const std::map<uint16_t, std::string>& kinds,
                 Counts& counts) {
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || !placement->character) continue;
        const auto kind = kinds.find(*placement->character);
        counts.characters["places " +
                          (kind == kinds.end() ? std::string("unknown id") : kind->second)]++;
    }
}

void CountCharacters(const AfpAnimation::Animation& animation, Counts& counts) {
    const std::map<uint16_t, std::string> kinds = DefinedKinds(animation, counts);
    CountPlaced(animation.root, kinds, counts);
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
            CountPlaced(sprite->container, kinds, counts);
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
    CountCharacters(animation, counts);
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

void TextureNames(const std::vector<Document::Node>& nodes, std::set<std::string>& names) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Texture) names.insert(node.name);
        TextureNames(node.children, names);
    }
}

void CountImageNames(const AfpAnimation::Animation& animation,
                     const std::set<std::string>& textures, Counts& counts) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* image = std::get_if<AfpAnimation::Image>(&tag.body);
        if (image == nullptr) continue;
        const std::string name =
            image->name < animation.strings.size() ? animation.strings[image->name] : std::string();
        std::string dashed = name;
        std::ranges::replace(dashed, '_', '-');
        std::string underscored = name;
        std::ranges::replace(underscored, '-', '_');
        std::string where = "in no texture list";
        if (textures.contains(name)) {
            where = "names a texture as written";
        } else if (textures.contains(dashed)) {
            where = "names a texture once _ becomes -";
        } else if (textures.contains(underscored)) {
            where = "names a texture once - becomes _";
        }
        counts.characters["image " + where +
                          (name.find('_') != std::string::npos ? ", has _" : ", no _")]++;
    }
}

void CountNodes(const Document::File& file, const std::vector<Document::Node>& nodes,
                Counts& counts) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) {
            const auto animation = file.ReadAnimation(node.path);
            if (animation) {
                CountAnimation(*animation, counts);
                std::set<std::string> textures;
                TextureNames(file.Nodes(), textures);
                CountImageNames(*animation, textures, counts);
            }
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
    for (const auto& [what, count] : counts.characters)
        std::cerr << std::format("[nesting] characters, {}: {}\n", what, count);
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
