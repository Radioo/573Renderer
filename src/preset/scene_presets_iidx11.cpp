#include "preset/scene_registry.h"

#include "formats/gcanim.h"
#include "preset/scene_preset.h"
#include "preset/scene_presets_iidx11_ending.h"

#include <array>
#include <numbers>
#include <span>
#include <string_view>

namespace Preset {

namespace {

constexpr GcAnim::Timing kHold = {.playback = GcAnim::Playback::HoldLast};
constexpr GcAnim::Timing kOnce = {.playback = GcAnim::Playback::HideAfterEnd};

constexpr float kDegree = 0.017453292F;

constexpr std::string_view kRedScene = "data/graph/model/red";

constexpr std::string_view kDan = "data/graph/sys/dan_e";
constexpr std::string_view kExpert = "data/graph/sys/expert";
constexpr std::string_view kCard = "data/graph/sys/card";
constexpr std::string_view kTitle = "data/graph/sys/title";
constexpr std::string_view kEnding = "data/graph/sys/ending";
constexpr std::string_view kSystem = "data/graph/sys/system";

constexpr std::array<DirectionalLight, 2> kRedLights = {{
    {.direction = {1.0F, 1.0F, 1.0F}},
    {.direction = {-1.0F, -1.0F, -1.0F}},
}};

constexpr float kRedAnimSpeed = 0.75F;

constexpr Camera kRedCamera = {.eye = {0.0F, 0.0F, -1.0F},
                               .at = {0.0F, 0.0F, 0.0F},
                               .up = {0.0F, 1.0F, 0.0F},
                               .fov_y = 1.0471976F,
                               .near_z = 0.0F,
                               .far_z = 1000.0F,
                               .aspect = 1.7708334F};

constexpr Camera kMusicSelectCamera = {.eye = {0.0F, 0.0F, -1.0F},
                                       .at = {0.0F, 0.0F, 0.0F},
                                       .up = {0.0F, 1.0F, 0.0F},
                                       .fov_y = 1.0471976F,
                                       .near_z = 0.0F,
                                       .far_z = 1000.0F,
                                       .aspect = 1.7708334F};

constexpr std::array<float, 3> kMusicSelectPos = {-0.1F, 0.0F, -0.27555565F};

constexpr ModelMotion kEmblemSpin = {.spin_per_frame = {0.0F, 0.008726646F, 0.0F}};
constexpr ModelMotion kSideSpin = {.spin_per_frame = {0.0F, 0.011635529F, 0.008726646F}};

constexpr std::array<ModelLayer, 4> kMusicSelectModels = {{
    {.scene_dir = kRedScene,
     .model = "core",
     .blend_mode = 3,
     .alpha = 0.8F,
     .anim_speed = kRedAnimSpeed,
     .position = kMusicSelectPos,
     .rotation = {0.0F, 4.712389F, 45.0F},
     .motion = kEmblemSpin},
    {.scene_dir = kRedScene,
     .model = "shield",
     .blend_mode = 3,
     .alpha = 0.525F,
     .anim_speed = kRedAnimSpeed,
     .position = kMusicSelectPos,
     .rotation = {0.0F, 4.712389F, 45.0F},
     .motion = kEmblemSpin},
    {.scene_dir = kRedScene,
     .model = "flame",
     .blend_mode = 0,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .position = kMusicSelectPos,
     .rotation = {0.0F, 4.712389F, 45.0F},
     .motion = kEmblemSpin},
    {.scene_dir = kRedScene,
     .model = "r_side",
     .blend_mode = 3,
     .alpha = 0.65F,
     .anim_speed = kRedAnimSpeed,
     .position = kMusicSelectPos,
     .rotation = {0.0F, 0.407243F, 0.305433F},
     .motion = kSideSpin},
}};

constexpr std::array<Ramp, 8> kMusicSelectFlyIn = {{
    {.id = "model[core].position",
     .from = {-0.1F, 0.0F, -1.0F},
     .to = {-0.1F, 0.0F, -0.25F},
     .frames = 35,
     .degrees_per_frame = 3.0F,
     .curve = Curve::Sine},
    {.id = "model[shield].position",
     .from = {-0.1F, 0.0F, -1.0F},
     .to = {-0.1F, 0.0F, -0.25F},
     .frames = 35,
     .degrees_per_frame = 3.0F,
     .curve = Curve::Sine},
    {.id = "model[flame].position",
     .from = {-0.1F, 0.0F, -1.0F},
     .to = {-0.1F, 0.0F, -0.25F},
     .frames = 35,
     .degrees_per_frame = 3.0F,
     .curve = Curve::Sine},
    {.id = "model[r_side].position",
     .from = {-0.1F, 0.0F, -1.0F},
     .to = {-0.1F, 0.0F, -0.25F},
     .frames = 35,
     .degrees_per_frame = 3.0F,
     .curve = Curve::Sine},
    {.id = "model[core].rotation",
     .from = {0.0F, 0.0F, 45.0F},
     .to = {0.0F, 4.712389F, 45.0F},
     .frames = 34,
     .degrees_per_frame = 2.6470588F,
     .curve = Curve::Sine},
    {.id = "model[shield].rotation",
     .from = {0.0F, 0.0F, 45.0F},
     .to = {0.0F, 4.712389F, 45.0F},
     .frames = 34,
     .degrees_per_frame = 2.6470588F,
     .curve = Curve::Sine},
    {.id = "model[flame].rotation",
     .from = {0.0F, 0.0F, 45.0F},
     .to = {0.0F, 4.712389F, 45.0F},
     .frames = 34,
     .degrees_per_frame = 2.6470588F,
     .curve = Curve::Sine},
    {.id = "model[r_side].rotation",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.407243F, 0.305433F},
     .frames = 34,
     .degrees_per_frame = 2.6470588F,
     .curve = Curve::Sine},
}};

constexpr std::array<ParamOverride, 4> kMusicSelectSettled = {{
    {.id = "model[core].position", .f = {-0.1F, 0.0F, -0.27555565F}},
    {.id = "model[shield].position", .f = {-0.1F, 0.0F, -0.27555565F}},
    {.id = "model[flame].position", .f = {-0.1F, 0.0F, -0.27555565F}},
    {.id = "model[r_side].position", .f = {-0.1F, 0.0F, -0.27555565F}},
}};

constexpr std::array<Phase, 2> kMusicSelectPhases = {{
    {.label = "Fly in, punching past the resting point",
     .start_frame = 0,
     .ramps = kMusicSelectFlyIn},
    {.label = "Settled", .start_frame = 35, .params = kMusicSelectSettled},
}};

constexpr std::array<float, 3> kAttackPos = {-0.05F, -0.01F, -0.85511113F};

constexpr std::array<ParamOverride, 11> kAttackParams = {{
    {.id = "model[core].position", .f = kAttackPos},
    {.id = "model[shield].position", .f = kAttackPos},
    {.id = "model[flame].position", .f = kAttackPos},
    {.id = "model[r_side].position", .f = kAttackPos},
    {.id = "model[core].rotation", .f = {0.0F, -std::numbers::pi_v<float>, 45.0F}},
    {.id = "model[shield].rotation", .f = {0.0F, -3.0F * std::numbers::pi_v<float>, 45.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, -3.0F * std::numbers::pi_v<float>, 45.0F}},
    {.id = "model[core].motion.spin_per_frame", .f = {0.004363323F, 0.011635528F, 0.0F}},
    {.id = "model[shield].motion.spin_per_frame", .f = {0.004363323F, 0.034906585F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.004363323F, 0.034906585F, 0.0F}},
    {.id = "model[r_side].motion.spin_per_frame", .f = {0.0F, 0.011635529F, 0.008726646F}},
}};

constexpr std::array<OptionChoice, 2> kMusicSelectModes = {{
    {.label = "Normal"},
    {.label = "ATTACK", .params = kAttackParams},
}};

constexpr std::array<Option, 1> kMusicSelectOptions = {{
    {.id = "attack", .label = "Screen variant", .choices = kMusicSelectModes},
}};

constexpr std::array<std::string_view, 1> kCardChrome = {"T_REMAIN"};

constexpr ModelMotion kModeSelectSpin = {.spin_per_frame = {0.0F, -0.008F, 0.0F}};

constexpr std::array<ModelLayer, 2> kModeSelectModels = {{
    {.scene_dir = kRedScene,
     .model = "core",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .rotation = {0.0F, 4.2F, 0.0F},
     .motion = kModeSelectSpin},
    {.scene_dir = kRedScene,
     .model = "flame",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .rotation = {0.0F, 4.2F, 0.0F},
     .motion = kModeSelectSpin},
}};

constexpr std::array<Ramp, 2> kModeSelectSpinA = {{
    {.id = "model[core].motion.spin_per_frame",
     .from = {0.0F, 0.12F, 0.0F},
     .to = {0.0F, 0.064F, 0.0F},
     .frames = 15},
    {.id = "model[flame].motion.spin_per_frame",
     .from = {0.0F, 0.12F, 0.0F},
     .to = {0.0F, 0.064F, 0.0F},
     .frames = 15},
}};

constexpr std::array<Ramp, 2> kModeSelectSpinB = {{
    {.id = "model[core].motion.spin_per_frame",
     .from = {0.0F, 0.30F, 0.0F},
     .to = {0.0F, 0.06F, 0.0F},
     .frames = 13},
    {.id = "model[flame].motion.spin_per_frame",
     .from = {0.0F, 0.30F, 0.0F},
     .to = {0.0F, 0.06F, 0.0F},
     .frames = 13},
}};

constexpr std::array<ParamOverride, 4> kModeSelectFloor = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.04F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.04F, 0.0F}},
    {.id = "model[core].rotation", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, 0.0F, 0.0F}},
}};

