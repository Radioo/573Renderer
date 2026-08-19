#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset_host_stubs.h"

#include "backend/preset_clear_color.h"
#include "backend/preset_command_apply.h"
#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_host.h"
#include "gc2d/gc_sprite.h"
#include "preset/asset_index.h"
#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/frame_report.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_host.h"
#include "scene3d/atlas.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_merge.h"
#include "scene3d/scene3d_render.h"
#include "state/preset_commands.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <any>
#include <array>
#include <fstream>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <string_view>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

constexpr std::string_view kSceneDir = "data/graph/model/red";
constexpr std::string_view kPackageDir = "data/graph/sys/title";

std::string FixturePath() {
    return std::string(R573_FIXTURE_DIR) + "/asset_lengths.json";
}

Preset::AssetLengths AssetLengths() {
    const std::ifstream file(FixturePath(), std::ios::binary);
    REQUIRE(file.good());
    std::ostringstream text;
    text << file.rdbuf();
    const nlohmann::json doc = nlohmann::json::parse(text.str(), nullptr, false);
    REQUIRE_FALSE(doc.is_discarded());
    Preset::AssetLengths lengths;
    for (const auto& [dir, ticks] : doc["scene3d"].items())
        lengths.scene_ticks[dir] = ticks.get<float>();
    for (const auto& [dir, animations] : doc["package2d"].items()) {
        for (const auto& [name, frames] : animations.items())
            lengths.animation_frames[dir][name] = frames.get<int>();
    }
    return lengths;
}

void PrepareStub() {
    std::string err;
    REQUIRE(PresetStub::LoadAssetLengths(FixturePath(), err));
    PresetStub::Reset();
}

Doc::Document MakeDocument(int width, int height) {
    Doc::Document document;
    document.id = "host-test";
    document.name = "Host test";
    document.build = "iidx11";
    document.fps = 60;
    document.length = 120;
    document.render.width = width;
    document.render.height = height;
    document.assets.push_back(
        Doc::Asset{.id = "scene", .kind = Doc::AssetKind::Scene3d, .dir = std::string(kSceneDir)});
    document.assets.push_back(Doc::Asset{
        .id = "pkg", .kind = Doc::AssetKind::Package2d, .dir = std::string(kPackageDir)});

    Doc::Track models;
    models.id = "core_track";
    models.kind = Doc::TrackKind::Model;
    models.target = "core";
    Doc::Clip draw;
    draw.id = "core_draw";
    draw.start = 0;
    draw.command = Doc::ModelDraw{.asset = "scene", .model = "core", .anim_speed = 0.75};
    models.clips.push_back(std::move(draw));
    document.tracks.push_back(std::move(models));

    Doc::Track sprites;
    sprites.id = "bg_track";
    sprites.kind = Doc::TrackKind::Sprite;
    sprites.target = "BG";
    Doc::Clip animate;
    animate.id = "bg_anim";
    animate.start = 0;
    animate.command = Doc::SpriteAnimate{.asset = "pkg", .animation = "TITLE", .x = 0.0, .y = 0.0};
    sprites.clips.push_back(std::move(animate));
    document.tracks.push_back(std::move(sprites));
    return document;
}

bool Mentions(const std::vector<std::string>& calls, std::string_view needle) {
    return std::ranges::any_of(calls, [needle](const std::string& call) {
        return call.find(needle) != std::string::npos;
    });
}

float LastModelTime(const std::vector<std::string>& calls, const std::string& model) {
    const std::string prefix = "Scene3dHost::SetModelTime('" + model + "', ";
    float found = -1.0F;
    for (const std::string& call : calls) {
        if (!call.starts_with(prefix)) continue;
        found = std::stof(call.substr(prefix.size()));
    }
    return found;
}

}

