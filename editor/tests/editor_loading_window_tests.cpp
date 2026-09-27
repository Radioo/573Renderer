#include <catch2/catch_test_macros.hpp>

#include "editor_timeline.h"
#include "editor_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStringList>
#include <QString>
#include <QTreeWidget>
#include <QWidget>

#include "window_test_support.h"

using namespace WindowTest;

namespace {

constexpr qint64 kAnswerMs = 250;
constexpr int kRecentRowHeight = 56;
constexpr int kRecentRoom = 6;

QWidget* Page(Editor::Window& window) {
    auto* centre = window.findChild<QStackedWidget*>("centre_stack");
    REQUIRE(centre != nullptr);
    return centre->currentWidget();
}

QString EditsText(Editor::Window& window) {
    auto* label = window.findChild<QLabel*>("document_edits");
    REQUIRE(label != nullptr);
    return label->text();
}

QString BusyText(Editor::Window& window) {
    auto* what = window.findChild<QLabel*>("busy_what");
    REQUIRE(what != nullptr);
    return what->text();
}

}

TEST_CASE("Opening a package says what it is doing before any panel is filled") {
    Opened opened;
    const QString path = WritePackage(opened.dir, true);
    auto* animations = opened.window.findChild<QTreeWidget*>("package_animations");
    REQUIRE(animations != nullptr);

    opened.window.OpenDocument(path);
    CHECK(opened.window.Loading());
    CHECK(Page(opened.window)->objectName() == QString("busy"));
    CHECK(BusyText(opened.window).contains("sample.ifs"));
    CHECK(animations->topLevelItemCount() == 0);

    WaitForOpen(opened.window);
    CHECK_FALSE(opened.window.Loading());
    CHECK(Page(opened.window)->objectName() != QString("busy"));
    CHECK(animations->topLevelItemCount() > 0);
}

TEST_CASE("The window keeps answering while a package is opening") {
    Opened opened;
    const QString path = WritePackage(opened.dir, true);
    opened.window.OpenDocument(path);
    REQUIRE(opened.window.Loading());

    QElapsedTimer answered;
    answered.start();
    QApplication::processEvents();
    CHECK(answered.elapsed() < kAnswerMs);
    REQUIRE(RunCommand(opened.window, "view.snap").isEmpty());
    CHECK(answered.elapsed() < kAnswerMs);
    WaitForOpen(opened.window);
    REQUIRE(RunCommand(opened.window, "view.snap").isEmpty());
}

TEST_CASE("Saving says it is saving and comes back to the panels") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    REQUIRE(RunCommand(opened.window, "depth.split").isEmpty());

    QAction* save = ShortcutAction(opened.window, QKeySequence(QKeySequence::Save));
    REQUIRE(save != nullptr);
    save->trigger();
    CHECK(opened.window.Loading());
    CHECK(Page(opened.window)->objectName() == QString("busy"));
    CHECK(BusyText(opened.window).contains("sample.ifs"));

    WaitForOpen(opened.window);
    CHECK_FALSE(opened.window.Loading());
    CHECK(Page(opened.window)->objectName() != QString("busy"));
    CHECK(EditsText(opened.window) == QString("saved"));
}

TEST_CASE("A new animation refreshes the package rows without blocking the window") {
    Opened opened;
    Open(opened, true);
    auto* animations = opened.window.findChild<QTreeWidget*>("package_animations");
    REQUIRE(animations != nullptr);
    const int before = animations->topLevelItemCount();
    {
        Script added({Choose("New animation..."), Answer("second"), AnswerNumber(4)});
        RunMenu(added, *opened.tree);
        CHECK(added.Problems().isEmpty());
    }
    QElapsedTimer answered;
    answered.start();
    QApplication::processEvents();
    CHECK(answered.elapsed() < kAnswerMs);
    WaitForOpen(opened.window);
    CHECK(animations->topLevelItemCount() == before + 1);
}

TEST_CASE("A recent row is as tall as the design says and its words clear the rule") {
    QSettings().setValue("recent/files", QStringList{QCoreApplication::applicationFilePath()});
    Opened opened;
    ShowOffScreen(opened.window);
    QSettings().remove("recent/files");
    auto* start = opened.window.findChild<QWidget*>("start_screen");
    REQUIRE(start != nullptr);
    const QList<QPushButton*> rows = start->findChildren<QPushButton*>("recent_row");
    REQUIRE(rows.size() == 1);
    QPushButton* row = rows.front();
    CHECK(row->height() == kRecentRowHeight);
    auto* name = row->findChild<QLabel*>("recent_name");
    auto* detail = row->findChild<QLabel*>("recent_detail");
    REQUIRE(name != nullptr);
    REQUIRE(detail != nullptr);
    CHECK(name->geometry().top() >= kRecentRoom);
    CHECK(row->height() - detail->geometry().bottom() >= kRecentRoom);
}
