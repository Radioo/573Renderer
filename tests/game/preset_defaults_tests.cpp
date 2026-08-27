#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "preset/defaults/defaults.h"
#include "preset/defaults/defaults_build.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "preset/preset_tools.h"
#include "preset/preset_asset_lengths.h"
#include "preset/eval/eval_particles.h"
#include "preset/eval/eval_poly.h"
#include "preset/eval/eval_push.h"
#include "preset_push_text.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace PD = Preset::Doc;

const PD::Document* Find(const std::vector<PD::Document>& documents, std::string_view id) {
    for (const PD::Document& document : documents) {
        if (document.id == id) return &document;
    }
    return nullptr;
}

std::string Describe(const std::vector<PD::Problem>& problems) {
    std::string out;
    for (const PD::Problem& problem : problems) {
        if (problem.severity != PD::Severity::Error) continue;
        out += "\n  error " + problem.path + ": " + problem.message;
    }
    return out;
}

}

TEST_CASE("every built-in document round trips through JSON byte for byte") {
    for (const PD::Document& document : PD::BuiltIns()) {
        const std::string text = PD::Save(document);
        const PD::Loaded loaded = PD::Load(text);
        INFO(document.id);
        REQUIRE(loaded.has_value());
        REQUIRE(*loaded == document);
        REQUIRE(PD::Save(*loaded) == text);
    }
}

TEST_CASE("the attract built-in carries the five phase markers") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* attract = Find(built_ins, "iidx11-attract");
    REQUIRE(attract != nullptr);
    REQUIRE(attract->markers.size() == 5);
    CHECK(attract->markers[0].frame == 0);
    CHECK(attract->markers[1].frame == 502);
    CHECK(attract->markers[2].frame == 793);
    CHECK(attract->markers[3].frame == 902);
    CHECK(attract->markers[4].frame == 1736);
    REQUIRE(attract->length.has_value());
    CHECK(*attract->length == 2456);
}

TEST_CASE("built-in documents carry unique ids and validate without an error") {
    std::set<std::string> seen;
    for (const PD::Document& document : PD::BuiltIns()) {
        INFO(document.id);
        REQUIRE_FALSE(document.id.empty());
        REQUIRE_FALSE(document.build.empty());
        REQUIRE(seen.insert(document.build + "/" + document.id).second);
        const std::vector<PD::Problem> problems = PD::Validate(document);
        INFO(Describe(problems));
        bool failed = false;
        for (const PD::Problem& problem : problems)
            failed = failed || problem.severity == PD::Severity::Error;
        REQUIRE_FALSE(failed);
    }
}

TEST_CASE("the defaults dump writes every built-in document with no window or device") {
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "r573_dump_defaults_test";
    std::filesystem::remove_all(out);
    REQUIRE(PresetTools::DumpDefaults(out.string()) == 0);

    std::size_t written = 0;
    for (const std::filesystem::directory_entry& build : std::filesystem::directory_iterator(out)) {
        REQUIRE(build.is_directory());
        for (const std::filesystem::directory_entry& file :
             std::filesystem::directory_iterator(build.path())) {
            INFO(file.path().string());
            const std::ifstream stream(file.path(), std::ios::binary);
            REQUIRE(stream.good());
            std::ostringstream text;
            text << stream.rdbuf();
            const PD::Loaded loaded = PD::Load(text.str());
            REQUIRE(loaded.has_value());
            CHECK(loaded->build == build.path().filename().string());
            CHECK(loaded->id == file.path().stem().string());
            written++;
        }
    }
    CHECK(written == PD::BuiltIns().size());
    std::filesystem::remove_all(out);
}

TEST_CASE("the defaults dump refuses to write into an unnamed directory") {
    CHECK(PresetTools::DumpDefaults("") == 2);
}

TEST_CASE("every IIDX 10 and RED built-in carries the game's white ambient and black specular") {
    int checked = 0;
    for (const PD::Document& document : PD::BuiltIns()) {
        if (document.build != "iidx10" && document.build != "iidx11") continue;
        INFO(document.build << "/" << document.id);
        REQUIRE(document.lights.size() == 2);
        CHECK(document.lights[0].direction == PD::Vec3{1.0, 1.0, 1.0});
        CHECK(document.lights[1].direction == PD::Vec3{-1.0, -1.0, -1.0});
        for (const PD::LightSpec& light : document.lights) {
            CHECK(light.diffuse == PD::Vec3{1.0, 1.0, 1.0});
            INFO("the title update writes ambient white and nothing ever writes specular");
            CHECK(light.ambient == PD::Vec3{1.0, 1.0, 1.0});
            CHECK(light.specular == PD::Vec3{0.0, 0.0, 0.0});
        }
        checked++;
    }
    CHECK(checked == 18);
}

