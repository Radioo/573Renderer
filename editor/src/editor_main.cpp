#include "editor_window.h"

#include <QApplication>
#include <QStringList>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("IFS Editor");
    QApplication::setOrganizationName("573Renderer");
    Editor::Window window;
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) window.OpenDocument(arguments.at(1));
    window.show();
    return QApplication::exec();
}
