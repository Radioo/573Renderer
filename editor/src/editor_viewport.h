#pragma once

#include "document/stage_bounds.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <optional>
#include <vector>

class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;

namespace Editor {

class Viewport : public QWidget {
    Q_OBJECT

public:
    explicit Viewport(QWidget* parent = nullptr);

    void ShowFrame(const QImage& frame, QSize stage);
    void ShowMessage(const QString& message);
    void ShowOutlines(std::vector<Document::StageOutline> outlines,
                      std::optional<uint16_t> selected);
    [[nodiscard]] QSize FittedSize(QSize available) const;

signals:
    void Resized(int width, int height);
    void Picked(double x, double y);
    void Dragged(uint16_t depth, double dx, double dy);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QRectF Target() const;
    [[nodiscard]] std::optional<QPointF> ToStage(QPointF widget) const;
    [[nodiscard]] QPointF ToWidget(const Document::Point& stage) const;
    [[nodiscard]] const Document::StageOutline* SelectedOutline() const;
    void DrawOutline(QPainter& painter, const Document::StageOutline& outline, QPointF shift) const;

    QImage frame_;
    QSize stage_;
    QString message_;
    std::vector<Document::StageOutline> outlines_;
    std::optional<uint16_t> selected_;
    std::optional<QPointF> drag_start_;
    QPointF drag_offset_;
    bool dragging_ = false;
};

}
