#include "editor_stage_bar.h"

#include "editor_commands.h"
#include "editor_icons.h"
#include "editor_theme.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>

#include <array>
#include <cmath>
#include <utility>

namespace Editor {

namespace {

constexpr int kZoomWidth = 62;

QToolButton* IconButton(const QString& name) {
    auto* button = new QToolButton;
    button->setObjectName(name);
    button->setProperty("stage_icon", true);
    button->setIconSize(QSize(Theme::kStageIconSide, Theme::kStageIconSide));
    button->setFixedSize(Theme::kStageButtonSide, Theme::kStageButtonSide);
    return button;
}

}

StageBar::StageBar(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), zoom_(new QToolButton) {
    setObjectName("stage_bar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setFixedHeight(Theme::kStageBarHeight);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 6, 0);
    layout->setSpacing(0);
    crumbs_ = new QHBoxLayout;
    crumbs_->setContentsMargins(0, 0, 0, 0);
    crumbs_->setSpacing(2);
    layout->addLayout(crumbs_);
    layout->addStretch();
    buttons_ = new QHBoxLayout;
    buttons_->setContentsMargins(0, 0, 0, 0);
    buttons_->setSpacing(1);
    layout->addLayout(buttons_);
}

void StageBar::Build() {
    const std::array<std::pair<QString, Icons::Glyph>, 5> toggles{
        {{QStringLiteral("view.rulers"), Icons::Glyph::Rulers},
         {QStringLiteral("view.snap"), Icons::Glyph::Snap},
         {QStringLiteral("view.onion"), Icons::Glyph::Onion},
         {QStringLiteral("view.path"), Icons::Glyph::Path},
         {QStringLiteral("view.background"), Icons::Glyph::Background}}};
    for (const auto& [id, glyph] : toggles) {
        QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        QToolButton* toggle = IconButton("stage_" + id);
        toggle->setIcon(
            Icons::Toggling(glyph, Theme::kSoft, Theme::OnChosen(), Theme::kStageIconSide));
        toggle->setToolTip(action->text().remove('&'));
        toggle->setCheckable(true);
        toggle->setChecked(action->isChecked());
        connect(action, &QAction::toggled, toggle, &QToolButton::setChecked);
        connect(toggle, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        buttons_->addWidget(toggle);
    }

    auto* divider = new QFrame;
    divider->setObjectName("divider");
    divider->setFixedSize(1, 20);
    buttons_->addSpacing(6);
    buttons_->addWidget(divider);
    buttons_->addSpacing(6);

    zoom_->setObjectName("stage_zoom");
    zoom_->setPopupMode(QToolButton::InstantPopup);
    zoom_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    zoom_->setIcon(Icons::Of(Icons::Glyph::Chevron, Theme::kSoft, 12));
    zoom_->setLayoutDirection(Qt::RightToLeft);
    zoom_->setFixedSize(kZoomWidth, Theme::kStageButtonSide - 2);
    auto* zooming = new QMenu(zoom_);
    commands_.ShowAvailabilityIn(zooming);
    zooming->addAction(commands_.Action("view.zoom_in"));
    zooming->addAction(commands_.Action("view.zoom_out"));
    zooming->addAction(commands_.Action("view.fit_stage"));
    zoom_->setMenu(zooming);
    buttons_->addWidget(zoom_);

    const std::array<std::pair<QString, Icons::Glyph>, 2> presses{
        {{QStringLiteral("view.fit_stage"), Icons::Glyph::Fit},
         {QStringLiteral("file.save_frame"), Icons::Glyph::Frame}}};
    for (const auto& [id, glyph] : presses) {
        QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        QToolButton* button = IconButton("stage_" + id);
        button->setIcon(Icons::Of(glyph, Theme::kSoft, Theme::kStageIconSide));
        button->setToolTip(action->text().remove('&'));
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        buttons_->addWidget(button);
    }
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
        if (crumbs_->count() > 0) {
            auto* between = new QLabel(QStringLiteral(">"));
            between->setObjectName("crumb_between");
            crumbs_->addWidget(between);
        }
        auto* button = new QToolButton;
        button->setObjectName("crumb_" + QString::number(crumb.clip));
        button->setProperty("last", &crumb == &crumbs.back());
        button->setText(crumb.text);
        button->setFixedHeight(Theme::kStageButtonSide - 2);
        button->setAutoRaise(true);
        const int wanted = crumb.clip;
        connect(button, &QToolButton::clicked, this, [this, wanted] { emit ClipAsked(wanted); });
        crumbs_->addWidget(button);
    }
}

}
