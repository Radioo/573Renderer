#include "editor_script_editor.h"

#include "editor_icons.h"
#include "editor_script_lexer.h"
#include "editor_script_offers.h"
#include "editor_theme.h"

#include "document/script_docs.h"
#include "document/script_source.h"

#include <Qsci/qsciscintilla.h>

#include <QChar>
#include <QColor>
#include <QEvent>
#include <QMouseEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QGuiApplication>
#include <QPoint>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace Editor {

namespace {

constexpr int kRowHeight = 30;
constexpr int kGutterDigits = 3;
constexpr int kTabWidth = 4;
constexpr int kWrongName = 0;
constexpr int kWrongLine = 0;
constexpr int kMarkerMargin = 1;
constexpr int kMarkerWidth = 16;
constexpr int kLeastTyped = 1;
constexpr int kWindowText = 13;
constexpr int kDockedText = 12;
constexpr int kWindowLine = 22;
constexpr int kDockedLine = 20;
constexpr int kPlaceHeight = 20;

bool PartOfWord(QChar letter) {
    return letter.isLetterOrNumber() || letter == QChar('_');
}

void Space(QsciScintilla& area, int wanted) {
    const int natural = static_cast<int>(area.SendScintilla(QsciScintilla::SCI_TEXTHEIGHT, 0UL));
    const int room = std::max(0, wanted - natural);
    area.SendScintilla(QsciScintilla::SCI_SETEXTRAASCENT, static_cast<long>((room + 1) / 2));
    area.SendScintilla(QsciScintilla::SCI_SETEXTRADESCENT, static_cast<long>(room / 2));
}

}

