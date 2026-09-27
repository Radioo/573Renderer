#include <catch2/catch_test_macros.hpp>

#include "editor_search.h"
#include "editor_timeline.h"
#include "editor_window.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "window_test_support.h"

using namespace WindowTest;

namespace {

struct Search {
    Editor::CommandSearch* popup = nullptr;
    QLineEdit* query = nullptr;
    QTreeWidget* results = nullptr;

    explicit Search(QWidget& window) {
        REQUIRE(CommandsOf(window).Run("edit.search"));
        popup = window.findChild<Editor::CommandSearch*>("command_search");
        REQUIRE(popup != nullptr);
        query = popup->findChild<QLineEdit*>("search_query");
        results = popup->findChild<QTreeWidget*>("search_results");
        REQUIRE(query != nullptr);
        REQUIRE(results != nullptr);
    }

    void Press(Qt::Key key) const {
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
        QApplication::sendEvent(query, &press);
    }

    [[nodiscard]] const QTreeWidgetItem* Row(const QString& starting) const {
        for (int at = 0; at < results->topLevelItemCount(); at++) {
            if (results->topLevelItem(at)->text(1).startsWith(starting))
                return results->topLevelItem(at);
        }
        return nullptr;
    }

    void Pick(const QString& starting) const {
        const QTreeWidgetItem* wanted = Row(starting);
        REQUIRE(wanted != nullptr);
        for (int step = 0; step < results->topLevelItemCount() && results->currentItem() != wanted;
             step++)
            Press(Qt::Key_Down);
        REQUIRE(results->currentItem() == wanted);
        Press(Qt::Key_Return);
    }
};

}

TEST_CASE("Ctrl+K searches the commands, greys the refused ones and runs the one picked") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    {
        const Search search(opened.window);
        CHECK(search.popup->isVisible());
        search.query->setText("split");
        const QTreeWidgetItem* split = search.Row("Split depth at the playhead");
        REQUIRE(split != nullptr);
        CHECK_FALSE(split->flags().testFlag(Qt::ItemIsEnabled));
        CHECK(split->text(1).contains("Choose a depth first"));
        CHECK(split->text(0) == "Depth");
        search.Press(Qt::Key_Escape);
        CHECK_FALSE(search.popup->isVisible());
    }
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    {
        const Search search(opened.window);
        search.query->setText("split");
        search.Pick("Split depth at the playhead");
    }
    CHECK(CommandsOf(opened.window).Action("edit.undo")->text() ==
          QString("&Undo Split depth 2 at frame 1"));
}

TEST_CASE("Ctrl+K goes to a depth of the open clip and opens an animation of the package") {
    Opened opened;
    Open(opened, true);
    {
        const Search search(opened.window);
        search.query->setText("go depth");
        search.Pick("Depth 2");
    }
    auto* chosen = opened.window.findChild<QLabel*>("chosen_status");
    REQUIRE(chosen != nullptr);
    CHECK(chosen->text() == "Depth 2 chosen");
    {
        const Search search(opened.window);
        search.query->setText("animation intro");
        CHECK(search.Row("Animation intro") != nullptr);
    }
}
