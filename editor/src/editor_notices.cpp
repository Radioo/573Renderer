#include "editor_notices.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

namespace Editor {

namespace {

constexpr int kLifeMs = 8000;
constexpr int kMostShown = 3;

}

Notices::Notices(QWidget* parent) : QWidget(parent) {
    setObjectName("notices");
    stack_ = new QVBoxLayout(this);
    stack_->setContentsMargins(6, 2, 6, 2);
    stack_->setSpacing(2);
    hide();
}

void Notices::Say(const QString& text, bool undoable) {
    auto* notice = new QFrame;
    notice->setObjectName("notice");
    notice->setFrameShape(QFrame::StyledPanel);
    auto* line = new QHBoxLayout(notice);
    line->setContentsMargins(8, 2, 4, 2);
    line->setSpacing(8);
    auto* said = new QLabel(text);
    said->setObjectName("notice_text");
    line->addWidget(said, 1);
    if (undoable) {
        auto* undo = new QPushButton(tr("Undo"));
        undo->setObjectName("notice_undo");
        connect(undo, &QPushButton::clicked, this, [this, notice] {
            Remove(notice);
            emit UndoAsked();
        });
        line->addWidget(undo);
    }
    auto* close = new QPushButton(tr("Close"));
    close->setObjectName("notice_close");
    connect(close, &QPushButton::clicked, this, [this, notice] { Remove(notice); });
    line->addWidget(close);

    stack_->addWidget(notice);
    while (stack_->count() > kMostShown)
        Remove(stack_->itemAt(0)->widget());
    show();
    QTimer::singleShot(kLifeMs, notice, [this, notice] { Remove(notice); });
}

void Notices::Remove(QWidget* notice) {
    if (notice == nullptr) return;
    stack_->removeWidget(notice);
    notice->hide();
    notice->setParent(nullptr);
    notice->deleteLater();
    if (stack_->count() == 0) hide();
}

}
