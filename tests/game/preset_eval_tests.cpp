#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_particles.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_rng.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace PD = Preset::Doc;
namespace PE = Preset::Eval;

constexpr float kFrameSeconds = 1.0F / 60.0F;

Preset::AssetLengths Lengths() {
    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/red"] = 240.0F;
    lengths.scene_ticks["data/graph/texture/music"] = 240.0F;
    lengths.scene_ticks["data/graph/texture/cube_x"] = 60.0F;
    lengths.scene_ticks["data/graph/texture/ex01"] = 480.0F;
    lengths.scene_ticks["data/graph/texture/tranbox"] = 120.0F;
    lengths.scene_ticks["data/graph/texture/samurai"] = 60.0F;
    lengths.animation_frames["data/graph/sys/title"]["TITLE"] = 1736;
    lengths.animation_frames["data/graph/sys/title"]["TITLE_TAIKI"] = 720;
    return lengths;
}

std::shared_ptr<PD::Document> DocumentFor(std::string_view id) {
    for (PD::Document& document : PD::BuiltIns()) {
        if (document.id == id) return std::make_shared<PD::Document>(std::move(document));
    }
    FAIL("no preset " << id);
    return std::make_shared<PD::Document>();
}

struct Yaw {
    float current = 0.0F;
    float legacy = 0.0F;
};

Yaw YawAt(std::string_view preset, const std::string& model, int frame) {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor(preset), Lengths());
    Yaw out{.current = evaluator.Current().models.front().rotation[1],
            .legacy = evaluator.Current().models.front().rotation[1]};
    for (int step = 0; step < frame; step++) {
        for (const PE::Push& push : evaluator.RenderFrame(kFrameSeconds)) {
            if (push.call != PE::PushCall::SetModelTransform || push.name != model) continue;
            out.current = push.vec_b[1];
            out.legacy = push.legacy_vec_b[1];
        }
    }
    return out;
}

float RampRate(float from, float to, int frames, int elapsed) {
    const float t = (float)std::min(elapsed, frames) / (float)frames;
    return from + ((to - from) * t);
}

int DrawsBetween(Preset::Ran3 before, Preset::Ran3 after) {
    Preset::Ran3 probe = after;
    const int first = probe.Next();
    const int second = probe.Next();
    for (int draws = 0; draws <= 4096; draws++) {
        Preset::Ran3 candidate = before;
        for (int i = 0; i < draws; i++)
            candidate.Next();
        if (candidate.Next() == first && candidate.Next() == second) return draws;
    }
    return -1;
}

bool SameParticles(const PE::EvalState& a, const PE::EvalState& b) {
    if (a.particles.size() != b.particles.size()) return false;
    for (std::size_t i = 0; i < a.particles.size(); i++) {
        const PE::Particle& left = a.particles[i];
        const PE::Particle& right = b.particles[i];
        if (left.cell != right.cell || left.age != right.age) return false;
        if (left.life != right.life) return false;
        if (left.to_x != right.to_x || left.to_y != right.to_y) return false;
    }
    return true;
}

bool SameModels(const PE::EvalState& a, const PE::EvalState& b) {
    if (a.models.size() != b.models.size()) return false;
    for (std::size_t i = 0; i < a.models.size(); i++) {
        const PE::ModelRuntime& left = a.models[i];
        const PE::ModelRuntime& right = b.models[i];
        if (left.spin != right.spin || left.legacy_spin != right.legacy_spin) return false;
        if (left.kick != right.kick || left.tick != right.tick) return false;
        if (left.draw_start != right.draw_start) return false;
    }
    return true;
}

bool SameState(const PE::EvalState& a, const PE::EvalState& b) {
    if (a.frame != b.frame) return false;
    if (a.beat != b.beat || a.beat_since != b.beat_since) return false;
    if (a.jitter != b.jitter || a.pulse != b.pulse) return false;
    if (a.transition != b.transition || a.choices != b.choices) return false;
    if (a.sprite_clock != b.sprite_clock || a.sprite_start != b.sprite_start) return false;
    if (!SameParticles(a, b) || !SameModels(a, b)) return false;
    return DrawsBetween(a.rng, b.rng) == 0;
}

