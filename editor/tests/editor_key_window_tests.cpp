#include <catch2/catch_test_macros.hpp>

#include "editor_graph.h"
#include "editor_timeline.h"
#include "editor_viewport.h"
#include "editor_window.h"

#include "document/key_selection.h"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QPoint>
#include <QString>
#include <QTableWidget>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "window_test_support.h"

using namespace WindowTest;

TEST_CASE("Time-reversing an owned depth's keyframes plays its move backwards") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    {
        Script inserted({Choose("Insert a frame at 2")});
        emit timeline->MenuRequested(QPoint(4, 4), 2, QString());
        REQUIRE(Settle([&inserted] { return inserted.Finished(); }));
        CHECK(inserted.Problems().isEmpty());
    }
    OwnDroppedDot(opened);
    const auto move_on = [&](uint32_t frame, double dx) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        Script moved({});
        emit viewport->Dragged(3, dx, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    };
    const auto x_on = [&](uint32_t frame) {
        emit timeline->FrameChosen(frame);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    move_on(0, 10);
    move_on(1, 20);
    move_on(3, 40);
    CHECK(x_on(0) == 10200);
    CHECK(x_on(1) == 10600);
    CHECK(x_on(2) == 10600);
    CHECK(x_on(3) == 11400);

    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    emit timeline->FrameChosen(0);
    emit timeline->KeyChosen("Translation", 0);
    timeline->SelectKeys({ref(0), ref(1), ref(3)});
    {
        Script reversed({Choose("Time-reverse 3 keyframe(s)")});
        emit timeline->KeyMenuRequested(QPoint(4, 4), "Translation", 0, true);
        REQUIRE(Settle([&reversed] { return reversed.Finished(); }));
        CHECK(reversed.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Keyframe") == "Translation on frame 3");
    std::vector<Document::KeyRef> selected = timeline->SelectedKeys();
    std::ranges::sort(selected);
    CHECK(selected == std::vector<Document::KeyRef>{ref(0), ref(2), ref(3)});
    CHECK(x_on(0) == 11400);
    CHECK(x_on(1) == 11400);
    CHECK(x_on(2) == 10600);
    CHECK(x_on(3) == 10200);
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(x_on(0) == 10200);
    CHECK(x_on(3) == 11400);
}

TEST_CASE("Easy ease smooths the selected keyframes from its keys and the lane menu") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    {
        Script inserted({Choose("Insert a frame at 2")});
        emit timeline->MenuRequested(QPoint(4, 4), 2, QString());
        REQUIRE(Settle([&inserted] { return inserted.Finished(); }));
        CHECK(inserted.Problems().isEmpty());
    }
    OwnDroppedDot(opened);
    const auto move_on = [&](uint32_t frame, double dx) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        Script moved({});
        emit viewport->Dragged(3, dx, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    };
    const auto x_on = [&](uint32_t frame) {
        emit timeline->FrameChosen(frame);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    const auto refusal = [](QAction* action) {
        Script refused({});
        action->trigger();
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        return refused.Problems().front();
    };
    move_on(0, 10);
    move_on(3, 40);
    CHECK(x_on(1) == 10200);
    CHECK(x_on(2) == 10200);

    QAction* both = ShortcutAction(opened.window, QKeySequence(Qt::Key_F9));
    QAction* in = ShortcutAction(opened.window, QKeySequence(Qt::SHIFT | Qt::Key_F9));
    QAction* out = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9));
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(both != nullptr);
    REQUIRE(in != nullptr);
    REQUIRE(out != nullptr);
    REQUIRE(undo != nullptr);
    timeline->SelectKeys({ref(0), ref(3)});
    {
        Script eased({});
        both->trigger();
        QApplication::processEvents();
        CHECK(eased.Problems().isEmpty());
    }
    CHECK(timeline->SelectedKeys().size() == 2);
    emit timeline->FrameChosen(1);
    CHECK(timeline->SelectedKeys().size() == 2);
    const int first = x_on(1);
    const int second = x_on(2);
    CHECK(first > 10200);
    CHECK(first < 10467);
    CHECK(second > 10733);
    CHECK(second < 11000);
    undo->trigger();
    CHECK(x_on(1) == 10200);

    timeline->SelectKeys({ref(0), ref(3)});
    {
        Script linear({Choose("How it leaves frame 0"), Choose("Linear")});
        emit timeline->KeyMenuRequested(QPoint(4, 4), "Translation", 0, true);
        REQUIRE(Settle([&linear] { return linear.Finished(); }));
        CHECK(linear.Problems().isEmpty());
    }
    CHECK(timeline->SelectedKeys().size() == 2);
    CHECK(x_on(1) == 10467);
    undo->trigger();

    timeline->SelectKeys({ref(3)});
    {
        Script eased({Choose("Easy ease"), Choose("In")});
        emit timeline->KeyMenuRequested(QPoint(4, 4), "Translation", 3, true);
        REQUIRE(Settle([&eased] { return eased.Finished(); }));
        CHECK(eased.Problems().isEmpty());
    }
    CHECK(x_on(1) > 10467);
    const QString out_refused = refusal(out);
    INFO(out_refused.toStdString());
    CHECK(out_refused.contains("neighbour"));
    timeline->SelectKeys({ref(0)});
    CHECK(refusal(in).contains("neighbour"));
}

