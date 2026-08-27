#include "preset/defaults/defaults_build.h"

#include "formats/gcanim.h"

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

constexpr int kDanSelectFrames = 1260;
constexpr int kDanCameraFrame = 21;
constexpr int kDanParticleFrame = 59;

constexpr float kEyeY = std::bit_cast<float>(0x3D75C28FU);
constexpr float kEyeZ = std::bit_cast<float>(0xBEE66666U);
constexpr float kAtY = std::bit_cast<float>(0x3E23D70AU);
constexpr float kSkyRate = std::bit_cast<float>(0x3DCCCCCDU);
constexpr float kNearScale = 1.0F;
constexpr float kFarScale = 3.5F;
constexpr float kNearLift = 0.0F;
constexpr float kFarLift = std::bit_cast<float>(0xBDAAAAABU);
constexpr float kLightAlpha = std::bit_cast<float>(0x3F19999AU);
constexpr float kLightRise = std::bit_cast<float>(0x3BA3D70AU);
constexpr float kSeaAlpha = std::bit_cast<float>(0x3F4CCCCDU);
constexpr float kSkyTwoSpeed = std::bit_cast<float>(0x3E99999AU);
constexpr float kLightSpeed = std::bit_cast<float>(0x3E4CCCCDU);
constexpr float kSeaSink = std::bit_cast<float>(0xBC23D70AU);
constexpr float kSeaFlipRoll = std::bit_cast<float>(0x4048F5C3U);
constexpr float kLightDepth = std::bit_cast<float>(0xBF19999AU);
constexpr float kSlowRate = std::bit_cast<float>(0x3D4CCCCDU);
constexpr float kFastRate = std::bit_cast<float>(0x3E19999AU);
constexpr float kClassEyeY = std::bit_cast<float>(0x3D9BA5E3U);
constexpr float kClassEyeZ = std::bit_cast<float>(0xBE4CCCCDU);
constexpr float kClassAtY = std::bit_cast<float>(0x3E3851ECU);
constexpr float kDanEyeY = std::bit_cast<float>(0x3F0CCCCDU);
constexpr float kTopEyeY = std::bit_cast<float>(0xBD4CCCCDU);
constexpr float kTopEyeZ = std::bit_cast<float>(0xBF428F5CU);
constexpr float kTopAtY = std::bit_cast<float>(0xBF19999AU);
constexpr float kKeyDirX = std::bit_cast<float>(0xBF1EB852U);
constexpr float kKeyDirZ = std::bit_cast<float>(0x3F47AE14U);

const char* const kClassChoice = "CLASS 7 to 1";
const char* const kDanChoice = "1ST to 8TH DAN";
const char* const kTopChoice = "9TH and 10TH DAN";

Gate WhenGrade(std::string choice) {
    return Gate{.option = "grade", .choices = {std::move(choice)}};
}

Gate WhenNotGrade(std::string choice) {
    return Gate{.option = "grade", .kind = GateKind::Not, .choices = {std::move(choice)}};
}

Clip SeaModel(std::string id, std::string mesh, ModelBlend blend, double alpha, Vec3 position,
              Vec3 rotation) {
    return Clip{.id = std::move(id),
                .end = kDanSelectFrames,
                .command = ModelDraw{.asset = "dan",
                                     .model = std::move(mesh),
                                     .blend_mode = blend,
                                     .alpha = alpha,
                                     .anim_speed = 1.0,
                                     .position = position,
                                     .rotation = rotation,
                                     .clip_time = {.clock = ClipClock::Restart}}};
}

Clip SkyModel(std::string id, ModelBlend blend, double speed) {
    return Clip{.id = std::move(id),
                .end = kDanSelectFrames,
                .command = ModelDraw{.asset = "dan",
                                     .blend_mode = blend,
                                     .alpha = 1.0,
                                     .anim_speed = speed,
                                     .clip_time = {.clock = ClipClock::Restart}}};
}

Clip SkyEase(std::string id, Gate gate, float scale, float lift) {
    return Clip{.id = std::move(id),
                .end = kDanSelectFrames,
                .when = std::move(gate),
                .command = ModelEaseCmd{.scale_target = Widen(scale, scale, scale),
                                        .position_target = Widen(0.0F, lift, 0.0F),
                                        .rate = Widen(kSkyRate),
                                        .start_at_target = true}};
}

