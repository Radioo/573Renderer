#include "editor_layout.h"

#include <DockManager.h>

#include <QByteArray>
#include <QMainWindow>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>

namespace Editor {

namespace {

constexpr const char* kGeometryKey = "window/geometry";
constexpr const char* kStateKey = "window/state";
constexpr const char* kDocksKey = "window/docks";
constexpr int kDocksVersion = 5;

bool AllZero(const QString& sizes) {
    const QStringList numbers = sizes.split(' ', Qt::SkipEmptyParts);
    if (numbers.isEmpty()) return true;
    for (const QString& one : numbers) {
        if (one.toInt() > 0) return false;
    }
    return true;
}

}

bool LayoutSized(const QByteArray& state) {
    if (state.isEmpty()) return false;
    const QByteArray plain = qUncompress(state);
    QXmlStreamReader reading(plain.isEmpty() ? state : plain);
    int splitters = 0;
    while (!reading.atEnd()) {
        if (reading.readNext() != QXmlStreamReader::StartElement) continue;
        if (reading.name() != QLatin1String("Sizes")) continue;
        splitters++;
        if (AllZero(reading.readElementText())) return false;
    }
    return splitters > 0 && !reading.hasError();
}

void SaveLayout(const QMainWindow& window, const ads::CDockManager& docks) {
    QSettings settings;
    settings.setValue(kGeometryKey, window.saveGeometry());
    settings.setValue(kStateKey, window.saveState());
    if (!docks.isVisible()) return;
    settings.setValue(kDocksKey, docks.saveState(kDocksVersion));
}

void RestoreLayout(QMainWindow& window, ads::CDockManager& docks) {
    const QSettings settings;
    const QByteArray geometry = settings.value(kGeometryKey).toByteArray();
    if (!geometry.isEmpty()) window.restoreGeometry(geometry);
    const QByteArray state = settings.value(kStateKey).toByteArray();
    if (!state.isEmpty()) window.restoreState(state);
    const QByteArray docked = settings.value(kDocksKey).toByteArray();
    if (LayoutSized(docked)) docks.restoreState(docked, kDocksVersion);
}

}