PD::Document WithEmitterMoved(const PD::Document& source, int start) {
    PD::Document out = source;
    for (PD::Track& track : out.tracks) {
        for (PD::Clip& clip : track.clips) {
            if (!std::holds_alternative<PD::EmitterCmd>(clip.command)) continue;
            clip.start = start;
            return out;
        }
    }
    FAIL("the ending document has no emitter clip");
    return out;
}

std::shared_ptr<PD::Document> OneClipDocument(std::optional<int> clip_end) {
    auto out = std::make_shared<PD::Document>();
    out->id = "clip-document";
    out->build = "iidx11";
    out->assets.push_back(
        PD::Asset{.id = "scene", .kind = PD::AssetKind::Scene3d, .dir = "data/graph/model/red"});
    PD::Track models;
    models.id = "core_track";
    models.kind = PD::TrackKind::Model;
    models.target = "core";
    PD::Clip draw;
    draw.id = "core_draw";
    draw.start = 0;
    draw.end = clip_end;
    draw.command = PD::ModelDraw{.asset = "scene", .model = "core", .anim_speed = 0.75};
    models.clips.push_back(std::move(draw));
    out->tracks.push_back(std::move(models));
    return out;
}

}

TEST_CASE("seek clamps to the frames the document has") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx10-game-over"), Lengths());

    evaluator.Seek(-5);
    CHECK(evaluator.State().frame == 0);

    evaluator.Seek(1000);
    CHECK(evaluator.State().frame == 179);
}

TEST_CASE("seek clamps an auto length document to the length it derives") {
    PE::Evaluator ended;
    ended.Load(OneClipDocument(90), Lengths());
    ended.Seek(1000);
    CHECK(ended.State().frame == 89);

    PE::Evaluator open;
    open.Load(OneClipDocument({}), Lengths());
    open.Seek(1000);
    CHECK(open.State().frame == 319);
}

TEST_CASE("the mode select spin ramps integrate the rate the phase resolves") {
    const Yaw start = YawAt("iidx11-mode-select", "core", 0);
    REQUIRE(start.current == 4.2F);

    float spin = 0.0F;
    for (int frame = 1; frame <= 14; frame++)
        spin += RampRate(0.12F, 0.064F, 15, frame);
    const Yaw fourteen = YawAt("iidx11-mode-select", "core", 14);
    REQUIRE(fourteen.current == 4.2F + spin);
    REQUIRE(fourteen.legacy == fourteen.current);

    const Yaw fifteen = YawAt("iidx11-mode-select", "core", 15);
    REQUIRE(fifteen.current == 4.2F);
    REQUIRE(fifteen.legacy == 4.2F + RampRate(0.30F, 0.06F, 13, 0));

    spin = 0.0F;
    for (int frame = 1; frame <= 12; frame++)
        spin += RampRate(0.30F, 0.06F, 13, frame);
    const Yaw twentyseven = YawAt("iidx11-mode-select", "core", 27);
    REQUIRE(twentyseven.current == 4.2F + spin);

    const Yaw twentyeight = YawAt("iidx11-mode-select", "core", 28);
    REQUIRE(twentyeight.current == 0.0F);
    REQUIRE(twentyeight.legacy == 0.04F);

    const Yaw forty = YawAt("iidx11-mode-select", "core", 40);
    REQUIRE(forty.current == 4.2F);
    REQUIRE(forty.legacy == 4.2F - 0.008F);
}

TEST_CASE("a hidden model keeps integrating its 3D tick and wraps as the host wraps it") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx11-attract"), Lengths());
    evaluator.Seek(320);
    REQUIRE(evaluator.State().models.front().tick == 240.0F);
    evaluator.Seek(321);
    REQUIRE(evaluator.State().models.front().tick == 0.0F);
    evaluator.Seek(502);
    REQUIRE(evaluator.State().models.front().tick == 0.75F * (float)(502 - 321));
}

TEST_CASE("seeking a frame reproduces the state advancing to it leaves behind") {
    PE::Evaluator seeked;
    seeked.Load(DocumentFor("iidx11-ending"), Lengths());
    seeked.Seek(700);

    PE::Evaluator stepped;
    stepped.Load(DocumentFor("iidx11-ending"), Lengths());
    for (int frame = 0; frame < 700; frame++)
        stepped.RenderFrame(kFrameSeconds);

    REQUIRE(seeked.State().frame == 700);
    REQUIRE(SameState(seeked.State(), stepped.State()));
}

