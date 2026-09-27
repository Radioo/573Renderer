#include <catch2/catch_test_macros.hpp>

#include "editor_commands.h"
#include "editor_timeline.h"
#include "editor_window.h"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QString>

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

#include "window_test_support.h"

using namespace WindowTest;

namespace {

bool InMenus(const QList<QAction*>& actions, const QAction* wanted) {
    return std::ranges::any_of(actions, [wanted](const QAction* action) {
        if (action == wanted) return true;
        return action->menu() != nullptr && InMenus(action->menu()->actions(), wanted);
    });
}

QMenu* MenuHolding(const QList<QAction*>& actions, const QAction* wanted) {
    for (const QAction* action : actions) {
        QMenu* menu = action->menu();
        if (menu == nullptr) continue;
        if (menu->actions().contains(wanted)) return menu;
        if (QMenu* deeper = MenuHolding(menu->actions(), wanted)) return deeper;
    }
    return nullptr;
}

}

TEST_CASE("Every command is in a menu and no two commands share a shortcut") {
    Editor::Window window;
    Editor::Commands& commands = CommandsOf(window);
    const std::vector<QString> ids = commands.Ids();
    REQUIRE(ids.size() > 50);
    std::map<QString, QString> taken;
    for (const QString& id : ids) {
        QAction* action = commands.Action(id);
        REQUIRE(action != nullptr);
        INFO(id.toStdString());
        CHECK(InMenus(window.menuBar()->actions(), action));
        const QString keys = action->shortcut().toString();
        if (keys.isEmpty()) continue;
        const auto [at, fresh] = taken.emplace(keys, id);
        INFO("shared with " << at->second.toStdString());
        CHECK(fresh);
    }
}

TEST_CASE("Every shortcut the editor had before the redesign still runs a command") {
    Editor::Window window;
    Editor::Commands& commands = CommandsOf(window);
    const std::vector<QKeySequence> kept{
        QKeySequence::Open,
        QKeySequence::Save,
        QKeySequence::SaveAs,
        QKeySequence::Undo,
        QKeySequence::Redo,
        QKeySequence::Copy,
        QKeySequence::Cut,
        QKeySequence::Paste,
        QKeySequence::Delete,
        QKeySequence::SelectAll,
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S),
        QKeySequence(Qt::CTRL | Qt::Key_E),
        QKeySequence(Qt::Key_Space),
        QKeySequence(Qt::Key_PageUp),
        QKeySequence(Qt::Key_PageDown),
        QKeySequence(Qt::Key_Home),
        QKeySequence(Qt::Key_End),
        QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_J),
        QKeySequence(Qt::Key_J),
        QKeySequence(Qt::Key_K),
        QKeySequence(Qt::Key_B),
        QKeySequence(Qt::Key_N),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D),
        QKeySequence(Qt::CTRL | Qt::Key_D),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Home),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_H),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_G),
        QKeySequence(Qt::Key_BracketLeft),
        QKeySequence(Qt::Key_BracketRight),
        QKeySequence(Qt::ALT | Qt::Key_BracketLeft),
        QKeySequence(Qt::ALT | Qt::Key_BracketRight),
        QKeySequence(Qt::CTRL | Qt::Key_BracketRight),
        QKeySequence(Qt::CTRL | Qt::Key_BracketLeft),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft),
        QKeySequence(Qt::CTRL | Qt::Key_R),
        QKeySequence(Qt::Key_Equal),
        QKeySequence(Qt::Key_Minus),
        QKeySequence(Qt::CTRL | Qt::Key_0),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H),
        QKeySequence(Qt::Key_F9),
        QKeySequence(Qt::SHIFT | Qt::Key_F9),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9),
    };
    for (const QKeySequence& keys : kept) {
        INFO(keys.toString().toStdString());
        const std::vector<QString> ids = commands.Ids();
        CHECK(std::ranges::any_of(ids, [&commands, &keys](const QString& id) {
            return commands.Action(id)->shortcut() == keys;
        }));
    }
}

TEST_CASE("A command that cannot run says why, greys out in its menu and changes nothing") {
    Opened opened;
    Editor::Commands& commands = CommandsOf(opened.window);
    CHECK(commands.Refusal("depth.split") == QString("Open an animation first"));
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    CHECK(commands.Refusal("depth.split") == QString("Choose a depth first"));
    CHECK_FALSE(commands.Run("depth.split"));

    QAction* split = commands.Action("depth.split");
    QMenu* menu = MenuHolding(opened.window.menuBar()->actions(), split);
    REQUIRE(menu != nullptr);
    emit menu->aboutToShow();
    CHECK_FALSE(split->isEnabled());
    CHECK(split->toolTip() == QString("Choose a depth first"));
    emit menu->aboutToHide();

    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    CHECK_FALSE(commands.Refusal("depth.split").has_value());
    emit menu->aboutToShow();
    CHECK(split->isEnabled());
    emit menu->aboutToHide();
    {
        Script done({});
        CHECK(commands.Run("depth.split"));
        QApplication::processEvents();
        CHECK(done.Problems().isEmpty());
    }
    CHECK(commands.Action("edit.undo")->text() == QString("&Undo Split depth 2 at frame 1"));
    REQUIRE(commands.Run("edit.undo"));

    OwnDroppedDot(opened);
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(3);
    const std::optional<QString> owned = commands.Refusal("depth.split");
    REQUIRE(owned.has_value());
    CHECK(owned->contains("owns depth 3"));
    const QString undo_before = commands.Action("edit.undo")->text();
    CHECK_FALSE(commands.Run("depth.split"));
    CHECK(commands.Action("edit.undo")->text() == undo_before);
}

TEST_CASE("Keyframe commands say how many selected keyframes they need") {
    Opened opened;
    Open(opened, true);
    Editor::Commands& commands = CommandsOf(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    OwnDroppedDot(opened);
    emit timeline->DepthChosen(3);
    CHECK(commands.Refusal("key.reverse") == QString("Select two or more keyframes first"));
    CHECK(commands.Refusal("key.simplify") == QString("Select three or more keyframes first"));
    CHECK(commands.Refusal("key.hold") == QString("Select keyframes first"));
    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 0}});
    CHECK_FALSE(commands.Refusal("key.hold").has_value());
    CHECK(commands.Refusal("key.reverse") == QString("Select two or more keyframes first"));
    emit timeline->DepthChosen(2);
    CHECK(commands.Refusal("key.hold") == QString("Choose a depth the project owns first"));
}
