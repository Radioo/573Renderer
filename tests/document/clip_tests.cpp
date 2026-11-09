#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/outline.h"
#include "formats/afp_animation.h"

#include "sample_package.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

AfpAnimation::Container Frames(std::size_t count) {
    AfpAnimation::Container clip;
    clip.frames.resize(count);
    return clip;
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = SamplePackage::SampleAnimation();
    animation.strings.emplace_back("spinner");

    AfpAnimation::Container spinner = Frames(12);
    spinner.labels.push_back(AfpAnimation::Label{
        .frame = 4, .name = static_cast<AfpAnimation::StringId>(animation.strings.size() - 1)});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 5, .container = spinner}});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 9, .container = Frames(3)}});
    animation.exports.push_back(AfpAnimation::Export{
        .tag = 5, .name = static_cast<AfpAnimation::StringId>(animation.strings.size() - 1)});
    return animation;
}

}

TEST_CASE("The root is the first clip of an animation") {
    const AfpAnimation::Animation animation = Scene();
    const std::vector<Document::ClipSummary> clips = Document::Clips(animation);
    REQUIRE(!clips.empty());
    CHECK(clips.front().id == Document::ClipId{});
    CHECK(clips.front().frame_count == animation.root.frames.size());
}

TEST_CASE("Every sprite of an animation is a clip, in the order it is defined") {
    const AfpAnimation::Animation animation = Scene();
    const std::vector<Document::ClipSummary> clips = Document::Clips(animation);
    REQUIRE(clips.size() == 3);
    CHECK(clips[1].id == Document::ClipId{.sprite = uint16_t{5}});
    CHECK(clips[1].frame_count == 12);
    CHECK(clips[2].id == Document::ClipId{.sprite = uint16_t{9}});
    CHECK(clips[2].frame_count == 3);
}

TEST_CASE("A sprite with an export name is listed by it") {
    const std::vector<Document::ClipSummary> clips = Document::Clips(Scene());
    REQUIRE(clips.size() == 3);
    CHECK(clips[1].name == "spinner");
    CHECK(clips[2].name.empty());
}

TEST_CASE("A clip reads as a label a person recognises") {
    const std::vector<Document::ClipSummary> clips = Document::Clips(Scene());
    REQUIRE(clips.size() == 3);
    CHECK(Document::ClipLabel(clips[0]) == "Root");
    CHECK(Document::ClipLabel(clips[1]) == "spinner (sprite 5)");
    CHECK(Document::ClipLabel(clips[2]) == "Sprite 9");
}

TEST_CASE("A clip is found by its id") {
    AfpAnimation::Animation animation = Scene();
    CHECK(Document::FindClip(animation, Document::ClipId{}) == &animation.root);

    const AfpAnimation::Container* spinner =
        Document::FindClip(animation, Document::ClipId{.sprite = uint16_t{5}});
    REQUIRE(spinner != nullptr);
    CHECK(spinner->frames.size() == 12);
}

TEST_CASE("A sprite that is not there is not found") {
    AfpAnimation::Animation animation = Scene();
    CHECK(Document::FindClip(animation, Document::ClipId{.sprite = uint16_t{77}}) == nullptr);
    const AfpAnimation::Animation& read_only = animation;
    CHECK(Document::FindClip(read_only, Document::ClipId{.sprite = uint16_t{77}}) == nullptr);
}

TEST_CASE("A found clip can be changed in place") {
    AfpAnimation::Animation animation = Scene();
    AfpAnimation::Container* spinner =
        Document::FindClip(animation, Document::ClipId{.sprite = uint16_t{9}});
    REQUIRE(spinner != nullptr);
    spinner->frames.resize(7);

    const std::vector<Document::ClipSummary> clips = Document::Clips(animation);
    REQUIRE(clips.size() == 3);
    CHECK(clips[2].frame_count == 7);
}

TEST_CASE("A clip is described with its own frames, labels and depths") {
    const AfpAnimation::Animation animation = Scene();
    const std::optional<Document::AnimationDetails> spinner =
        Document::DescribeClip(animation, Document::ClipId{.sprite = uint16_t{5}});
    REQUIRE(spinner.has_value());
    if (!spinner) return;
    const Document::AnimationDetails& described = *spinner;
    CHECK(described.frame_count == 12);
    REQUIRE(described.labels.size() == 1);
    CHECK(described.labels.front().name == "spinner");
    CHECK(described.labels.front().frame == 4);

    CHECK_FALSE(
        Document::DescribeClip(animation, Document::ClipId{.sprite = uint16_t{77}}).has_value());
}
