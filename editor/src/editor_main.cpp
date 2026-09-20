#include "editor_window.h"

#include "editor_theme.h"

#include <QApplication>
#include <QStringList>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("IFS Editor");
    QApplication::setOrganizationName("573Renderer");
    Editor::Theme::Apply(app);
    Editor::Window window;
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) window.OpenDocument(arguments.at(1));
    window.show();
    return QApplication::exec();
}
