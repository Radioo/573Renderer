#pragma once

#include <Qsci/qscilexercustom.h>

#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>

class QObject;

namespace Editor {

class ScriptLexer : public QsciLexerCustom {
    Q_OBJECT

public:
    enum Style { Plain = 0, Call, Text, Number, Word, Punctuation, Wrong, Styles };

    explicit ScriptLexer(int size, QObject* parent = nullptr);

    [[nodiscard]] const char* language() const override;
    [[nodiscard]] QString description(int style) const override;
    [[nodiscard]] QColor defaultColor(int style) const override;
    [[nodiscard]] QColor defaultPaper(int style) const override;
    [[nodiscard]] QFont defaultFont(int style) const override;
    void styleText(int start, int end) override;

    void KnowNames(const QStringList& names);

    [[nodiscard]] static QStringList Vocabulary();
    [[nodiscard]] static QList<int> StylesOf(const QString& line, const QStringList& names = {});

private:
    QStringList names_;
    int size_ = 12;
};

}