TEST_CASE("the host pushes the document canvas into the 2D renderer", "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(1280, 720));
    REQUIRE(PresetHost::LoadDocument({}, document));

    const Gc2d::Canvas canvas = PresetStub::Canvas();
    CHECK(canvas.width == 1280);
    CHECK(canvas.height == 720);

    const std::array<float, 2> factors = Gc2d::ScaleFactors(canvas, 1280, 720);
    CHECK(factors[0] == Catch::Approx(1.0F));
    CHECK(factors[1] == Catch::Approx(1.0F));
    CHECK(Gc2d::PivotFor(canvas, 0.0F, 0.0F)[0] == Catch::Approx(640.0F));
    CHECK(Gc2d::PivotFor(canvas, 0.0F, 0.0F)[1] == Catch::Approx(360.0F));

    SysIdx::Package package;
    package.cells.push_back(SysIdx::Cell{.x = 0, .y = 0, .w = 16, .h = 16});
    package.cell_names["DOT"] = 0;
    std::vector<GcAnim::DrawNode> nodes;
    Gc2d::SpriteDraw dot;
    dot.name = "DOT";
    Gc2d::AppendNodes(package, dot, canvas, nodes);
    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].x * factors[0] == Catch::Approx(0.0F));

    PresetHost::Unload();
}

TEST_CASE("every built-in keeps the 640x480 canvas", "[preset][host]") {
    const std::vector<Doc::Document> documents = Doc::BuiltIns();
    REQUIRE(documents.size() == 25);
    for (const Doc::Document& document : documents) {
        INFO(document.id);
        CHECK(document.render.width == 640);
        CHECK(document.render.height == 480);
    }
}

TEST_CASE("the evaluator owns time: 120 host frames advance 60 document frames", "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));
    PresetStub::Take();

    Preset::Eval::Evaluator reference;
    reference.Load(document, AssetLengths());

    int mirrored = 0;
    for (int i = 0; i < 120; i++) {
        PresetHost::RenderFrame(1.0F / 120.0F);
        const std::vector<std::string> calls = PresetStub::Take();
        const int frame = PresetHost::GetStatus().frame;
        INFO("host frame " << i);
        CHECK(frame == (i + 1) / 2);
        while (mirrored < frame) {
            reference.AdvanceFrame();
            mirrored++;
        }
        CHECK(LastModelTime(calls, "core") == Catch::Approx(reference.State().models.front().tick));
        CHECK(Mentions(calls, "Scene3dHost::RenderFrame(0)"));
        CHECK_FALSE(Mentions(calls, "Gc2dHost::AdvanceSprites"));
    }
    CHECK(PresetHost::GetStatus().frame == 60);
    PresetHost::Unload();
}

TEST_CASE("the host pushes model alpha and the camera on every frame", "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));
    PresetStub::Take();

    for (int i = 0; i < 4; i++) {
        PresetHost::RenderFrame(1.0F / 60.0F);
        const std::vector<std::string> calls = PresetStub::Take();
        INFO("frame " << i);
        CHECK(Mentions(calls, "Scene3dHost::SetModelAlpha('core'"));
        CHECK(Mentions(calls, "Scene3dHost::SetView"));
        CHECK(Mentions(calls, "Scene3dHost::SetProjection"));
        CHECK(Mentions(calls, "Scene3dHost::SetModelTransform('core'"));
    }
    PresetHost::Unload();
}

