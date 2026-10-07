#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/key_simplify.h"
#include "document/keyframes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using Document::KeyRef;

Document::Keyframe Held(uint32_t frame, std::vector<int64_t> value) {
    return Document::Keyframe{
        .frame = frame, .value = std::move(value), .ease = Document::Ease::Hold, .bezier = {}};
}

Document::Track Dense(const std::string& property, uint32_t last,
                      const std::function<int64_t(uint32_t)>& value) {
    Document::Track track{.property = property, .keys = {}};
    for (uint32_t frame = 0; frame <= last; frame++)
        track.keys.push_back(Held(frame, {value(frame), 0}));
    return track;
}

Document::AuthoredDepth Depth(std::vector<Document::Track> tracks) {
    return Document::AuthoredDepth{.animation = "afp/scene",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 20,
                                   .tracks = std::move(tracks),
                                   .script = std::nullopt,
                                   .clip = {}};
}

KeyRef Ref(const std::string& property, uint32_t frame) {
    return KeyRef{.property = property, .frame = frame};
}

std::vector<KeyRef> Every(const Document::Track& track) {
    std::vector<KeyRef> keys;
    keys.reserve(track.keys.size());
    for (const Document::Keyframe& key : track.keys)
        keys.push_back(Ref(track.property, key.frame));
    return keys;
}

std::vector<uint32_t> Frames(const Document::Track& track) {
    std::vector<uint32_t> frames;
    frames.reserve(track.keys.size());
    for (const Document::Keyframe& key : track.keys)
        frames.push_back(key.frame);
    return frames;
}

int64_t WorstGap(const Document::Track& a, const Document::Track& b, uint32_t last) {
    int64_t worst = 0;
    for (uint32_t frame = 0; frame <= last; frame++) {
        const std::vector<int64_t> left = Document::SampleTrack(a, frame);
        const std::vector<int64_t> right = Document::SampleTrack(b, frame);
        REQUIRE(left.size() == right.size());
        for (std::size_t i = 0; i < left.size() && i < right.size(); i++)
            worst = std::max(worst, std::abs(left.at(i) - right.at(i)));
    }
    return worst;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("Simplifying a straight run keeps only its ends and draws every frame the same") {
    Document::AuthoredDepth depth =
        Depth({Dense("Translation", 10, [](uint32_t frame) { return int64_t{20} * frame; })});
    const Document::Track before = depth.tracks[0];
    const auto kept = Document::SimplifyKeys(depth, Every(before), 0);
    INFO(Error(kept));
    REQUIRE(kept.has_value());
    CHECK(*kept == std::vector<KeyRef>{Ref("Translation", 0), Ref("Translation", 10)});
    CHECK(Frames(depth.tracks[0]) == std::vector<uint32_t>{0, 10});
    CHECK(depth.tracks[0].keys[0].ease == Document::Ease::Linear);
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Hold);
    CHECK(WorstGap(depth.tracks[0], before, 12) == 0);
}

TEST_CASE("Simplifying keeps a keyframe wherever the line bends") {
    Document::AuthoredDepth depth = Depth({Dense("Translation", 10, [](uint32_t frame) {
        return int64_t{10} * std::min<int64_t>(frame, 5);
    })});
    const Document::Track before = depth.tracks[0];
    REQUIRE(Document::SimplifyKeys(depth, Every(before), 0).has_value());
    CHECK(Frames(depth.tracks[0]) == std::vector<uint32_t>{0, 5, 10});
    CHECK(WorstGap(depth.tracks[0], before, 10) == 0);
}

TEST_CASE("Simplifying a curve keeps every frame within the tolerance") {
    Document::AuthoredDepth depth = Depth(
        {Dense("Translation", 20, [](uint32_t frame) { return int64_t{3} * frame * frame; })});
    const Document::Track before = depth.tracks[0];
    REQUIRE(Document::SimplifyKeys(depth, Every(before), 12).has_value());
    CHECK(depth.tracks[0].keys.size() < before.keys.size());
    CHECK(depth.tracks[0].keys.size() > 2);
    CHECK(WorstGap(depth.tracks[0], before, 20) <= 12);
    CHECK(WorstGap(depth.tracks[0], before, 20) > 0);
}

