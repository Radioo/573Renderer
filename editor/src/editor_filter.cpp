#include "editor_filter.h"

#include "editor_icons.h"
#include "editor_theme.h"

#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace Editor {

namespace {

constexpr int kFilterIcon = 13;
constexpr int kFilterHeight = 26;

bool Show(QTreeWidgetItem& item, const QString& text, bool ancestor_matched) {
    const bool matched = ancestor_matched || item.text(0).contains(text, Qt::CaseInsensitive);
    bool inside = false;
    for (int child = 0; child < item.childCount(); child++)
        inside = Show(*item.child(child), text, matched) || inside;
    const bool shown = matched || inside;
    item.setHidden(!shown);
    return shown;
}

}

void ApplyFilter(QTreeWidget& tree, const QString& text) {
    for (int top = 0; top < tree.topLevelItemCount(); top++)
        Show(*tree.topLevelItem(top), text, false);
}

QWidget* WithFilter(QTreeWidget* tree, QLineEdit* filter, const QString& placeholder) {
    filter->setObjectName("filter_field");
    filter->setPlaceholderText(placeholder);
    filter->setClearButtonEnabled(true);
    filter->addAction(Icons::Of(Icons::Glyph::Search, Theme::kFaint, kFilterIcon),
                      QLineEdit::LeadingPosition);
    filter->setFixedHeight(kFilterHeight);
    QObject::connect(filter, &QLineEdit::textChanged, tree,
                     [tree](const QString& text) { ApplyFilter(*tree, text); });
    auto* around = new QWidget;
    auto* margins = new QVBoxLayout(around);
    margins->setContentsMargins(8, 6, 8, 2);
    margins->setSpacing(0);
    margins->addWidget(filter);
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(around);
    layout->addWidget(tree, 1);
    return panel;
}

}