TEST_CASE("the status snapshot carries an asset index with the asset names", "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    std::shared_ptr<const Preset::AssetIndex> index = PresetHost::GetAssetIndex();
    REQUIRE(index != nullptr);
    REQUIRE(index->assets.size() == 2);
    CHECK(index->assets[0].id == "scene");
    CHECK(index->assets[0].kind == Doc::AssetKind::Scene3d);
    CHECK(index->assets[0].loaded);
    CHECK(index->assets[0].models == std::vector<std::string>{"core"});
    CHECK(index->assets[1].id == "pkg");
    CHECK(index->assets[1].dir == kPackageDir);
    CHECK(index->assets[1].loaded);
    CHECK(index->assets[1].cells == std::vector<std::string>{"PTC"});
    REQUIRE(index->assets[1].animations.size() == 3);
    CHECK(index->assets[1].animations[0].name == "LOGO_IN");
    CHECK(index->assets[1].animations[1].name == "TITLE");
    CHECK(index->assets[1].animations[1].frames == 1736);
    CHECK(index->assets[1].animations[2].name == "TITLE_TAIKI");

    Doc::Document replaced = MakeDocument(640, 480);
    replaced.assets.erase(replaced.assets.begin() + 1);
    replaced.tracks.pop_back();
    PresetHost::ReplaceDocument(std::make_shared<const Doc::Document>(std::move(replaced)));
    PresetHost::RenderFrame(1.0F / 60.0F);

    index = PresetHost::GetAssetIndex();
    REQUIRE(index != nullptr);
    REQUIRE(index->assets.size() == 1);
    CHECK(index->assets[0].id == "scene");
    CHECK(index->assets[0].models == std::vector<std::string>{"core"});
    PresetHost::Unload();
}

TEST_CASE("the published asset index is one shared object, not a copy per frame",
          "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    const std::shared_ptr<const Preset::AssetIndex> first = PresetHost::GetAssetIndex();
    REQUIRE(first != nullptr);
    for (int frame = 0; frame < 8; frame++) {
        PresetHost::RenderFrame(1.0F / 60.0F);
        CHECK(PresetHost::GetAssetIndex().get() == first.get());
    }

    Doc::Document replaced = MakeDocument(640, 480);
    replaced.assets.erase(replaced.assets.begin() + 1);
    replaced.tracks.pop_back();
    PresetHost::ReplaceDocument(std::make_shared<const Doc::Document>(std::move(replaced)));
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetAssetIndex().get() != first.get());
    PresetHost::Unload();
}

TEST_CASE("seeking re-simulates and republishes the frame", "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    for (int i = 0; i < 30; i++)
        PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 30);

    PresetHost::Seek(10);
    PresetStub::Take();
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 11);

    PresetHost::SetPaused(true);
    PresetHost::RenderFrame(1.0F / 60.0F);
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 11);
    CHECK_FALSE(PresetHost::GetStatus().playing);

    PresetHost::SetPaused(false);
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 12);
    PresetHost::Unload();
}

TEST_CASE("a scroll clip carries its scroll offset into the 2D placement", "[preset][host]") {
    PrepareStub();
    Doc::Document scrolled = MakeDocument(640, 480);
    Doc::Clip scroll;
    scroll.id = "bg_scroll";
    scroll.start = 0;
    scroll.command =
        Doc::SpriteScroll{.scroll_x = 1.0, .scroll_wrap = 640.0, .scroll_offset = 100.0};
    scrolled.tracks.back().clips.push_back(std::move(scroll));
    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(scrolled)));

    const std::vector<Gc2dHost::SpritePlacement> placed = PresetStub::Sprites();
    REQUIRE(placed.size() == 1);
    CHECK(placed[0].scroll_x == Catch::Approx(1.0F));
    CHECK(placed[0].scroll_wrap == Catch::Approx(640.0F));
    CHECK(placed[0].scroll_offset == Catch::Approx(100.0F));

    Gc2d::SpriteDraw draw;
    draw.name = placed[0].name;
    draw.x = placed[0].x;
    draw.scroll_x = placed[0].scroll_x;
    draw.scroll_wrap = placed[0].scroll_wrap;
    draw.scroll_offset = placed[0].scroll_offset;
    CHECK(draw.x - Gc2d::ScrollOffset(draw) == Catch::Approx(-100.0F));
    PresetHost::Unload();
}

