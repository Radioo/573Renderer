#include "editor_graph.h"

#include "document/keyframes.h"

#include <QColor>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kMinimumHeight = 160;
constexpr double kMargin = 16.0;
constexpr double kHeadroom = 0.1;
constexpr double kKeySize = 7.0;
constexpr double kKeyReach = 7.0;
const std::array<QColor, 4> kLineColours{QColor(240, 90, 90), QColor(110, 210, 110),
                                         QColor(100, 160, 255), QColor(240, 190, 80)};
const QColor kAxis(90, 90, 96);
const QColor kPlayhead(80, 200, 255);

}

GraphEditor::GraphEditor(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kMinimumHeight);
}

void GraphEditor::ShowTrack(std::optional<Document::Track> track, uint32_t first_frame,
                            uint32_t last_frame, uint32_t playhead) {
    track_ = std::move(track);
    first_frame_ = first_frame;
    last_frame_ = std::max(first_frame, last_frame);
    playhead_ = playhead;
    grabbed_.reset();
    dragged_.reset();
    Measure();
    update();
}

void GraphEditor::Measure() {
    lowest_ = std::numeric_limits<double>::max();
    highest_ = std::numeric_limits<double>::lowest();
    if (track_) {
        for (uint32_t frame = first_frame_; frame <= last_frame_; frame++) {
            for (const int64_t value : Document::SampleTrack(*track_, frame)) {
                lowest_ = std::min(lowest_, static_cast<double>(value));
                highest_ = std::max(highest_, static_cast<double>(value));
            }
        }
        for (const Document::Keyframe& key : track_->keys) {
            for (const int64_t value : key.value) {
                lowest_ = std::min(lowest_, static_cast<double>(value));
                highest_ = std::max(highest_, static_cast<double>(value));
            }
        }
    }
    if (lowest_ > highest_) {
        lowest_ = 0.0;
        highest_ = 1.0;
    }
    const double room = std::max((highest_ - lowest_) * kHeadroom, 1.0);
    lowest_ -= room;
    highest_ += room;
}

Document::Track GraphEditor::Shown() const {
    Document::Track shown = *track_;
    if (grabbed_ && dragged_) shown.keys.at(grabbed_->key).value = *dragged_;
    return shown;
}

QPointF GraphEditor::ToWidget(double frame, double value) const {
    const double across = width() - (2 * kMargin);
    const double down = height() - (2 * kMargin);
    const double frames = std::max(1.0, static_cast<double>(last_frame_ - first_frame_));
    return {kMargin + ((frame - first_frame_) / frames * across),
            kMargin + ((highest_ - value) / (highest_ - lowest_) * down)};
}

double GraphEditor::ValueAt(double y) const {
    const double down = height() - (2 * kMargin);
    return highest_ - ((y - kMargin) / down * (highest_ - lowest_));
}

uint32_t GraphEditor::FrameAt(double x) const {
    const double across = width() - (2 * kMargin);
    const double frames = static_cast<double>(last_frame_ - first_frame_);
    const double at = first_frame_ + std::round((x - kMargin) / across * frames);
    return static_cast<uint32_t>(
        std::clamp(at, static_cast<double>(first_frame_), static_cast<double>(last_frame_)));
}

std::optional<QPointF> GraphEditor::KeyPoint(uint32_t frame, std::size_t component) const {
    if (!track_) return std::nullopt;
    const Document::Track shown = Shown();
    const auto key = std::ranges::find(shown.keys, frame, &Document::Keyframe::frame);
    if (key == shown.keys.end() || component >= key->value.size()) return std::nullopt;
    return ToWidget(key->frame, static_cast<double>(key->value[component]));
}

std::optional<GraphEditor::Grab> GraphEditor::KeyNear(QPointF widget) const {
    if (!track_) return std::nullopt;
    for (std::size_t key = 0; key < track_->keys.size(); key++) {
        for (std::size_t component = 0; component < track_->keys[key].value.size(); component++) {
            const std::optional<QPointF> point = KeyPoint(track_->keys[key].frame, component);
            if (point && QLineF(*point, widget).length() <= kKeyReach)
                return Grab{.key = key, .component = component};
        }
    }
    return std::nullopt;
}

void GraphEditor::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));
    if (!track_ || track_->keys.empty()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("Choose a keyframe of an owned depth to graph its property"));
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing);
    if (lowest_ < 0.0 && highest_ > 0.0) {
        painter.setPen(kAxis);
        painter.drawLine(ToWidget(first_frame_, 0.0), ToWidget(last_frame_, 0.0));
    }
    painter.setPen(QPen(kPlayhead, 1));
    painter.drawLine(QPointF(ToWidget(playhead_, 0.0).x(), 0.0),
                     QPointF(ToWidget(playhead_, 0.0).x(), height()));

    const Document::Track shown = Shown();
    const std::size_t components = shown.keys.front().value.size();
    for (std::size_t component = 0; component < components; component++) {
        const QColor colour = kLineColours.at(component % kLineColours.size());
        QPolygonF line;
        for (uint32_t frame = first_frame_; frame <= last_frame_; frame++) {
            const std::vector<int64_t> value = Document::SampleTrack(shown, frame);
            if (component < value.size())
                line << ToWidget(frame, static_cast<double>(value[component]));
        }
        painter.setPen(QPen(colour, 2));
        painter.drawPolyline(line);
        painter.setBrush(colour);
        for (const Document::Keyframe& key : shown.keys) {
            const std::optional<QPointF> point = KeyPoint(key.frame, component);
            if (!point) continue;
            painter.drawRect(QRectF(point->x() - (kKeySize / 2), point->y() - (kKeySize / 2),
                                    kKeySize, kKeySize));
        }
        painter.setBrush(Qt::NoBrush);
    }
}

void GraphEditor::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !track_) return;
    grabbed_ = KeyNear(event->position());
    if (!grabbed_) {
        emit FrameChosen(FrameAt(event->position().x()));
        return;
    }
    dragged_ = track_->keys.at(grabbed_->key).value;
}

void GraphEditor::mouseMoveEvent(QMouseEvent* event) {
    if (!grabbed_ || !dragged_) return;
    dragged_->at(grabbed_->component) = std::llround(ValueAt(event->position().y()));
    update();
}

void GraphEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !grabbed_ || !dragged_ || !track_) return;
    const Document::Keyframe& key = track_->keys.at(grabbed_->key);
    const QString property = QString::fromStdString(track_->property);
    const uint32_t frame = key.frame;
    std::vector<int64_t> value = *dragged_;
    const bool changed = value != key.value;
    grabbed_.reset();
    dragged_.reset();
    update();
    emit KeyChosen(property, frame);
    if (changed) emit KeyValueChanged(property, frame, std::move(value));
}

}
