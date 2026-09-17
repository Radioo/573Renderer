#include "editor_timeline.h"

#include "document/keyframes.h"
#include "document/outline.h"
#include "document/timeline.h"

#include <QColor>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
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
constexpr int kKeyReach = 6;
constexpr int kKeyRadius = 4;
constexpr int kPropertyIndent = 8;
constexpr double kZoomStep = 1.25;
constexpr double kMostPixelsPerFrame = 48.0;
constexpr int kTickSpacing = 60;
constexpr int kTickHeight = 6;
constexpr std::array<uint32_t, 10> kTickSteps{1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};

const QColor kRuler(58, 58, 62);
const QColor kRow(40, 40, 44);
const QColor kPropertyRow(34, 34, 38);
const QColor kBar(70, 128, 196);
const QColor kKey(210, 210, 216);
const QColor kKeySelected(240, 190, 80);
const QColor kKeyLine(96, 96, 104);
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
    keyed_depth_.reset();
    tracks_.clear();
    selected_property_.clear();
    selected_key_.reset();
    Resize();
    ApplyZoom();
    update();
}

void Timeline::ShowKeys(std::optional<uint16_t> depth, std::vector<Document::Track> tracks) {
    keyed_depth_ = depth;
    tracks_ = std::move(tracks);
    if (std::ranges::none_of(tracks_, [this](const Document::Track& track) {
            return QString::fromStdString(track.property) == selected_property_;
        })) {
        selected_property_.clear();
        selected_key_.reset();
    }
    Resize();
    update();
}

void Timeline::SelectKey(const QString& property, std::optional<uint32_t> frame) {
    selected_property_ = property;
    selected_key_ = frame;
    update();
}

void Timeline::Clear() {
    frame_count_ = 0;
    frame_ = 0;
    rows_.clear();
    labels_.clear();
    keyed_depth_.reset();
    tracks_.clear();
    selected_property_.clear();
    selected_key_.reset();
    zoom_.reset();
    setMinimumHeight(kEmptyHeight);
    setMinimumWidth(0);
    update();
}

void Timeline::SetFrame(uint32_t frame) {
    frame_ = frame;
    if (zoom_) {
        if (auto* area = ScrollArea())
            area->ensureVisible(FrameToX(frame), area->verticalScrollBar()->value(), kTickSpacing,
                                0);
    }
    update();
}

QScrollArea* Timeline::ScrollArea() const {
    return qobject_cast<QScrollArea*>(parentWidget() != nullptr ? parentWidget()->parentWidget()
                                                                : nullptr);
}

void Timeline::Resize() {
    const int lanes = static_cast<int>(Lanes().size());
    setMinimumHeight(std::max(kEmptyHeight, kRulerHeight + lanes * kRowHeight));
}

std::vector<Timeline::Lane> Timeline::Lanes() const {
    std::vector<Lane> lanes;
    for (const Document::DepthRow& row : rows_) {
        lanes.push_back(Lane{.is_property = false, .depth = row.depth, .track = 0});
        if (!keyed_depth_ || row.depth != *keyed_depth_) continue;
        for (std::size_t i = 0; i < tracks_.size(); i++)
            lanes.push_back(Lane{.is_property = true, .depth = row.depth, .track = i});
    }
    return lanes;
}

std::optional<std::size_t> Timeline::LaneAt(int y) const {
    const int lane = (y - kRulerHeight) / kRowHeight;
    if (lane < 0 || y < kRulerHeight) return std::nullopt;
    const std::vector<Lane> lanes = Lanes();
    if (static_cast<std::size_t>(lane) >= lanes.size()) return std::nullopt;
    return static_cast<std::size_t>(lane);
}

double Timeline::PixelsPerFrame() const {
    if (frame_count_ <= 1) return 0.0;
    return static_cast<double>(width() - kGutterWidth - 1) / static_cast<double>(frame_count_ - 1);
}

void Timeline::ApplyZoom() {
    if (!zoom_ || frame_count_ <= 1) {
        setMinimumWidth(0);
        return;
    }
    setMinimumWidth(kGutterWidth + 1 +
                    static_cast<int>(std::ceil(*zoom_ * static_cast<double>(frame_count_ - 1))));
}

