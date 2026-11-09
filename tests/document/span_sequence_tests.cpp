#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/span_sequence.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
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

void Add(AfpAnimation::Animation& animation, uint16_t depth, uint32_t first, uint32_t last) {
    const auto added = Document::AddDepth(animation, kRoot, depth, 7, first, last);
    REQUIRE(added.has_value());
}

std::map<uint16_t, std::vector<Document::Span>> Spans(const AfpAnimation::Animation& animation) {
    std::map<uint16_t, std::vector<Document::Span>> spans;
    for (const Document::DepthRow& row : Document::DepthRows(animation.root))
        spans[row.depth] = row.spans;
    return spans;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

using Shifts = std::vector<Document::SpanShift>;
using Layout = std::map<uint16_t, std::vector<Document::Span>>;

}

TEST_CASE("Sequencing puts each chosen depth's span right after the one before it") {
    AfpAnimation::Animation animation = Clip(12);
    Add(animation, 1, 0, 3);
    Add(animation, 4, 1, 5);
    Add(animation, 7, 2, 2);
    const auto sequenced = Document::SequenceSpans(animation, kRoot, {7, 1, 4}, 2);
    INFO(Error(sequenced));
    REQUIRE(sequenced.has_value());
    CHECK(*sequenced ==
          Shifts{{.depth = 4, .frame = 1, .by = 3}, {.depth = 7, .frame = 2, .by = 7}});
    CHECK(Spans(animation) == Layout{{1, {{0, 3}}}, {4, {{4, 8}}}, {7, {{9, 9}}}});
}

TEST_CASE("Sequencing keeps clear of a depth's other spans and of depths not chosen") {
    AfpAnimation::Animation animation = Clip(12);
    Add(animation, 1, 0, 1);
    Add(animation, 2, 0, 8);
    Add(animation, 3, 5, 9);
    Add(animation, 3, 0, 1);
    const auto sequenced = Document::SequenceSpans(animation, kRoot, {1, 3}, 0);
    INFO(Error(sequenced));
    REQUIRE(sequenced.has_value());
    CHECK(*sequenced == Shifts{{.depth = 3, .frame = 0, .by = 2}});
    CHECK(Spans(animation).at(3) == std::vector<Document::Span>{{2, 3}, {5, 9}});
    CHECK(Spans(animation).at(2) == std::vector<Document::Span>{{0, 8}});
}

TEST_CASE("Sequencing is refused, leaving the clip alone, when the spans cannot line up") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, 1, 0, 3);
    Add(animation, 2, 0, 3);
    Add(animation, 3, 0, 1);
    Add(animation, 3, 5, 7);
    Add(animation, 4, 2, 2);
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::SequenceSpans(animation, kRoot, {1}, 0).has_value());
    CHECK_FALSE(Document::SequenceSpans(animation, kRoot, {1, 4}, 0).has_value());
    CHECK_FALSE(Document::SequenceSpans(animation, kRoot, {1, 2, 3}, 0).has_value());
    CHECK_FALSE(Document::SequenceSpans(animation, kRoot, {1, 3}, 0).has_value());
    CHECK_FALSE(
        Document::SequenceSpans(animation, Document::ClipId{.sprite = uint16_t{9}}, {1, 2}, 0)
            .has_value());
    CHECK(animation == before);
}
