#pragma once

#include <QString>
#include <QWidget>

class QVBoxLayout;

namespace Editor {

class Notices : public QWidget {
    Q_OBJECT

public:
    explicit Notices(QWidget* parent = nullptr);

    void Say(const QString& text, bool undoable);

signals:
    void UndoAsked();

private:
    void Remove(QWidget* notice);

    QVBoxLayout* stack_;
};

}
