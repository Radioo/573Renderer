#include "editor_theme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <cmath>

namespace Editor::Theme {

namespace {

constexpr double kContrastOffset = 0.05;
constexpr double kLowChannel = 0.03928;
constexpr double kFlatSlope = 12.92;
constexpr double kCurveShift = 0.055;
constexpr double kCurveScale = 1.055;
constexpr double kCurvePower = 2.4;
constexpr double kRedShare = 0.2126;
constexpr double kGreenShare = 0.7152;
constexpr double kBlueShare = 0.0722;
constexpr double kChosenMix = 0.26;
constexpr double kOnChosenMix = 0.44;
constexpr double kLiftMix = 0.12;
constexpr double kQuietStep = 0.08;
constexpr double kQuietFloor = 5.0;
constexpr int kQuietSteps = 12;
constexpr int kFitSteps = 8;
constexpr int kReadSteps = 16;
constexpr double kReadStep = 0.1;
constexpr int kFitPercent = 115;
constexpr int kMidLightness = 128;
constexpr int kTopChannel = 255;
constexpr uint kRedByte = 16;
constexpr uint kGreenByte = 8;
constexpr uint kByteMask = 0xFFU;

QString Hex(const QColor& colour) {
    return colour.name(QColor::HexRgb);
}

double Channel(int value) {
    const double part = static_cast<double>(value) / 255.0;
    return part <= kLowChannel ? part / kFlatSlope
                               : std::pow((part + kCurveShift) / kCurveScale, kCurvePower);
}

double Luminance(const QColor& colour) {
    return (kRedShare * Channel(colour.red())) + (kGreenShare * Channel(colour.green())) +
           (kBlueShare * Channel(colour.blue()));
}

QColor Blend(const QColor& from, const QColor& to, double part) {
    const auto mix = [part](int a, int b) {
        return static_cast<int>(std::lround(a + ((b - a) * part)));
    };
    return {mix(from.red(), to.red()), mix(from.green(), to.green()), mix(from.blue(), to.blue())};
}

QColor Ink(const QColor& surface) {
    return Contrast(kText, surface) >= Contrast(kInk, surface) ? kText : kInk;
}

QColor Quiet(const QColor& surface) {
    const QColor ink = Ink(surface);
    QColor quiet = ink;
    for (int step = 1; step <= kQuietSteps; step++) {
        const QColor tried = Blend(ink, surface, step * kQuietStep);
        if (Contrast(tried, surface) < kQuietFloor) break;
        quiet = tried;
    }
    return quiet;
}

QColor Readable(const QColor& from, const QColor& toward, const QColor& behind) {
    QColor tried = from;
    for (int step = 0; step <= kReadSteps; step++) {
        if (Contrast(tried, behind) >= kLeastContrast) break;
        tried = Blend(tried, toward, kReadStep);
    }
    return tried;
}

QColor Fitted(const QColor& accent) {
    QColor fitted = accent;
    for (int step = 0; step < kFitSteps; step++) {
        if (Contrast(Ink(fitted), fitted) >= kLeastContrast) break;
        fitted = fitted.lightness() < kMidLightness ? fitted.darker(kFitPercent)
                                                    : fitted.lighter(kFitPercent);
    }
    return fitted;
}

struct Palette {
    QColor accent = kDesignAccent;
    QColor lifted = kDesignAccent;
    QColor on_accent = kInk;
    QColor quiet_on_accent = kInk;
    QColor chosen = kPage;
    QColor on_chosen = kText;
};

Palette& Shown() {
    static Palette shown;
    return shown;
}

QString Pick(const QStringList& wanted, const QString& fallback) {
    const QStringList families = QFontDatabase::families();
    for (const QString& one : wanted) {
        if (families.contains(one, Qt::CaseInsensitive)) return one;
    }
    return fallback;
}

}

double Contrast(const QColor& text, const QColor& behind) {
    const double lit = Luminance(text);
    const double under = Luminance(behind);
    const double brighter = std::max(lit, under);
    const double darker = std::min(lit, under);
    return (brighter + kContrastOffset) / (darker + kContrastOffset);
}

std::vector<QColor> InkColours() {
    return {kText,    kSoft,      kFaint,     kAmber,   kGreen,
            Accent(), OnAccent(), OnChosen(), Lifted(), QuietOnAccent()};
}

std::optional<QColor> AccentFromDwm(quint32 packed, bool reversed) {
    const int first = static_cast<int>((packed >> kRedByte) & kByteMask);
    const int green = static_cast<int>((packed >> kGreenByte) & kByteMask);
    const int last = static_cast<int>(packed & kByteMask);
    const QColor said(reversed ? last : first, green, reversed ? first : last);
    if (!said.isValid()) return std::nullopt;
    if (said.red() == 0 && said.green() == 0 && said.blue() == 0) return std::nullopt;
    if (said.red() == kTopChannel && said.green() == kTopChannel && said.blue() == kTopChannel)
        return std::nullopt;
    return said;
}

std::optional<QColor> SystemAccent() {
    const QSettings dwm(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\DWM)",
                        QSettings::NativeFormat);
    const QVariant accent = dwm.value("AccentColor");
    if (accent.isValid()) return AccentFromDwm(accent.toUInt(), true);
    const QVariant colorization = dwm.value("ColorizationColor");
    if (colorization.isValid()) return AccentFromDwm(colorization.toUInt(), false);
    return std::nullopt;
}

