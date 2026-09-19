#include "editor_layout.h"

#include <DockManager.h>

#include <QByteArray>
#include <QMainWindow>
#include <QSettings>
#include <QString>

namespace Editor {

namespace {

constexpr const char* kGeometryKey = "window/geometry";
constexpr const char* kStateKey = "window/state";
constexpr const char* kDocksKey = "window/docks";
constexpr int kDocksVersion = 1;

}

void SaveLayout(const QMainWindow& window, const ads::CDockManager& docks) {
    QSettings settings;
    settings.setValue(kGeometryKey, window.saveGeometry());
    settings.setValue(kStateKey, window.saveState());
    settings.setValue(kDocksKey, docks.saveState(kDocksVersion));
}

void RestoreLayout(QMainWindow& window, ads::CDockManager& docks) {
    const QSettings settings;
    const QByteArray geometry = settings.value(kGeometryKey).toByteArray();
    if (!geometry.isEmpty()) window.restoreGeometry(geometry);
    const QByteArray state = settings.value(kStateKey).toByteArray();
    if (!state.isEmpty()) window.restoreState(state);
    const QByteArray docked = settings.value(kDocksKey).toByteArray();
    if (!docked.isEmpty()) docks.restoreState(docked, kDocksVersion);
}

}
