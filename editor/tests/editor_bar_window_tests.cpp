#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "editor_timeline.h"
#include "editor_timeline_metrics.h"
#include "editor_viewport.h"
#include "editor_window.h"

#include <QAction>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QPointF>
#include <QMouseEvent>
#include <QEvent>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QPoint>
#include <QSize>
#include <QSlider>
#include <QString>
#include <QToolButton>

#include <cmath>
#include <vector>

#include "window_test_support.h"

using namespace WindowTest;

TEST_CASE("The tool strip chooses one tool at a time and the stage follows it") {
    Opened opened;
    Open(opened);
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(viewport != nullptr);
    const auto button = [&](const QString& id) {
        auto* found = opened.window.findChild<QToolButton*>("strip_" + id);
        REQUIRE(found != nullptr);
        return found;
    };
    CHECK(viewport->CurrentTool() == Editor::Tool::Select);
    CHECK(button("tool.select")->isChecked());

    button("tool.pan")->click();
    QApplication::processEvents();
    CHECK(viewport->CurrentTool() == Editor::Tool::Pan);
    CHECK(button("tool.pan")->isChecked());
    CHECK_FALSE(button("tool.select")->isChecked());

    RunCommand(opened.window, "tool.zoom");
    CHECK(viewport->CurrentTool() == Editor::Tool::Zoom);
    CHECK(button("tool.zoom")->isChecked());
    CHECK_FALSE(button("tool.pan")->isChecked());

    QAction* sketch = ShortcutAction(opened.window, QKeySequence(Qt::Key_Y));
    REQUIRE(sketch != nullptr);
    sketch->trigger();
    CHECK(viewport->CurrentTool() == Editor::Tool::Sketch);
    CHECK(button("tool.sketch")->isChecked());

    RunCommand(opened.window, "tool.sketch");
    CHECK(viewport->CurrentTool() == Editor::Tool::Sketch);
    CHECK(button("tool.sketch")->isChecked());
}

TEST_CASE("The stage bar zoom control steps the stage and Fit puts it back") {
    Opened opened;
    Open(opened);
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(viewport != nullptr);
    const auto button = [&](const QString& name) {
        auto* found = opened.window.findChild<QToolButton*>(name);
        REQUIRE(found != nullptr);
        return found;
    };
    auto* shown = opened.window.findChild<QToolButton*>("stage_zoom");
    REQUIRE(shown != nullptr);
    viewport->resize(960, 540);
    QImage frame(1920, 1080, QImage::Format_ARGB32);
    frame.fill(QColor(0, 0, 0));
    viewport->ShowFrame(frame, QSize(1920, 1080));
    const double plain = viewport->StageScale();
    REQUIRE(plain > 0);
    const auto percent = [&viewport] {
        return QString("%1%").arg(std::lround(viewport->StageScale() * 100));
    };

    REQUIRE(RunCommand(opened.window, "view.zoom_in").isEmpty());
    QApplication::processEvents();
    CHECK(viewport->StageScale() > plain);
    CHECK(shown->text() == percent());

    REQUIRE(RunCommand(opened.window, "view.zoom_out").isEmpty());
    QApplication::processEvents();
    CHECK_THAT(viewport->StageScale(), Catch::Matchers::WithinAbs(plain, 1e-9));

    REQUIRE(RunCommand(opened.window, "view.zoom_in").isEmpty());
    button("stage_view.fit_stage")->click();
    QApplication::processEvents();
    CHECK_THAT(viewport->StageScale(), Catch::Matchers::WithinAbs(plain, 1e-9));
    CHECK(shown->text() == percent());
}

TEST_CASE("The scripts lane's camera button puts a camera on the playhead and takes it away") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const QImage plain = Picture(*timeline);
    {
        Script added({AnswerNumber(0)});
        emit timeline->CameraAsked();
        REQUIRE(Settle([&added] { return added.Finished(); }));
        CHECK(added.Problems().isEmpty());
    }
    WaitForOpen(opened.window);
    CHECK(Picture(*timeline) != plain);
    {
        Script removed({});
        emit timeline->CameraAsked();
        REQUIRE(Settle([&removed] { return removed.Problems().isEmpty(); }));
    }
    WaitForOpen(opened.window);
    CHECK(Picture(*timeline) == plain);
}

TEST_CASE("The gutter's column headers show every hidden depth and unlock every locked one") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const auto offers = [&](const QString& text) {
        emit timeline->DepthChosen(1);
        bool found = false;
        Script looked({Look(text, found)});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
        return found;
    };
    emit timeline->VisibilityToggled(1);
    emit timeline->LockToggled(1);
    QApplication::processEvents();
    CHECK(offers("Show depth 1 in the view"));
    CHECK(offers("Unlock depth 1 on stage"));

    emit timeline->ShowAllAsked();
    QApplication::processEvents();
    CHECK(offers("Hide depth 1 in the view"));
    CHECK(offers("Unlock depth 1 on stage"));

    emit timeline->UnlockAllAsked();
    QApplication::processEvents();
    CHECK(offers("Lock depth 1 on stage"));
    CHECK(RefusalOf(opened.window, "depth.unlock_all") == "No depth is locked");
}