ScriptEditor::ScriptEditor(Place place, QWidget* parent) : QWidget(parent) {
    const QString lead = place == Place::Window ? QString("script_ide") : QString("script");
    const bool bare = place == Place::Window;
    setObjectName(lead + "_editor");

    title_ = new QLabel(this);
    title_->setObjectName(lead + "_title");
    title_->setStyleSheet(QString("color: %1; font-weight: 600;").arg(Theme::kText.name()));

    compile_ = new QPushButton(tr("Compile"), this);
    compile_->setObjectName(lead + "_compile");
    compile_->setFixedHeight(Theme::kControlHeight);
    compile_->setEnabled(false);
    compile_->setToolTip(tr("Write this script back into the animation (Ctrl+Return)"));
    compile_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 0; "
                                    "font-weight: 600; } QPushButton:disabled { background: %3; "
                                    "color: %4; }")
                                .arg(Theme::Accent().name(), Theme::OnAccent().name(),
                                     Theme::kPanel.name(), Theme::kFaint.name()));

    revert_ = new QPushButton(tr("Revert"), this);
    revert_->setObjectName(lead + "_revert");
    revert_->setFixedHeight(Theme::kControlHeight);
    revert_->setEnabled(false);
    revert_->setToolTip(tr("Put back the script the animation holds"));

    place_ = new QPushButton(tr("Open full"), this);
    place_->setObjectName(lead + "_place");
    place_->setFixedHeight(kPlaceHeight);
    place_->setStyleSheet(QString("QPushButton { background: transparent; border: 1px solid %1; "
                                  "color: %2; font-size: 10px; padding: 0 7px; }")
                              .arg(Theme::kEdge.name(), Theme::kSoft.name()));
    place_->setToolTip(tr("Open this script in the script editor, where there is room for it"));
    connect(place_, &QPushButton::clicked, this, &ScriptEditor::OpenAsked);

    counted_ = new QLabel(this);
    counted_->setObjectName(lead + "_counted");
    counted_->setStyleSheet(QString("color: %1; font-family: %2; font-size: 10px;")
                                .arg(Theme::kFaint.name(), Theme::MonoFamily()));

    auto* heading = new QHBoxLayout;
    heading->setContentsMargins(0, 0, 0, 0);
    heading->setSpacing(8);
    heading->addWidget(title_, 1);
    heading->addWidget(counted_, 0);
    heading->addWidget(place_, 0);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(7);
    buttons->addWidget(compile_, 1);
    buttons->addWidget(revert_, 0);

    area_ = new QsciScintilla(this);
    area_->setObjectName(lead + "_text");
    lexer_ = new ScriptLexer(bare ? kWindowText : kDockedText, area_);
    area_->setLexer(lexer_);

    const QFont mono = lexer_->defaultFont(ScriptLexer::Plain);
    area_->setUtf8(true);
    area_->viewport()->installEventFilter(this);
    area_->installEventFilter(this);
    area_->setEolMode(QsciScintilla::EolUnix);
    area_->setMarginsFont(mono);
    area_->setMarginType(0, QsciScintilla::NumberMargin);
    area_->setMarginWidth(0, QString(kGutterDigits, QChar('0')));
    area_->setMarginLineNumbers(0, true);
    area_->setMarginsBackgroundColor(Theme::kCodeMargin);
    area_->setMarginsForegroundColor(Theme::kFaint);
    area_->setIndentationsUseTabs(false);
    area_->setTabWidth(kTabWidth);
    area_->setAutoIndent(true);
    area_->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    area_->setMatchedBraceForegroundColor(Theme::kText);
    area_->setMatchedBraceBackgroundColor(Theme::kLine);
    area_->setUnmatchedBraceForegroundColor(Theme::kWrong);
    area_->setUnmatchedBraceBackgroundColor(Theme::kPage);
    area_->setCaretForegroundColor(Theme::kText);
    area_->setCaretLineVisible(true);
    area_->setCaretLineBackgroundColor(Theme::kCodeLine);
    area_->setSelectionBackgroundColor(Theme::Chosen());
    area_->setSelectionForegroundColor(Theme::OnChosen());
    area_->setWrapMode(QsciScintilla::WrapWord);
    area_->setWrapVisualFlags(QsciScintilla::WrapFlagByText);
    area_->setWrapIndentMode(QsciScintilla::WrapIndentIndented);
    area_->setAutoCompletionSource(QsciScintilla::AcsNone);
    area_->setCallTipsStyle(QsciScintilla::CallTipsNoContext);
    area_->setCallTipsBackgroundColor(Theme::kCodeMargin);
    area_->setCallTipsForegroundColor(Theme::kSoft);
    area_->setCallTipsHighlightColor(Theme::kGreen);
    area_->setMarginType(kMarkerMargin, QsciScintilla::SymbolMargin);
    area_->setMarginWidth(kMarkerMargin, bare ? kMarkerWidth : 0);
    area_->setMarginSensitivity(kMarkerMargin, false);
    area_->markerDefine(Icons::Of(Icons::Glyph::Warning, Theme::kWrong, kMarkerWidth)
                            .pixmap(kMarkerWidth, kMarkerWidth),
                        kWrongLine);
    area_->indicatorDefine(QsciScintilla::DotsIndicator, kWrongName);
    area_->setIndicatorForegroundColor(Theme::kWrong, kWrongName);
    area_->setFrameShape(QFrame::NoFrame);
    area_->setStyleSheet(
        QString("QsciScintilla { border: 1px solid %1; }").arg(Theme::kLine.name()));

    Space(*area_, bare ? kWindowLine : kDockedLine);

    status_ = new QLabel(this);
    status_->setObjectName(lead + "_status");
    status_->setWordWrap(true);
    status_->hide();

    QVBoxLayout* stack = nullptr;

    stack = new QVBoxLayout(this);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(7);
    stack->addLayout(heading);
    stack->addWidget(area_, 1);
    stack->addLayout(buttons);
    stack->addWidget(status_, 0);

    words_ = ScriptLexer::Vocabulary();
    offers_ = new ScriptOffers(this);

    connect(compile_, &QPushButton::clicked, this, &ScriptEditor::Compile);
    connect(revert_, &QPushButton::clicked, this, &ScriptEditor::Revert);
    connect(area_, &QsciScintilla::textChanged, this, &ScriptEditor::Retitle);
    connect(area_, &QsciScintilla::cursorPositionChanged, this,
            [this](int line, int index) { Q_EMIT CaretAt(line + 1, index + 1); });
    connect(area_, &QsciScintilla::textChanged, this, [this] { Rethink(); });
    connect(area_, &QsciScintilla::cursorPositionChanged, this, [this](int, int) { Rethink(); });
    connect(offers_, &ScriptOffers::Took, this,
            [this](const QString& word, bool call) { Take(word, call); });

    auto* quickly = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    quickly->setContext(Qt::WidgetWithChildrenShortcut);
    connect(quickly, &QShortcut::activated, this, &ScriptEditor::Compile);

    auto* asked = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space), this);
    asked->setContext(Qt::WidgetWithChildrenShortcut);
    connect(asked, &QShortcut::activated, this, &ScriptEditor::Offer);

    title_->setVisible(!bare);
    counted_->setVisible(!bare);
    place_->setVisible(!bare);
    compile_->setVisible(!bare);
    revert_->setVisible(!bare);
    if (bare) stack->setSpacing(0);
    setMinimumHeight(kRowHeight * (place == Place::Docked ? 6 : 12));
}

