#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/stage_bounds.h"
#include "document/stage_fit.h"
#include "document/stage_move.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kUseMatrix = 0x4;
constexpr uint16_t kShape = 1;
constexpr uint16_t kDepth = 3;

const std::map<uint16_t, Document::Box> kShapes{
    {kShape, Document::Box{.left = 2, .right = 12, .top = 1, .bottom = 5}}};

AfpAnimation::Animation Scene(const AfpAnimation::Placement& placed) {
    AfpAnimation::Animation animation;
    animation.rect = {0, 1920, 0, 1080};
    for (int i = 0; i < 2; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{placed});
    return animation;
}

AfpAnimation::Placement Placed() {
    AfpAnimation::Placement placed;
    placed.flags = kUseMatrix;
    placed.depth = kDepth;
    placed.character = kShape;
    placed.scale = std::array<int32_t, 2>{2048, 1024};
    placed.translation = std::array<int32_t, 2>{200, 400};
    placed.origin = std::array<int32_t, 2>{60, 40};
    return placed;
}

Document::Box Fitted(Document::StageFit fit) {
    AfpAnimation::Animation animation = Scene(Placed());
    const auto change = Document::FitToStage(animation, kDepth, 0, kShapes, fit);
    REQUIRE(change.has_value());
    REQUIRE(Document::ReshapeBakedDepth(animation, {}, kDepth, 0, change->reshape).has_value());
    REQUIRE(Document::MoveBakedDepth(animation, {}, kDepth, 0, change->offset).has_value());
    const auto outlines = Document::StageOutlines(animation, {}, 0, kShapes);
    REQUIRE(outlines.size() == 1);
    Document::Box box{.left = 1e9, .right = -1e9, .top = 1e9, .bottom = -1e9};
    for (const Document::Point& corner : outlines.front().corners) {
        box.left = std::min(box.left, corner[0]);
        box.right = std::max(box.right, corner[0]);
        box.top = std::min(box.top, corner[1]);
        box.bottom = std::max(box.bottom, corner[1]);
    }
    return box;
}

bool Near(const Document::Box& a, const Document::Box& b) {
    return std::abs(a.left - b.left) < 0.1 && std::abs(a.right - b.right) < 0.1 &&
           std::abs(a.top - b.top) < 0.1 && std::abs(a.bottom - b.bottom) < 0.1;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("Fitting a depth to the stage fills the stage exactly") {
    const Document::Box box = Fitted(Document::StageFit::Both);
    INFO(box.left << " " << box.right << " " << box.top << " " << box.bottom);
    CHECK(Near(box, Document::Box{.left = 0, .right = 1920, .top = 0, .bottom = 1080}));
}

TEST_CASE("Fitting to the stage's width or height keeps the aspect and centres the depth") {
    const Document::Box wide = Fitted(Document::StageFit::Width);
    INFO(wide.left << " " << wide.right << " " << wide.top << " " << wide.bottom);
    CHECK(Near(wide, Document::Box{.left = 0, .right = 1920, .top = 348, .bottom = 732}));
    const Document::Box tall = Fitted(Document::StageFit::Height);
    INFO(tall.left << " " << tall.right << " " << tall.top << " " << tall.bottom);
    CHECK(Near(tall, Document::Box{.left = -1740, .right = 3660, .top = 0, .bottom = 1080}));
}

TEST_CASE("Fitting is refused for a depth with no box, one turned or one squashed flat") {
    const auto refusal = [](const AfpAnimation::Placement& placed, uint32_t frame) {
        return Error(
            Document::FitToStage(Scene(placed), kDepth, frame, kShapes, Document::StageFit::Both));
    };
    CHECK(refusal(Placed(), 5).find("no") != std::string::npos);
    AfpAnimation::Placement unknown = Placed();
    unknown.character = uint16_t{99};
    CHECK(refusal(unknown, 0).find("box the editor knows") != std::string::npos);
    AfpAnimation::Placement turned = Placed();
    turned.rotate_skew = std::array<int32_t, 2>{0, 512};
    CHECK(refusal(turned, 0).find("turned") != std::string::npos);
    AfpAnimation::Placement skewed = Placed();
    skewed.rotate_skew = std::array<int32_t, 2>{512, 0};
    CHECK(refusal(skewed, 0).find("turned") != std::string::npos);
    AfpAnimation::Placement flat = Placed();
    flat.scale = std::array<int32_t, 2>{0, 1024};
    CHECK(refusal(flat, 0).find("flat") != std::string::npos);
    AfpAnimation::Placement squashed = Placed();
    squashed.scale = std::array<int32_t, 2>{1024, 0};
    CHECK(refusal(squashed, 0).find("flat") != std::string::npos);
}