TEST_CASE("the ending built-in merges both halves into 18 markers and unique clip ids") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx11-ending");
    REQUIRE(ending != nullptr);
    REQUIRE(ending->markers.size() == 18);
    int previous = -1;
    for (const PD::Marker& marker : ending->markers) {
        INFO(marker.label);
        REQUIRE(marker.frame > previous);
        previous = marker.frame;
    }
    std::set<std::string> clip_ids;
    std::set<std::string> track_ids;
    for (const PD::Track& track : ending->tracks) {
        INFO(track.id);
        REQUIRE(track_ids.insert(track.id).second);
        for (const PD::Clip& clip : track.clips) {
            INFO(clip.id);
            REQUIRE(clip_ids.insert(clip.id).second);
        }
    }
}

TEST_CASE(
    "the HAPPY SKY expert select built-in carries the additive sky over the breathing clear") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* expert = Find(built_ins, "iidx12-expert-select");
    REQUIRE(expert != nullptr);
    CHECK(expert->build == "iidx12");
    CHECK(expert->length == 2760);
    CHECK(Describe(PD::Validate(*expert)).empty());
    REQUIRE(expert->markers.size() == 1);
    CHECK(expert->markers[0].frame == 0);

    int models = 0;
    int sprites = 0;
    const PD::RenderSettingsCmd* clear = nullptr;
    const PD::Clip* clear_clip = nullptr;
    for (const PD::Track& track : expert->tracks) {
        for (const PD::Clip& clip : track.clips) {
            if (const auto* draw = std::get_if<PD::ModelDraw>(&clip.command)) {
                models++;
                CHECK(draw->blend_mode == PD::ModelBlend::Additive);
                CHECK(draw->alpha == 1.0);
                CHECK(draw->anim_speed == 1.0);
                CHECK(draw->clip_time.clock == PD::ClipClock::Restart);
            }
            if (std::holds_alternative<PD::SpriteAnimate>(clip.command) ||
                std::holds_alternative<PD::SpriteDraw>(clip.command))
                sprites++;
            if (const auto* settings = std::get_if<PD::RenderSettingsCmd>(&clip.command)) {
                clear = settings;
                clear_clip = &clip;
            }
        }
    }
    CHECK(models == 1);
    CHECK(sprites == 0);
    REQUIRE(expert->lights.size() == 2);
    for (const PD::LightSpec& light : expert->lights) {
        CHECK(light.ambient == PD::Vec3{1.0, 1.0, 1.0});
        CHECK(light.diffuse == PD::Vec3{1.0, 1.0, 1.0});
        CHECK(light.specular == PD::Vec3{0.0, 0.5, 1.0});
    }
    CHECK(expert->lights[0].direction == PD::Build::Widen(0.78F, -0.5F, 0.67F));
    CHECK(expert->lights[1].direction == PD::Vec3{0.0, 1.0, 0.0});

    REQUIRE(clear != nullptr);
    REQUIRE(clear_clip != nullptr);
    REQUIRE(clear->clear_color.has_value());
    CHECK(*clear->clear_color == PD::Vec3{0.0, 48.0 / 255.0, 96.0 / 255.0});
    REQUIRE(clear_clip->keys.size() >= 2);
    CHECK(clear_clip->keys[0].at == 0);
    CHECK(clear_clip->keys[1].at == 2700);
    REQUIRE(clear_clip->keys[1].values.size() == 1);
    CHECK(clear_clip->keys[1].values[0].id == "clear_color");
    CHECK(std::get<PD::Vec3>(clear_clip->keys[1].values[0].value) ==
          PD::Vec3{96.0 / 255.0, 0.0, 0.0});

    CHECK(expert->camera.eye == PD::Build::Widen(-0.2F, -0.2F, -0.2F));
    CHECK(expert->camera.at == PD::Build::Widen(1.0F, 0.78F, 1.0F));
    CHECK(expert->camera.up == PD::Vec3{0.0, 100.0, 0.0});
    CHECK(expert->camera.near_z == 0.0);
    CHECK(expert->camera.far_z == 1000.0);
    CHECK_FALSE(expert->camera.aspect.automatic);
    CHECK(expert->camera.aspect.value == PD::Build::Widen(1.7708334F));
    CHECK(expert->render.clear_color == PD::Vec3{0.0, 48.0 / 255.0, 96.0 / 255.0});
}

namespace {

PD::Vec3 ModeSelectEyeAt(int frame, bool eye) {
    const double t = std::min((double)frame * 0.025252523, 1.0);
    const double s = std::sin(t);
    const PD::Vec3 eye_from = PD::Build::Widen(0.0F, 0.3F, 0.0F);
    const PD::Vec3 eye_to = PD::Build::Widen(0.0F, std::bit_cast<float>(0x3CF7EBC8U),
                                             std::bit_cast<float>(0xBE8D0F2EU));
    const auto at_height = std::bit_cast<float>(0x3EDE3D44U);
    const PD::Vec3 at_from = PD::Build::Widen(0.0F, at_height, 0.0F);
    const PD::Vec3 at_to = PD::Build::Widen(0.0F, at_height, std::bit_cast<float>(0x3F66A04CU));
    PD::Vec3 out;
    for (std::size_t i = 0; i < 3; i++) {
        out[i] = eye ? (eye_from[i] * (1.0 - s)) + (eye_to[i] * s)
                     : (at_from[i] * (1.0 - t)) + (at_to[i] * t);
    }
    return out;
}

}

