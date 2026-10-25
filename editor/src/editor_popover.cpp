#include "editor_popover.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QPoint>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace Editor {

namespace {

constexpr int kWidth = 320;

void Empty(QVBoxLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

}

Popover::Popover(QWidget* owner)
    : QFrame(owner, Qt::Popup), owner_(owner), title_(new QLabel), detail_(new QLabel) {
    setObjectName("popover");
    setFrameShape(QFrame::StyledPanel);
    title_->setObjectName("popover_title");
    detail_->setObjectName("popover_detail");
    QFont bold = title_->font();
    bold.setBold(true);
    title_->setFont(bold);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);
    layout->addWidget(title_);
    auto* fields = new QWidget;
    rows_ = new QVBoxLayout(fields);
    rows_->setContentsMargins(0, 0, 0, 0);
    rows_->setSpacing(4);
    layout->addWidget(fields);
    layout->addWidget(detail_);
    buttons_ = new QWidget;
    layout->addWidget(buttons_);
    setFixedWidth(kWidth);
}

void Popover::Ask(PopoverAsk ask, QWidget* anchor) {
    ask_ = std::move(ask);
    boxes_.clear();
    Empty(rows_);
    delete buttons_->layout();
    qDeleteAll(buttons_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));

    title_->setText(ask_.title);
    for (const PopoverField& field : ask_.fields) {
        auto* row = new QWidget;
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        line->addWidget(new QLabel(field.label));
        line->addStretch();
        auto* box = new QDoubleSpinBox;
        box->setObjectName("popover_" + field.label);
        box->setDecimals(field.decimals);
        box->setRange(field.lowest, field.highest);
        box->setValue(field.value);
        box->setSuffix(field.suffix);
        box->setKeyboardTracking(false);
        connect(box, &QDoubleSpinBox::valueChanged, this, &Popover::Changed);
        boxes_.push_back(box);
        line->addWidget(box);
        rows_->addWidget(row);
    }

    auto* line = new QHBoxLayout(buttons_);
    line->setContentsMargins(0, 0, 0, 0);
    line->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setObjectName("popover_cancel");
    connect(cancel, &QPushButton::clicked, this, &Popover::hide);
    line->addWidget(cancel);
    auto* apply = new QPushButton(ask_.apply);
    apply->setObjectName("popover_apply");
    apply->setDefault(true);
    connect(apply, &QPushButton::clicked, this, [this] {
        applying_ = true;
        const PopoverValues values = Values();
        hide();
        ask_.run(values);
    });
    line->addWidget(apply);

    applying_ = false;
    Changed();
    const QWidget* at = anchor != nullptr ? anchor : owner_;
    move(at->mapToGlobal(QPoint(0, -sizeHint().height())));
    show();
    if (!boxes_.empty()) boxes_.front()->setFocus();
}

PopoverValues Popover::Values() const {
    PopoverValues values;
    values.reserve(boxes_.size());
    for (const QDoubleSpinBox* box : boxes_)
        values.push_back(box->value());
    return values;
}

void Popover::Changed() {
    const PopoverValues values = Values();
    if (ask_.describe) detail_->setText(ask_.describe(values));
    detail_->setVisible(!detail_->text().isEmpty());
    if (ask_.preview) ask_.preview(values);
}

void Popover::hideEvent(QHideEvent* event) {
    QFrame::hideEvent(event);
    if (applying_) return;
    if (ask_.cancelled) ask_.cancelled();
}

}
