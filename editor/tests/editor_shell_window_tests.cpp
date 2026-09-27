#include <catch2/catch_test_macros.hpp>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include "editor_hover.h"
#include "editor_layout.h"
#include "editor_timeline.h"
#include "editor_window.h"

#include <QApplication>
#include <QPointF>
#include <QUrl>
#include <QStackedWidget>
#include <QSettings>
#include <QByteArray>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QComboBox>
#include <QLabel>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QPushButton>
#include <QSpinBox>
#include <QToolBar>
#include <QTreeWidget>
#include <QToolButton>
#include <QAction>
#include <QWidget>

#include <cstdint>

#include "window_test_support.h"

using namespace WindowTest;

namespace {

constexpr int kLeastPanelWidth = 220;
constexpr int kLeastTimelineHeight = 200;

QRect Placed(QWidget& window, QWidget* widget) {
    return QRect(widget->mapTo(&window, QPoint(0, 0)), widget->size());
}

bool OnTopBar(QWidget& window, const QToolButton* button) {
    auto* bar = window.findChild<QToolBar*>("top_bar");
    REQUIRE(bar != nullptr);
    for (QAction* action : bar->actions()) {
        if (bar->widgetForAction(action) == button) return action->isVisible();
    }
    return false;
}

QString LabelText(QWidget& window, const QString& name) {
    auto* label = window.findChild<QLabel*>(name);
    REQUIRE(label != nullptr);
    return label->text();
}

}

TEST_CASE("The window puts the timeline under the package and the stage, with the inspector at "
          "full height beside them") {
    Opened opened;
    Editor::Window& window = opened.window;
    window.resize(1600, 1000);
    window.show();
    Open(opened);
    QApplication::processEvents();
    ads::CDockWidget* package = Panel(window, "Package");
    ads::CDockWidget* library = Panel(window, "Library");
    ads::CDockWidget* stage = Panel(window, "Stage");
    ads::CDockWidget* timeline = Panel(window, "Timeline");
    ads::CDockWidget* inspector = Panel(window, "Inspector");
    ads::CDockWidget* history = Panel(window, "History");
    REQUIRE(package != nullptr);
    REQUIRE(library != nullptr);
    REQUIRE(stage != nullptr);
    REQUIRE(timeline != nullptr);
    REQUIRE(inspector != nullptr);
    REQUIRE(history != nullptr);
    CHECK(history->dockAreaWidget() == inspector->dockAreaWidget());

    const QRect package_at = Placed(window, package->dockAreaWidget());
    const QRect library_at = Placed(window, library->dockAreaWidget());
    const QRect stage_at = Placed(window, stage->dockAreaWidget());
    const QRect timeline_at = Placed(window, timeline->dockAreaWidget());
    const QRect inspector_at = Placed(window, inspector->dockAreaWidget());
    CHECK(package_at.right() < stage_at.left());
    CHECK(library_at.top() > package_at.bottom());
    CHECK(library_at.right() < stage_at.left());
    CHECK(timeline_at.top() > stage_at.bottom());
    CHECK(timeline_at.top() > library_at.bottom());
    CHECK(timeline_at.left() <= package_at.left());
    CHECK(timeline_at.right() < inspector_at.left());
    CHECK(inspector_at.top() <= stage_at.top());
    CHECK(inspector_at.bottom() >= timeline_at.bottom());
}

TEST_CASE("The top bar names the file and counts the edits since it was saved") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    CHECK(LabelText(opened.window, "document_state") == QString("sample.ifs"));
    CHECK(LabelText(opened.window, "document_edits") == QString("saved"));
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    REQUIRE(RunCommand(opened.window, "depth.split").isEmpty());
    CHECK(LabelText(opened.window, "document_edits") == QString("unsaved, 1 edit"));
    REQUIRE(RunCommand(opened.window, "edit.undo").isEmpty());
    CHECK(LabelText(opened.window, "document_edits") == QString("saved"));
}

