#include "document/inspector.h"

#include "document/authored.h"
#include "document/camera_edit.h"
#include "document/clip.h"
#include "document/filter_fields.h"
#include "document/filter_values.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/library_call.h"
#include "document/outline.h"
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
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kFilters = "Filters";

InspectedRow Plain(std::string name, std::string value) {
    return InspectedRow{.field = Field{.name = std::move(name), .value = std::move(value)},
                        .edits = EditTarget::None};
}

void AppendOwned(std::vector<InspectedRow>& rows, const Selection& selection) {
    const AuthoredDepth& owned = *selection.owned;
    rows.push_back(Plain("Owned by the project", "frames " + std::to_string(owned.first_frame) +
                                                     " to " + std::to_string(owned.last_frame) +
                                                     ", edited through its keyframes"));
    if (selection.key_property.empty() || !selection.key_frame) return;
    const std::optional<Keyframe> key = KeyAt(owned, selection.key_property, *selection.key_frame);
    if (!key) return;

    rows.push_back(Plain("Keyframe", selection.key_property + " on frame " +
                                         std::to_string(*selection.key_frame)));
    const auto filters = selection.key_property == kFilters
                             ? FiltersFrom(key->value)
                             : Support::Expected<std::vector<AfpAnimation::Filter>, std::string>(
                                   Support::Unexpected(std::string()));
    if (filters) {
        for (const Field& field : FilterFields(*filters)) {
            rows.push_back(InspectedRow{.field = field,
                                        .edits = FilterFieldIsEditable(field.name)
                                                     ? EditTarget::KeyFilter
                                                     : EditTarget::None});
        }
    } else {
        rows.push_back(
            InspectedRow{.field = Field{.name = "Keyframe value", .value = KeyValueText(*key)},
                         .edits = EditTarget::KeyValue});
    }
    rows.push_back(Plain("Keyframe leaves as", std::string(EaseName(key->ease))));
}

void AppendPlacement(std::vector<InspectedRow>& rows, const AfpAnimation::Animation& animation,
                     const AfpAnimation::Placement& placement, bool owned) {
    for (const Field& field : PlacementFields(animation, placement)) {
        const bool editable = !owned && PlacementFieldIsEditable(field.name);
        rows.push_back(InspectedRow{.field = field,
                                    .edits = editable ? EditTarget::Placement : EditTarget::None});
    }
    if (!placement.clip_actions) return;
    for (const AfpAnimation::ClipEvent& event : placement.clip_actions->events) {
        for (const Field& field : ScriptFields(animation, event.bytecode)) {
            const bool editable = !owned && CallArgumentIndex(field.name).has_value();
            rows.push_back(InspectedRow{
                .field = field, .edits = editable ? EditTarget::CallArgument : EditTarget::None});
        }
    }
}

void AppendCamera(std::vector<InspectedRow>& rows, const AfpAnimation::Container& clip,
                  uint32_t frame) {
    const std::optional<std::size_t> tag = CameraTag(clip, frame);
    if (!tag) return;
    const auto* camera = std::get_if<AfpAnimation::Camera>(&clip.tags[*tag].body);
    if (camera == nullptr) return;
    for (const Field& field : CameraFields(*camera)) {
        rows.push_back(InspectedRow{.field = field,
                                    .edits = CameraFieldIsEditable(field.name) ? EditTarget::Camera
                                                                               : EditTarget::None});
    }
}

}

std::vector<InspectedRow> InspectFrame(const AfpAnimation::Animation& animation,
                                       const Selection& selection) {
    std::vector<InspectedRow> rows;
    const AfpAnimation::Container* clip = FindClip(animation, selection.clip);
    if (clip == nullptr) {
        rows.push_back(Plain("Clip", MissingClipMessage(selection.clip)));
        return rows;
    }
    const bool owned = selection.owned != nullptr;
    if (owned) AppendOwned(rows, selection);

    if (selection.depth) {
        const std::optional<std::size_t> tag =
            LivePlacementTag(*clip, *selection.depth, selection.frame);
        const auto* placement =
            tag ? std::get_if<AfpAnimation::Placement>(&clip->tags[*tag].body) : nullptr;
        if (placement == nullptr) {
            rows.push_back(
                Plain("Depth", std::to_string(*selection.depth) + " holds nothing here"));
        } else {
            AppendPlacement(rows, animation, *placement, owned);
        }
    }
    AppendCamera(rows, *clip, selection.frame);
    return rows;
}

}
