#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/eval_particles.h"
#include "preset/eval/frame_report.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_rng.h"
#include "preset_push_text.h"

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
        if (left.ease.armed != right.ease.armed) return false;
        if (left.ease.scale != right.ease.scale) return false;
        if (left.ease.position != right.ease.position) return false;
        if (left.ease.alpha != right.ease.alpha) return false;
    }
    return true;
}

bool SameState(const PE::EvalState& a, const PE::EvalState& b) {
    if (a.frame != b.frame) return false;
    if (a.beat != b.beat || a.beat_since != b.beat_since) return false;
    if (a.jitter != b.jitter || a.pulse != b.pulse) return false;
    if (a.transition != b.transition || a.choices != b.choices) return false;
    if (a.sprite_clock != b.sprite_clock || a.sprite_start != b.sprite_start) return false;
    if (a.camera_ease.armed != b.camera_ease.armed) return false;
    if (a.camera_ease.eye != b.camera_ease.eye || a.camera_ease.at != b.camera_ease.at)
        return false;
    if (a.camera_motion.armed != b.camera_motion.armed) return false;
    if (a.camera_motion.up != b.camera_motion.up) return false;
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

namespace {

PD::Key ClearKey(int at, PD::Vec3 color) {
    return PD::Key{.at = at, .values = {PD::KeyValue{.id = "clear_color", .value = color}}};
}

PD::Document ClearCycleDocument() {
    PD::Document doc;
    doc.id = "clear-cycle";
    doc.name = "Clear cycle";
    doc.build = "iidx12";
    doc.length = 5401;
    PD::Track track;
    track.id = "scene";
    track.name = "scene";
    track.kind = PD::TrackKind::Scene;
    PD::Clip clip;
    clip.id = "clear_cycle";
    clip.start = 0;
    clip.end = 5401;
    clip.command = PD::RenderSettingsCmd{};
    clip.keys.push_back(ClearKey(0, {0.0, 0.0, 0.0}));
    clip.keys.push_back(ClearKey(2700, {96.0 / 255.0, 96.0 / 255.0, 24.0 / 255.0}));
    clip.keys.push_back(ClearKey(5400, {0.0, 0.0, 0.0}));
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));
    return doc;
}

std::vector<PE::Vec3f> ClearColors(const std::vector<PE::Push>& pushes) {
    std::vector<PE::Vec3f> out;
    for (const PE::Push& push : pushes) {
        if (push.call == PE::PushCall::SetClearColor) out.push_back(push.vec_a);
    }
    return out;
}

const PE::Push* LightsPush(const std::vector<PE::Push>& pushes) {
    for (const PE::Push& push : pushes) {
        if (push.call == PE::PushCall::SetLights) return &push;
    }
    return nullptr;
}

}

TEST_CASE("a render.settings clear_color key resolves and pushes SetClearColor once a frame") {
    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(ClearCycleDocument()), Lengths());

    const std::vector<PE::Vec3f> start = ClearColors(evaluator.Rebind());
    REQUIRE(start.size() == 1);
    CHECK(start[0] == PE::Vec3f{0.0F, 0.0F, 0.0F});

    const std::vector<PE::Vec3f> peak = ClearColors(evaluator.Seek(2700));
    REQUIRE(peak.size() == 1);
    CHECK(peak[0] == PE::Vec3f{96.0F / 255.0F, 96.0F / 255.0F, 24.0F / 255.0F});

    const std::vector<PE::Vec3f> quarter = ClearColors(evaluator.Seek(1350));
    REQUIRE(quarter.size() == 1);
    CHECK(quarter[0][2] == (24.0F / 255.0F) * 0.5F);

    CHECK(ClearColors(evaluator.RenderFrame(kFrameSeconds)).size() == 1);
    CHECK(evaluator.Resolve(2700).clear_color ==
          PE::Vec3f{96.0F / 255.0F, 96.0F / 255.0F, 24.0F / 255.0F});
}

