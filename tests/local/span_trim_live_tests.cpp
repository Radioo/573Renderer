#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/frame_edit.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "live_stage.h"
#include "support/env.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint32_t kLaterBy = 10;
constexpr uint32_t kShortestSpan = 30;

using LiveStage::AnimationPath;
using LiveStage::ReadAll;
using LiveStage::Stage;
using LiveStage::Target;

bool IsThreeD(const AfpAnimation::Placement& placement) {
    return (placement.flags & kThreeD) != 0;
}

bool UpdatesCurves(const AfpAnimation::Placement& placement) {
    return (placement.flags & kUpdateExisting) != 0 && placement.curves.has_value();
}

bool AnyAtDepth(const AfpAnimation::Animation& animation, uint16_t depth,
                bool (*wanted)(const AfpAnimation::Placement&)) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth && wanted(*placement)) return true;
    }
    return false;
}

struct Picked {
    uint16_t depth = 0;
    Document::Span span;
    AfpAnimation::Animation trimmed;
};

std::optional<Picked> TrimmableSpan(const AfpAnimation::Animation& animation,
                                    bool (*wanted)(const AfpAnimation::Placement&)) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        if (!AnyAtDepth(animation, row.depth, wanted)) continue;
        for (const Document::Span& span : row.spans) {
            if (span.last_frame - span.first_frame < kShortestSpan) continue;
            AfpAnimation::Animation trimmed = animation;
            const Document::Span kept{.first_frame = span.first_frame + kLaterBy,
                                      .last_frame = span.last_frame};
            if (!Document::TrimSpan(trimmed, {}, row.depth, span.first_frame, kept)) continue;
            return Picked{.depth = row.depth, .span = span, .trimmed = std::move(trimmed)};
        }
    }
    return std::nullopt;
}

void CheckTrimDrawsAlike(const Target& target, bool (*wanted)(const AfpAnimation::Placement&)) {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto original =
        Document::File::Open(ReadAll(dir + "/data/graphic/1/" + target.package + ".ifs"));
    REQUIRE(original.has_value());
    if (!original) return;
    const std::string path = AnimationPath(*original, target.animation);
    REQUIRE(!path.empty());
    const auto animation = original->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return;
    const std::optional<Picked> picked = TrimmableSpan(*animation, wanted);
    REQUIRE(picked.has_value());
    if (!picked) return;
    INFO("depth " << picked->depth << " frames " << picked->span.first_frame << " to "
                  << picked->span.last_frame);

    Document::File trimmed = *original;
    REQUIRE(trimmed.WriteAnimation(path, picked->trimmed).has_value());
    Document::File without = *original;
    AfpAnimation::Animation removed = *animation;
    REQUIRE(
        Document::RemoveDepth(removed, {}, picked->depth, picked->span.first_frame).has_value());
    REQUIRE(without.WriteAnimation(path, removed).has_value());

    Stage stage(dir, target);
    const std::array<uint32_t, 3> kept{picked->span.first_frame + kLaterBy,
                                       picked->span.first_frame + kLaterBy + 5,
                                       picked->span.last_frame};
    bool depth_drawn = false;
    for (const uint32_t frame : kept) {
        INFO("frame " << frame);
        const std::vector<uint8_t> before = stage.Render(*original, frame);
        const std::vector<uint8_t> after = stage.Render(trimmed, frame);
        CHECK(after == before);
        depth_drawn = depth_drawn || stage.Render(without, frame) != before;
    }
    CHECK(depth_drawn);
}

}

TEST_CASE("A trimmed 3D span draws its kept frames exactly as before") {
    CheckTrimDrawsAlike(Target{.package = "arena", .animation = "x_panel_broken"}, IsThreeD);
}

TEST_CASE("A trimmed span whose updates change curves draws its kept frames exactly as before") {
    CheckTrimDrawsAlike(Target{.package = "led_effects", .animation = "Background_life"},
                        UpdatesCurves);
}