TEST_CASE("The top bar shows the project and how many entries wait for an export") {
    Opened opened;
    Open(opened, true);
    auto* project = opened.window.findChild<QToolButton*>("project");
    auto* exporting = opened.window.findChild<QToolButton*>("export");
    REQUIRE(project != nullptr);
    REQUIRE(exporting != nullptr);
    CHECK_FALSE(OnTopBar(opened.window, project));
    CHECK_FALSE(OnTopBar(opened.window, exporting));
    OwnDroppedDot(opened);
    CHECK(OnTopBar(opened.window, project));
    CHECK(project->text() == QString("Project project"));
    CHECK(OnTopBar(opened.window, exporting));
    CHECK(exporting->text() == QString("Export to IFS (2)"));
    REQUIRE(RunCommand(opened.window, "project.export").isEmpty());
    WaitForOpen(opened.window);
    CHECK(exporting->text() == QString("Export to IFS"));
}

TEST_CASE("The status bar says which frame is shown and what is chosen") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(2);
    CHECK(LabelText(opened.window, "frame_status") == QString("Frame 2 of 3"));
    CHECK(LabelText(opened.window, "chosen_status").isEmpty());
    emit timeline->DepthChosen(2);
    CHECK(LabelText(opened.window, "chosen_status") == QString("Depth 2 chosen"));
    emit timeline->DepthsChosen({1, 2});
    CHECK(LabelText(opened.window, "chosen_status") == QString("2 depths chosen"));
    CHECK(LabelText(opened.window, "host_status") == QString("not running"));
    CHECK(LabelText(opened.window, "snap_status") == QString("Snap on"));
    REQUIRE(RunCommand(opened.window, "view.snap").isEmpty());
    CHECK(LabelText(opened.window, "snap_status") == QString("Snap off"));
    REQUIRE(RunCommand(opened.window, "view.snap").isEmpty());
}

TEST_CASE("The timeline bar shows the frame, the time, the label and the work area") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* frame = opened.window.findChild<QSpinBox*>("timeline_frame");
    REQUIRE(timeline != nullptr);
    REQUIRE(frame != nullptr);
    emit timeline->FrameChosen(2);
    CHECK(frame->value() == 2);
    CHECK(LabelText(opened.window, "timeline_count") == QString("/ 2"));
    CHECK(LabelText(opened.window, "timeline_time") == QString("0.03 s"));
    CHECK(LabelText(opened.window, "timeline_work_area") == QString("none"));
    CHECK(LabelText(opened.window, "timeline_label") == QString("loop"));

    frame->setValue(0);
    emit frame->editingFinished();
    QApplication::processEvents();
    CHECK(LabelText(opened.window, "frame_status") == QString("Frame 0 of 3"));

    REQUIRE(RunCommand(opened.window, "clip.work_start").isEmpty());
    emit timeline->FrameChosen(2);
    REQUIRE(RunCommand(opened.window, "clip.work_end").isEmpty());
    CHECK(LabelText(opened.window, "timeline_work_area") == QString("0 to 2"));
    REQUIRE(RunCommand(opened.window, "clip.work_clear").isEmpty());
    CHECK(LabelText(opened.window, "timeline_work_area") == QString("none"));
}

TEST_CASE("The timeline bar switches the panel between the timeline and the graph") {
    Opened opened;
    Open(opened, true);
    auto* graph = opened.window.findChild<QToolButton*>("graph_mode");
    auto* back = opened.window.findChild<QToolButton*>("timeline_mode");
    REQUIRE(graph != nullptr);
    REQUIRE(back != nullptr);
    CHECK(Panel(opened.window, "Timeline")->isCurrentTab());
    graph->click();
    QApplication::processEvents();
    CHECK(Panel(opened.window, "Graph")->isCurrentTab());
    back->click();
    QApplication::processEvents();
    CHECK(Panel(opened.window, "Timeline")->isCurrentTab());
}

