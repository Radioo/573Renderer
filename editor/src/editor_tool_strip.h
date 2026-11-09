#pragma once

#include <QString>
#include <QWidget>

namespace Editor {

class Commands;

class ToolStrip : public QWidget {
    Q_OBJECT

public:
    ToolStrip(Commands& commands, QWidget* parent = nullptr);

    void Build();

private:
    Commands& commands_;
};

}
