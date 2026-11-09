#include "document/tags.h"

#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace Document {

void InsertTag(AfpAnimation::Container& clip, uint32_t frame, AfpAnimation::Tag tag) {
    AfpAnimation::Frame& owner = clip.frames[frame];
    const std::size_t at = owner.first_tag + owner.tag_count;
    clip.tags.insert(clip.tags.begin() + static_cast<std::ptrdiff_t>(at), std::move(tag));
    owner.tag_count++;
    for (std::size_t i = frame + 1; i < clip.frames.size(); i++)
        clip.frames[i].first_tag++;
}

void EraseTag(AfpAnimation::Container& clip, std::size_t at) {
    clip.tags.erase(clip.tags.begin() + static_cast<std::ptrdiff_t>(at));
    for (AfpAnimation::Frame& frame : clip.frames) {
        if (frame.first_tag > at) {
            frame.first_tag--;
        } else if (at < frame.first_tag + frame.tag_count) {
            frame.tag_count--;
        }
    }
}

}
