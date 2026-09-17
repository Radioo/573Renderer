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

void Window::EditOnStage(uint16_t depth, const QString& name, const OwnedStageChange& owned,
                         const AnimationChange& baked) {
    if (!file_ || animation_path_.empty()) return;
    depth_ = depth;
    if (AuthoredIndexAt(depth, frame_)) {
        const auto animation = file_->ReadAnimation(animation_path_);
        if (!animation) {
            ReportOnce(QString::fromStdString(animation.error()));
            return;
        }
        EditAuthored(name, [&animation, &owned](Document::AuthoredDepth& authored) {
            using Changed = Support::Expected<void, std::string>;
            const auto from = Document::BakedFor(*animation, authored);
            if (!from) return Changed(Support::Unexpected(from.error()));
            return owned(authored, *from);
        });
        return;
    }
    EditAnimation(name, baked);
}

void Window::MoveOnStage(uint16_t depth, double dx, double dy) {
    const Document::StageOffset offset{.x = dx, .y = dy};
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    EditOnStage(
        depth, tr("Move depth %1").arg(depth),
        [frame, offset](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
            return Document::MoveOwnedDepth(authored, baked, frame, offset);
        },
        [clip, depth, frame, offset](AfpAnimation::Animation& edited) {
            return Document::MoveBakedDepth(edited, clip, depth, frame, offset);
        });
}

void Window::ReshapeOnStage(uint16_t depth, double scale_x, double scale_y, double turn) {
    const Document::Reshape reshape{.scale_x = scale_x, .scale_y = scale_y, .turn = turn};
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    const QString name =
        turn != 0 ? tr("Turn depth %1").arg(depth) : tr("Scale depth %1").arg(depth);
    EditOnStage(
        depth, name,
        [frame, reshape](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
            return Document::ReshapeOwnedDepth(authored, baked, frame, reshape);
        },
        [clip, depth, frame, reshape](AfpAnimation::Animation& edited) {
            return Document::ReshapeBakedDepth(edited, clip, depth, frame, reshape);
        });
}

}
