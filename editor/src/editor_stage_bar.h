#pragma once

#include <QString>
#include <QWidget>

#include <vector>

class QHBoxLayout;
class QToolButton;

namespace Editor {

class Commands;

struct Crumb {
    QString text;
    int clip = 0;

    friend bool operator==(const Crumb&, const Crumb&) = default;
};

class StageBar : public QWidget {
    Q_OBJECT

public:
    StageBar(Commands& commands, QWidget* parent = nullptr);

    void Build();
    void Show(const std::vector<Crumb>& crumbs, double scale);

signals:
    void ClipAsked(int clip);

private:
    Commands& commands_;
    QHBoxLayout* buttons_;
    QHBoxLayout* crumbs_;
    QToolButton* zoom_;
    std::vector<Crumb> shown_;
};

}
