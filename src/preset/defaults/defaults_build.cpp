#include "preset/defaults/defaults_build.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <string>
#include <utility>
#include <vector>

namespace Preset::Doc::Build {

namespace {

Track Named(std::string id, TrackKind kind, std::string target, std::vector<Clip> clips) {
    Track track;
    track.id = std::move(id);
    track.name = track.id;
    track.kind = kind;
    track.target = std::move(target);
    track.clips = std::move(clips);
    return track;
}

Asset Named(std::string id, AssetKind kind, std::string dir) {
    return Asset{.id = std::move(id), .kind = kind, .dir = std::move(dir)};
}

}

double Widen(float value) {
    return (double)value;
}

Vec2 Widen(float x, float y) {
    return Vec2{(double)x, (double)y};
}

Vec3 Widen(float x, float y, float z) {
    return Vec3{(double)x, (double)y, (double)z};
}

CameraSpec DefaultLens() {
    return CameraSpec{
        .eye = {0.0, 0.0, -1.0}, .fov_y = Widen(1.0471976F), .near_z = Widen(0.1F), .far_z = 500.0};
}

CameraSpec WideLens(Vec3 eye) {
    return CameraSpec{.eye = eye,
                      .fov_y = Widen(1.0471976F),
                      .near_z = 0.0,
                      .far_z = 1000.0,
                      .aspect = {.automatic = false, .value = Widen(1.7708334F)}};
}

CameraSpec WideLensAt(Vec3 eye, Vec3 at) {
    CameraSpec camera = WideLens(eye);
    camera.at = at;
    return camera;
}

std::vector<LightSpec> StandardLights() {
    return {LightSpec{.direction = {1.0, 1.0, 1.0},
                      .specular = {0.0, 0.0, 0.0},
                      .ambient = {1.0, 1.0, 1.0}},
            LightSpec{.direction = {-1.0, -1.0, -1.0},
                      .specular = {0.0, 0.0, 0.0},
                      .ambient = {1.0, 1.0, 1.0}}};
}

Asset Scene3dAsset(std::string id, std::string dir) {
    return Named(std::move(id), AssetKind::Scene3d, std::move(dir));
}

Asset Package2dAsset(std::string id, std::string dir) {
    return Named(std::move(id), AssetKind::Package2d, std::move(dir));
}

Track SpriteTrack(std::string id, std::string target, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Sprite, std::move(target), std::move(clips));
}

Track ModelTrack(std::string id, std::string target, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Model, std::move(target), std::move(clips));
}

Track CameraTrack(std::string id, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Camera, {}, std::move(clips));
}

Track LightTrack(std::string id, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Light, {}, std::move(clips));
}

Track FxTrack(std::string id, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Fx, {}, std::move(clips));
}

Track SceneTrack(std::string id, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Scene, {}, std::move(clips));
}

Track PolyTrack(std::string id, std::vector<Clip> clips) {
    return Named(std::move(id), TrackKind::Poly, {}, std::move(clips));
}

void AppendPart(Document& document, DocumentPart part) {
    for (Marker& marker : part.markers)
        document.markers.push_back(std::move(marker));
    for (Track& track : part.tracks) {
        Track* existing = nullptr;
        for (Track& known : document.tracks) {
            if (known.id == track.id) existing = &known;
        }
        if (existing == nullptr) {
            document.tracks.push_back(std::move(track));
            continue;
        }
        for (Clip& clip : track.clips)
            existing->clips.push_back(std::move(clip));
    }
}

}
