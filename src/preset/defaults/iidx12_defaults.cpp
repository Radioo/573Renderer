#include "preset/defaults/defaults_build.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <utility>
#include <vector>

namespace Preset::Doc {

using namespace Build;

namespace {

constexpr int kExpertSelectFrames = 2760;
constexpr int kExpertClearPeriod = 5400;

Vec3 Level(double r, double g, double b) {
    return Vec3{r / 255.0, g / 255.0, b / 255.0};
}

Clip ExpertClearBreath() {
    const Vec3 blue = Level(0.0, 48.0, 96.0);
    const Vec3 red = Level(96.0, 0.0, 0.0);
    Clip clip;
    clip.id = "clear_breathing";
    clip.end = kExpertSelectFrames;
    clip.command = RenderSettingsCmd{.clear_color = blue};
    clip.keys = {
        Key{.at = 0, .values = {KeyValue{.id = "clear_color", .value = blue}}},
        Key{.at = kExpertClearPeriod / 2, .values = {KeyValue{.id = "clear_color", .value = red}}},
        Key{.at = kExpertClearPeriod, .values = {KeyValue{.id = "clear_color", .value = blue}}}};
    return clip;
}

LightSpec ExpertLight(Vec3 direction) {
    return LightSpec{.direction = direction,
                     .diffuse = {1.0, 1.0, 1.0},
                     .specular = {0.0, 0.5, 1.0},
                     .ambient = {1.0, 1.0, 1.0}};
}

CameraSpec ExpertCamera() {
    CameraSpec camera = WideLensAt(Widen(-0.2F, -0.2F, -0.2F), Widen(1.0F, 0.78F, 1.0F));
    camera.up = Vec3{0.0, 100.0, 0.0};
    return camera;
}

Document Iidx12ExpertSelect() {
    return Document{
        .id = "iidx12-expert-select",
        .name = "Expert course select",
        .build = "iidx12",
        .length = kExpertSelectFrames,
        .render = RenderSpec{.clear_color = Level(0.0, 48.0, 96.0)},
        .camera = ExpertCamera(),
        .lights = {ExpertLight(Widen(0.78F, -0.5F, 0.67F)), ExpertLight(Vec3{0.0, 1.0, 0.0})},
        .assets = {Scene3dAsset("ex_bg", "data/graph/model/ex_bg")},
        .markers = {Marker{.frame = 0, .label = "Course select"}},
        .tracks = {
            ModelTrack("ex_sky", "ex_sky",
                       {Clip{.id = "ex_sky_course_select",
                             .end = kExpertSelectFrames,
                             .command = ModelDraw{.asset = "ex_bg",
                                                  .blend_mode = ModelBlend::Additive,
                                                  .anim_speed = 1.0,
                                                  .clip_time = {.clock = ClipClock::Restart}}}}),
            SceneTrack("scene", {ExpertClearBreath()}),
        }};
}

}

std::vector<Document> Iidx12Defaults() {
    std::vector<Document> documents = {Iidx12ExpertSelect(), Iidx12ModeSelect(),
                                       Iidx12MusicSelect(), Iidx12DanSelect(), Iidx12Ending()};
    for (Document& document : Iidx12TwoD())
        documents.push_back(std::move(document));
    return documents;
}

}
