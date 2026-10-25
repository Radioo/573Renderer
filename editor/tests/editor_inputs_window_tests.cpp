#include <catch2/catch_test_macros.hpp>

#include "editor_rows.h"
#include "editor_window.h"
#include "window_test_support.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <array>
#include <string>
#include <utility>

namespace {
constexpr int kNarrowPanel = 200;
}

using namespace WindowTest;

TEST_CASE("Typing a number into the inputs panel swaps the glyph under every digit place") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* number = panel->findChild<QLineEdit*>("input_number");
    auto* texture = panel->findChild<QComboBox*>("input_texture");
    REQUIRE(inputs != nullptr);
    REQUIRE(number != nullptr);
    REQUIRE(texture != nullptr);

    QTreeWidgetItem* score = InputNamed(*inputs, "score");
    REQUIRE(score != nullptr);
    CHECK(score->childCount() == 4);
    CHECK(score->child(0)->text(0) == QString("score_1000"));
    CHECK(score->child(3)->text(0) == QString("score_0001"));

    inputs->setCurrentItem(score);
    QApplication::processEvents();
    REQUIRE_FALSE(number->isHidden());
    CHECK(number->text() == QString("0"));
    number->setText("123");
    emit number->returnPressed();
    QApplication::processEvents();

    const std::array<std::pair<const char*, const char*>, 4> wanted{
        std::pair{"score_1000", "num0_flat"}, std::pair{"score_0100", "num1_flat"},
        std::pair{"score_0010", "num2_flat"}, std::pair{"score_0001", "num3_flat"}};
    for (const auto& [place, glyph] : wanted) {
        QTreeWidgetItem* row = InputNamed(*inputs, place);
        REQUIRE(row != nullptr);
        inputs->setCurrentItem(row);
        QApplication::processEvents();
        CHECK(texture->currentText().toStdString() == std::string(glyph));
    }

    inputs->setCurrentItem(score);
    QApplication::processEvents();
    CHECK(number->text() == QString("123"));
}

TEST_CASE("A number wider than its digit places is refused instead of drawn wrong") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* number = panel->findChild<QLineEdit*>("input_number");
    REQUIRE(inputs != nullptr);
    REQUIRE(number != nullptr);

    QTreeWidgetItem* score = InputNamed(*inputs, "score");
    REQUIRE(score != nullptr);
    inputs->setCurrentItem(score);
    QApplication::processEvents();
    number->setText("55555");
    emit number->returnPressed();
    QApplication::processEvents();

    CHECK(LastNotice(opened.window).contains("cannot show 55555"));
    inputs->setCurrentItem(InputNamed(*inputs, "score_0001"));
    QApplication::processEvents();
    auto* texture = panel->findChild<QComboBox*>("input_texture");
    REQUIRE(texture != nullptr);
    CHECK(texture->currentText().toStdString() == std::string("num0_flat"));
}

TEST_CASE("A name with one digit place offers a picture, never a number box to refuse") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir, "lone"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* number = panel->findChild<QLineEdit*>("input_number");
    auto* texture = panel->findChild<QComboBox*>("input_texture");
    auto* hint = panel->findChild<QLabel*>("input_hint");
    REQUIRE(inputs != nullptr);
    REQUIRE(number != nullptr);
    REQUIRE(texture != nullptr);
    REQUIRE(hint != nullptr);

    QTreeWidgetItem* lone = InputNamed(*inputs, "lone");
    REQUIRE(lone != nullptr);
    CHECK(lone->childCount() == 0);
    CHECK(lone->data(0, Editor::Rows::kDetailRole)
              .toString()
              .endsWith("one of num0_flat to num9_flat"));

    inputs->setCurrentItem(lone);
    QApplication::processEvents();
    CHECK(number->isHidden());
    CHECK_FALSE(texture->isHidden());
    CHECK(hint->text().contains("one place in the file"));
    CHECK(hint->text().contains("num0_flat to num9_flat"));
}

TEST_CASE("Clicking an input keeps it selected while the clip it lives in opens") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteNamedInSprite(opened.dir, "clear_lamp"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    REQUIRE(inputs != nullptr);
    REQUIRE(OpenClip(opened.window).isEmpty());

    QTreeWidgetItem* lamp = InputNamed(*inputs, "clear_lamp");
    REQUIRE(lamp != nullptr);
    inputs->setCurrentItem(lamp);
    emit inputs->itemClicked(lamp, 0);
    REQUIRE(Settle([&opened] { return !OpenClip(opened.window).isEmpty(); }));
    REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));

    REQUIRE(inputs->currentItem() != nullptr);
    CHECK(inputs->currentItem()->text(0) == QString("clear_lamp"));
}

