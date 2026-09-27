#include <catch2/catch_test_macros.hpp>

#include "editor_graph.h"
#include "editor_ease_editor.h"
#include "editor_timeline.h"
#include "editor_viewport.h"
#include "editor_window.h"

#include "document/key_selection.h"

#include <QAction>
#include <QListWidgetItem>
#include <QListWidget>
#include <QToolButton>
#include <QLabel>
#include <QApplication>
#include <QKeySequence>
#include <QPoint>
#include <QStatusBar>
#include <QString>
#include <QTableWidget>
#include <QWidget>

#include <algorithm>
#include <cstdlib>
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

TEST_CASE("Cut and paste move keyframes, and paste takes whatever was copied last") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
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
    QAction* copy = ShortcutAction(opened.window, QKeySequence(QKeySequence::Copy));
    QAction* cut = ShortcutAction(opened.window, QKeySequence(QKeySequence::Cut));
    QAction* paste = ShortcutAction(opened.window, QKeySequence(QKeySequence::Paste));
    REQUIRE(copy != nullptr);
    REQUIRE(cut != nullptr);
    REQUIRE(paste != nullptr);
    move_on(0, 10);
    move_on(2, 40);
    CHECK(x_on(2) == 11000);
    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 2}});
    cut->trigger();
    CHECK(x_on(2) == 10200);
    emit timeline->FrameChosen(1);
    paste->trigger();
    CHECK(x_on(1) == 11000);
    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 0}});
    copy->trigger();
    emit timeline->FrameChosen(2);
    paste->trigger();
    CHECK(x_on(2) == 10200);
    CHECK(x_on(1) == 11000);

    timeline->SelectKeys({});
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    copy->trigger();
    {
        Script pasted({AcceptInput()});
        paste->trigger();
        REQUIRE(Settle([&pasted] { return pasted.Finished(); }));
        CHECK(pasted.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Depth") == "4");
}

TEST_CASE("Time-stretching an owned depth's keyframes spreads them from the first") {
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
    const int first = x_on(0);
    const int second = x_on(1);
    const int last = x_on(3);
    REQUIRE(first != second);

    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    const auto stretch = [&](double percent, bool apply) {
        emit timeline->FrameChosen(1);
        emit timeline->KeyChosen("Translation", 1);
        timeline->SelectKeys({ref(0), ref(1)});
        REQUIRE(RunCommand(opened.window, "key.stretch").isEmpty());
        SetPopoverValue(opened.window, "Stretch to", percent);
        Script run({});
        PressPopover(opened.window, apply ? "apply" : "cancel");
        QApplication::processEvents();
        return run.Problems();
    };
    CHECK(stretch(200, false).isEmpty());
    CHECK(x_on(1) == second);
    CHECK(stretch(200, true).isEmpty());
    CHECK(RowValue(*opened.inspector, "Keyframe") == "Translation on frame 2");
    std::vector<Document::KeyRef> selected = timeline->SelectedKeys();
    std::ranges::sort(selected);
    CHECK(selected == std::vector<Document::KeyRef>{ref(0), ref(2)});
    CHECK(x_on(0) == first);
    CHECK(x_on(2) == second);
    CHECK(x_on(3) == last);
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(x_on(1) == second);

    const QStringList refused = stretch(400, true);
    REQUIRE_FALSE(refused.isEmpty());
    CHECK(refused.front().contains("outside"));
    CHECK(x_on(1) == second);

    emit timeline->FrameChosen(1);
    timeline->SelectKeys({ref(0), ref(1)});
    emit timeline->KeysStretched(Document::KeyStretch{.anchor = 0, .scale = 2, .over = 1});
    CHECK(x_on(2) == second);
    CHECK(x_on(1) == first);
}

TEST_CASE("Simplifying an owned depth's keyframes keeps the ends of a straight run") {
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
    move_on(2, 20);
    move_on(3, 20);
    const std::vector<int> before{x_on(0), x_on(1), x_on(2), x_on(3)};
    REQUIRE(before[1] - before[0] == before[2] - before[1]);
    REQUIRE(before[3] - before[2] == before[2] - before[1]);

    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    const auto choose = [&](std::vector<Document::KeyRef> keys) {
        emit timeline->FrameChosen(1);
        emit timeline->KeyChosen("Translation", 1);
        timeline->SelectKeys(std::move(keys));
    };
    choose({ref(0), ref(1)});
    CHECK(RefusalOf(opened.window, "key.simplify").contains("three or more"));
    choose({ref(0), ref(1), ref(2), ref(3)});
    REQUIRE(RunCommand(opened.window, "key.simplify").isEmpty());
    SetPopoverValue(opened.window, "Largest change allowed", 0);
    CHECK(PopoverDetail(opened.window) == "2 of 4 keyframes go");
    PressPopover(opened.window, "cancel");
    CHECK(timeline->SelectedKeys().size() == 4);
    choose({ref(0), ref(1), ref(2), ref(3)});
    REQUIRE(RunCommand(opened.window, "key.simplify").isEmpty());
    SetPopoverValue(opened.window, "Largest change allowed", 0);
    PressPopover(opened.window, "apply");
    std::vector<Document::KeyRef> selected = timeline->SelectedKeys();
    std::ranges::sort(selected);
    CHECK(selected == std::vector<Document::KeyRef>{ref(0), ref(3)});
    CHECK(LastNotice(opened.window).contains("2 keyframe(s) removed"));
    CHECK(RowValue(*opened.inspector, "Keyframe") != "Translation on frame 1");
    CHECK(std::vector<int>{x_on(0), x_on(1), x_on(2), x_on(3)} == before);
}

TEST_CASE("Wiggling an owned depth's keyframes adds jittered keyframes between them") {
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
    move_on(3, 20);
    const std::vector<int> before{x_on(0), x_on(1), x_on(2), x_on(3)};

    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    const auto ask = [&] {
        emit timeline->FrameChosen(0);
        emit timeline->KeyChosen("Translation", 0);
        timeline->SelectKeys({ref(0), ref(3)});
        REQUIRE(RunCommand(opened.window, "key.wiggle").isEmpty());
        CHECK(PopoverOf(opened.window).isVisible());
        SetPopoverValue(opened.window, "A keyframe every", 1);
        SetPopoverValue(opened.window, "Largest change", 20);
    };
    ask();
    PressPopover(opened.window, "cancel");
    CHECK_FALSE(PopoverOf(opened.window).isVisible());
    CHECK(std::vector<int>{x_on(0), x_on(1), x_on(2), x_on(3)} == before);
    ask();
    PressPopover(opened.window, "apply");
    std::vector<Document::KeyRef> selected = timeline->SelectedKeys();
    std::ranges::sort(selected);
    CHECK(selected == std::vector<Document::KeyRef>{ref(0), ref(1), ref(2), ref(3)});
    CHECK(x_on(0) == before[0]);
    CHECK(x_on(3) == before[3]);
    for (const uint32_t frame : {1U, 2U}) {
        INFO(frame);
        CHECK(std::abs(x_on(frame) - before[frame]) <= 20);
    }
}

TEST_CASE("A motion sketch records the drag on every frame played and keys it on release") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    OwnDroppedDot(opened);
    const auto x_on = [&](uint32_t depth, uint32_t frame) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(depth);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    const std::vector<int> before{x_on(3, 0), x_on(3, 1), x_on(3, 2)};
    const int baked_before = x_on(2, 0);

    RunCommand(opened.window, "tool.sketch");
    x_on(3, 0);
    {
        Script run({});
        emit viewport->Dragged(3, 1, 0, false);
        emit timeline->FrameChosen(1);
        emit viewport->Dragged(3, 2, 0, false);
        emit timeline->FrameChosen(2);
        emit viewport->Dragged(3, 3, 0, true);
        QApplication::processEvents();
        CHECK(run.Problems().isEmpty());
        CHECK(
            RowValue(*opened.inspector, "Translation").starts_with(std::to_string(before[0] + 20)));
    }
    CHECK(x_on(3, 0) == before[0] + 20);
    CHECK(x_on(3, 1) == before[0] + 40);
    CHECK(x_on(3, 2) == before[0] + 60);

    x_on(2, 0);
    {
        Script run({});
        emit viewport->Dragged(2, 1, 0, true);
        QApplication::processEvents();
        CHECK(run.Problems().isEmpty());
    }
    CHECK(x_on(2, 0) == baked_before + 20);

    RunCommand(opened.window, "tool.select");
    const int sketched_one = x_on(3, 1);
    const int sketched_two = x_on(3, 2);
    x_on(3, 1);
    {
        Script run({});
        emit viewport->Dragged(3, 1, 0, false);
        emit timeline->FrameChosen(2);
        emit viewport->Dragged(3, 1, 0, true);
        QApplication::processEvents();
        CHECK(run.Problems().isEmpty());
    }
    CHECK(x_on(3, 1) == sketched_one);
    CHECK(x_on(3, 2) == sketched_two + 20);
}