TEST_CASE("a param.override of clear_color wins over the document clear colour") {
    PD::Document doc;
    doc.id = "clear-override";
    doc.name = "Clear override";
    doc.build = "iidx12";
    doc.length = 60;
    doc.render.clear_color = {1.0, 1.0, 1.0};
    PD::Track track;
    track.id = "scene";
    track.name = "scene";
    track.kind = PD::TrackKind::Scene;
    PD::Clip clip;
    clip.id = "clear_blue";
    clip.start = 10;
    clip.end = 20;
    clip.command = PD::ParamOverrideCmd{.id = "clear_color", .value = PD::Vec3{0.0, 0.0, 1.0}};
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));
    CHECK(PD::Validate(doc).empty());

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(std::move(doc)), Lengths());
    CHECK(evaluator.Resolve(9).clear_color == PE::Vec3f{1.0F, 1.0F, 1.0F});
    CHECK(evaluator.Resolve(10).clear_color == PE::Vec3f{0.0F, 0.0F, 1.0F});
    CHECK(evaluator.Resolve(19).clear_color == PE::Vec3f{0.0F, 0.0F, 1.0F});
    CHECK(evaluator.Resolve(20).clear_color == PE::Vec3f{1.0F, 1.0F, 1.0F});
}

TEST_CASE("a light carries an ambient channel from the document and from light.set") {
    PD::Document doc;
    doc.id = "light-ambient";
    doc.name = "Light ambient";
    doc.build = "iidx12";
    doc.length = 60;
    doc.lights.push_back(PD::LightSpec{.ambient = {0.25, 0.5, 0.75}});
    doc.lights.push_back(PD::LightSpec{});

    PD::Track lit;
    lit.id = "light1";
    lit.name = "light";
    lit.kind = PD::TrackKind::Light;
    PD::Clip set_ambient;
    set_ambient.id = "light1_set";
    set_ambient.start = 0;
    set_ambient.end = 60;
    set_ambient.command = PD::LightSet{.index = 1, .ambient = PD::Vec3{1.0, 1.0, 1.0}};
    lit.clips.push_back(std::move(set_ambient));
    doc.tracks.push_back(std::move(lit));

    PD::Track plain;
    plain.id = "light2";
    plain.name = "light";
    plain.kind = PD::TrackKind::Light;
    PD::Clip set_plain;
    set_plain.id = "light2_set";
    set_plain.start = 0;
    set_plain.end = 60;
    set_plain.command = PD::LightSet{.index = 2, .diffuse = PD::Vec3{1.0, 0.0, 0.0}};
    plain.clips.push_back(std::move(set_plain));
    doc.tracks.push_back(std::move(plain));

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(std::move(doc)), Lengths());
    const std::vector<PE::Push> pushes = evaluator.Rebind();
    const PE::Push* lights = LightsPush(pushes);
    REQUIRE(lights != nullptr);
    REQUIRE(lights->lights.size() == 3);
    CHECK(lights->lights[0].ambient == PE::Vec3f{0.25F, 0.5F, 0.75F});
    CHECK(lights->lights[1].ambient == PE::Vec3f{1.0F, 1.0F, 1.0F});
    CHECK(lights->lights[2].ambient == PE::Vec3f{0.0F, 0.0F, 0.0F});
}

TEST_CASE("a param.override of light[0].ambient validates and reaches the lights push") {
    PD::Document doc;
    doc.id = "light-ambient-override";
    doc.name = "Light ambient override";
    doc.build = "iidx12";
    doc.length = 60;
    doc.lights.push_back(PD::LightSpec{});
    PD::Track track;
    track.id = "scene";
    track.name = "scene";
    track.kind = PD::TrackKind::Scene;
    PD::Clip clip;
    clip.id = "ambient_grey";
    clip.start = 0;
    clip.end = 60;
    clip.command = PD::ParamOverrideCmd{.id = "light[0].ambient", .value = PD::Vec3{0.5, 0.5, 0.5}};
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));
    CHECK(PD::Validate(doc).empty());

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(std::move(doc)), Lengths());
    CHECK(evaluator.Resolve(0).lights.at(0).ambient == PE::Vec3f{0.5F, 0.5F, 0.5F});
    const std::vector<PE::Push> pushes = evaluator.Rebind();
    const PE::Push* lights = LightsPush(pushes);
    REQUIRE(lights != nullptr);
    REQUIRE(lights->lights.size() == 1);
    CHECK(lights->lights[0].ambient == PE::Vec3f{0.5F, 0.5F, 0.5F});
}

