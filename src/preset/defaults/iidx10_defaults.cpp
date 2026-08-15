#include "preset/defaults/defaults_build.h"

#include "formats/gcanim.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <vector>

namespace Preset::Doc {

using namespace Build;

namespace {

Document Iidx10CardIn() {
    return Document{
        .id = "iidx10-card-in",
        .name = "Card in",
        .build = "iidx10",
        .length = 3600,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("music", "data/graph/texture/music"),
                   Package2dAsset("card", "data/graph/sys/card")},
        .tracks = {
            SpriteTrack("CARD_BG", "CARD_BG",
                        {Clip{.id = "CARD_BG",
                              .command = SpriteAnimate{.asset = "card",
                                                       .animation = "CARD_BG",
                                                       .priority = 31,
                                                       .playback = GcAnim::Playback::HoldLast}}}),
            ModelTrack("music_bg", "music_bg",
                       {Clip{.id = "music_bg",
                             .command = ModelDraw{.asset = "music",
                                                  .blend_mode = ModelBlend::Additive,
                                                  .alpha = 0.5,
                                                  .anim_speed = 0.25}}})}};
}

Document Iidx10DanSelect() {
    return Document{
        .id = "iidx10-dan-select",
        .name = "Class course select",
        .build = "iidx10",
        .length = 1200,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("cube_x", "data/graph/texture/cube_x"),
                   Package2dAsset("dan_e", "data/graph/sys/dan_e")},
        .tracks = {
            SpriteTrack("DAN_BG", "DAN_BG",
                        {Clip{.id = "DAN_BG",
                              .command = SpriteAnimate{.asset = "dan_e",
                                                       .animation = "DAN_BG",
                                                       .priority = 31,
                                                       .playback = GcAnim::Playback::HoldLast}}}),
            ModelTrack(
                "cube_x", "cube_x",
                {Clip{.id = "cube_x",
                      .command = ModelDraw{.asset = "cube_x",
                                           .blend_mode = ModelBlend::Additive,
                                           .anim_speed = 0.75,
                                           .spin_per_frame = Widen(-0.025F, 0.025F, 0.0125F)}}}),
            ModelTrack("cube_x_motion", "cube_x",
                       {Clip{.id = "cube_x_motion",
                             .command = ModelMotionCmd{
                                 .orbit = Orbit{.radius = Widen(0.2F),
                                                .rate_rad_per_frame = Widen(0.033333335F),
                                                .center = Widen(-0.55F, 0.2F),
                                                .z_start = 100.0,
                                                .z_per_frame = 10.0,
                                                .z_min = Widen(1.4F)}}}})}};
}

Document Iidx10ExpertSelect() {
    return Document{
        .id = "iidx10-expert-select",
        .name = "Expert select",
        .build = "iidx10",
        .length = 1800,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("ex01", "data/graph/texture/ex01"),
                   Package2dAsset("expert", "data/graph/sys/expert")},
        .tracks = {
            SpriteTrack("EX_BG", "EX_BG",
                        {Clip{.id = "EX_BG",
                              .command = SpriteAnimate{.asset = "expert",
                                                       .animation = "EX_BG",
                                                       .priority = 30,
                                                       .playback = GcAnim::Playback::HoldLast}}}),
            ModelTrack("ex01", "ex01",
                       {Clip{.id = "ex01",
                             .command = ModelDraw{.asset = "ex01",
                                                  .blend_mode = ModelBlend::Additive,
                                                  .alpha = Widen(0.8F),
                                                  .anim_speed = 0.5,
                                                  .position = {-0.5, 0.0, 40.0},
                                                  .rotation = Widen(0.0F, 0.0F, 9.2251F)}}}),
            ModelTrack(
                "ex01_intro", "ex01",
                {Clip{
                    .id = "ex01_intro",
                    .command = ModelTween{},
                    .keys = {Key{.at = 0, .values = {KeyValue{.id = "anim_speed", .value = -8.5}}},
                             Key{.at = 22,
                                 .values = {KeyValue{.id = "anim_speed", .value = 0.5}}}}}}),
            ModelTrack("ex01_countdown", "ex01",
                       {Clip{.id = "ex01_countdown",
                             .start = 1201,
                             .command = ModelTween{},
                             .keys = {Key{.at = 0,
                                          .values = {KeyValue{.id = "anim_speed",
                                                              .value = 0.5041666668839753},
                                                     KeyValue{.id = "blend_mode",
                                                              .value = (int)ModelBlend::Additive}}},
                                      Key{.at = 598,
                                          .values = {KeyValue{.id = "anim_speed",
                                                              .value = 2.995833463501185}}}}}})}};
}