constexpr std::array<ParamOverride, 4> kModeSelectUnwind = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, -0.008F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.008F, 0.0F}},
    {.id = "model[core].rotation", .f = {0.0F, 4.2F, 0.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, 4.2F, 0.0F}},
}};

constexpr std::array<Phase, 4> kModeSelectPhases = {{
    {.label = "Fly in, fast wind up", .start_frame = 0, .ramps = kModeSelectSpinA},
    {.label = "Fly in, five times the accumulate", .start_frame = 15, .ramps = kModeSelectSpinB},
    {.label = "Fly in, spin at its floor", .start_frame = 28, .params = kModeSelectFloor},
    {.label = "Interactive, unwinding", .start_frame = 40, .params = kModeSelectUnwind},
}};

constexpr Camera kModeSelectCamera = {.eye = {-0.15F, 0.14F, -0.06F},
                                      .at = {1.12F, -1.31F, 1.28F},
                                      .up = {0.0F, 1.0F, 0.0F},
                                      .fov_y = 1.0471976F,
                                      .near_z = 0.0F,
                                      .far_z = 1000.0F,
                                      .aspect = 1.7708334F};

constexpr std::array<ModelLayer, 3> kDanSelectModels = {{
    {.scene_dir = kRedScene,
     .model = "dan_bg1",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .rotation = {3.1400001F, 0.0F, 0.0F},
     .motion = {.spin_per_frame = {0.0F, 0.03F, 0.0F}}},
    {.scene_dir = kRedScene,
     .model = "core",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .motion = {.spin_per_frame = {0.0F, 0.015F, 0.0F}}},
    {.scene_dir = kRedScene,
     .model = "flame",
     .blend_mode = 0,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .motion = {.spin_per_frame = {0.0F, -0.015F, 0.0F}}},
}};

