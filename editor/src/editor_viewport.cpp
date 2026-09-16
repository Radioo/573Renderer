#include "editor_viewport.h"

#include <QColor>
#include <QPainter>
#include <QPaintEvent>
#include <QPoint>
#include <QRect>
#include <QResizeEvent>
#include <QSize>

namespace Editor {

namespace {

constexpr int kMinimumWidth = 320;
constexpr int kMinimumHeight = 180;

}

Viewport::Viewport(QWidget* parent) : QWidget(parent) {
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    setAutoFillBackground(false);
    message_ = tr("No animation selected");
}

void Viewport::ShowFrame(const QImage& frame) {
    frame_ = frame;
    message_.clear();
    update();
}

void Viewport::ShowMessage(const QString& message) {
    frame_ = QImage();
    message_ = message;
    update();
}

void Viewport::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0, 0, 0));
    if (frame_.isNull()) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(rect(), Qt::AlignCenter, message_);
        return;
    }
    const QSize fitted = frame_.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2),
                       fitted);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, frame_);
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit Resized(event->size().width(), event->size().height());
}

}