void UseAccent(const QColor& accent) {
    Palette& shown = Shown();
    shown.accent = Fitted(accent);
    shown.on_accent = Ink(shown.accent);
    shown.lifted = Blend(shown.accent, shown.on_accent == kText ? kInk : kText, kLiftMix);
    shown.quiet_on_accent = Quiet(shown.accent);
    shown.chosen = Blend(kPage, shown.accent, kChosenMix);
    shown.on_chosen = Readable(Blend(shown.accent, kText, kOnChosenMix), kText, shown.chosen);
}

QColor Accent() {
    return Shown().accent;
}

QColor Lifted() {
    return Shown().lifted;
}

QColor OnAccent() {
    return Shown().on_accent;
}

QColor QuietOnAccent() {
    return Shown().quiet_on_accent;
}

QColor Chosen() {
    return Shown().chosen;
}

QColor OnChosen() {
    return Shown().on_chosen;
}

QString SansFamily() {
    return Pick({"Segoe UI Variable Text", "Segoe UI", "IBM Plex Sans"}, "Segoe UI");
}

QString MonoFamily() {
    return Pick({"Cascadia Mono", "Consolas", "IBM Plex Mono"}, "Consolas");
}

QString DockStyle() {
    return QString(R"(
ads--CDockContainerWidget { background: %page; }
ads--CDockContainerWidget QSplitter::handle { background: %page; }
ads--CDockAreaWidget { background: %panel; border: 1px solid %line; }
ads--CDockAreaTitleBar {
    background: %panel;
    border-bottom: 1px solid %line;
    padding: 0px;
    min-height: 30px;
}
ads--CDockWidgetTab {
    background: %panel;
    border: 0px;
    padding: 0px 10px;
    min-height: 30px;
}
ads--CDockWidgetTab[activeTab="true"] {
    background: %panel;
    border-bottom: 2px solid %accent;
}
ads--CDockWidgetTab QLabel { color: %faint; font-weight: 600; }
ads--CDockWidgetTab[activeTab="true"] QLabel { color: %text; }
ads--CDockWidget {
    background: %panel;
    border-color: %line;
    border-style: solid;
    border-width: 0px;
    padding: 0px;
}
ads--CTitleBarButton { background: transparent; border: 0px; padding: 0px 4px; }
ads--CTitleBarButton:hover { background: %field; }
)")
        .replace("%page", Hex(kPage))
        .replace("%panel", Hex(kPanel))
        .replace("%field", Hex(kField))
        .replace("%line", Hex(kLine))
        .replace("%accent", Hex(Accent()))
        .replace("%faint", Hex(kFaint))
        .replace("%text", Hex(kText));
}

