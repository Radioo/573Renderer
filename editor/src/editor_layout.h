#pragma once

#include <QByteArray>
#include <QString>

namespace ads {
class CDockManager;
}

class QMainWindow;

namespace Editor {

bool LayoutSized(const QByteArray& state);

void SaveLayout(const QMainWindow& window, const ads::CDockManager& docks);

void RestoreLayout(QMainWindow& window, ads::CDockManager& docks);

}
