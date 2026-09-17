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
    void Reshaped(uint16_t depth, double scale_x, double scale_y, double turn);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Gesture : uint8_t { None, Move, Scale, Turn };

    [[nodiscard]] QRectF Target() const;
    [[nodiscard]] std::optional<QPointF> ToStage(QPointF widget) const;
    [[nodiscard]] QPointF ToWidget(const Document::Point& stage) const;
    [[nodiscard]] const Document::StageOutline* SelectedOutline() const;
    [[nodiscard]] QPointF TurnHandle(const Document::StageOutline& outline) const;
    [[nodiscard]] Gesture GestureAt(const Document::StageOutline& outline, QPointF widget,
                                    Document::Point stage) const;
    [[nodiscard]] Document::StageOutline Preview(const Document::StageOutline& outline) const;
    void DrawSelection(QPainter& painter, const Document::StageOutline& outline) const;

    QImage frame_;
    QSize stage_;
    QString message_;
    std::vector<Document::StageOutline> outlines_;
    std::optional<uint16_t> selected_;
    Gesture gesture_ = Gesture::None;
    QPointF press_;
    Document::Point grab_{};
    Document::Point pointer_{};
    bool dragging_ = false;
};

}
