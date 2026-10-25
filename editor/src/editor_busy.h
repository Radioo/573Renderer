#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

namespace Editor {

class Busy : public QWidget {
    Q_OBJECT

public:
    explicit Busy(QWidget* parent = nullptr);

    void Say(const QString& what);
    void Move(int done, int total, const QString& detail);
    void AllowStopping(bool allowed);

signals:
    void StopAsked();

private:
    QLabel* what_;
    QLabel* detail_;
    QProgressBar* bar_;
    QPushButton* stop_;
};

}
