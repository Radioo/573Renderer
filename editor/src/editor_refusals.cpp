#include "editor_window.h"

#include "editor_timeline.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/stage_bounds.h"

#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Editor {

std::optional<QString> Window::NeedsDocument() const {
    if (!file_) return tr("Open an IFS first");
    return std::nullopt;
}

std::optional<QString> Window::NeedsAnimation() const {
    if (!file_ || animation_path_.empty()) return tr("Open an animation first");
    return std::nullopt;
}

std::optional<QString> Window::NeedsDepth() const {
    if (const std::optional<QString> refused = NeedsAnimation()) return refused;
    if (!depth_) return tr("Choose a depth first");
    return std::nullopt;
}

std::optional<QString> Window::NeedsDepths(std::size_t fewest) const {
    if (const std::optional<QString> refused = NeedsDepth()) return refused;
    if (SelectedDepths().size() < fewest)
        return fewest == 2 ? tr("Choose two or more depths first")
                           : tr("Choose %1 or more depths first").arg(fewest);
    return std::nullopt;
}

std::optional<QString> Window::OwnedRefusal(uint16_t depth, uint32_t frame,
                                            const QString& doing) const {
    if (!AuthoredIndexAt(depth, frame)) return std::nullopt;
    return tr("The project owns depth %1 here. Detach it before %2.").arg(depth).arg(doing);
}

std::optional<QString> Window::ChosenDepthRefusal(const QString& doing) const {
    if (const std::optional<QString> refused = NeedsDepth()) return refused;
    return OwnedRefusal(static_cast<uint16_t>(*depth_), frame_, doing);
}

std::optional<QString> Window::ChosenDepthsRefusal(const QString& doing) const {
    if (const std::optional<QString> refused = NeedsDepth()) return refused;
    for (const uint16_t depth : SelectedDepths()) {
        if (const std::optional<QString> refused = OwnedRefusal(depth, frame_, doing))
            return refused;
    }
    return std::nullopt;
}

std::optional<QString> Window::NeedsOwnedDepth() const {
    if (const std::optional<QString> refused = NeedsDepth()) return refused;
    if (AuthoredAt(static_cast<uint16_t>(*depth_), frame_) == nullptr)
        return tr("Choose a depth the project owns first");
    return std::nullopt;
}

std::optional<QString> Window::NeedsKeys(std::size_t fewest) const {
    if (const std::optional<QString> refused = NeedsOwnedDepth()) return refused;
    if (timeline_->SelectedKeys().size() >= fewest) return std::nullopt;
    if (fewest == 1) return tr("Select keyframes first");
    if (fewest == 2) return tr("Select two or more keyframes first");
    return tr("Select three or more keyframes first");
}

std::optional<QString> Window::NeedsProject() const {
    if (const std::optional<QString> refused = NeedsDocument()) return refused;
    if (!project_) return tr("Open or create a project first");
    return std::nullopt;
}

std::optional<QString> Window::WorkAreaRefusal() const {
    if (const std::optional<QString> refused = NeedsAnimation()) return refused;
    if (!work_area_) return tr("Mark the frames as the work area first, with B and N");
    const bool owned = std::ranges::any_of(authored_, [this](const Document::AuthoredDepth& one) {
        return one.animation == animation_path_ && one.clip == clip_;
    });
    if (owned)
        return tr("The project owns depths in this clip. Detach them before cutting its frames.");
    return std::nullopt;
}

std::optional<QString> Window::AnchorRefusal(uint16_t depth) const {
    if (!AuthoredIndexAt(depth, frame_)) return std::nullopt;
    return tr("The project owns depth %1 and draws it from keyframes, which keep no anchor. "
              "Detach it before moving its anchor.")
        .arg(depth);
}

std::optional<QString> Window::FitRefusal() const {
    if (const std::optional<QString> refused = NeedsAnimation()) return refused;
    if (clip_.sprite)
        return tr("A sprite has no stage of its own. Fit depths on the root timeline.");
    return NeedsDepth();
}

std::optional<QString> Window::StageDepthsRefusal(std::size_t fewest) const {
    if (const std::optional<QString> refused = NeedsAnimation()) return refused;
    const std::optional<QString> too_few =
        tr("Choose at least %1 depths shown on the stage first").arg(fewest);
    if (!OutlinesMatchView()) return too_few;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return QString::fromStdString(animation.error());
    const std::vector<uint16_t> group = SelectedDepths();
    const std::size_t shown = std::ranges::count_if(
        VisibleOutlines(*animation), [&group](const Document::StageOutline& one) {
            return std::ranges::find(group, one.depth) != group.end();
        });
    if (shown < fewest) return too_few;
    return std::nullopt;
}

}
