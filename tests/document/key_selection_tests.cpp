#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/keyframes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using Document::KeyRef;

KeyRef Ref(const std::string& property, uint32_t frame) {
    return KeyRef{.property = property, .frame = frame};
}

Document::Keyframe Key(uint32_t frame, std::vector<int64_t> value,
                       Document::Ease ease = Document::Ease::Linear) {
    return Document::Keyframe{
        .frame = frame, .value = std::move(value), .ease = ease, .bezier = {}};
}

Document::AuthoredDepth Depth() {
    Document::Track moving{.property = "Translation",
                           .keys = {Key(0, {0, 0}), Key(4, {40, 0}), Key(8, {80, 0})}};
    Document::Track tinted{.property = "Multiply colour",
                           .keys = {Key(0, {255, 255, 255, 255}), Key(6, {0, 255, 255, 255})}};
    return Document::AuthoredDepth{.animation = "afp/scene",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 12,
                                   .tracks = {moving, tinted},
                                   .script = std::nullopt,
                                   .clip = {}};
}

std::vector<uint32_t> Frames(const Document::AuthoredDepth& depth, const std::string& property) {
    const auto track = std::ranges::find(depth.tracks, property, &Document::Track::property);
    REQUIRE(track != depth.tracks.end());
    std::vector<uint32_t> frames;
    for (const Document::Keyframe& key : track->keys)
        frames.push_back(key.frame);
    return frames;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("Every keyframe of a depth can be selected") {
    CHECK(Document::AllKeys(Depth()) ==
          std::vector<KeyRef>{Ref("Translation", 0), Ref("Translation", 4), Ref("Translation", 8),
                              Ref("Multiply colour", 0), Ref("Multiply colour", 6)});
}

TEST_CASE("Copied keyframes keep their spacing from the earliest one") {
    const auto clip = Document::CopyKeys(
        Depth(), {Ref("Multiply colour", 6), Ref("Translation", 4), Ref("Translation", 8)});
    INFO(Error(clip));
    REQUIRE(clip.has_value());
    if (!clip) return;
    REQUIRE(clip->tracks.size() == 2);
    CHECK(clip->tracks[0].property == "Translation");
    CHECK(clip->tracks[0].keys ==
          std::vector<Document::Keyframe>{Key(0, {40, 0}), Key(4, {80, 0})});
    CHECK(clip->tracks[1].keys == std::vector<Document::Keyframe>{Key(2, {0, 255, 255, 255})});
}

TEST_CASE("Copying needs a selection of keyframes that exist") {
    CHECK_FALSE(Document::CopyKeys(Depth(), {}).has_value());
    CHECK_FALSE(Document::CopyKeys(Depth(), {Ref("Translation", 3)}).has_value());
    CHECK_FALSE(Document::CopyKeys(Depth(), {Ref("Scale", 0)}).has_value());
}

TEST_CASE("Pasting places the keyframes from a frame and replaces what is already there") {
    Document::AuthoredDepth depth = Depth();
    const auto clip = Document::CopyKeys(depth, {Ref("Translation", 0), Ref("Translation", 4)});
    REQUIRE(clip.has_value());
    if (!clip) return;
    const auto pasted = Document::PasteKeys(depth, {}, *clip, 8);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());
    CHECK(*pasted == std::vector<KeyRef>{Ref("Translation", 8), Ref("Translation", 12)});
    CHECK(Frames(depth, "Translation") == std::vector<uint32_t>{0, 4, 8, 12});
    CHECK(depth.tracks[0].keys[2].value == std::vector<int64_t>{0, 0});
}

TEST_CASE("Pasting starts a track the depth does not animate yet") {
    Document::AuthoredDepth source = Depth();
    source.tracks.push_back(
        Document::Track{.property = "Scale", .keys = {Key(2, {2048, 2048}, Document::Ease::Hold)}});
    const auto clip = Document::CopyKeys(source, {Ref("Scale", 2)});
    REQUIRE(clip.has_value());
    if (!clip) return;
    Document::AuthoredDepth target = Depth();
    const auto pasted = Document::PasteKeys(target, {}, *clip, 5);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());
    CHECK(Frames(target, "Scale") == std::vector<uint32_t>{0, 5});
}

