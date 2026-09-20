#include "editor_stage_bar.h"

#include "editor_commands.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>

#include <cmath>

namespace Editor {

namespace {

constexpr int kZoomWidth = 70;

}

StageBar::StageBar(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), zoom_(new QLabel) {
    setObjectName("stage_bar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);
    crumbs_ = new QHBoxLayout;
    crumbs_->setContentsMargins(0, 0, 0, 0);
    crumbs_->setSpacing(2);
    layout->addLayout(crumbs_);
    layout->addStretch();
    buttons_ = new QHBoxLayout;
    buttons_->setContentsMargins(0, 0, 0, 0);
    buttons_->setSpacing(4);
    layout->addLayout(buttons_);
}

void StageBar::Build() {
    for (const auto& [id, text] :
         {std::pair{QStringLiteral("view.snap"), tr("Snap")},
          std::pair{QStringLiteral("view.rulers"), tr("Rulers")},
          std::pair{QStringLiteral("view.onion"), tr("Onion")},
          std::pair{QStringLiteral("view.path"), tr("Path")},
          std::pair{QStringLiteral("view.background"), tr("Background")}}) {
        QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        auto* toggle = new QToolButton;
        toggle->setObjectName("stage_" + id);
        toggle->setText(text);
        toggle->setCheckable(true);
        toggle->setChecked(action->isChecked());
        connect(action, &QAction::toggled, toggle, &QToolButton::setChecked);
        connect(toggle, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        buttons_->addWidget(toggle);
    }

    const auto press = [this](const QString& id, const QString& text) {
        auto* button = new QToolButton;
        button->setObjectName("stage_" + id);
        button->setText(text);
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        buttons_->addWidget(button);
    };
    press(QStringLiteral("view.zoom_out"), tr("-"));
    zoom_->setObjectName("stage_zoom");
    zoom_->setAlignment(Qt::AlignCenter);
    zoom_->setFixedWidth(kZoomWidth);
    buttons_->addWidget(zoom_);
    press(QStringLiteral("view.zoom_in"), tr("+"));
    press(QStringLiteral("view.fit_stage"), tr("Fit"));
    press(QStringLiteral("file.save_frame"), tr("Picture"));
}

void StageBar::Show(const std::vector<Crumb>& crumbs, double scale) {
    zoom_->setText(scale > 0 ? tr("%1%").arg(std::lround(scale * 100)) : QString());
    if (crumbs == shown_) return;
    shown_ = crumbs;
    while (QLayoutItem* item = crumbs_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
    for (const Crumb& crumb : crumbs) {
        if (crumbs_->count() > 0) crumbs_->addWidget(new QLabel(QStringLiteral(">")));
        auto* button = new QToolButton;
        button->setObjectName("crumb_" + QString::number(crumb.clip));
        button->setText(crumb.text);
        button->setAutoRaise(true);
        const int wanted = crumb.clip;
        connect(button, &QToolButton::clicked, this, [this, wanted] { emit ClipAsked(wanted); });
        crumbs_->addWidget(button);
    }
}

}
