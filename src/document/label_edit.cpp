#include "document/label_edit.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kLinearLabelLookup = 0x8;

void SortLabels(const AfpAnimation::Animation& animation, AfpAnimation::Container& clip) {
    if ((animation.flags & kLinearLabelLookup) != 0) {
        std::ranges::stable_sort(clip.labels, {}, &AfpAnimation::Label::frame);
        return;
    }
    std::ranges::stable_sort(
        clip.labels, [&](const AfpAnimation::Label& a, const AfpAnimation::Label& b) {
            return StringText(animation, a.name) < StringText(animation, b.name);
        });
}

AfpAnimation::Label* Find(const AfpAnimation::Animation& animation, AfpAnimation::Container& clip,
                          std::string_view name) {
    for (AfpAnimation::Label& label : clip.labels) {
        if (StringText(animation, label.name) == name) return &label;
    }
    return nullptr;
}

Support::Expected<void, std::string> CheckFrame(const AfpAnimation::Container& clip,
                                                uint32_t frame) {
    if (frame >= clip.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(frame));
    if (frame > 0xFFFF) return Support::Unexpected(std::string("a label frame is a u16"));
    return {};
}

Support::Expected<void, std::string> CheckName(const AfpAnimation::Animation& animation,
                                               AfpAnimation::Container& clip,
                                               std::string_view name) {
    if (name.empty()) return Support::Unexpected(std::string("a label needs a name"));
    if (Find(animation, clip, name) != nullptr)
        return Support::Unexpected("the clip already has a label called " + std::string(name));
    return {};
}

}

Support::Expected<void, std::string> AddLabel(AfpAnimation::Animation& animation, ClipId clip,
                                              std::string_view name, uint32_t frame) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    auto named = CheckName(animation, target, name);
    if (!named) return Support::Unexpected(named.error());
    auto framed = CheckFrame(target, frame);
    if (!framed) return Support::Unexpected(framed.error());
    const AfpAnimation::StringId id = InternString(animation, name);
    target.labels.push_back(AfpAnimation::Label{.frame = static_cast<uint16_t>(frame), .name = id});
    SortLabels(animation, target);
    return {};
}

Support::Expected<void, std::string> RenameLabel(AfpAnimation::Animation& animation, ClipId clip,
                                                 std::string_view name, std::string_view renamed) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    AfpAnimation::Label* label = Find(animation, target, name);
    if (label == nullptr)
        return Support::Unexpected("the clip has no label called " + std::string(name));
    auto named = CheckName(animation, target, renamed);
    if (!named) return Support::Unexpected(named.error());
    label->name = InternString(animation, renamed);
    SortLabels(animation, target);
    CompactStrings(animation);
    return {};
}

Support::Expected<void, std::string> MoveLabel(AfpAnimation::Animation& animation, ClipId clip,
                                               std::string_view name, uint32_t frame) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    AfpAnimation::Label* label = Find(animation, target, name);
    if (label == nullptr)
        return Support::Unexpected("the clip has no label called " + std::string(name));
    auto framed = CheckFrame(target, frame);
    if (!framed) return Support::Unexpected(framed.error());
    label->frame = static_cast<uint16_t>(frame);
    SortLabels(animation, target);
    return {};
}

Support::Expected<void, std::string> RemoveLabel(AfpAnimation::Animation& animation, ClipId clip,
                                                 std::string_view name) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    const AfpAnimation::Label* label = Find(animation, target, name);
    if (label == nullptr)
        return Support::Unexpected("the clip has no label called " + std::string(name));
    const AfpAnimation::StringId id = label->name;
    const auto gone = std::ranges::remove(target.labels, id, &AfpAnimation::Label::name);
    target.labels.erase(gone.begin(), gone.end());
    CompactStrings(animation);
    return {};
}

}