TEST_CASE("A one place name can be made a real number and then typed into") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir, "lone"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* places = panel->findChild<QSpinBox*>("input_spread");
    auto* spread = panel->findChild<QPushButton*>("input_spread_go");
    auto* number = panel->findChild<QLineEdit*>("input_number");
    auto* texture = panel->findChild<QComboBox*>("input_texture");
    REQUIRE(inputs != nullptr);
    REQUIRE(places != nullptr);
    REQUIRE(spread != nullptr);
    REQUIRE(number != nullptr);
    REQUIRE(texture != nullptr);

    inputs->setCurrentItem(InputNamed(*inputs, "lone"));
    QApplication::processEvents();
    REQUIRE_FALSE(spread->isHidden());
    places->setValue(3);
    spread->click();
    REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));
    REQUIRE(Settle([inputs] { return InputNamed(*inputs, "lone_010") != nullptr; }));

    QTreeWidgetItem* made = InputNamed(*inputs, "lone");
    REQUIRE(made != nullptr);
    CHECK(made->childCount() == 3);
    inputs->setCurrentItem(made);
    QApplication::processEvents();
    REQUIRE_FALSE(number->isHidden());
    number->setText("123");
    emit number->returnPressed();
    QApplication::processEvents();

    const std::array<std::pair<const char*, const char*>, 3> wanted{
        std::pair{"lone_100", "num1_flat"}, std::pair{"lone_010", "num2_flat"},
        std::pair{"lone_001", "num3_flat"}};
    for (const auto& [place, glyph] : wanted) {
        inputs->setCurrentItem(InputNamed(*inputs, place));
        QApplication::processEvents();
        CHECK(texture->currentText().toStdString() == std::string(glyph));
    }
}

TEST_CASE("A spread number runs rightwards from the place the author put there") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir, "lone"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* step = panel->findChild<QSpinBox*>("input_step");
    auto* grows = panel->findChild<QComboBox*>("input_grows");
    REQUIRE(inputs != nullptr);
    REQUIRE(step != nullptr);
    REQUIRE(grows != nullptr);

    inputs->setCurrentItem(InputNamed(*inputs, "lone"));
    QApplication::processEvents();
    CHECK(grows->currentText() == QString("growing right"));
    CHECK(step->value() > 0);
}

TEST_CASE("Leading zeros can be left blank the way a game font blanks them") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir, "lone"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* places = panel->findChild<QSpinBox*>("input_spread");
    auto* spread = panel->findChild<QPushButton*>("input_spread_go");
    auto* number = panel->findChild<QLineEdit*>("input_number");
    auto* blank = panel->findChild<QCheckBox*>("input_blank");
    REQUIRE(inputs != nullptr);
    REQUIRE(places != nullptr);
    REQUIRE(spread != nullptr);
    REQUIRE(number != nullptr);
    REQUIRE(blank != nullptr);

    inputs->setCurrentItem(InputNamed(*inputs, "lone"));
    QApplication::processEvents();
    places->setValue(3);
    spread->click();
    REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));
    REQUIRE(Settle([inputs] { return InputNamed(*inputs, "lone_100") != nullptr; }));

    inputs->setCurrentItem(InputNamed(*inputs, "lone"));
    QApplication::processEvents();
    REQUIRE_FALSE(blank->isHidden());
    number->setText("12");
    emit number->returnPressed();
    QApplication::processEvents();
    CHECK_FALSE(InputNamed(*inputs, "lone_100")->icon(0).isNull());

    blank->setChecked(true);
    QApplication::processEvents();
    CHECK(InputNamed(*inputs, "lone_100")->icon(0).isNull());
    CHECK_FALSE(InputNamed(*inputs, "lone_010")->icon(0).isNull());
    CHECK_FALSE(InputNamed(*inputs, "lone_001")->icon(0).isNull());

    blank->setChecked(false);
    QApplication::processEvents();
    CHECK_FALSE(InputNamed(*inputs, "lone_100")->icon(0).isNull());
}

TEST_CASE("The value form fits a narrow panel instead of growing a sideways scrollbar") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteDigitPlaces(opened.dir, "lone"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    QWidget* panel = Panel(opened.window, "Inputs")->widget();
    REQUIRE(panel != nullptr);
    auto* inputs = panel->findChild<QTreeWidget*>("inputs");
    auto* form = panel->findChild<QWidget*>("input_value");
    REQUIRE(inputs != nullptr);
    REQUIRE(form != nullptr);

    const std::array<const char*, 2> rows{"lone", "score_0001"};
    for (const char* row : rows) {
        inputs->setCurrentItem(InputNamed(*inputs, row));
        QApplication::processEvents();
        REQUIRE_FALSE(form->isHidden());
        CHECK(form->minimumSizeHint().width() <= kNarrowPanel);
    }
}
