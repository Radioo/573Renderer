#include "document/clip_edit.h"

#include "document/camera_edit.h"
#include "document/clip.h"
#include "document/library_call.h"
#include "document/placement_edit.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace Document {

namespace {

Support::Expected<AfpAnimation::Placement*, std::string>
LivePlacement(AfpAnimation::Container& clip, uint16_t depth, uint32_t frame) {
    const std::optional<std::size_t> tag = LivePlacementTag(clip, depth, frame);
    if (!tag) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[*tag].body);
    if (placement == nullptr)
        return Support::Unexpected(std::string("that tag is not a placement"));
    return placement;
}

}

Support::Expected<void, std::string> EditPlacementField(AfpAnimation::Animation& animation,
                                                        ClipId clip, uint16_t depth, uint32_t frame,
                                                        std::string_view field,
                                                        std::string_view value) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    auto placement = LivePlacement(**found, depth, frame);
    if (!placement) return Support::Unexpected(placement.error());
    return SetPlacementField(animation, **placement, field, value);
}

Support::Expected<void, std::string> EditCallArgument(AfpAnimation::Animation& animation,
                                                      ClipId clip, uint16_t depth, uint32_t frame,
                                                      std::size_t index, std::string_view value) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    auto placement = LivePlacement(**found, depth, frame);
    if (!placement) return Support::Unexpected(placement.error());
    std::optional<AfpAnimation::ClipActions>& actions = (*placement)->clip_actions;
    if (!actions.has_value())
        return Support::Unexpected(std::string("that placement carries no script"));

    for (AfpAnimation::ClipEvent& event : actions->events) {
        std::optional<LibraryCall> call = ReadLibraryCall(animation, event.bytecode);
        if (!call) continue;
        if (index >= call->arguments.size()) {
            return Support::Unexpected("the call takes " + std::to_string(call->arguments.size()) +
                                       " arguments, not " + std::to_string(index + 1));
        }
        call->arguments[index].text = std::string(value);
        auto written = WriteLibraryCall(animation, event.bytecode, *call);
        if (!written) return Support::Unexpected(written.error());
        event.bytecode = std::move(*written);
        return {};
    }
    return Support::Unexpected(std::string("that script is not a library call"));
}

Support::Expected<void, std::string> EditCameraField(AfpAnimation::Animation& animation,
                                                     ClipId clip, uint32_t frame,
                                                     std::string_view field,
                                                     std::string_view value) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const std::optional<std::size_t> tag = CameraTag(**found, frame);
    if (!tag) return Support::Unexpected("frame " + std::to_string(frame) + " places no camera");
    auto* camera = std::get_if<AfpAnimation::Camera>(&(*found)->tags[*tag].body);
    if (camera == nullptr) return Support::Unexpected(std::string("that tag is not a camera"));
    return SetCameraField(*camera, field, value);
}

}
