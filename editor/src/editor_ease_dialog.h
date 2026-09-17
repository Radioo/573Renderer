#pragma once

#include "document/keyframes.h"

#include <QDialog>
#include <QPointF>
#include <QWidget>

#include <cstddef>
#include <optional>

class QLineEdit;
class QMouseEvent;
class QPaintEvent;

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

class EaseDialog : public QDialog {
    Q_OBJECT

public:
    EaseDialog(const Document::Bezier& initial, QWidget* parent);

    [[nodiscard]] const Document::Bezier& Result() const { return curve_->Curve(); }

private:
    void ShowNumbers();
    void ReadNumbers();

    CurveEditor* curve_ = nullptr;
    QLineEdit* numbers_ = nullptr;
};

}
