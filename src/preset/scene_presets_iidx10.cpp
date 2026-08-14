#include "preset/scene_registry.h"

#include "formats/gcanim.h"
#include "preset/scene_preset.h"

#include <array>
#include <span>
#include <string_view>

namespace Preset {

namespace {

constexpr GcAnim::Timing kLoop = {.playback = GcAnim::Playback::Loop};
constexpr GcAnim::Timing kHold = {.playback = GcAnim::Playback::HoldLast};
constexpr GcAnim::Timing kOnce = {.playback = GcAnim::Playback::HideAfterEnd};

constexpr Camera kDefaultCamera = {.eye = {0.0F, 0.0F, -1.0F},
                                   .at = {0.0F, 0.0F, 0.0F},
                                   .up = {0.0F, 1.0F, 0.0F},
                                   .fov_y = 1.0471976F,
                                   .near_z = 0.1F,
                                   .far_z = 500.0F};

constexpr std::array<DirectionalLight, 2> kIidx10Lights = {{
    {.direction = {1.0F, 1.0F, 1.0F}},
    {.direction = {-1.0F, -1.0F, -1.0F}},
}};

constexpr float kOrbitRate = 0.033333335F;
constexpr float kSpinRate = 0.025F;
constexpr float kOrbitRadius = 0.2F;

constexpr std::string_view kMusicScene = "data/graph/texture/music";
constexpr std::string_view kCubeScene = "data/graph/texture/cube_x";
constexpr std::string_view kExpertScene = "data/graph/texture/ex01";
constexpr std::string_view kTranboxScene = "data/graph/texture/tranbox";
constexpr std::string_view kSamuraiScene = "data/graph/texture/samurai";

constexpr std::string_view kMselect = "data/graph/sys/mselect";
constexpr std::string_view kCard = "data/graph/sys/card";
constexpr std::string_view kTitle = "data/graph/sys/title_10th";
constexpr std::string_view kMode = "data/graph/sys/mode";
constexpr std::string_view kDan = "data/graph/sys/dan_e";
constexpr std::string_view kExpert = "data/graph/sys/expert";

constexpr std::array<ModelLayer, 1> kMusicSelectModels = {{
    {
        .scene_dir = kMusicScene,
        .model = "music_bg",
        .blend_mode = 0,
        .alpha = 1.0F,
        .anim_speed = 0.25F,
        .rotation = {0.0F, 49.5179214F, 0.0F},
    },
}};

constexpr std::array<ModelLayer, 1> kMusicSelectSamuraiModels = {{
    {
        .scene_dir = kSamuraiScene,
        .model = "samurai",
        .blend_mode = 0,
        .alpha = 1.0F,
        .anim_speed = 0.25F,
    },
}};

constexpr std::array<SpriteLayer, 3> kMusicSelectSprites = {{
    {.package_dir = kMselect, .sprite = "MU10_BG", .priority = 31},
    {.package_dir = kMselect,
     .sprite = "BG_SKY",
     .x = 640.0F,
     .y = 120.0F,
     .priority = 30,
     .scroll_x = 1.0F,
     .scroll_wrap = 640.0F},
    {.package_dir = kMselect,
     .sprite = "BG_SKY",
     .y = 120.0F,
     .priority = 30,
     .scroll_x = 1.0F,
     .scroll_wrap = 640.0F},
}};

constexpr Countdown kMusicSelectCountdown = {.start_frames = 1800,
                                             .ramp_below = 600,
                                             .speed_base = 0.25F,
                                             .speed_per_frame = 0.0025F,
                                             .fade_from = 1.0F,
                                             .fade_per_frame = 0.00066666666F,
                                             .ramp_blend_mode = 3};

constexpr std::array<std::string_view, 2> kLoginChrome = {"BE2DX10", "TXT"};

constexpr std::array<ModelLayer, 1> kCardInModels = {{
    {
        .scene_dir = kMusicScene,
        .model = "music_bg",
        .blend_mode = 3,
        .alpha = 0.5F,
        .anim_speed = 0.25F,
    },
}};

constexpr std::array<SpriteLayer, 1> kCardInSprites = {{
    {.package_dir = kCard, .sprite = "CARD_BG", .animated = true, .priority = 31, .timing = kHold},
}};

constexpr std::array<ModelLayer, 1> kLoginModels = {{
    {
        .scene_dir = kMusicScene,
        .model = "music_bg",
        .blend_mode = 3,
        .alpha = 0.8F,
        .anim_speed = 1.0F,
    },
}};

constexpr std::array<SpriteLayer, 1> kLoginSprites = {{
    {.package_dir = kTitle,
     .sprite = "LOGIN",
     .animated = true,
     .priority = 31,
     .timing = kOnce,
     .hidden_parts = kLoginChrome},
}};

constexpr float kQuarterTurn = 0.7853982F;

constexpr std::array<ModelLayer, 1> kModeSelectModels = {{
    {
        .scene_dir = kCubeScene,
        .model = "cube_x",
        .blend_mode = 3,
        .alpha = 1.0F,
        .anim_speed = 0.75F,
        .position = {1.0F, 0.3F, 1.75F},
        .rotation = {-kQuarterTurn, 0.0F, kQuarterTurn},
        .motion = {.spin_per_frame = {0.0F, 0.02F, 0.0F},
                   .spin_kick = 15.0F,
                   .spin_kick_decay = 0.5F},
    },
}};

constexpr std::array<std::string_view, 11> kModeSelectChrome = {
    "FRAME",    "FRAME_GLOW", "FRAME_GLOW2", "CTXT",   "MODE_T", "SETSUMEI",
    "T_REMAIN", "INFOWAKU",   "FRAME4",      "FRAME6", "M_KAKKO"};

constexpr std::array<OptionChoice, 6> kModeSelectModes = {{
    {.label = "BEGINNER", .position = {1.0F, 0.2F, 1.4F}},
    {.label = "LIGHT7", .position = {1.8F, 0.4F, 5.4F}},
    {.label = "7KEYS", .position = {0.6F, -0.15F, 2.7F}},
    {.label = "EXPERT", .position = {0.0F, 0.0F, 3.0F}},
    {.label = "CLASS COURSE", .position = {1.2F, 0.5F, 4.7F}},
    {.label = "FREE", .position = {1.0F, 0.4F, 1.0F}},
}};

constexpr std::array<Option, 1> kModeSelectOptions = {{
    {.id = "mode",
     .label = "Selected mode",
     .choices = kModeSelectModes,
     .default_choice = 0,
     .transition_frames = 100,
     .spin_kick = 15.0F},
}};

constexpr std::array<SpriteLayer, 1> kModeSelectSprites = {{
    {.package_dir = kMode,
     .sprite = "MODE_BG_LOOP",
     .animated = true,
     .priority = 25,
     .timing = kLoop,
     .hidden_parts = kModeSelectChrome},
}};

constexpr std::array<ModelLayer, 1> kDanSelectModels = {{
    {
        .scene_dir = kCubeScene,
        .model = "cube_x",
        .blend_mode = 3,
        .alpha = 1.0F,
        .anim_speed = 0.75F,
        .motion = {.orbit_radius = kOrbitRadius,
                   .orbit_rate = kOrbitRate,
                   .center_x = -0.55F,
                   .center_y = 0.2F,
                   .z_start = 100.0F,
                   .z_per_frame = 10.0F,
                   .z_min = 1.4F,
                   .spin_per_frame = {-kSpinRate, kSpinRate, kSpinRate * 0.5F}},
    },
}};

constexpr std::array<SpriteLayer, 1> kDanSelectSprites = {{
    {.package_dir = kDan, .sprite = "DAN_BG", .animated = true, .priority = 31, .timing = kHold},
}};

constexpr std::array<ModelLayer, 1> kExpertSelectModels = {{
    {
        .scene_dir = kExpertScene,
        .model = "ex01",
        .blend_mode = 3,
        .alpha = 0.8F,
        .anim_speed = 0.5F,
        .position = {-0.5F, 0.0F, 40.0F},
        .rotation = {0.0F, 0.0F, 9.2251F},
    },
}};

constexpr std::array<SpriteLayer, 1> kExpertSelectSprites = {{
    {.package_dir = kExpert, .sprite = "EX_BG", .animated = true, .priority = 30, .timing = kHold},
}};

constexpr std::array<ModelLayer, 1> kNewPlayerModels = {{
    {
        .scene_dir = kTranboxScene,
        .model = "tran_box",
        .blend_mode = 3,
        .alpha = 0.8F,
        .anim_speed = 0.0F,
        .motion = {.orbit_radius = kOrbitRadius,
                   .orbit_rate = kOrbitRate,
                   .center_x = 2.23F,
                   .center_y = -0.4F,
                   .z_start = 100.0F,
                   .z_per_frame = 1.5833333F,
                   .z_min = 5.0F,
                   .spin_per_frame = {-kSpinRate, kSpinRate, kSpinRate * 0.5F}},
    },
}};

constexpr std::array<SpriteLayer, 1> kNewPlayerSprites = {{
    {.package_dir = kCard, .sprite = "CARD_BG", .animated = true, .priority = 31, .timing = kHold},
}};

constexpr int kGameOverFrames = 180;

constexpr std::array<ModelLayer, 1> kGameOverModels = {{
    {
        .scene_dir = kMusicScene,
        .model = "music_bg",
        .blend_mode = 3,
        .alpha = 0.8F,
        .anim_speed = 1.6F,
    },
}};

constexpr std::array<Scene, 9> kScenes = {{
    {
        .id = "iidx10-music-select",
        .name = "Music select",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = kMusicSelectCountdown,
        .models = kMusicSelectModels,
        .sprites = kMusicSelectSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-music-select-samurai",
        .name = "Music select (samurai)",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = kMusicSelectCountdown,
        .models = kMusicSelectSamuraiModels,
        .sprites = kMusicSelectSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-card-in",
        .name = "Card in",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = {.start_frames = 3600},
        .models = kCardInModels,
        .sprites = kCardInSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-login",
        .name = "Login",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .models = kLoginModels,
        .sprites = kLoginSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-mode-select",
        .name = "Mode select",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = {.start_frames = 1200},
        .models = kModeSelectModels,
        .sprites = kModeSelectSprites,
        .lights = kIidx10Lights,
        .options = kModeSelectOptions,
    },
    {
        .id = "iidx10-dan-select",
        .name = "Class course select",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = {.start_frames = 1200},
        .models = kDanSelectModels,
        .sprites = kDanSelectSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-expert-select",
        .name = "Expert select",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = {.start_frames = 1800,
                      .ramp_below = 600,
                      .speed_base = 0.5F,
                      .speed_per_frame = 0.0041666667F,
                      .ramp_blend_mode = 3},
        .intro = {.frames = 22, .speed_from = -8.5F, .speed_to = 0.5F},
        .models = kExpertSelectModels,
        .sprites = kExpertSelectSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-new-player",
        .name = "New player invited",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = {.start_frames = 1200},
        .models = kNewPlayerModels,
        .sprites = kNewPlayerSprites,
        .lights = kIidx10Lights,
    },
    {
        .id = "iidx10-game-over",
        .name = "Game over",
        .build = "iidx10",
        .shading = Shading::LitMaterial,
        .camera = kDefaultCamera,
        .countdown = {.start_frames = kGameOverFrames,
                      .ramp_below = kGameOverFrames,
                      .speed_base = 1.6F,
                      .speed_per_frame = -1.6F / (float)kGameOverFrames,
                      .fade_from = 0.8F,
                      .fade_per_frame = 0.8F / (float)kGameOverFrames,
                      .ramp_blend_mode = 3},
        .models = kGameOverModels,
        .sprites = {},
        .lights = kIidx10Lights,
    },
}};

}

std::span<const Scene> Iidx10Scenes() {
    return kScenes;
}

}