TEST_CASE("the HAPPY SKY mode select built-in flies its camera in on the game's sin(t) curve") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* mode = Find(built_ins, "iidx12-mode-select");
    REQUIRE(mode != nullptr);
    CHECK(mode->build == "iidx12");
    CHECK(mode->length == 1201);
    CHECK(Describe(PD::Validate(*mode)).empty());
    REQUIRE(mode->markers.size() == 2);
    CHECK(mode->markers[0].frame == 0);
    CHECK(mode->markers[1].frame == 40);
    CHECK(mode->render.clear_color == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(mode->camera.fov_y == PD::Build::Widen(-4.7528005F));
    CHECK(mode->camera.aspect.automatic);
    CHECK(mode->camera.near_z == PD::Build::Widen(0.1F));
    CHECK(mode->camera.far_z == 500.0);
    REQUIRE(mode->lights.size() == 1);
    CHECK(mode->lights[0].direction == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(mode->lights[0].diffuse == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(mode->lights[0].ambient == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(mode->lights[0].specular == PD::Vec3{0.0, 0.0, 0.0});

    int models = 0;
    for (const PD::Track& track : mode->tracks) {
        for (const PD::Clip& clip : track.clips) {
            if (const auto* draw = std::get_if<PD::ModelDraw>(&clip.command)) {
                models++;
                CHECK(draw->blend_mode == PD::ModelBlend::Opaque);
                CHECK(draw->alpha == 1.0);
                CHECK(draw->anim_speed == 1.0);
                CHECK(draw->clip_time.clock == PD::ClipClock::Restart);
            }
            CHECK_FALSE(std::holds_alternative<PD::SpriteAnimate>(clip.command));
        }
    }
    CHECK(models == 1);

    Preset::Eval::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(*mode), Preset::AssetLengths{});
    for (const int frame : {0, 1, 20, 39, 40, 41, 100, 1200}) {
        const Preset::Eval::CameraState camera = evaluator.Resolve(frame).camera;
        const PD::Vec3 eye = ModeSelectEyeAt(frame, true);
        const PD::Vec3 at = ModeSelectEyeAt(frame, false);
        INFO("frame " << frame);
        for (std::size_t i = 0; i < 3; i++) {
            CHECK(camera.eye[i] == Catch::Approx((float)eye[i]).margin(1e-6));
            CHECK(camera.at[i] == Catch::Approx((float)at[i]).margin(1e-6));
        }
        CHECK(camera.up == Preset::Eval::Vec3f{0.0F, 1.0F, 0.0F});
        CHECK(camera.fov_y == -4.7528005F);
    }
}

namespace {

std::string FogLine(const std::vector<Preset::Eval::Push>& pushes) {
    for (const Preset::Eval::Push& push : pushes) {
        const std::string text = PresetPushText::Format(push, false);
        if (text.find("Scene3dHost::SetFog") != std::string::npos) return text;
    }
    return {};
}

Preset::Eval::Vec3f Grey(int level) {
    const auto channel = (float)level / 255.0F;
    return Preset::Eval::Vec3f{channel, channel, channel};
}

int VisibleModels(const Preset::Eval::FrameState& state) {
    int visible = 0;
    for (const Preset::Eval::ModelSlot& slot : state.models)
        visible += slot.visible ? 1 : 0;
    return visible;
}

const Preset::Eval::ModelSlot* SlotNamed(const Preset::Eval::FrameState& state,
                                         std::string_view name) {
    for (const Preset::Eval::ModelSlot& slot : state.models) {
        if (slot.name == name) return &slot;
    }
    return nullptr;
}

}

TEST_CASE("the HAPPY SKY music select built-in swaps the whole scene with the stage option") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* music = Find(built_ins, "iidx12-music-select");
    REQUIRE(music != nullptr);
    CHECK(music->build == "iidx12");
    CHECK(music->length == 1800);
    CHECK(Describe(PD::Validate(*music)).empty());
    REQUIRE(music->markers.size() == 1);
    CHECK(music->markers[0].frame == 0);
    CHECK(music->markers[0].label == "Song list");
    REQUIRE(music->options.size() == 1);
    CHECK(music->options[0].id == "stage");
    CHECK(music->options[0].default_choice == 0);
    CHECK(music->options[0].transition.frames == 0);
    REQUIRE(music->options[0].choices.size() == 2);
    CHECK(music->options[0].choices[0].label == "NORMAL");
    CHECK(music->options[0].choices[1].label == "EXTRA");

    REQUIRE(music->lights.size() == 1);
    CHECK(music->lights[0].direction == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(music->lights[0].diffuse == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(music->lights[0].ambient == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(music->lights[0].specular == PD::Vec3{0.0, 0.0, 0.0});
    CHECK(music->camera.fov_y == PD::Build::Widen(1.0471976F));
    CHECK_FALSE(music->camera.aspect.automatic);
    CHECK(music->camera.aspect.value == PD::Build::Widen(1.7708334F));
    CHECK(music->camera.near_z == 0.0);
    CHECK(music->camera.far_z == 1000.0);

    int sprites = 0;
    for (const PD::Track& track : music->tracks) {
        for (const PD::Clip& clip : track.clips) {
            if (std::holds_alternative<PD::SpriteAnimate>(clip.command) ||
                std::holds_alternative<PD::SpriteDraw>(clip.command))
                sprites++;
        }
    }
    CHECK(sprites == 0);

    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/sky"] = 300.0F;
    lengths.scene_ticks["data/graph/model/extra_st"] = 640.0F;

    Preset::Eval::Evaluator normal;
    normal.Load(std::make_shared<PD::Document>(*music), lengths);
    for (const int frame : {0, 900}) {
        INFO("normal frame " << frame);
        const Preset::Eval::FrameState state = normal.Resolve(frame);
        CHECK(VisibleModels(state) == 2);
        REQUIRE(SlotNamed(state, "sky") != nullptr);
        CHECK(SlotNamed(state, "sky")->visible);
        CHECK(SlotNamed(state, "sky")->alpha == 1.0F);
        CHECK(SlotNamed(state, "sky")->blend_mode == (int)PD::ModelBlend::Opaque);
        REQUIRE(SlotNamed(state, "muring") != nullptr);
        CHECK(SlotNamed(state, "muring")->visible);
        CHECK(SlotNamed(state, "muring")->alpha == 0.2F);
        CHECK(SlotNamed(state, "muring")->blend_mode == (int)PD::ModelBlend::Additive);
        REQUIRE(SlotNamed(state, "extra_bg") != nullptr);
        CHECK_FALSE(SlotNamed(state, "extra_bg")->visible);
        CHECK(state.clear_color == Preset::Eval::Vec3f{1.0F, 1.0F, 1.0F});
        CHECK(state.camera.eye == Preset::Eval::Vec3f{std::bit_cast<float>(0xBD982ECBU),
                                                      std::bit_cast<float>(0x3D007AAFU),
                                                      std::bit_cast<float>(0xBF238EF3U)});
        CHECK(state.camera.at == Preset::Eval::Vec3f{1.559F, 0.0F, 1.12F});
        CHECK(state.camera.up == Preset::Eval::Vec3f{-0.17364818F, 0.98480775F, 0.0F});
    }
    CHECK(FogLine(normal.Rebind()) == "Scene3dHost::SetFog(fog[true [1 1 1] 55 62.4 0.5])");

    Preset::Eval::Evaluator extra;
    extra.Load(std::make_shared<PD::Document>(*music), lengths);
    extra.SetOption(0, 1);
    const std::vector<std::pair<int, int>> cycle = {{0, 128}, {1, 127}, {25, 117}, {300, 0}};
    for (const auto& [frame, level] : cycle) {
        INFO("extra frame " << frame);
        const Preset::Eval::FrameState state = extra.Resolve(frame);
        CHECK(VisibleModels(state) == 1);
        REQUIRE(SlotNamed(state, "extra_bg") != nullptr);
        CHECK(SlotNamed(state, "extra_bg")->visible);
        CHECK(SlotNamed(state, "extra_bg")->anim_speed == 2.0F);
        CHECK(SlotNamed(state, "extra_bg")->blend_mode == (int)PD::ModelBlend::Additive);
        REQUIRE(SlotNamed(state, "sky") != nullptr);
        CHECK_FALSE(SlotNamed(state, "sky")->visible);
        REQUIRE(SlotNamed(state, "muring") != nullptr);
        CHECK_FALSE(SlotNamed(state, "muring")->visible);
        CHECK(state.clear_color == Grey(level));
        CHECK(state.camera.eye == Preset::Eval::Vec3f{-0.2F, -0.2F, -0.2F});
        CHECK(state.camera.at == Preset::Eval::Vec3f{1.0F, 0.78F, 1.0F});
        CHECK(state.camera.up == Preset::Eval::Vec3f{0.0F, 100.0F, 0.0F});
    }
    CHECK(FogLine(extra.Rebind()) == "Scene3dHost::SetFog(fog[false [1 1 1] 0 1 0.5])");
}

namespace {

Preset::AssetLengths DanLengths() {
    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/dan"] = 1500.0F;
    lengths.animation_frames["data/graph/sys/dan_e"]["DAN_BG"] = 600;
    return lengths;
}

Preset::Eval::Evaluator DanAt(const PD::Document& doc, int choice, int frame) {
    PD::Document opened = doc;
    opened.options.front().default_choice = choice;
    Preset::Eval::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(std::move(opened)), DanLengths());
    evaluator.Seek(frame);
    return evaluator;
}

const Preset::Eval::ModelSlot* DanSlot(const Preset::Eval::FrameState& state,
                                       std::string_view name) {
    return SlotNamed(state, name);
}

struct BurstLog {
    std::string frames;
    float scale = 0.0F;
    int to_y = 0;
};

BurstLog DanBursts(const PD::Document& doc, int through) {
    Preset::Eval::Evaluator evaluator = DanAt(doc, 2, 0);
    BurstLog out;
    for (int frame = 1; frame <= through; frame++) {
        const std::size_t before = evaluator.State().particles.size();
        evaluator.RenderFrame(1.0F / 60.0F);
        const std::vector<Preset::Eval::Particle>& live = evaluator.State().particles;
        if (live.size() == before) continue;
        if (out.frames.empty()) {
            out.scale = live.front().scale;
            out.to_y = live.front().to_y;
        } else {
            out.frames += " ";
        }
        out.frames += std::to_string(frame);
    }
    return out;
}

}

TEST_CASE("the HAPPY SKY class course built-in poses six models with two sea instances") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* dan = Find(built_ins, "iidx12-dan-select");
    REQUIRE(dan != nullptr);
    CHECK(dan->build == "iidx12");
    CHECK(dan->length == 1260);
    CHECK(Describe(PD::Validate(*dan)).empty());
    CHECK(dan->render.clear_color == PD::Vec3{0.0, 0.0, 0.0});
    REQUIRE(dan->markers.size() == 2);
    CHECK(dan->markers[0].frame == 0);
    CHECK(dan->markers[0].label == "Enter");
    CHECK(dan->markers[1].frame == 21);
    CHECK(dan->markers[1].label == "Camera follows the grade");
    REQUIRE(dan->options.size() == 1);
    CHECK(dan->options[0].id == "grade");
    CHECK(dan->options[0].default_choice == 0);
    CHECK(dan->options[0].transition.frames == 0);
    REQUIRE(dan->options[0].choices.size() == 3);
    CHECK(dan->options[0].choices[0].label == "CLASS 7 to 1");
    CHECK(dan->options[0].choices[1].label == "1ST to 8TH DAN");
    CHECK(dan->options[0].choices[2].label == "9TH and 10TH DAN");

    REQUIRE(dan->lights.size() == 6);
    CHECK(dan->lights[0].direction == PD::Vec3{0.5, 0.0, 0.5});
    CHECK(dan->lights[1].direction == PD::Vec3{0.0, 1.0, 0.0});
    CHECK(dan->lights[2].direction == PD::Vec3{0.0, -1.0, 0.5});
    CHECK(dan->lights[5].direction ==
          PD::Vec3{PD::Build::Widen(std::bit_cast<float>(0xBF1EB852U)), 0.5,
                   PD::Build::Widen(std::bit_cast<float>(0x3F47AE14U))});
    for (const PD::LightSpec& light : dan->lights) {
        CHECK(light.diffuse == PD::Vec3{1.0, 1.0, 1.0});
        CHECK(light.ambient == PD::Vec3{1.0, 1.0, 1.0});
        CHECK(light.specular == PD::Vec3{0.0, 0.5, 1.0});
    }

    const Preset::Eval::FrameState first = DanAt(*dan, 0, 0).Current();
    CHECK(VisibleModels(first) == 6);
    REQUIRE(DanSlot(first, "dan_sea2_flip") != nullptr);
    CHECK(DanSlot(first, "dan_sea2_flip")->mesh == "dan_sea2");
    CHECK(DanSlot(first, "dan_sea2_flip")->rotation ==
          Preset::Eval::Vec3f{std::bit_cast<float>(0x4048F5C3U), 0.0F, 0.0F});
    CHECK(DanSlot(first, "dan_sea2_flip")->position ==
          Preset::Eval::Vec3f{0.0F, std::bit_cast<float>(0xBC23D70AU), 0.0F});
    CHECK(DanSlot(first, "dan_sea2_flip")->blend_mode == (int)PD::ModelBlend::Additive);
    REQUIRE(DanSlot(first, "dan_sea2") != nullptr);
    CHECK(DanSlot(first, "dan_sea2")->mesh == "dan_sea2");
    CHECK(DanSlot(first, "dan_sea2")->blend_mode == (int)PD::ModelBlend::Opaque);
    CHECK(DanSlot(first, "dan_sea2")->alpha == 1.0F);
    REQUIRE(DanSlot(first, "dan_sky2") != nullptr);
    CHECK(DanSlot(first, "dan_sky2")->anim_speed == std::bit_cast<float>(0x3E99999AU));
    REQUIRE(DanSlot(first, "dan_light_bg") != nullptr);
    CHECK(DanSlot(first, "dan_light_bg")->anim_speed == std::bit_cast<float>(0x3E4CCCCDU));

    REQUIRE(first.lights.size() == 6);
    for (std::size_t slot = 0; slot < first.lights.size(); slot++) {
        INFO("light slot " << slot);
        CHECK(first.lights[slot].enabled == (slot != 3 && slot != 4));
    }

    REQUIRE(first.sprites.size() == 1);
    CHECK(first.sprites[0].source == "DAN_BG");
    CHECK(first.sprites[0].priority == 30);
    CHECK(first.sprites[0].priority >= first.sprite_split_priority);
    CHECK(first.sprites[0].timing.playback == GcAnim::Playback::HoldLast);
}

TEST_CASE("the HAPPY SKY class course built-in seeds each grade settled and eases the camera") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* dan = Find(built_ins, "iidx12-dan-select");
    REQUIRE(dan != nullptr);

    const Preset::Eval::Vec3f init_eye = {0.0F, std::bit_cast<float>(0x3D75C28FU),
                                          std::bit_cast<float>(0xBEE66666U)};
    const Preset::Eval::Vec3f init_at = {0.0F, std::bit_cast<float>(0x3E23D70AU), 0.0F};
    const auto far_lift = std::bit_cast<float>(0xBDAAAAABU);

    for (int choice = 0; choice < 3; choice++) {
        INFO("choice " << choice);
        const Preset::Eval::FrameState settled = DanAt(*dan, choice, 0).Current();
        const float scale = (choice == 0) ? 1.0F : 3.5F;
        const float lift = (choice == 0) ? 0.0F : far_lift;
        CHECK(DanSlot(settled, "dan_sky")->scale == Preset::Eval::Vec3f{scale, scale, scale});
        CHECK(DanSlot(settled, "dan_sky")->position == Preset::Eval::Vec3f{0.0F, lift, 0.0F});
        CHECK(DanSlot(settled, "dan_sky2")->scale == Preset::Eval::Vec3f{scale, scale, scale});

        const Preset::Eval::FrameState held = DanAt(*dan, choice, 20).Current();
        INFO("the camera holds the entry pose until frame 21");
        CHECK(held.camera.eye == init_eye);
        CHECK(held.camera.at == init_at);

        const Preset::Eval::FrameState moving = DanAt(*dan, choice, 21).Current();
        CHECK(moving.camera.eye != init_eye);
        CHECK(moving.camera.eye[0] == 0.0F);
        CHECK(moving.camera.at[0] == 0.0F);
        CHECK(moving.camera.at[2] == 0.0F);
    }

    const Preset::Eval::FrameState klass = DanAt(*dan, 0, 200).Current();
    CHECK(klass.camera.eye[1] == Catch::Approx(std::bit_cast<float>(0x3D9BA5E3U)).margin(0.002));
    CHECK(klass.camera.eye[2] == Catch::Approx(-0.2F).margin(0.002));
    CHECK(klass.camera.at[1] == Catch::Approx(0.18F).margin(0.002));
    CHECK(DanSlot(klass, "dan_light_bg")->alpha == 0.0F);

    const Preset::Eval::FrameState grade = DanAt(*dan, 1, 200).Current();
    CHECK(grade.camera.eye[1] == Catch::Approx(0.55F).margin(0.002));
    CHECK(grade.camera.eye[2] == Catch::Approx(-0.45F).margin(0.002));
    CHECK(grade.camera.at[1] == Catch::Approx(0.0F).margin(0.002));
    CHECK(DanSlot(grade, "dan_light_bg")->alpha == 0.0F);

    const Preset::Eval::FrameState top = DanAt(*dan, 2, 200).Current();
    CHECK(top.camera.eye[1] == Catch::Approx(-0.05F).margin(0.002));
    CHECK(top.camera.eye[2] == Catch::Approx(-0.76F).margin(0.002));
    CHECK(top.camera.at[1] == Catch::Approx(-0.6F).margin(0.002));
    INFO("the light curtain climbs 0.005 per frame to 0.6 and the bubbles are live");
    CHECK(DanSlot(top, "dan_light_bg")->alpha == std::bit_cast<float>(0x3F19999AU));
    CHECK_FALSE(DanAt(*dan, 2, 200).State().particles.empty());
    CHECK(DanAt(*dan, 0, 200).State().particles.empty());
}

TEST_CASE("the HAPPY SKY bubbles fire on the seeded burst schedule and rise a shrinking step") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* dan = Find(built_ins, "iidx12-dan-select");
    REQUIRE(dan != nullptr);

    const BurstLog log = DanBursts(*dan, 180);
    INFO("seed 1 picks one period per frame and the half period fires between the whole ones");
    CHECK(log.frames == "70 72 80 85 90 91 104 110 119 120 125 126 130 132 140 144 145 150 161 "
                        "165 168 170 175 180");
    INFO("frame 70 rises 40 + 10 - (70 % 60), which is the scale in percent and the lift of to_y");
    CHECK(log.scale == Catch::Approx(0.4).margin(0.0001));
    CHECK(log.to_y == -60);
}

