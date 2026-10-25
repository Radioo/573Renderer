#include "document/clip.h"

#include "document/outline.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Document {

namespace {

std::string StringAt(const AfpAnimation::Animation& animation, AfpAnimation::StringId id) {
    return id < animation.strings.size() ? animation.strings[id] : std::string();
}

std::string ExportName(const AfpAnimation::Animation& animation, uint16_t sprite) {
    const auto exported = std::ranges::find(animation.exports, sprite, &AfpAnimation::Export::tag);
    if (exported == animation.exports.end()) return {};
    return StringAt(animation, exported->name);
}

template <typename AnimationT>
auto ClipIn(AnimationT& animation, ClipId clip) -> decltype(&animation.root) {
    if (!clip.sprite) return &animation.root;
    for (auto& tag : animation.root.tags) {
        auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite != nullptr && sprite->id == *clip.sprite) return &sprite->container;
    }
    return nullptr;
}

}

std::vector<ClipSummary> Clips(const AfpAnimation::Animation& animation) {
    std::vector<ClipSummary> clips;
    clips.push_back(
        ClipSummary{.id = ClipId{},
                    .name = {},
                    .frame_count = static_cast<uint32_t>(animation.root.frames.size())});
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr) continue;
        clips.push_back(
            ClipSummary{.id = ClipId{.sprite = sprite->id},
                        .name = ExportName(animation, sprite->id),
                        .frame_count = static_cast<uint32_t>(sprite->container.frames.size())});
    }
    return clips;
}

std::string ClipLabel(const ClipSummary& clip) {
    if (!clip.id.sprite) return "Root";
    const std::string number = std::to_string(*clip.id.sprite);
    if (clip.name.empty()) return "Sprite " + number;
    return clip.name + " (sprite " + number + ")";
}

const AfpAnimation::Container* FindClip(const AfpAnimation::Animation& animation, ClipId clip) {
    return ClipIn(animation, clip);
}

AfpAnimation::Container* FindClip(AfpAnimation::Animation& animation, ClipId clip) {
    return ClipIn(animation, clip);
}

Support::Expected<AfpAnimation::Container*, std::string>
RequireClip(AfpAnimation::Animation& animation, ClipId clip) {
    AfpAnimation::Container* found = FindClip(animation, clip);
    if (found == nullptr) return Support::Unexpected(MissingClipMessage(clip));
    return found;
}

std::string MissingClipMessage(ClipId clip) {
    return "sprite " + std::to_string(clip.sprite.value_or(0)) + " is no longer in this animation";
}

std::optional<AnimationDetails> DescribeClip(const AfpAnimation::Animation& animation,
                                             ClipId clip) {
    const AfpAnimation::Container* found = FindClip(animation, clip);
    if (found == nullptr) return std::nullopt;
    AnimationDetails details{.frame_count = static_cast<uint32_t>(found->frames.size()),
                             .labels = {},
                             .depths = DepthRows(*found)};
    for (const AfpAnimation::Label& label : found->labels) {
        details.labels.push_back(
            AnimationLabel{.name = StringAt(animation, label.name), .frame = label.frame});
    }
    return details;
}

}
