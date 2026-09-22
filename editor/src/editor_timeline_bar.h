#pragma once

#include "document/playback.h"

#include "editor_icons.h"

#include <QString>
#include <QWidget>

#include <cstdint>
#include <optional>

class QHBoxLayout;
class QLabel;
class QSpinBox;
class QSlider;
class QToolButton;

namespace Editor {

class Commands;

struct TimelineState {
    uint32_t frame = 0;
    uint32_t frame_count = 0;
    double rate = 0;
    QString label;
    std::optional<Document::WorkArea> work_area;
    double zoom_pixels = 0;
};

class TimelineBar : public QWidget {
    Q_OBJECT

public:
    TimelineBar(Commands& commands, QWidget* parent = nullptr);

    void Build();
    void Show(const TimelineState& state);

signals:
    void FrameTyped(uint32_t frame);
    void GraphAsked(bool graph);
    void ZoomAsked(double pixels);

private:
    [[nodiscard]] QHBoxLayout* Row() const;
    [[nodiscard]] QToolButton* AddCommand(const QString& id, Icons::Glyph glyph);
    [[nodiscard]] QToolButton* ZoomStep(const QString& id, Icons::Glyph glyph);

    Commands& commands_;
    QSpinBox* frame_;
    QLabel* count_;
    QLabel* time_;
    QLabel* label_;
    QLabel* work_area_;
    QToolButton* timeline_;
    QToolButton* graph_;
    QSlider* zoom_;
};

}
