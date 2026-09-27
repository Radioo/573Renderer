#include "document/clip_edit.h"

#include "document/camera_edit.h"
#include "document/clip.h"
#include "document/library_call.h"
#include "document/placement_edit.h"
#include "document/placement_effect.h"
#include "document/script_source.h"
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
    const auto shown = ReplayDepth(**found, depth, frame, frame);
    auto written = SetPlacementField(animation, **placement, field, value);
    if (!written) return written;
    if (shown.empty()) return written;
    const uint32_t missing = ControlsNeeded(**placement) & ~(*placement)->flags;
    if (missing == 0) return written;
    CarryApplied(**placement, shown.back().second, missing);
    (*placement)->flags |= missing;
    return written;
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

std::optional<std::size_t> FrameScriptTag(const AfpAnimation::Container& clip, uint32_t frame) {
    if (frame >= clip.frames.size()) return std::nullopt;
    const AfpAnimation::Frame& owner = clip.frames[frame];
    for (uint32_t at = 0; at < owner.tag_count; at++) {
        const std::size_t index = owner.first_tag + at;
        if (index >= clip.tags.size()) break;
        if (std::holds_alternative<AfpAnimation::Action>(clip.tags[index].body)) return index;
    }
    return std::nullopt;
}

Support::Expected<void, std::string> WriteFrameScript(AfpAnimation::Animation& animation,
                                                      ClipId clip, uint32_t frame,
                                                      std::string_view source) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const std::optional<std::size_t> tag = FrameScriptTag(**found, frame);
    if (!tag) return Support::Unexpected("frame " + std::to_string(frame) + " carries no script");
    auto* action = std::get_if<AfpAnimation::Action>(&(*found)->tags[*tag].body);
    if (action == nullptr) return Support::Unexpected(std::string("that tag is not a script"));
    auto code = CompileScript(animation, source);
    if (!code) return Support::Unexpected(code.error());
    action->bytecode = std::move(*code);
    return {};
}

Support::Expected<void, std::string> WritePlacementScript(AfpAnimation::Animation& animation,
                                                          ClipId clip, uint16_t depth,
                                                          uint32_t frame, std::string_view source) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    auto placement = LivePlacement(**found, depth, frame);
    if (!placement) return Support::Unexpected(placement.error());
    std::optional<AfpAnimation::ClipActions>& actions = (*placement)->clip_actions;
    if (!actions.has_value() || actions->events.empty())
        return Support::Unexpected(std::string("that placement carries no script"));
    auto code = CompileScript(animation, source);
    if (!code) return Support::Unexpected(code.error());
    actions->events.front().bytecode = std::move(*code);
    return {};
}

Support::Expected<void, std::string> EditFrameCallArgument(AfpAnimation::Animation& animation,
                                                           ClipId clip, uint32_t frame,
                                                           std::size_t index,
                                                           std::string_view value) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const std::optional<std::size_t> tag = FrameScriptTag(**found, frame);
    if (!tag) return Support::Unexpected("frame " + std::to_string(frame) + " carries no script");
    auto* action = std::get_if<AfpAnimation::Action>(&(*found)->tags[*tag].body);
    if (action == nullptr) return Support::Unexpected(std::string("that tag is not a script"));

    std::optional<LibraryCall> call = ReadLibraryCall(animation, action->bytecode);
    if (!call) return Support::Unexpected(std::string("that script is not a library call"));
    if (index >= call->arguments.size()) {
        return Support::Unexpected("the call takes " + std::to_string(call->arguments.size()) +
                                   " arguments, not " + std::to_string(index + 1));
    }
    call->arguments[index].text = std::string(value);
    auto written = WriteLibraryCall(animation, action->bytecode, *call);
    if (!written) return Support::Unexpected(written.error());
    action->bytecode = std::move(*written);
    return {};
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