namespace {

Preset::AssetLengths EndingLengths() {
    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/sky"] = 300.0F;
    return lengths;
}

Preset::Eval::Evaluator EndingAt(const PD::Document& doc, int frame) {
    Preset::Eval::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), EndingLengths());
    evaluator.Seek(frame);
    return evaluator;
}

const Preset::Eval::Push* PolyPush(const std::vector<Preset::Eval::Push>& pushes) {
    const Preset::Eval::Push* found = nullptr;
    for (const Preset::Eval::Push& push : pushes) {
        if (push.call == Preset::Eval::PushCall::SetPolyGrid) found = &push;
    }
    return found;
}

}

TEST_CASE("the HAPPY SKY staff roll built-in holds a rolling camera over a fogged sky") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx12-ending");
    REQUIRE(ending != nullptr);
    CHECK(ending->build == "iidx12");
    CHECK(ending->length == 5112);
    CHECK(Describe(PD::Validate(*ending)).empty());
    CHECK(ending->render.clear_color == PD::Vec3{1.0, 1.0, 1.0});

    CHECK(ending->camera.eye == PD::Vec3{0.0, 0.0, 0.0});
    CHECK(ending->camera.at == PD::Vec3{0.0, 0.0, 1.0});
    CHECK(ending->camera.up == PD::Vec3{1.0, 0.0, 0.0});
    CHECK(ending->camera.fov_y == PD::Build::Widen(1.0471976F));
    CHECK(ending->camera.near_z == 0.0);
    CHECK(ending->camera.far_z == 1000.0);
    CHECK_FALSE(ending->camera.aspect.automatic);
    CHECK(ending->camera.aspect.value == PD::Build::Widen(1.7708334F));

    REQUIRE(ending->lights.size() == 1);
    CHECK(ending->lights[0].direction == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(ending->lights[0].diffuse == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(ending->lights[0].ambient == PD::Vec3{1.0, 1.0, 1.0});
    CHECK(ending->lights[0].specular == PD::Vec3{0.0, 0.0, 0.0});

    REQUIRE(ending->markers.size() == 5);
    CHECK(ending->markers[0].frame == 0);
    CHECK(ending->markers[0].label == "Title plate blooms in");
    CHECK(ending->markers[1].frame == 200);
    CHECK(ending->markers[1].label == "Sky and movie tiles");
    CHECK(ending->markers[2].frame == 300);
    CHECK(ending->markers[2].label == "Plate gone");
    CHECK(ending->markers[3].frame == 4833);
    CHECK(ending->markers[3].label == "Speed ramp and burst");
    CHECK(ending->markers[4].frame == 5077);
    CHECK(ending->markers[4].label == "Fade");

    const Preset::Eval::FrameState fogged = EndingAt(*ending, 0).Current();
    CHECK(fogged.fog.enabled);
    CHECK(fogged.fog.color == Preset::Eval::Vec3f{1.0F, 1.0F, 1.0F});
    CHECK(fogged.fog.start == 50.0F);
    CHECK(fogged.fog.end == 80.0F);
    CHECK(fogged.fog.density == std::bit_cast<float>(0x3DCCCCCDU));
    CHECK(EndingAt(*ending, 5111).Current().fog.enabled);
}

TEST_CASE("the HAPPY SKY staff roll shows the sky at 200 with its clock already run on") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx12-ending");
    REQUIRE(ending != nullptr);

    CHECK(VisibleModels(EndingAt(*ending, 0).Current()) == 0);
    CHECK(VisibleModels(EndingAt(*ending, 199).Current()) == 0);

    const Preset::Eval::Evaluator shown = EndingAt(*ending, 200);
    const Preset::Eval::FrameState& state = shown.Current();
    CHECK(VisibleModels(state) == 1);
    REQUIRE(SlotNamed(state, "sky") != nullptr);
    CHECK(SlotNamed(state, "sky")->visible);
    CHECK(SlotNamed(state, "sky")->alpha == 0.5F);
    CHECK(SlotNamed(state, "sky")->blend_mode == (int)PD::ModelBlend::Alpha);
    CHECK(SlotNamed(state, "sky")->anim_speed == 1.0F);
    CHECK(SlotNamed(state, "sky")->position == Preset::Eval::Vec3f{0.0F, 0.0F, 0.0F});
    CHECK(SlotNamed(state, "sky")->scale == Preset::Eval::Vec3f{1.0F, 1.0F, 1.0F});
    INFO("the dome's clock runs while it is hidden, exactly as the scene loop advances it");
    REQUIRE_FALSE(shown.State().models.empty());
    CHECK(shown.State().models.front().tick == 200.0F);
}