constexpr Camera kDanSelectCamera = {.eye = {0.0F, 0.59045085F, -0.00809017F},
                                     .at = {0.0F, 0.0F, 0.0F},
                                     .up = {0.0F, 1.0F, 0.0F},
                                     .fov_y = 1.0471976F,
                                     .near_z = 0.0F,
                                     .far_z = 1000.0F,
                                     .aspect = 1.7708334F};

constexpr std::array<OptionChoice, 17> kDanCourses = {{
    {.label = "CLASS 7", .camera_eye = {0.0F, 0.54270510F, -0.00904509F}, .moves_camera = true},
    {.label = "CLASS 6", .camera_eye = {0.0F, 0.51169339F, -0.01809017F}, .moves_camera = true},
    {.label = "CLASS 5", .camera_eye = {0.0F, 0.48068167F, -0.03618034F}, .moves_camera = true},
    {.label = "CLASS 4", .camera_eye = {0.0F, 0.44966993F, -0.05427051F}, .moves_camera = true},
    {.label = "CLASS 3", .camera_eye = {0.0F, 0.41865821F, -0.07236068F}, .moves_camera = true},
    {.label = "CLASS 2", .camera_eye = {0.0F, 0.38764650F, -0.09045085F}, .moves_camera = true},
    {.label = "CLASS 1", .camera_eye = {0.0F, 0.35663478F, -0.10854102F}, .moves_camera = true},
    {.label = "1ST DAN", .camera_eye = {0.0F, 0.32562306F, -0.12663119F}, .moves_camera = true},
    {.label = "2ND DAN", .camera_eye = {0.0F, 0.29461132F, -0.14472136F}, .moves_camera = true},
    {.label = "3RD DAN", .camera_eye = {0.0F, 0.26359959F, -0.16281153F}, .moves_camera = true},
    {.label = "4TH DAN", .camera_eye = {0.0F, 0.23258788F, -0.18090170F}, .moves_camera = true},
    {.label = "5TH DAN", .camera_eye = {0.0F, 0.20157616F, -0.19899187F}, .moves_camera = true},
    {.label = "6TH DAN", .camera_eye = {0.0F, 0.17056445F, -0.21708204F}, .moves_camera = true},
    {.label = "7TH DAN", .camera_eye = {0.0F, 0.13955274F, -0.23517221F}, .moves_camera = true},
    {.label = "8TH DAN", .camera_eye = {0.0F, 0.10854102F, -0.25326238F}, .moves_camera = true},
    {.label = "9TH DAN", .camera_eye = {0.0F, 0.10854102F, -0.17185662F}, .moves_camera = true},
    {.label = "10TH DAN", .camera_eye = {0.0F, 0.10854102F, -0.09045085F}, .moves_camera = true},
}};

