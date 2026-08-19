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

constexpr int kEndingFrames = 5112;
constexpr int kSkyFrame = 200;
constexpr int kPlateGoneFrame = 300;
constexpr int kRampFrame = 4833;
constexpr int kFadeFrame = 5077;
constexpr int kRampKeyFrame = kRampFrame - 1;
constexpr int kLastFrame = kEndingFrames - 1;

constexpr float kFovY = 1.0471976F;
constexpr float kAspect = 1.7708334F;
constexpr float kFogDensity = std::bit_cast<float>(0x3DCCCCCDU);
constexpr float kRollDegrees = std::bit_cast<float>(0x3E4CCCCDU);
constexpr float kFinalSpeed = 7.975F;
constexpr float kOrbitZ = std::bit_cast<float>(0xBEAAAAABU);
constexpr double kTileAlpha = 127.0 / 255.0;

const char* const kMovieFile = "data/movie/08ra.4";
const char* const kLatticeOption = "lattice";
const char* const kSeedOne = "Seed 1";
const char* const kSeedTwo = "Seed 573";
const char* const kSeedThree = "Seed 12345";

CameraSpec EndingCamera() {
    return CameraSpec{.eye = {0.0, 0.0, 0.0},
                      .at = {0.0, 0.0, 1.0},
                      .up = {1.0, 0.0, 0.0},
                      .fov_y = Widen(kFovY),
                      .near_z = 0.0,
                      .far_z = 1000.0,
                      .aspect = AspectSpec{.automatic = false, .value = Widen(kAspect)}};
}

std::vector<Clip> CameraClips() {
    return {Clip{.id = "camera_hold",
                 .end = kEndingFrames,
                 .command =
                     CameraSet{.eye = Vec3{0.0, 0.0, 0.0},
                               .at = Vec3{0.0, 0.0, 1.0},
                               .up = Vec3{1.0, 0.0, 0.0},
                               .fov_y = Widen(kFovY),
                               .near_z = 0.0,
                               .far_z = 1000.0,
                               .aspect = AspectSpec{.automatic = false, .value = Widen(kAspect)}}},
            Clip{.id = "camera_roll",
                 .end = kEndingFrames,
                 .command = CameraMotionCmd{.up_roll_deg_per_frame = Widen(kRollDegrees)}}};
}

std::vector<Clip> SkyClips() {
    return {Clip{
        .id = "sky_dome",
        .start = kSkyFrame,
        .end = kEndingFrames,
        .command = ModelDraw{
            .asset = "sky", .blend_mode = ModelBlend::Alpha, .alpha = 0.5, .anim_speed = 1.0}}};
}

std::vector<Clip> SkySpeedClips() {
    return {
        Clip{.id = "sky_speed_ramp",
             .start = kRampKeyFrame,
             .end = kEndingFrames,
             .command = ModelTween{},
             .keys = {Key{.at = 0, .values = {KeyValue{.id = "anim_speed", .value = 1.0}}},
                      Key{.at = kLastFrame - kRampKeyFrame,
                          .values = {KeyValue{.id = "anim_speed", .value = Widen(kFinalSpeed)}}}}}};
}

Clip TileGrid(std::string id, int seed, Gate gate) {
    return Clip{.id = std::move(id),
                .start = kSkyFrame,
                .end = kEndingFrames,
                .when = std::move(gate),
                .command = PolyTileGrid{.rows = 3,
                                        .cols = 3,
                                        .lattice_amplitude = 0.1,
                                        .lattice_seed = seed,
                                        .spacing = Vec2{3.0, 2.25},
                                        .depth = 5.0,
                                        .quad_scale = Vec2{3.6, 2.7},
                                        .spin_rates = Vec3{1.0, 1.0, -2.0},
                                        .orbit_rates = Vec3{0.0, -0.5, Widen(kOrbitZ)},
                                        .burst_from = kRampFrame,
                                        .burst_step = 2.0,
                                        .burst_delay_per_tile = 10.0,
                                        .alpha = kTileAlpha,
                                        .texture = MovieTexture{.path = kMovieFile},
                                        .movie_size = Vec2{304.0, 416.0},
                                        .texture_size = 512.0}};
}

Gate WhenSeed(std::string choice) {
    return Gate{.option = kLatticeOption, .choices = {std::move(choice)}};
}

std::vector<Clip> TileClips() {
    return {TileGrid("movie_tiles_seed_1", 1, WhenSeed(kSeedOne)),
            TileGrid("movie_tiles_seed_573", 573, WhenSeed(kSeedTwo)),
            TileGrid("movie_tiles_seed_12345", 12345, WhenSeed(kSeedThree))};
}

std::vector<Clip> SceneClips() {
    return {Clip{.id = "fog_white_dome",
                 .end = kEndingFrames,
                 .command = FogCmd{.enabled = true,
                                   .color = {1.0, 1.0, 1.0},
                                   .start = 50.0,
                                   .end = 80.0,
                                   .density = Widen(kFogDensity)}}};
}

OptionSpec LatticeOption() {
    return OptionSpec{.id = kLatticeOption,
                      .label = "Tile lattice",
                      .transition = {.frames = 0, .step = 4},
                      .choices = {ChoiceSpec{.label = kSeedOne}, ChoiceSpec{.label = kSeedTwo},
                                  ChoiceSpec{.label = kSeedThree}}};
}

}

Document Iidx12Ending() {
    return Document{.id = "iidx12-ending",
                    .name = "Staff roll",
                    .build = "iidx12",
                    .length = kEndingFrames,
                    .render = RenderSpec{.clear_color = {1.0, 1.0, 1.0}},
                    .camera = EndingCamera(),
                    .lights = {LightSpec{.direction = {1.0, 1.0, 1.0},
                                         .specular = {0.0, 0.0, 0.0},
                                         .ambient = {1.0, 1.0, 1.0}}},
                    .assets = {Scene3dAsset("sky", "data/graph/model/sky")},
                    .options = {LatticeOption()},
                    .markers = {Marker{.frame = 0, .label = "Title plate blooms in"},
                                Marker{.frame = kSkyFrame, .label = "Sky and movie tiles"},
                                Marker{.frame = kPlateGoneFrame, .label = "Plate gone"},
                                Marker{.frame = kRampFrame, .label = "Speed ramp and burst"},
                                Marker{.frame = kFadeFrame, .label = "Fade"}},
                    .tracks = {
                        ModelTrack("sky", "sky", SkyClips()),
                        ModelTrack("sky_speed", "sky", SkySpeedClips()),
                        CameraTrack("camera", CameraClips()),
                        PolyTrack("movie_tiles", TileClips()),
                        SceneTrack("scene", SceneClips()),
                    }};
}

}
