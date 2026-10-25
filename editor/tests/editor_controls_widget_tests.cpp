#include <catch2/catch_test_macros.hpp>

#include "editor_filter.h"
#include "editor_theme.h"

#include <QApplication>
#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLineEdit>
#include <QTreeWidget>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <cstdlib>

namespace {

constexpr int kFieldWide = 180;
constexpr int kFieldTall = 30;
constexpr int kStripPart = 6;
constexpr int kApart = 24;
constexpr int kLeastMarks = 10;
constexpr int kBoxWide = 180;
constexpr int kBoxTall = 22;
constexpr int kIndicatorWide = 20;
constexpr int kLeastTick = 30;
constexpr int kIconStrip = 30;
constexpr int kOffCentre = 1;

std::pair<int, int> InkRows(const QImage& shown, int until) {
    int top = shown.height();
    int bottom = -1;
    for (int y = 0; y < shown.height(); y++) {
        for (int x = 0; x < std::min(until, shown.width()); x++) {
            const QColor found = shown.pixelColor(x, y);
            const int gap = std::max({std::abs(found.red() - Editor::Theme::kField.red()),
                                      std::abs(found.green() - Editor::Theme::kField.green()),
                                      std::abs(found.blue() - Editor::Theme::kField.blue())});
            if (gap <= kApart) continue;
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    return {top, bottom};
}

int MarksOnTheLeft(QWidget& one) {
    one.resize(kBoxWide, kBoxTall);
    one.ensurePolished();
    const QImage shown = one.grab().toImage();
    int marks = 0;
    for (int y = 0; y < shown.height(); y++) {
        for (int x = 0; x < std::min(kIndicatorWide, shown.width()); x++) {
            const QColor found = shown.pixelColor(x, y);
            const int gap = std::max({std::abs(found.red() - Editor::Theme::kField.red()),
                                      std::abs(found.green() - Editor::Theme::kField.green()),
                                      std::abs(found.blue() - Editor::Theme::kField.blue())});
            if (gap > kApart) marks++;
        }
    }
    return marks;
}

int MarksOnTheRight(QWidget& one) {
    one.resize(kFieldWide, kFieldTall);
    one.ensurePolished();
    const QImage shown = one.grab().toImage();
    int marks = 0;
    for (int y = 0; y < shown.height(); y++) {
        for (int x = shown.width() - (shown.width() / kStripPart); x < shown.width(); x++) {
            const QColor found = shown.pixelColor(x, y);
            const int gap = std::max({std::abs(found.red() - Editor::Theme::kField.red()),
                                      std::abs(found.green() - Editor::Theme::kField.green()),
                                      std::abs(found.blue() - Editor::Theme::kField.blue())});
            if (gap > kApart) marks++;
        }
    }
    return marks;
}

}

TEST_CASE("A dropdown and a spin box carry a mark a plain field does not") {
    Editor::Theme::Apply(*qApp);

    QLineEdit plain;
    QComboBox picking;
    picking.addItem(QString());
    QSpinBox stepping;
    stepping.setRange(0, 10);
    stepping.setSpecialValueText(" ");

    const int flat = MarksOnTheRight(plain);
    CHECK(MarksOnTheRight(picking) - flat >= kLeastMarks);
    CHECK(MarksOnTheRight(stepping) - flat >= kLeastMarks);
}

TEST_CASE("A ticked checkbox does not look like an empty one") {
    Editor::Theme::Apply(*qApp);

    QCheckBox off;
    QCheckBox on;
    on.setChecked(true);

    CHECK(MarksOnTheLeft(on) - MarksOnTheLeft(off) >= kLeastTick);
}

TEST_CASE("The search icon sits on the filter field's centre line") {
    Editor::Theme::Apply(*qApp);
    QWidget holder;
    auto* tree = new QTreeWidget;
    auto* filter = new QLineEdit;
    QWidget* around = Editor::WithFilter(tree, filter, QString());
    around->setParent(&holder);
    around->resize(220, 120);
    around->ensurePolished();
    filter->ensurePolished();

    const QImage shown = filter->grab().toImage();
    const auto [top, bottom] = InkRows(shown, kIconStrip);
    REQUIRE(bottom > top);
    CHECK(std::abs((top + bottom) - (shown.height() - 1)) <= kOffCentre * 2);
}
