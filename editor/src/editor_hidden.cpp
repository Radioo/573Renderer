#include "editor_timeline.h"
#include "editor_window.h"

#include "document/clip.h"
#include "document/hidden_depths.h"
#include "document/stage_bounds.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <utility>
#include <cstdint>
#include <vector>

namespace Editor {

std::vector<uint16_t> Window::HiddenHere() const {
    std::vector<uint16_t> depths;
    for (const Document::DepthInClip& hidden : hidden_) {
        if (hidden.animation == animation_path_ && hidden.clip == clip_)
            depths.push_back(hidden.depth);
    }
    return depths;
}

bool Window::IsHidden(uint16_t depth) const {
    return std::ranges::find(hidden_, Document::DepthInClip{.animation = animation_path_,
                                                            .clip = clip_,
                                                            .depth = depth}) != hidden_.end();
}

void Window::ToggleHidden(uint16_t depth) {
    const Document::DepthInClip entry{.animation = animation_path_, .clip = clip_, .depth = depth};
    const auto found = std::ranges::find(hidden_, entry);
    if (found == hidden_.end()) {
        hidden_.push_back(entry);
    } else {
        hidden_.erase(found);
    }
    Reload();
    UpdateViewRows();
}

void Window::ShowEveryDepth() {
    hidden_.clear();
    Reload();
    UpdateViewRows();
}

void Window::UnlockEveryDepth() {
    locked_.clear();
    ShowFrame();
    UpdateViewRows();
}

void Window::UpdateViewRows() {
    timeline_->SetHiddenDepths(HiddenHere());
    std::vector<uint16_t> locked;
    for (const Document::DepthInClip& entry : locked_) {
        if (entry.animation == animation_path_ && entry.clip == clip_)
            locked.push_back(entry.depth);
    }
    timeline_->SetLockedDepths(std::move(locked));
}

bool Window::IsLocked(uint16_t depth) const {
    return std::ranges::find(locked_, Document::DepthInClip{.animation = animation_path_,
                                                            .clip = clip_,
                                                            .depth = depth}) != locked_.end();
}

void Window::ToggleLocked(uint16_t depth) {
    const Document::DepthInClip entry{.animation = animation_path_, .clip = clip_, .depth = depth};
    const auto found = std::ranges::find(locked_, entry);
    if (found == locked_.end()) {
        locked_.push_back(entry);
    } else {
        locked_.erase(found);
    }
    ShowFrame();
    UpdateViewRows();
}

void Window::SoloDepth(uint16_t depth) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;
    const auto details = Document::DescribeClip(*animation, clip_);
    if (!details) return;
    std::erase_if(hidden_, [this](const Document::DepthInClip& entry) {
        return entry.animation == animation_path_ && entry.clip == clip_;
    });
    for (const Document::DepthRow& row : details->depths) {
        if (row.depth == depth) continue;
        hidden_.push_back(
            Document::DepthInClip{.animation = animation_path_, .clip = clip_, .depth = row.depth});
    }
    Reload();
    UpdateViewRows();
}

std::vector<Document::StageOutline>
Window::VisibleOutlines(const AfpAnimation::Animation& animation) const {
    std::vector<Document::StageOutline> outlines =
        Document::StageOutlines(animation, clip_, frame_, shape_bounds_);
    const std::vector<uint16_t> hidden = HiddenHere();
    std::erase_if(outlines, [this, &hidden](const Document::StageOutline& outline) {
        return std::ranges::find(hidden, outline.depth) != hidden.end() || IsLocked(outline.depth);
    });
    return outlines;
}

}