TEST_CASE("replacing the document re-simulates every frame before the playhead") {
    const PD::Document edited = WithEmitterMoved(*DocumentFor("iidx11-ending"), 10);
    auto shared = std::make_shared<PD::Document>(edited);

    PE::Evaluator live;
    live.Load(DocumentFor("iidx11-ending"), Lengths());
    live.Seek(700);
    live.Load(shared, Lengths());
    live.Seek(700);

    PE::Evaluator fresh;
    fresh.Load(std::make_shared<PD::Document>(edited), Lengths());
    for (int frame = 0; frame < 700; frame++)
        fresh.RenderFrame(kFrameSeconds);

    REQUIRE(SameState(live.State(), fresh.State()));
}

TEST_CASE("the ending jitter windows draw exactly one random value per active frame") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx11-ending"), Lengths());
    const std::vector<std::pair<int, int>> expected = {{430, 0}, {431, 1}, {439, 1}, {440, 960},
                                                       {723, 0}, {724, 1}, {813, 1}, {814, 0}};
    int frame = 0;
    for (const auto& [at, draws] : expected) {
        while (frame < at - 1) {
            evaluator.RenderFrame(kFrameSeconds);
            frame++;
        }
        const Preset::Ran3 before = evaluator.State().rng;
        evaluator.RenderFrame(kFrameSeconds);
        frame++;
        INFO("frame " << at);
        REQUIRE(DrawsBetween(before, evaluator.State().rng) == draws);
    }
}

TEST_CASE("jitter set replaces the position and jitter add offsets it") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx11-ending"), Lengths());
    evaluator.Seek(430);
    const std::vector<PE::Push> pushes = evaluator.RenderFrame(kFrameSeconds);
    const float shake = evaluator.State().jitter;
    REQUIRE(shake != 0.0F);
    bool seen = false;
    for (const PE::Push& push : pushes) {
        if (push.call != PE::PushCall::SetModelTransform) continue;
        REQUIRE(push.vec_a[0] == shake);
        REQUIRE(push.vec_a[1] == shake);
        REQUIRE(push.vec_a[2] == 0.0F);
        seen = true;
    }
    REQUIRE(seen);

    PD::Document offset = *DocumentFor("iidx11-ending");
    for (PD::Track& track : offset.tracks) {
        for (PD::Clip& clip : track.clips) {
            if (auto* jitter = std::get_if<PD::RhythmJitter>(&clip.command))
                jitter->mode = PD::JitterMode::Add;
        }
    }
    for (PD::Track& track : offset.tracks) {
        for (PD::Clip& clip : track.clips) {
            auto* draw = std::get_if<PD::ModelDraw>(&clip.command);
            if (draw != nullptr) draw->position = PD::Vec3{1.0, 2.0, 3.0};
        }
    }
    PE::Evaluator added;
    added.Load(std::make_shared<PD::Document>(offset), Lengths());
    added.Seek(430);
    const std::vector<PE::Push> shifted = added.RenderFrame(kFrameSeconds);
    const float shake_add = added.State().jitter;
    bool checked = false;
    for (const PE::Push& push : shifted) {
        if (push.call != PE::PushCall::SetModelTransform) continue;
        REQUIRE(push.vec_a[0] == 1.0F + shake_add);
        REQUIRE(push.vec_a[1] == 2.0F + shake_add);
        REQUIRE(push.vec_a[2] == 3.0F);
        checked = true;
    }
    REQUIRE(checked);
}

TEST_CASE("a ramp longer than its phase is cut off, not rescaled into the phase") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx11-ending"), Lengths());
    const auto alpha = [&evaluator](int frame) {
        const PE::FrameState state = evaluator.Resolve(frame);
        for (const PE::ModelSlot& slot : state.models) {
            if (slot.name == "core") return slot.alpha;
        }
        FAIL("the ending document has no core model");
        return 0.0F;
    };
    const auto ramped = [](int elapsed) { return 1.0F - ((float)elapsed / 60.0F); };

    REQUIRE(alpha(2858) == ramped(0));
    REQUIRE(alpha(2859) == ramped(1));
    REQUIRE(alpha(2880) == ramped(22));
    REQUIRE(alpha(2903) == ramped(45));
    REQUIRE(alpha(2904) == 1.0F);
}

