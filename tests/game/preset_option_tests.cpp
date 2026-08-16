#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace PD = Preset::Doc;
namespace PE = Preset::Eval;

constexpr float kFrameSeconds = 1.0F / 60.0F;

Preset::AssetLengths Lengths() {
    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/red"] = 240.0F;
    lengths.scene_ticks["data/graph/texture/cube_x"] = 60.0F;
    lengths.animation_frames["data/graph/sys/title"]["TITLE"] = 1736;
    return lengths;
}

std::shared_ptr<PD::Document> BuiltIn(const std::string& id) {
    for (const PD::Document& document : PD::BuiltIns()) {
        if (document.id == id) return std::make_shared<PD::Document>(document);
    }
    FAIL("no built-in document " << id);
    return {};
}

std::size_t ModelIndex(const PE::Evaluator& evaluator, const std::string& name) {
    const std::vector<PE::ModelSlot>& models = evaluator.Current().models;
    for (std::size_t i = 0; i < models.size(); i++) {
        if (models[i].name == name) return i;
    }
    FAIL("no model " << name);
    return 0;
}

void Advance(PE::Evaluator& evaluator, int frames) {
    for (int i = 0; i < frames; i++)
        evaluator.RenderFrame(kFrameSeconds);
}

PE::Vec3f PositionOf(const PE::Evaluator& evaluator, const std::string& name) {
    for (const PE::ModelSlot& slot : evaluator.Current().models) {
        if (slot.name == name) return slot.position;
    }
    FAIL("no model " << name);
    return {};
}

PD::Document SelectDocument(bool with_clip) {
    PD::Document document;
    document.id = "option-select-test";
    document.name = "Option select test";
    document.build = "iidx11";
    document.length = 1200;
    document.assets.push_back(
        PD::Asset{.id = "scene", .kind = PD::AssetKind::Scene3d, .dir = "data/model"});
    document.options.push_back(PD::OptionSpec{
        .id = "mode",
        .label = "Mode",
        .transition = {.frames = 100, .step = 4},
        .choices = {
            PD::ChoiceSpec{.label = "LEFT",
                           .values = {PD::ChoiceValue{.id = "model[core].position",
                                                      .value = PD::Vec3{0.0, 0.0, 0.0}}}},
            PD::ChoiceSpec{.label = "RIGHT",
                           .values = {PD::ChoiceValue{.id = "model[core].position",
                                                      .value = PD::Vec3{4.0, 0.0, 0.0}}}}}});

    PD::Track core;
    core.id = "core";
    core.name = "core";
    core.kind = PD::TrackKind::Model;
    core.target = "core";
    core.clips.push_back(PD::Clip{.id = "core_draw",
                                  .start = 0,
                                  .command = PD::ModelDraw{.asset = "scene", .model = "core"}});
    document.tracks.push_back(std::move(core));

    if (!with_clip) return document;
    PD::Track scene;
    scene.id = "scene_track";
    scene.name = "scene";
    scene.kind = PD::TrackKind::Scene;
    scene.clips.push_back(
        PD::Clip{.id = "to_right",
                 .start = 900,
                 .command = PD::OptionSelect{.option = "mode", .choice = "RIGHT"}});
    document.tracks.push_back(std::move(scene));
    return document;
}

}

TEST_CASE("a mode select choice change lerps by the transition spec and signs the kick",
          "[preset][options][transition]") {
    PE::Evaluator evaluator;
    evaluator.Load(BuiltIn("iidx10-mode-select"), Lengths());
    Advance(evaluator, 200);

    const std::size_t cube = ModelIndex(evaluator, "cube_x");
    const PE::Vec3f from = PositionOf(evaluator, "cube_x");
    const PE::Vec3f to = {0.0F, 0.0F, 3.0F};
    evaluator.SetOption(0, 3);
    CHECK(evaluator.State().models[cube].kick == Catch::Approx(15.0F));
    CHECK(evaluator.State().transition == 100);

    for (int step = 1; step <= 26; step++) {
        evaluator.RenderFrame(kFrameSeconds);
        const int left = 100 - (4 * step);
        const float factor = (left > 0) ? (1.0F - ((float)left / 100.0F)) : 1.0F;
        const PE::Vec3f now = PositionOf(evaluator, "cube_x");
        INFO("frame " << (200 + step) << " counter " << left);
        for (std::size_t axis = 0; axis < now.size(); axis++) {
            CHECK(now[axis] ==
                  Catch::Approx(from[axis] + ((to[axis] - from[axis]) * factor)).margin(1.0e-5));
        }
    }

    evaluator.SetOption(0, 0);
    CHECK(evaluator.State().models[cube].kick == Catch::Approx(-15.0F));
}