TEST_CASE("The graph shows the focused property and edits its keyframes") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    Editor::GraphEditor* graph = nullptr;
    for (QWidget* widget : QApplication::allWidgets()) {
        if (auto* found = qobject_cast<Editor::GraphEditor*>(widget)) graph = found;
    }
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(graph != nullptr);
    OwnDroppedDot(opened);
    const auto move_on = [&](uint32_t frame, double dx) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        Script moved({});
        emit viewport->Dragged(3, dx, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    };
    move_on(0, 10);
    move_on(2, 40);
    emit timeline->KeyChosen("Translation", 2);
    REQUIRE(graph->KeyPoint(2, 0).has_value());
    CHECK(graph->KeyPoint(0, 1).has_value());
    CHECK_FALSE(graph->KeyPoint(1, 0).has_value());

    {
        Script edited({});
        emit graph->KeyMoved("Translation", 2, 2, {12345, 6000});
        QApplication::processEvents();
        CHECK(edited.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Translation") == "12345, 6000");
    emit graph->KeyChosen("Translation", 0);
    CHECK(RowValue(*opened.inspector, "Keyframe") == "Translation on frame 0");
    emit graph->FrameChosen(2);
    CHECK(RowValue(*opened.inspector, "Translation") == "12345, 6000");
    emit graph->FrameChosen(0);
    CHECK(RowValue(*opened.inspector, "Translation") == "10200, 6000");
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    emit timeline->FrameChosen(2);
    CHECK(RowValue(*opened.inspector, "Translation") == "11000, 6000");

    {
        Script retimed({});
        emit graph->KeyMoved("Translation", 2, 1, {11000, 6000});
        QApplication::processEvents();
        CHECK(retimed.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Keyframe") == "Translation on frame 1");
    emit timeline->FrameChosen(1);
    CHECK(RowValue(*opened.inspector, "Translation") == "11000, 6000");
    {
        Script refused({});
        emit graph->KeyMoved("Translation", 1, 0, {11000, 6000});
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    }
    emit timeline->FrameChosen(0);
    CHECK(RowValue(*opened.inspector, "Translation") == "10200, 6000");

    emit timeline->DepthChosen(2);
    CHECK_FALSE(graph->KeyPoint(0, 0).has_value());
}

TEST_CASE("Toggling hold switches the selected keyframes between holding and linear") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    {
        Script inserted({Choose("Insert a frame at 2")});
        emit timeline->MenuRequested(QPoint(4, 4), 2, QString());
        REQUIRE(Settle([&inserted] { return inserted.Finished(); }));
    }
    OwnDroppedDot(opened);
    const auto move_on = [&](uint32_t frame, double dx) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        Script moved({});
        emit viewport->Dragged(3, dx, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    };
    const auto x_on = [&](uint32_t frame) {
        emit timeline->FrameChosen(frame);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    move_on(0, 10);
    move_on(3, 40);
    CHECK(x_on(1) == 10200);
    QAction* toggle = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
    REQUIRE(toggle != nullptr);
    timeline->SelectKeys({ref(0), ref(3)});
    toggle->trigger();
    const int linear = x_on(1);
    CHECK(linear > 10200);
    CHECK(linear < 11000);
    toggle->trigger();
    CHECK(x_on(1) == 10200);
    {
        Script toggled({Choose("Toggle hold")});
        emit timeline->KeyMenuRequested(QPoint(4, 4), "Translation", 0, true);
        REQUIRE(Settle([&toggled] { return toggled.Finished(); }));
        CHECK(toggled.Problems().isEmpty());
    }
    CHECK(x_on(1) == linear);
}