namespace {

PD::Document SceneDocument(const std::string& tracks, int length) {
    const std::string text = std::string(R"({
  "schema": "573renderer/scene-preset",
  "version": 1,
  "id": "scene-doc",
  "name": "Scene doc",
  "build": "iidx12",
  "fps": 60,
  "length": )") + std::to_string(length) +
                             R"(,
  "render": {"width": 640, "height": 480, "opaque": true, "shading": "lit_material",
             "sprite_split_priority": 30},
  "camera": {"eye": [0, 0, -1], "at": [0, 0, 0], "up": [0, 1, 0], "fov_y": 1.0471976,
             "near_z": 0.1, "far_z": 500.0, "aspect": "auto"},
  "lights": [], "assets": {}, "options": [], "rng_seed": 1, "markers": [],
  "tracks": )" + tracks + "\n}";
    const PD::Loaded loaded = PD::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    REQUIRE(PD::Validate(*loaded).empty());
    return *loaded;
}

PE::Vec3f Grey(int level) {
    const auto channel = (float)level / 255.0F;
    return PE::Vec3f{channel, channel, channel};
}

std::string PushLine(const std::vector<PE::Push>& pushes, std::string_view needle) {
    for (const PE::Push& push : pushes) {
        const std::string text = PresetPushText::Format(push, false);
        if (text.find(needle) != std::string::npos) return text;
    }
    return {};
}

const PE::ReportValue* ReportRow(const PE::FrameReport& report, std::string_view field) {
    for (const PE::ReportEntity& entity : report.entities) {
        for (const PE::ReportValue& value : entity.values) {
            if (value.field == field) return &value;
        }
    }
    return nullptr;
}

}

TEST_CASE("render.clear_cycle reproduces the extra stage strobe and ramp frame by frame") {
    const PD::Document doc = SceneDocument(
        R"([{"id": "scene", "kind": "scene", "clips": [
            {"id": "clear_black", "type": "render.settings", "start": 0, "end": 2401,
             "params": {"clear_color": [1.0, 1.0, 1.0]}},
            {"id": "strobe", "type": "render.clear_cycle", "start": 0, "end": 2401}]}])",
        2401);

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());

    const std::vector<std::pair<int, int>> expected = {
        {0, 128},  {1, 127},  {2, 127},  {3, 126}, {24, 117}, {25, 117},
        {299, 0},  {300, 0},  {314, 48}, {315, 0}, {599, 0},  {600, 0},
        {601, 48}, {623, 48}, {624, 0},  {625, 0}, {799, 0},  {800, 128}};
    for (const auto& [frame, level] : expected) {
        INFO("frame " << frame);
        CHECK(evaluator.Resolve(frame).clear_color == Grey(level));
    }
}

TEST_CASE("scene.fog resolves into the frame report and a SetFog push every frame") {
    const PD::Document doc = SceneDocument(
        R"([{"id": "scene", "kind": "scene", "clips": [
            {"id": "fog_on", "type": "scene.fog", "start": 0, "end": 60,
             "params": {"color": [1.0, 1.0, 1.0], "start": 55.0, "end": 62.5,
                        "density": 0.5}}]}])",
        120);

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());

    const std::string on = PushLine(evaluator.Rebind(), "Scene3dHost::SetFog");
    INFO("frame 0 fog push");
    CHECK(on == "Scene3dHost::SetFog(fog[true [1 1 1] 55 62.5 0.5])");

    const std::string off = PushLine(evaluator.Seek(60), "Scene3dHost::SetFog");
    INFO("frame 60 fog push");
    CHECK(off == "Scene3dHost::SetFog(fog[false [1 1 1] 0 1 0.5])");

    const PE::FrameReport lit = PE::BuildFrameReport(doc, evaluator.Resolve(0), evaluator.State());
    const PE::ReportValue* enabled = ReportRow(lit, "fog");
    REQUIRE(enabled != nullptr);
    CHECK(enabled->kind == PE::ReportKind::Boolean);
    CHECK(enabled->integer == 1);
    CHECK(enabled->clip == "fog_on");
    const PE::ReportValue* start = ReportRow(lit, "fog start");
    REQUIRE(start != nullptr);
    CHECK(start->scalar == 55.0F);
    const PE::ReportValue* end = ReportRow(lit, "fog end");
    REQUIRE(end != nullptr);
    CHECK(end->scalar == 62.5F);
    const PE::ReportValue* density = ReportRow(lit, "fog density");
    REQUIRE(density != nullptr);
    CHECK(density->scalar == 0.5F);

    const PE::FrameReport dark =
        PE::BuildFrameReport(doc, evaluator.Resolve(60), evaluator.State());
    REQUIRE(ReportRow(dark, "fog") != nullptr);
    CHECK(ReportRow(dark, "fog")->integer == 0);
    CHECK(ReportRow(dark, "fog start") == nullptr);
}