TEST_CASE("The timeline zoom slider shows the fit and cannot zoom out past it") {
    Opened opened;
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    WidenTimeline(*timeline);
    Open(opened);
    auto* slider = opened.window.findChild<QSlider*>("timeline_zoom");
    REQUIRE(slider != nullptr);
    RunCommand(opened.window, "view.timeline_out");
    QApplication::processEvents();
    const auto shown = [&timeline] {
        return static_cast<int>(std::lround(timeline->ZoomPixels()));
    };
    const int fitted = shown();
    CHECK(slider->value() == fitted);

    slider->setValue(slider->minimum());
    QApplication::processEvents();
    CHECK(shown() == fitted);
    CHECK(slider->value() == fitted);
    CHECK(timeline->minimumWidth() == 0);

    RunCommand(opened.window, "view.timeline_in");
    QApplication::processEvents();
    CHECK(slider->value() == shown());

    RunCommand(opened.window, "view.timeline_out");
    QApplication::processEvents();
    CHECK(slider->value() == shown());
}

TEST_CASE("Add a depth places a picked character from the playhead to the end") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    const QImage before = Picture(*timeline);
    {
        Script placed({AcceptInput()});
        RunCommand(opened.window, "depth.add");
        REQUIRE(Settle([&placed] { return placed.Finished(); }));
        INFO(placed.Problems().join("|").toStdString());
        CHECK(placed.Problems().isEmpty());
    }
    CHECK(Picture(*timeline) != before);
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(Picture(*timeline) == before);
}

TEST_CASE("A sprite opens by double-clicking its bar, and Escape leaves it") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    CHECK(RefusalOf(opened.window, "clip.leave") == "The root is already open");
    WidenTimeline(*timeline);

    emit timeline->DepthChosen(1);
    {
        Script grouped({Choose("Group depth 1 and up here into a sprite...")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&grouped] { return grouped.Finished(); }));
        PressPopover(opened.window, "apply");
        CHECK(grouped.Problems().isEmpty());
    }
    CHECK(OpenClip(opened.window).isEmpty());

    const QPointF on_bar(Editor::kGutterWidth + 4, Editor::kRowsTop + 8);
    QMouseEvent entered(QEvent::MouseButtonDblClick, on_bar, timeline->mapToGlobal(on_bar),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(timeline, &entered);
    QApplication::processEvents();
    const QString sprite = OpenClip(opened.window);
    CHECK_FALSE(sprite.isEmpty());

    REQUIRE(RunCommand(opened.window, "clip.leave").isEmpty());
    QApplication::processEvents();
    CHECK(OpenClip(opened.window).isEmpty());

    CHECK(sprite.contains("Sprite"));
}

TEST_CASE("A drag inside a clip shown in place is mapped through the clip's own scale") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    emit timeline->DepthChosen(2);
    {
        Script grouped({Choose("Group depth 2 and up here into a sprite...")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&grouped] { return grouped.Finished(); }));
        PressPopover(opened.window, "apply");
        CHECK(grouped.Problems().isEmpty());
    }
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    {
        Script scaled({});
        emit viewport->Reshaped(2, 2.0, 2.0, 0, true);
        QApplication::processEvents();
        CHECK(scaled.Problems().isEmpty());
    }
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(RowValue(*opened.inspector, "Scale") == "2048, 2048");
    const QString placed = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
    const double clip_x = placed.section(',', 0, 0).trimmed().toDouble() / 20;
    const double clip_y = placed.section(',', 1, 1).trimmed().toDouble() / 20;

    REQUIRE(EnterFirstSprite(opened.window));
    WaitForOpen(opened.window);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(2);
    WaitForOpen(opened.window);
    const QString before = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
    {
        Script dragged({});
        emit viewport->Dragged(2, 20, 0, true);
        QApplication::processEvents();
        CHECK(dragged.Problems().isEmpty());
    }
    emit timeline->DepthChosen(2);
    WaitForOpen(opened.window);
    const QString after = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
    CHECK(after != before);
    const double inside_x = after.section(',', 0, 0).trimmed().toDouble() / 20;
    const double inside_y = after.section(',', 1, 1).trimmed().toDouble() / 20;
    emit timeline->DepthChosen(1);
    WaitForOpen(opened.window);
    emit viewport->Picked(clip_x + (2 * inside_x), clip_y + (2 * inside_y));
    WaitForOpen(opened.window);
    CHECK(RowValue(*opened.inspector, "Depth") == "2");
    const double moved = after.section(',', 0, 0).trimmed().toDouble() -
                         before.section(',', 0, 0).trimmed().toDouble();
    CHECK_THAT(moved, Catch::Matchers::WithinAbs(200, 1));
}
