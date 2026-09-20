#include "document/frame_notes.h"

#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace Document {

std::vector<FrameNote> FrameNotes(const AfpAnimation::Container& clip) {
    std::vector<FrameNote> notes;
    for (std::size_t index = 0; index < clip.frames.size(); index++) {
        const AfpAnimation::Frame& frame = clip.frames[index];
        FrameNote note{.frame = static_cast<uint32_t>(index), .script = false, .camera = false};
        for (uint32_t tag = 0; tag < frame.tag_count; tag++) {
            const std::size_t position = frame.first_tag + tag;
            if (position >= clip.tags.size()) break;
            const AfpAnimation::Tag& one = clip.tags[position];
            if (std::holds_alternative<AfpAnimation::Action>(one.body)) note.script = true;
            if (std::holds_alternative<AfpAnimation::Camera>(one.body)) note.camera = true;
        }
        if (note.script || note.camera) notes.push_back(note);
    }
    return notes;
}

}