constexpr std::array<Option, 1> kDanOptions = {{
    {.id = "course",
     .label = "Selected course",
     .choices = kDanCourses,
     .default_choice = 0,
     .transition_frames = 12},
}};

constexpr std::array<SpriteLayer, 1> kDanSelectSprites = {{
    {.package_dir = kDan, .sprite = "BG", .animated = true, .priority = 31, .timing = kHold},
}};

constexpr std::array<float, 3> kExpertPos = {-0.19112459F, -0.00592613F, -0.7036935F};

constexpr std::array<ModelLayer, 2> kExpertSelectModels = {{
    {.scene_dir = kRedScene,
     .model = "core",
     .blend_mode = 3,
     .alpha = 0.8F,
     .anim_speed = kRedAnimSpeed,
     .position = kExpertPos,
     .rotation = {0.0F, 2.19911F, 45.0F},
     .motion = {.spin_per_frame = {0.0F, 0.052359876F, 0.0F}}},
    {.scene_dir = kRedScene,
     .model = "flame",
     .blend_mode = 0,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .position = kExpertPos,
     .rotation = {0.0F, -0.36652F, -0.18326F},
     .motion = {.spin_per_frame = {0.0F, -0.008726646F, -0.004363323F}}},
}};

constexpr std::array<ParamOverride, 2> kExpertFlyInLayers = {{
    {.id = "sprite[EXPERT_BG].visible", .i = 1},
    {.id = "sprite[COURSE_DECIDE].visible"},
}};

