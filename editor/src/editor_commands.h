#pragma once

#include <QKeySequence>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QAction;
class QMenu;
class QWidget;

namespace Editor {

using Refusal = std::function<std::optional<QString>()>;

struct Command {
    QString id;
    QString text;
    QString brief;
    QKeySequence keys;
    std::function<void()> run;
    Refusal refusal;
    QWidget* scope = nullptr;
};

struct Toggle {
    QString id;
    QString text;
    QKeySequence keys;
    bool checked = false;
    std::function<void(bool)> set;
};

class Commands : public QObject {
    Q_OBJECT

public:
    explicit Commands(QWidget* window);

    QAction* Add(Command command);
    QAction* AddToggle(Toggle toggle);
    void ShowAvailabilityIn(QMenu* menu);

    [[nodiscard]] QAction* Action(const QString& id) const;
    [[nodiscard]] QString Brief(const QString& id) const;
    [[nodiscard]] std::optional<QString> Refusal(const QString& id) const;
    [[nodiscard]] std::vector<QString> Ids() const;
    bool Run(const QString& id);

signals:
    void Refused(const QString& reason);

private:
    struct Entry {
        QString id;
        QString brief;
        QAction* action = nullptr;
        std::function<void()> run;
        Editor::Refusal refusal;
    };

    [[nodiscard]] const Entry* Find(const QString& id) const;
    [[nodiscard]] const Entry* FindAction(const QAction* action) const;

    QWidget* window_;
    std::vector<Entry> entries_;
};

}
