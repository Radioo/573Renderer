#pragma once

#include <QFrame>
#include <QString>

#include <functional>
#include <vector>

class QDoubleSpinBox;
class QLabel;
class QVBoxLayout;

namespace Editor {

struct PopoverField {
    QString label;
    double value = 0;
    double lowest = 0;
    double highest = 0;
    int decimals = 0;
    QString suffix;
};

using PopoverValues = std::vector<double>;

struct PopoverAsk {
    QString title;
    QString apply;
    std::vector<PopoverField> fields;
    std::function<QString(const PopoverValues&)> describe;
    std::function<void(const PopoverValues&)> preview;
    std::function<void(const PopoverValues&)> run;
    std::function<void()> cancelled;
};

class Popover : public QFrame {
    Q_OBJECT

public:
    explicit Popover(QWidget* owner);

    void Ask(PopoverAsk ask, QWidget* anchor);

protected:
    void hideEvent(QHideEvent* event) override;

private:
    [[nodiscard]] PopoverValues Values() const;
    void Changed();

    QWidget* owner_;
    QLabel* title_;
    QLabel* detail_;
    QVBoxLayout* rows_;
    QWidget* buttons_;
    std::vector<QDoubleSpinBox*> boxes_;
    PopoverAsk ask_;
    bool applying_ = false;
};

}