ScriptEditor::~ScriptEditor() {
    area_->setLexer(nullptr);
}

void ScriptEditor::ShowScript(const QString& title, const QString& source) {
    what_ = title;
    shown_ = source;
    taking_ = true;
    area_->setReadOnly(false);
    area_->setText(source);
    area_->setModified(false);
    taking_ = false;
    ShowProblem({});
    Retitle();
}

void ScriptEditor::ShowNothing(const QString& title, const QString& why) {
    what_ = title;
    shown_.clear();
    taking_ = true;
    area_->setText({});
    area_->setReadOnly(true);
    taking_ = false;
    ShowProblem(why);
    Retitle();
}

void ScriptEditor::KnowNames(const QList<ScriptName>& names) {
    names_.clear();
    details_.clear();
    kinds_.clear();
    QStringList plain;
    for (const ScriptName& one : names) {
        plain.append(one.name);
        names_.append(one.name);
        details_.insert(one.name, one.detail);
        kinds_.insert(one.name, one.kind.isEmpty() ? QString("L") : one.kind);
    }
    lexer_->KnowNames(plain);
    area_->recolor();
}

void ScriptEditor::KnowExamples(const QMap<QString, QString>& examples) {
    examples_ = examples;
}

QString ScriptEditor::QuotedAt(const QString& line, int caret) {
    if (caret < 0 || caret > line.size()) return {};
    qsizetype at = 0;
    while (at < line.size()) {
        const qsizetype open = line.indexOf(QChar('"'), at);
        if (open < 0) return {};
        const qsizetype close = line.indexOf(QChar('"'), open + 1);
        if (close < 0) return {};
        if (caret > open && caret <= close) return line.sliced(open + 1, close - open - 1);
        at = close + 1;
    }
    return {};
}

QString ScriptEditor::KindOf(const QString& word) {
    const std::string named = word.toStdString();
    for (const std::string_view held : Document::ScriptWords()) {
        if (held == named) return QString("k");
    }
    for (const std::string_view held : Document::ScriptInstructions()) {
        if (held == named) return QString("k");
    }
    return QString("f");
}

ScriptEditor::Inside ScriptEditor::CalledAround(const QString& line, int caret) {
    Inside inside;
    if (caret < 0 || caret > line.size()) return inside;
    int depth = 0;
    int commas = 0;
    bool quoted = false;
    int open = -1;
    for (int at = 0; at < caret; at++) {
        const QChar letter = line.at(at);
        if (letter == QChar('"')) quoted = !quoted;
        if (quoted) continue;
        if (letter == QChar('(')) {
            depth++;
            if (depth == 1) {
                open = at;
                commas = 0;
            }
            continue;
        }
        if (letter == QChar(')')) {
            depth--;
            if (depth <= 0) open = -1;
            continue;
        }
        if (letter == QChar(',') && depth == 1) commas++;
    }
    if (open <= 0) return inside;
    int from = open;
    while (from > 0 && PartOfWord(line.at(from - 1)))
        from--;
    inside.name = line.sliced(from, open - from);
    inside.active = commas;
    return inside;
}

