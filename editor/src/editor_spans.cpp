#include "editor_window.h"

#include "document/span_edit.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QInputDialog>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

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

}