TEST_CASE("switching RED music select to ATTACK keeps the accumulated yaw continuous",
          "[preset][options][spin]") {
    PE::Evaluator evaluator;
    evaluator.Load(BuiltIn("iidx11-music-select"), Lengths());
    Advance(evaluator, 499);

    const std::size_t core = ModelIndex(evaluator, "core");
    const float normal_rate = 0.008726646F;
    const float attack_rate = 0.011635528F;
    const float spin_499 = evaluator.State().models[core].spin[1];
    CHECK(spin_499 == Catch::Approx((float)(499 - 35) * normal_rate).margin(1.0e-3));

    evaluator.SetOption(0, 1);
    CHECK(evaluator.State().models[core].spin[1] == Catch::Approx(spin_499));

    evaluator.RenderFrame(kFrameSeconds);
    const float spin_500 = evaluator.State().models[core].spin[1];
    evaluator.RenderFrame(kFrameSeconds);
    const float spin_501 = evaluator.State().models[core].spin[1];
    CHECK(spin_500 - spin_499 == Catch::Approx(attack_rate).margin(1.0e-6));
    CHECK(spin_501 - spin_500 == Catch::Approx(attack_rate).margin(1.0e-6));
}

TEST_CASE("an option.select clip fires the same change as clicking the choice, once",
          "[preset][options][select]") {
    PE::Evaluator clipped;
    clipped.Load(std::make_shared<PD::Document>(SelectDocument(true)), Lengths());
    PE::Evaluator clicked;
    clicked.Load(std::make_shared<PD::Document>(SelectDocument(false)), Lengths());

    Advance(clipped, 900);
    Advance(clicked, 900);
    clicked.SetOption(0, 1);
    CHECK(clipped.State().choices == clicked.State().choices);
    CHECK(clipped.State().transition == clicked.State().transition);
    CHECK(PositionOf(clipped, "core")[0] == Catch::Approx(PositionOf(clicked, "core")[0]));

    for (int frame = 901; frame <= 960; frame++) {
        clipped.RenderFrame(kFrameSeconds);
        clicked.RenderFrame(kFrameSeconds);
        INFO("frame " << frame);
        CHECK(clipped.State().choices == clicked.State().choices);
        CHECK(clipped.State().transition == clicked.State().transition);
        CHECK(PositionOf(clipped, "core")[0] == Catch::Approx(PositionOf(clicked, "core")[0]));
    }
    CHECK(clipped.State().choices == std::vector<int>{1});
}

TEST_CASE("an option.select clip fires again every time the playhead crosses it",
          "[preset][options][select]") {
    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(SelectDocument(true)), Lengths());

    evaluator.Seek(910);
    CHECK(evaluator.State().choices == std::vector<int>{1});
    CHECK(evaluator.State().transition == 60);

    evaluator.Seek(700);
    CHECK(evaluator.State().choices == std::vector<int>{0});
    CHECK(evaluator.State().transition == 0);

    evaluator.Seek(910);
    CHECK(evaluator.State().choices == std::vector<int>{1});
    CHECK(evaluator.State().transition == 60);

    evaluator.Seek(0);
    CHECK(evaluator.State().choices == std::vector<int>{0});
    CHECK(evaluator.State().transition == 0);

    evaluator.Seek(900);
    CHECK(evaluator.State().choices == std::vector<int>{1});
    CHECK(evaluator.State().transition == 100);
}
