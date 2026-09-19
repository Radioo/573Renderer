#pragma once

#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/outline.h"
#include "document/playback.h"
#include "document/timeline.h"

#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

class QEvent;
class QPainter;

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

class QContextMenuEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;
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
    void SelectDepths(std::vector<uint16_t> depths);
    void SetHiddenDepths(std::vector<uint16_t> depths);
    void SetLockedDepths(std::vector<uint16_t> depths);
    void SetCharacterNames(std::map<uint16_t, QString> names);
    [[nodiscard]] QString SpanNameAt(QPoint at) const;
    void SetWorkArea(std::optional<Document::WorkArea> area);
    void Clear();
    void SetFrame(uint32_t frame);

signals:
    void FrameChosen(uint32_t frame);
    void DepthChosen(uint32_t depth);
    void DepthsChosen(std::vector<uint16_t> depths);
    void KeyChosen(const QString& property, uint32_t frame);
    void KeysShifted(int64_t by);
    void SpanMoved(uint16_t depth, uint32_t frame, int64_t by);
    void SpanTrimmed(uint16_t depth, uint32_t frame, uint32_t first, uint32_t last);
    void VisibilityToggled(uint16_t depth);
    void LockToggled(uint16_t depth);
    void MenuRequested(const QPoint& where, uint32_t frame, const QString& label);
    void KeyMenuRequested(const QPoint& where, const QString& property, uint32_t frame,
                          bool on_key);
    void CharacterDropped(uint16_t character, uint32_t frame, std::optional<uint16_t> depth);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum class SpanDrag : uint8_t { Move, TrimStart, TrimEnd };

    struct Lane {
        bool is_property = false;
        uint16_t depth = 0;
        std::size_t track = 0;
    };

    [[nodiscard]] std::vector<Lane> Lanes() const;
    [[nodiscard]] std::optional<std::size_t> LaneAt(int y) const;
    [[nodiscard]] bool TakesDrop(const QMimeData* data, QPointF at) const;
    [[nodiscard]] int FrameToX(uint32_t frame) const;
    [[nodiscard]] double PixelsPerFrame() const;
    void ApplyZoom();
    [[nodiscard]] QScrollArea* ScrollArea() const;
    void DrawTicks(QPainter& painter) const;
    [[nodiscard]] uint32_t XToFrame(int x) const;
    [[nodiscard]] std::optional<uint32_t> KeyNear(std::size_t track, int x) const;
    void ChooseAt(int x, int y);
    void PressKeys(const Lane& lane, QPoint at, bool toggle);
    void PressSpan(const Lane& lane, QPoint at);
    [[nodiscard]] std::optional<Document::Span> SpanAt(uint16_t depth, int x) const;
    [[nodiscard]] SpanDrag DragAt(const Document::Span& span, int x) const;
    [[nodiscard]] Document::Span Dragged(const Document::Span& span, uint32_t to) const;
    [[nodiscard]] uint32_t SnappedTo(uint16_t depth, uint32_t to) const;
    void ShowHoverCursor(QPoint at);
    [[nodiscard]] bool IsSelected(const Document::KeyRef& key) const;
    void SelectBand(bool adding);
    void DrawKeys(QPainter& painter, const Document::Track& track, int y) const;
    void DrawSpanGhost(QPainter& painter, const Document::Span& span, int y) const;
    void Resize();
    [[nodiscard]] QString LabelNear(int x) const;
    [[nodiscard]] bool PressSwitch(const Lane& lane, QPoint at);
    [[nodiscard]] bool PressDepthNumber(const Lane& lane, QPoint at,
                                        Qt::KeyboardModifiers modifiers);
    void DrawSwitches(QPainter& painter, uint16_t depth, int y) const;
    [[nodiscard]] QString SpanName(const Document::DepthRow& row, const Document::Span& span) const;
    void DrawSpanName(QPainter& painter, const QString& name, const QRect& bar) const;

    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    std::vector<Document::DepthRow> rows_;
    std::vector<Document::AnimationLabel> labels_;
    std::optional<uint16_t> keyed_depth_;
    std::vector<uint16_t> selected_depths_;
    std::vector<uint16_t> hidden_depths_;
    std::vector<uint16_t> locked_depths_;
    std::map<uint16_t, QString> names_;
    std::optional<Document::WorkArea> work_area_;
    std::vector<Document::Track> tracks_;
    std::vector<Document::KeyRef> selected_keys_;
    std::optional<uint32_t> drag_from_;
    uint32_t drag_to_ = 0;
    std::optional<QPoint> band_from_;
    std::optional<uint16_t> span_depth_;
    std::optional<Document::Span> span_grabbed_;
    int span_press_x_ = 0;
    uint32_t span_from_ = 0;
    uint32_t span_to_ = 0;
    bool span_dragging_ = false;
    SpanDrag span_drag_ = SpanDrag::Move;
    QPoint band_to_;
    std::vector<Document::KeyRef> band_kept_;
    std::optional<double> zoom_;
};

}