Document Iidx10GameOver() {
    return Document{
        .id = "iidx10-game-over",
        .name = "Game over",
        .build = "iidx10",
        .length = 180,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("music", "data/graph/texture/music")},
        .tracks = {
            ModelTrack("music_bg", "music_bg",
                       {Clip{.id = "music_bg",
                             .command = ModelDraw{.asset = "music",
                                                  .blend_mode = ModelBlend::Additive,
                                                  .alpha = Widen(0.8F),
                                                  .anim_speed = Widen(1.6F)}}}),
            ModelTrack(
                "music_bg_countdown", "music_bg",
                {Clip{
                    .id = "music_bg_countdown",
                    .start = 1,
                    .command = ModelTween{},
                    .keys = {Key{.at = 0,
                                 .values = {KeyValue{.id = "anim_speed", .value = 1.59111113473773},
                                            KeyValue{.id = "blend_mode",
                                                     .value = (int)ModelBlend::Additive},
                                            KeyValue{.id = "alpha", .value = 0.795555567368865}}},
                             Key{.at = 178,
                                 .values = {
                                     KeyValue{.id = "anim_speed", .value = Widen(0.008888874F)},
                                     KeyValue{.id = "alpha", .value = Widen(0.004444437F)}}}}}})}};
}

Document Iidx10Login() {
    return Document{
        .id = "iidx10-login",
        .name = "Login",
        .build = "iidx10",
        .length = 240,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("music", "data/graph/texture/music"),
                   Package2dAsset("title_10th", "data/graph/sys/title_10th")},
        .tracks = {
            SpriteTrack("LOGIN", "LOGIN",
                        {Clip{.id = "LOGIN",
                              .command = SpriteAnimate{.asset = "title_10th",
                                                       .animation = "LOGIN",
                                                       .priority = 31,
                                                       .playback = GcAnim::Playback::HideAfterEnd,
                                                       .hidden_parts = {"BE2DX10", "TXT"}}}}),
            ModelTrack("music_bg", "music_bg",
                       {Clip{.id = "music_bg",
                             .command = ModelDraw{.asset = "music",
                                                  .blend_mode = ModelBlend::Additive,
                                                  .alpha = Widen(0.8F)}}})}};
}

