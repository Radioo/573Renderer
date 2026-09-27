#pragma once

#include <QString>
#include <QWidget>

#include <vector>

class QFrame;
class QHBoxLayout;
class QLabel;

namespace Editor {

class Commands;

class SelectionBar : public QWidget {
    Q_OBJECT

public:
    SelectionBar(Commands& commands, QWidget* parent = nullptr);

    void Show(const QString& summary, const QString& detail, const std::vector<QString>& ids);

private:
    Commands& commands_;
    QLabel* summary_;
    QLabel* detail_;
    QLabel* thumbnail_;
    QFrame* divider_ = nullptr;
    QHBoxLayout* buttons_;
    QHBoxLayout* removals_;
};

}
