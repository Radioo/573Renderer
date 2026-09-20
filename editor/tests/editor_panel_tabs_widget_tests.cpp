#include <catch2/catch_test_macros.hpp>

#include "editor_panel_tabs.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QToolButton>
#include <QWidget>

namespace {

constexpr int kButtonSide = 26;
constexpr int kGap = 6;
constexpr int kTabsWidth = 288;
constexpr int kTabsHeight = 200;

struct Corner {
    QWidget* held = nullptr;
    QLabel* said = nullptr;
    QToolButton* button = nullptr;
};

Corner BuildCorner() {
    Corner corner;
    corner.held = new QWidget;
    auto* beside = new QHBoxLayout(corner.held);
    beside->setContentsMargins(0, 0, 4, 0);
    beside->setSpacing(kGap);
    corner.said = new QLabel;
    beside->addWidget(corner.said);
    corner.button = new QToolButton;
    corner.button->setFixedSize(kButtonSide, kButtonSide);
    beside->addWidget(corner.button);
    return corner;
}

}

TEST_CASE("A corner widget that grows after the panel is laid out keeps its parts apart") {
    Editor::PanelTabs tabs;
    tabs.addTab(new QWidget, "Library");
    const Corner corner = BuildCorner();
    tabs.setCornerWidget(corner.held, Qt::TopRightCorner);
    tabs.resize(kTabsWidth, kTabsHeight);
    tabs.show();
    QApplication::processEvents();

    corner.said->setText("of title");
    tabs.Relayout();
    QApplication::processEvents();

    const QRect said(corner.said->mapTo(corner.held, QPoint(0, 0)), corner.said->size());
    const QRect plus(corner.button->mapTo(corner.held, QPoint(0, 0)), corner.button->size());
    CHECK(said.width() >= corner.said->sizeHint().width());
    CHECK(said.right() < plus.left());
    CHECK(plus.right() < corner.held->width());
}