Document Iidx10ModeSelect() {
    return Document{
        .id = "iidx10-mode-select",
        .name = "Mode select",
        .build = "iidx10",
        .length = 1200,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("cube_x", "data/graph/texture/cube_x"),
                   Package2dAsset("mode", "data/graph/sys/mode")},
        .options = {OptionSpec{
            .id = "mode",
            .label = "Selected mode",
            .transition = {.frames = 100, .step = 4, .spin_kick = 15.0},
            .choices = {ChoiceSpec{.label = "BEGINNER",
                                   .values = {ChoiceValue{.id = "model[cube_x].position",
                                                          .value = Widen(1.0F, 0.2F, 1.4F)}}},
                        ChoiceSpec{.label = "LIGHT7",
                                   .values = {ChoiceValue{.id = "model[cube_x].position",
                                                          .value = Widen(1.8F, 0.4F, 5.4F)}}},
                        ChoiceSpec{.label = "7KEYS",
                                   .values = {ChoiceValue{.id = "model[cube_x].position",
                                                          .value = Widen(0.6F, -0.15F, 2.7F)}}},
                        ChoiceSpec{.label = "EXPERT",
                                   .values = {ChoiceValue{.id = "model[cube_x].position",
                                                          .value = Vec3{0.0, 0.0, 3.0}}}},
                        ChoiceSpec{.label = "CLASS COURSE",
                                   .values = {ChoiceValue{.id = "model[cube_x].position",
                                                          .value = Widen(1.2F, 0.5F, 4.7F)}}},
                        ChoiceSpec{.label = "FREE",
                                   .values = {ChoiceValue{.id = "model[cube_x].position",
                                                          .value = Widen(1.0F, 0.4F, 1.0F)}}}}}},
        .tracks = {
            SpriteTrack(
                "MODE_BG_LOOP", "MODE_BG_LOOP",
                {Clip{.id = "MODE_BG_LOOP",
                      .command = SpriteAnimate{.asset = "mode",
                                               .animation = "MODE_BG_LOOP",
                                               .priority = 25,
                                               .hidden_parts = {"FRAME", "FRAME_GLOW",
                                                                "FRAME_GLOW2", "CTXT", "MODE_T",
                                                                "SETSUMEI", "T_REMAIN", "INFOWAKU",
                                                                "FRAME4", "FRAME6", "M_KAKKO"}}}}),
            ModelTrack("cube_x", "cube_x",
                       {Clip{.id = "cube_x",
                             .command = ModelDraw{.asset = "cube_x",
                                                  .blend_mode = ModelBlend::Additive,
                                                  .anim_speed = 0.75,
                                                  .position = Widen(1.0F, 0.3F, 1.75F),
                                                  .rotation = Widen(-0.7853982F, 0.0F, 0.7853982F),
                                                  .spin_per_frame = Widen(0.0F, 0.02F, 0.0F)}}}),
            ModelTrack(
                "cube_x_motion", "cube_x",
                {Clip{.id = "cube_x_motion",
                      .command = ModelMotionCmd{.spin_kick = 15.0, .spin_kick_decay = 0.5}}})}};
}

Document Iidx10MusicSelect() {
    return Document{
        .id = "iidx10-music-select",
        .name = "Music select",
        .build = "iidx10",
        .length = 1800,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("music", "data/graph/texture/music"),
                   Package2dAsset("mselect", "data/graph/sys/mselect")},
        .tracks = {
            SpriteTrack(
                "MU10_BG", "MU10_BG",
                {Clip{.id = "MU10_BG",
                      .command =
                          SpriteDraw{.asset = "mselect", .cell = "MU10_BG", .priority = 31}}}),
            SpriteTrack("BG_SKY", "BG_SKY",
                        {Clip{.id = "BG_SKY",
                              .command = SpriteDraw{.asset = "mselect",
                                                    .cell = "BG_SKY",
                                                    .x = 640.0,
                                                    .y = 120.0,
                                                    .priority = 30}},
                         Clip{.id = "BG_SKY_scroll",
                              .command = SpriteScroll{.scroll_x = 1.0, .scroll_wrap = 640.0}}}),
            SpriteTrack(
                "BG_SKY_2", "BG_SKY_2",
                {Clip{.id = "BG_SKY_2",
                      .command =
                          SpriteDraw{
                              .asset = "mselect", .cell = "BG_SKY", .y = 120.0, .priority = 30}},
                 Clip{.id = "BG_SKY_2_scroll",
                      .command = SpriteScroll{.scroll_x = 1.0, .scroll_wrap = 640.0}}}),
            ModelTrack("music_bg", "music_bg",
                       {Clip{.id = "music_bg",
                             .command = ModelDraw{.asset = "music",
                                                  .anim_speed = 0.25,
                                                  .rotation = Widen(0.0F, 49.51792F, 0.0F)}}}),
            ModelTrack(
                "music_bg_countdown", "music_bg",
                {Clip{.id = "music_bg_countdown",
                      .start = 1201,
                      .command = ModelTween{},
                      .keys = {
                          Key{.at = 0,
                              .values = {KeyValue{.id = "anim_speed", .value = 0.25249999994412065},
                                         KeyValue{.id = "blend_mode",
                                                  .value = (int)ModelBlend::Additive},
                                         KeyValue{.id = "alpha", .value = 0.9993333333404735}}},
                          Key{.at = 598,
                              .values = {
                                  KeyValue{.id = "anim_speed", .value = 1.7474999665282667},
                                  KeyValue{.id = "alpha", .value = 0.6006666709436104}}}}}})}};
}