namespace {

PD::Document EaseDocument(const std::string& tracks, const std::string& options, int length) {
    const std::string text = std::string(R"({
  "schema": "573renderer/scene-preset",
  "version": 1,
  "id": "ease-doc",
  "name": "Ease doc",
  "build": "iidx12",
  "fps": 60,
  "length": )") + std::to_string(length) +
                             R"(,
  "render": {"width": 640, "height": 480, "opaque": true, "shading": "lit_material",
             "sprite_split_priority": 30},
  "camera": {"eye": [0, 0, -1], "at": [0, 0, 0], "up": [0, 1, 0], "fov_y": 1.0471976,
             "near_z": 0.1, "far_z": 500.0, "aspect": "auto"},
  "lights": [],
  "assets": {"scene": {"kind": "scene3d", "dir": "data/graph/model/red"},
             "pkg": {"kind": "package2d", "dir": "data/graph/sys/title"}},
  "options": )" + options +
                             R"(, "rng_seed": 1, "markers": [],
  "tracks": )" + tracks + "\n}";
    const PD::Loaded loaded = PD::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    REQUIRE(PD::Validate(*loaded).empty());
    return *loaded;
}

PE::CameraState CameraAt(const PD::Document& doc, int frame) {
    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());
    evaluator.Seek(frame);
    return evaluator.Current().camera;
}

PE::ModelSlot ModelAt(const PD::Document& doc, int frame) {
    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());
    evaluator.Seek(frame);
    return evaluator.Current().models.front();
}

std::string CameraTrackWith(const std::string& ease_params) {
    return R"([{"id": "camera", "kind": "camera", "clips": [
        {"id": "hold", "type": "camera.set", "start": 0, "end": 400,
         "params": {"eye": [5, 0, 0], "at": [7, 0, 0]}},
        {"id": "chase", "type": "camera.ease", "start": 0, "end": 400,
         "params": )" +
           ease_params + R"(}]}])";
}

}

TEST_CASE("camera.ease closes the gap geometrically and never moves a masked axis") {
    const std::string params =
        R"({"eye_target": [9, 1, -1], "at_target": [9, 2, 3], "rate": 0.05,
            "eye_x": false, "at_x": false, "at_z": false})";
    const PD::Document doc = EaseDocument(CameraTrackWith(params), "[]", 400);

    const PE::CameraState start = CameraAt(doc, 0);
    INFO("frame 0 holds the camera.set pose apart from one eased step");
    CHECK(start.eye[0] == 5.0F);
    CHECK(start.at[0] == 7.0F);
    CHECK(start.at[2] == 0.0F);
    CHECK(start.eye[1] == 0.05F);

    const PE::CameraState mid = CameraAt(doc, 30);
    CHECK(mid.eye[1] < 0.95F);
    CHECK(mid.eye[0] == 5.0F);

    const PE::CameraState settled = CameraAt(doc, 60);
    INFO("60 frames at 0.05 close 95% of the gap");
    CHECK(settled.eye[1] >= 0.95F);
    CHECK(settled.eye[2] <= -0.95F);
    CHECK(settled.at[1] >= 1.90F);
    INFO("the masked axes keep whatever camera.set wrote");
    CHECK(settled.eye[0] == 5.0F);
    CHECK(settled.at[0] == 7.0F);
    CHECK(settled.at[2] == 0.0F);

    const std::string fast =
        R"({"eye_target": [9, 1, -1], "at_target": [9, 2, 3], "rate": 0.15,
            "eye_x": false, "at_x": false, "at_z": false})";
    const PD::Document quick = EaseDocument(CameraTrackWith(fast), "[]", 400);
    INFO("19 frames at 0.15 close the same 95%");
    CHECK(CameraAt(quick, 19).eye[1] >= 0.95F);
    CHECK(CameraAt(quick, 12).eye[1] < 0.95F);
}

