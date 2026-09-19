#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/span_arrange.h"
#include "document/frame_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace {

const Document::ClipId kRoot{};

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

void Add(AfpAnimation::Animation& animation, uint16_t depth, uint16_t character, uint32_t first,
         uint32_t last) {
    const auto added = Document::AddDepth(animation, kRoot, depth, character, first, last);
    REQUIRE(added.has_value());
}

std::map<int, int> ShownAt(const AfpAnimation::Animation& animation, uint32_t frame) {
    std::map<int, int> shown;
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        for (const auto& [first, character] : row.shows) {
            if (first > frame) break;
            const bool open = std::ranges::any_of(row.spans, [frame](const Document::Span& span) {
                return frame >= span.first_frame && frame <= span.last_frame;
            });
            if (open) shown[row.depth] = character;
        }
    }
    return shown;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

AfpAnimation::Animation ThreeDepths() {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, 1, 10, 0, 5);
    Add(animation, 2, 20, 0, 5);
    Add(animation, 3, 30, 0, 5);
    return animation;
}

using Changes = std::vector<Document::DepthChange>;
using Shown = std::map<int, int>;

}

TEST_CASE("Bringing a span forward swaps it with the next depth shown on the frame") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, 0, 5, 0, 5);
    Add(animation, 2, 20, 0, 5);
    Add(animation, 4, 40, 3, 5);
    Add(animation, 6, 60, 0, 5);
    const auto arranged = Document::ArrangeSpan(animation, kRoot, 2, 1, Document::Arrange::Forward);
    INFO(Error(arranged));
    REQUIRE(arranged.has_value());
    CHECK(*arranged == Changes{{.from = 2, .to = 6}, {.from = 6, .to = 2}});
    CHECK(ShownAt(animation, 1) == Shown{{0, 5}, {2, 60}, {6, 20}});
    CHECK(ShownAt(animation, 4) == Shown{{0, 5}, {2, 60}, {4, 40}, {6, 20}});
}

TEST_CASE("Bringing a span forward passes only the depth right above it") {
    AfpAnimation::Animation animation = ThreeDepths();
    const auto arranged = Document::ArrangeSpan(animation, kRoot, 1, 0, Document::Arrange::Forward);
    REQUIRE(arranged.has_value());
    CHECK(*arranged == Changes{{.from = 1, .to = 2}, {.from = 2, .to = 1}});
    CHECK(ShownAt(animation, 0) == Shown{{1, 20}, {2, 10}, {3, 30}});
}

TEST_CASE("Sending a span backward swaps it with the next depth below") {
    AfpAnimation::Animation animation = ThreeDepths();
    const auto arranged =
        Document::ArrangeSpan(animation, kRoot, 3, 0, Document::Arrange::Backward);
    REQUIRE(arranged.has_value());
    CHECK(*arranged == Changes{{.from = 3, .to = 2}, {.from = 2, .to = 3}});
    CHECK(ShownAt(animation, 0) == Shown{{1, 10}, {2, 30}, {3, 20}});
}

TEST_CASE("Bringing a span to the front moves every depth above it down one place") {
    AfpAnimation::Animation animation = ThreeDepths();
    const auto arranged = Document::ArrangeSpan(animation, kRoot, 1, 2, Document::Arrange::Front);
    REQUIRE(arranged.has_value());
    CHECK(*arranged == Changes{{.from = 1, .to = 3}, {.from = 2, .to = 1}, {.from = 3, .to = 2}});
    CHECK(ShownAt(animation, 2) == Shown{{1, 20}, {2, 30}, {3, 10}});
}

TEST_CASE("Sending a span to the back moves every depth below it up one place") {
    AfpAnimation::Animation animation = ThreeDepths();
    const auto arranged = Document::ArrangeSpan(animation, kRoot, 3, 2, Document::Arrange::Back);
    REQUIRE(arranged.has_value());
    CHECK(*arranged == Changes{{.from = 3, .to = 1}, {.from = 2, .to = 3}, {.from = 1, .to = 2}});
    CHECK(ShownAt(animation, 2) == Shown{{1, 30}, {2, 10}, {3, 20}});
}

TEST_CASE("Arranging is refused, leaving the clip alone, when the spans cannot change places") {
    AfpAnimation::Animation animation = ThreeDepths();
    Add(animation, 5, 50, 0, 2);
    Add(animation, 5, 51, 3, 5);
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(
        Document::ArrangeSpan(animation, kRoot, 5, 1, Document::Arrange::Forward).has_value());
    CHECK_FALSE(Document::ArrangeSpan(animation, kRoot, 1, 1, Document::Arrange::Back).has_value());
    CHECK_FALSE(
        Document::ArrangeSpan(animation, kRoot, 1, 7, Document::Arrange::Forward).has_value());
    CHECK_FALSE(
        Document::ArrangeSpan(animation, kRoot, 3, 1, Document::Arrange::Forward).has_value());
    CHECK_FALSE(Document::ArrangeSpan(animation, Document::ClipId{.sprite = uint16_t{3}}, 1, 1,
                                      Document::Arrange::Forward)
                    .has_value());
    CHECK(animation == before);
}

TEST_CASE("A span that masks other depths is not arranged") {
    AfpAnimation::Animation animation = ThreeDepths();
    for (AfpAnimation::Tag& tag : animation.root.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == 1) placement->clip_depth = uint16_t{2};
    }
    const AfpAnimation::Animation before = animation;
    const auto masking = Document::ArrangeSpan(animation, kRoot, 1, 0, Document::Arrange::Forward);
    REQUIRE_FALSE(masking.has_value());
    CHECK(masking.error().find("masks") != std::string::npos);
    CHECK_FALSE(
        Document::ArrangeSpan(animation, kRoot, 2, 0, Document::Arrange::Backward).has_value());
    CHECK(animation == before);
    CHECK(Document::ArrangeSpan(animation, kRoot, 2, 0, Document::Arrange::Forward).has_value());
}
