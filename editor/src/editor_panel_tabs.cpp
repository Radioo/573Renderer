#include "editor_panel_tabs.h"

#include "editor_theme.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QVariant>

namespace Editor {

namespace {

constexpr int kCountRole = Qt::UserRole + 30;
constexpr int kSide = 10;
constexpr int kGap = 5;
constexpr int kNameSize = 12;
constexpr int kCountSize = 11;
constexpr int kUnderline = 2;

QFont Named() {
    QFont font(Theme::SansFamily());
    font.setPixelSize(kNameSize);
    font.setBold(true);
    return font;
}

QFont Counted() {
    QFont font(Theme::SansFamily());
    font.setPixelSize(kCountSize);
    return font;
}

QString CountText(const QVariant& held) {
    return held.isValid() ? QString::number(held.toInt()) : QString();
}

}

PanelTabBar::PanelTabBar(QWidget* parent) : QTabBar(parent) {
    setObjectName("panel_tab_bar");
    setDrawBase(false);
    setExpanding(false);
    setFocusPolicy(Qt::NoFocus);
}

void PanelTabBar::ShowCount(int index, int count) {
    setTabData(index, count);
    setIconSize(iconSize());
    update();
}

QSize PanelTabBar::tabSizeHint(int index) const {
    const QString count = CountText(tabData(index));
    int width = kSide * 2 + QFontMetrics(Named()).horizontalAdvance(tabText(index));
    if (!count.isEmpty()) width += kGap + QFontMetrics(Counted()).horizontalAdvance(count);
    return {width, Theme::kPanelStripHeight};
}

void PanelTabBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Theme::kPanel);
    for (int at = 0; at < count(); at++) {
        const QRect where = tabRect(at);
        const bool current = at == currentIndex();
        if (underMouse() && where.contains(mapFromGlobal(QCursor::pos())) && !current)
            painter.fillRect(where, Theme::kField);
        painter.setFont(Named());
        painter.setPen(current ? Theme::kText : Theme::kFaint);
        const QString name = tabText(at);
        const int named = QFontMetrics(Named()).horizontalAdvance(name);
        QRect text = where.adjusted(kSide, 0, -kSide, 0);
        painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter, name);
        const QString count_text = CountText(tabData(at));
        if (!count_text.isEmpty()) {
            painter.setFont(Counted());
            painter.setPen(Theme::kFaint);
            painter.drawText(text.adjusted(named + kGap, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                             count_text);
        }
        if (!current) continue;
        painter.fillRect(where.left(), where.bottom() - kUnderline + 1, where.width(), kUnderline,
                         Theme::kAccent);
    }
}

PanelTabs::PanelTabs(QWidget* parent) : QTabWidget(parent) {
    setTabBar(new PanelTabBar);
    setDocumentMode(true);
}

void PanelTabs::ShowCount(int index, int count) {
    qobject_cast<PanelTabBar*>(tabBar())->ShowCount(index, count);
}

}
