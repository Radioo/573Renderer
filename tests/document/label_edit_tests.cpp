#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/animation_strings.h"
#include "document/label_edit.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kLinearLabelLookup = 0x8;

AfpAnimation::Animation Clip() {
    AfpAnimation::Animation animation;
    animation.strings = {"", "loop"};
    animation.root.frames = {AfpAnimation::Frame{}, AfpAnimation::Frame{}, AfpAnimation::Frame{},
                             AfpAnimation::Frame{}};
    animation.root.labels = {AfpAnimation::Label{.frame = 2, .name = 1}};
    return animation;
}

std::vector<std::string> LabelNames(const AfpAnimation::Animation& animation) {
    std::vector<std::string> names;
    names.reserve(animation.root.labels.size());
    for (const AfpAnimation::Label& label : animation.root.labels)
        names.push_back(Document::StringText(animation, label.name));
    return names;
}

}

TEST_CASE("A label is added at a frame and sorted the way the lookup reads it") {
    AfpAnimation::Animation animation = Clip();
    REQUIRE(Document::AddLabel(animation, Document::ClipId{}, "start", 0).has_value());
    REQUIRE(Document::AddLabel(animation, Document::ClipId{}, "end", 3).has_value());
    CHECK(LabelNames(animation) == std::vector<std::string>{"end", "loop", "start"});

    AfpAnimation::Animation linear = Clip();
    linear.flags = kLinearLabelLookup;
    REQUIRE(Document::AddLabel(linear, Document::ClipId{}, "start", 0).has_value());
    REQUIRE(Document::AddLabel(linear, Document::ClipId{}, "end", 3).has_value());
    CHECK(LabelNames(linear) == std::vector<std::string>{"start", "loop", "end"});
}

TEST_CASE("A label a clip cannot hold is refused") {
    AfpAnimation::Animation animation = Clip();
    CHECK_FALSE(Document::AddLabel(animation, Document::ClipId{}, "loop", 1).has_value());
    CHECK_FALSE(Document::AddLabel(animation, Document::ClipId{}, "", 1).has_value());
    CHECK_FALSE(Document::AddLabel(animation, Document::ClipId{}, "late", 4).has_value());
    CHECK(animation.root.labels.size() == 1);
}

TEST_CASE("A label is renamed, moved and removed by the name the game looks it up by") {
    AfpAnimation::Animation animation = Clip();
    REQUIRE(Document::MoveLabel(animation, Document::ClipId{}, "loop", 1).has_value());
    CHECK(animation.root.labels.front().frame == 1);
    CHECK_FALSE(Document::MoveLabel(animation, Document::ClipId{}, "loop", 9).has_value());
    CHECK_FALSE(Document::MoveLabel(animation, Document::ClipId{}, "nothing", 1).has_value());

    REQUIRE(Document::RenameLabel(animation, Document::ClipId{}, "loop", "again").has_value());
    CHECK(LabelNames(animation) == std::vector<std::string>{"again"});
    CHECK(std::ranges::find(animation.strings, "loop") == animation.strings.end());
    CHECK_FALSE(Document::RenameLabel(animation, Document::ClipId{}, "loop", "other").has_value());

    REQUIRE(Document::RemoveLabel(animation, Document::ClipId{}, "again").has_value());
    CHECK(animation.root.labels.empty());
    CHECK(animation.strings.size() == 1);
    CHECK(animation.strings.front().empty());
    CHECK_FALSE(Document::RemoveLabel(animation, Document::ClipId{}, "again").has_value());
}

TEST_CASE("Compacting keeps every string the animation still refers to") {
    AfpAnimation::Animation animation;
    animation.strings = {"",      "unused", "movie", "asset", "instance",
                         "class", "image",  "label", "script"};
    animation.name = 2;
    animation.exports = {AfpAnimation::Export{.tag = 1, .name = 2}};
    animation.imports = {AfpAnimation::Import{
        .movie = 2, .assets = {AfpAnimation::ImportedAsset{.tag = 1, .name = 3}}}};
    animation.root.frames = {AfpAnimation::Frame{}};
    animation.root.labels = {AfpAnimation::Label{.frame = 0, .name = 7}};
    animation.root.script_labels =
        std::vector<AfpAnimation::Label>{AfpAnimation::Label{.frame = 0, .name = 8}};

    AfpAnimation::Placement placement;
    placement.name = 4;
    placement.class_name = 5;
    animation.root.tags.push_back(AfpAnimation::Tag{placement});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Image{.flags = 0, .id = 1, .name = 6}});

    Document::CompactStrings(animation);
    CHECK(animation.strings.size() == 8);
    CHECK(animation.strings.front().empty());
    CHECK(std::ranges::find(animation.strings, "unused") == animation.strings.end());
    CHECK(Document::StringText(animation, animation.name) == "movie");
    CHECK(Document::StringText(animation, animation.exports.front().name) == "movie");
    CHECK(Document::StringText(animation, animation.imports.front().assets.front().name) ==
          "asset");
    CHECK(Document::StringText(animation, animation.root.labels.front().name) == "label");
    CHECK(Document::StringText(animation, animation.root.script_labels->front().name) == "script");
}

TEST_CASE("Interning reuses a string the animation already holds") {
    AfpAnimation::Animation animation;
    animation.strings = {"", "loop"};
    CHECK(Document::InternString(animation, "loop") == 1);
    CHECK(Document::InternString(animation, "other") == 2);
    CHECK(animation.strings.size() == 3);
}