TEST_CASE("model.ease seeds at its target and its linear mode stops on the target") {
    const std::string tracks = R"([{"id": "m", "kind": "model", "target": "core", "clips": [
        {"id": "draw", "type": "model.draw", "start": 0, "end": 400,
         "params": {"asset": "scene", "alpha": 0.0}},
        {"id": "grow", "type": "model.ease", "start": 0, "end": 400,
         "params": {"scale_target": [3.5, 3.5, 3.5], "position_target": [0, -0.5, 0],
                    "rate": 0.1, "start_at_target": true}}]}])";
    const PD::Document doc = EaseDocument(tracks, "[]", 400);

    const PE::ModelSlot seeded = ModelAt(doc, 0);
    INFO("start_at_target means the first frame is already settled");
    CHECK(seeded.scale == PE::Vec3f{3.5F, 3.5F, 3.5F});
    CHECK(seeded.position == PE::Vec3f{0.0F, -0.5F, 0.0F});
    CHECK(ModelAt(doc, 200).scale == PE::Vec3f{3.5F, 3.5F, 3.5F});

    const std::string ramp = R"([{"id": "m", "kind": "model", "target": "core", "clips": [
        {"id": "draw", "type": "model.draw", "start": 0, "end": 400,
         "params": {"asset": "scene", "alpha": 0.0}},
        {"id": "lit", "type": "model.ease", "start": 0, "end": 400,
         "params": {"alpha_target": 0.6, "rate": 0.005, "mode": "linear"}}]}])";
    const PD::Document lit = EaseDocument(ramp, "[]", 400);
    INFO("a linear ramp of 0.005 per frame climbs to 0.6 and stops there");
    CHECK(ModelAt(lit, 0).alpha == 0.005F);
    CHECK(ModelAt(lit, 100).alpha < 0.6F);
    CHECK(ModelAt(lit, 200).alpha == 0.6F);
    CHECK(ModelAt(lit, 399).alpha == 0.6F);
}

TEST_CASE("a gated ease that takes over continues from the current value with no snap") {
    const std::string options =
        R"([{"id": "grade", "label": "Grade", "default_choice": 0, "transition": {"frames": 0, "step": 4, "ease": "linear", "spin_kick": 0},
             "choices": [{"label": "NEAR", "values": {}}, {"label": "FAR", "values": {}}]}])";
    const std::string tracks = R"([{"id": "camera", "kind": "camera", "clips": [
        {"id": "hold", "type": "camera.set", "start": 0, "end": 400,
         "params": {"eye": [0, 0, 0], "at": [0, 0, 0]}},
        {"id": "near", "type": "camera.ease", "start": 0, "end": 400,
         "when": {"option": "grade", "choice": "NEAR"},
         "params": {"eye_target": [0, 1, 0], "at_target": [0, 0, 0], "rate": 0.05}},
        {"id": "far", "type": "camera.ease", "start": 0, "end": 400,
         "when": {"option": "grade", "choice": "FAR"},
         "params": {"eye_target": [0, -1, 0], "at_target": [0, 0, 0], "rate": 0.05}}]}])";
    const PD::Document doc = EaseDocument(tracks, options, 400);

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());
    evaluator.Seek(40);
    const float before = evaluator.Current().camera.eye[1];
    CHECK(before > 0.5F);
    CHECK(before < 1.0F);

    evaluator.SetOption(0, 1);
    INFO("the choice change alone must not move the eased value");
    CHECK(evaluator.Current().camera.eye[1] == before);

    evaluator.RenderFrame(kFrameSeconds);
    const float after = evaluator.Current().camera.eye[1];
    INFO("the next frame walks back toward the new target from where it was");
    CHECK(after < before);
    CHECK(after > before - 0.15F);
}