TEST_CASE("a light set to disabled reaches the 3D host disabled", "[preset][host]") {
    PrepareStub();
    Doc::Document lit = MakeDocument(640, 480);
    lit.lights.push_back(Doc::LightSpec{});
    lit.lights.push_back(Doc::LightSpec{});
    Doc::Track lights;
    lights.id = "light_track";
    lights.kind = Doc::TrackKind::Light;
    Doc::Clip off;
    off.id = "light_off";
    off.start = 0;
    off.command = Doc::LightSet{.index = 1, .enabled = false};
    lights.clips.push_back(std::move(off));
    lit.tracks.push_back(std::move(lights));
    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(lit)));

    const std::vector<Scene3d::Light> pushed = PresetStub::Lights();
    REQUIRE(pushed.size() == 2);
    CHECK(pushed[0].enabled);
    CHECK_FALSE(pushed[1].enabled);
    PresetHost::Unload();
}

TEST_CASE("the host clamps a seek to the frames that exist", "[preset][host]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    PresetHost::Seek(-5);
    PresetHost::SetPaused(true);
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 0);

    PresetHost::Seek(500);
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 119);
    PresetHost::Unload();
}

TEST_CASE("with the loop toggle off playback pauses on the last frame", "[preset][host][loop]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    PresetHost::SetLoop(false);
    PresetHost::Seek(117);
    for (int i = 0; i < 8; i++)
        PresetHost::RenderFrame(1.0F / 60.0F);

    CHECK(PresetHost::GetStatus().frame == 119);
    CHECK_FALSE(PresetHost::GetStatus().playing);
    CHECK_FALSE(PresetHost::GetStatus().loop);
    PresetHost::Unload();
}

TEST_CASE("with the loop toggle on the frame after the last one is zero", "[preset][host][loop]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    PresetHost::SetLoop(true);
    PresetHost::Seek(118);
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 119);
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 0);
    CHECK(PresetHost::GetStatus().playing);
    PresetHost::Unload();
}

TEST_CASE("every editor command reaches the host through the backend seam",
          "[preset][host][command]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));

    CHECK_FALSE(Backend::ApplyPresetCommand(std::any(42)));

    CHECK(Backend::ApplyPresetCommand(std::any(PresetCmd::Any(PresetCmd::Seek{.frame = 40}))));
    CHECK(Backend::ApplyPresetCommand(
        std::any(PresetCmd::Any(PresetCmd::SetPaused{.paused = true}))));
    CHECK(Backend::ApplyPresetCommand(std::any(PresetCmd::Any(PresetCmd::SetLoop{.loop = false}))));
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().frame == 40);
    CHECK_FALSE(PresetHost::GetStatus().playing);
    CHECK_FALSE(PresetHost::GetStatus().loop);

    Doc::Document longer = MakeDocument(640, 480);
    longer.length = 240;
    CHECK(Backend::ApplyPresetCommand(std::any(PresetCmd::Any(PresetCmd::ReplaceDocument{
        .document = std::make_shared<const Doc::Document>(std::move(longer))}))));
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetStatus().length == 240);
    CHECK(PresetHost::GetStatus().frame == 40);
    PresetHost::Unload();
}

TEST_CASE("SetOption through the backend seam switches a gated clip",
          "[preset][host][command][option]") {
    PrepareStub();
    Doc::Document gated = MakeDocument(640, 480);
    Doc::OptionSpec mode;
    mode.id = "mode";
    mode.label = "Mode";
    mode.choices.push_back(Doc::ChoiceSpec{.label = "NORMAL"});
    mode.choices.push_back(Doc::ChoiceSpec{.label = "ATTACK"});
    gated.options.push_back(std::move(mode));
    gated.tracks.front().clips.front().when =
        Doc::Gate{.option = "mode", .kind = Doc::GateKind::Choice, .choices = {"ATTACK"}};
    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(std::move(gated))));

    PresetHost::RenderFrame(1.0F / 60.0F);
    const std::vector<std::string> before = PresetStub::Take();
    CHECK(Mentions(before, "Scene3dHost::SetModelVisibleByName('core', false)"));
    CHECK_FALSE(Mentions(before, "Scene3dHost::SetModelVisibleByName('core', true)"));
    CHECK(PresetHost::GetStatus().option_choices == std::vector<int>{0});

    CHECK(Backend::ApplyPresetCommand(
        std::any(PresetCmd::Any(PresetCmd::SetOption{.option = 0, .choice = 1}))));
    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(Mentions(PresetStub::Take(), "Scene3dHost::SetModelVisibleByName('core', true)"));
    CHECK(PresetHost::GetStatus().option_choices == std::vector<int>{1});
    PresetHost::Unload();
}

