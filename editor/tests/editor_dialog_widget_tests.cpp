#include <catch2/catch_test_macros.hpp>

#include "editor_drift_sheet.h"
#include "editor_ease_editor.h"

#include "document/keyframes.h"

#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>

#include <utility>
#include <vector>

TEST_CASE("The ease editor shows the chosen keys' ease and says what was picked") {
    Editor::EaseEditor ease;
    std::vector<std::pair<Document::Ease, Document::Bezier>> picked;
    QObject::connect(&ease, &Editor::EaseEditor::EaseChosen,
                     [&picked](Document::Ease kind, const Document::Bezier& bezier) {
                         picked.emplace_back(kind, bezier);
                     });
    ease.Show(Editor::EaseView{.ease = Document::Ease::Bezier,
                               .bezier = {.x1 = 0.1, .y1 = 0.2, .x2 = 0.3, .y2 = 0.4},
                               .keys = 2});
    auto* count = ease.findChild<QLabel*>("ease_count");
    auto* curve = ease.findChild<Editor::CurveEditor*>("ease_curve");
    auto* numbers = ease.findChild<QLineEdit*>("ease_numbers");
    auto* hold = ease.findChild<QToolButton*>("ease_hold");
    REQUIRE(count != nullptr);
    REQUIRE(curve != nullptr);
    REQUIRE(numbers != nullptr);
    REQUIRE(hold != nullptr);
    CHECK(count->text() == "2 keyframes");
    CHECK(curve->Curve() == Document::Bezier{.x1 = 0.1, .y1 = 0.2, .x2 = 0.3, .y2 = 0.4});
    CHECK(numbers->text() == "0.1, 0.2, 0.3, 0.4");
    CHECK(curve->isEnabled());

    hold->click();
    REQUIRE(picked.size() == 1);
    CHECK(picked.front().first == Document::Ease::Hold);

    ease.Show(Editor::EaseView{.ease = Document::Ease::Hold, .bezier = {}, .keys = 1});
    CHECK(count->text() == "1 keyframe");
    CHECK_FALSE(curve->isEnabled());
    CHECK(hold->isChecked());

    auto* preset = ease.findChildren<QToolButton*>().at(0);
    REQUIRE(preset != nullptr);
    const QString name = QString::fromStdString(Document::EasePresets().front().name);
    auto* first = ease.findChild<QToolButton*>("ease_preset_" + name);
    REQUIRE(first != nullptr);
    first->click();
    REQUIRE(picked.size() == 2);
    CHECK(picked.back().first == Document::Ease::Bezier);
    CHECK(picked.back().second == Document::EasePresets().front().bezier);
}

TEST_CASE("The drift sheet lists what changed and reports what is kept") {
    Editor::DriftSheet sheet({Editor::DriftRow{.path = "afp/one", .missing = false},
                              Editor::DriftRow{.path = "afp/two", .missing = true}},
                             nullptr);
    auto* rows = sheet.findChild<QTableWidget*>("drift_rows");
    REQUIRE(rows != nullptr);
    REQUIRE(rows->rowCount() == 2);
    CHECK(rows->item(0, 0)->text() == "afp/one");
    CHECK(rows->item(0, 1)->text() == "Changed in the IFS");
    CHECK(rows->item(1, 1)->text() == "No longer in the IFS");
    CHECK(sheet.Kept() == std::vector<QString>{"afp/one", "afp/two"});

    rows->item(0, 0)->setCheckState(Qt::Unchecked);
    CHECK(sheet.Kept() == std::vector<QString>{"afp/two"});

    auto* export_all = sheet.findChild<QPushButton*>("drift_export_all");
    REQUIRE(export_all != nullptr);
    export_all->click();
    CHECK(sheet.Kept().empty());

    auto* keep_all = sheet.findChild<QPushButton*>("drift_keep_all");
    REQUIRE(keep_all != nullptr);
    keep_all->click();
    CHECK(sheet.Kept().size() == 2);
}
