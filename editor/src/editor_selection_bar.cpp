#include "editor_selection_bar.h"

#include "editor_commands.h"
#include "editor_icons.h"
#include "editor_rows.h"
#include "editor_theme.h"

#include <QAction>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QPixmap>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>

#include <optional>

namespace Editor {

namespace {

constexpr int kBarHeight = 40;
constexpr int kThumbWidth = 22;
constexpr int kThumbHeight = 16;
constexpr int kButtonHeight = 28;
constexpr int kIconSide = 16;
constexpr int kSummarySize = 12;
constexpr int kDetailSize = 11;

QFrame* Divider() {
    auto* line = new QFrame;
    line->setObjectName("divider");
    line->setFixedSize(1, 24);
    return line;
}

}

SelectionBar::SelectionBar(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), summary_(new QLabel), detail_(new QLabel),
      thumbnail_(new QLabel) {
    setObjectName("selection_bar");
    setFixedHeight(kBarHeight);
    summary_->setObjectName("selection_summary");
    QFont named(Theme::SansFamily());
    named.setPixelSize(kSummarySize);
    named.setBold(true);
    summary_->setFont(named);
    detail_->setObjectName("selection_detail");
    QFont said(Theme::SansFamily());
    said.setPixelSize(kDetailSize);
    detail_->setFont(said);
    thumbnail_->setObjectName("selection_thumbnail");
    thumbnail_->setFixedSize(kThumbWidth, kThumbHeight);

    auto* inside = new QWidget;
    inside->setObjectName("selection_inside");
    auto* layout = new QHBoxLayout(inside);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);
    layout->addWidget(thumbnail_);
    auto* named_column = new QVBoxLayout;
    named_column->setContentsMargins(0, 0, 0, 0);
    named_column->setSpacing(0);
    named_column->addWidget(summary_);
    named_column->addWidget(detail_);
    layout->addLayout(named_column);
    divider_ = Divider();
    layout->addWidget(divider_);
    buttons_ = new QHBoxLayout;
    buttons_->setContentsMargins(0, 0, 0, 0);
    buttons_->setSpacing(2);
    layout->addLayout(buttons_);
    layout->addStretch();
    removals_ = new QHBoxLayout;
    removals_->setContentsMargins(0, 0, 0, 0);
    removals_->setSpacing(2);
    layout->addLayout(removals_);

    auto* room = new QScrollArea;
    room->setObjectName("selection_room");
    room->setWidget(inside);
    room->setWidgetResizable(true);
    room->setFrameShape(QFrame::NoFrame);
    room->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    room->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* around = new QHBoxLayout(this);
    around->setContentsMargins(0, 0, 0, 0);
    around->setSpacing(0);
    around->addWidget(room);
}

void SelectionBar::Show(const QString& summary, const QString& detail,
                        const std::vector<QString>& ids) {
    summary_->setText(summary);
    detail_->setText(detail);
    thumbnail_->setVisible(!ids.empty());
    divider_->setVisible(!ids.empty());
    if (!ids.empty())
        thumbnail_->setPixmap(Rows::Stripes(kThumbWidth, kThumbHeight, summary.size()));
    for (QHBoxLayout* row : {buttons_, removals_}) {
        while (QLayoutItem* item = row->takeAt(0)) {
            delete item->widget();
            delete item;
        }
    }
    for (const QString& id : ids) {
        const QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        const bool removes = id.endsWith(".remove") || id.endsWith(".delete");
        auto* button = new QToolButton;
        button->setObjectName("bar_" + id);
        button->setProperty("selection", true);
        button->setFixedHeight(kButtonHeight);
        if (removes) {
            button->setIcon(Icons::Of(Icons::Glyph::Trash, Theme::kSoft, kIconSide));
            button->setIconSize(QSize(kIconSide, kIconSide));
            button->setFixedWidth(kButtonHeight);
        } else {
            button->setText(commands_.Brief(id));
        }
        const std::optional<QString> refused = commands_.Refusal(id);
        button->setEnabled(!refused);
        button->setToolTip(refused.value_or(QString(action->text()).remove('&')));
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        if (removes) {
            removals_->addWidget(button);
            continue;
        }
        const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
        if (!shortcut.isEmpty()) button->setToolTip(button->toolTip() + " (" + shortcut + ")");
        buttons_->addWidget(button);
    }
}

}