TEST_CASE("The inspector's Keyframes section eases the chosen keyframes with no dialog") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    OwnDroppedDot(opened);
    const auto section = [&opened] {
        auto* found = opened.window.findChild<Editor::EaseEditor*>("ease_editor");
        REQUIRE(found != nullptr);
        return found;
    };
    const auto button = [&](const QString& name) {
        auto* found = section()->findChild<QToolButton*>(name);
        REQUIRE(found != nullptr);
        return found;
    };
    CHECK_FALSE(section()->isVisibleTo(&opened.window));

    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    {
        Script moved({});
        emit viewport->Dragged(3, 10, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    }
    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 0}});
    emit timeline->KeysSelected();
    QApplication::processEvents();
    CHECK(section()->isVisibleTo(&opened.window));
    auto* count = section()->findChild<QLabel*>("ease_count");
    REQUIRE(count != nullptr);
    CHECK(count->text() == "1 keyframe");
    CHECK(button("ease_hold")->isChecked());
    auto* curve = section()->findChild<Editor::CurveEditor*>("ease_curve");
    REQUIRE(curve != nullptr);
    CHECK_FALSE(curve->isEnabled());

    {
        Script eased({});
        button("ease_bezier")->click();
        QApplication::processEvents();
        CHECK(eased.Problems().isEmpty());
    }
    emit timeline->KeysSelected();
    QApplication::processEvents();
    CHECK(button("ease_bezier")->isChecked());
    CHECK_FALSE(button("ease_hold")->isChecked());
    CHECK(curve->isEnabled());
    CHECK(curve->Curve() == Document::EasePresets().front().bezier);

    {
        Script eased({});
        auto* preset = section()->findChild<QToolButton*>(
            "ease_preset_" + QString::fromStdString(Document::EasePresets().back().name));
        REQUIRE(preset != nullptr);
        preset->click();
        QApplication::processEvents();
        CHECK(eased.Problems().isEmpty());
    }
    emit timeline->KeysSelected();
    QApplication::processEvents();
    CHECK(curve->Curve() == Document::EasePresets().back().bezier);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    undo->trigger();
    QApplication::processEvents();
    CHECK(button("ease_hold")->isChecked());

    timeline->SelectKeys({});
    emit timeline->KeysSelected();
    QApplication::processEvents();
    CHECK_FALSE(section()->isVisibleTo(&opened.window));
}