Document Iidx10MusicSelectSamurai() {
    return Document{
        .id = "iidx10-music-select-samurai",
        .name = "Music select (samurai)",
        .build = "iidx10",
        .length = 1800,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("samurai", "data/graph/texture/samurai"),
                   Package2dAsset("mselect", "data/graph/sys/mselect")},
        .tracks = {
            SpriteTrack(
                "MU10_BG", "MU10_BG",
                {Clip{.id = "MU10_BG",
                      .command =
                          SpriteDraw{.asset = "mselect", .cell = "MU10_BG", .priority = 31}}}),
            SpriteTrack("BG_SKY", "BG_SKY",
                        {Clip{.id = "BG_SKY",
                              .command = SpriteDraw{.asset = "mselect",
                                                    .cell = "BG_SKY",
                                                    .x = 640.0,
                                                    .y = 120.0,
                                                    .priority = 30}},
                         Clip{.id = "BG_SKY_scroll",
                              .command = SpriteScroll{.scroll_x = 1.0, .scroll_wrap = 640.0}}}),
            SpriteTrack(
                "BG_SKY_2", "BG_SKY_2",
                {Clip{.id = "BG_SKY_2",
                      .command =
                          SpriteDraw{
                              .asset = "mselect", .cell = "BG_SKY", .y = 120.0, .priority = 30}},
                 Clip{.id = "BG_SKY_2_scroll",
                      .command = SpriteScroll{.scroll_x = 1.0, .scroll_wrap = 640.0}}}),
            ModelTrack("samurai", "samurai",
                       {Clip{.id = "samurai",
                             .command = ModelDraw{.asset = "samurai", .anim_speed = 0.25}}}),
            ModelTrack(
                "samurai_countdown", "samurai",
                {Clip{.id = "samurai_countdown",
                      .start = 1201,
                      .command = ModelTween{},
                      .keys = {
                          Key{.at = 0,
                              .values = {KeyValue{.id = "anim_speed", .value = 0.25249999994412065},
                                         KeyValue{.id = "blend_mode",
                                                  .value = (int)ModelBlend::Additive},
                                         KeyValue{.id = "alpha", .value = 0.9993333333404735}}},
                          Key{.at = 598,
                              .values = {
                                  KeyValue{.id = "anim_speed", .value = 1.7474999665282667},
                                  KeyValue{.id = "alpha", .value = 0.6006666709436104}}}}}})}};
}

Document Iidx10NewPlayer() {
    return Document{
        .id = "iidx10-new-player",
        .name = "New player invited",
        .build = "iidx10",
        .length = 1200,
        .camera = DefaultLens(),
        .lights = StandardLights(),
        .assets = {Scene3dAsset("tranbox", "data/graph/texture/tranbox"),
                   Package2dAsset("card", "data/graph/sys/card")},
        .tracks = {
            SpriteTrack("CARD_BG", "CARD_BG",
                        {Clip{.id = "CARD_BG",
                              .command = SpriteAnimate{.asset = "card",
                                                       .animation = "CARD_BG",
                                                       .priority = 31,
                                                       .playback = GcAnim::Playback::HoldLast}}}),
            ModelTrack(
                "tran_box", "tran_box",
                {Clip{.id = "tran_box",
                      .command = ModelDraw{.asset = "tranbox",
                                           .blend_mode = ModelBlend::Additive,
                                           .alpha = Widen(0.8F),
                                           .anim_speed = 0.0,
                                           .spin_per_frame = Widen(-0.025F, 0.025F, 0.0125F)}}}),
            ModelTrack("tran_box_motion", "tran_box",
                       {Clip{.id = "tran_box_motion",
                             .command = ModelMotionCmd{
                                 .orbit = Orbit{.radius = Widen(0.2F),
                                                .rate_rad_per_frame = Widen(0.033333335F),
                                                .center = Widen(2.23F, -0.4F),
                                                .z_start = 100.0,
                                                .z_per_frame = Widen(1.5833333F),
                                                .z_min = 5.0}}}})}};
}

}

std::vector<Document> Iidx10Defaults() {
    return {Iidx10CardIn(),   Iidx10DanSelect(),  Iidx10ExpertSelect(), Iidx10GameOver(),
            Iidx10Login(),    Iidx10ModeSelect(), Iidx10MusicSelect(),  Iidx10MusicSelectSamurai(),
            Iidx10NewPlayer()};
}

}