void Apply(QApplication& app) {
    UseAccent(SystemAccent().value_or(kDesignAccent));
    QFont base(SansFamily());
    base.setPixelSize(kBaseSize);
    QApplication::setFont(base);

    QPalette palette;
    palette.setColor(QPalette::Window, kPage);
    palette.setColor(QPalette::WindowText, kText);
    palette.setColor(QPalette::Base, kPanel);
    palette.setColor(QPalette::AlternateBase, kField);
    palette.setColor(QPalette::Text, kText);
    palette.setColor(QPalette::PlaceholderText, kFaint);
    palette.setColor(QPalette::Button, kPanel);
    palette.setColor(QPalette::ButtonText, kText);
    palette.setColor(QPalette::BrightText, kText);
    palette.setColor(QPalette::Highlight, Chosen());
    palette.setColor(QPalette::HighlightedText, kText);
    palette.setColor(QPalette::ToolTipBase, kPanel);
    palette.setColor(QPalette::ToolTipText, kText);
    palette.setColor(QPalette::Mid, kEdge);
    palette.setColor(QPalette::Dark, kLine);
    palette.setColor(QPalette::Disabled, QPalette::Text, kFaint);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, kFaint);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, kFaint);
    QApplication::setPalette(palette);

    app.setStyleSheet(QString(R"(
QWidget { background: %page; color: %text; }
QMainWindow::separator { background: %page; width: 4px; height: 4px; }
QToolTip { background: %panel; color: %text; border: 1px solid %edge; padding: 3px 6px; }

QMenuBar { background: %panel; color: %soft; padding: 0px; }
QMenuBar::item { background: transparent; padding: 6px 8px; color: %soft; }
QMenuBar::item:selected { background: %field; color: %text; }
QMenu { background: %panel; border: 1px solid %edge; padding: 4px 0px; }
QMenu::item { padding: 5px 22px 5px 22px; color: %text; }
QMenu::item:selected { background: %chosen; }
QMenu::item:disabled { color: %faint; }
QMenu::separator { height: 1px; background: %line; margin: 4px 8px; }

QStatusBar { background: %panel; border-top: 1px solid %line; color: %faint; }
QStatusBar::item { border: 0px; }
QStatusBar QLabel { color: %faint; font-size: 11px; }
QLabel#host_named { color: %soft; }
QStatusBar QWidget { background: %panel; }

QToolBar#top_bar { background: %panel; border: 0px; border-bottom: 1px solid %line;
                   spacing: 2px; padding: 0px 10px 0px 6px; }
QToolBar { background: %panel; border: 0px; border-bottom: 1px solid %line; spacing: 2px;
           padding: 0px 10px 0px 6px; }
QToolBar::separator { background: %edge; width: 1px; height: 20px; margin: 0px 6px; }
QWidget#divider { background: %edge; }
QLabel#logo { background: %accent; color: %onaccent; }
QToolButton#menu_button { color: %soft; padding: 0px 8px; font-size: 13px; }
QToolButton#menu_button::menu-indicator { image: none; width: 0px; }
QToolButton#menu_button:hover { background: %field; color: %text; }
QToolButton#bar_icon { padding: 0px; }
QPushButton#search { background: %field; border: 1px solid %edge; padding: 0px; }
QPushButton#search:hover { border-color: %soft; }
QLabel#search_text { color: %faint; }
QLabel#search_chip { color: %soft; border: 1px solid %edge; padding: 1px 5px; }
QLabel#document_state { color: %text; }
QLabel#document_edits { color: %amber; }
QToolButton#project { color: %soft; padding: 0px 8px; }
QToolButton#project::menu-indicator { image: none; width: 0px; }
QToolButton#export { color: %amber; background: #3d3016; border: 1px solid #6b5520;
                     padding: 0px 9px; }
QToolButton#export:hover { background: #4a3a1b; }
QToolButton#save { color: %onaccent; background: %accent; font-weight: 600; padding: 0px 9px; }
QToolButton#save:hover { background: %lifted; }

QWidget#start_screen { background: %page; }
QPushButton#start_open { background: %accent; color: %onaccent; border: 0px; }
QPushButton#start_open:hover { background: %lifted; }
QPushButton#start_open_project { background: transparent; border: 1px solid %edge; }
QPushButton#start_open_project:hover { background: %panel; }
QFrame#start_drop { border: 1px dashed %edge; background: transparent; }
QFrame#start_card { border: 1px solid %edge; background: %panel; }
QFrame#start_rule { background: %line; border: 0px; }
QLabel#start_dot { border: 0px; }
QPushButton#recent_row { background: transparent; border: 0px; border-bottom: 1px solid %line;
                         padding: 0px; min-height: 55px; max-height: 55px; }
