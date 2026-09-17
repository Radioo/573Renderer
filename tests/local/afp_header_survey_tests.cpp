#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using Tally = std::map<std::string, std::size_t>;

struct Counts {
    std::size_t animations = 0;
    Tally export_names;
    Tally export_targets;
    Tally export_counts;
    Tally imports;
    Tally initializers;
    Tally rects;
    Tally stored_forms;
    Tally colours;
    Tally script_labels;
    Tally self_export_places;
    Tally helper_sprites;
    Tally self_sprite;
    std::map<std::string, AfpAnimation::Container> first_helper;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string TargetOf(const AfpAnimation::Animation& animation, uint16_t tag) {
    for (const AfpAnimation::Tag& defined : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&defined.body)) {
            if (sprite->id == tag) return "sprite";
        }
        if (const auto* shape = std::get_if<AfpAnimation::Shape>(&defined.body)) {
            if (shape->id == tag) return "shape";
        }
        if (const auto* image = std::get_if<AfpAnimation::Image>(&defined.body)) {
            if (image->id == tag) return "image";
        }
    }
    for (const AfpAnimation::Import& imported : animation.imports) {
        for (const AfpAnimation::ImportedAsset& asset : imported.assets) {
            if (asset.tag == tag) return "import";
        }
    }
    return "nothing";
}

const AfpAnimation::Container* SpriteContainer(const AfpAnimation::Animation& animation,
                                               uint16_t tag) {
    for (const AfpAnimation::Tag& defined : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&defined.body);
        if (sprite != nullptr && sprite->id == tag) return &sprite->container;
    }
    return nullptr;
}

std::string Shape(const AfpAnimation::Container& container) {
    std::map<std::string, std::size_t> kinds;
    for (const AfpAnimation::Tag& tag : container.tags) {
        const char* kind = std::visit(
            [](const auto& body) {
                using T = std::decay_t<decltype(body)>;
                if (std::is_same_v<T, AfpAnimation::Placement>) return "place";
                if (std::is_same_v<T, AfpAnimation::Remove>) return "remove";
                if (std::is_same_v<T, AfpAnimation::Action>) return "action";
                if (std::is_same_v<T, AfpAnimation::Shape>) return "shape";
                return "other";
            },
            tag.body);
        kinds[kind]++;
    }
    std::string text =
        std::format("{} frames, {} labels:", container.frames.size(), container.labels.size());
    for (const auto& [kind, count] : kinds)
        text += std::format(" {} {}", count, kind);
    return text;
}

AfpAnimation::Container WithoutDefinitions(const AfpAnimation::Container& root) {
    AfpAnimation::Container kept = root;
    kept.tags.clear();
    for (AfpAnimation::Frame& frame : kept.frames) {
        frame.first_tag = 0;
        frame.tag_count = 0;
    }
    for (std::size_t f = 0; f < root.frames.size(); f++) {
        const AfpAnimation::Frame& frame = root.frames[f];
        kept.frames[f].first_tag = static_cast<uint32_t>(kept.tags.size());
        for (uint32_t i = 0; i < frame.tag_count; i++) {
            const AfpAnimation::Tag& tag = root.tags[frame.first_tag + i];
            if (std::holds_alternative<AfpAnimation::Sprite>(tag.body) ||
                std::holds_alternative<AfpAnimation::Shape>(tag.body))
                continue;
            kept.tags.push_back(tag);
            kept.frames[f].tag_count++;
        }
    }
    return kept;
}

std::string RootPlaces(const AfpAnimation::Animation& animation, uint16_t tag) {
    for (const AfpAnimation::Tag& defined : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&defined.body);
        if (placement != nullptr && placement->character == tag) return "placed on the root";
    }
    return "not placed on the root";
}

void CountHelper(const AfpAnimation::Animation& animation, const std::string& name,
                 const AfpAnimation::Container& inside, Counts& counts) {
    for (const AfpAnimation::Tag& tag : inside.tags) {
        const auto* placed = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placed == nullptr) continue;
        counts.helper_sprites[std::format(
            "{} places {} flags {:x} depth {} name '{}'", name,
            placed->character ? TargetOf(animation, *placed->character) : "no character",
            placed->flags, placed->depth,
            placed->name ? Document::StringText(animation, *placed->name) : "")]++;
    }
    auto first = counts.first_helper.try_emplace(name, inside).first;
    counts.helper_sprites[std::format(
        "{} {} ({})", name,
        first->second == inside ? "same as the first" : "differs from the first", Shape(inside))]++;
}

void CountSelf(const AfpAnimation::Animation& animation, const AfpAnimation::Container& inside,
               Counts& counts) {
    counts.self_sprite[inside == WithoutDefinitions(animation.root)
                           ? "same as the root without its definitions"
                           : "differs from the root without its definitions"]++;
    counts.self_sprite[inside.frames.size() == animation.root.frames.size()
                           ? "as many frames as the root"
                           : "a different frame count from the root"]++;
}

