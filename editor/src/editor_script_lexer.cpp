#include "editor_script_lexer.h"

#include "editor_theme.h"

#include "document/script_source.h"

#include <Qsci/qsciscintilla.h>

#include <QByteArray>
#include <QChar>
#include <QtGlobal>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace Editor {

namespace {

constexpr char kBuiltinLead[] = "builtin_0x";
constexpr double kPointsPerPixel = 0.75;

bool PartOfWord(char letter) {
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
           (letter >= '0' && letter <= '9') || letter == '_';
}

bool Digit(char letter) {
    return letter >= '0' && letter <= '9';
}

bool Blank(char letter) {
    return letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n';
}

bool Listed(std::span<const std::string_view> names, const QString& word) {
    const std::string text = word.toStdString();
    return std::ranges::find(names, text) != names.end();
}

bool Register(const QString& word) {
    if (word.size() < 2 || word.front() != QChar('r')) return false;
    return std::ranges::all_of(word.sliced(1), [](QChar one) { return one.isDigit(); });
}

bool Builtin(const QString& word) {
    if (!word.startsWith(QString(kBuiltinLead))) return false;
    const QString digits = word.sliced(qstrlen(kBuiltinLead));
    if (digits.isEmpty()) return false;
    return std::ranges::all_of(digits, [](QChar one) {
        return one.isDigit() || (one.toLower() >= QChar('a') && one.toLower() <= QChar('f'));
    });
}

int StyleForWord(const QString& word, bool after_verb) {
    if (Listed(Document::ScriptWords(), word)) return ScriptLexer::Word;
    if (Register(word)) return ScriptLexer::Word;
    if (Builtin(word)) return ScriptLexer::Call;
    if (after_verb) return ScriptLexer::Wrong;
    if (Listed(Document::ScriptCalls(), word)) return ScriptLexer::Call;
    if (Listed(Document::ScriptInstructions(), word)) return ScriptLexer::Call;
    return ScriptLexer::Wrong;
}

bool Opens(const QByteArray& bytes, int from) {
    int at = from;
    while (at < bytes.size() && Blank(bytes.at(at)))
        at++;
    return at < bytes.size() && bytes.at(at) == '(';
}

int StyleAfterDot(const QByteArray& bytes, int to) {
    return Opens(bytes, to) ? ScriptLexer::Call : ScriptLexer::Text;
}

bool Separates(char letter) {
    return letter == '.' || letter == '=';
}

bool Punctuates(char letter) {
    return letter == '(' || letter == ')' || letter == ',' || Separates(letter);
}

}

ScriptLexer::ScriptLexer(int size, QObject* parent) : QsciLexerCustom(parent), size_(size) {}

const char* ScriptLexer::language() const {
    return "AFP script";
}

QString ScriptLexer::description(int style) const {
    switch (style) {
    case Plain:
        return tr("Plain");
    case Call:
        return tr("Call");
    case Text:
        return tr("Text");
    case Number:
        return tr("Number");
    case Word:
        return tr("Keyword");
    case Punctuation:
        return tr("Punctuation");
    case Wrong:
        return tr("Not a word this language has");
    default:
        return {};
    }
}

QColor ScriptLexer::defaultColor(int style) const {
    switch (style) {
    case Call:
        return Theme::kDesignAccent;
    case Text:
        return Theme::kGreen;
    case Number:
        return Theme::kAmber;
    case Word:
        return Theme::kViolet;
    case Wrong:
        return Theme::kWrong;
    default:
        return Theme::kSoft;
    }
}

QColor ScriptLexer::defaultPaper(int) const {
    return Theme::kPage;
}

QFont ScriptLexer::defaultFont(int) const {
    QFont face(Theme::MonoFamily());
    face.setPointSizeF(size_ * kPointsPerPixel);
    face.setStyleHint(QFont::Monospace);
    return face;
}

void ScriptLexer::KnowNames(const QStringList& names) {
    names_ = names;
}

QList<int> ScriptLexer::StylesOf(const QString& line, const QStringList& names) {
    const QByteArray bytes = line.toUtf8();
    QList<int> styles(bytes.size(), Plain);
    bool after_verb = false;
    bool after_dot = false;
    int at = 0;
    while (at < bytes.size()) {
        const char letter = bytes.at(at);
        if (Blank(letter)) {
            at++;
            continue;
        }
        if (letter == '"') {
            int to = at + 1;
            while (to < bytes.size() && bytes.at(to) != '"')
                to++;
            const bool closed = to < bytes.size();
            if (closed) to++;
            const QString named = QString::fromUtf8(bytes.mid(at + 1, to - at - (closed ? 2 : 1)));
            const bool known = names.isEmpty() || names.contains(named);
            for (int i = at; i < to; i++)
                styles[i] = known ? Text : Wrong;
            after_dot = false;
            at = to;
            continue;
        }
        if (Digit(letter) || (letter == '-' && at + 1 < bytes.size() && Digit(bytes.at(at + 1)))) {
            int to = at + 1;
            while (to < bytes.size() && Digit(bytes.at(to)))
                to++;
            for (int i = at; i < to; i++)
                styles[i] = Number;
            after_dot = false;
            at = to;
            continue;
        }
        if (PartOfWord(letter)) {
            int to = at;
            while (to < bytes.size() && PartOfWord(bytes.at(to)))
                to++;
            const QString word = QString::fromUtf8(bytes.mid(at, to - at));
            const int style = after_dot ? StyleAfterDot(bytes, to) : StyleForWord(word, after_verb);
            after_dot = false;
            if (style != Word) after_verb = true;
            for (int i = at; i < to; i++)
                styles[i] = style;
            at = to;
            continue;
        }
        styles[at] = Punctuates(letter) ? Punctuation : Wrong;
        if (Separates(letter)) after_verb = false;
        after_dot = letter == '.';
        at++;
    }
    return styles;
}

void ScriptLexer::styleText(int start, int end) {
    if (!editor()) return;
    const QByteArray whole = editor()->text().toUtf8();
    if (whole.isEmpty()) return;
    QList<int> styles;
    styles.reserve(whole.size());
    int at = 0;
    while (at < whole.size()) {
        int stop = whole.indexOf('\n', at);
        stop = stop < 0 ? whole.size() : stop + 1;
        styles.append(StylesOf(QString::fromUtf8(whole.mid(at, stop - at)), names_));
        at = stop;
    }
    const int from = std::max(0, std::min(start, static_cast<int>(styles.size())));
    const int to = std::max(from, std::min(end, static_cast<int>(styles.size())));
    startStyling(from);
    int run = from;
    while (run < to) {
        int same = run;
        while (same < to && styles.at(same) == styles.at(run))
            same++;
        setStyling(same - run, styles.at(run));
        run = same;
    }
}

QStringList ScriptLexer::Vocabulary() {
    QStringList words;
    for (const std::string_view name : Document::ScriptCalls())
        words.append(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    for (const std::string_view name : Document::ScriptInstructions())
        words.append(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    for (const std::string_view name : Document::ScriptWords())
        words.append(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    words.sort();
    return words;
}

}