std::vector<Clip> SkyClips(std::string draw_id, ModelBlend blend, double speed, std::string near_id,
                           std::string far_id) {
    return {SkyModel(std::move(draw_id), blend, speed),
            SkyEase(std::move(near_id), WhenGrade(kClassChoice), kNearScale, kNearLift),
            SkyEase(std::move(far_id), WhenNotGrade(kClassChoice), kFarScale, kFarLift)};
}

std::vector<Clip> LightBackdropClips() {
    return {Clip{.id = "dan_light_bg_draw",
                 .end = kDanSelectFrames,
                 .command = ModelDraw{.asset = "dan",
                                      .blend_mode = ModelBlend::Additive,
                                      .alpha = 0.0,
                                      .anim_speed = Widen(kLightSpeed),
                                      .position = Widen(0.0F, kSeaSink, kLightDepth),
                                      .rotation = {1.0, 0.0, 0.0},
                                      .clip_time = {.clock = ClipClock::Restart}}},
            Clip{.id = "dan_light_bg_dark",
                 .end = kDanSelectFrames,
                 .when = WhenNotGrade(kTopChoice),
                 .command = ModelEaseCmd{.alpha_target = 0.0,
                                         .rate = Widen(kSkyRate),
                                         .mode = EaseMode::Linear}},
            Clip{.id = "dan_light_bg_lit",
                 .end = kDanSelectFrames,
                 .when = WhenGrade(kTopChoice),
                 .command = ModelEaseCmd{.alpha_target = Widen(kLightAlpha),
                                         .rate = Widen(kLightRise),
                                         .mode = EaseMode::Linear}}};
}

Clip CameraHold() {
    return Clip{.id = "camera_enter",
                .end = kDanSelectFrames,
                .command = CameraSet{.eye = Widen(0.0F, kEyeY, kEyeZ),
                                     .at = Widen(0.0F, kAtY, 0.0F),
                                     .up = Vec3{0.0, 1.0, 0.0},
                                     .fov_y = Widen(1.0471976F),
                                     .near_z = Widen(0.1F),
                                     .far_z = 500.0,
                                     .aspect = AspectSpec{}}};
}

Clip CameraEase(std::string id, Gate gate, Vec3 eye, double at_y, float rate) {
    return Clip{.id = std::move(id),
                .start = kDanCameraFrame,
                .end = kDanSelectFrames,
                .when = std::move(gate),
                .command = CameraEaseCmd{.eye_target = eye,
                                         .at_target = Vec3{0.0, at_y, 0.0},
                                         .rate = Widen(rate),
                                         .eye_x = false,
                                         .at_x = false,
                                         .at_z = false}};
}

std::vector<Clip> CameraClips() {
    return {CameraHold(),
            CameraEase("camera_class", WhenGrade(kClassChoice), Widen(0.0F, kClassEyeY, kClassEyeZ),
                       Widen(kClassAtY), kSlowRate),
            CameraEase("camera_dan", WhenGrade(kDanChoice), Widen(0.0F, kDanEyeY, kEyeZ), 0.0,
                       kSlowRate),
            CameraEase("camera_top", WhenGrade(kTopChoice), Widen(0.0F, kTopEyeY, kTopEyeZ),
                       Widen(kTopAtY), kFastRate)};
}

Clip BubbleEmitter() {
    return Clip{.id = "dan_bubbles",
                .start = kDanParticleFrame,
                .end = kDanSelectFrames,
                .when = WhenGrade(kTopChoice),
                .command = EmitterCmd{.asset = "system",
                                      .cell = "AWA1",
                                      .spawn = Spawn::Burst,
                                      .count = 3,
                                      .priority = 27,
                                      .blend = SpriteBlend::Additive,
                                      .life_base = 150,
                                      .life_span = 100,
                                      .burst = Burst{}}};
}

LightSpec DanLight(Vec3 direction) {
    return LightSpec{.direction = direction,
                     .diffuse = {1.0, 1.0, 1.0},
                     .specular = {0.0, 0.5, 1.0},
                     .ambient = {1.0, 1.0, 1.0}};
}

