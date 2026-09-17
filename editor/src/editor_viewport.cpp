#include "editor_viewport.h"

#include "document/stage_bounds.h"

#include <QColor>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QResizeEvent>
#include <QSize>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kMinimumWidth = 320;
constexpr int kMinimumHeight = 180;
constexpr double kDragThreshold = 3.0;
constexpr double kHandleReach = 7.0;
constexpr double kHandleSize = 7.0;
constexpr double kTurnDistance = 28.0;
constexpr double kAnchorSize = 6.0;
constexpr int kOutlineWidth = 2;
const QColor kSelectedColour(80, 200, 255);
const QColor kGuideColour(255, 80, 200);
constexpr double kSnapReach = 6.0;

QPointF Middle(QPointF a, QPointF b) {
    return (a + b) / 2.0;
}

}

Viewport::Viewport(QWidget* parent) : QWidget(parent) {
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    setAutoFillBackground(false);
    message_ = tr("No animation selected");
}

void Viewport::ShowFrame(const QImage& frame, QSize stage) {
    frame_ = frame;
    stage_ = stage;
    message_.clear();
    update();
}

void Viewport::ShowMessage(const QString& message) {
    frame_ = QImage();
    outlines_.clear();
    selected_.reset();
    message_ = message;
    update();
}

void Viewport::ShowOutlines(std::vector<Document::StageOutline> outlines,
                            std::optional<uint16_t> selected) {
    outlines_ = std::move(outlines);
    selected_ = selected;
    update();
}

QSize Viewport::FittedSize(QSize available) const {
    if (stage_.isEmpty()) return available;
    return stage_.scaled(available, Qt::KeepAspectRatio);
}

void Viewport::SetSnapping(bool on) {
    snapping_ = on;
}

Document::Snapped Viewport::Moved(const Document::StageOutline& outline) const {
    const Document::Point raw{pointer_[0] - grab_[0], pointer_[1] - grab_[1]};
    const QRectF target = Target();
    if (!snapping_ || snap_suspended_ || stage_.isEmpty() || target.isEmpty())
        return Document::Snapped{.offset = raw, .guides = {}};
    const double reach = kSnapReach * stage_.width() / target.width();
    return Document::SnapMove(
        outline, outlines_,
        {static_cast<double>(stage_.width()), static_cast<double>(stage_.height())}, raw, reach);
}

void Viewport::DrawGuides(QPainter& painter, const Document::StageOutline& outline) const {
    if (!dragging_ || gesture_ != Gesture::Move) return;
    painter.setPen(QPen(kGuideColour, 1));
    const QRectF target = Target();
    for (const Document::SnapGuide& guide : Moved(outline).guides) {
        if (guide.vertical) {
            const double x = ToWidget({guide.at, 0}).x();
            painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
        } else {
            const double y = ToWidget({0, guide.at}).y();
            painter.drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
        }
    }
}

QRectF Viewport::Target() const {
    const QSize fitted = frame_.size().scaled(size(), Qt::KeepAspectRatio);
    return {QPointF((width() - fitted.width()) / 2.0, (height() - fitted.height()) / 2.0),
            QSizeF(fitted)};
}

std::optional<QPointF> Viewport::ToStage(QPointF widget) const {
    const QRectF target = Target();
    if (frame_.isNull() || stage_.isEmpty() || target.isEmpty()) return std::nullopt;
    return QPointF((widget.x() - target.x()) * stage_.width() / target.width(),
                   (widget.y() - target.y()) * stage_.height() / target.height());
}

QPointF Viewport::ToWidget(const Document::Point& stage) const {
    const QRectF target = Target();
    return {target.x() + (stage[0] * target.width() / stage_.width()),
            target.y() + (stage[1] * target.height() / stage_.height())};
}

const Document::StageOutline* Viewport::SelectedOutline() const {
    if (!selected_) return nullptr;
    for (const Document::StageOutline& outline : outlines_) {
        if (outline.depth == *selected_) return &outline;
    }
    return nullptr;
}

QPointF Viewport::TurnHandle(const Document::StageOutline& outline) const {
    QPointF centre;
    for (const Document::Point& corner : outline.corners)
        centre += ToWidget(corner) / static_cast<double>(outline.corners.size());
    const QPointF top = Middle(ToWidget(outline.corners[0]), ToWidget(outline.corners[1]));
    QLineF away(centre, top);
    if (away.length() < 1.0) away = QLineF(top, top + QPointF(0, -1));
    away.setLength(away.length() + kTurnDistance);
    return away.p2();
}

Viewport::Gesture Viewport::GestureAt(const Document::StageOutline& outline, QPointF widget,
                                      Document::Point stage) const {
    for (const Document::Point& corner : outline.corners) {
        if (QLineF(ToWidget(corner), widget).length() <= kHandleReach) return Gesture::Scale;
    }
    if (QLineF(TurnHandle(outline), widget).length() <= kHandleReach) return Gesture::Turn;
    const std::vector<Document::StageOutline> only{outline};
    return Document::DepthAt(only, stage) ? Gesture::Move : Gesture::None;
}