TEST_CASE("the HAPPY SKY staff roll ramps the dome speed over its last 279 frames") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx12-ending");
    REQUIRE(ending != nullptr);

    CHECK(SlotNamed(EndingAt(*ending, 4000).Current(), "sky")->anim_speed == 1.0F);
    CHECK(SlotNamed(EndingAt(*ending, 4832).Current(), "sky")->anim_speed == 1.0F);
    INFO("the first ramped frame already carries one 0.025 step");
    CHECK(SlotNamed(EndingAt(*ending, 4833).Current(), "sky")->anim_speed ==
          Catch::Approx(1.025F).margin(1e-4));
    CHECK(SlotNamed(EndingAt(*ending, 5111).Current(), "sky")->anim_speed ==
          Catch::Approx(7.975F).margin(1e-4));
}

TEST_CASE("the HAPPY SKY staff roll rolls its up vector 0.2 degrees a frame and never resets") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx12-ending");
    REQUIRE(ending != nullptr);

    const Preset::Eval::CameraState start = EndingAt(*ending, 0).Current().camera;
    CHECK(start.up == Preset::Eval::Vec3f{1.0F, 0.0F, 0.0F});

    const Preset::Eval::CameraState turned = EndingAt(*ending, 150).Current().camera;
    INFO("150 frames at 0.2 degrees is 30 degrees from +X toward -Y");
    CHECK(turned.up[0] == Catch::Approx(0.8660254F).margin(1e-5));
    CHECK(turned.up[1] == Catch::Approx(-0.5F).margin(1e-5));

    const Preset::Eval::CameraState last = EndingAt(*ending, 5111).Current().camera;
    INFO("5111 frames of 0.2 degrees is 1022.2 degrees, which is 2.84 turns");
    CHECK(last.up[0] == Catch::Approx(0.53338F).margin(2e-3));
    CHECK(last.up[1] == Catch::Approx(0.84588F).margin(2e-3));
    CHECK(last.eye == Preset::Eval::Vec3f{0.0F, 0.0F, 0.0F});
    CHECK(last.at == Preset::Eval::Vec3f{0.0F, 0.0F, 1.0F});
}

