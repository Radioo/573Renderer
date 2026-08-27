#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "editor/curve_geometry.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/eval/eval_tween.h"

#include <array>
#include <string>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

constexpr Editor::CurveRect kRect = {.x0 = 100.0F, .y0 = 40.0F, .x1 = 500.0F, .y1 = 240.0F};

constexpr int kTweenFrames = 100;

Doc::Clip LateChannelTween() {
    Doc::Clip clip;
    clip.id = "core_fade";
    clip.start = 40;
    clip.end = clip.start + kTweenFrames;
    clip.command = Doc::ModelTween{};
    clip.keys.push_back(Doc::Key{.at = 0, .values = {Doc::KeyValue{.id = "alpha", .value = 0.0}}});
    clip.keys.push_back(Doc::Key{.at = kTweenFrames,
                                 .values = {Doc::KeyValue{.id = "alpha", .value = 1.0},
                                            Doc::KeyValue{.id = "scale", .value = 3.0}}});
    return clip;
}

Editor::CurveChannel ChannelNamed(const Doc::Clip& clip, const std::string& field) {
    for (const Editor::CurveChannel& channel : Editor::ChannelsOf(clip)) {
        if (channel.field == field) return channel;
    }
    FAIL("clip has no channel named " + field);
    return {};
}

Preset::Eval::TweenValue Scalar(float value) {
    Preset::Eval::TweenValue out;
    out.kind = Preset::Eval::TweenValue::Kind::Scalar;
    out.scalar = value;
    return out;
}

}

TEST_CASE("the curve editor maps frames to x and back", "[editor][curve]") {
    CHECK(Editor::CurveX(kRect, 200, 0.0) == Catch::Approx(100.0));
    CHECK(Editor::CurveX(kRect, 200, 200.0) == Catch::Approx(500.0));
    CHECK(Editor::CurveX(kRect, 200, 100.0) == Catch::Approx(300.0));
    CHECK(Editor::CurveFrame(kRect, 200, 300.0F) == Catch::Approx(100.0));
    for (const double frame : {0.0, 37.0, 123.5, 200.0}) {
        CHECK(Editor::CurveFrame(kRect, 200, Editor::CurveX(kRect, 200, frame)) ==
              Catch::Approx(frame));
    }
}

TEST_CASE("the curve editor maps values to y and back with y growing upwards", "[editor][curve]") {
    const Editor::CurveRange range{.low = -1.0, .high = 3.0};
    CHECK(Editor::CurveY(kRect, range, 3.0) == Catch::Approx(40.0));
    CHECK(Editor::CurveY(kRect, range, -1.0) == Catch::Approx(240.0));
    CHECK(Editor::CurveY(kRect, range, 1.0) == Catch::Approx(140.0));
    for (const double value : {-1.0, 0.0, 0.75, 3.0}) {
        CHECK(Editor::CurveValue(kRect, range, Editor::CurveY(kRect, range, value)) ==
              Catch::Approx(value));
    }
}

TEST_CASE("a flat value list still gets a range with height", "[editor][curve]") {
    const Editor::CurveRange flat = Editor::RangeOfValues({2.0, 2.0});
    CHECK(flat.high > flat.low);
    CHECK(flat.low < 2.0);
    CHECK(flat.high > 2.0);

    const Editor::CurveRange spread = Editor::RangeOfValues({-1.0, 0.0, 4.0});
    CHECK(spread.low <= -1.0);
    CHECK(spread.high >= 4.0);
}

TEST_CASE("a bezier handle round-trips through screen space", "[editor][curve]") {
    const Editor::CurveRange range{.low = 0.0, .high = 10.0};
    const Editor::CurveSegment segment{.a_at = 20, .b_at = 120, .a_value = 2.0, .b_value = 8.0};
    const std::array<double, 4> cp = {0.42, 0.0, 0.58, 1.0};

    const Editor::CurvePoint first = Editor::BezierHandle(kRect, range, 200, segment, cp, 0);
    CHECK(first.x == Catch::Approx(Editor::CurveX(kRect, 200, 20.0 + (0.42 * 100.0))));
    CHECK(first.y == Catch::Approx(Editor::CurveY(kRect, range, 2.0)));

    const Editor::CurvePoint moved{.x = first.x + 20.0F, .y = first.y - 10.0F};
    const std::array<double, 4> edited =
        Editor::BezierWithHandle(kRect, range, 200, segment, cp, 0, moved);
    CHECK(edited[2] == Catch::Approx(cp[2]));
    CHECK(edited[3] == Catch::Approx(cp[3]));
    CHECK(Editor::BezierHandle(kRect, range, 200, segment, edited, 0).x == Catch::Approx(moved.x));
    CHECK(Editor::BezierHandle(kRect, range, 200, segment, edited, 0).y == Catch::Approx(moved.y));

    const Editor::CurvePoint second = Editor::BezierHandle(kRect, range, 200, segment, cp, 1);
    CHECK(second.x == Catch::Approx(Editor::CurveX(kRect, 200, 20.0 + (0.58 * 100.0))));
    CHECK(second.y == Catch::Approx(Editor::CurveY(kRect, range, 8.0)));
}

TEST_CASE("the auto range holds every point the curve draws from the underlying value",
          "[editor][curve]") {
    const Doc::Clip clip = LateChannelTween();
    const Editor::CurveChannel channel = ChannelNamed(clip, "scale");
    const Preset::Eval::TweenValue underlying = Scalar(10.0F);

    const std::vector<Editor::CurveSample> drawn =
        Editor::CurveSamples(clip, channel, underlying, kTweenFrames);
    REQUIRE(drawn.size() > 2);
    CHECK(drawn.front().value == Catch::Approx(10.0));
    CHECK(drawn.back().value == Catch::Approx(3.0));

    const Editor::CurveRange range =
        Editor::CurveAutoRange(clip, channel, underlying, kTweenFrames);
    for (const Editor::CurveSample& sample : drawn) {
        CHECK(sample.value >= range.low);
        CHECK(sample.value <= range.high);
    }
}

TEST_CASE("a bezier handle x stays inside the segment", "[editor][curve]") {
    const Editor::CurveRange range{.low = 0.0, .high = 10.0};
    const Editor::CurveSegment segment{.a_at = 20, .b_at = 120, .a_value = 2.0, .b_value = 8.0};
    const std::array<double, 4> cp = {0.42, 0.0, 0.58, 1.0};
    const Editor::CurvePoint far{.x = kRect.x1 + 400.0F, .y = kRect.y0};
    const std::array<double, 4> edited =
        Editor::BezierWithHandle(kRect, range, 200, segment, cp, 0, far);
    CHECK(edited[0] <= 1.0);
    CHECK(edited[0] >= 0.0);
}
