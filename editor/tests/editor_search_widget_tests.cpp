#include <catch2/catch_test_macros.hpp>

#include "editor_search.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <vector>

namespace {

struct Scene {
    QWidget owner;
    Editor::CommandSearch search{&owner};
    std::vector<QString> ran;

    Scene() {
        owner.resize(900, 600);
        search.Open({
            {.category = "Depth",
             .text = "Split depth at the playhead",
             .detail = {},
             .keys = "Ctrl+Shift+D",
             .available = true,
             .run = [this] { ran.push_back("split"); }},
            {.category = "Depth",
             .text = "Fit to the stage",
             .detail = "Choose a depth first",
             .keys = "Ctrl+Alt+F",
             .available = false,
             .run = [this] { ran.push_back("fit"); }},
            {.category = "Go to",
             .text = "Depth 4, splash",
             .detail = {},
             .keys = {},
             .available = true,
             .run = [this] { ran.push_back("depth 4"); }},
        });
    }

    QLineEdit& Query() {
        auto* query = search.findChild<QLineEdit*>("search_query");
        REQUIRE(query != nullptr);
        return *query;
    }

    QTreeWidget& Results() {
        auto* results = search.findChild<QTreeWidget*>("search_results");
        REQUIRE(results != nullptr);
        return *results;
    }

    void Type(const QString& text) { Query().setText(text); }

    void Press(Qt::Key key) {
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
        QApplication::sendEvent(&Query(), &press);
    }
};

}

TEST_CASE("Command search keeps the items holding every typed word, ignoring case") {
    Scene scene;
    CHECK(scene.Results().topLevelItemCount() == 3);
    scene.Type("DEPTH");
    CHECK(scene.Results().topLevelItemCount() == 3);
    scene.Type("stage fit");
    REQUIRE(scene.Results().topLevelItemCount() == 1);
    CHECK(scene.Results().topLevelItem(0)->text(1).startsWith("Fit to the stage"));
    scene.Type("splash go");
    REQUIRE(scene.Results().topLevelItemCount() == 1);
    CHECK(scene.Results().topLevelItem(0)->text(1) == "Depth 4, splash");
    scene.Type("nothing like this");
    CHECK(scene.Results().topLevelItemCount() == 0);
}

TEST_CASE("Command search shows a refused command greyed with its reason and never runs it") {
    Scene scene;
    scene.Type("fit");
    REQUIRE(scene.Results().topLevelItemCount() == 1);
    const QTreeWidgetItem* fit = scene.Results().topLevelItem(0);
    CHECK_FALSE(fit->flags().testFlag(Qt::ItemIsEnabled));
    CHECK(fit->text(1).contains("Choose a depth first"));
    CHECK(fit->text(2) == "Ctrl+Alt+F");
    scene.Press(Qt::Key_Return);
    CHECK(scene.ran.empty());
}

TEST_CASE("Command search runs the highlighted result on Enter and moves with the arrows") {
    Scene scene;
    CHECK(scene.Results().currentItem() == scene.Results().topLevelItem(0));
    scene.Press(Qt::Key_Down);
    CHECK(scene.Results().currentItem() == scene.Results().topLevelItem(2));
    scene.Press(Qt::Key_Up);
    CHECK(scene.Results().currentItem() == scene.Results().topLevelItem(0));
    scene.Press(Qt::Key_Down);
    scene.Press(Qt::Key_Return);
    REQUIRE(scene.ran.size() == 1);
    CHECK(scene.ran.front() == "depth 4");
    CHECK(scene.search.isHidden());
}

TEST_CASE("Command search closes on Escape without running anything") {
    Scene scene;
    scene.Press(Qt::Key_Escape);
    CHECK(scene.search.isHidden());
    CHECK(scene.ran.empty());
}
