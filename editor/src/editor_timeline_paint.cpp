#include "editor_timeline.h"

#include "editor_icons.h"
#include "editor_timeline_metrics.h"

#include "editor_mime.h"

#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/outline.h"
#include "document/timeline.h"
#include "document/timeline_snap.h"

#include <QColor>
#include <QMimeData>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QDragEnterEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QPaintEvent>
#include <QPolygon>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kTickPixels = 10;

}

void Timeline::DrawTicks(QPainter& painter) const {
    const double spacing = PixelsPerFrame();
    if (spacing <= 0.0) return;
    uint32_t step = kTickSteps.back();
    for (const uint32_t candidate : kTickSteps) {
        if (spacing * candidate >= kTickSpacing) {
            step = candidate;
            break;
        }
    }
    QFont ticked(painter.font());
    ticked.setPixelSize(kTickPixels);
    painter.setFont(ticked);
    for (uint32_t frame = 0; frame < frame_count_; frame += step) {
        const int x = FrameToX(frame);
        if (x < kGutterWidth) continue;
        painter.setPen(kGridLine);
        painter.drawLine(x, kRowsTop, x, height());
        painter.setPen(kRulerLine);
        painter.drawLine(x, kRulerHeight - kTickHeight, x, kRulerHeight);
        painter.setPen(kRulerText);
        painter.drawText(x + 3, kRulerHeight - kTickHeight - 3, QString::number(frame));
    }
}

void Timeline::DrawSwitches(QPainter& painter, uint16_t depth, int y) const {
    const bool hidden = std::ranges::find(hidden_depths_, depth) != hidden_depths_.end();
    const bool locked = std::ranges::find(locked_depths_, depth) != locked_depths_.end();
    const bool solo = hidden_depths_.size() + 1 == rows_.size() && !hidden;
    const int top = y + ((kRowHeight - kSwitchWidth) / 2);
    painter.drawPixmap(
        kEyeLeft, top,
        Icons::Drawn(Icons::Glyph::Eye, hidden ? kSwitchOff : kSwitchOn, kSwitchWidth));
    painter.drawPixmap(
        kLockLeft, top,
        Icons::Drawn(Icons::Glyph::Lock, locked ? kSwitchOn : kSwitchOff, kSwitchWidth));
    painter.drawPixmap(
        kSoloLeft, top,
        Icons::Drawn(Icons::Glyph::Solo, solo ? SoloOn() : kSwitchOff, kSwitchWidth));
}

QColor Timeline::BarColour(const Document::DepthRow& row, const Document::Span& span,
                           bool hidden) const {
    if (hidden) return kHiddenBar;
    std::optional<uint16_t> character;
    for (const auto& [frame, shown] : row.shows) {
        if (frame >= span.first_frame && frame <= span.last_frame && !character) character = shown;
    }
    if (!character) return kBar;
    const auto kind = kinds_.find(*character);
    if (kind == kinds_.end()) return kBar;
    switch (kind->second) {
    case Document::CharacterKind::Image:
        return kImageBar;
    case Document::CharacterKind::Shape:
        return kShapeBar;
    case Document::CharacterKind::Sprite:
        return kSpriteBar;
    default:
        return kTextBar;
    }
}

QColor Timeline::BarTop(const Document::DepthRow& row, const Document::Span& span,
                        bool hidden) const {
    const QColor bar = BarColour(row, span, hidden);
    if (bar == kShapeBar) return kShapeBarTop;
    if (bar == kSpriteBar) return kSpriteBarTop;
    if (bar == kHiddenBar) return kSwitchOff;
    return kBarTop;
}

QString Timeline::RowName(const Document::DepthRow& row) const {
    QString shown;
    for (const Document::Span& span : row.spans) {
        const QString named = SpanName(row, span);
        if (named.isEmpty()) continue;
        if (shown.isEmpty()) shown = named;
        if (span.first_frame <= frame_ && frame_ <= span.last_frame) return named;
    }
    return shown;
}

void Timeline::DrawSpanMarks(QPainter& painter, uint16_t depth, const Document::Span& span,
                             const QRect& bar) const {
    const auto marks = marks_.find(depth);
    if (marks == marks_.end()) return;
    painter.setPen(kSpanMark);
    for (const uint32_t frame : marks->second) {
        if (frame < span.first_frame || frame > span.last_frame) continue;
        const int x = FrameToX(frame);
        if (x < bar.left() || x > bar.right()) continue;
        painter.drawLine(x, bar.bottom() - 4, x, bar.bottom());
    }
}

