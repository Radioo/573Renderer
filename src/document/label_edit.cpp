#include "document/label_edit.h"

#include "document/animation_strings.h"
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

void SortLabels(AfpAnimation::Animation& animation) {
    if ((animation.flags & kLinearLabelLookup) != 0) {
        std::ranges::stable_sort(animation.root.labels, {}, &AfpAnimation::Label::frame);
        return;
    }
    std::ranges::stable_sort(
        animation.root.labels, [&](const AfpAnimation::Label& a, const AfpAnimation::Label& b) {
            return StringText(animation, a.name) < StringText(animation, b.name);
        });
}

AfpAnimation::Label* Find(AfpAnimation::Animation& animation, std::string_view name) {
    for (AfpAnimation::Label& label : animation.root.labels) {
        if (StringText(animation, label.name) == name) return &label;
    }
    return nullptr;
}

Support::Expected<void, std::string> CheckFrame(const AfpAnimation::Animation& animation,
                                                uint32_t frame) {
    if (frame >= animation.root.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(frame));
    if (frame > 0xFFFF) return Support::Unexpected(std::string("a label frame is a u16"));
    return {};
}

Support::Expected<void, std::string> CheckName(AfpAnimation::Animation& animation,
                                               std::string_view name) {
    if (name.empty()) return Support::Unexpected(std::string("a label needs a name"));
    if (Find(animation, name) != nullptr)
        return Support::Unexpected("the clip already has a label called " + std::string(name));
    return {};
}

}

Support::Expected<void, std::string> AddLabel(AfpAnimation::Animation& animation,
                                              std::string_view name, uint32_t frame) {
    auto named = CheckName(animation, name);
    if (!named) return Support::Unexpected(named.error());
    auto framed = CheckFrame(animation, frame);
    if (!framed) return Support::Unexpected(framed.error());
    animation.root.labels.push_back(AfpAnimation::Label{.frame = static_cast<uint16_t>(frame),
                                                        .name = InternString(animation, name)});
    SortLabels(animation);
    return {};
}

Support::Expected<void, std::string> RenameLabel(AfpAnimation::Animation& animation,
                                                 std::string_view name, std::string_view renamed) {
    AfpAnimation::Label* label = Find(animation, name);
    if (label == nullptr)
        return Support::Unexpected("the clip has no label called " + std::string(name));
    auto named = CheckName(animation, renamed);
    if (!named) return Support::Unexpected(named.error());
    label->name = InternString(animation, renamed);
    SortLabels(animation);
    CompactStrings(animation);
    return {};
}

Support::Expected<void, std::string> MoveLabel(AfpAnimation::Animation& animation,
                                               std::string_view name, uint32_t frame) {
    AfpAnimation::Label* label = Find(animation, name);
    if (label == nullptr)
        return Support::Unexpected("the clip has no label called " + std::string(name));
    auto framed = CheckFrame(animation, frame);
    if (!framed) return Support::Unexpected(framed.error());
    label->frame = static_cast<uint16_t>(frame);
    SortLabels(animation);
    return {};
}

Support::Expected<void, std::string> RemoveLabel(AfpAnimation::Animation& animation,
                                                 std::string_view name) {
    const AfpAnimation::Label* label = Find(animation, name);
    if (label == nullptr)
        return Support::Unexpected("the clip has no label called " + std::string(name));
    const AfpAnimation::StringId id = label->name;
    const auto gone = std::ranges::remove(animation.root.labels, id, &AfpAnimation::Label::name);
    animation.root.labels.erase(gone.begin(), gone.end());
    CompactStrings(animation);
    return {};
}

}
