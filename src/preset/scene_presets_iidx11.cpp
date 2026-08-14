#include "preset/scene_registry.h"

#include "formats/gcanim.h"
#include "preset/scene_preset.h"

#include <array>
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

constexpr std::array<SpriteLayer, 1> kExpertSelectSprites = {{
    {.package_dir = kExpert,
     .sprite = "EXPERT_BG",
     .animated = true,
     .priority = 31,
     .timing = kHold},
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

constexpr std::array<SpriteLayer, 1> kAttractSprites = {{
    {.package_dir = kTitle, .sprite = "TITLE", .animated = true, .priority = 31, .timing = kOnce},
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
     .priority = 31,
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
        .models = kEndingModels,
        .sprites = kEndingSprites,
        .lights = kRedLights,
    },
}};

}

std::span<const Scene> Iidx11Scenes() {
    return kScenes;
}

}