TEST_CASE("the HAPPY SKY staff roll pushes nine movie tiles from frame 200") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx12-ending");
    REQUIRE(ending != nullptr);

    Preset::Eval::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(*ending), EndingLengths());
    const std::vector<Preset::Eval::Push> before = evaluator.Seek(199);
    const Preset::Eval::Push* quiet = PolyPush(before);
    REQUIRE(quiet != nullptr);
    CHECK_FALSE(quiet->poly.active);
    CHECK(quiet->poly.tiles.empty());

    const std::vector<Preset::Eval::Push> shown = evaluator.Seek(200);
    const Preset::Eval::Push* live = PolyPush(shown);
    REQUIRE(live != nullptr);
    CHECK(live->poly.active);
    REQUIRE(live->poly.tiles.size() == 9);
    CHECK(live->poly.alpha == Catch::Approx(127.0F / 255.0F).margin(1e-6));
    CHECK(live->poly.movie == "data/movie/08ra.4");
    CHECK(live->poly.movie_width == 304);
    CHECK(live->poly.movie_height == 416);
    CHECK(live->poly.texture_side == 512);
    INFO("the movie plays against the wall clock, so frame 200 is 200 sixtieths of a second in");
    CHECK(live->poly.seconds == Catch::Approx(200.0F / 60.0F).margin(1e-5));

    const Preset::Eval::PolyGridState& grid = evaluator.Current().poly;
    REQUIRE(grid.quads.size() == 9);
    INFO("hand transcribed from the game's grid arithmetic at frame 200 with lattice seed 1");
    CHECK(grid.quads[0].corners[0][0] == Catch::Approx(-2.33472F).margin(2e-3));
    CHECK(grid.quads[0].corners[0][1] == Catch::Approx(0.52712F).margin(2e-3));
    CHECK(grid.quads[0].corners[0][2] == Catch::Approx(2.40187F).margin(2e-3));
}

