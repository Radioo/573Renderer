#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframes.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint16_t kDepth = 3;
const Document::ClipId kRoot{};

std::vector<AfpAnimation::Filter> Tint(int32_t red) {
    AfpAnimation::ColourMatrixFilter matrix;
    matrix.head = {6, 0, 0, 0};
    matrix.matrix.at(0) = red;
    return {matrix};
}

AfpAnimation::Placement Update(std::optional<std::vector<AfpAnimation::Filter>> filters,
                               int32_t x) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting | kUseMatrix;
    placement.depth = kDepth;
    placement.end_frame = 5;
    placement.filters = std::move(filters);
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Animation Filtered(std::optional<std::vector<AfpAnimation::Filter>> later) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (int i = 0; i < 5; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AfpAnimation::Placement create;
    create.flags = kUseMatrix;
    create.depth = kDepth;
    create.end_frame = 5;
    create.character = uint16_t{7};
    create.filters = Tint(65536);
    create.translation = std::array<int32_t, 2>{0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{Update(std::nullopt, 20)});
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Update(std::move(later), 40)});
    return animation;
}

const Document::Track* TrackFor(const Document::AuthoredDepth& authored,
                                const std::string& property) {
    const auto found = std::ranges::find(authored.tracks, property, &Document::Track::property);
    return found == authored.tracks.end() ? nullptr : &*found;
}

}

TEST_CASE("A depth whose updates change its filters is owned with a stepped filter track") {
    const AfpAnimation::Animation animation = Filtered(Tint(32768));
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    const std::string error = owned.has_value() ? std::string() : owned.error();
    INFO(error);
    REQUIRE(owned.has_value());
    if (!owned) return;
    const Document::Track* filters = TrackFor(owned->authored, "Filters");
    REQUIRE(filters != nullptr);
    REQUIRE(filters->keys.size() == 2);
    CHECK(filters->keys[0].frame == 0);
    CHECK(filters->keys[1].frame == 2);
    CHECK(filters->keys[0].ease == Document::Ease::Hold);
    CHECK_FALSE(owned->baked.create.filters.has_value());

    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A depth whose updates leave its filters alone keeps them on the create") {
    const AfpAnimation::Animation animation = Filtered(std::nullopt);
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    CHECK(TrackFor(owned->authored, "Filters") == nullptr);
    REQUIRE(owned->baked.create.filters.has_value());
    if (!owned->baked.create.filters) return;
    const std::vector<AfpAnimation::Filter> kept = *owned->baked.create.filters;
    CHECK(kept == Tint(65536));
    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A depth whose filters change shape is refused") {
    std::vector<AfpAnimation::Filter> two = Tint(1);
    two.push_back(two.front());
    const AfpAnimation::Animation animation = Filtered(two);
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("shape of its Filters") != std::string::npos);
}
