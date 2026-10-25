#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/anchor_edit.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/frame_edit.h"
#include "document/span_tags.h"
#include "document/stage_bounds.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "live_stage.h"
#include "support/env.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kSamples = 12;
constexpr int kRoundingTolerance = 7;
constexpr int32_t kOnePixel = 20;
constexpr uint32_t kUseMatrix = 0x4;
constexpr std::array<int32_t, 2> kUnitScale{1024, 1024};
const LiveStage::Target kTarget{.package = "title", .animation = "title"};

bool ScaledOrTurned(const AfpAnimation::Container& clip, uint16_t depth,
                    const Document::Span& span) {
    return std::ranges::any_of(Document::SpanTags(clip, depth, span), [&clip](std::size_t index) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
        if (placement == nullptr) return false;
        return placement->rotate_skew || placement->short_rotate_skew || placement->short_scale ||
               (placement->scale && *placement->scale != kUnitScale);
    });
}

int WorstGap(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    REQUIRE(a.size() == b.size());
    int worst = 0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); i++)
        worst = std::max(worst, std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])));
    return worst;
}

AfpAnimation::Animation NudgedOnePixel(const AfpAnimation::Animation& animation,
                                       Document::ClipId clip, uint16_t depth,
                                       const Document::Span& span) {
    AfpAnimation::Animation nudged = animation;
    AfpAnimation::Container& target = **Document::RequireClip(nudged, clip);
    for (const std::size_t index : Document::SpanTags(target, depth, span)) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&target.tags[index].body);
        if (placement == nullptr || (placement->flags & kUseMatrix) == 0) continue;
        std::array<int32_t, 2> moved = placement->translation.value_or(std::array<int32_t, 2>{});
        moved[0] += kOnePixel;
        placement->translation = moved;
    }
    return nudged;
}

struct Centred {
    Document::ClipId clip;
    uint16_t depth = 0;
    Document::Span span;
    AfpAnimation::Animation animation;
};

std::optional<Centred> ScaledSpanInASprite(const AfpAnimation::Animation& animation,
                                           const std::map<uint16_t, Document::Box>& shapes) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr) continue;
        const Document::ClipId clip{.sprite = sprite->id};
        for (const Document::DepthRow& row : Document::DepthRows(sprite->container)) {
            for (const Document::Span& span : row.spans) {
                if (!ScaledOrTurned(sprite->container, row.depth, span)) continue;
                AfpAnimation::Animation centred = animation;
                if (!Document::CentreAnchor(centred, clip, row.depth, span.first_frame, shapes))
                    continue;
                return Centred{.clip = clip,
                               .depth = row.depth,
                               .span = span,
                               .animation = std::move(centred)};
            }
        }
    }
    return std::nullopt;
}

}

TEST_CASE(
    "A centred anchor on a scaled object draws every frame as before, to the format's precision") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto original = Document::File::Open(
        LiveStage::ReadAll(dir + "/data/graphic/1/" + kTarget.package + ".ifs"));
    REQUIRE(original.has_value());
    if (!original) return;
    const std::string path = LiveStage::AnimationPath(*original, kTarget.animation);
    REQUIRE(!path.empty());
    const auto animation = original->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return;
    const std::optional<Centred> centred =
        ScaledSpanInASprite(*animation, original->ShapeBounds(path));
    REQUIRE(centred.has_value());
    if (!centred) return;
    INFO("sprite " << *centred->clip.sprite << " depth " << centred->depth << " frames "
                   << centred->span.first_frame << " to " << centred->span.last_frame);

    Document::File moved = *original;
    REQUIRE(moved.WriteAnimation(path, centred->animation).has_value());
    Document::File without = *original;
    AfpAnimation::Animation removed = *animation;
    REQUIRE(Document::RemoveDepth(removed, centred->clip, centred->depth, centred->span.first_frame)
                .has_value());
    REQUIRE(without.WriteAnimation(path, removed).has_value());
    Document::File nudged = *original;
    REQUIRE(nudged
                .WriteAnimation(
                    path, NudgedOnePixel(*animation, centred->clip, centred->depth, centred->span))
                .has_value());

    LiveStage::Stage stage(dir, kTarget);
    const auto frames = static_cast<uint32_t>(animation->root.frames.size());
    bool depth_drawn = false;
    for (uint32_t sample = 0; sample < kSamples; sample++) {
        const uint32_t frame = sample * frames / kSamples;
        INFO("frame " << frame);
        const std::vector<uint8_t> before = stage.Render(*original, frame);
        CHECK(WorstGap(stage.Render(moved, frame), before) <= kRoundingTolerance);
        if (stage.Render(without, frame) == before) continue;
        depth_drawn = true;
        CHECK(WorstGap(stage.Render(nudged, frame), before) > kRoundingTolerance);
    }
    CHECK(depth_drawn);
}