std::vector<LightSpec> DanLights() {
    return {DanLight(Vec3{0.5, 0.0, 0.5}),  DanLight(Vec3{0.0, 1.0, 0.0}),
            DanLight(Vec3{0.0, -1.0, 0.5}), DanLight(Vec3{0.0, 0.0, -1.0}),
            DanLight(Vec3{0.0, 0.0, -1.0}), DanLight(Widen(kKeyDirX, 0.5F, kKeyDirZ))};
}

std::vector<Clip> UnlitSlots() {
    return {Clip{.id = "light_3_off",
                 .end = kDanSelectFrames,
                 .command = LightSet{.index = 3, .enabled = false}},
            Clip{.id = "light_4_off",
                 .end = kDanSelectFrames,
                 .command = LightSet{.index = 4, .enabled = false}}};
}

OptionSpec GradeOption() {
    return OptionSpec{.id = "grade",
                      .label = "Grade",
                      .transition = {.frames = 0, .step = 4},
                      .choices = {ChoiceSpec{.label = kClassChoice},
                                  ChoiceSpec{.label = kDanChoice},
                                  ChoiceSpec{.label = kTopChoice}}};
}

}

Document Iidx12DanSelect() {
    return Document{
        .id = "iidx12-dan-select",
        .name = "Class course select",
        .build = "iidx12",
        .length = kDanSelectFrames,
        .render = RenderSpec{.clear_color = {0.0, 0.0, 0.0}},
        .camera = CameraSpec{.eye = Widen(0.0F, kEyeY, kEyeZ), .at = Widen(0.0F, kAtY, 0.0F)},
        .lights = DanLights(),
        .assets = {Scene3dAsset("dan", "data/graph/model/dan"),
                   Package2dAsset("dan_e", "data/graph/sys/dan_e"),
                   Package2dAsset("system", "data/graph/sys/system")},
        .options = {GradeOption()},
        .markers = {Marker{.frame = 0, .label = "Enter"},
                    Marker{.frame = kDanCameraFrame, .label = "Camera follows the grade"}},
        .tracks = {
            SpriteTrack("dan_bg", "dan_bg",
                        {Clip{.id = "dan_bg_hold",
                              .end = kDanSelectFrames,
                              .command = SpriteAnimate{.asset = "dan_e",
                                                       .animation = "DAN_BG",
                                                       .priority = 30,
                                                       .playback = GcAnim::Playback::HoldLast}}}),
            ModelTrack(
                "dan_sky", "dan_sky",
                SkyClips("dan_sky_draw", ModelBlend::Opaque, 1.0, "dan_sky_near", "dan_sky_far")),
            ModelTrack("dan_sky2", "dan_sky2",
                       SkyClips("dan_sky2_draw", ModelBlend::Additive, Widen(kSkyTwoSpeed),
                                "dan_sky2_near", "dan_sky2_far")),
            ModelTrack("dan_sea", "dan_sea",
                       {SeaModel("dan_sea_draw", {}, ModelBlend::Additive, Widen(kSeaAlpha),
                                 Vec3{0.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0})}),
            ModelTrack("dan_sea2", "dan_sea2",
                       {SeaModel("dan_sea2_draw", {}, ModelBlend::Opaque, 1.0, Vec3{0.0, 0.0, 0.0},
                                 Vec3{0.0, 0.0, 0.0})}),
            ModelTrack(
                "dan_sea2_flip", "dan_sea2_flip",
                {SeaModel("dan_sea2_flip_draw", "dan_sea2", ModelBlend::Additive, Widen(kSeaAlpha),
                          Widen(0.0F, kSeaSink, 0.0F), Widen(kSeaFlipRoll, 0.0F, 0.0F))}),
            ModelTrack("dan_light_bg", "dan_light_bg", LightBackdropClips()),
            CameraTrack("camera", CameraClips()),
            LightTrack("lights", UnlitSlots()),
            FxTrack("bubbles", {BubbleEmitter()}),
            SceneTrack("scene", {Clip{.id = "split_behind_dan_bg",
                                      .end = kDanSelectFrames,
                                      .command = RenderSettingsCmd{.sprite_split_priority = 30}}}),
        }};
}

}
