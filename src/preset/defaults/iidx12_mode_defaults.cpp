#include "preset/defaults/defaults_build.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Preset::Doc {

using namespace Build;

namespace {

constexpr int kModeSelectFrames = 1201;
constexpr int kModeFlyInFrames = 40;
constexpr float kModeFlyInRate = 0.025252523F;
constexpr float kModeAtHeight = std::bit_cast<float>(0x3EDE3D44U);
constexpr float kModeEyeToY = std::bit_cast<float>(0x3CF7EBC8U);
constexpr float kModeEyeToZ = std::bit_cast<float>(0xBE8D0F2EU);
constexpr float kModeAtToZ = std::bit_cast<float>(0x3F66A04CU);

Vec3 Mix(const Vec3& from, const Vec3& to, double weight) {
    Vec3 out;
    for (std::size_t i = 0; i < out.size(); i++)
        out[i] = (from[i] * (1.0 - weight)) + (to[i] * weight);
    return out;
}

Key FlyInKey(int frame) {
    const double t = std::min((double)frame * kModeFlyInRate, 1.0);
    const double s = std::sin(t);
    const Vec3 eye = Mix(Widen(0.0F, 0.3F, 0.0F), Widen(0.0F, kModeEyeToY, kModeEyeToZ), s);
    const Vec3 at =
        Mix(Widen(0.0F, kModeAtHeight, 0.0F), Widen(0.0F, kModeAtHeight, kModeAtToZ), t);
    return Key{.at = frame,
               .values = {KeyValue{.id = "eye", .value = eye}, KeyValue{.id = "at", .value = at}}};
}

Clip CameraFlyIn() {
    Clip clip;
    clip.id = "camera_fly_in";
    clip.end = kModeSelectFrames;
    clip.command = CameraTween{};
    for (int frame = 0; frame <= kModeFlyInFrames; frame++)
        clip.keys.push_back(FlyInKey(frame));
    return clip;
}

CameraSpec ModeCamera() {
    CameraSpec camera = DefaultLens();
    camera.eye = Widen(0.0F, 0.3F, 0.0F);
    camera.at = Widen(0.0F, kModeAtHeight, 0.0F);
    camera.fov_y = Widen(-4.7528005F);
    return camera;
}

}

Document Iidx12ModeSelect() {
    return Document{
        .id = "iidx12-mode-select",
        .name = "Mode select",
        .build = "iidx12",
        .length = kModeSelectFrames,
        .render = RenderSpec{.clear_color = {1.0, 1.0, 1.0}},
        .camera = ModeCamera(),
        .lights = {LightSpec{
            .direction = {1.0, 1.0, 1.0}, .specular = {0.0, 0.0, 0.0}, .ambient = {1.0, 1.0, 1.0}}},
        .assets = {Scene3dAsset("mode_bg", "data/graph/model/mode_bg")},
        .markers = {Marker{.frame = 0, .label = "Fly in"},
                    Marker{.frame = kModeFlyInFrames, .label = "Settled"}},
        .tracks = {
            ModelTrack("harfsky", "harfsky",
                       {Clip{.id = "harfsky_mode_select",
                             .end = kModeSelectFrames,
                             .command = ModelDraw{.asset = "mode_bg",
                                                  .anim_speed = 1.0,
                                                  .clip_time = {.clock = ClipClock::Restart}}}}),
            CameraTrack("camera_tween", {CameraFlyIn()}),
        }};
}

}
