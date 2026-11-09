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
#include <string>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kMinimumHeight = 160;
constexpr double kMargin = 16.0;
constexpr double kHeadroom = 0.1;
constexpr double kKeySize = 7.0;
constexpr double kKeyReach = 7.0;
constexpr double kHandleRadius = 4.0;
const std::array<QColor, 6> kLineColours{QColor(240, 90, 90),   QColor(110, 210, 110),
                                         QColor(100, 160, 255), QColor(240, 190, 80),
                                         QColor(200, 130, 240), QColor(120, 220, 220)};
const QColor kAxis(90, 90, 96);
const QColor kPlayhead(80, 200, 255);

}

GraphEditor::GraphEditor(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kMinimumHeight);
}

QColor GraphEditor::ColourOf(std::size_t track) {
    return kLineColours.at(track % kLineColours.size());
}

void GraphEditor::ShowTrack(std::optional<Document::Track> track, uint32_t first_frame,
                            uint32_t last_frame, uint32_t playhead) {
    std::vector<Document::Track> tracks;
    std::vector<std::string> shown;
    if (track) {
        shown.push_back(track->property);
        tracks.push_back(std::move(*track));
    }
    ShowTracks(std::move(tracks), std::move(shown), first_frame, last_frame, playhead);
}

void GraphEditor::ShowTracks(std::vector<Document::Track> tracks, std::vector<std::string> shown,
                             uint32_t first_frame, uint32_t last_frame, uint32_t playhead) {
    tracks_ = std::move(tracks);
    shown_ = std::move(shown);
    first_frame_ = first_frame;
    last_frame_ = std::max(first_frame, last_frame);
    playhead_ = playhead;
    grabbed_.reset();
    handled_.reset();
    curve_.reset();
    dragged_.reset();
    dragged_frame_.reset();
    Fit(keys_only_);
}

void GraphEditor::SetFrame(uint32_t frame) {
    if (playhead_ == frame) return;
    playhead_ = frame;
    update();
}

std::vector<std::size_t> GraphEditor::Drawn() const {
    std::vector<std::size_t> drawn;
    for (std::size_t at = 0; at < tracks_.size(); at++) {
        if (std::ranges::find(shown_, tracks_[at].property) != shown_.end()) drawn.push_back(at);
    }
    return drawn;
}

void GraphEditor::Fit(bool keys_only) {
    keys_only_ = keys_only;
    lowest_ = std::numeric_limits<double>::max();
    highest_ = std::numeric_limits<double>::lowest();
    const auto reach = [this](double value) {
        lowest_ = std::min(lowest_, value);
        highest_ = std::max(highest_, value);
    };
    for (const std::size_t at : Drawn()) {
        const Document::Track& track = tracks_[at];
        for (const Document::Keyframe& key : track.keys) {
            for (const int64_t value : key.value)
                reach(static_cast<double>(value));
        }
        if (keys_only) continue;
        for (uint32_t frame = first_frame_; frame <= last_frame_; frame++) {
            for (const int64_t value : Document::SampleTrack(track, frame))
                reach(static_cast<double>(value));
        }
    }
    if (lowest_ > highest_) {
        lowest_ = 0.0;
        highest_ = 1.0;
    }
    const double room = std::max((highest_ - lowest_) * kHeadroom, 1.0);
    lowest_ -= room;
    highest_ += room;
    update();
}

Document::Bezier GraphEditor::DraggedCurve(const Document::Keyframe& key) const {
    return curve_ ? *curve_ : key.bezier;
}

