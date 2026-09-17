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
constexpr int kOutlineWidth = 2;
const QColor kSelectedColour(80, 200, 255);

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

void Viewport::DrawOutline(QPainter& painter, const Document::StageOutline& outline,
                           QPointF shift) const {
    QPolygonF polygon;
    for (const Document::Point& corner : outline.corners)
        polygon << ToWidget({corner[0] + shift.x(), corner[1] + shift.y()});
    painter.drawPolygon(polygon);
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
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kSelectedColour, kOutlineWidth));
    DrawOutline(painter, *selected, dragging_ ? drag_offset_ : QPointF());
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit Resized(event->size().width(), event->size().height());
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    drag_start_.reset();
    dragging_ = false;
    if (event->button() != Qt::LeftButton) return;
    const std::optional<QPointF> stage = ToStage(event->position());
    if (!stage) return;
    emit Picked(stage->x(), stage->y());
    const Document::StageOutline* selected = SelectedOutline();
    if (selected == nullptr) return;
    const std::vector<Document::StageOutline> only{*selected};
    if (Document::DepthAt(only, {stage->x(), stage->y()})) drag_start_ = event->position();
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    if (!drag_start_ || (event->buttons() & Qt::LeftButton) == 0) return;
    if (!dragging_ && QLineF(*drag_start_, event->position()).length() < kDragThreshold) return;
    const std::optional<QPointF> from = ToStage(*drag_start_);
    const std::optional<QPointF> to = ToStage(event->position());
    if (!from || !to) return;
    dragging_ = true;
    drag_offset_ = *to - *from;
    update();
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const bool moved = dragging_ && selected_.has_value();
    const QPointF offset = drag_offset_;
    drag_start_.reset();
    dragging_ = false;
    drag_offset_ = QPointF();
    update();
    if (moved) emit Dragged(*selected_, offset.x(), offset.y());
}

}