TEST_CASE("the HAPPY SKY staff roll lets the lattice option pick another jitter") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx12-ending");
    REQUIRE(ending != nullptr);

    REQUIRE(ending->options.size() == 1);
    CHECK(ending->options[0].id == "lattice");
    CHECK(ending->options[0].default_choice == 0);
    REQUIRE(ending->options[0].choices.size() == 3);
    CHECK(ending->options[0].choices[0].label == "Seed 1");
    CHECK(ending->options[0].choices[1].label == "Seed 573");
    CHECK(ending->options[0].choices[2].label == "Seed 12345");

    std::vector<std::vector<Preset::Eval::PolyVec3f>> seen;
    for (int choice = 0; choice < 3; choice++) {
        PD::Document opened = *ending;
        opened.options.front().default_choice = choice;
        const Preset::Eval::PolyGridState grid = EndingAt(opened, 200).Current().poly;
        INFO("choice " << choice);
        REQUIRE(grid.quads.size() == 9);
        std::vector<Preset::Eval::PolyVec3f> corners;
        for (const Preset::Eval::PolyQuad& quad : grid.quads) {
            for (const Preset::Eval::PolyVec3f& corner : quad.corners)
                corners.push_back(corner);
        }
        seen.push_back(std::move(corners));
    }
    CHECK(seen[0] != seen[1]);
    CHECK(seen[0] != seen[2]);
    CHECK(seen[1] != seen[2]);
}
