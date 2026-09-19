#pragma once

#include <QString>

class QLineEdit;
class QTreeWidget;
class QWidget;

namespace Editor {

void ApplyFilter(QTreeWidget& tree, const QString& text);

[[nodiscard]] QWidget* WithFilter(QTreeWidget* tree, QLineEdit* filter);

}