TEST_CASE("A stretch that a line cannot reproduce keeps its own ease") {
    Document::AuthoredDepth depth =
        Depth({Document::Track{.property = "Translation",
                               .keys = {Held(0, {0, 0}), Held(5, {100, 0}), Held(6, {100, 0}),
                                        Held(7, {100, 0}), Held(9, {300, 0})}}});
    const Document::Track before = depth.tracks[0];
    const auto kept = Document::SimplifyKeys(depth,
                                             {Ref("Translation", 0), Ref("Translation", 5),
                                              Ref("Translation", 6), Ref("Translation", 7)},
                                             0);
    INFO(Error(kept));
    REQUIRE(kept.has_value());
    CHECK(Frames(depth.tracks[0]) == std::vector<uint32_t>{0, 5, 7, 9});
    CHECK(depth.tracks[0].keys[0].ease == Document::Ease::Hold);
    CHECK(depth.tracks[0].keys[1].ease == Document::Ease::Linear);
    CHECK(depth.tracks[0].keys[2].ease == Document::Ease::Hold);
    CHECK(WorstGap(depth.tracks[0], before, 12) == 0);
}

TEST_CASE("Simplifying leaves stepped properties alone and refuses what it cannot do") {
    const Document::Track character{.property = "Character",
                                    .keys = {Held(0, {3}), Held(1, {3}), Held(2, {3})}};
    Document::AuthoredDepth depth = Depth(
        {Dense("Translation", 4, [](uint32_t frame) { return int64_t{20} * frame; }), character});
    const Document::AuthoredDepth before = depth;
    const auto refusal = [&depth](const std::vector<KeyRef>& keys, int64_t tolerance) {
        return Error(Document::SimplifyKeys(depth, keys, tolerance));
    };
    CHECK(refusal({}, 0).find("no keyframes") != std::string::npos);
    CHECK(refusal(Every(depth.tracks[0]), -1).find("tolerance") != std::string::npos);
    CHECK(refusal({Ref("Translation", 0), Ref("Translation", 9)}, 0).find("frame 9") !=
          std::string::npos);
    CHECK(refusal({Ref("Translation", 0), Ref("Translation", 2), Ref("Translation", 4)}, 0)
              .find("frame 1") != std::string::npos);
    CHECK(refusal(Every(character), 0).find("nothing") != std::string::npos);
    Document::AuthoredDepth bowed =
        Depth({Dense("Translation", 3, [](uint32_t frame) { return int64_t{frame} * frame; })});
    CHECK(Error(Document::SimplifyKeys(bowed, Every(bowed.tracks[0]), 0)).find("nothing") !=
          std::string::npos);
    CHECK(Document::SimplifyKeys(bowed, Every(bowed.tracks[0]), 1).has_value());
    CHECK(refusal({Ref("Translation", 0), Ref("Translation", 1)}, 0).find("nothing") !=
          std::string::npos);
    CHECK(refusal({Ref("Translation", 0), Ref("Translation", 1), Ref("Translation", 2),
                   Ref("Scale", 0)},
                  0)
              .find("no property") != std::string::npos);
    CHECK(depth == before);

    std::vector<KeyRef> both = Every(depth.tracks[0]);
    for (const KeyRef& key : Every(character))
        both.push_back(key);
    const auto kept = Document::SimplifyKeys(depth, both, 0);
    INFO(Error(kept));
    REQUIRE(kept.has_value());
    CHECK(depth.tracks[1] == character);
    CHECK(std::ranges::count(*kept, std::string("Character"), &KeyRef::property) == 3);
    CHECK(Frames(depth.tracks[0]) == std::vector<uint32_t>{0, 4});
}