constexpr std::array<ParamOverride, 4> kExpertFade = {{
    {.id = "model[core].position", .f = {0.0F, 0.0F, -0.699999988F}},
    {.id = "model[flame].position", .f = {0.0F, 0.0F, -0.699999988F}},
    {.id = "sprite[EXPERT_BG].visible"},
    {.id = "sprite[COURSE_DECIDE].visible", .i = 1},
}};

constexpr std::array<Ramp, 2> kExpertFade0ut = {{
    {.id = "model[core].alpha", .from = {0.8F, 0.0F, 0.0F}, .to = {0.0F, 0.0F, 0.0F}, .frames = 31},
    {.id = "model[flame].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 31},
}};

constexpr std::array<Ramp, 2> kExpertFlyIn = {{
    {.id = "model[core].position",
     .from = {0.2F, 0.0F, -1.0F},
     .to = {-0.196F, -0.006F, -0.7F},
     .frames = 42,
     .degrees_per_frame = 2.3684211F,
     .curve = Curve::Sine},
    {.id = "model[flame].position",
     .from = {0.2F, 0.0F, -1.0F},
     .to = {-0.196F, -0.006F, -0.7F},
     .frames = 42,
     .degrees_per_frame = 2.3684211F,
     .curve = Curve::Sine},
}};

constexpr std::array<ParamOverride, 4> kExpertSettled = {{
    {.id = "model[core].position", .f = {-0.19112459F, -0.00592613F, -0.7036935F}},
    {.id = "model[flame].position", .f = {-0.19112459F, -0.00592613F, -0.7036935F}},
    {.id = "sprite[EXPERT_BG].visible", .i = 1},
    {.id = "sprite[COURSE_DECIDE].visible"},
}};

constexpr std::array<ParamOverride, 4> kExpertOutro = {{
    {.id = "model[core].position", .f = {0.0F, 0.0F, -0.699999988F}},
    {.id = "model[flame].position", .f = {0.0F, 0.0F, -0.699999988F}},
    {.id = "sprite[EXPERT_BG].visible"},
    {.id = "sprite[COURSE_DECIDE].visible", .i = 1},
}};

constexpr std::array<Phase, 4> kExpertPhases = {{
    {.label = "Fly in from the eye plane",
     .start_frame = 0,
     .params = kExpertFlyInLayers,
     .ramps = kExpertFlyIn},
    {.label = "Settled", .start_frame = 42, .params = kExpertSettled},
    {.label = "Outro hold, snapped to centre", .start_frame = 400, .params = kExpertOutro},
    {.label = "Outro fade", .start_frame = 550, .params = kExpertFade, .ramps = kExpertFade0ut},
}};

constexpr std::array<std::string_view, 3> kCourseDecideChrome = {"SKM", "OPTION_1P", "OPTION_2P"};

constexpr std::array<SpriteLayer, 2> kExpertSelectSprites = {{
    {.package_dir = kExpert,
     .sprite = "EXPERT_BG",
     .animated = true,
     .priority = 31,
     .timing = kHold},
    {.package_dir = kExpert,
     .sprite = "COURSE_DECIDE",
     .animated = true,
     .priority = 31,
     .timing = kHold,
     .hidden_parts = kCourseDecideChrome},
}};

constexpr std::array<ModelLayer, 1> kNewPlayerModels = {{
    {.scene_dir = kRedScene,
     .model = "gate",
     .blend_mode = 3,
     .alpha = 0.5F,
     .anim_speed = 0.25F,
     .rotation = {-0.8F, 0.0F, 0.0F}},
}};

constexpr std::array<SpriteLayer, 1> kNewPlayerSprites = {{
    {.package_dir = kCard,
     .sprite = "CARD_BG",
     .animated = true,
     .priority = 31,
     .timing = kHold,
     .hidden_parts = kCardChrome},
}};

constexpr Camera kTitleCamera = {.eye = {0.0F, 0.0F, -0.25F},
                                 .at = {0.0F, 0.0F, 0.0F},
                                 .up = {0.0F, 1.0F, 0.0F},
                                 .fov_y = 1.0471976F,
                                 .near_z = 0.0F,
                                 .far_z = 1000.0F,
                                 .aspect = 1.7708334F};

constexpr std::array<float, 3> kAttractPos = {0.105F, 0.0F, 0.0F};

constexpr ModelMotion kAttractSpin = {.spin_per_frame = {0.0F, kDegree * 0.125F, 0.0F}};
constexpr ModelMotion kAttractSideSpin = {.spin_per_frame = {0.0F, kDegree * 0.5F, 0.0F}};

constexpr std::array<float, 3> kWarpPos = {0.0F, 0.0F, -0.15F};
constexpr float kWarpSpin = kDegree;

constexpr std::array<ParamOverride, 21> kAttractHidden = {{
    {.id = "model[core].visible"},
    {.id = "model[shield].visible"},
    {.id = "model[flame].visible"},
    {.id = "model[r_side].visible"},
    {.id = "model[core].position", .f = kWarpPos},
    {.id = "model[shield].position", .f = kWarpPos},
    {.id = "model[flame].position", .f = kWarpPos},
    {.id = "model[r_side].position", .f = kWarpPos},
    {.id = "model[core].rotation", .f = {0.0F, 0.0F, 45.0F}},
    {.id = "model[shield].rotation", .f = {0.0F, 0.0F, 45.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, 0.0F, 45.0F}},
    {.id = "model[r_side].rotation", .f = {0.0F, 0.0F, 85.0F}},
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[shield].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[r_side].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[r_side].blend_mode", .i = 3},
    {.id = "camera.aspect_auto", .i = 0},
    {.id = "camera.aspect_value", .f = {1.3333334F, 0.0F, 0.0F}},
    {.id = "sprite[TITLE].visible", .i = 1},
    {.id = "sprite[TITLE_TAIKI].visible"},
}};

constexpr std::array<ParamOverride, 24> kAttractWarp = {{
    {.id = "model[core].visible", .i = 1},
    {.id = "model[shield].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "model[r_side].visible", .i = 1},
    {.id = "model[core].position", .f = kWarpPos},
    {.id = "model[shield].position", .f = kWarpPos},
    {.id = "model[flame].position", .f = kWarpPos},
    {.id = "model[r_side].position", .f = kWarpPos},
    {.id = "model[core].rotation", .f = {0.0F, 8.7615527F, 45.0F}},
    {.id = "model[shield].rotation", .f = {0.0F, 8.7615527F, 45.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, 8.7615527F, 45.0F}},
    {.id = "model[r_side].rotation", .f = {0.0F, 8.7615527F, 85.0F}},
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[shield].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[r_side].motion.spin_per_frame", .f = {0.0F, kWarpSpin, 0.0F}},
    {.id = "model[r_side].blend_mode", .i = 3},
    {.id = "camera.aspect_auto", .i = 0},
    {.id = "camera.aspect_value", .f = {1.3333334F, 0.0F, 0.0F}},
    {.id = "intro.frames", .i = 291},
    {.id = "intro.fov_from", .f = {22.546017F, 0.0F, 0.0F}},
    {.id = "intro.fov_to", .f = {25.110188F, 0.0F, 0.0F}},
    {.id = "sprite[TITLE].visible", .i = 1},
    {.id = "sprite[TITLE_TAIKI].visible"},
}};

constexpr std::array<ParamOverride, 6> kAttractLoop = {{
    {.id = "model[core].rotation", .f = {0.0F, 1.9678687F, 45.0F}},
    {.id = "model[shield].rotation", .f = {0.0F, 1.9678687F, 45.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, 1.9678687F, 45.0F}},
    {.id = "model[r_side].rotation", .f = {0.0F, 7.8714347F, -10.0F}},
    {.id = "sprite[TITLE].visible", .i = 1},
    {.id = "sprite[TITLE_TAIKI].visible"},
}};

constexpr std::array<ParamOverride, 6> kAttractStandby = {{
    {.id = "model[core].rotation", .f = {0.0F, 3.7873644F, 45.0F}},
    {.id = "model[shield].rotation", .f = {0.0F, 3.7873644F, 45.0F}},
    {.id = "model[flame].rotation", .f = {0.0F, 3.7873644F, 45.0F}},
    {.id = "model[r_side].rotation", .f = {0.0F, 15.1494575F, -10.0F}},
    {.id = "sprite[TITLE].visible"},
    {.id = "sprite[TITLE_TAIKI].visible", .i = 1},
}};

constexpr std::array<Emitter, 1> kWarpParticles = {{
    {.package_dir = kSystem,
     .cell = "PTC_ORAN",
     .count = 16,
     .angle_step_deg = 22.5F,
     .phase_rate_deg = 32.0F,
     .phase_amplitude_deg = 360.0F,
     .radius_from = 10,
     .radius_to = 630,
     .frames = 290,
     .priority = 31,
     .spawn = Spawn::EveryFrame,
     .life = 60},
}};

constexpr std::array<Phase, 5> kAttractPhases = {{
    {.label = "Boot animation, models hidden", .start_frame = 0, .params = kAttractHidden},
    {.label = "Warp in, rotating and zooming",
     .start_frame = 502,
     .params = kAttractWarp,
     .emitters = kWarpParticles},
    {.label = "Boot animation runs on, models hidden again",
     .start_frame = 793,
     .params = kAttractHidden},
    {.label = "Attract loop, settled to the right", .start_frame = 902, .params = kAttractLoop},
    {.label = "Standby logo, TITLE_TAIKI looping", .start_frame = 1736, .params = kAttractStandby},
}};

constexpr std::array<ModelLayer, 4> kAttractModels = {{
    {.scene_dir = kRedScene,
     .model = "core",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .position = kAttractPos,
     .rotation = {0.0F, 0.0F, 45.0F},
     .motion = kAttractSpin},
    {.scene_dir = kRedScene,
     .model = "shield",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .position = kAttractPos,
     .rotation = {0.0F, 0.0F, 45.0F},
     .motion = kAttractSpin},
    {.scene_dir = kRedScene,
     .model = "flame",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .position = kAttractPos,
     .rotation = {0.0F, 0.0F, 45.0F},
     .motion = kAttractSpin},
    {.scene_dir = kRedScene,
     .model = "r_side",
     .blend_mode = 0,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .position = kAttractPos,
     .rotation = {0.0F, 0.0F, -10.0F},
     .motion = kAttractSideSpin},
}};

constexpr std::array<SpriteLayer, 2> kAttractSprites = {{
    {.package_dir = kTitle, .sprite = "TITLE", .animated = true, .priority = 15, .timing = kHold},
    {.package_dir = kTitle,
     .sprite = "TITLE_TAIKI",
     .animated = true,
     .priority = 15,
     .timing = {}},
}};

constexpr std::array<ModelLayer, 1> kCardInModels = {{
    {.scene_dir = kRedScene,
     .model = "gate",
     .blend_mode = 3,
     .alpha = 0.5F,
     .anim_speed = 0.25F,
     .rotation = {-0.8F, 0.0F, 0.0F}},
}};

constexpr std::array<SpriteLayer, 1> kCardInSprites = {{
    {.package_dir = kCard,
     .sprite = "CARD_BG",
     .animated = true,
     .priority = 31,
     .timing = kHold,
     .hidden_parts = kCardChrome},
}};

constexpr std::array<ModelLayer, 1> kLoginModels = {{
    {.scene_dir = kRedScene,
     .model = "gate",
     .blend_mode = 3,
     .alpha = 0.8F,
     .anim_speed = 1.0F,
     .rotation = {-0.8F, 0.0F, 0.0F}},
}};

constexpr std::array<std::string_view, 2> kLoginChrome = {"OP_BG_U", "OP_BG_D"};

constexpr std::array<SpriteLayer, 1> kLoginSprites = {{
    {.package_dir = kTitle,
     .sprite = "LOGIN",
     .animated = true,
     .priority = 15,
     .timing = kOnce,
     .hidden_parts = kLoginChrome},
}};

constexpr ModelMotion kEndingSpin = {.spin_per_frame = {0.0F, kDegree, 0.0F}};

constexpr std::array<ModelLayer, 2> kEndingModels = {{
    {.scene_dir = kRedScene,
     .model = "core",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .rotation = {0.0F, 0.0F, 45.0F},
     .motion = kEndingSpin},
    {.scene_dir = kRedScene,
     .model = "flame",
     .blend_mode = 3,
     .alpha = 1.0F,
     .anim_speed = kRedAnimSpeed,
     .rotation = {0.0F, 0.0F, 45.0F},
     .motion = kEndingSpin},
}};

constexpr std::array<SpriteLayer, 1> kEndingSprites = {{
    {.package_dir = kEnding, .sprite = "END_BG1", .priority = 31},
}};

constexpr Camera kEndingCamera = {.eye = {0.0F, 0.0F, 0.249F},
                                  .at = {0.0F, 0.0F, 0.0F},
                                  .up = {0.0F, 1.0F, 0.0F},
                                  .fov_y = 1.0471976F,
                                  .near_z = 0.0F,
                                  .far_z = 1000.0F,
                                  .aspect = 1.7708334F};

constexpr std::array<Scene, 9> kScenes = {{
    {
        .id = "iidx11-music-select",
        .name = "Music select",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kMusicSelectCamera,
        .countdown = {.start_frames = 3600},
        .models = kMusicSelectModels,
        .sprites = {},
        .lights = kRedLights,
        .options = kMusicSelectOptions,
        .phases = kMusicSelectPhases,
    },
    {
        .id = "iidx11-mode-select",
        .name = "Mode select",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kModeSelectCamera,
        .countdown = {.start_frames = 1200},
        .models = kModeSelectModels,
        .sprites = {},
        .lights = kRedLights,
        .phases = kModeSelectPhases,
    },
    {
        .id = "iidx11-dan-select",
        .name = "Class course select",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kDanSelectCamera,
        .countdown = {.start_frames = 1200},
        .models = kDanSelectModels,
        .sprites = kDanSelectSprites,
        .lights = kRedLights,
        .options = kDanOptions,
    },
    {
        .id = "iidx11-expert-select",
        .name = "Expert select",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kRedCamera,
        .countdown = {.start_frames = 2700},
        .models = kExpertSelectModels,
        .sprites = kExpertSelectSprites,
        .lights = kRedLights,
        .phases = kExpertPhases,
    },
    {
        .id = "iidx11-new-player",
        .name = "New player invited",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kRedCamera,
        .countdown = {.start_frames = 1200},
        .models = kNewPlayerModels,
        .sprites = kNewPlayerSprites,
        .lights = kRedLights,
    },
    {
        .id = "iidx11-attract",
        .name = "Attract logo",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kTitleCamera,
        .models = kAttractModels,
        .sprites = kAttractSprites,
        .lights = kRedLights,
        .phases = kAttractPhases,
    },
    {
        .id = "iidx11-card-in",
        .name = "Card in",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kTitleCamera,
        .countdown = {.start_frames = 3600},
        .models = kCardInModels,
        .sprites = kCardInSprites,
        .lights = kRedLights,
    },
    {
        .id = "iidx11-login",
        .name = "Login",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kTitleCamera,
        .models = kLoginModels,
        .sprites = kLoginSprites,
        .lights = kRedLights,
    },
    {
        .id = "iidx11-ending",
        .name = "Ending",
        .build = "iidx11",
        .shading = Shading::LitMaterial,
        .camera = kEndingCamera,
        .beat = {.rate = 155, .span = 3600, .offset_a = 70, .offset_b = 59},
        .models = kEndingModels,
        .sprites = kEndingSprites,
        .lights = kRedLights,
        .phases = kEndingPhases,
    },
}};

}

std::span<const Scene> Iidx11Scenes() {
    return kScenes;
}

}
