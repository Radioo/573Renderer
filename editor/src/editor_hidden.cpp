#include "editor_timeline.h"
#include "editor_window.h"

#include "document/clip.h"
#include "document/hidden_depths.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Editor {

std::vector<uint16_t> Window::HiddenHere() const {
    std::vector<uint16_t> depths;
    for (const Document::HiddenDepth& hidden : hidden_) {
        if (hidden.animation == animation_path_ && hidden.clip == clip_)
            depths.push_back(hidden.depth);
    }
    return depths;
}

bool Window::IsHidden(uint16_t depth) const {
    return std::ranges::find(hidden_, Document::HiddenDepth{.animation = animation_path_,
                                                            .clip = clip_,
                                                            .depth = depth}) != hidden_.end();
}

void Window::ToggleHidden(uint16_t depth) {
    const Document::HiddenDepth entry{.animation = animation_path_, .clip = clip_, .depth = depth};
    const auto found = std::ranges::find(hidden_, entry);
    if (found == hidden_.end()) {
        hidden_.push_back(entry);
    } else {
        hidden_.erase(found);
    }
    Reload();
    UpdateHiddenRows();
}

void Window::ShowEveryDepth() {
    hidden_.clear();
    Reload();
    UpdateHiddenRows();
}

void Window::UpdateHiddenRows() {
    timeline_->SetHiddenDepths(HiddenHere());
}

std::vector<Document::StageOutline>
Window::VisibleOutlines(const AfpAnimation::Animation& animation) const {
    std::vector<Document::StageOutline> outlines =
        Document::StageOutlines(animation, clip_, frame_, shape_bounds_);
    const std::vector<uint16_t> hidden = HiddenHere();
    std::erase_if(outlines, [&hidden](const Document::StageOutline& outline) {
        return std::ranges::find(hidden, outline.depth) != hidden.end();
    });
    return outlines;
}

}
