#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include "editor_script_offers.h"

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstdint>

class QLabel;
class QPushButton;
class QsciScintilla;

namespace Editor {

class ScriptLexer;

struct ScriptName {
    QString name;
    QString detail;
    QString kind;
};

class ScriptEditor : public QWidget {
    Q_OBJECT

public:
    enum class Place : uint8_t { Docked, Window };

    explicit ScriptEditor(Place place, QWidget* parent = nullptr);
    ~ScriptEditor() override;

    void ShowScript(const QString& title, const QString& source);
    void ShowNothing(const QString& title, const QString& why);
    void KnowNames(const QList<ScriptName>& names);
    void KnowExamples(const QMap<QString, QString>& examples);
    void ShowProblem(const QString& problem);
    void Restore();
    void ClearProblems();
    void MarkProblem(int line, int column, int length);
    void ShowWritten(int bytes, bool same);

    [[nodiscard]] QString Source() const;

    [[nodiscard]] static QString Counted(const QString& source);
    [[nodiscard]] static QString Bytes(int bytes);
    [[nodiscard]] static QStringList Matches(const QStringList& from, const QString& typed);
    [[nodiscard]] static bool Humped(const QString& word, const QString& typed);
    struct Inside {
        QString name;
        int active = -1;
    };

    [[nodiscard]] static Inside CalledAround(const QString& line, int caret);
    [[nodiscard]] static QString QuotedAt(const QString& line, int caret);

Q_SIGNALS:
    void Compiled(const QString& source);
    void OpenAsked();
    void Changed(bool edited);
    void CaretAt(int line, int column);
    void NameChosen(const QString& name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Prefix {
        int line = 0;
        int from = 0;
        int to = 0;
        QString typed;
        bool quoted = false;
    };

    void Compile();
    void Revert();
    void Retitle();
    void Say(const QString& what, const QColor& ink);
    void Rethink();
    void Offer();
    void Hint();
    [[nodiscard]] QPoint Where(const Prefix& typing) const;
    [[nodiscard]] ScriptOffer OfferFor(const QString& name) const;
    [[nodiscard]] static QString KindOf(const QString& word);
    void Take(const QString& word, bool call);
    [[nodiscard]] Prefix Typing() const;

    QLabel* title_ = nullptr;
    QPushButton* compile_ = nullptr;
    QPushButton* revert_ = nullptr;
    QPushButton* place_ = nullptr;
    QLabel* counted_ = nullptr;
    QsciScintilla* area_ = nullptr;
    QLabel* status_ = nullptr;
    ScriptLexer* lexer_ = nullptr;
    ScriptOffers* offers_ = nullptr;
    QStringList words_;
    QStringList names_;
    QMap<QString, QString> details_;
    QMap<QString, QString> kinds_;
    QMap<QString, QString> examples_;
    QString shown_;
    QString what_;
    bool taking_ = false;
    bool thinking_ = false;
};

}
