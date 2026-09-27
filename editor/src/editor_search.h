#pragma once

#include <QFrame>
#include <QString>

#include <cstddef>
#include <functional>
#include <vector>

class QEvent;
class QLineEdit;
class QTreeWidget;

namespace Editor {

struct SearchItem {
    QString category;
    QString text;
    QString detail;
    QString keys;
    bool available = true;
    std::function<void()> run;
};

class CommandSearch : public QFrame {
    Q_OBJECT

public:
    explicit CommandSearch(QWidget* owner);

    void Open(std::vector<SearchItem> items);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void Filter(const QString& query);
    void Step(int by);
    void RunCurrent();

    QWidget* owner_;
    QLineEdit* query_;
    QTreeWidget* results_;
    std::vector<SearchItem> items_;
    std::vector<std::size_t> shown_;
};

}
