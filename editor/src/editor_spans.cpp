#include "editor_window.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/outline.h"
#include "document/span_edit.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QInputDialog>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace Editor {

void Window::MoveSpanInTime(uint16_t depth, uint32_t frame, int64_t by) {
    if (!file_ || animation_path_.empty()) return;
    const Document::ClipId clip = clip_;
    const std::optional<std::size_t> owned = AuthoredIndexAt(depth, frame);
    if (owned) {
        Document::AuthoredDepth shifted = authored_[*owned];
        auto moved = Document::ShiftAuthored(shifted, by);
        if (!moved) {
            ReportProblem(QString::fromStdString(moved.error()));
            return;
        }
    }
    depth_ = depth;
    if (!EditAnimation(tr("Move depth %1 by %2 frames").arg(depth).arg(by),
                       [clip, depth, frame, by](AfpAnimation::Animation& edited) {
                           using Moved = Support::Expected<void, std::string>;
                           auto span = Document::MoveSpan(edited, clip, depth, frame, by);
                           if (!span) return Moved(Support::Unexpected(span.error()));
                           return Moved();
                       })) {
        return;
    }
    if (!owned) return;
    auto shifted = Document::ShiftAuthored(authored_[*owned], by);
    if (!shifted) ReportProblem(QString::fromStdString(shifted.error()));
    SaveProject();
    ShowFrame();
}

void Window::TrimSpanOnTimeline(uint16_t depth, uint32_t frame, uint32_t first, uint32_t last) {
    if (!file_ || animation_path_.empty()) return;
    const Document::ClipId clip = clip_;
    const Document::Span wanted{.first_frame = first, .last_frame = last};
    const std::optional<std::size_t> owned = AuthoredIndexAt(depth, frame);
    std::optional<Document::AuthoredDepth> trimmed;
    if (owned) trimmed = authored_[*owned];
    depth_ = depth;
    const QString name = tr("Trim depth %1 to frames %2 to %3").arg(depth).arg(first).arg(last);
    if (!EditAnimation(name,
                       [clip, depth, frame, wanted, &trimmed](AfpAnimation::Animation& edited) {
                           if (trimmed) return Document::TrimOwnedSpan(edited, *trimmed, wanted);
                           return Document::TrimSpan(edited, clip, depth, frame, wanted);
                       })) {
        return;
    }
    if (!owned || !trimmed) return;
    authored_[*owned] = std::move(*trimmed);
    SaveProject();
    ShowFrame();
}

void Window::MoveSpanToDepth(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    bool answered = false;
    const int chosen = QInputDialog::getInt(this, tr("Move to another depth"), tr("Depth"), depth,
                                            0, std::numeric_limits<uint16_t>::max(), 1, &answered);
    if (!answered) return;
    const auto to = static_cast<uint16_t>(chosen);
    if (to == depth) return;
    const Document::ClipId clip = clip_;
    const std::optional<std::size_t> owned = AuthoredIndexAt(depth, frame);
    if (!EditAnimation(tr("Move depth %1 to depth %2").arg(depth).arg(to),
                       [clip, depth, frame, to](AfpAnimation::Animation& edited) {
                           return Document::ChangeSpanDepth(edited, clip, depth, frame, to);
                       })) {
        return;
    }
    depth_ = to;
    if (owned) {
        authored_[*owned].depth = to;
        SaveProject();
    }
    ShowFrame();
}

void Window::DuplicateSpanToDepth(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    uint16_t highest = depth;
    const auto details = Document::DescribeClip(*animation, clip_);
    if (details) {
        for (const Document::DepthRow& row : details->depths)
            highest = std::max(highest, row.depth);
    }
    const int suggested = std::min<int>(highest + 1, std::numeric_limits<uint16_t>::max());
    bool answered = false;
    const int chosen =
        QInputDialog::getInt(this, tr("Duplicate onto another depth"), tr("Depth"), suggested, 0,
                             std::numeric_limits<uint16_t>::max(), 1, &answered);
    if (!answered) return;
    const auto to = static_cast<uint16_t>(chosen);
    const Document::ClipId clip = clip_;
    if (!EditAnimation(tr("Duplicate depth %1 onto depth %2").arg(depth).arg(to),
                       [clip, depth, frame, to](AfpAnimation::Animation& edited) {
                           return Document::DuplicateSpan(edited, clip, depth, frame, to);
                       })) {
        return;
    }
    depth_ = to;
    ShowFrame();
}

}
