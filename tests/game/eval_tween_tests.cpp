#include <catch2/catch_test_macros.hpp>

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_document.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace PD = Preset::Doc;
namespace PE = Preset::Eval;

float LegacyFactor(bool sine, int elapsed, int frames, float degrees_per_frame) {
    const int span = std::max(1, frames);
    const float t = (float)std::min(elapsed, span) / (float)span;
    if (!sine) return t;
    const auto degrees = (float)std::min(elapsed, span) * degrees_per_frame;
    return std::sin(degrees * 0.017453292F);
}

PD::Key KeyAt(int at, PD::Ease ease, const std::string& id, const PD::Vec3& value) {
    PD::Key key{.at = at, .ease = ease};
    key.values.push_back(PD::KeyValue{.id = id, .value = value});
    return key;
}

PD::Document AlphaBlendDocument() {
    PD::Document doc;
    doc.id = "tween-alpha";
    doc.build = "iidx11";
    doc.length = 200;
    doc.assets.push_back(PD::Asset{.id = "red", .kind = PD::AssetKind::Scene3d, .dir = "red"});

    PD::Clip draw;
    draw.id = "core_draw";
    draw.start = 0;
    draw.command = PD::ModelDraw{.asset = "red", .alpha = 0.25};
    doc.tracks.push_back(PD::Track{.id = "core",
                                   .name = "core",
                                   .kind = PD::TrackKind::Model,
                                   .target = "core",
                                   .clips = {std::move(draw)}});

    PD::Clip tween;
    tween.id = "core_fade";
    tween.start = 0;
    tween.end = 100;
    tween.command = PD::ModelTween{};
    tween.keys.push_back(KeyAt(0, PD::Ease::Linear, "position", PD::Vec3{0.0, 0.0, 0.0}));
    PD::Key late{.at = 50};
    late.values.push_back(PD::KeyValue{.id = "alpha", .value = 1.0});
    tween.keys.push_back(std::move(late));
    doc.tracks.push_back(PD::Track{.id = "core_tween",
                                   .name = "core_tween",
                                   .kind = PD::TrackKind::Model,
                                   .target = "core",
                                   .clips = {std::move(tween)}});
    return doc;
}

}

TEST_CASE("the linear ease reproduces the value ApplyRamps computes") {
    const PD::Vec3 from = {0.0, 0.0, 0.0};
    const PD::Vec3 to = {0.0, 0.0, 0.249};
    const int frames = 126;
    const std::vector<PD::Key> keys = {KeyAt(0, PD::Ease::Linear, "eye", from),
                                       KeyAt(frames, PD::Ease::Linear, "eye", to)};
    for (int elapsed = 0; elapsed <= frames + 40; elapsed++) {
        const float legacy = LegacyFactor(false, elapsed, frames, 0.0F);
        PE::TweenValue sampled;
        REQUIRE(PE::SampleKeys(keys, "eye", elapsed, PE::TweenValue{}, sampled));
        const float expected = (float)from[2] + (((float)to[2] - (float)from[2]) * legacy);
        REQUIRE(sampled.vector[2] == expected);
    }
}

TEST_CASE("the sine_deg ease reproduces the unnormalised sine ApplyRamps computes") {
    const PD::Vec3 from = {-0.1, 0.0, -1.0};
    const PD::Vec3 to = {-0.1, 0.0, -0.25};
    const int frames = 35;
    const double rate = 3.0;
    PD::Key first = KeyAt(0, PD::Ease::SineDeg, "position", from);
    first.rate_deg = rate;
    const std::vector<PD::Key> keys = {std::move(first),
                                       KeyAt(frames, PD::Ease::Linear, "position", to)};
    for (int elapsed = 0; elapsed <= frames + 40; elapsed++) {
        const float legacy = LegacyFactor(true, elapsed, frames, (float)rate);
        PE::TweenValue sampled;
        REQUIRE(PE::SampleKeys(keys, "position", elapsed, PE::TweenValue{}, sampled));
        const float expected = (float)from[2] + (((float)to[2] - (float)from[2]) * legacy);
        REQUIRE(sampled.vector[2] == expected);
    }
    PE::TweenValue settled;
    REQUIRE(PE::SampleKeys(keys, "position", frames + 10, PE::TweenValue{}, settled));
    REQUIRE(settled.vector[2] > -0.28F);
    REQUIRE(settled.vector[2] < -0.27F);
}

TEST_CASE("a value first named at a later key blends from the value under the tween") {
    auto document = std::make_shared<PD::Document>(AlphaBlendDocument());
    PE::Evaluator evaluator;
    evaluator.Load(document, Preset::AssetLengths{});

    REQUIRE(evaluator.Resolve(0).models.front().alpha == 0.25F);
    const float middle = evaluator.Resolve(25).models.front().alpha;
    REQUIRE(middle > 0.624F);
    REQUIRE(middle < 0.626F);
    REQUIRE(evaluator.Resolve(50).models.front().alpha == 1.0F);
    REQUIRE(evaluator.Resolve(80).models.front().alpha == 1.0F);
}
