#include "editor_selection_bar.h"

#include "editor_commands.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QString>
#include <QToolButton>

#include <optional>

namespace Editor {

SelectionBar::SelectionBar(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), summary_(new QLabel) {
    setObjectName("selection_bar");
    summary_->setObjectName("selection_summary");
    QFont bold = summary_->font();
    bold.setBold(true);
    summary_->setFont(bold);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(6);
    layout->addWidget(summary_);
    buttons_ = new QHBoxLayout;
    buttons_->setContentsMargins(0, 0, 0, 0);
    buttons_->setSpacing(2);
    layout->addLayout(buttons_);
    layout->addStretch();
}

void SelectionBar::Show(const QString& summary, const std::vector<QString>& ids) {
    summary_->setText(summary);
    while (QLayoutItem* item = buttons_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const QString& id : ids) {
        const QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        auto* button = new QToolButton;
        button->setObjectName("bar_" + id);
        button->setText(commands_.Brief(id));
        const std::optional<QString> refused = commands_.Refusal(id);
        button->setEnabled(!refused);
        button->setToolTip(refused.value_or(QString(action->text()).remove('&')));
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        buttons_->addWidget(button);
    }
}

}
