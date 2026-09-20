#pragma once

#include <QTabBar>
#include <QTabWidget>

namespace Editor {

class PanelTabBar : public QTabBar {
    Q_OBJECT

public:
    explicit PanelTabBar(QWidget* parent = nullptr);

    void ShowCount(int index, int count);

protected:
    [[nodiscard]] QSize tabSizeHint(int index) const override;
    void paintEvent(QPaintEvent* event) override;
};

class PanelTabs : public QTabWidget {
    Q_OBJECT

public:
    explicit PanelTabs(QWidget* parent = nullptr);

    void ShowCount(int index, int count);
};

}
