#pragma once

#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/outline.h"
#include "document/timeline.h"

#include <QPoint>
#include <QRect>
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
    void SelectKeys(std::vector<Document::KeyRef> keys);
    [[nodiscard]] const std::vector<Document::KeyRef>& SelectedKeys() const {
        return selected_keys_;
    }
    void SelectDepth(std::optional<uint16_t> depth);
    void Clear();
    void SetFrame(uint32_t frame);

signals:
    void FrameChosen(uint32_t frame);
    void DepthChosen(uint32_t depth);
    void KeyChosen(const QString& property, uint32_t frame);
    void KeysShifted(int64_t by);
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
    void PressKeys(const Lane& lane, QPoint at, bool toggle);
    [[nodiscard]] bool IsSelected(const Document::KeyRef& key) const;
    void SelectBand(bool adding);
    void DrawKeys(QPainter& painter, const Document::Track& track, int y) const;
    void Resize();
    [[nodiscard]] QString LabelNear(int x) const;

    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    std::vector<Document::DepthRow> rows_;
    std::vector<Document::AnimationLabel> labels_;
    std::optional<uint16_t> keyed_depth_;
    std::optional<uint16_t> selected_depth_;
    std::vector<Document::Track> tracks_;
    std::vector<Document::KeyRef> selected_keys_;
    std::optional<uint32_t> drag_from_;
    uint32_t drag_to_ = 0;
    std::optional<QPoint> band_from_;
    QPoint band_to_;
    std::vector<Document::KeyRef> band_kept_;
    std::optional<double> zoom_;
};

}