void CountExports(const AfpAnimation::Animation& animation, Counts& counts) {
    const std::string movie = Document::StringText(animation, animation.name);
    counts.export_counts[std::to_string(animation.exports.size())]++;
    for (const AfpAnimation::Export& exported : animation.exports) {
        const std::string name = Document::StringText(animation, exported.name);
        const bool self = name == movie;
        counts.export_names[self ? "(the movie's own name)" : name]++;
        counts.export_targets[std::format("{} -> {}", self ? "self" : name,
                                          TargetOf(animation, exported.tag))]++;
        if (self) counts.self_export_places[RootPlaces(animation, exported.tag)]++;
        const AfpAnimation::Container* inside = SpriteContainer(animation, exported.tag);
        if (inside == nullptr) continue;
        if (self) CountSelf(animation, *inside, counts);
        if (name == "aeplibset" || name == "aep_mask_dummy")
            CountHelper(animation, name, *inside, counts);
    }
}

std::string ImportsText(const AfpAnimation::Animation& animation) {
    std::string imports;
    for (const AfpAnimation::Import& imported : animation.imports) {
        imports += Document::StringText(animation, imported.movie) + "[";
        for (const AfpAnimation::ImportedAsset& asset : imported.assets) {
            imports +=
                std::format("{}@{} ", Document::StringText(animation, asset.name), asset.tag);
        }
        imports += "]";
    }
    return imports;
}

std::string InitializersText(const AfpAnimation::Animation& animation) {
    if (!animation.import_initializers) return "none";
    std::string text = std::format("word {}:", animation.import_initializers->leading_word);
    for (const AfpAnimation::ImportInitializer& entry : animation.import_initializers->entries)
        text += std::format(" ({}, {})", entry.tag, entry.frame);
    return text;
}

void Count(const AfpAnimation::Animation& animation, Counts& counts) {
    counts.animations++;
    CountExports(animation, counts);
    counts.imports[ImportsText(animation)]++;
    counts.initializers[InitializersText(animation)]++;
    counts.rects[std::format("{},{},{},{}", animation.rect[0], animation.rect[1], animation.rect[2],
                             animation.rect[3])]++;
    counts.stored_forms[std::format("scrambled {} swapped {}",
                                    animation.stored_form.strings_scrambled,
                                    animation.stored_form.background_colour_swapped)]++;
    counts.colours[std::format("{},{},{},{}", animation.background_colour[0],
                               animation.background_colour[1], animation.background_colour[2],
                               animation.background_colour[3])]++;
    counts
        .script_labels[animation.root.script_labels ? "root has script labels" : "root has none"]++;
}

std::string TagText(const AfpAnimation::Tag& tag) {
    if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
        return std::format("sprite {} of {}", sprite->id, sprite->container.frames.size());
    if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body))
        return std::format("shape {} word {}", shape->id, shape->unread_word);
    return "other";
}

std::vector<std::string> ExportTexts(const AfpAnimation::Animation& animation) {
    std::vector<std::string> texts;
    texts.reserve(animation.exports.size());
    for (const AfpAnimation::Export& exported : animation.exports) {
        texts.push_back(
            std::format("{} {}", Document::StringText(animation, exported.name), exported.tag));
    }
    return texts;
}

void CountNodes(const Document::File& file, const std::vector<Document::Node>& nodes,
                Counts& counts) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) {
            const auto animation = file.ReadAnimation(node.path);
            if (animation) Count(*animation, counts);
        }
        CountNodes(file, node.children, counts);
    }
}

void Print(const std::string& title, const Tally& tally) {
    std::vector<std::pair<std::string, std::size_t>> sorted(tally.begin(), tally.end());
    std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cerr << std::format("[header] {} ({} kinds)\n", title, sorted.size());
    for (std::size_t i = 0; i < sorted.size() && i < 12; i++)
        std::cerr << std::format("[header]   {}: {}\n", sorted[i].first, sorted[i].second);
}

}

TEST_CASE("The title animation opens with its helper definitions") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto file = Document::File::Open(ReadAll(dir + "/data/graphic/1/title.ifs"));
    REQUIRE(file.has_value());
    if (!file) return;
    std::string path;
    for (const Document::Node& node : file->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == "title") path = child.path;
        }
    }
    const auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return;
    const std::vector<std::string> exports = ExportTexts(*animation);
    REQUIRE(exports.size() >= 3);
    CHECK(exports[0] == "aep_mask_dummy 6");
    CHECK(exports[1] == "aeplibset 3");
    CHECK(exports[2] == "title 335");
    CHECK(std::ranges::is_sorted(exports));
    REQUIRE(animation->root.tags.size() >= 3);
    CHECK(TagText(animation->root.tags[0]) == "sprite 3 of 1");
    CHECK(TagText(animation->root.tags[1]) == "shape 5 word 0");
    CHECK(TagText(animation->root.tags[2]) == "sprite 6 of 1");
    CHECK(animation->root.frames[0].first_tag == 0);
}

TEST_CASE("What a shipped animation header holds besides its content") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        auto file = Document::File::Open(ReadAll(entry.path()));
        if (!file) continue;
        CountNodes(*file, file->Nodes(), counts);
    }
    std::cerr << std::format("[header] {} animations\n", counts.animations);
    Print("export counts", counts.export_counts);
    Print("export names", counts.export_names);
    Print("export targets", counts.export_targets);
    Print("self export", counts.self_export_places);
    Print("helper sprites", counts.helper_sprites);
    Print("self sprite", counts.self_sprite);
    Print("imports", counts.imports);
    Print("import initializers", counts.initializers);
    Print("stage rects", counts.rects);
    Print("stored forms", counts.stored_forms);
    Print("background colours", counts.colours);
    Print("script labels", counts.script_labels);
    CHECK(counts.animations > 0);
}