TEST_CASE("A paste that does not fit changes nothing") {
    Document::AuthoredDepth depth = Depth();
    const auto clip = Document::CopyKeys(depth, {Ref("Translation", 0), Ref("Translation", 8)});
    REQUIRE(clip.has_value());
    if (!clip) return;
    const Document::AuthoredDepth before = depth;
    CHECK_FALSE(Document::PasteKeys(depth, {}, *clip, 6).has_value());
    CHECK(depth == before);
    CHECK_FALSE(Document::PasteKeys(depth, {}, Document::KeyClip{}, 0).has_value());

    const Document::KeyClip wrong{
        .tracks = {Document::Track{.property = "Translation", .keys = {Key(0, {1, 2, 3})}}}};
    CHECK_FALSE(Document::PasteKeys(depth, {}, wrong, 4).has_value());
    CHECK(depth == before);
}

TEST_CASE("Selected keyframes are removed together, or not at all") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(Document::RemoveKeys(depth, {Ref("Translation", 4), Ref("Multiply colour", 6)})
                .has_value());
    CHECK(Frames(depth, "Translation") == std::vector<uint32_t>{0, 8});
    CHECK(Frames(depth, "Multiply colour") == std::vector<uint32_t>{0});

    const Document::AuthoredDepth before = depth;
    CHECK_FALSE(Document::RemoveKeys(depth, {Ref("Translation", 8), Ref("Multiply colour", 0)})
                    .has_value());
    CHECK(depth == before);
    CHECK_FALSE(Document::RemoveKeys(depth, {}).has_value());
}

TEST_CASE("Selected keyframes move together and keep their order") {
    Document::AuthoredDepth depth = Depth();
    const auto shifted = Document::ShiftKeys(
        depth, {Ref("Translation", 4), Ref("Translation", 8), Ref("Multiply colour", 6)}, 3);
    INFO(Error(shifted));
    REQUIRE(shifted.has_value());
    CHECK(Frames(depth, "Translation") == std::vector<uint32_t>{0, 7, 11});
    CHECK(Frames(depth, "Multiply colour") == std::vector<uint32_t>{0, 9});
    if (!shifted) return;
    CHECK(std::ranges::is_permutation(*shifted, std::vector<KeyRef>{Ref("Translation", 7),
                                                                    Ref("Translation", 11),
                                                                    Ref("Multiply colour", 9)}));
}

TEST_CASE("Keyframes cannot move onto another keyframe or outside the range") {
    Document::AuthoredDepth depth = Depth();
    const Document::AuthoredDepth before = depth;
    CHECK_FALSE(Document::ShiftKeys(depth, {Ref("Translation", 4)}, 4).has_value());
    CHECK_FALSE(Document::ShiftKeys(depth, {Ref("Translation", 8)}, 5).has_value());
    CHECK_FALSE(Document::ShiftKeys(depth, {Ref("Translation", 0)}, -1).has_value());
    CHECK_FALSE(Document::ShiftKeys(depth, {Ref("Translation", 5)}, 1).has_value());
    CHECK_FALSE(Document::ShiftKeys(depth, {Ref("Scale", 0)}, 1).has_value());
    CHECK(depth == before);
    REQUIRE(
        Document::ShiftKeys(depth, {Ref("Translation", 4), Ref("Translation", 8)}, 4).has_value());
    CHECK(Frames(depth, "Translation") == std::vector<uint32_t>{0, 8, 12});
}

TEST_CASE("Selected keyframes take one ease together, or none does") {
    Document::AuthoredDepth depth = Depth();
    const Document::Bezier curve{.x1 = 0.25, .y1 = 0.0, .x2 = 0.75, .y2 = 1.0};
    REQUIRE(Document::SetKeysEase(depth, {Ref("Translation", 0), Ref("Multiply colour", 0)},
                                  Document::Ease::Bezier, curve)
                .has_value());
    CHECK(depth.tracks[0].keys[0].ease == Document::Ease::Bezier);
    CHECK(depth.tracks[0].keys[0].bezier == curve);
    CHECK(depth.tracks[1].keys[0].ease == Document::Ease::Bezier);
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Linear);

    const Document::AuthoredDepth before = depth;
    CHECK_FALSE(Document::SetKeysEase(depth, {Ref("Translation", 4), Ref("Translation", 5)},
                                      Document::Ease::Hold, {})
                    .has_value());
    CHECK_FALSE(Document::SetKeysEase(depth, {Ref("Translation", 4)}, Document::Ease::Bezier,
                                      {.x1 = 2.0, .y1 = 0.0, .x2 = 1.0, .y2 = 1.0})
                    .has_value());
    CHECK(depth == before);
}