TEST_CASE("the converted countdown reproduces the speed, alpha and blend of the timer") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx10-music-select"), Lengths());
    const auto model = [&evaluator](int frame) { return evaluator.Resolve(frame).models.front(); };

    REQUIRE(model(1199).anim_speed == 0.25F);
    REQUIRE(model(1199).alpha == 1.0F);
    REQUIRE(model(1199).blend_mode == 0);
    REQUIRE(model(1200).anim_speed == 0.25F);
    REQUIRE(model(1200).blend_mode == 0);
    REQUIRE(model(1201).anim_speed == 0.2525F);
    REQUIRE(model(1201).alpha > 0.9993F);
    REQUIRE(model(1201).alpha < 0.99934F);
    REQUIRE(model(1201).blend_mode == 3);
    REQUIRE(std::abs(model(1500).anim_speed - 1.0F) < 1.0e-6F);
    REQUIRE(model(1500).alpha > 0.7999F);
    REQUIRE(model(1500).alpha < 0.8001F);
    REQUIRE(model(1799).anim_speed == 1.7475F);
    REQUIRE(model(1799).alpha > 0.6006F);
    REQUIRE(model(1799).alpha < 0.6007F);
}

TEST_CASE("the game over timer ramps from its first frame to the last frame that exists") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx10-game-over"), Lengths());
    const auto model = [&evaluator](int frame) { return evaluator.Resolve(frame).models.front(); };

    REQUIRE(model(0).anim_speed == 1.6F);
    REQUIRE(model(0).alpha == 0.8F);
    REQUIRE(model(0).blend_mode == 3);
    const float step = 1.6F / 180.0F;
    REQUIRE(model(1).anim_speed == 1.6F - step);
    REQUIRE(model(1).blend_mode == 3);
    REQUIRE(model(179).anim_speed > 0.0F);
    REQUIRE(model(179).anim_speed < 0.01F);
    REQUIRE(model(179).alpha > 0.0F);
    REQUIRE(model(179).alpha < 0.005F);
}

TEST_CASE("the intro speed keeps writing until the countdown ramp takes over") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx10-expert-select"), Lengths());
    const auto speed = [&evaluator](int frame) {
        return evaluator.Resolve(frame).models.front().anim_speed;
    };

    REQUIRE(speed(21) == -8.5F + (9.0F * (21.0F / 22.0F)));
    REQUIRE(speed(22) == 0.5F);
    REQUIRE(speed(1200) == 0.5F);
    REQUIRE(speed(1201) == 0.5F + 0.0041666667F);
}

TEST_CASE("the orbit position carries the same float bits on every machine") {
    PE::Evaluator evaluator;
    evaluator.Load(DocumentFor("iidx10-dan-select"), Lengths());
    std::array<float, 3> orbit{};
    for (int frame = 0; frame <= 732; frame++) {
        for (const PE::Push& push : evaluator.RenderFrame(kFrameSeconds)) {
            if (push.call != PE::PushCall::SetModelTransform || push.name != "cube_x") continue;
            orbit = push.vec_a;
        }
    }
    CHECK(std::bit_cast<std::uint32_t>(orbit[0]) == 0xbecb3db6U);
    CHECK(std::bit_cast<std::uint32_t>(orbit[1]) == 0x3d91ea10U);
    CHECK(std::bit_cast<std::uint32_t>(orbit[2]) == 0x3fb33333U);
}

TEST_CASE("the game's subtractive generator reproduces its own stream") {
    Preset::Ran3 rng;
    rng.Seed(1);
    std::vector<int> first;
    for (int i = 0; i < 512; i++) {
        const int draw = rng.Next();
        REQUIRE(draw >= 0);
        REQUIRE(draw < 1000000000);
        first.push_back(draw);
    }

    rng.Seed(1);
    for (const int expected : first)
        REQUIRE(rng.Next() == expected);

    rng.Seed(2);
    int same = 0;
    for (const int expected : first) {
        if (rng.Next() == expected) same++;
    }
    REQUIRE(same < 8);
}
