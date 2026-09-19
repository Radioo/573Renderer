#include "editor_window.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/outline.h"
#include "document/span_arrange.h"
#include "document/span_clipboard.h"
#include "document/span_sequence.h"
#include "document/span_split.h"
#include "document/span_edit.h"
#include "document/span_transplant.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QMenu>
#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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

void Window::ArrangeDepth(Document::Arrange how, const QString& name) {
    if (!file_ || animation_path_.empty() || !depth_) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    std::vector<Document::DepthChange> changes;
    if (!EditAnimation(name.arg(depth),
                       [&changes, clip, depth, frame, how](AfpAnimation::Animation& edited)
                           -> Support::Expected<void, std::string> {
                           auto arranged = Document::ArrangeSpan(edited, clip, depth, frame, how);
                           if (!arranged) return Support::Unexpected(arranged.error());
                           changes = std::move(*arranged);
                           return {};
                       })) {
        return;
    }
    std::vector<std::pair<std::size_t, uint16_t>> owned;
    for (const Document::DepthChange& change : changes) {
        if (const auto index = AuthoredIndexAt(change.from, frame))
            owned.emplace_back(*index, change.to);
    }
    for (const auto& [index, to] : owned)
        authored_[index].depth = to;
    if (!owned.empty()) SaveProject();
    depth_ = changes.front().to;
    ShowFrame();
}

void Window::SplitDepthAt(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    if (AuthoredIndexAt(depth, frame)) {
        ReportProblem(
            tr("The project owns depth %1 here. Detach it before splitting it.").arg(depth));
        return;
    }
    const Document::ClipId clip = clip_;
    EditAnimation(tr("Split depth %1 at frame %2").arg(depth).arg(frame),
                  [clip, depth, frame](AfpAnimation::Animation& edited) {
                      return Document::SplitSpan(edited, clip, depth, frame);
                  });
}

void Window::SequenceChosenDepths(uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    const std::vector<uint16_t> chosen = SelectedDepths();
    std::vector<std::optional<std::size_t>> owned;
    owned.reserve(chosen.size());
    for (const uint16_t depth : chosen)
        owned.push_back(AuthoredIndexAt(depth, frame));
    const Document::ClipId clip = clip_;
    std::vector<Document::SpanShift> shifts;
    if (!EditAnimation(tr("Sequence %n depths", nullptr, static_cast<int>(chosen.size())),
                       [clip, &chosen, frame, &shifts](AfpAnimation::Animation& edited)
                           -> Support::Expected<void, std::string> {
                           auto sequenced = Document::SequenceSpans(edited, clip, chosen, frame);
                           if (!sequenced) return Support::Unexpected(sequenced.error());
                           shifts = std::move(*sequenced);
                           return {};
                       })) {
        return;
    }
    bool saved = false;
    for (const Document::SpanShift& shift : shifts) {
        const auto at = std::ranges::find(chosen, shift.depth);
        const std::optional<std::size_t> index =
            owned.at(static_cast<std::size_t>(at - chosen.begin()));
        if (!index) continue;
        auto moved = Document::ShiftAuthored(authored_[*index], shift.by);
        if (!moved) ReportProblem(QString::fromStdString(moved.error()));
        saved = true;
    }
    if (saved) SaveProject();
    ShowFrame();
}

void Window::AddArrangeMenu(QMenu* edit) {
    QMenu* arrange = edit->addMenu(tr("A&rrange"));
    const std::array<std::tuple<QString, QKeySequence, Document::Arrange, QString>, 4> lines{{
        {tr("Bring &forward"), QKeySequence(Qt::CTRL | Qt::Key_BracketRight),
         Document::Arrange::Forward, tr("Bring depth %1 forward")},
        {tr("Send &backward"), QKeySequence(Qt::CTRL | Qt::Key_BracketLeft),
         Document::Arrange::Backward, tr("Send depth %1 backward")},
        {tr("Bring to f&ront"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight),
         Document::Arrange::Front, tr("Bring depth %1 to the front")},
        {tr("Send to bac&k"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft),
         Document::Arrange::Back, tr("Send depth %1 to the back")},
    }};
    for (const auto& [text, keys, how, name] : lines) {
        QAction* action = arrange->addAction(text);
        action->setShortcut(keys);
        connect(action, &QAction::triggered, this, [this, how, name] { ArrangeDepth(how, name); });
    }
}

bool Window::OwnsDepthIn(const Document::GroupRange& range) const {
    return std::ranges::any_of(authored_, [&](const Document::AuthoredDepth& owned) {
        return owned.animation == animation_path_ && owned.clip == range.clip &&
               owned.depth >= range.first_depth && owned.depth <= range.last_depth &&
               owned.first_frame <= range.last_frame && owned.last_frame >= range.first_frame;
    });
}