bool ScriptEditor::eventFilter(QObject* watched, QEvent* event) {
    if (watched == area_ && event->type() == QEvent::KeyPress && offers_->Offering()) {
        auto* typed = static_cast<QKeyEvent*>(event);
        switch (typed->key()) {
        case Qt::Key_Down:
            offers_->Step(1);
            return true;
        case Qt::Key_Up:
            offers_->Step(-1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            offers_->Take();
            return true;
        case Qt::Key_Escape:
            offers_->Put();
            return true;
        default:
            break;
        }
    }
    if (watched != area_->viewport() || event->type() != QEvent::MouseButtonPress)
        return QWidget::eventFilter(watched, event);
    auto* click = static_cast<QMouseEvent*>(event);
    if ((click->modifiers() & Qt::ControlModifier) == 0)
        return QWidget::eventFilter(watched, event);
    const int caret = static_cast<int>(area_->SendScintilla(
        QsciScintillaBase::SCI_POSITIONFROMPOINT, static_cast<unsigned long>(click->pos().x()),
        static_cast<long>(click->pos().y())));
    int line = 0;
    int index = 0;
    area_->lineIndexFromPosition(caret, &line, &index);
    const QString named = QuotedAt(area_->text(line), index);
    if (named.isEmpty() || !names_.contains(named)) return QWidget::eventFilter(watched, event);
    Q_EMIT NameChosen(named);
    return true;
}

void ScriptEditor::Take(const QString& word, bool call) {
    const Prefix typing = Typing();
    const QString line = area_->text(typing.line);
    const bool already = typing.to < line.size() && line.at(typing.to) == QChar('(');
    const bool brackets = call && !typing.quoted && !already;
    taking_ = true;
    area_->setSelection(typing.line, typing.from, typing.line, typing.to);
    area_->replaceSelectedText(brackets ? word + "()" : word);
    if (brackets) {
        const std::optional<Document::ScriptDoc> doc = Document::ScriptWordDoc(word.toStdString());
        const bool empty = doc && doc->measured && doc->params.empty();
        area_->setCursorPosition(typing.line,
                                 typing.from + static_cast<int>(word.size()) + (empty ? 2 : 1));
    }
    taking_ = false;
    area_->setFocus();
    Retitle();
    Rethink();
}

void ScriptEditor::Hint() {
    if (area_->isReadOnly()) {
        offers_->Put();
        return;
    }
    const Prefix typing = Typing();
    const Inside inside = CalledAround(area_->text(typing.line), typing.to);
    if (inside.name.isEmpty()) {
        offers_->Put();
        return;
    }
    const ScriptOffer offer = OfferFor(inside.name);
    if (offer.said.isEmpty()) {
        offers_->Put();
        return;
    }
    offers_->Hint(offer, inside.active, Where(typing));
}

void ScriptEditor::ShowProblem(const QString& problem) {
    Say(problem, Theme::kWrong);
}

void ScriptEditor::ShowWritten(int bytes, bool same) {
    Say(same ? tr("Compiled, %1, identical to the original").arg(Bytes(bytes))
             : tr("Compiled, %1, written into the animation").arg(Bytes(bytes)),
        Theme::kGreen);
}

QString ScriptEditor::Counted(const QString& source) {
    const int shown = static_cast<int>(source.split(QChar('\n'), Qt::SkipEmptyParts).size());
    if (Document::ScriptIsCalls(source.toStdString()))
        return shown == 1 ? tr("%1 call").arg(shown) : tr("%1 calls").arg(shown);
    return shown == 1 ? tr("%1 line").arg(shown) : tr("%1 lines").arg(shown);
}

QString ScriptEditor::Bytes(int bytes) {
    return bytes == 1 ? tr("%1 byte").arg(bytes) : tr("%1 bytes").arg(bytes);
}

void ScriptEditor::Say(const QString& what, const QColor& ink) {
    status_->setStyleSheet(QString("color: %1;").arg(ink.name()));
    status_->setText(what);
    status_->setVisible(!what.isEmpty());
}

void ScriptEditor::Revert() {
    Restore();
}

void ScriptEditor::ClearProblems() {
    area_->markerDeleteAll(kWrongLine);
    area_->clearIndicatorRange(0, 0, area_->lines(), 0, kWrongName);
}

void ScriptEditor::MarkProblem(int line, int column, int length) {
    const int at = line - 1;
    if (at < 0 || at >= area_->lines()) return;
    area_->markerAdd(at, kWrongLine);
    area_->fillIndicatorRange(at, column - 1, at, column - 1 + length, kWrongName);
}

void ScriptEditor::Restore() {
    ShowScript(what_, shown_);
}

QString ScriptEditor::Source() const {
    return area_->text();
}

void ScriptEditor::Compile() {
    if (area_->isReadOnly() || area_->text() == shown_) return;
    Q_EMIT Compiled(Source());
}

void ScriptEditor::Retitle() {
    const bool changed = !area_->isReadOnly() && area_->text() != shown_;
    compile_->setEnabled(changed);
    revert_->setEnabled(changed);
    title_->setText(changed ? tr("%1, edited").arg(what_) : what_);
    counted_->setText(Counted(area_->text()));
    Q_EMIT Changed(changed);
}

ScriptEditor::Prefix ScriptEditor::Typing() const {
    Prefix typing;
    int index = 0;
    area_->getCursorPosition(&typing.line, &index);
    const QString line = area_->text(typing.line);
    typing.to = std::min(index, static_cast<int>(line.size()));
    typing.from = typing.to;
    while (typing.from > 0 && PartOfWord(line.at(typing.from - 1)))
        typing.from--;
    typing.typed = line.mid(typing.from, typing.to - typing.from);
    int quotes = 0;
    for (int at = 0; at < typing.from; at++) {
        if (line.at(at) == QChar('"')) quotes++;
    }
    typing.quoted = quotes % 2 == 1;
    return typing;
}

void ScriptEditor::Rethink() {
    if (thinking_) return;
    thinking_ = true;
    QTimer::singleShot(0, this, [this] {
        thinking_ = false;
        Offer();
    });
}

ScriptOffer ScriptEditor::OfferFor(const QString& name) const {
    const std::optional<Document::ScriptDoc> doc = Document::ScriptWordDoc(name.toStdString());
    if (!doc) {
        return ScriptOffer{.name = name,
                           .detail = details_.value(name),
                           .kind = kinds_.value(name, QString("L")),
                           .signature = name,
                           .said = details_.value(name).isEmpty()
                                       ? tr("A name the open package holds. A script points at it "
                                            "by writing it in quotes.")
                                       : tr("%1. A script points at it by writing it in quotes.")
                                             .arg(details_.value(name)),
                           .returns = {},
                           .params = {},
                           .note = details_.value(name),
                           .example = {},
                           .call = false};
    }

    QStringList params;
    for (const Document::ScriptParam& one : doc->params) {
        params.append(QString("<b>%1</b> <span style=\"color:%2\">%3</span><br>%4")
                          .arg(QString::fromStdString(one.name), Theme::kFaint.name(),
                               QString::fromStdString(one.type), QString::fromStdString(one.said)));
    }
    const QString signature = QString::fromStdString(doc->signature);
    const qsizetype open = signature.indexOf(QChar('('));
    return ScriptOffer{.name = name,
                       .detail =
                           open > 0 ? signature.sliced(open) : QString::fromStdString(doc->kind),
                       .kind = doc->call ? QString("f") : QString("k"),
                       .signature = signature,
                       .said = QString::fromStdString(doc->summary),
                       .returns = QString::fromStdString(doc->returns),
                       .params = params,
                       .note = QString::fromStdString(doc->id),
                       .example = examples_.value(name),
                       .call = doc->call || signature.contains(QChar('('))};
}

void ScriptEditor::Offer() {
    const Prefix typing = Typing();
    const QStringList from = typing.quoted ? names_ : words_;
    const QStringList offered = taking_ || area_->isReadOnly() || typing.typed.size() < kLeastTyped
                                    ? QStringList()
                                    : Matches(from, typing.typed);
    if (offered.isEmpty()) {
        Hint();
        return;
    }

    QList<ScriptOffer> shown;
    shown.reserve(offered.size());
    for (const QString& name : offered)
        shown.append(OfferFor(name));
    const QString counted = typing.quoted ? tr("%1 of %2 names in this package start with %3")
                                                .arg(offered.size())
                                                .arg(names_.size())
                                                .arg(typing.typed)
                                          : tr("%1 of %2 words this language knows match %3")
                                                .arg(offered.size())
                                                .arg(words_.size())
                                                .arg(typing.typed);
    offers_->Offer(shown, counted, Where(typing));
}

QPoint ScriptEditor::Where(const Prefix& typing) const {
    const int at = area_->positionFromLineIndex(typing.line, typing.from);
    const int x = static_cast<int>(
        area_->SendScintilla(QsciScintilla::SCI_POINTXFROMPOSITION, 0UL, static_cast<long>(at)));
    const int y = static_cast<int>(
        area_->SendScintilla(QsciScintilla::SCI_POINTYFROMPOSITION, 0UL, static_cast<long>(at)));
    const int high = static_cast<int>(area_->SendScintilla(QsciScintilla::SCI_TEXTHEIGHT, 0UL));
    const QPoint under = area_->viewport()->mapTo(this, QPoint(x, y + high));
    const int room = std::max(0, width() - offers_->width());
    return QPoint(std::clamp(under.x(), 0, room), under.y());
}

bool ScriptEditor::Humped(const QString& word, const QString& typed) {
    int at = 0;
    for (int which = 0; which < word.size() && at < typed.size(); which++) {
        const QChar letter = word.at(which);
        const bool starts = which == 0 || letter.isUpper() || word.at(which - 1) == QChar('_');
        if (starts && letter.toLower() == typed.at(at).toLower()) at++;
    }
    return at == typed.size();
}

QStringList ScriptEditor::Matches(const QStringList& from, const QString& typed) {
    QStringList started;
    QStringList humped;
    for (const QString& word : from) {
        if (word == typed) continue;
        if (word.startsWith(typed, Qt::CaseInsensitive)) {
            started.append(word);
            continue;
        }
        if (typed.size() > 1 && Humped(word, typed)) humped.append(word);
    }
    const auto sorted = [](QStringList& words) {
        std::ranges::sort(words, [](const QString& one, const QString& other) {
            return one.compare(other, Qt::CaseInsensitive) < 0;
        });
    };
    sorted(started);
    sorted(humped);
    return started + humped;
}

}
