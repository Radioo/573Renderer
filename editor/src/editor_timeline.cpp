#include "editor_timeline.h"

#include "document/outline.h"
#include "document/timeline.h"

#include <QColor>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kGutterWidth = 56;
constexpr int kRulerHeight = 26;
constexpr int kRowHeight = 16;
constexpr int kBarInset = 3;
constexpr int kEmptyHeight = 80;
constexpr int kLabelReach = 30;

const QColor kRuler(58, 58, 62);
const QColor kRow(40, 40, 44);
const QColor kBar(70, 128, 196);
const QColor kLabelMark(220, 180, 90);
const QColor kPlayhead(230, 90, 90);

}

Timeline::Timeline(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kEmptyHeight);
}

void Timeline::ShowAnimation(uint32_t frame_count, std::vector<Document::DepthRow> rows,
                             std::vector<Document::AnimationLabel> labels) {
    frame_count_ = frame_count;
    rows_ = std::move(rows);
    labels_ = std::move(labels);
    frame_ = 0;
    setMinimumHeight(
        std::max(kEmptyHeight, kRulerHeight + static_cast<int>(rows_.size()) * kRowHeight));
    update();
}

void Timeline::Clear() {
    frame_count_ = 0;
    frame_ = 0;
    rows_.clear();
    labels_.clear();
    setMinimumHeight(kEmptyHeight);
    update();
}

void Timeline::SetFrame(uint32_t frame) {
    frame_ = frame;
    update();
}

int Timeline::FrameToX(uint32_t frame) const {
    if (frame_count_ <= 1) return kGutterWidth;
    const double span = static_cast<double>(width() - kGutterWidth - 1);
    const double fraction = static_cast<double>(frame) / static_cast<double>(frame_count_ - 1);
    return kGutterWidth + static_cast<int>(span * fraction);
}

uint32_t Timeline::XToFrame(int x) const {
    if (frame_count_ <= 1) return 0;
    const double span = static_cast<double>(width() - kGutterWidth - 1);
    if (span <= 0) return 0;
    const double fraction = std::clamp(static_cast<double>(x - kGutterWidth) / span, 0.0, 1.0);
    return static_cast<uint32_t>(fraction * static_cast<double>(frame_count_ - 1) + 0.5);
}

void Timeline::ChooseAt(int x, int y) {
    if (frame_count_ == 0) return;
    const int row = (y - kRulerHeight) / kRowHeight;
    if (row >= 0 && row < static_cast<int>(rows_.size()))
        emit DepthChosen(rows_[static_cast<std::size_t>(row)].depth);
    const uint32_t frame = XToFrame(x);
    if (frame == frame_) return;
    frame_ = frame;
    update();
    emit FrameChosen(frame);
}

QString Timeline::LabelNear(int x) const {
    QString found;
    int best = kLabelReach;
    for (const Document::AnimationLabel& label : labels_) {
        const int distance = std::abs(FrameToX(label.frame) - x);
        if (distance > best) continue;
        best = distance;
        found = QString::fromStdString(label.name);
    }
    return found;
}

void Timeline::contextMenuEvent(QContextMenuEvent* event) {
    if (frame_count_ == 0) return;
    emit MenuRequested(event->globalPos(), XToFrame(event->pos().x()), LabelNear(event->pos().x()));
}

void Timeline::mousePressEvent(QMouseEvent* event) {
    ChooseAt(event->pos().x(), event->pos().y());
}

void Timeline::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) == 0) return;
    ChooseAt(event->pos().x(), event->pos().y());
}

void Timeline::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));
    if (frame_count_ == 0) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, tr("No animation selected"));
        return;
    }

    painter.fillRect(QRect(0, 0, width(), kRulerHeight), kRuler);
    painter.setPen(palette().color(QPalette::BrightText));
    painter.drawText(QRect(0, 0, kGutterWidth, kRulerHeight), Qt::AlignCenter,
                     QString::number(frame_));
    for (const Document::AnimationLabel& label : labels_) {
        const int x = FrameToX(label.frame);
        painter.setPen(kLabelMark);
        painter.drawLine(x, 0, x, kRulerHeight);
        painter.drawText(x + 3, kRulerHeight - 8, QString::fromStdString(label.name));
    }

    int y = kRulerHeight;
    for (const Document::DepthRow& row : rows_) {
        painter.fillRect(QRect(0, y, width(), kRowHeight - 1), kRow);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRect(0, y, kGutterWidth - 6, kRowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(row.depth));
        for (const Document::Span& span : row.spans) {
            const int from = FrameToX(span.first_frame);
            const int to = FrameToX(span.last_frame);
            painter.fillRect(
                QRect(from, y + kBarInset, std::max(2, to - from), kRowHeight - 1 - 2 * kBarInset),
                kBar);
        }
        y += kRowHeight;
    }

    const int playhead = FrameToX(frame_);
    painter.setPen(kPlayhead);
    painter.drawLine(playhead, 0, playhead, height());
}

}