TEST_CASE("A pasted filter keyframe replaces one that holds a different number of filters") {
    Document::AuthoredDepth depth = Depth();
    depth.tracks.push_back(
        Document::Track{.property = "Filters",
                        .keys = {Key(0, {0}, Document::Ease::Hold),
                                 Key(4, {1, 0, 6, 0, 0, 0}, Document::Ease::Hold)}});
    const auto clip = Document::CopyKeys(depth, {Ref("Filters", 0)});
    REQUIRE(clip.has_value());
    if (!clip) return;
    const auto pasted = Document::PasteKeys(depth, {}, *clip, 4);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());
    CHECK(depth.tracks.back().keys[1].value == std::vector<int64_t>{0});
}

TEST_CASE("Time-reversing keyframes plays their stretch backwards frame by frame") {
    Document::AuthoredDepth depth = Depth();
    depth.tracks[0].keys[0].ease = Document::Ease::Bezier;
    depth.tracks[0].keys[0].bezier = {.x1 = 0.5, .y1 = 0.0, .x2 = 0.75, .y2 = 0.25};
    depth.tracks[0].keys[2].ease = Document::Ease::Hold;
    const Document::Track before = depth.tracks[0];
    const auto reversed = Document::ReverseKeys(
        depth, {Ref("Translation", 8), Ref("Translation", 0), Ref("Translation", 4)});
    INFO(Error(reversed));
    REQUIRE(reversed.has_value());
    CHECK(*reversed ==
          std::vector<KeyRef>{Ref("Translation", 0), Ref("Translation", 8), Ref("Translation", 4)});
    for (uint32_t frame = 0; frame <= 8; frame++) {
        INFO(frame);
        const std::vector<int64_t> now = Document::SampleTrack(depth.tracks[0], frame);
        const std::vector<int64_t> then = Document::SampleTrack(before, 8 - frame);
        REQUIRE(now.size() == then.size());
        for (std::size_t i = 0; i < now.size(); i++)
            CHECK(std::abs(now[i] - then[i]) <= 1);
    }
    CHECK(depth.tracks[0].keys[0].ease == Document::Ease::Linear);
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Bezier);
    CHECK(depth.tracks[0].keys[1].bezier ==
          Document::Bezier{.x1 = 0.25, .y1 = 0.75, .x2 = 0.5, .y2 = 1.0});
    CHECK(depth.tracks[0].keys[2].ease == Document::Ease::Hold);
    CHECK(depth.tracks[1] == Depth().tracks[1]);
}

TEST_CASE("Time-reversing keeps the reversed stretch inside the selected keys' frames") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(
        Document::ReverseKeys(depth, {Ref("Translation", 4), Ref("Translation", 8)}).has_value());
    CHECK(Frames(depth, "Translation") == std::vector<uint32_t>{0, 4, 8});
    CHECK(depth.tracks[0].keys[1].value == std::vector<int64_t>{80, 0});
    CHECK(depth.tracks[0].keys[2].value == std::vector<int64_t>{40, 0});
    CHECK(depth.tracks[0].keys[0].value == std::vector<int64_t>{0, 0});
}

TEST_CASE("Time-reversing is refused, leaving the depth alone, when it cannot mirror the keys") {
    Document::AuthoredDepth depth = Depth();
    const Document::AuthoredDepth before = depth;
    CHECK_FALSE(Document::ReverseKeys(depth, {}).has_value());
    CHECK_FALSE(Document::ReverseKeys(depth, {Ref("Translation", 4)}).has_value());
    const auto between =
        Document::ReverseKeys(depth, {Ref("Translation", 0), Ref("Translation", 8)});
    REQUIRE_FALSE(between.has_value());
    CHECK(between.error().find("frame 4") != std::string::npos);
    CHECK_FALSE(Document::ReverseKeys(
                    depth, {Ref("Translation", 4), Ref("Translation", 8), Ref("Translation", 9)})
                    .has_value());
    CHECK(depth == before);
    const auto lone = Document::ReverseKeys(
        depth, {Ref("Multiply colour", 6), Ref("Translation", 0), Ref("Translation", 4)});
    REQUIRE(lone.has_value());
    CHECK(*lone == std::vector<KeyRef>{Ref("Multiply colour", 6), Ref("Translation", 4),
                                       Ref("Translation", 0)});
    CHECK(depth.tracks[1] == before.tracks[1]);
}

