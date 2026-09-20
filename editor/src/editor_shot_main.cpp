#include "editor_window.h"

#include "editor_hover.h"
#include "editor_theme.h"
#include "editor_timeline.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QImage>
#include <QList>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QWidget>

namespace {

constexpr int kOffScreen = -32000;
constexpr qint64 kLongestWaitMs = 120000;

QSize SizeFrom(const QString& text) {
    const QStringList parts = text.split('x');
    if (parts.size() != 2) return {1600, 1000};
    return {parts[0].toInt(), parts[1].toInt()};
}

QString Taken(const QString& key, const QStringList& arguments, const QString& fallback) {
    const int at = arguments.indexOf(key);
    if (at < 0 || at + 1 >= arguments.size()) return fallback;
    return arguments[at + 1];
}

}

int main(int argc, char** argv) {
    QByteArray platform("windows");
    for (int at = 1; at + 1 < argc; at++) {
        if (QByteArray(argv[at]) == "--platform") platform = argv[at + 1];
    }
    qputenv("QT_QPA_PLATFORM", platform);
    QApplication::setOrganizationName("573Renderer.shot");
    QApplication::setApplicationName("IfsEditorShot");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    const QApplication app(argc, argv);
    Editor::Theme::Apply(const_cast<QApplication&>(app));
    const QStringList arguments = QApplication::arguments();
    if (!arguments.contains("--keep-settings")) QSettings().clear();

    const QSize size = SizeFrom(Taken("--size", arguments, "1600x1000"));
    const QString out = Taken("--out", arguments, "screenshots");
    const QString name = Taken("--name", arguments, "editor");
    const QString file = Taken("--ifs", arguments, QString());
    const QString game = Taken("--game", arguments, QString());
    if (!game.isEmpty()) QSettings().setValue("game/directory", game);
    QDir().mkpath(out);

    Editor::Window window;
    window.setWindowFlag(Qt::Tool);
    window.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    window.setAttribute(Qt::WA_ShowWithoutActivating);
    window.resize(size);
    window.move(kOffScreen, kOffScreen);
    window.show();
    window.move(kOffScreen, kOffScreen);
    for (int pass = 0; pass < 4; pass++)
        QApplication::processEvents();
    if (!file.isEmpty()) {
        window.OpenDocument(file);
        QElapsedTimer waited;
        waited.start();
        while (!arguments.contains("--busy") && window.Loading() &&
               waited.elapsed() < kLongestWaitMs)
            QApplication::processEvents();
    }
    const QString depth = Taken("--depth", arguments, QString());
    if (!depth.isEmpty()) {
        if (auto* timeline = window.findChild<Editor::Timeline*>()) {
            emit timeline->FrameChosen(Taken("--frame", arguments, "0").toUInt());
            emit timeline->DepthChosen(depth.toInt());
        }
        QApplication::processEvents();
    }
    const QString hovered = Taken("--hover", arguments, QString());
    if (!hovered.isEmpty()) {
        if (QWidget* under = window.findChild<QWidget*>(hovered)) Editor::Hover(*under);
    }
    const QString report = Taken("--report", arguments, QString());
    if (!report.isEmpty()) {
        QFile writing(QDir(out).filePath(report));
        if (writing.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream lines(&writing);
            for (QWidget* one : window.findChildren<QWidget*>()) {
                if (one->objectName().isEmpty()) continue;
                const QPoint at = one->mapTo(&window, QPoint(0, 0));
                lines << one->objectName() << ' ' << one->metaObject()->className() << ' ' << at.x()
                      << ',' << at.y() << ' ' << one->width() << 'x' << one->height() << " hint "
                      << one->sizeHint().width() << 'x' << one->sizeHint().height() << " font "
                      << one->font().pixelSize() << " shown " << (one->isVisible() ? 1 : 0)
                      << Qt::endl;
            }
        }
    }
    QImage shot;
    for (int pass = 0; pass < 8; pass++) {
        const QSize settled = window.size();
        QApplication::processEvents();
        shot = window.grab().toImage();
        if (window.size() == settled) break;
    }
    window.hide();
    const QString path = QDir(out).filePath(name + ".png");
    return shot.save(path, "PNG") ? 0 : 1;
}