TEST_CASE("The graph's property list checks off what it draws and the Fit buttons reach it") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    QListWidget* properties = nullptr;
    QToolButton* fit_all = nullptr;
    QToolButton* fit_keys = nullptr;
    Editor::GraphEditor* graph = nullptr;
    for (QWidget* widget : QApplication::allWidgets()) {
        if (auto* found = qobject_cast<Editor::GraphEditor*>(widget)) graph = found;
        if (widget->objectName() == "graph_properties")
            properties = qobject_cast<QListWidget*>(widget);
        if (widget->objectName() == "graph_graph.fit_all")
            fit_all = qobject_cast<QToolButton*>(widget);
        if (widget->objectName() == "graph_graph.fit_keys")
            fit_keys = qobject_cast<QToolButton*>(widget);
    }
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(properties != nullptr);
    REQUIRE(graph != nullptr);
    CHECK(properties->count() == 0);

    OwnDroppedDot(opened);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    {
        Script moved({});
        emit viewport->Dragged(3, 10, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    }
    REQUIRE(properties->count() >= 1);
    const auto row = [&properties](const QString& property) -> QListWidgetItem* {
        for (int at = 0; at < properties->count(); at++) {
            if (properties->item(at)->text() == property) return properties->item(at);
        }
        return nullptr;
    };
    QListWidgetItem* translation = row("Translation");
    REQUIRE(translation != nullptr);
    CHECK(translation->checkState() == Qt::Checked);
    CHECK(translation->foreground().color() == Editor::GraphEditor::ColourOf(0));
    CHECK(graph->KeyPoint(0, 0).has_value());

    translation->setCheckState(Qt::Unchecked);
    QApplication::processEvents();
    CHECK_FALSE(graph->KeyPoint(0, 0).has_value());

    translation->setCheckState(Qt::Checked);
    QApplication::processEvents();
    CHECK(graph->KeyPoint(0, 0).has_value());

    REQUIRE(fit_all != nullptr);
    REQUIRE(fit_keys != nullptr);
    const double loose = graph->KeyPoint(0, 0).value().y();
    fit_keys->click();
    QApplication::processEvents();
    fit_all->click();
    QApplication::processEvents();
    CHECK(graph->KeyPoint(0, 0).value().y() == loose);
    CHECK(CommandsOf(opened.window).Action("graph.fit_all") != nullptr);
}