Document::StageOutline Viewport::Preview(const Document::StageOutline& outline) const {
    if (!dragging_) return outline;
    switch (gesture_) {
    case Gesture::Move: {
        Document::StageOutline moved = outline;
        const Document::Point offset = Moved(outline).offset;
        const double dx = offset[0];
        const double dy = offset[1];
        for (Document::Point& corner : moved.corners)
            corner = {corner[0] + dx, corner[1] + dy};
        moved.anchor = {moved.anchor[0] + dx, moved.anchor[1] + dy};
        return moved;
    }
    case Gesture::Scale:
        return Document::ReshapedOutline(outline, Document::ScaleToReach(outline, grab_, pointer_));
    case Gesture::Turn:
        return Document::ReshapedOutline(outline, Document::TurnToReach(outline, grab_, pointer_));
    case Gesture::None:
        break;
    }
    return outline;
}

void Viewport::DrawSelection(QPainter& painter, const Document::StageOutline& outline) const {
    QPolygonF polygon;
    for (const Document::Point& corner : outline.corners)
        polygon << ToWidget(corner);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(polygon);
    const QPointF handle = TurnHandle(outline);
    painter.drawLine(Middle(polygon[0], polygon[1]), handle);
    const QPointF anchor = ToWidget(outline.anchor);
    painter.drawLine(anchor - QPointF(kAnchorSize, 0), anchor + QPointF(kAnchorSize, 0));
    painter.drawLine(anchor - QPointF(0, kAnchorSize), anchor + QPointF(0, kAnchorSize));
    painter.setBrush(kSelectedColour);
    const QPointF half(kHandleSize / 2, kHandleSize / 2);
    for (const QPointF& corner : polygon)
        painter.drawRect(QRectF(corner - half, corner + half));
    painter.drawEllipse(handle, kHandleSize / 2, kHandleSize / 2);
}

void Viewport::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0, 0, 0));
    if (frame_.isNull()) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(rect(), Qt::AlignCenter, message_);
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(Target(), frame_);
    const Document::StageOutline* selected = SelectedOutline();
    if (selected == nullptr || stage_.isEmpty()) return;
    DrawGuides(painter, *selected);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kSelectedColour, kOutlineWidth));
    DrawSelection(painter, Preview(*selected));
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit Resized(event->size().width(), event->size().height());
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    gesture_ = Gesture::None;
    dragging_ = false;
    if (event->button() != Qt::LeftButton) return;
    const std::optional<QPointF> stage = ToStage(event->position());
    if (!stage) return;
    const Document::Point point{stage->x(), stage->y()};
    const Document::StageOutline* selected = SelectedOutline();
    Gesture gesture =
        selected != nullptr ? GestureAt(*selected, event->position(), point) : Gesture::None;
    if (gesture != Gesture::Scale && gesture != Gesture::Turn) {
        emit Picked(point[0], point[1]);
        selected = SelectedOutline();
        gesture =
            selected != nullptr && GestureAt(*selected, event->position(), point) == Gesture::Move
                ? Gesture::Move
                : Gesture::None;
    }
    gesture_ = gesture;
    press_ = event->position();
    grab_ = point;
    pointer_ = point;
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    if (gesture_ == Gesture::None || (event->buttons() & Qt::LeftButton) == 0) return;
    if (!dragging_ && QLineF(press_, event->position()).length() < kDragThreshold) return;
    const std::optional<QPointF> stage = ToStage(event->position());
    if (!stage) return;
    dragging_ = true;
    snap_suspended_ = (event->modifiers() & Qt::AltModifier) != 0;
    pointer_ = {stage->x(), stage->y()};
    update();
    const Document::StageOutline* selected = SelectedOutline();
    if (selected != nullptr) EmitGesture(gesture_, *selected, false);
}

void Viewport::EmitGesture(Gesture gesture, const Document::StageOutline& outline, bool finished) {
    switch (gesture) {
    case Gesture::Move: {
        const Document::Point offset = Moved(outline).offset;
        emit Dragged(outline.depth, offset[0], offset[1], finished);
        break;
    }
    case Gesture::Scale: {
        const Document::Reshape reshape = Document::ScaleToReach(outline, grab_, pointer_);
        emit Reshaped(outline.depth, reshape.scale_x, reshape.scale_y, reshape.turn, finished);
        break;
    }
    case Gesture::Turn: {
        const Document::Reshape reshape = Document::TurnToReach(outline, grab_, pointer_);
        emit Reshaped(outline.depth, reshape.scale_x, reshape.scale_y, reshape.turn, finished);
        break;
    }
    case Gesture::None:
        break;
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const Gesture gesture = dragging_ ? gesture_ : Gesture::None;
    const Document::StageOutline* selected = SelectedOutline();
    gesture_ = Gesture::None;
    dragging_ = false;
    update();
    if (selected == nullptr) return;
    const Document::StageOutline outline = *selected;
    EmitGesture(gesture, outline, true);
}

}