TEST_CASE("A refused command and an edit's side effect are said in a notice") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    CHECK(NoticeTexts(opened.window).isEmpty());

    CHECK_FALSE(CommandsOf(opened.window).Run("depth.split"));
    CHECK(LastNotice(opened.window) == QString("Choose a depth first"));
    auto* undo_on_refusal = opened.window.findChild<QPushButton*>("notice_undo");
    CHECK(undo_on_refusal == nullptr);
    auto* close = opened.window.findChild<QPushButton*>("notice_close");
    REQUIRE(close != nullptr);
    close->click();
    QApplication::processEvents();
    CHECK(NoticeTexts(opened.window).isEmpty());

    emit timeline->FrameChosen(2);
    emit timeline->DepthChosen(2);
    REQUIRE(RunCommand(opened.window, "depth.remove").isEmpty());
    opened.tree->setCurrentItem(AnimationNamed(*opened.tree, "intro"));
    {
        Script tidied({Choose("Remove unused definitions from intro")});
        RunMenu(tidied, *opened.tree);
        CHECK(tidied.Problems().isEmpty());
    }
    CHECK(LastNotice(opened.window).startsWith("Removed 1 unused definition"));
    auto* undo = opened.window.findChild<QPushButton*>("notice_undo");
    REQUIRE(undo != nullptr);
    const QString before = CommandsOf(opened.window).Action("edit.undo")->text();
    undo->click();
    QApplication::processEvents();
    CHECK(CommandsOf(opened.window).Action("edit.undo")->text() != before);
}

TEST_CASE("The stage bar names the clip on screen and its toggles follow the commands") {
    Opened opened;
    Open(opened, true);
    const auto crumbs = [&opened] {
        QStringList said;
        auto* bar = opened.window.findChild<QWidget*>("stage_bar");
        REQUIRE(bar != nullptr);
        for (const QToolButton* crumb : bar->findChildren<QToolButton*>()) {
            if (crumb->objectName().startsWith("crumb_")) said.append(crumb->text());
        }
        return said;
    };
    CHECK(crumbs() == QStringList{"intro", "Root"});

    auto* snap = opened.window.findChild<QToolButton*>("stage_view.snap");
    REQUIRE(snap != nullptr);
    CHECK(snap->isChecked());
    REQUIRE(RunCommand(opened.window, "view.snap").isEmpty());
    CHECK_FALSE(snap->isChecked());
    snap->click();
    QApplication::processEvents();
    CHECK(snap->isChecked());
    CHECK(CommandsOf(opened.window).Action("view.snap")->isChecked());
}

TEST_CASE("The start screen lists recent files until one is open, and a drop opens an IFS") {
    QSettings().remove("recent/files");
    Opened opened;
    auto* start = opened.window.findChild<QWidget*>("start_screen");
    auto* centre = opened.window.findChild<QStackedWidget*>("centre_stack");
    auto* install = opened.window.findChild<QLabel*>("start_install");
    REQUIRE(start != nullptr);
    REQUIRE(centre != nullptr);
    REQUIRE(install != nullptr);
    const auto rows = [&start] { return start->findChildren<QPushButton*>("recent_row"); };
    CHECK(centre->currentWidget() == start);
    CHECK(rows().empty());
    REQUIRE(start->findChild<QLabel*>("recent_empty") != nullptr);
    CHECK(start->findChild<QLabel*>("recent_empty")->text() == "Nothing opened yet");
    CHECK(install->text().contains("None chosen"));

    const QString path = WritePackage(opened.dir);
    QMimeData data;
    data.setUrls({QUrl::fromLocalFile(path)});
    QDragEnterEvent entered(QPoint(4, 4), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&opened.window, &entered);
    CHECK(entered.isAccepted());
    QDropEvent drop(QPointF(4, 4), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&opened.window, &drop);
    WaitForOpen(opened.window);

    CHECK(centre->currentWidget() != start);
    auto* tree = opened.window.findChild<QTreeWidget*>("package");
    REQUIRE(tree != nullptr);
    CHECK(AnimationNamed(*tree, "intro") != nullptr);
    REQUIRE(rows().size() == 1);
    CHECK(rows().at(0)->property("path").toString() == path);
    CHECK(rows().at(0)->findChild<QLabel*>("recent_name")->text() == "sample.ifs");
    CHECK(rows().at(0)->findChild<QLabel*>("recent_detail")->text().contains("1 animation"));
    QSettings().remove("recent/files");
}

