#include "editor_ease_dialog.h"

#include "document/keyframes.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <array>
#include <cstddef>
#include <optional>

namespace Editor {

namespace {

constexpr int kMinimumSide = 280;
constexpr double kMargin = 24.0;
constexpr double kLowest = -0.5;
constexpr double kHighest = 1.5;
constexpr int kSteps = 120;
constexpr double kHandleRadius = 5.0;
constexpr double kHandleReach = 9.0;
const QColor kFrame(90, 90, 96);
const QColor kCurve(80, 200, 255);
const QColor kHandle(240, 190, 80);

QString CurveText(const Document::Bezier& bezier) {
    return QString("%1, %2, %3, %4")
        .arg(bezier.x1, 0, 'g', 4)
        .arg(bezier.y1, 0, 'g', 4)
        .arg(bezier.x2, 0, 'g', 4)
        .arg(bezier.y2, 0, 'g', 4);
}

std::optional<Document::Bezier> CurveFrom(const QString& text) {
    const QStringList parts = text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    if (parts.size() != 4) return std::nullopt;
    std::array<double, 4> numbers{};
    for (std::size_t i = 0; i < numbers.size(); i++) {
        bool ok = false;
        numbers.at(i) = parts[static_cast<qsizetype>(i)].toDouble(&ok);
        if (!ok) return std::nullopt;
    }
    return Document::Bezier{.x1 = numbers[0], .y1 = numbers[1], .x2 = numbers[2], .y2 = numbers[3]};
}

}

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent) {
    setMinimumSize(kMinimumSide, kMinimumSide);
}

void CurveEditor::SetCurve(const Document::Bezier& bezier) {
    bezier_ = Document::WithinTime(bezier);
    update();
}

QPointF CurveEditor::ToWidget(double time, double progress) const {
    const double across = width() - (2 * kMargin);
    const double down = height() - (2 * kMargin);
    return {kMargin + (time * across),
            kMargin + ((kHighest - progress) / (kHighest - kLowest) * down)};
}

QPointF CurveEditor::ToCurve(QPointF widget) const {
    const double across = width() - (2 * kMargin);
    const double down = height() - (2 * kMargin);
    return {(widget.x() - kMargin) / across,
            kHighest - ((widget.y() - kMargin) / down * (kHighest - kLowest))};
}

std::optional<std::size_t> CurveEditor::HandleAt(QPointF widget) const {
    const std::array<QPointF, 2> handles{ToWidget(bezier_.x1, bezier_.y1),
                                         ToWidget(bezier_.x2, bezier_.y2)};
    for (std::size_t i = 0; i < handles.size(); i++) {
        if (QLineF(handles.at(i), widget).length() <= kHandleReach) return i;
    }
    return std::nullopt;
}

void CurveEditor::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(kFrame);
    painter.drawRect(QRectF(ToWidget(0, 1), ToWidget(1, 0)));

    QPainterPath path(ToWidget(0, 0));
    for (int step = 1; step <= kSteps; step++) {
        const double time = static_cast<double>(step) / kSteps;
        path.lineTo(ToWidget(time, Document::EaseProgress(Document::Ease::Bezier, bezier_, time)));
    }
    painter.setPen(QPen(kCurve, 2));
    painter.drawPath(path);

    const QPointF first = ToWidget(bezier_.x1, bezier_.y1);
    const QPointF second = ToWidget(bezier_.x2, bezier_.y2);
    painter.setPen(QPen(kHandle, 1));
    painter.drawLine(ToWidget(0, 0), first);
    painter.drawLine(ToWidget(1, 1), second);
    painter.setBrush(kHandle);
    painter.drawEllipse(first, kHandleRadius, kHandleRadius);
    painter.drawEllipse(second, kHandleRadius, kHandleRadius);
}

void CurveEditor::mousePressEvent(QMouseEvent* event) {
    grabbed_ = HandleAt(event->position());
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event) {
    if (!grabbed_) return;
    const QPointF point = ToCurve(event->position());
    Document::Bezier moved = bezier_;
    if (*grabbed_ == 0) {
        moved.x1 = point.x();
        moved.y1 = point.y();
    } else {
        moved.x2 = point.x();
        moved.y2 = point.y();
    }
    SetCurve(moved);
    emit CurveEdited();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent* event) {
    grabbed_.reset();
    event->accept();
}

EaseDialog::EaseDialog(const Document::Bezier& initial, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Bezier ease"));
    curve_ = new CurveEditor;
    curve_->SetCurve(initial);
    numbers_ = new QLineEdit;
    numbers_->setToolTip(tr("Control points as x1, y1, x2, y2"));
    auto* presets = new QHBoxLayout;
    for (const Document::EasePreset& preset : Document::EasePresets()) {
        auto* button = new QPushButton(QString::fromStdString(preset.name));
        const Document::Bezier chosen = preset.bezier;
        connect(button, &QPushButton::clicked, this, [this, chosen] {
            curve_->SetCurve(chosen);
            ShowNumbers();
        });
        presets->addWidget(button);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(curve_, &CurveEditor::CurveEdited, this, &EaseDialog::ShowNumbers);
    connect(numbers_, &QLineEdit::editingFinished, this, &EaseDialog::ReadNumbers);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(curve_, 1);
    layout->addLayout(presets);
    layout->addWidget(numbers_);
    layout->addWidget(buttons);
    ShowNumbers();
}

void EaseDialog::ShowNumbers() {
    numbers_->setText(CurveText(curve_->Curve()));
}

void EaseDialog::ReadNumbers() {
    const std::optional<Document::Bezier> typed = CurveFrom(numbers_->text());
    if (typed) curve_->SetCurve(*typed);
    ShowNumbers();
}

}