TEST_CASE("seeking an eased document reproduces the state advancing to it leaves behind") {
    const std::string tracks = R"([{"id": "camera", "kind": "camera", "clips": [
        {"id": "hold", "type": "camera.set", "start": 0, "end": 900,
         "params": {"eye": [0, 0, 0], "at": [0, 0, 0]}},
        {"id": "chase", "type": "camera.ease", "start": 0, "end": 900,
         "params": {"eye_target": [0, 1, 0], "at_target": [0, 2, 0], "rate": 0.05}}]},
       {"id": "m", "kind": "model", "target": "core", "clips": [
        {"id": "draw", "type": "model.draw", "start": 0, "end": 900,
         "params": {"asset": "scene"}},
        {"id": "grow", "type": "model.ease", "start": 0, "end": 900,
         "params": {"scale_target": [3.5, 3.5, 3.5], "rate": 0.1}}]}])";
    const PD::Document doc = EaseDocument(tracks, "[]", 900);

    PE::Evaluator stepped;
    stepped.Load(std::make_shared<PD::Document>(doc), Lengths());
    for (int i = 0; i < 700; i++)
        stepped.RenderFrame(kFrameSeconds);

    PE::Evaluator sought;
    sought.Load(std::make_shared<PD::Document>(doc), Lengths());
    sought.Seek(700);

    CHECK(SameState(stepped.State(), sought.State()));
    CHECK(stepped.Current().camera.eye == sought.Current().camera.eye);
    CHECK(stepped.Current().models.front().scale == sought.Current().models.front().scale);
}

TEST_CASE("scrubbing back onto a checkpoint frame keeps the eased pose the checkpoint carried") {
    const std::string tracks = R"([{"id": "camera", "kind": "camera", "clips": [
        {"id": "hold", "type": "camera.set", "start": 0, "end": 900,
         "params": {"eye": [0, 0, 0], "at": [0, 0, 0]}},
        {"id": "chase", "type": "camera.ease", "start": 0, "end": 900,
         "params": {"eye_target": [0, 1, 0], "at_target": [0, 2, 0], "rate": 0.05}}]},
       {"id": "m", "kind": "model", "target": "core", "clips": [
        {"id": "draw", "type": "model.draw", "start": 0, "end": 900,
         "params": {"asset": "scene"}},
        {"id": "grow", "type": "model.ease", "start": 0, "end": 900,
         "params": {"scale_target": [3.5, 3.5, 3.5], "rate": 0.1}}]}])";
    const PD::Document doc = EaseDocument(tracks, "[]", 900);

    PE::Evaluator forward;
    forward.Load(std::make_shared<PD::Document>(doc), Lengths());
    forward.Seek(256);

    PE::Evaluator scrubbed;
    scrubbed.Load(std::make_shared<PD::Document>(doc), Lengths());
    scrubbed.Seek(300);
    scrubbed.Seek(256);

    INFO("the restored checkpoint must carry the eased camera, not the camera.set pose");
    CHECK(scrubbed.Current().camera.eye[1] > 0.9F);
    CHECK(scrubbed.Current().camera.eye == forward.Current().camera.eye);
    INFO("and the eased model scale, not the model.draw scale");
    CHECK(scrubbed.Current().models.front().scale[0] > 3.0F);
    CHECK(scrubbed.Current().models.front().scale == forward.Current().models.front().scale);
}

TEST_CASE("the rising burst emitter draws its period every frame and rises from the bottom") {
    const std::string tracks = R"([{"id": "fx", "kind": "fx", "clips": [
        {"id": "bubbles", "type": "emitter", "start": 0, "end": 400,
         "params": {"asset": "pkg", "cell": "AWA1", "spawn": "burst", "count": 3,
                    "priority": 27, "blend": "additive", "life_base": 150, "life_span": 100,
                    "burst": {"period_base": 10, "period_span": 5, "life_drift": 30,
                              "rise_base": 40, "rise_step": 10, "rise_period": 60,
                              "span_x": 640, "from_y": 480, "to_y": -20}}}]}])";
    const PD::Document doc = EaseDocument(tracks, "[]", 400);

    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());
    const Preset::Ran3 seeded = evaluator.State().rng;
    evaluator.RenderFrame(kFrameSeconds);
    const int first = DrawsBetween(seeded, evaluator.State().rng);
    INFO("two draws pick the period, then two per particle when the burst fires");
    CHECK((first == 2 || first == 8));

    evaluator.Seek(120);
    const std::vector<PE::Particle>& live = evaluator.State().particles;
    REQUIRE_FALSE(live.empty());
    for (const PE::Particle& particle : live) {
        INFO("every bubble rises straight up from y 480 and keeps its x");
        CHECK(particle.cell == "AWA1");
        CHECK(particle.from_y == 480);
        CHECK(particle.to_y < 0);
        CHECK(particle.from_x == particle.to_x);
        CHECK(particle.from_x >= 0);
        CHECK(particle.from_x < 640);
        CHECK(particle.priority == 27);
    }
}
