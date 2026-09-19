#include "editor_filter.h"

#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace Editor {

namespace {

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

QWidget* WithFilter(QTreeWidget* tree, QLineEdit* filter) {
    filter->setPlaceholderText(QObject::tr("Search"));
    filter->setClearButtonEnabled(true);
    QObject::connect(filter, &QLineEdit::textChanged, tree,
                     [tree](const QString& text) { ApplyFilter(*tree, text); });
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(filter);
    layout->addWidget(tree);
    return panel;
}

}
