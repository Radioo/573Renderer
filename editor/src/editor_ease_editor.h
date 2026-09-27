#pragma once

#include "document/keyframes.h"

#include <QPointF>
#include <QWidget>

#include <cstddef>
#include <optional>

class QLabel;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QToolButton;

namespace Editor {

class CurveEditor : public QWidget {
    Q_OBJECT

public:
    explicit CurveEditor(QWidget* parent = nullptr);

    void SetCurve(const Document::Bezier& bezier);
    [[nodiscard]] const Document::Bezier& Curve() const { return bezier_; }

signals:
    void CurveEdited();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QPointF ToWidget(double time, double progress) const;
    [[nodiscard]] QPointF ToCurve(QPointF widget) const;
    [[nodiscard]] std::optional<std::size_t> HandleAt(QPointF widget) const;

    Document::Bezier bezier_;
    std::optional<std::size_t> grabbed_;
};

struct EaseView {
    Document::Ease ease = Document::Ease::Linear;
    Document::Bezier bezier;
    int keys = 0;
};

class EaseEditor : public QWidget {
    Q_OBJECT

public:
    explicit EaseEditor(QWidget* parent = nullptr);

    void Show(const EaseView& view);

signals:
    void EaseChosen(Document::Ease ease, const Document::Bezier& bezier);

private:
    void ShowNumbers();
    void ReadNumbers();
    void Choose(Document::Ease ease, const Document::Bezier& bezier);

    CurveEditor* curve_ = nullptr;
    QLineEdit* numbers_ = nullptr;
    QLabel* counted_ = nullptr;
    QToolButton* hold_ = nullptr;
    QToolButton* linear_ = nullptr;
    QToolButton* bezier_ = nullptr;
};

}