TEST_CASE("Undoing an edit keeps the chosen depth and the keyframes that were chosen") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    OwnDroppedDot(opened);
    const auto move_on = [&](uint32_t frame, double dx) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        Script moved({});
        emit viewport->Dragged(3, dx, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    };
    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    move_on(0, 10);
    move_on(2, 40);
    timeline->SelectKeys({ref(0), ref(2)});
    emit timeline->KeysSelected();
    QApplication::processEvents();
    REQUIRE(timeline->SelectedKeys().size() == 2);
    CHECK(RowValue(*opened.inspector, "Depth") == "3");

    QAction* ease = ShortcutAction(opened.window, QKeySequence(Qt::Key_F9));
    REQUIRE(ease != nullptr);
    {
        Script eased({});
        ease->trigger();
        QApplication::processEvents();
        CHECK(eased.Problems().isEmpty());
    }
    CHECK(timeline->SelectedKeys().size() == 2);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    QApplication::processEvents();
    CHECK(RowValue(*opened.inspector, "Depth") == "3");
    CHECK(timeline->SelectedKeys() == std::vector<Document::KeyRef>{ref(0), ref(2)});

    QAction* redo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Redo));
    REQUIRE(redo != nullptr);
    redo->trigger();
    QApplication::processEvents();
    CHECK(RowValue(*opened.inspector, "Depth") == "3");
    CHECK(timeline->SelectedKeys() == std::vector<Document::KeyRef>{ref(0), ref(2)});
}

TEST_CASE("Undoing the edit that made a keyframe drops only that keyframe from the chosen ones") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    OwnDroppedDot(opened);
    const auto move_on = [&](uint32_t frame, double dx) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        Script moved({});
        emit viewport->Dragged(3, dx, 0, true);
        QApplication::processEvents();
        CHECK(moved.Problems().isEmpty());
    };
    const auto ref = [](uint32_t frame) {
        return Document::KeyRef{.property = "Translation", .frame = frame};
    };
    move_on(0, 10);
    move_on(2, 40);
    timeline->SelectKeys({ref(0), ref(2)});
    emit timeline->KeysSelected();
    QApplication::processEvents();
    REQUIRE(timeline->SelectedKeys().size() == 2);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    QApplication::processEvents();
    CHECK(timeline->SelectedKeys() == std::vector<Document::KeyRef>{ref(0)});
}
