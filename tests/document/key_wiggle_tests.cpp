#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/key_wiggle.h"
#include "document/keyframes.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using Document::KeyRef;

Document::Keyframe Linear(uint32_t frame, std::vector<int64_t> value) {
    return Document::Keyframe{
        .frame = frame, .value = std::move(value), .ease = Document::Ease::Linear, .bezier = {}};
}

Document::AuthoredDepth Depth() {
    const Document::Track moving{.property = "Translation",
                                 .keys = {Linear(0, {0, 0}), Linear(4, {400, 40}),
                                          Linear(20, {2000, 200}), Linear(24, {2000, 200})}};
    const Document::Track character{.property = "Character",
                                    .keys = {Linear(0, {3}), Linear(20, {4})}};
    return Document::AuthoredDepth{.animation = "afp/scene",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 30,
                                   .tracks = {moving, character},
                                   .script = std::nullopt,
                                   .clip = {}};
}

KeyRef Ref(const std::string& property, uint32_t frame) {
    return KeyRef{.property = property, .frame = frame};
}

std::vector<uint32_t> Frames(const Document::Track& track) {
    std::vector<uint32_t> frames;
    frames.reserve(track.keys.size());
    for (const Document::Keyframe& key : track.keys)
        frames.push_back(key.frame);
    return frames;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

const Document::Wiggle kWiggle{.every = 3, .magnitude = 50, .seed = 7};

}

TEST_CASE("Wiggling puts a keyframe every few frames within the magnitude of the old curve") {
    Document::AuthoredDepth depth = Depth();
    const Document::Track before = depth.tracks[0];
    const auto wiggled = Document::WiggleKeys(
        depth, {Ref("Translation", 0), Ref("Translation", 4), Ref("Translation", 20)}, kWiggle);
    INFO(Error(wiggled));
    REQUIRE(wiggled.has_value());
    const Document::Track& after = depth.tracks[0];
    CHECK(Frames(after) == std::vector<uint32_t>{0, 3, 6, 9, 12, 15, 18, 20, 24});
    CHECK(after.keys.front() == before.keys.front());
    CHECK(after.keys[7].value == before.keys[2].value);
    CHECK(after.keys.back() == before.keys.back());
    bool moved = false;
    for (std::size_t i = 1; i < 7; i++) {
        const Document::Keyframe& key = after.keys[i];
        INFO(key.frame);
        CHECK(key.ease == Document::Ease::Linear);
        const std::vector<int64_t> was = Document::SampleTrack(before, key.frame);
        REQUIRE(key.value.size() == was.size());
        for (std::size_t v = 0; v < was.size(); v++) {
            CHECK(std::abs(key.value[v] - was[v]) <= 50);
            moved = moved || key.value[v] != was[v];
        }
    }
    CHECK(moved);
    CHECK(after.keys[1].value[0] - Document::SampleTrack(before, 3)[0] !=
          after.keys[1].value[1] - Document::SampleTrack(before, 3)[1]);
    std::vector<KeyRef> expected;
    for (const uint32_t frame : Frames(after)) {
        if (frame <= 20) expected.push_back(Ref("Translation", frame));
    }
    CHECK(*wiggled == expected);
    CHECK(depth.tracks[1] == Depth().tracks[1]);
}

TEST_CASE("The same seed wiggles the same way and another seed does not") {
    Document::AuthoredDepth first = Depth();
    Document::AuthoredDepth again = Depth();
    Document::AuthoredDepth other = Depth();
    const std::vector<KeyRef> keys{Ref("Translation", 0), Ref("Translation", 4),
                                   Ref("Translation", 20)};
    REQUIRE(Document::WiggleKeys(first, keys, kWiggle).has_value());
    REQUIRE(Document::WiggleKeys(again, keys, kWiggle).has_value());
    REQUIRE(
        Document::WiggleKeys(other, keys, Document::Wiggle{.every = 3, .magnitude = 50, .seed = 8})
            .has_value());
    CHECK(first == again);
    CHECK(first != other);
}

TEST_CASE("Wiggling is refused, leaving the depth alone, where it cannot wiggle") {
    Document::AuthoredDepth depth = Depth();
    const Document::AuthoredDepth before = depth;
    const auto refusal = [&depth](const std::vector<KeyRef>& keys, Document::Wiggle wiggle) {
        return Error(Document::WiggleKeys(depth, keys, wiggle));
    };
    const std::vector<KeyRef> run{Ref("Translation", 0), Ref("Translation", 4),
                                  Ref("Translation", 20)};
    CHECK(refusal({}, kWiggle).find("no keyframes") != std::string::npos);
    CHECK(refusal(run, Document::Wiggle{.every = 0, .magnitude = 50, .seed = 7}).find("every") !=
          std::string::npos);
    CHECK(refusal(run, Document::Wiggle{.every = 3, .magnitude = 0, .seed = 7}).find("magnitude") !=
          std::string::npos);
    CHECK(refusal({Ref("Translation", 0), Ref("Translation", 5)}, kWiggle).find("frame 5") !=
          std::string::npos);
    CHECK(refusal({Ref("Translation", 4), Ref("Translation", 24)}, kWiggle).find("frame 20") !=
          std::string::npos);
    CHECK(refusal({Ref("Translation", 20)}, kWiggle).find("two") != std::string::npos);
    CHECK(refusal({Ref("Character", 0), Ref("Character", 20)}, kWiggle).find("two") !=
          std::string::npos);
    CHECK(refusal({Ref("Translation", 20), Ref("Translation", 24)},
                  Document::Wiggle{.every = 5, .magnitude = 50, .seed = 7})
              .find("room") != std::string::npos);
    CHECK(refusal({Ref("Translation", 20), Ref("Translation", 24)},
                  Document::Wiggle{.every = 4, .magnitude = 50, .seed = 7})
              .find("room") != std::string::npos);
    CHECK(refusal({Ref("Translation", 0), Ref("Translation", 4), Ref("Scale", 0)}, kWiggle)
              .find("no property") != std::string::npos);
    CHECK(depth == before);
}
