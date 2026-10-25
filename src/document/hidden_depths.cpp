#include "document/hidden_depths.h"

#include "document/clip.h"
#include "document/document.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

std::optional<uint16_t> DepthOf(const AfpAnimation::Tag& tag) {
    if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body))
        return placement->depth;
    if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) return remove->depth;
    return std::nullopt;
}

void Filter(AfpAnimation::Container& clip, const std::vector<uint16_t>& depths) {
    std::vector<AfpAnimation::Tag> kept;
    kept.reserve(clip.tags.size());
    std::vector<AfpAnimation::Frame> frames;
    frames.reserve(clip.frames.size());
    std::size_t next = 0;
    for (const AfpAnimation::Frame& frame : clip.frames) {
        for (; next < clip.tags.size() && next < std::size_t{frame.first_tag}; next++)
            kept.push_back(std::move(clip.tags[next]));
        AfpAnimation::Frame rebuilt{.first_tag = static_cast<uint32_t>(kept.size()),
                                    .tag_count = 0};
        const std::size_t end =
            std::min<std::size_t>(clip.tags.size(), std::size_t{frame.first_tag} + frame.tag_count);
        for (; next < end; next++) {
            const std::optional<uint16_t> depth = DepthOf(clip.tags[next]);
            if (depth && std::ranges::find(depths, *depth) != depths.end()) continue;
            kept.push_back(std::move(clip.tags[next]));
            rebuilt.tag_count++;
        }
        frames.push_back(rebuilt);
    }
    for (; next < clip.tags.size(); next++)
        kept.push_back(std::move(clip.tags[next]));
    clip.tags = std::move(kept);
    clip.frames = std::move(frames);
}

}

Support::Expected<void, std::string> HideDepths(AfpAnimation::Animation& animation, ClipId clip,
                                                const std::vector<uint16_t>& depths) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    Filter(**found, depths);
    return {};
}

Support::Expected<File, std::string> ViewWithout(const File& file,
                                                 const std::vector<DepthInClip>& hidden) {
    File view = file;
    std::set<std::string> animations;
    for (const DepthInClip& one : hidden)
        animations.insert(one.animation);
    for (const std::string& path : animations) {
        auto animation = view.ReadAnimation(path);
        if (!animation) return Support::Unexpected(animation.error());
        for (const DepthInClip& one : hidden) {
            if (one.animation != path) continue;
            auto hid = HideDepths(*animation, one.clip, {one.depth});
            if (!hid) return Support::Unexpected(hid.error());
        }
        auto written = view.WriteAnimation(path, *animation);
        if (!written) return Support::Unexpected(written.error());
    }
    return view;
}

}
