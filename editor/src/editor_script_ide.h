#pragma once

#include "document/script_index.h"

#include <QString>
#include <QStringList>
#include <QList>
#include <QSignalBlocker>
#include <QVariant>
#include <QWidget>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;

namespace Editor {

class ScriptEditor;

struct IdeName {
    QString kind;
    QString name;
    QString where;
};

struct IdeProblem {
    QString said;
    QString where;
    int line = 0;
    int column = 0;
    int length = 0;
};

class ScriptIde : public QWidget {
    Q_OBJECT

public:
    explicit ScriptIde(QWidget* parent = nullptr);

    void ShowPackage(const QString& file, const QString& animation);
    void ShowScripts(const std::vector<Document::ScriptEntry>& scripts,
                     const std::optional<Document::ScriptPlace>& chosen);
    void ShowScript(const QString& title, const QString& source, const std::vector<uint8_t>& bytes,
                    bool round_trips, const Document::ScriptPlace& place);
    void ShowNothing(const QString& title, const QString& why);
    void ShowNames(const std::vector<IdeName>& names);
    void ShowProblems(const std::vector<IdeProblem>& problems);
    void ShowBytes(const std::vector<uint8_t>& bytes);
    void ShowHistory(const QStringList& steps, int at);

Q_SIGNALS:
    void ScriptChosen(const Document::ScriptPlace& place);
    void CompileAsked(const QString& source);
    void CloseAsked();
    void NameChosen(const QString& name);

private:
    void Compile();
    void Revert();
    void Filter(const QString& text);
    void Settled(bool changed);
    void Moved(int line, int column);
    void Retell();
    [[nodiscard]] QString Crumb() const;
    void Rebar();
    void Redot();
    void Restate();
    void Retab();
    void ShowPane(int which);

    QLabel* crumb_ = nullptr;
    QLabel* bytes_ = nullptr;
    QPushButton* compile_ = nullptr;
    QPushButton* revert_ = nullptr;
    QPushButton* close_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QTreeWidget* scripts_ = nullptr;
    QWidget* tabs_ = nullptr;
    QHBoxLayout* tab_row_ = nullptr;
    ScriptEditor* editor_ = nullptr;
    QLabel* problem_count_ = nullptr;
    QListWidget* problems_ = nullptr;
    QPlainTextEdit* byte_view_ = nullptr;
    QListWidget* history_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    std::vector<QPushButton*> pane_tabs_;
    QLabel* summary_ = nullptr;
    QLabel* trip_ = nullptr;
    QLabel* object_ = nullptr;
    QLabel* caret_ = nullptr;
    QTreeWidget* names_ = nullptr;

    std::vector<Document::ScriptPlace> listed_;
    std::vector<Document::ScriptShape> shapes_;
    std::vector<QString> clips_;
    QList<QLabel*> dots_;
    QList<QWidget*> marks_;
    QLabel* tab_dot_ = nullptr;
    std::vector<std::string> known_;
    std::vector<Document::ScriptPlace> open_;
    std::vector<QString> open_names_;
    std::optional<Document::ScriptPlace> showing_;
    QString file_;
    QString animation_;
    QString title_;
    int bytes_written_ = 0;
    bool round_trips_ = false;
    bool changed_ = false;
};

}