TEST_CASE("The keyframe that ends up last keeps the ease it leaves the stretch with") {
    Document::AuthoredDepth depth = Depth();
    depth.tracks[0].keys[1].ease = Document::Ease::Bezier;
    depth.tracks[0].keys[1].bezier = {.x1 = 0.5, .y1 = 0.0, .x2 = 0.75, .y2 = 0.25};
    REQUIRE(
        Document::ReverseKeys(depth, {Ref("Translation", 0), Ref("Translation", 4)}).has_value());
    CHECK(depth.tracks[0].keys[1].value == std::vector<int64_t>{0, 0});
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Bezier);
    CHECK(depth.tracks[0].keys[1].bezier ==
          Document::Bezier{.x1 = 0.5, .y1 = 0.0, .x2 = 0.75, .y2 = 0.25});
    CHECK(depth.tracks[0].keys[0].ease == Document::Ease::Linear);
}

TEST_CASE("Easy ease on both sides of a stretch gives it the Ease preset") {
    Document::AuthoredDepth depth = Depth();
    depth.tracks[0].keys[0].ease = Document::Ease::Hold;
    REQUIRE(Document::EasyEaseKeys(depth, {Ref("Translation", 0), Ref("Translation", 4)},
                                   Document::EasySide::Both)
                .has_value());
    CHECK(depth.tracks[0].keys[0].ease == Document::Ease::Bezier);
    CHECK(depth.tracks[0].keys[0].bezier == Document::EasePresets().front().bezier);
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Bezier);
    CHECK(depth.tracks[0].keys[1].bezier ==
          Document::Bezier{.x1 = 1.0 / 3.0, .y1 = 0.0, .x2 = 2.0 / 3.0, .y2 = 2.0 / 3.0});
    CHECK(depth.tracks[0].keys[2].ease == Document::Ease::Linear);
    CHECK(depth.tracks[1] == Depth().tracks[1]);
}

TEST_CASE("Easy ease in and out touch only their side and keep the other") {
    Document::AuthoredDepth depth = Depth();
    depth.tracks[0].keys[0].ease = Document::Ease::Bezier;
    depth.tracks[0].keys[0].bezier = {.x1 = 0.25, .y1 = 0.5, .x2 = 0.75, .y2 = 0.5};
    REQUIRE(
        Document::EasyEaseKeys(depth, {Ref("Translation", 4)}, Document::EasySide::In).has_value());
    CHECK(depth.tracks[0].keys[0].bezier ==
          Document::Bezier{.x1 = 0.25, .y1 = 0.5, .x2 = 2.0 / 3.0, .y2 = 1.0});
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Linear);
    REQUIRE(Document::EasyEaseKeys(depth, {Ref("Translation", 4)}, Document::EasySide::Out)
                .has_value());
    CHECK(depth.tracks[0].keys[1].bezier ==
          Document::Bezier{.x1 = 1.0 / 3.0, .y1 = 0.0, .x2 = 2.0 / 3.0, .y2 = 2.0 / 3.0});
    CHECK(depth.tracks[0].keys[0].bezier ==
          Document::Bezier{.x1 = 0.25, .y1 = 0.5, .x2 = 2.0 / 3.0, .y2 = 1.0});
}

TEST_CASE("Easy ease is refused, leaving the depth alone, when there is nothing to ease") {
    Document::AuthoredDepth depth = Depth();
    depth.tracks.push_back(Document::Track{
        .property = "Blend", .keys = {Key(0, {1}, Document::Ease::Hold), Key(4, {2})}});
    const Document::AuthoredDepth before = depth;
    CHECK_FALSE(Document::EasyEaseKeys(depth, {}, Document::EasySide::Both).has_value());
    CHECK_FALSE(Document::EasyEaseKeys(depth, {Ref("Translation", 8)}, Document::EasySide::Out)
                    .has_value());
    CHECK_FALSE(
        Document::EasyEaseKeys(depth, {Ref("Translation", 0)}, Document::EasySide::In).has_value());
    CHECK_FALSE(Document::EasyEaseKeys(depth, {Ref("Translation", 5)}, Document::EasySide::Both)
                    .has_value());
    const auto stepped = Document::EasyEaseKeys(depth, {Ref("Blend", 0)}, Document::EasySide::Out);
    REQUIRE_FALSE(stepped.has_value());
    CHECK(stepped.error().find("only holds") != std::string::npos);
    CHECK(depth == before);
}