Document::Track GraphEditor::Shown(std::size_t track) const {
    Document::Track shown = tracks_[track];
    if (handled_ && handled_->track == track && curve_)
        shown.keys.at(handled_->key).bezier = *curve_;
    if (!grabbed_ || grabbed_->track != track || !dragged_ || !dragged_frame_) return shown;
    shown.keys.at(grabbed_->key).value = *dragged_;
    shown.keys.at(grabbed_->key).frame = *dragged_frame_;
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

uint32_t GraphEditor::FrameBetweenNeighbours(uint32_t frame) const {
    const std::vector<Document::Keyframe>& keys = tracks_[grabbed_->track].keys;
    const std::size_t key = grabbed_->key;
    const uint32_t lowest = key > 0 ? keys[key - 1].frame + 1 : first_frame_;
    const uint32_t highest = key + 1 < keys.size() ? keys[key + 1].frame - 1 : last_frame_;
    return std::clamp(frame, lowest, std::max(lowest, highest));
}

std::optional<QPointF> GraphEditor::KeyPoint(uint32_t frame, std::size_t component) const {
    const std::vector<std::size_t> drawn = Drawn();
    if (drawn.empty()) return std::nullopt;
    const Document::Track shown = Shown(drawn.front());
    const auto key = std::ranges::find(shown.keys, frame, &Document::Keyframe::frame);
    if (key == shown.keys.end() || component >= key->value.size()) return std::nullopt;
    return ToWidget(key->frame, static_cast<double>(key->value[component]));
}

std::optional<GraphEditor::Grab> GraphEditor::KeyNear(QPointF widget) const {
    for (const std::size_t at : Drawn()) {
        const Document::Track shown = Shown(at);
        for (std::size_t key = 0; key < shown.keys.size(); key++) {
            const Document::Keyframe& one = shown.keys[key];
            for (std::size_t component = 0; component < one.value.size(); component++) {
                const QPointF point =
                    ToWidget(one.frame, static_cast<double>(one.value[component]));
                if (QLineF(point, widget).length() <= kKeyReach)
                    return Grab{.track = at, .key = key, .component = component};
            }
        }
    }
    return std::nullopt;
}

std::optional<QPointF> GraphEditor::HandlePoint(const Handle& handle) const {
    const Document::Track shown = Shown(handle.track);
    if (handle.key + 1 >= shown.keys.size()) return std::nullopt;
    const Document::Keyframe& key = shown.keys[handle.key];
    const Document::Keyframe& next = shown.keys[handle.key + 1];
    if (key.ease != Document::Ease::Bezier || key.value.empty() || next.value.empty())
        return std::nullopt;
    const Document::Bezier curve = DraggedCurve(key);
    const double from_frame = key.frame;
    const double to_frame = next.frame;
    const auto from_value = static_cast<double>(key.value.front());
    const auto to_value = static_cast<double>(next.value.front());
    const double time = handle.second ? curve.x2 : curve.x1;
    const double progress = handle.second ? curve.y2 : curve.y1;
    return ToWidget(from_frame + ((to_frame - from_frame) * time),
                    from_value + ((to_value - from_value) * progress));
}

std::optional<GraphEditor::Handle> GraphEditor::HandleNear(QPointF widget) const {
    for (const std::size_t at : Drawn()) {
        const Document::Track shown = Shown(at);
        for (std::size_t key = 0; key < shown.keys.size(); key++) {
            for (const bool second : {false, true}) {
                const Handle handle{.track = at, .key = key, .second = second};
                const std::optional<QPointF> point = HandlePoint(handle);
                if (point && QLineF(*point, widget).length() <= kKeyReach) return handle;
            }
        }
    }
    return std::nullopt;
}

void GraphEditor::DrawHandles(QPainter& painter, std::size_t track) const {
    const Document::Track shown = Shown(track);
    painter.setPen(QPen(kAxis, 1));
    for (std::size_t key = 0; key < shown.keys.size(); key++) {
        if (shown.keys[key].value.empty()) continue;
        const QPointF from =
            ToWidget(shown.keys[key].frame, static_cast<double>(shown.keys[key].value.front()));
        for (const bool second : {false, true}) {
            const Handle handle{.track = track, .key = key, .second = second};
            const std::optional<QPointF> point = HandlePoint(handle);
            if (!point) continue;
            const QPointF anchor =
                second && key + 1 < shown.keys.size() && !shown.keys[key + 1].value.empty()
                    ? ToWidget(shown.keys[key + 1].frame,
                               static_cast<double>(shown.keys[key + 1].value.front()))
                    : from;
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(anchor, *point);
            painter.setBrush(ColourOf(track));
            painter.drawEllipse(*point, kHandleRadius, kHandleRadius);
        }
    }
    painter.setBrush(Qt::NoBrush);
}

void GraphEditor::DrawTrack(QPainter& painter, std::size_t track) const {
    const Document::Track shown = Shown(track);
    if (shown.keys.empty()) return;
    const QColor colour = ColourOf(track);
    const std::size_t components = shown.keys.front().value.size();
    for (std::size_t component = 0; component < components; component++) {
        QPolygonF line;
        for (uint32_t frame = first_frame_; frame <= last_frame_; frame++) {
            const std::vector<int64_t> value = Document::SampleTrack(shown, frame);
            if (component < value.size())
                line << ToWidget(frame, static_cast<double>(value[component]));
        }
        painter.setPen(QPen(colour, component == 0 ? 2 : 1));
        painter.drawPolyline(line);
        painter.setBrush(colour);
        for (const Document::Keyframe& key : shown.keys) {
            if (component >= key.value.size()) continue;
            const QPointF point = ToWidget(key.frame, static_cast<double>(key.value[component]));
            painter.drawRect(
                QRectF(point.x() - (kKeySize / 2), point.y() - (kKeySize / 2), kKeySize, kKeySize));
        }
        painter.setBrush(Qt::NoBrush);
    }
    DrawHandles(painter, track);
}

void GraphEditor::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));
    const std::vector<std::size_t> drawn = Drawn();
    if (drawn.empty()) {
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
    for (const std::size_t at : drawn)
        DrawTrack(painter, at);
}