TEST_CASE("replacing the document reports progress per asset it loads", "[preset][host]") {
    PrepareStub();
    std::vector<std::string> stages;
    const auto record = [&stages](const std::string& stage, float) { stages.push_back(stage); };
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document, record));
    CHECK_FALSE(stages.empty());

    stages.clear();
    Doc::Document swapped = MakeDocument(640, 480);
    swapped.assets.push_back(Doc::Asset{
        .id = "pkg2", .kind = Doc::AssetKind::Package2d, .dir = std::string(kPackageDir)});
    PresetHost::ReplaceDocument(std::make_shared<const Doc::Document>(std::move(swapped)), record);
    PresetHost::RenderFrame(1.0F / 60.0F);

    CHECK(Mentions(stages, "2D package pkg2"));
    CHECK(Mentions(stages, "3D scenes"));
    PresetHost::Unload();
}

TEST_CASE("merging two scene dirs offsets the second one's tile indices", "[preset][scene3d]") {
    Scene3d::Scene first;
    first.name = "red";
    first.max_time = 240.0F;
    first.bounds_min = {-1.0F, -1.0F, -1.0F};
    first.bounds_max = {1.0F, 1.0F, 1.0F};
    first.tiles.push_back(Scene3d::Tile{.name = "red_0", .width = 0, .height = 0, .bgra = {}});
    Scene3d::Model core;
    core.name = "core";
    core.chunks.push_back(Scene3d::DrawChunk{.frame = 0, .tile = 0, .vertices = {}, .indices = {}});
    first.models.push_back(std::move(core));

    Scene3d::Scene second;
    second.name = "cube";
    second.max_time = 60.0F;
    second.bounds_min = {-4.0F, 0.0F, 0.0F};
    second.bounds_max = {0.0F, 2.0F, 0.0F};
    second.camera_model = 0;
    second.camera_frame = 7;
    second.tiles.push_back(Scene3d::Tile{.name = "cube_0", .width = 0, .height = 0, .bgra = {}});
    second.tiles.push_back(Scene3d::Tile{.name = "cube_1", .width = 0, .height = 0, .bgra = {}});
    Scene3d::Model cube;
    cube.name = "cube";
    cube.chunks.push_back(Scene3d::DrawChunk{.frame = 0, .tile = 1, .vertices = {}, .indices = {}});
    cube.chunks.push_back(
        Scene3d::DrawChunk{.frame = 0, .tile = -1, .vertices = {}, .indices = {}});
    second.models.push_back(std::move(cube));

    Scene3d::Merge(first, std::move(second));
    REQUIRE(first.models.size() == 2);
    REQUIRE(first.tiles.size() == 3);
    CHECK(first.tiles[1].name == "cube_0");
    CHECK(first.models[1].chunks[0].tile == 2);
    CHECK(first.models[1].chunks[1].tile == -1);
    CHECK(first.max_time == Catch::Approx(240.0F));
    CHECK(first.camera_model == 1);
    CHECK(first.camera_frame == 7);
    CHECK(first.bounds_min[0] == Catch::Approx(-4.0F));
    CHECK(first.bounds_max[1] == Catch::Approx(2.0F));
}

