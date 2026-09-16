#pragma once

#include "document/outline.h"
#include "document/timeline.h"

#include <QPoint>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <vector>

class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;

namespace Editor {

class Timeline : public QWidget {
    Q_OBJECT

public:
    explicit Timeline(QWidget* parent = nullptr);

    void ShowAnimation(uint32_t frame_count, std::vector<Document::DepthRow> rows,
                       std::vector<Document::AnimationLabel> labels);
    void Clear();
    void SetFrame(uint32_t frame);

signals:
    void FrameChosen(uint32_t frame);
    void DepthChosen(uint32_t depth);
    void MenuRequested(const QPoint& where, uint32_t frame, const QString& label);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    [[nodiscard]] int FrameToX(uint32_t frame) const;
    [[nodiscard]] uint32_t XToFrame(int x) const;
    void ChooseAt(int x, int y);
    [[nodiscard]] QString LabelNear(int x) const;

    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    std::vector<Document::DepthRow> rows_;
    std::vector<Document::AnimationLabel> labels_;
};

}
