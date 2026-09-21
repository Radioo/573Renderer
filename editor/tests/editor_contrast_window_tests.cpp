#include <catch2/catch_test_macros.hpp>

#include "editor_theme.h"
#include "editor_timeline_metrics.h"
#include "editor_timeline.h"
#include "editor_window.h"

#include "document/key_selection.h"

#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <map>
#include <optional>
#include <tuple>
#include <vector>
#include <string>

#include "window_test_support.h"

using namespace WindowTest;
using namespace Editor;

namespace {

constexpr int kLeastPixels = 40;
constexpr int kLeastInkPixels = 6;

QColor Behind(const std::map<QRgb, int>& counted) {
    QRgb common = 0;
    int most = 0;
    for (const auto& [colour, times] : counted) {
        if (times <= most) continue;
        most = times;
        common = colour;
    }
    return QColor(common);
}

QString Names(const QWidget& widget) {
    const QString name = widget.objectName();
    return name.isEmpty() ? QString(widget.metaObject()->className()) : name;
}

QString Shows(const QWidget& widget) {
    if (const auto* label = qobject_cast<const QLabel*>(&widget)) return label->text();
    if (const auto* button = qobject_cast<const QAbstractButton*>(&widget)) return button->text();
    if (const auto* field = qobject_cast<const QLineEdit*>(&widget)) return field->text();
    return {};
}

std::map<QRgb, int> Counted(const QImage& shown, const QRect& at) {
    std::map<QRgb, int> counted;
    for (int y = at.top(); y <= at.bottom(); y++) {
        for (int x = at.left(); x <= at.right(); x++)
            counted[shown.pixel(x, y)]++;
    }
    return counted;
}

std::optional<QColor> Ink(const std::map<QRgb, int>& counted, const QColor& behind) {
    std::optional<QColor> worst;
    double least = Editor::Theme::kLeastContrast;
    for (const QColor& token : Editor::Theme::InkColours()) {
        if (token.rgb() == behind.rgb()) continue;
        const auto found = counted.find(token.rgb());
        if (found == counted.end() || found->second < kLeastInkPixels) continue;
        const double ratio = Editor::Theme::Contrast(token, behind);
        if (ratio >= least) continue;
        least = ratio;
        worst = token;
    }
    return worst;
}

QStringList FaintText(QWidget& window) {
    QApplication::processEvents();
    const QImage shown = window.grab().toImage();
    QStringList faint;
    for (QWidget* widget : window.findChildren<QWidget*>()) {
        if (!widget->isVisible() || !widget->isEnabled()) continue;
        const QString text = Shows(*widget);
        if (text.isEmpty()) continue;
        const QRect at(widget->mapTo(&window, QPoint(0, 0)), widget->size());
        const QRect inside = at.intersected(shown.rect());
        if (inside.width() * inside.height() < kLeastPixels) continue;
        const std::map<QRgb, int> counted = Counted(shown, inside);
        const QColor behind = Behind(counted);
        const std::optional<QColor> drawn = Ink(counted, behind);
        if (!drawn) continue;
        faint.append(QString("%1 \"%2\" %3 on %4 is %5:1")
                         .arg(Names(*widget), text, drawn->name(), behind.name())
                         .arg(Editor::Theme::Contrast(*drawn, behind), 0, 'f', 2));
    }
    return faint;
}

}

TEST_CASE("Every word on the start screen is readable against what it sits on") {
    QSettings().remove("recent/files");
    Opened opened;
    ShowOffScreen(opened.window);
    CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());
}

TEST_CASE("Every word around an open animation is readable against what it sits on") {
    Opened opened;
    ShowOffScreen(opened.window);
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());

    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 0}});
    QApplication::processEvents();
    CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());

    REQUIRE(RunCommand(opened.window, "view.history").isEmpty());
    QApplication::processEvents();
    CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());

    REQUIRE(RunCommand(opened.window, "graph.fit_all").isEmpty());
    QApplication::processEvents();
    CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());
}

TEST_CASE("Every word of a project's chips, the search and a popover is readable") {
    Opened opened;
    ShowOffScreen(opened.window);
    Open(opened, true);
    OwnDroppedDot(opened);
    QApplication::processEvents();
    CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());

    REQUIRE(CommandsOf(opened.window).Run("edit.search"));
    auto* search = opened.window.findChild<QWidget*>("command_search");
    REQUIRE(search != nullptr);
    auto* query = search->findChild<QLineEdit*>("search_query");
    REQUIRE(query != nullptr);
    query->setText("depth");
    QApplication::processEvents();
    CHECK(FaintText(*search).join("; ").toStdString() == std::string());
    search->hide();

    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    for (const uint32_t frame : {uint32_t{0}, uint32_t{1}}) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(3);
        emit viewport->Dragged(3, 10, 0, true);
        QApplication::processEvents();
    }
    emit timeline->KeyChosen("Translation", 0);
    timeline->SelectKeys({Document::KeyRef{.property = "Translation", .frame = 0},
                          Document::KeyRef{.property = "Translation", .frame = 1}});
    QApplication::processEvents();
    REQUIRE(RunCommand(opened.window, "key.wiggle").isEmpty());
    QApplication::processEvents();
    CHECK(FaintText(PopoverOf(opened.window)).join("; ").toStdString() == std::string());
    PressPopover(opened.window, "cancel");
}

