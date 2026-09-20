#pragma once

#include <QString>
#include <QWidget>

#include <vector>

class QHBoxLayout;
class QLabel;

namespace Editor {

class Commands;

class SelectionBar : public QWidget {
    Q_OBJECT

public:
    SelectionBar(Commands& commands, QWidget* parent = nullptr);

    void Show(const QString& summary, const std::vector<QString>& ids);

private:
    Commands& commands_;
    QLabel* summary_;
    QHBoxLayout* buttons_;
};

}