TEST_CASE("an instance clones the mesh it names, never a stranger holding the same name",
          "[preset][host]") {
    Scene3d::Scene scene;
    Scene3d::Model core;
    core.name = "core";
    core.chunks.push_back(Scene3d::DrawChunk{.frame = 0, .tile = 0, .vertices = {}, .indices = {}});
    scene.models.push_back(std::move(core));
    Scene3d::Model stranger;
    stranger.name = "core_flip";
    stranger.chunks.push_back(
        Scene3d::DrawChunk{.frame = 0, .tile = 4, .vertices = {}, .indices = {}});
    scene.models.push_back(std::move(stranger));

    const std::vector<Scene3d::Instance> want = {
        Scene3d::Instance{.target = "core", .model = "core"},
        Scene3d::Instance{.target = "core_twin", .model = "core"},
        Scene3d::Instance{.target = "core_flip", .model = "core"},
        Scene3d::Instance{.target = "core_flip", .model = "core"},
        Scene3d::Instance{.target = "ghost", .model = "absent"}};
    const std::vector<Scene3d::InstanceProblem> problems =
        Scene3d::MakeInstances(scene.models, want);

    REQUIRE(scene.models.size() == 3);
    INFO("a target that names its own mesh clones nothing, a fresh target appends one clone");
    CHECK(scene.models[2].name == "core_twin");
    CHECK(scene.models[2].chunks[0].tile == 0);
    INFO("the instance carries the mesh it named, not the stranger that owned the name");
    CHECK(scene.models[1].name == "core_flip");
    CHECK(scene.models[1].chunks[0].tile == 0);

    REQUIRE(problems.size() == 2);
    INFO("the repeated target is cloned once, and both problems name their own instance");
    CHECK(problems[0].index == 2);
    CHECK_FALSE(problems[0].missing_model);
    CHECK(problems[1].index == 4);
    CHECK(problems[1].missing_model);
}

TEST_CASE("a capture at half the document fps advances two document frames per host frame",
          "[preset][host][export]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    REQUIRE(PresetHost::LoadDocument({}, document));
    PresetStub::Take();

    for (int i = 0; i < 10; i++) {
        PresetHost::RenderFrame(1.0F / 30.0F);
        INFO("host frame " << i);
        CHECK(PresetHost::GetStatus().frame == (i + 1) * 2);
    }
    PresetHost::Unload();
}

TEST_CASE("the frame report is built only while the frame inspector asks for it",
          "[preset][host][frame_inspector]") {
    PrepareStub();
    const auto document = std::make_shared<const Doc::Document>(MakeDocument(640, 480));
    PresetHost::SetFrameReportWanted(false);
    REQUIRE(PresetHost::LoadDocument({}, document));

    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::GetFrameReport() == nullptr);

    PresetHost::SetFrameReportWanted(true);
    PresetHost::RenderFrame(1.0F / 60.0F);
    const std::shared_ptr<const Preset::Eval::FrameReport> report = PresetHost::GetFrameReport();
    REQUIRE(report != nullptr);
    CHECK(report->frame == PresetHost::GetStatus().frame);

    PresetHost::SetFrameReportWanted(false);
    PresetHost::Unload();
}

TEST_CASE("the host publishes the document clear colour packed with a zero alpha",
          "[preset][host]") {
    PrepareStub();
    CHECK(PresetHost::ClearColor() == 0x00000000U);

    Doc::Document blue = MakeDocument(640, 480);
    blue.render.clear_color = {0.0, 48.0 / 255.0, 96.0 / 255.0};
    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(blue)));
    CHECK(PresetHost::ClearColor() == 0x00003060U);

    PresetHost::RenderFrame(1.0F / 60.0F);
    CHECK(PresetHost::ClearColor() == 0x00003060U);

    PresetHost::Unload();
    CHECK(PresetHost::ClearColor() == 0x00000000U);
}

