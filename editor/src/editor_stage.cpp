#include "editor_window.h"

#include "editor_viewport.h"

#include "document/authored.h"
#include "document/stage_bounds.h"
#include "document/stage_move.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Editor {

bool Window::OutlinesMatchView() const {
    return !clip_.sprite || symbol_shown_;
}

void Window::UpdateOutlines(const AfpAnimation::Animation& animation) {
    if (!file_ || !OutlinesMatchView()) {
        viewport_->ShowOutlines({}, std::nullopt);
        return;
    }
    if (shape_bounds_path_ != animation_path_) {
        shape_bounds_ = file_->ShapeBounds(animation_path_);
        shape_bounds_path_ = animation_path_;
    }
    viewport_->ShowOutlines(Document::StageOutlines(animation, clip_, frame_, shape_bounds_),
                            depth_ ? std::optional<uint16_t>(static_cast<uint16_t>(*depth_))
                                   : std::nullopt);
}

void Window::PickOnStage(double x, double y) {
    if (!file_ || animation_path_.empty() || !OutlinesMatchView()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const std::optional<uint16_t> picked = Document::DepthAt(
        Document::StageOutlines(*animation, clip_, frame_, shape_bounds_), {x, y});
    if (picked) {
        ChooseDepth(*picked);
        return;
    }
    depth_.reset();
    ShowFrame();
}

void Window::MoveOnStage(uint16_t depth, double dx, double dy) {
    if (!file_ || animation_path_.empty()) return;
    const Document::StageOffset offset{.x = dx, .y = dy};
    const uint32_t frame = frame_;
    const QString name = tr("Move depth %1").arg(depth);
    depth_ = depth;
    if (AuthoredIndexAt(depth, frame)) {
        const auto animation = file_->ReadAnimation(animation_path_);
        if (!animation) {
            ReportOnce(QString::fromStdString(animation.error()));
            return;
        }
        EditAuthored(name, [&animation, frame, offset](Document::AuthoredDepth& owned) {
            using Moved = Support::Expected<void, std::string>;
            const auto baked = Document::BakedFor(*animation, owned);
            if (!baked) return Moved(Support::Unexpected(baked.error()));
            return Document::MoveOwnedDepth(owned, *baked, frame, offset);
        });
        return;
    }
    const Document::ClipId clip = clip_;
    EditAnimation(name, [clip, depth, frame, offset](AfpAnimation::Animation& edited) {
        return Document::MoveBakedDepth(edited, clip, depth, frame, offset);
    });
}

}