TEST_CASE("Hovering the command search lifts it without clashing with the shortcut chip") {
    Opened opened;
    ShowOffScreen(opened.window);
    auto* field = opened.window.findChild<QPushButton*>("search");
    auto* chip = opened.window.findChild<QLabel*>("search_chip");
    auto* glass = opened.window.findChild<QLabel*>("search_glass");
    REQUIRE(field != nullptr);
    REQUIRE(chip != nullptr);
    REQUIRE(glass != nullptr);

    const QImage quiet = field->grab().toImage();
    const QImage quiet_glass = glass->grab().toImage();
    Editor::Hover(*field);
    const QImage lifted = field->grab().toImage();
    CHECK(lifted != quiet);

    CHECK(glass->grab().toImage() != quiet_glass);

    const QPoint chip_at = chip->mapTo(field, QPoint(0, 0));
    const QRgb edge = lifted.pixel(0, lifted.height() / 2);
    const QRgb chip_edge = lifted.pixel(chip_at.x(), chip_at.y() + chip->height() / 2);
    CHECK(QColor(edge).name().toStdString() == QColor(chip_edge).name().toStdString());

    Editor::Unhover(*field);
    CHECK(field->grab().toImage() == quiet);
    CHECK(glass->grab().toImage() == quiet_glass);
}

TEST_CASE("A layout saved before a package was opened does not shrink the panels next time") {
    QSettings().remove("window/docks");
    {
        Editor::Window closed;
        auto* docks = closed.findChild<ads::CDockManager*>();
        REQUIRE(docks != nullptr);
        Editor::SaveLayout(closed, *docks);
    }
    CHECK_FALSE(QSettings().contains("window/docks"));

    Opened opened;
    ShowOffScreen(opened.window);
    Open(opened);
    auto* docks = opened.window.findChild<ads::CDockManager*>();
    REQUIRE(docks != nullptr);
    const auto room = [docks](const QString& name) {
        ads::CDockWidget* panel = docks->findDockWidget(name);
        REQUIRE(panel != nullptr);
        REQUIRE(panel->dockAreaWidget() != nullptr);
        return panel->dockAreaWidget()->size();
    };
    CHECK(room("Package").width() >= kLeastPanelWidth);
    CHECK(room("Library").width() >= kLeastPanelWidth);
    CHECK(room("Inspector").width() >= kLeastPanelWidth);
    CHECK(room("Timeline").height() >= kLeastTimelineHeight);
    QSettings().remove("window/docks");
}

TEST_CASE("A stored layout whose splitters are all zero is not put back") {
    const QByteArray dead =
        R"(<QtAdvancedDockingSystem><Sizes>0 0 </Sizes><Sizes>0 0 </Sizes></QtAdvancedDockingSystem>)";
    const QByteArray alive =
        R"(<QtAdvancedDockingSystem><Sizes>290 1280 </Sizes><Sizes>0 0 </Sizes></QtAdvancedDockingSystem>)";
    const QByteArray sized =
        R"(<QtAdvancedDockingSystem><Sizes>290 1280 </Sizes><Sizes>592 340 </Sizes></QtAdvancedDockingSystem>)";
    CHECK_FALSE(Editor::LayoutSized(QByteArray()));
    CHECK_FALSE(Editor::LayoutSized(dead));
    CHECK_FALSE(Editor::LayoutSized(alive));
    CHECK(Editor::LayoutSized(sized));
    CHECK(Editor::LayoutSized(qCompress(sized, 9)));
}