void Timeline::wheelEvent(QWheelEvent* event) {
    if ((event->modifiers() & Qt::ControlModifier) == 0 || frame_count_ <= 1) {
        QWidget::wheelEvent(event);
        return;
    }
    event->accept();
    QScrollArea* area = ScrollArea();
    const int cursor = static_cast<int>(event->position().x());
    const int scrolled = area != nullptr ? area->horizontalScrollBar()->value() : 0;
    const uint32_t anchor = XToFrame(cursor);
    const double factor = event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep;
    const double wanted = std::min(PixelsPerFrame() * factor, kMostPixelsPerFrame);
    const int visible = area != nullptr ? area->viewport()->width() : width();
    const double fits =
        static_cast<double>(visible - kGutterWidth - 1) / static_cast<double>(frame_count_ - 1);
    if (wanted <= fits) {
        zoom_.reset();
    } else {
        zoom_ = wanted;
    }
    ApplyZoom();
    resize(std::max(minimumWidth(), visible), height());
    if (area != nullptr)
        area->horizontalScrollBar()->setValue(FrameToX(anchor) - (cursor - scrolled));
    update();
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
    painter.setPen(palette().color(QPalette::Mid));
    for (uint32_t frame = 0; frame < frame_count_; frame += step) {
        const int x = FrameToX(frame);
        painter.drawLine(x, kRulerHeight - kTickHeight, x, kRulerHeight);
        painter.drawText(x + 2, kRulerHeight - kTickHeight - 2, QString::number(frame));
    }
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

std::optional<uint32_t> Timeline::KeyNear(std::size_t track, int x) const {
    if (track >= tracks_.size()) return std::nullopt;
    std::optional<uint32_t> found;
    int best = kKeyReach;
    for (const Document::Keyframe& key : tracks_[track].keys) {
        const int distance = std::abs(FrameToX(key.frame) - x);
        if (distance > best) continue;
        best = distance;
        found = key.frame;
    }
    return found;
}

void Timeline::ChooseAt(int x, int y) {
    if (frame_count_ == 0) return;
    const std::optional<std::size_t> lane = LaneAt(y);
    if (lane) {
        const std::vector<Lane> lanes = Lanes();
        const Lane& found = lanes[*lane];
        if (found.is_property) {
            const std::optional<uint32_t> key = KeyNear(found.track, x);
            if (key) emit KeyChosen(QString::fromStdString(tracks_[found.track].property), *key);
        } else {
            emit DepthChosen(found.depth);
        }
    }
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
    const int x = event->pos().x();
    const std::optional<std::size_t> lane = LaneAt(event->pos().y());
    if (lane) {
        const std::vector<Lane> lanes = Lanes();
        const Lane& found = lanes[*lane];
        if (found.is_property) {
            const std::optional<uint32_t> key = KeyNear(found.track, x);
            emit KeyMenuRequested(event->globalPos(),
                                  QString::fromStdString(tracks_[found.track].property),
                                  key.value_or(XToFrame(x)), key.has_value());
            return;
        }
    }
    emit MenuRequested(event->globalPos(), XToFrame(x), LabelNear(x));
}

void Timeline::mousePressEvent(QMouseEvent* event) {
    const std::optional<std::size_t> lane = LaneAt(event->pos().y());
    if (lane) {
        const std::vector<Lane> lanes = Lanes();
        const Lane& found = lanes[*lane];
        if (found.is_property) {
            const std::optional<uint32_t> key = KeyNear(found.track, event->pos().x());
            if (key) {
                dragging_ = found.track;
                drag_from_ = key;
            }
        }
    }
    ChooseAt(event->pos().x(), event->pos().y());
}

void Timeline::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) == 0) return;
    if (dragging_) {
        update();
        return;
    }
    ChooseAt(event->pos().x(), event->pos().y());
}

void Timeline::mouseReleaseEvent(QMouseEvent* event) {
    if (!dragging_ || !drag_from_) {
        dragging_.reset();
        drag_from_.reset();
        return;
    }
    const std::size_t track = *dragging_;
    const uint32_t from = *drag_from_;
    dragging_.reset();
    drag_from_.reset();
    const uint32_t to = XToFrame(event->pos().x());
    if (to == from || track >= tracks_.size()) return;
    emit KeyMoved(QString::fromStdString(tracks_[track].property), from, to);
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
    DrawTicks(painter);
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
            const bool selected_row = QString::fromStdString(track.property) == selected_property_;
            for (const Document::Keyframe& key : track.keys) {
                const int x = FrameToX(key.frame);
                const int middle = y + kRowHeight / 2;
                const bool selected = selected_row && selected_key_ == key.frame;
                painter.setPen(Qt::NoPen);
                painter.setBrush(selected ? kKeySelected : kKey);
                if (key.ease == Document::Ease::Hold) {
                    painter.drawRect(QRect(x - kKeyRadius + 1, middle - kKeyRadius + 1,
                                           2 * kKeyRadius - 2, 2 * kKeyRadius - 2));
                } else {
                    const QPolygon diamond(
                        {QPoint(x, middle - kKeyRadius), QPoint(x + kKeyRadius, middle),
                         QPoint(x, middle + kKeyRadius), QPoint(x - kKeyRadius, middle)});
                    painter.drawPolygon(diamond);
                }
            }
            painter.setBrush(Qt::NoBrush);
            y += kRowHeight;
            continue;
        }

        const auto row = std::ranges::find(rows_, lane.depth, &Document::DepthRow::depth);
        painter.fillRect(QRect(0, y, width(), kRowHeight - 1), kRow);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRect(0, y, kGutterWidth - 6, kRowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(lane.depth));
        if (row != rows_.end()) {
            for (const Document::Span& span : row->spans) {
                const int from = FrameToX(span.first_frame);
                const int to = FrameToX(span.last_frame);
                painter.fillRect(QRect(from, y + kBarInset, std::max(2, to - from),
                                       kRowHeight - 1 - 2 * kBarInset),
                                 kBar);
            }
        }
        y += kRowHeight;
    }

    const int playhead = FrameToX(frame_);
    painter.setPen(kPlayhead);
    painter.drawLine(playhead, 0, playhead, height());
}

}