QPushButton#recent_row:hover { background: %panel; }

QWidget#stage_bar { background: %panel; border-bottom: 1px solid %line; }
QWidget#tool_strip { background: %panel; border-right: 1px solid %line; }
QFrame#tool_rule { background: %edge; border: 0px; }
QFrame#divider { background: %edge; border: 0px; }
QToolButton[strip="true"] { background: transparent; border: 0px; padding: 0px; }
QToolButton[strip="true"]:hover { background: %field; }
QToolButton[strip="true"]:checked { background: %chosen; }
QToolButton[stage_icon="true"] { background: transparent; border: 0px; padding: 0px; }
QToolButton[stage_icon="true"]:hover { background: %field; }
QToolButton[stage_icon="true"]:checked { background: %chosen; }
QToolButton#stage_zoom { border: 1px solid %line; color: %soft; padding: 0px 4px;
                         font-family: "%mono"; font-size: 11px; }
QToolButton#stage_zoom::menu-indicator { image: none; width: 0px; }
QToolButton#stage_zoom:hover { background: %field; }
QLabel#crumb_between { color: %faint; }
QWidget#stage_bar QToolButton { color: %soft; padding: 0px 6px; min-height: 0px; }
QWidget#stage_bar QToolButton[last="true"] { color: %text; font-weight: 600; }

QWidget#timeline_bar { background: %panel; border-top: 1px solid %edge;
                       border-bottom: 1px solid %line; }
QWidget#transport { background: transparent; }
QToolButton[transport="true"] { background: transparent; border: 0px; padding: 0px; }
QToolButton[transport="true"]:hover { background: %field; }
QToolButton[transport="true"]:checked { background: %chosen; }
QToolButton[play="true"] { background: #22252a; }
QSpinBox#timeline_frame { background: %page; border: 1px solid %edge; color: %onchosen;
                          font-family: "%mono"; font-size: 13px; padding: 0px 6px; }
QLabel#timeline_count, QLabel#timeline_time, QLabel#timeline_work_area,
QLabel#timeline_work_named { color: %faint; }
QLabel#timeline_label { color: %amber; }
QFrame#timeline_modes { border: 1px solid %edge; background: transparent; }
QToolButton[mode="true"] { color: %faint; padding: 0px 9px; background: transparent;
                           min-height: 0px; }
QToolButton[mode="true"]:checked { background: %chosen; color: %onchosen; }

QToolButton[panel_icon="true"] { background: transparent; border: 0px; padding: 0px; }
QToolButton[panel_icon="true"]:hover { background: %field; }
QToolButton[chip="true"] { background: transparent; border: 1px solid %line; color: %faint;
                           padding: 0px 7px; font-size: 11px; min-height: 0px; }
QToolButton[chip="true"]:checked { background: %chosen; border-color: %chosen;
                                   color: %onchosen; }
QLineEdit#filter_field { background: %field; border: 1px solid %line; color: %text;
                         padding: 0px 6px; }
QLabel#library_of { color: %faint; font-size: 11px; }
QTabWidget#package_tabs::pane, QTabWidget#library_tabs::pane {
    border: 0px; border-top: 1px solid %line; background: %panel; top: -1px;
}

QWidget#selection_bar { background: %panel; border-top: 1px solid %line; }
QLabel#selection_summary { color: %text; }
QLabel#selection_detail { color: %faint; }
QToolButton[selection="true"] { color: %soft; padding: 0px 9px; min-height: 0px; }
QToolButton[selection="true"]:hover { background: %field; color: %text; }
QToolButton[selection="true"]:disabled { color: %line; }

QWidget[section="true"] { background: %panel; border-top: 1px solid %line; }
QWidget[section="true"] QLabel[section_name="true"] { color: %soft; font-size: 11px;
                                                      font-weight: 600; }
QLabel#inspector_title { color: %text; }
QLabel#inspector_detail { color: %faint; font-size: 11px; }
QLabel#inspector_badge { color: %soft; border: 1px solid %edge; padding: 1px 5px;
                         font-size: 10px; font-weight: 700; }