void Window::GroupDepthsIntoSprite(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    const AfpAnimation::Container* shown = Document::FindClip(*animation, clip_);
    if (shown == nullptr || shown->frames.empty()) return;
    const QString title = tr("Group into a sprite");
    const int widest = std::numeric_limits<uint16_t>::max();
    bool answered = false;
    const int last_depth =
        QInputDialog::getInt(this, title, tr("Last depth"), depth, depth, widest, 1, &answered);
    if (!answered) return;
    Document::Span around{.first_frame = frame, .last_frame = frame};
    for (int at = depth; at <= last_depth; at++) {
        const auto span = Document::SpanOfDepth(*shown, static_cast<uint16_t>(at), frame);
        if (!span) continue;
        around.first_frame = std::min(around.first_frame, span->first_frame);
        around.last_frame = std::max(around.last_frame, span->last_frame);
    }
    const int final_frame = static_cast<int>(shown->frames.size()) - 1;
    const int first =
        QInputDialog::getInt(this, title, tr("First frame"), static_cast<int>(around.first_frame),
                             0, final_frame, 1, &answered);
    if (!answered) return;
    const int last = QInputDialog::getInt(this, title, tr("Last frame"),
                                          std::max(static_cast<int>(around.last_frame), first),
                                          first, final_frame, 1, &answered);
    if (!answered) return;
    const Document::GroupRange range{.clip = clip_,
                                     .first_depth = depth,
                                     .last_depth = static_cast<uint16_t>(last_depth),
                                     .first_frame = static_cast<uint32_t>(first),
                                     .last_frame = static_cast<uint32_t>(last)};
    if (OwnsDepthIn(range)) {
        ReportProblem(tr("The project owns a depth in that range. Detach it before grouping."));
        return;
    }
    if (!EditAnimation(tr("Group depths %1 to %2 into a sprite").arg(depth).arg(last_depth),
                       [range](AfpAnimation::Animation& edited) {
                           using Grouped = Support::Expected<void, std::string>;
                           auto sprite = Document::GroupIntoSprite(edited, range);
                           if (!sprite) return Grouped(Support::Unexpected(sprite.error()));
                           return Grouped();
                       })) {
        return;
    }
    depth_ = depth;
    RefillClipsKeepingChoice();
    ShowFrame();
}

void Window::UngroupSpriteAt(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    if (AuthoredIndexAt(depth, frame)) {
        ReportProblem(
            tr("The project owns depth %1 here. Detach it before ungrouping.").arg(depth));
        return;
    }
    const Document::ClipId clip = clip_;
    if (!EditAnimation(tr("Ungroup the sprite on depth %1").arg(depth),
                       [clip, depth, frame](AfpAnimation::Animation& edited) {
                           return Document::UngroupSprite(edited, clip, depth, frame);
                       })) {
        return;
    }
    depth_ = depth;
    RefillClipsKeepingChoice();
    ShowFrame();
}

std::optional<uint16_t> Window::NextFreeDepth(uint16_t fallback) {
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return std::nullopt;
    }
    uint16_t highest = fallback;
    const auto details = Document::DescribeClip(*animation, clip_);
    if (details) {
        for (const Document::DepthRow& row : details->depths)
            highest = std::max(highest, row.depth);
    }
    return static_cast<uint16_t>(std::min<int>(highest + 1, std::numeric_limits<uint16_t>::max()));
}

std::optional<uint16_t> Window::AskForFreeDepth(const QString& title, uint16_t fallback) {
    const std::optional<uint16_t> next = NextFreeDepth(fallback);
    if (!next) return std::nullopt;
    const int suggested = *next;
    bool answered = false;
    const int chosen = QInputDialog::getInt(this, title, tr("Depth"), suggested, 0,
                                            std::numeric_limits<uint16_t>::max(), 1, &answered);
    if (!answered) return std::nullopt;
    return static_cast<uint16_t>(chosen);
}

void Window::CopySpanAt(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    auto copied = Document::CopySpanFrom(*file_, animation_path_, clip_, depth, frame);
    if (!copied) {
        ReportProblem(QString::fromStdString(copied.error()));
        return;
    }
    copied_span_ = std::move(*copied);
    statusBar()->showMessage(tr("Copied depth %1, %2 frames").arg(depth).arg(copied_span_->length));
}

void Window::PasteSpanAt(uint32_t frame) {
    if (!file_ || animation_path_.empty() || !copied_span_) return;
    const std::optional<uint16_t> to = AskForFreeDepth(tr("Paste onto a depth"), 0);
    if (!to) return;
    const Document::ClipId clip = clip_;
    const Document::CopiedSpan& copied = *copied_span_;
    const std::string path = animation_path_;
    const uint16_t depth = *to;
    if (!EditDocument(tr("Paste onto depth %1").arg(depth), [&path, clip, &copied, depth,
                                                             frame](Document::File& document) {
            using Pasted = Support::Expected<void, std::string>;
            auto placed = Document::PasteSpanInto(document, path, clip, copied, depth, frame);
            if (!placed) return Pasted(Support::Unexpected(placed.error()));
            return Pasted();
        })) {
        return;
    }
    depth_ = depth;
    ShowFrame();
}

void Window::RemoveChosenDepths(uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    const std::vector<uint16_t> group = SelectedDepths();
    if (group.empty()) return;
    for (const uint16_t depth : group) {
        if (AuthoredIndexAt(depth, frame)) {
            ReportProblem(
                tr("The project owns depth %1 here. Detach it before removing it.").arg(depth));
            return;
        }
    }
    const Document::ClipId clip = clip_;
    const QString name = group.size() == 1
                             ? tr("Remove depth %1").arg(group.front())
                             : tr("Remove %n depths", nullptr, static_cast<int>(group.size()));
    if (!EditAnimation(name, [clip, group, frame](AfpAnimation::Animation& edited) {
            for (const uint16_t depth : group) {
                auto removed = Document::RemoveDepth(edited, clip, depth, frame);
                if (!removed) return removed;
            }
            return Support::Expected<void, std::string>();
        })) {
        return;
    }
    depth_.reset();
    selected_depths_.clear();
    ShowFrame();
}

void Window::DuplicateSpanToDepth(uint16_t depth, uint32_t frame) {
    if (!file_ || animation_path_.empty()) return;
    const std::optional<uint16_t> chosen =
        AskForFreeDepth(tr("Duplicate onto another depth"), depth);
    if (!chosen) return;
    const auto to = *chosen;
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