void Timeline::DrawSpanName(QPainter& painter, const QString& name, const QRect& bar) const {
    if (name.isEmpty() || bar.width() < kShortestNamedBar) return;
    QFont font = painter.font();
    font.setPixelSize(kNamePixels);
    painter.setFont(font);
    const QRect inside = bar.adjusted(kNamePadding, 0, -kNamePadding, 0);
    painter.setPen(kSpanName);
    painter.drawText(inside, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(name, Qt::ElideRight, inside.width()));
}

void Timeline::DrawSpanGhost(QPainter& painter, const Document::Span& span, int y) const {
    const Document::Span ghost = Dragged(span, span_to_);
    const int from = FrameToX(ghost.first_frame);
    const int to = FrameToX(ghost.last_frame);
    painter.setPen(kKeySelected);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(
        QRect(from, y + kBarInset, std::max(2, to - from), kRowHeight - 2 - 2 * kBarInset));
}

void Timeline::DrawKeys(QPainter& painter, const Document::Track& track, int y) const {
    const int middle = y + kRowHeight / 2;
    for (const Document::Keyframe& key : track.keys) {
        const bool selected = IsSelected({.property = track.property, .frame = key.frame});
        const int64_t shown = selected ? ShownFrame(key.frame) : static_cast<int64_t>(key.frame);
        const int x = FrameToX(static_cast<uint32_t>(std::clamp<int64_t>(
            shown, 0, frame_count_ > 0 ? static_cast<int64_t>(frame_count_ - 1) : 0)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(selected ? kKeySelected : kKey);
        if (key.ease == Document::Ease::Hold) {
            painter.drawRect(QRect(x - kKeyRadius + 1, middle - kKeyRadius + 1, 2 * kKeyRadius - 2,
                                   2 * kKeyRadius - 2));
        } else {
            const QPolygon diamond({QPoint(x, middle - kKeyRadius), QPoint(x + kKeyRadius, middle),
                                    QPoint(x, middle + kKeyRadius),
                                    QPoint(x - kKeyRadius, middle)});
            painter.drawPolygon(diamond);
        }
    }
    painter.setBrush(Qt::NoBrush);
}

void Timeline::DrawColumnHeaders(QPainter& painter) const {
    const int top = (kRulerHeight - kSwitchWidth) / 2;
    painter.drawPixmap(kEyeLeft, top,
                       Icons::Drawn(Icons::Glyph::Eye,
                                    hidden_depths_.empty() ? kSwitchOff : kSwitchOn, kSwitchWidth));
    painter.drawPixmap(kLockLeft, top,
                       Icons::Drawn(Icons::Glyph::Lock,
                                    locked_depths_.empty() ? kSwitchOff : kSwitchOn, kSwitchWidth));
    QFont named(painter.font());
    named.setPixelSize(kNamePixels);
    painter.setFont(named);
    painter.setPen(kRulerText);
    painter.drawText(QRect(kSoloLeft + kSwitchWidth + 8, 0, kNameLeft, kRulerHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Depth"));
    painter.drawText(QRect(kNameLeft, 0, kGutterWidth - kNameLeft - 8, kRulerHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Character, front first"));
}

void Timeline::DrawNotes(QPainter& painter) const {
    painter.fillRect(QRect(0, kRulerHeight, width(), kNotesHeight), kNotesLane);
    painter.setPen(kNotesText);
    painter.drawText(QRect(kNameLeft, kRulerHeight, kGutterWidth - kNameLeft, kNotesHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Scripts on frames"));
    for (const Document::FrameNote& note : notes_) {
        const int x = FrameToX(note.frame);
        const int middle = kRulerHeight + (kNotesHeight / 2);
        painter.setPen(Qt::NoPen);
        if (note.script) {
            painter.setBrush(kScriptMark);
            painter.drawEllipse(QPoint(x, middle), kNoteRadius, kNoteRadius);
        }
        if (!note.camera) continue;
        painter.setBrush(kCameraMark);
        painter.drawRect(QRect(x - kNoteRadius, middle - kNoteRadius, kNoteRadius, kNoteRadius));
    }
    painter.setBrush(Qt::NoBrush);
}

void Timeline::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));
    if (frame_count_ == 0) {
        painter.setPen(waiting_.isEmpty() ? palette().color(QPalette::Text) : kRulerText);
        painter.drawText(rect(), Qt::AlignCenter,
                         waiting_.isEmpty() ? tr("No animation selected") : waiting_);
        return;
    }

    painter.fillRect(QRect(0, 0, width(), kRulerHeight), kRuler);
    painter.fillRect(QRect(0, kRowsTop, kGutterWidth, height() - kRowsTop), kGutter);
    if (work_area_) {
        const int from = FrameToX(work_area_->first_frame);
        const int to = FrameToX(work_area_->last_frame + 1);
        painter.fillRect(QRect(from, 0, std::max(2, to - from), kRulerHeight), WorkArea());
        painter.fillRect(QRect(from, kRowsTop, std::max(2, to - from), height() - kRowsTop),
                         WorkAreaTint());
    }
    DrawTicks(painter);
    DrawColumnHeaders(painter);
    for (const Document::AnimationLabel& label : labels_) {
        const bool dragged = label_dragging_ && label_grabbed_ &&
                             *label_grabbed_ == QString::fromStdString(label.name);
        const int x = FrameToX(dragged ? label_to_ : label.frame);
        painter.setPen(kLabelMark);
        painter.drawLine(x, 0, x, kRulerHeight);
        painter.setPen(kBarText);
        painter.drawText(x + 4, kRulerHeight - 9, QString::fromStdString(label.name));
    }

    DrawNotes(painter);
    int y = kRowsTop;
    for (const Lane& lane : Lanes()) {
        if (lane.is_property) {
            const Document::Track& track = tracks_[lane.track];
            painter.fillRect(QRect(0, y, width(), kRowHeight - 1), kPropertyRow);
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(QRect(kPropertyIndent, y, kGutterWidth * 3, kRowHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString::fromStdString(track.property));
            if (track.keys.size() > 1) {
                painter.setPen(kKeyLine);
                painter.drawLine(FrameToX(track.keys.front().frame), y + kRowHeight / 2,
                                 FrameToX(track.keys.back().frame), y + kRowHeight / 2);
            }
            DrawKeys(painter, track, y);
            y += kRowHeight;
            continue;
        }

        const auto row = std::ranges::find(rows_, lane.depth, &Document::DepthRow::depth);
        painter.fillRect(QRect(kGutterWidth, y, width() - kGutterWidth, kRowHeight - 1), kRow);
        if (std::ranges::find(selected_depths_, lane.depth) != selected_depths_.end())
            painter.fillRect(QRect(0, y, width(), kRowHeight - 1), SelectedRow());
        painter.setPen(kGutterLine);
        painter.drawLine(0, y + kRowHeight - 1, width(), y + kRowHeight - 1);
        DrawSwitches(painter, lane.depth, y);
        if (row_dragged_ && row_onto_ == lane.depth && row_dragged_ != row_onto_) {
            painter.setPen(kKeySelected);
            painter.drawRect(QRect(0, y, kGutterWidth - 1, kRowHeight - 2));
        }
        QFont numbered(painter.font());
        numbered.setPixelSize(kNamePixels);
        painter.setFont(numbered);
        painter.setPen(kRulerText);
        painter.drawText(QRect(kNumberLeft, y, kNumberRight - kNumberLeft, kRowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(lane.depth));
        const bool hidden = std::ranges::find(hidden_depths_, lane.depth) != hidden_depths_.end();
        if (row != rows_.end()) {
            const QRect named(kNameLeft, y, kGutterWidth - kNameLeft - 8, kRowHeight);
            painter.setPen(kSwitchOn);
            painter.drawText(
                named, Qt::AlignLeft | Qt::AlignVCenter,
                painter.fontMetrics().elidedText(RowName(*row), Qt::ElideRight, named.width()));
        }
        if (row != rows_.end()) {
            for (const Document::Span& span : row->spans) {
                const int from = FrameToX(span.first_frame);
                const int to = FrameToX(span.last_frame);
                const QRect bar(from, y + kBarInset, std::max(2, to - from),
                                kRowHeight - 1 - 2 * kBarInset);
                painter.fillRect(bar, BarColour(*row, span, hidden));
                painter.setPen(BarTop(*row, span, hidden));
                painter.drawLine(bar.left(), bar.top(), bar.right(), bar.top());
                if (std::ranges::find(keyed_depths_, lane.depth) != keyed_depths_.end()) {
                    painter.setPen(kKeyedEdge);
                    painter.drawLine(bar.left(), bar.bottom(), bar.right(), bar.bottom());
                }
                DrawSpanMarks(painter, lane.depth, span, bar);
                DrawSpanName(painter, SpanName(*row, span), bar);
                if (span_dragging_ && span_depth_ == lane.depth && span_grabbed_ == span)
                    DrawSpanGhost(painter, span, y);
            }
        }
        y += kRowHeight;
    }

    const int playhead = FrameToX(frame_);
    painter.setPen(kPlayhead);
    painter.drawLine(playhead, 0, playhead, height());
    if (band_from_) {
        painter.setPen(kKeySelected);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRect(*band_from_, band_to_).normalized());
    }
}

}