TEST_CASE("Every colour the panels paint their own text with is readable on its surface") {
    const std::vector<std::tuple<QString, QColor, QColor>> painted{
        {"row name", Theme::kSoft, Theme::kPanel},
        {"row name under the pointer", Theme::kSoft, Theme::kField},
        {"chosen row name", Theme::kText, Theme::Chosen()},
        {"row detail", Theme::kFaint, Theme::kPanel},
        {"row detail under the pointer", Theme::kFaint, Theme::kField},
        {"chosen row detail", Theme::kSoft, Theme::Chosen()},
        {"search placeholder", Theme::kFaint, Theme::kField},
        {"search placeholder under the pointer", Theme::kSoft, Theme::kLine},
        {"section header in a list", Theme::kFaint, Theme::kPanel},
        {"number on a value chip", Theme::kText, Theme::kField},
        {"panel tab", Theme::kText, Theme::kPanel},
        {"other panel tab", Theme::kFaint, Theme::kPanel},
        {"frame number", kRulerText, kRuler},
        {"depth number", kRulerText, kGutter},
        {"depth name", kSwitchOn, kGutter},
        {"label", kBarText, kLabelChip},
        {"name on an image bar", kBarText, kImageBar},
        {"name on a shape bar", kBarText, kShapeBar},
        {"name on a sprite bar", kBarText, kSpriteBar},
        {"name on a hidden bar", kBarText, kHiddenBar},
    };
    QStringList faint;
    for (const auto& [what, ink, behind] : painted) {
        const double ratio = Theme::Contrast(ink, behind);
        if (ratio >= Theme::kLeastContrast) continue;
        faint.append(QString("%1 %2 on %3 is %4:1")
                         .arg(what, ink.name(), behind.name())
                         .arg(ratio, 0, 'f', 2));
    }
    CHECK(faint.join("; ").toStdString() == std::string());
}

TEST_CASE("Every word stays readable whatever accent Windows reports") {
    const std::vector<QColor> accents{QColor(0x00, 0x78, 0xd4), QColor(0x80, 0x62, 0x26)};
    for (const QColor& accent : accents) {
        Editor::Theme::Apply(*qApp, accent);
        Opened opened;
        ShowOffScreen(opened.window);
        Open(opened, true);
        QApplication::processEvents();
        CHECK(FaintText(opened.window).join("; ").toStdString() == std::string());

        REQUIRE(CommandsOf(opened.window).Run("edit.search"));
        auto* search = opened.window.findChild<QWidget*>("command_search");
        REQUIRE(search != nullptr);
        auto* query = search->findChild<QLineEdit*>("search_query");
        REQUIRE(query != nullptr);
        query->setText("depth");
        QApplication::processEvents();
        CHECK(FaintText(*search).join("; ").toStdString() == std::string());
        search->hide();
    }
    Editor::Theme::Apply(*qApp);
}

TEST_CASE("The accent comes from Windows and keeps every pair it colours readable") {
    const QColor was = Theme::Accent();
    const std::vector<QColor> accents{Theme::kDesignAccent,     QColor(0x9e, 0xb6, 0x50),
                                      QColor(0x00, 0x78, 0xd4), QColor(0xff, 0xf0, 0x00),
                                      QColor(0x10, 0x10, 0x40), QColor(0x80, 0x80, 0x80),
                                      QColor(0xe3, 0x00, 0x8c)};
    QStringList faint;
    for (const QColor& accent : accents) {
        Theme::UseAccent(accent);
        const std::vector<std::tuple<QString, QColor, QColor>> pairs{
            {"word on the accent", Theme::OnAccent(), Theme::Accent()},
            {"chip on the accent", Theme::QuietOnAccent(), Theme::Accent()},
            {"word on the hovered accent", Theme::OnAccent(), Theme::Lifted()},
            {"word on a chosen row", Theme::kText, Theme::Chosen()},
            {"detail on a chosen row", Theme::kSoft, Theme::Chosen()},
            {"tint on a chosen row", Theme::OnChosen(), Theme::Chosen()},
        };
        for (const auto& [what, ink, behind] : pairs) {
            const double ratio = Theme::Contrast(ink, behind);
            if (ratio >= Theme::kLeastContrast) continue;
            faint.append(QString("%1 %2 on %3 (accent %4) is %5:1")
                             .arg(what, ink.name(), behind.name(), accent.name())
                             .arg(ratio, 0, 'f', 2));
        }
        const std::vector<std::tuple<QString, QColor, QColor>> marks{
            {"the accent on the page", Theme::Accent(), Theme::kPage},
            {"the accent on a panel", Theme::Accent(), Theme::kPanel},
            {"the accent on a field", Theme::Accent(), Theme::kField},
            {"the hovered accent on a panel", Theme::Lifted(), Theme::kPanel},
        };
        for (const auto& [what, mark, behind] : marks) {
            const double ratio = Theme::Contrast(mark, behind);
            if (ratio >= Theme::kLeastMark) continue;
            faint.append(QString("%1 %2 on %3 (accent %4) is %5:1")
                             .arg(what, mark.name(), behind.name(), accent.name())
                             .arg(ratio, 0, 'f', 2));
        }
    }
    Theme::UseAccent(was);
    CHECK(faint.join("; ").toStdString() == std::string());
}

TEST_CASE("Windows says the accent in two orders and says nothing when it has none") {
    CHECK(Theme::AccentFromDwm(0xFF50B69EU, true) == QColor(0x9e, 0xb6, 0x50));
    CHECK(Theme::AccentFromDwm(0xC49EB650U, false) == QColor(0x9e, 0xb6, 0x50));
    CHECK_FALSE(Theme::AccentFromDwm(0xFF000000U, true).has_value());
    CHECK_FALSE(Theme::AccentFromDwm(0xFFFFFFFFU, true).has_value());
}
