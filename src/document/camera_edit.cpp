#include "document/camera_edit.h"

#include "document/field_values.h"
#include "document/outline.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kIdField = "Camera";
constexpr std::string_view kCentreField = "Projection centre and depth";
constexpr std::string_view kFocalField = "Focal length";

Support::Expected<void, std::string> SetId(AfpAnimation::Camera& camera, std::string_view value) {
    std::optional<uint16_t> id;
    auto set = SetScalar(id, value);
    if (!set) return set;
    if (!id) return Support::Unexpected(std::string("a camera tag always carries a number"));
    camera.id = *id;
    return {};
}

}

std::optional<std::size_t> CameraTag(const AfpAnimation::Container& clip, uint32_t frame) {
    if (frame >= clip.frames.size()) return std::nullopt;
    const AfpAnimation::Frame& owner = clip.frames[frame];
    std::optional<std::size_t> found;
    for (uint32_t i = 0; i < owner.tag_count; i++) {
        const std::size_t index = owner.first_tag + i;
        if (index >= clip.tags.size()) break;
        if (std::holds_alternative<AfpAnimation::Camera>(clip.tags[index].body)) found = index;
    }
    return found;
}

std::vector<Field> CameraFields(const AfpAnimation::Camera& camera) {
    return {Field{.name = std::string(kIdField), .value = std::to_string(camera.id)},
            Field{.name = std::string(kCentreField), .value = Vector(camera.position)},
            Field{.name = std::string(kFocalField), .value = Scalar(camera.focal_length)}};
}

bool CameraFieldIsEditable(std::string_view name) {
    return name == kIdField || name == kCentreField || name == kFocalField;
}

Support::Expected<void, std::string> SetCameraField(AfpAnimation::Camera& camera,
                                                    std::string_view name, std::string_view value) {
    if (name == kIdField) return SetId(camera, value);
    if (name == kCentreField) return SetVector(camera.position, value);
    if (name == kFocalField) return SetScalar(camera.focal_length, value);
    return Support::Unexpected(std::string(name) + " is not an editable camera field");
}

Support::Expected<void, std::string> AddCamera(AfpAnimation::Animation& animation, uint32_t frame,
                                               uint16_t id) {
    if (frame >= animation.root.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(frame));
    if (CameraTag(animation.root, frame))
        return Support::Unexpected("frame " + std::to_string(frame) + " already places a camera");

    AfpAnimation::Camera camera;
    camera.id = id;
    camera.position = std::array<int32_t, 3>{};
    camera.focal_length = 0;
    InsertTag(animation.root, frame, AfpAnimation::Tag{camera});
    return {};
}

Support::Expected<void, std::string> RemoveCamera(AfpAnimation::Animation& animation,
                                                  uint32_t frame) {
    const std::optional<std::size_t> found = CameraTag(animation.root, frame);
    if (!found) return Support::Unexpected("frame " + std::to_string(frame) + " places no camera");
    EraseTag(animation.root, *found);
    return {};
}

}