QDoubleSpinBox[value_box="true"], QSpinBox[value_box="true"], QLineEdit[value_box="true"] {
    background: %page; border: 1px solid %line; color: %text; font-family: "%mono";
    font-size: 11px; padding: 0px 4px;
}

QPushButton {
    background: %panel; color: %text; border: 1px solid %edge; border-radius: 0px;
    padding: 0px 9px; min-height: 26px;
}
QPushButton:hover { background: %field; }
QPushButton:pressed { background: %line; }
QPushButton:disabled { color: %faint; border-color: %line; }
QPushButton:default { background: %accent; color: %onaccent; border-color: %accent;
                      font-weight: 600; }

QToolButton {
    background: transparent; color: %soft; border: 0px; border-radius: 0px; padding: 0px 8px;
    min-height: 26px;
}
QToolButton:hover { background: %field; color: %text; }
QToolButton:pressed { background: %line; }
QToolButton:checked { background: %chosen; color: %text; }
QToolButton:disabled { color: %faint; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: %field; color: %text; border: 1px solid %line; border-radius: 0px;
    padding: 0px 6px; min-height: 24px; selection-background-color: %chosen;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: %accent; }
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0px; border: 0px; }
QComboBox::drop-down { border: 0px; width: 16px; }
QComboBox QAbstractItemView { background: %panel; border: 1px solid %edge;
                              selection-background-color: %chosen; }

QTreeWidget, QTreeView, QTableWidget, QTableView, QListWidget, QListView {
    background: %panel; alternate-background-color: %panel; border: 0px;
    outline: none; selection-background-color: %chosen; selection-color: %text;
}
QTreeView::item, QTableView::item, QListView::item { min-height: 22px; padding: 2px 4px;
                                                     border: 0px; }
QTreeView::item:selected, QTableView::item:selected, QListView::item:selected {
    background: %chosen; color: %text;
}
QTreeView::item:hover, QListView::item:hover { background: %field; }
QTreeView::branch { background: %panel; }
QHeaderView { background: %panel; }
QHeaderView::section {
    background: %panel; color: %faint; border: 0px; border-bottom: 1px solid %line;
    padding: 4px 6px; font-weight: 400;
}

QTabWidget::pane { border: 1px solid %line; background: %panel; top: -1px; }
QTabBar { background: %panel; qproperty-drawBase: 0; }
QTabBar::tab {
    background: %panel; color: %faint; border: 0px; padding: 7px 10px; font-weight: 600;
    min-height: 16px;
}
QTabBar::tab:selected { color: %text; border-bottom: 2px solid %accent; }
QTabBar::tab:hover { color: %text; }

QScrollBar:vertical { background: %page; width: 10px; margin: 0px; border: 0px; }
QScrollBar:horizontal { background: %page; height: 10px; margin: 0px; border: 0px; }
QScrollBar::handle { background: %edge; min-height: 24px; min-width: 24px; border-radius: 0px; }
QScrollBar::handle:hover { background: %soft; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; border: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QSlider::groove:horizontal { background: %line; height: 2px; }
QSlider::handle:horizontal { background: %soft; width: 8px; height: 12px; margin: -5px 0px; }
QSlider::sub-page:horizontal { background: %accent; height: 2px; }

QSplitter::handle { background: %page; }
QDialog { background: %page; }
QScrollArea { border: 0px; background: %page; }
QCheckBox, QRadioButton { color: %text; spacing: 6px; }
QLabel { background: transparent; }
)")
                          .replace("%page", Hex(kPage))
                          .replace("%panel", Hex(kPanel))
                          .replace("%field", Hex(kField))
                          .replace("%line", Hex(kLine))
                          .replace("%edge", Hex(kEdge))
                          .replace("%text", Hex(kText))
                          .replace("%soft", Hex(kSoft))
                          .replace("%faint", Hex(kFaint))
                          .replace("%accent", Hex(Accent()))
                          .replace("%onaccent", Hex(OnAccent()))
                          .replace("%chosen", Hex(Chosen()))
                          .replace("%amber", Hex(kAmber))
                          .replace("%mono", MonoFamily())
                          .replace("%onchosen", Hex(OnChosen()))
                          .replace("%lifted", Hex(Lifted())));
}

}
