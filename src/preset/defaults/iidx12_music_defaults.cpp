#include "preset/defaults/defaults_build.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <bit>
#include <string>
#include <utility>
#include <vector>

namespace Preset::Doc {

using namespace Build;

namespace {

constexpr int kMusicSelectFrames = 1800;
constexpr float kEyeX = std::bit_cast<float>(0xBD982ECBU);
constexpr float kEyeY = std::bit_cast<float>(0x3D007AAFU);
constexpr float kEyeZ = std::bit_cast<float>(0xBF238EF3U);
constexpr float kUpX = -0.17364818F;
constexpr float kUpY = 0.98480775F;

Gate WhenStage(std::string choice) {
    return Gate{.option = "stage", .choices = {std::move(choice)}};
}

Clip SkyModel(std::string id, std::string asset, ModelBlend blend, double alpha, double speed) {
    return Clip{.id = std::move(id),
                .end = kMusicSelectFrames,
                .when = WhenStage("NORMAL"),
                .command = ModelDraw{.asset = std::move(asset),
                                     .blend_mode = blend,
                                     .alpha = alpha,
                                     .anim_speed = speed,
                                     .clip_time = {.clock = ClipClock::Restart}}};
}

Clip ExtraModel() {
    return Clip{.id = "extra_bg_extra_stage",
                .end = kMusicSelectFrames,
                .when = WhenStage("EXTRA"),
                .command = ModelDraw{.asset = "extra_st",
                                     .blend_mode = ModelBlend::Additive,
                                     .alpha = 1.0,
                                     .anim_speed = 2.0,
                                     .clip_time = {.clock = ClipClock::Restart}}};
}

Clip NormalCamera() {
    return Clip{.id = "camera_normal_stage",
                .end = kMusicSelectFrames,
                .when = WhenStage("NORMAL"),
                .command = CameraSet{.eye = Widen(kEyeX, kEyeY, kEyeZ),
                                     .at = Vec3{1.559, 0.0, 1.12},
                                     .up = Widen(kUpX, kUpY, 0.0F)}};
}

Clip ExtraCamera() {
    return Clip{.id = "camera_extra_stage",
                .end = kMusicSelectFrames,
                .when = WhenStage("EXTRA"),
                .command = CameraSet{.eye = Vec3{-0.2, -0.2, -0.2},
                                     .at = Vec3{1.0, 0.78, 1.0},
                                     .up = Vec3{0.0, 100.0, 0.0}}};
}

std::vector<Clip> SceneClips() {
    return {Clip{.id = "fog_normal_stage",
                 .end = kMusicSelectFrames,
                 .when = WhenStage("NORMAL"),
                 .command = FogCmd{.enabled = true,
                                   .color = {1.0, 1.0, 1.0},
                                   .start = 55.0,
                                   .end = 62.4,
                                   .density = 0.5}},
            Clip{.id = "fog_extra_stage",
                 .end = kMusicSelectFrames,
                 .when = WhenStage("EXTRA"),
                 .command = FogCmd{.enabled = false}},
            Clip{.id = "clear_normal_stage",
                 .end = kMusicSelectFrames,
                 .when = WhenStage("NORMAL"),
                 .command = RenderSettingsCmd{.clear_color = Vec3{1.0, 1.0, 1.0}}},
            Clip{.id = "clear_extra_stage",
                 .end = kMusicSelectFrames,
                 .when = WhenStage("EXTRA"),
                 .command = RenderSettingsCmd{.clear_color = Vec3{0.0, 0.0, 0.0}}},
            Clip{.id = "clear_cycle_extra_stage",
                 .end = kMusicSelectFrames,
                 .when = WhenStage("EXTRA"),
                 .command = ClearCycleCmd{.base = Vec3{0.0, 0.0, 0.0},
                                          .strobe_color = Vec3{48.0, 48.0, 48.0},
                                          .strobe_period = 600,
                                          .strobe_window_a = 25,
                                          .strobe_window_b_offset = 300,
                                          .strobe_window_b = 15,
                                          .strobe_skip_every = 3,
                                          .ramp_period = 800,
                                          .ramp_length = 300,
                                          .ramp_peak = 128}}};
}

OptionSpec StageOption() {
    return OptionSpec{.id = "stage",
                      .label = "Stage",
                      .transition = {.frames = 0, .step = 4},
                      .choices = {ChoiceSpec{.label = "NORMAL"}, ChoiceSpec{.label = "EXTRA"}}};
}

}

Document Iidx12MusicSelect() {
    return Document{
        .id = "iidx12-music-select",
        .name = "Music select",
        .build = "iidx12",
        .length = kMusicSelectFrames,
        .render = RenderSpec{.clear_color = {1.0, 1.0, 1.0}},
        .camera = WideLensAt(Widen(kEyeX, kEyeY, kEyeZ), Vec3{1.559, 0.0, 1.12}),
        .lights = {LightSpec{
            .direction = {1.0, 1.0, 1.0}, .specular = {0.0, 0.0, 0.0}, .ambient = {1.0, 1.0, 1.0}}},
        .assets = {Scene3dAsset("sky", "data/graph/model/sky"),
                   Scene3dAsset("extra_st", "data/graph/model/extra_st")},
        .options = {StageOption()},
        .markers = {Marker{.frame = 0, .label = "Song list"}},
        .tracks = {
            ModelTrack("sky", "sky",
                       {SkyModel("sky_normal_stage", "sky", ModelBlend::Opaque, 1.0, 1.0)}),
            ModelTrack("muring", "muring",
                       {SkyModel("muring_normal_stage", "sky", ModelBlend::Additive, 0.2, 1.0)}),
            ModelTrack("extra_bg", "extra_bg", {ExtraModel()}),
            CameraTrack("camera", {NormalCamera(), ExtraCamera()}),
            SceneTrack("scene", SceneClips()),
        }};
}

}
