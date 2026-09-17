#pragma once

#include "document/keyframes.h"
#include "document/outline.h"
#include "document/timeline.h"

#include <QPoint>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QScrollArea;
class QWheelEvent;

namespace Editor {

class Timeline : public QWidget {
    Q_OBJECT

public:
    explicit Timeline(QWidget* parent = nullptr);

    void ShowAnimation(uint32_t frame_count, std::vector<Document::DepthRow> rows,
                       std::vector<Document::AnimationLabel> labels);
    void ShowKeys(std::optional<uint16_t> depth, std::vector<Document::Track> tracks);
    void SelectKey(const QString& property, std::optional<uint32_t> frame);
    void Clear();
    void SetFrame(uint32_t frame);

signals:
    void FrameChosen(uint32_t frame);
    void DepthChosen(uint32_t depth);
    void KeyChosen(const QString& property, uint32_t frame);
    void KeyMoved(const QString& property, uint32_t from, uint32_t to);
    void MenuRequested(const QPoint& where, uint32_t frame, const QString& label);
    void KeyMenuRequested(const QPoint& where, const QString& property, uint32_t frame,
                          bool on_key);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Lane {
        bool is_property = false;
        uint16_t depth = 0;
        std::size_t track = 0;
    };

    [[nodiscard]] std::vector<Lane> Lanes() const;
    [[nodiscard]] std::optional<std::size_t> LaneAt(int y) const;
    [[nodiscard]] int FrameToX(uint32_t frame) const;
    [[nodiscard]] double PixelsPerFrame() const;
    void ApplyZoom();
    [[nodiscard]] QScrollArea* ScrollArea() const;
    void DrawTicks(QPainter& painter) const;
    [[nodiscard]] uint32_t XToFrame(int x) const;
    [[nodiscard]] std::optional<uint32_t> KeyNear(std::size_t track, int x) const;
    void ChooseAt(int x, int y);
    void Resize();
    [[nodiscard]] QString LabelNear(int x) const;

    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    std::vector<Document::DepthRow> rows_;
    std::vector<Document::AnimationLabel> labels_;
    std::optional<uint16_t> keyed_depth_;
    std::vector<Document::Track> tracks_;
    QString selected_property_;
    std::optional<uint32_t> selected_key_;
    std::optional<std::size_t> dragging_;
    std::optional<uint32_t> drag_from_;
    std::optional<double> zoom_;
};

}