void GraphEditor::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || Drawn().empty()) return;
    handled_ = HandleNear(event->position());
    if (handled_) {
        curve_ = tracks_[handled_->track].keys.at(handled_->key).bezier;
        pressed_at_ = event->position();
        return;
    }
    grabbed_ = KeyNear(event->position());
    if (!grabbed_) {
        emit FrameChosen(FrameAt(event->position().x()));
        return;
    }
    dragged_ = tracks_[grabbed_->track].keys.at(grabbed_->key).value;
    dragged_frame_ = tracks_[grabbed_->track].keys.at(grabbed_->key).frame;
    pressed_at_ = event->position();
}

void GraphEditor::mouseMoveEvent(QMouseEvent* event) {
    if (handled_ && curve_) {
        const Document::Track& track = tracks_[handled_->track];
        const Document::Keyframe& key = track.keys.at(handled_->key);
        const Document::Keyframe& next = track.keys.at(handled_->key + 1);
        const double frames = std::max(1.0, static_cast<double>(next.frame - key.frame));
        const double values =
            static_cast<double>(next.value.front()) - static_cast<double>(key.value.front());
        const double time =
            (static_cast<double>(FrameAt(event->position().x())) - key.frame) / frames;
        const double progress =
            values == 0.0
                ? (handled_->second ? curve_->y2 : curve_->y1)
                : (ValueAt(event->position().y()) - static_cast<double>(key.value.front())) /
                      values;
        if (handled_->second) {
            curve_->x2 = std::clamp(time, 0.0, 1.0);
            curve_->y2 = progress;
        } else {
            curve_->x1 = std::clamp(time, 0.0, 1.0);
            curve_->y1 = progress;
        }
        update();
        return;
    }
    if (!grabbed_ || !dragged_ || !dragged_frame_) return;
    const Document::Keyframe& key = tracks_[grabbed_->track].keys.at(grabbed_->key);
    const QPointF at = event->position();
    const QPointF moved = at - pressed_at_;
    const bool constrained = (event->modifiers() & Qt::ShiftModifier) != 0;
    const bool across = std::abs(moved.x()) > std::abs(moved.y());
    dragged_->at(grabbed_->component) =
        constrained && across ? key.value.at(grabbed_->component) : std::llround(ValueAt(at.y()));
    dragged_frame_ = constrained && !across ? key.frame : FrameBetweenNeighbours(FrameAt(at.x()));
    update();
}

void GraphEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (handled_ && curve_) {
        const Document::Track& track = tracks_[handled_->track];
        const Document::Keyframe& key = track.keys.at(handled_->key);
        const QString property = QString::fromStdString(track.property);
        const uint32_t frame = key.frame;
        const Document::Bezier curve = *curve_;
        const bool changed = curve != key.bezier;
        handled_.reset();
        curve_.reset();
        update();
        if (changed) emit EaseEdited(property, frame, curve);
        return;
    }
    if (!grabbed_ || !dragged_ || !dragged_frame_) return;
    const Document::Track& track = tracks_[grabbed_->track];
    const Document::Keyframe& key = track.keys.at(grabbed_->key);
    const QString property = QString::fromStdString(track.property);
    const uint32_t frame = key.frame;
    const uint32_t to_frame = *dragged_frame_;
    std::vector<int64_t> value = *dragged_;
    const bool changed = value != key.value || to_frame != frame;
    grabbed_.reset();
    dragged_.reset();
    dragged_frame_.reset();
    update();
    emit KeyChosen(property, frame);
    if (changed) emit KeyMoved(property, frame, to_frame, std::move(value));
}

}
