#include <catch2/catch_test_macros.hpp>

#include "editor_selection_bar.h"
#include "editor_timeline.h"
#include "editor_window.h"

#include "document/key_selection.h"

#include <QApplication>
#include <QLabel>
#include <QString>
#include <QToolButton>

#include "window_test_support.h"

using namespace WindowTest;

namespace {

QString Summary(QWidget& window) {
    auto* summary = window.findChild<QLabel*>("selection_summary");
    REQUIRE(summary != nullptr);
    return summary->text();
}

QToolButton* Button(QWidget& window, const QString& id) {
    return window.findChild<QToolButton*>("bar_" + id);
}

}

TEST_CASE("The selection bar follows what is chosen") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    CHECK(Summary(opened.window) == "Nothing chosen");
    CHECK(Button(opened.window, "depth.split") == nullptr);

    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    CHECK(Summary(opened.window) == "Depth 2");
    REQUIRE(Button(opened.window, "depth.split") != nullptr);
    CHECK(Button(opened.window, "depth.split")->text() == "Split");
    CHECK(Button(opened.window, "depth.own") != nullptr);
    CHECK(Button(opened.window, "depth.align_left") == nullptr);

    emit timeline->DepthsChosen({1, 2});
    CHECK(Summary(opened.window) == "2 depths chosen");
    CHECK(Button(opened.window, "depth.align_left") != nullptr);
    CHECK(Button(opened.window, "depth.split") == nullptr);
}

TEST_CASE("A selection bar button runs its command and greys out with its reason") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    QToolButton* split = Button(opened.window, "depth.split");
    REQUIRE(split != nullptr);
    CHECK(split->isEnabled());
    split->click();
    QApplication::processEvents();
    CHECK(CommandsOf(opened.window).Action("edit.undo")->text() ==
          QString("&Undo Split depth 2 at frame 1"));
    REQUIRE(RunCommand(opened.window, "edit.undo").isEmpty());

    OwnDroppedDot(opened);
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(3);
    CHECK(Button(opened.window, "depth.own") == nullptr);
    REQUIRE(Button(opened.window, "depth.detach") != nullptr);
    QToolButton* refused = Button(opened.window, "depth.split");
    REQUIRE(refused != nullptr);
    CHECK_FALSE(refused->isEnabled());
    CHECK(refused->toolTip().contains("owns depth 3"));
}

TEST_CASE("Selecting keyframes swaps the bar for the keyframe commands") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    OwnDroppedDot(opened);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 0}});
    emit timeline->KeyChosen("Translation", 0);
    CHECK(Summary(opened.window) == "1 keyframe, translation");
    CHECK(Button(opened.window, "key.wiggle") != nullptr);
    CHECK(Button(opened.window, "depth.split") == nullptr);
    CHECK_FALSE(Button(opened.window, "key.wiggle")->isEnabled());
    CHECK(Button(opened.window, "key.wiggle")->toolTip().contains("two or more"));
    CHECK(Button(opened.window, "key.hold")->isEnabled());
}
