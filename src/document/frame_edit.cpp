#include "document/frame_edit.h"

#include "document/placement_edit.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kMaxFrame = 0xFFFF;

uint16_t DepthOf(const AfpAnimation::Tag& tag) {
    if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body))
        return placement->depth;
    if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) return remove->depth;
    return 0;
}

bool TouchesDepth(const AfpAnimation::Tag& tag, uint16_t depth) {
    const bool placed = std::holds_alternative<AfpAnimation::Placement>(tag.body);
    const bool removed = std::holds_alternative<AfpAnimation::Remove>(tag.body);
    return (placed || removed) && DepthOf(tag) == depth;
}

Support::Expected<void, std::string> CheckFrames(const AfpAnimation::Animation& animation,
                                                 uint32_t first, uint32_t last) {
    if (first > last) return Support::Unexpected(std::string("the frame range runs backwards"));
    if (last >= animation.root.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(last));
    if (last >= kMaxFrame)
        return Support::Unexpected(std::string("a placement end frame is a u16"));
    return {};
}

}

Support::Expected<void, std::string> AddDepth(AfpAnimation::Animation& animation, uint16_t depth,
                                              uint16_t character, uint32_t first_frame,
                                              uint32_t last_frame) {
    auto framed = CheckFrames(animation, first_frame, last_frame);
    if (!framed) return Support::Unexpected(framed.error());
    for (uint32_t frame = first_frame; frame <= last_frame; frame++) {
        if (LivePlacementTag(animation.root, depth, frame)) {
            return Support::Unexpected("depth " + std::to_string(depth) + " is taken on frame " +
                                       std::to_string(frame));
        }
    }

    AfpAnimation::Placement placement;
    placement.depth = depth;
    placement.end_frame = static_cast<uint16_t>(last_frame + 1);
    placement.character = character;
    InsertTag(animation.root, first_frame, AfpAnimation::Tag{placement});
    if (last_frame + 1 < animation.root.frames.size()) {
        InsertTag(animation.root, last_frame + 1,
                  AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = depth}});
    }
    return {};
}

Support::Expected<void, std::string> RemoveDepth(AfpAnimation::Animation& animation, uint16_t depth,
                                                 uint32_t frame) {
    if (frame >= animation.root.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(frame));
    if (!LivePlacementTag(animation.root, depth, frame)) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }

    uint32_t first = frame;
    while (first > 0 && LivePlacementTag(animation.root, depth, first - 1))
        first--;
    uint32_t last = frame;
    while (last + 1 < animation.root.frames.size() &&
           LivePlacementTag(animation.root, depth, last + 1)) {
        last++;
    }

    std::vector<std::size_t> gone;
    const uint32_t through =
        std::min<uint32_t>(last + 1, static_cast<uint32_t>(animation.root.frames.size()) - 1);
    for (uint32_t at = first; at <= through; at++) {
        const AfpAnimation::Frame& owner = animation.root.frames[at];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= animation.root.tags.size()) break;
            if (TouchesDepth(animation.root.tags[index], depth)) gone.push_back(index);
        }
    }
    for (std::size_t i = gone.size(); i > 0; i--)
        EraseTag(animation.root, gone[i - 1]);
    return {};
}

Support::Expected<void, std::string> InsertFrame(AfpAnimation::Animation& animation, uint32_t at) {
    if (at > animation.root.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(at));
    if (animation.root.frames.size() >= kMaxFrame)
        return Support::Unexpected(std::string("the clip cannot hold another frame"));

    const uint32_t first_tag = at < animation.root.frames.size()
                                   ? animation.root.frames[at].first_tag
                                   : static_cast<uint32_t>(animation.root.tags.size());
    animation.root.frames.insert(animation.root.frames.begin() + static_cast<std::ptrdiff_t>(at),
                                 AfpAnimation::Frame{.first_tag = first_tag, .tag_count = 0});

    for (AfpAnimation::Label& label : animation.root.labels) {
        if (label.frame >= at) label.frame++;
    }
    for (AfpAnimation::Tag& tag : animation.root.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->end_frame >= at) placement->end_frame++;
    }
    return {};
}

Support::Expected<void, std::string> RemoveFrame(AfpAnimation::Animation& animation, uint32_t at) {
    if (at >= animation.root.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(at));
    if (animation.root.frames.size() == 1)
        return Support::Unexpected(std::string("a clip keeps at least one frame"));

    const AfpAnimation::Frame owner = animation.root.frames[at];
    for (uint32_t i = owner.tag_count; i > 0; i--)
        EraseTag(animation.root, owner.first_tag + i - 1);
    animation.root.frames.erase(animation.root.frames.begin() + static_cast<std::ptrdiff_t>(at));

    const auto last = static_cast<uint16_t>(animation.root.frames.size() - 1);
    for (AfpAnimation::Label& label : animation.root.labels) {
        if (label.frame > at) label.frame--;
        label.frame = std::min(label.frame, last);
    }
    for (AfpAnimation::Tag& tag : animation.root.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->end_frame > at) placement->end_frame--;
    }
    return {};
}

}
