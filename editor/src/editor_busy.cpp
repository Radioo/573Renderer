#include "editor_busy.h"

#include "editor_theme.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace Editor {

namespace {

constexpr int kBarWidth = 260;
constexpr int kBarHeight = 4;
constexpr int kWhatSize = 13;
constexpr int kDetailSize = 11;

}

Busy::Busy(QWidget* parent)
    : QWidget(parent), what_(new QLabel), detail_(new QLabel), bar_(new QProgressBar),
      stop_(new QPushButton(tr("Stop"))) {
    setObjectName("busy");
    what_->setObjectName("busy_what");
    QFont named(Theme::SansFamily());
    named.setPixelSize(kWhatSize);
    what_->setFont(named);
    detail_->setObjectName("busy_detail");
    QFont said(Theme::SansFamily());
    said.setPixelSize(kDetailSize);
    detail_->setFont(said);
    bar_->setObjectName("busy_bar");
    bar_->setTextVisible(false);
    bar_->setFixedSize(kBarWidth, kBarHeight);
    bar_->setRange(0, 0);
    stop_->setObjectName("busy_stop");
    stop_->setVisible(false);
    connect(stop_, &QPushButton::clicked, this, &Busy::StopAsked);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(what_, 0, Qt::AlignHCenter);
    layout->addWidget(bar_, 0, Qt::AlignHCenter);
    layout->addWidget(detail_, 0, Qt::AlignHCenter);
    auto* stopping = new QHBoxLayout;
    stopping->addStretch();
    stopping->addWidget(stop_);
    stopping->addStretch();
    layout->addLayout(stopping);
    layout->addStretch();
}

void Busy::Say(const QString& what) {
    what_->setText(what);
    detail_->clear();
    bar_->setRange(0, 0);
}

void Busy::Move(int done, int total, const QString& detail) {
    bar_->setRange(0, total);
    bar_->setValue(done);
    detail_->setText(detail);
}

void Busy::AllowStopping(bool allowed) {
    stop_->setVisible(allowed);
}

}