TEST_CASE("the backend clears with the preset colour except in a transparent export",
          "[preset][host][export]") {
    const Backend::ClearInputs live{
        .preset_active = true, .exporting = false, .bg_transparent = true, .color = 0x00FFFFFFU};
    REQUIRE(Backend::FrameClearColor(live).has_value());
    CHECK(*Backend::FrameClearColor(live) == 0x00FFFFFFU);

    Backend::ClearInputs transparent = live;
    transparent.exporting = true;
    CHECK_FALSE(Backend::FrameClearColor(transparent).has_value());

    Backend::ClearInputs opaque_export = transparent;
    opaque_export.bg_transparent = false;
    REQUIRE(Backend::FrameClearColor(opaque_export).has_value());
    CHECK(*Backend::FrameClearColor(opaque_export) == 0x00FFFFFFU);

    Backend::ClearInputs no_preset = live;
    no_preset.preset_active = false;
    REQUIRE(Backend::FrameClearColor(no_preset).has_value());
    CHECK(*Backend::FrameClearColor(no_preset) == 0x00000000U);

    Backend::ClearInputs no_preset_transparent_export = transparent;
    no_preset_transparent_export.preset_active = false;
    CHECK_FALSE(Backend::FrameClearColor(no_preset_transparent_export).has_value());
}

TEST_CASE("the host pushes the music select fog state into the 3D renderer", "[preset][host]") {
    PrepareStub();
    const std::vector<Doc::Document> built_ins = Doc::BuiltIns();
    const Doc::Document* music = nullptr;
    for (const Doc::Document& document : built_ins) {
        if (document.id == "iidx12-music-select") music = &document;
    }
    REQUIRE(music != nullptr);
    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(*music)));
    PresetStub::Take();

    PresetHost::RenderFrame(1.0F / 60.0F);
    const std::vector<std::string> calls = PresetStub::Take();
    INFO(PresetStub::Error());
    CHECK(Mentions(calls, "Scene3dHost::SetFog(fog[true [1 1 1] 55 62.4 0.5])"));

    PresetHost::Unload();
}

TEST_CASE("two model tracks on one mesh draw two instances with their own transforms",
          "[preset][host]") {
    PrepareStub();
    Doc::Document document = MakeDocument(640, 480);
    Doc::Track twin;
    twin.id = "core_flip_track";
    twin.kind = Doc::TrackKind::Model;
    twin.target = "core_flip";
    Doc::Clip draw;
    draw.id = "core_flip_draw";
    draw.start = 0;
    draw.command = Doc::ModelDraw{.asset = "scene",
                                  .model = "core",
                                  .alpha = 0.5,
                                  .position = Doc::Vec3{0.0, -0.01, 0.0},
                                  .rotation = Doc::Vec3{3.14, 0.0, 0.0}};
    twin.clips.push_back(std::move(draw));
    document.tracks.push_back(std::move(twin));
    REQUIRE(Doc::Validate(document).empty());

    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(document)));
    const std::vector<std::string> boot = PresetStub::Take();
    INFO("the setup names both instances and points each at the same mesh");
    CHECK(Mentions(boot, "model[core core "));
    CHECK(Mentions(boot, "model[core_flip core "));

    PresetHost::RenderFrame(1.0F / 60.0F);
    const std::vector<std::string> calls = PresetStub::Take();
    INFO("each instance carries its own transform");
    CHECK(Mentions(calls, "Scene3dHost::SetModelTransform('core', [0 0 0]"));
    CHECK(Mentions(calls, "Scene3dHost::SetModelTransform('core_flip', [0 -0.01 0]"));
    CHECK(Mentions(calls, "Scene3dHost::SetModelAlpha('core_flip', 0.5)"));
    PresetHost::Unload();
}

TEST_CASE("the HAPPY SKY class course built-in loads its two sea instances", "[preset][host]") {
    PrepareStub();
    const std::vector<Doc::Document> documents = Doc::BuiltIns();
    const auto found = std::ranges::find_if(
        documents, [](const Doc::Document& doc) { return doc.id == "iidx12-dan-select"; });
    REQUIRE(found != documents.end());
    REQUIRE(PresetHost::LoadDocument({}, std::make_shared<const Doc::Document>(*found)));
    const std::vector<std::string> boot = PresetStub::Take();
    CHECK(Mentions(boot, "model[dan_sea2 dan_sea2 "));
    CHECK(Mentions(boot, "model[dan_sea2_flip dan_sea2 "));
    PresetHost::Unload();
}
