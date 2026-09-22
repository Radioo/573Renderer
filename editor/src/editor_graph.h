#pragma once

#include "document/keyframes.h"

#include <QColor>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class QMouseEvent;
class QPaintEvent;

namespace Editor {

class GraphEditor : public QWidget {
    Q_OBJECT

public:
    explicit GraphEditor(QWidget* parent = nullptr);

    void ShowTrack(std::optional<Document::Track> track, uint32_t first_frame, uint32_t last_frame,
                   uint32_t playhead);
    void ShowTracks(std::vector<Document::Track> tracks, std::vector<std::string> shown,
                    uint32_t first_frame, uint32_t last_frame, uint32_t playhead);
    void SetFrame(uint32_t frame);
    [[nodiscard]] std::optional<QPointF> KeyPoint(uint32_t frame, std::size_t component) const;
    [[nodiscard]] static QColor ColourOf(std::size_t track);
    void Fit(bool keys_only);

signals:
    void KeyChosen(const QString& property, uint32_t frame);
    void KeyMoved(const QString& property, uint32_t frame, uint32_t to_frame,
                  std::vector<int64_t> value);
    void EaseEdited(const QString& property, uint32_t frame, const Document::Bezier& bezier);
    void FrameChosen(uint32_t frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct Grab {
        std::size_t track = 0;
        std::size_t key = 0;
        std::size_t component = 0;
    };

    struct Handle {
        std::size_t track = 0;
        std::size_t key = 0;
        bool second = false;
    };

    [[nodiscard]] std::vector<std::size_t> Drawn() const;
    [[nodiscard]] Document::Track Shown(std::size_t track) const;
    [[nodiscard]] QPointF ToWidget(double frame, double value) const;
    [[nodiscard]] double ValueAt(double y) const;
    [[nodiscard]] uint32_t FrameAt(double x) const;
    [[nodiscard]] std::optional<Grab> KeyNear(QPointF widget) const;
    [[nodiscard]] std::optional<Handle> HandleNear(QPointF widget) const;
    [[nodiscard]] std::optional<QPointF> HandlePoint(const Handle& handle) const;
    [[nodiscard]] uint32_t FrameBetweenNeighbours(uint32_t frame) const;
    void DrawTrack(QPainter& painter, std::size_t track) const;
    void DrawHandles(QPainter& painter, std::size_t track) const;
    [[nodiscard]] Document::Bezier DraggedCurve(const Document::Keyframe& key) const;

    std::vector<Document::Track> tracks_;
    std::vector<std::string> shown_;
    uint32_t first_frame_ = 0;
    uint32_t last_frame_ = 0;
    uint32_t playhead_ = 0;
    double lowest_ = 0.0;
    double highest_ = 1.0;
    bool keys_only_ = false;
    std::optional<Grab> grabbed_;
    std::optional<Handle> handled_;
    std::optional<Document::Bezier> curve_;
    std::optional<std::vector<int64_t>> dragged_;
    std::optional<uint32_t> dragged_frame_;
    QPointF pressed_at_;
};

}
