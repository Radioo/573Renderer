#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

class QFrame;
class QLabel;
class QListWidget;
class QVBoxLayout;

namespace Editor {

struct ScriptOffer {
    QString name;
    QString detail;
    QString kind;
    QString signature;
    QString said;
    QString returns;
    QStringList params;
    QString note;
    QString example;
    bool call = false;
};

class ScriptOffers : public QWidget {
    Q_OBJECT

public:
    explicit ScriptOffers(QWidget* parent);

    void Offer(const QList<ScriptOffer>& offers, const QString& counted, QPoint at);
    void Hint(const ScriptOffer& offer, int active, QPoint at);
    void Put();
    void Step(int by);
    void Take();

    [[nodiscard]] bool Offering() const;
    [[nodiscard]] bool Hinting() const;
    [[nodiscard]] QString Chosen() const;
    [[nodiscard]] bool ChosenIsCall() const;

Q_SIGNALS:
    void Took(const QString& name, bool call);

private:
    void Explain(int active);
    void Fit();

    QFrame* box_ = nullptr;
    QFrame* help_ = nullptr;
    QListWidget* rows_ = nullptr;
    QLabel* counted_ = nullptr;
    QLabel* named_ = nullptr;
    QLabel* said_ = nullptr;
    QLabel* takes_ = nullptr;
    QLabel* gives_ = nullptr;
    QLabel* seen_ = nullptr;
    QLabel* note_ = nullptr;
    QVBoxLayout* helping_ = nullptr;
    QList<ScriptOffer> offers_;
    bool hinting_ = false;
};

}
