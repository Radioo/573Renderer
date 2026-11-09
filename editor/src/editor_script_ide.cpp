#include "editor_script_ide.h"

#include "editor_icons.h"
#include "editor_script_editor.h"
#include "editor_theme.h"

#include "document/script_problems.h"

#include <QFont>
#include <QSize>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLayoutItem>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace Editor {

namespace {

constexpr int kTopBar = 44;
constexpr int kStripHeight = 30;
constexpr int kStatusHeight = 26;
constexpr int kSideWidth = 274;
constexpr int kNamesWidth = 268;
constexpr int kProblemsHeight = 168;
constexpr int kBadgeSide = 22;
constexpr int kDotSide = 6;
constexpr int kScriptRow = 40;
constexpr int kProblemRow = 56;
constexpr int kTabDot = 5;
constexpr int kNameBadge = 13;
constexpr int kFrameColumn = 52;
constexpr int kBytesPerRow = 16;
constexpr int kTabHeight = 32;
constexpr int kHexBase = 16;

QString Faint(int size, bool spaced) {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: %3px;")
        .arg(Theme::kFaint.name())
        .arg(size)
        .arg(spaced ? 1 : 0);
}

QLabel* Heading(const QString& text) {
    auto* head = new QLabel(text);
    head->setFixedHeight(kStripHeight);
    head->setContentsMargins(12, 0, 12, 0);
    head->setStyleSheet(Faint(10, true));
    return head;
}

QWidget* Rule(bool upright) {
    auto* line = new QFrame;
    line->setFrameShape(upright ? QFrame::VLine : QFrame::HLine);
    line->setFixedWidth(upright ? 1 : QWIDGETSIZE_MAX);
    line->setFixedHeight(upright ? QWIDGETSIZE_MAX : 1);
    line->setStyleSheet(QString("background: %1; border: 0;").arg(Theme::kLine.name()));
    return line;
}

void Paint(QLabel* dot, const QColor& ink) {
    const int side = dot->width();
    dot->setStyleSheet(
        QString("background: %1; border-radius: %2px;").arg(ink.name()).arg(side / 2));
}

QLabel* Dot(const QColor& ink, int side = kDotSide) {
    auto* dot = new QLabel;
    dot->setFixedSize(side, side);
    Paint(dot, ink);
    return dot;
}

QString Coloured(const QString& said) {
    QString out;
    bool first = true;
    int at = 0;
    while (at < said.size()) {
        const int open = static_cast<int>(said.indexOf(QChar('"'), at));
        if (open < 0) break;
        const int close = static_cast<int>(said.indexOf(QChar('"'), open + 1));
        if (close < 0) break;
        out += said.mid(at, open - at).toHtmlEscaped();
        out += QString("<span style=\"color:%1;font-family:%2\">%3</span>")
                   .arg(first ? Theme::kWrong.name() : Theme::kGreen.name(), Theme::MonoFamily(),
                        said.mid(open + 1, close - open - 1).toHtmlEscaped());
        first = false;
        at = close + 1;
    }
    return out + said.mid(at).toHtmlEscaped();
}

QWidget* Problem(const IdeProblem& one) {
    auto* row = new QWidget;
    row->setStyleSheet(QString("background: transparent;"));
    auto* stack = new QVBoxLayout(row);
    stack->setContentsMargins(14, 11, 14, 11);
    stack->setSpacing(2);

    auto* said = new QLabel(Coloured(one.said));
    said->setObjectName("ide_problem_said");
    said->setWordWrap(true);
    said->setStyleSheet(
        QString("background: transparent; color: %1; font-size: 12px;").arg(Theme::kText.name()));

    auto* where = new QLabel(one.where);
    where->setObjectName("ide_problem_where");
    where->setStyleSheet(QString("background: transparent; color: %1; font-family: %2; "
                                 "font-size: 11px;")
                             .arg(Theme::kFaint.name(), Theme::MonoFamily()));
    where->setVisible(!one.where.isEmpty());

    stack->addWidget(said, 0);
    stack->addWidget(where, 0);
    return row;
}

QColor InkFor(Document::ScriptShape shape) {
    if (shape == Document::ScriptShape::Instructions) return Theme::kViolet;
    if (shape == Document::ScriptShape::Unreadable) return Theme::kWrong;
    return Theme::Accent();
}

QTreeWidget* Rows(const QString& name) {
    auto* rows = new QTreeWidget;
    rows->setObjectName(name);
    rows->setColumnCount(2);
    rows->setHeaderHidden(true);
    rows->setRootIsDecorated(false);
    rows->setIndentation(0);
    rows->setUniformRowHeights(false);
    rows->setSelectionMode(QAbstractItemView::SingleSelection);
    rows->setFocusPolicy(Qt::NoFocus);
    rows->header()->setStretchLastSection(true);
    rows->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    rows->setColumnWidth(0, kFrameColumn);
    return rows;
}

QTreeWidgetItem* Group(QTreeWidget* rows, const QString& name, bool ruled) {
    auto* head = new QTreeWidgetItem(rows);
    head->setFirstColumnSpanned(true);
    head->setFlags(Qt::NoItemFlags);
    head->setSizeHint(0, QSize(0, ruled ? 30 : 26));

    auto* said = new QLabel(name);
    said->setContentsMargins(12, ruled ? 10 : 6, 12, 4);
    said->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    said->setStyleSheet(
        QString("background: transparent; color: %1; font-size: 10px; font-weight: 700; "
                "letter-spacing: 1px; %2")
            .arg(Theme::kFaint.name(),
                 ruled ? QString("border-top: 1px solid %1;").arg(Theme::kLine.name())
                       : QString()));
    rows->setItemWidget(head, 0, said);
    return head;
}

QColor InkForKind(const QString& kind) {
    if (kind == QString("C")) return Theme::kViolet;
    if (kind == QString("f")) return Theme::Accent();
    return Theme::kGreen;
}

QLabel* Badge(const QString& letter, const QColor& fill) {
    auto* badge = new QLabel(letter);
    badge->setFixedSize(kNameBadge, kNameBadge);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QString("background: %1; color: %2; font-size: 8px; font-weight: 700;")
                             .arg(fill.name(), Theme::kInk.name()));
    return badge;
}

QWidget* Named(const IdeName& one) {
    auto* row = new QWidget;
    row->setStyleSheet(QString("background: transparent;"));
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(12, 4, 12, 4);
    line->setSpacing(8);

    auto* name = new QLabel(one.name);
    name->setStyleSheet(QString("background: transparent; color: %1; font-family: %2; "
                                "font-size: 11px;")
                            .arg(Theme::kSoft.name(), Theme::MonoFamily()));

    auto* where = new QLabel(one.where);
    where->setStyleSheet(
        QString("background: transparent; color: %1; font-size: 10px;").arg(Theme::kFaint.name()));

    line->addWidget(Badge(one.kind, InkForKind(one.kind)), 0);
    line->addWidget(name, 0);
    line->addStretch(1);
    line->addWidget(where, 0);
    return row;
}

}

ScriptIde::ScriptIde(QWidget* parent) : QWidget(parent) {
    setObjectName("script_ide");
    setStyleSheet(QString("background: %1;").arg(Theme::kPage.name()));

    auto* badge = new QLabel("IFS");
    badge->setFixedSize(kBadgeSide, kBadgeSide);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QString("background: %1; color: %2; font-size: 10px; font-weight: 700; "
                                 "letter-spacing: 0.5px; border-radius: 4px;")
                             .arg(Theme::Accent().name(), Theme::OnAccent().name()));

    crumb_ = new QLabel;
    crumb_->setObjectName("ide_crumb");
    crumb_->setStyleSheet(QString("color: %1;").arg(Theme::kFaint.name()));

    bytes_ = new QLabel;
    bytes_->setObjectName("ide_bytes");
    bytes_->setStyleSheet(QString("color: %1; font-family: %2; font-size: 11px;")
                              .arg(Theme::kFaint.name(), Theme::MonoFamily()));

    revert_ = new QPushButton(tr("Revert"));
    revert_->setObjectName("ide_revert");
    revert_->setFixedHeight(Theme::kControlHeight);
    revert_->setEnabled(false);

    compile_ = new QPushButton(tr("Compile"));
    compile_->setObjectName("ide_compile");
    compile_->setFixedHeight(Theme::kControlHeight);
    compile_->setIcon(Icons::Of(Icons::Glyph::Play, Theme::OnAccent()));
    compile_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 0; font-weight: 600; "
                "text-align: left; padding: 0 12px; } "
                "QPushButton:disabled { background: %3; color: %4; }")
            .arg(Theme::Accent().name(), Theme::OnAccent().name(), Theme::kPanel.name(),
                 Theme::kFaint.name()));

    auto* quick = new QLabel("Ctrl+B", compile_);
    quick->setObjectName("ide_compile_key");
    quick->setStyleSheet(QString("background: transparent; color: %1; font-family: %2; "
                                 "font-size: 10px; font-weight: 500;")
                             .arg(Theme::QuietOnAccent().name(), Theme::MonoFamily()));
    auto* keyed = new QHBoxLayout(compile_);
    keyed->setContentsMargins(0, 0, 12, 0);
    keyed->addStretch(1);
    keyed->addWidget(quick, 0);
    compile_->setMinimumWidth(compile_->sizeHint().width() + quick->sizeHint().width() + 18);

    close_ = new QPushButton(tr("Close"));
    close_->setObjectName("ide_close");
    close_->setFixedHeight(Theme::kControlHeight);
    connect(close_, &QPushButton::clicked, this, &ScriptIde::CloseAsked);

    auto* bar = new QWidget;
    bar->setFixedHeight(kTopBar);
    bar->setStyleSheet(QString("background: %1;").arg(Theme::kPanel.name()));
    auto* bar_row = new QHBoxLayout(bar);
    bar_row->setContentsMargins(14, 0, 14, 0);
    bar_row->setSpacing(14);
    bar_row->addWidget(badge);
    bar_row->addWidget(crumb_);
    bar_row->addStretch(1);
    bar_row->addWidget(bytes_);
    bar_row->addWidget(revert_);
    bar_row->addWidget(compile_);
    bar_row->addWidget(close_);

    filter_ = new QLineEdit;
    filter_->setObjectName("ide_filter");
    filter_->setClearButtonEnabled(true);
    filter_->addAction(Icons::Of(Icons::Glyph::Search, Theme::kFaint), QLineEdit::LeadingPosition);
    connect(filter_, &QLineEdit::textChanged, this, &ScriptIde::Filter);

    scripts_ = Rows("ide_scripts");
    QFont rowed(Theme::MonoFamily());
    rowed.setPixelSize(11);
    scripts_->setFont(rowed);
    scripts_->setStyleSheet(
        QString("QTreeWidget { background: %1; border: 0; } "
                "QTreeWidget::item { font-family: %4; font-size: 11px; color: %5; } "
                "QTreeWidget::item:selected { background: %2; color: %6; }")
            .arg(Theme::kPanel.name(), Theme::Chosen().name(), Theme::Accent().name(),
                 Theme::MonoFamily(), Theme::kSoft.name(), Theme::kText.name()));
    connect(scripts_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* now, QTreeWidgetItem*) {
                Rebar();
                if (now == nullptr) return;
                const QVariant held = now->data(0, Qt::UserRole);
                if (!held.isValid()) return;
                const auto at = static_cast<std::size_t>(held.toULongLong());
                if (at >= listed_.size()) return;
                Q_EMIT ScriptChosen(listed_.at(at));
            });

    auto* side = new QWidget;
    side->setFixedWidth(kSideWidth);
    side->setStyleSheet(QString("background: %1;").arg(Theme::kPanel.name()));
    auto* side_stack = new QVBoxLayout(side);
    side_stack->setContentsMargins(0, 0, 0, 0);
    side_stack->setSpacing(0);
    side_stack->addWidget(Heading(tr("SCRIPTS IN THIS ANIMATION")));
    side_stack->addWidget(Rule(false));
    auto* filter_pad = new QWidget;
    auto* filter_row = new QHBoxLayout(filter_pad);
    filter_row->setContentsMargins(8, 8, 8, 8);
    filter_row->addWidget(filter_);
    side_stack->addWidget(filter_pad);
    side_stack->addWidget(scripts_, 1);
    side_stack->addWidget(Rule(false));

    auto* legend = new QWidget;
    legend->setFixedHeight(kStatusHeight + 8);
    auto* legend_row = new QHBoxLayout(legend);
    legend_row->setContentsMargins(12, 0, 12, 0);
    legend_row->setSpacing(6);
    for (const auto& [said, ink] :
         {std::pair{tr("call"), Theme::Accent()}, std::pair{tr("edited"), Theme::kAmber},
          std::pair{tr("register"), Theme::kViolet}, std::pair{tr("unreadable"), Theme::kWrong}}) {
        legend_row->addWidget(Dot(ink));
        auto* word = new QLabel(said);
        word->setStyleSheet(QString("color: %1; font-size: 11px;").arg(Theme::kFaint.name()));
        legend_row->addWidget(word);
    }
    legend_row->addStretch(1);
    side_stack->addWidget(legend);

    tabs_ = new QWidget;
    tabs_->setObjectName("ide_tabs");
    tabs_->setFixedHeight(kTabHeight);
    tabs_->setStyleSheet(QString("background: %1;").arg(Theme::kPanel.name()));
    tab_row_ = new QHBoxLayout(tabs_);
    tab_row_->setContentsMargins(0, 0, 0, 0);
    tab_row_->setSpacing(0);
    tab_row_->addStretch(1);

    editor_ = new ScriptEditor(ScriptEditor::Place::Window);
    connect(editor_, &ScriptEditor::Compiled, this, &ScriptIde::CompileAsked);
    connect(editor_, &ScriptEditor::Changed, this, &ScriptIde::Settled);
    connect(editor_, &ScriptEditor::CaretAt, this, &ScriptIde::Moved);
    connect(editor_, &ScriptEditor::NameChosen, this, &ScriptIde::NameChosen);

    problem_count_ = new QLabel;
    problem_count_->setObjectName("ide_problem_count");

    problems_ = new QListWidget;
    problems_->setObjectName("ide_problems");
    problems_->setFocusPolicy(Qt::NoFocus);
    problems_->setStyleSheet(QString("background: %1; border: 0;").arg(Theme::kPanel.name()));

    byte_view_ = new QPlainTextEdit;
    byte_view_->setObjectName("ide_byte_view");
    byte_view_->setReadOnly(true);
    byte_view_->setFocusPolicy(Qt::NoFocus);
    byte_view_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    byte_view_->setFont(QFont(Theme::MonoFamily(), Theme::kBaseSize - 1));
    byte_view_->setStyleSheet(QString("background: %1; color: %2; border: 0;")
                                  .arg(Theme::kPanel.name(), Theme::kSoft.name()));

    history_ = new QListWidget;
    history_->setObjectName("ide_history");
    history_->setFocusPolicy(Qt::NoFocus);
    history_->setStyleSheet(QString("background: %1; border: 0;").arg(Theme::kPanel.name()));

    panes_ = new QStackedWidget;
    panes_->setObjectName("ide_panes");
    panes_->addWidget(problems_);
    panes_->addWidget(byte_view_);
    panes_->addWidget(history_);

    summary_ = new QLabel;
    summary_->setObjectName("ide_summary");
    caret_ = new QLabel;
    caret_->setObjectName("ide_caret");
    object_ = new QLabel("aeplib");
    object_->setObjectName("ide_object");
    for (QLabel* said : {summary_, caret_, object_}) {
        said->setStyleSheet(QString("color: %1; font-family: %2; font-size: 11px;")
                                .arg(Theme::kFaint.name(), Theme::MonoFamily()));
    }
    trip_ = new QLabel;
    trip_->setObjectName("ide_round_trip");
    trip_->setStyleSheet(QString("color: %1; font-family: %2; font-size: 11px;")
                             .arg(Theme::kGreen.name(), Theme::MonoFamily()));

    auto* status = new QWidget;
    status->setFixedHeight(kStatusHeight);
    auto* status_row = new QHBoxLayout(status);
    status_row->setContentsMargins(14, 0, 14, 0);
    status_row->setSpacing(16);
    status_row->addWidget(summary_);
    status_row->addWidget(trip_);
    status_row->addStretch(1);
    status_row->addWidget(caret_);
    status_row->addWidget(object_);

    auto* pane = new QWidget;
    pane->setFixedHeight(kProblemsHeight);
    pane->setStyleSheet(QString("background: %1;").arg(Theme::kPanel.name()));
    auto* pane_stack = new QVBoxLayout(pane);
    pane_stack->setContentsMargins(0, 0, 0, 0);
    pane_stack->setSpacing(0);
    auto* pane_head = new QWidget;
    pane_head->setFixedHeight(kStripHeight);
    auto* pane_row = new QHBoxLayout(pane_head);
    pane_row->setContentsMargins(0, 0, 14, 0);
    pane_row->setSpacing(0);
    const QStringList pane_names{tr("Problems"), tr("Bytes"), tr("History")};
    for (int which = 0; which < pane_names.size(); which++) {
        auto* tab = new QPushButton(pane_names.at(which));
        tab->setObjectName(QStringLiteral("ide_pane_%1").arg(which));
        tab->setFlat(true);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setFixedHeight(kStripHeight);
        connect(tab, &QPushButton::clicked, this, [this, which] { ShowPane(which); });
        pane_tabs_.push_back(tab);
        pane_row->addWidget(tab);
        if (which == 0) pane_row->addWidget(problem_count_);
    }
    pane_row->addStretch(1);
    pane_stack->addWidget(pane_head);
    pane_stack->addWidget(Rule(false));
    pane_stack->addWidget(panes_, 1);
    pane_stack->addWidget(Rule(false));
    pane_stack->addWidget(status);

    auto* middle = new QWidget;
    auto* middle_stack = new QVBoxLayout(middle);
    middle_stack->setContentsMargins(0, 0, 0, 0);
    middle_stack->setSpacing(0);
    middle_stack->addWidget(tabs_);
    middle_stack->addWidget(Rule(false));
    middle_stack->addWidget(editor_, 1);
    middle_stack->addWidget(Rule(false));
    middle_stack->addWidget(pane);

    names_ = Rows("ide_names");
    names_->setColumnWidth(0, 34);

    auto* names_side = new QWidget;
    names_side->setFixedWidth(kNamesWidth);
    names_side->setStyleSheet(QString("background: %1;").arg(Theme::kPanel.name()));
    auto* names_stack = new QVBoxLayout(names_side);
    names_stack->setContentsMargins(0, 0, 0, 0);
    names_stack->setSpacing(0);
    names_stack->addWidget(Heading(tr("NAMES YOU CAN USE HERE")));
    names_stack->addWidget(Rule(false));
    names_stack->addWidget(names_, 1);
    auto* names_note = new QLabel(
        tr("Every name here is read out of the open package. Nothing is hardcoded per game."));
    names_note->setWordWrap(true);
    names_note->setContentsMargins(12, 10, 12, 10);
    names_note->setStyleSheet(QString("color: %1; font-size: 11px;").arg(Theme::kFaint.name()));
    names_stack->addWidget(Rule(false));
    names_stack->addWidget(names_note);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(side);
    body->addWidget(Rule(true));
    body->addWidget(middle, 1);
    body->addWidget(Rule(true));
    body->addWidget(names_side);

    auto* stack = new QVBoxLayout(this);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);
    stack->addWidget(bar);
    stack->addWidget(Rule(false));
    stack->addLayout(body, 1);

    connect(compile_, &QPushButton::clicked, this, &ScriptIde::Compile);
    connect(revert_, &QPushButton::clicked, this, &ScriptIde::Revert);

    auto* quickly = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    quickly->setContext(Qt::WidgetWithChildrenShortcut);
    connect(quickly, &QShortcut::activated, this, &ScriptIde::Compile);

    ShowPane(0);
    Moved(1, 1);
    Retell();
}

void ScriptIde::ShowPackage(const QString& file, const QString& animation) {
    file_ = file;
    animation_ = animation;
    Retell();
}

void ScriptIde::ShowScripts(const std::vector<Document::ScriptEntry>& scripts,
                            const std::optional<Document::ScriptPlace>& chosen) {
    listed_.clear();
    shapes_.clear();
    clips_.clear();
    dots_.clear();
    marks_.clear();
    scripts_->clear();
    QString group;
    QTreeWidgetItem* chose = nullptr;
    for (const Document::ScriptEntry& one : scripts) {
        const QString where = one.place.depth
                                  ? tr("%1, DEPTH %2")
                                        .arg(QString::fromStdString(one.clip_name).toUpper())
                                        .arg(*one.place.depth)
                                  : QString::fromStdString(one.clip_name).toUpper();
        if (where != group) {
            Group(scripts_, where, group != QString());
            group = where;
        }
        auto* row = new QTreeWidgetItem(scripts_);
        row->setText(1, QString::fromStdString(one.preview));
        row->setSizeHint(0, QSize(kFrameColumn, kScriptRow));
        row->setData(0, Qt::UserRole, static_cast<qulonglong>(listed_.size()));
        row->setToolTip(1, QString::fromStdString(one.preview));

        auto* mark = new QWidget;
        auto* mark_row = new QHBoxLayout(mark);
        mark_row->setContentsMargins(0, 0, 7, 0);
        mark_row->setSpacing(7);
        auto* when = new QLabel(one.place.depth ? tr("load") : QString::number(one.place.frame));
        when->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        when->setStyleSheet(QString("background: transparent; color: %1; font-family: %2; "
                                    "font-size: %3px;")
                                .arg(Theme::kFaint.name(), Theme::MonoFamily())
                                .arg(one.place.depth ? 10 : 11));
        mark_row->addWidget(when, 1);
        auto* dot = Dot(InkFor(one.shape));
        dot->setObjectName("ide_dot");
        mark_row->addWidget(dot, 0);
        scripts_->setItemWidget(row, 0, mark);

        marks_.append(mark);
        dots_.append(dot);
        shapes_.push_back(one.shape);
        clips_.push_back(QString::fromStdString(one.clip_name).isEmpty()
                             ? tr("Root")
                             : QString::fromStdString(one.clip_name));
        listed_.push_back(one.place);
        if (chosen && one.place == *chosen) chose = row;
    }
    if (chose != nullptr) {
        const QSignalBlocker quiet(scripts_);
        scripts_->setCurrentItem(chose);
    }
    Rebar();
    QMap<QString, QString> examples;
    for (const Document::ScriptEntry& one : scripts) {
        const QString shown = QString::fromStdString(one.preview);
        const qsizetype open = shown.indexOf(QChar('('));
        if (open <= 0 || !shown.endsWith(QChar(')'))) continue;
        const QString called = shown.left(open);
        if (!examples.contains(called)) examples.insert(called, shown);
    }
    editor_->KnowExamples(examples);
    filter_->setPlaceholderText(tr("Filter %1 scripts").arg(scripts.size()));
    Filter(filter_->text());
}

void ScriptIde::ShowScript(const QString& title, const QString& source,
                           const std::vector<uint8_t>& bytes, bool round_trips,
                           const Document::ScriptPlace& place) {
    title_ = title;
    bytes_written_ = static_cast<int>(bytes.size());
    round_trips_ = round_trips;
    showing_ = place;
    if (std::ranges::find(open_, place) == open_.end()) {
        open_.push_back(place);
        open_names_.push_back(title);
    }
    Retab();
    editor_->ShowScript(title, source);
    ShowBytes(bytes);
    Restate();
    Retell();
}

void ScriptIde::ShowNothing(const QString& title, const QString& why) {
    title_ = title;
    bytes_written_ = 0;
    round_trips_ = false;
    ShowBytes({});
    showing_.reset();
    Retab();
    editor_->ShowNothing(title, why);
    ShowProblems({IdeProblem{.said = why, .where = {}}});
    Retell();
}

void ScriptIde::ShowNames(const std::vector<IdeName>& names) {
    names_->clear();
    QList<ScriptName> plain;
    QStringList seen;
    QString group;
    for (const IdeName& one : names) {
        if (seen.contains(one.kind + one.name)) continue;
        seen.append(one.kind + one.name);
        const QString head = one.kind == QString("L")   ? tr("FRAME LABELS")
                             : one.kind == QString("C") ? tr("CLIP NAMES")
                                                        : tr("CALLS");
        if (head != group) {
            Group(names_, head, group != QString());
            group = head;
        }
        auto* row = new QTreeWidgetItem(names_);
        row->setFirstColumnSpanned(true);
        row->setToolTip(0, one.where);
        QWidget* held = Named(one);
        row->setSizeHint(0, QSize(0, held->sizeHint().height()));
        names_->setItemWidget(row, 0, held);
        if (one.kind == QString("f")) continue;
        plain.append(ScriptName{.name = one.name, .detail = one.where, .kind = one.kind});
    }
    editor_->KnowNames(plain);
    known_.clear();
    known_.reserve(static_cast<std::size_t>(plain.size()));
    for (const ScriptName& one : plain)
        known_.push_back(one.name.toStdString());
    Restate();
}

void ScriptIde::ShowProblems(const std::vector<IdeProblem>& problems) {
    problems_->clear();
    editor_->ClearProblems();
    for (const IdeProblem& one : problems) {
        if (one.line > 0) editor_->MarkProblem(one.line, one.column, one.length);
        auto* row = new QListWidgetItem(problems_);
        QWidget* held = Problem(one);
        row->setSizeHint(QSize(0, std::max(kProblemRow, held->sizeHint().height())));
        problems_->setItemWidget(row, held);
    }
    problem_count_->setText(problems.empty() ? QString() : QString::number(problems.size()));
    problem_count_->setStyleSheet(
        problems.empty() ? QString()
                         : QString("background: %1; color: %2; font-size: 10px; font-weight: 700; "
                                   "padding: 0 6px; border-radius: 7px;")
                               .arg(Theme::kWrong.name(), Theme::kInk.name()));
}

void ScriptIde::ShowBytes(const std::vector<uint8_t>& bytes) {
    QString written;
    for (std::size_t at = 0; at < bytes.size(); at++) {
        if (at != 0 && at % kBytesPerRow == 0) written += QChar('\n');
        written += QString("%1 ").arg(bytes.at(at), 2, kHexBase, QChar('0'));
    }
    byte_view_->setPlainText(written.trimmed());
}

void ScriptIde::ShowHistory(const QStringList& steps, int at) {
    history_->clear();
    for (int which = 0; which < steps.size(); which++) {
        auto* row = new QListWidgetItem(steps.at(which), history_);
        const bool done = which < at;
        row->setForeground(done ? Theme::kSoft : Theme::kFaint);
    }
    if (steps.isEmpty()) {
        auto* row = new QListWidgetItem(tr("nothing has been changed yet"), history_);
        row->setForeground(Theme::kFaint);
    }
}

void ScriptIde::ShowPane(int which) {
    panes_->setCurrentIndex(which);
    for (int at = 0; at < static_cast<int>(pane_tabs_.size()); at++) {
        const bool chosen = at == which;
        pane_tabs_.at(static_cast<std::size_t>(at))
            ->setStyleSheet(QString("QPushButton { border: 0; padding: 0 14px; color: %1; "
                                    "border-bottom: 2px solid %2; text-align: center; }")
                                .arg(chosen ? Theme::kText.name() : Theme::kFaint.name(),
                                     chosen ? Theme::Accent().name() : QString("transparent")));
    }
}

void ScriptIde::Compile() {
    if (!compile_->isEnabled()) return;
    Q_EMIT CompileAsked(editor_->Source());
}

void ScriptIde::Revert() {
    editor_->Restore();
}

void ScriptIde::Filter(const QString& text) {
    for (int at = 0; at < scripts_->topLevelItemCount(); at++) {
        QTreeWidgetItem* row = scripts_->topLevelItem(at);
        if (!row->data(0, Qt::UserRole).isValid()) continue;
        const bool shown = text.isEmpty() || row->text(1).contains(text, Qt::CaseInsensitive) ||
                           row->text(0).contains(text, Qt::CaseInsensitive);
        row->setHidden(!shown);
    }
}

void ScriptIde::Settled(bool changed) {
    changed_ = changed;
    if (tab_dot_ != nullptr) tab_dot_->setVisible(changed);
    compile_->setEnabled(changed);
    revert_->setEnabled(changed);
    Redot();
    Restate();
    Retell();
}

void ScriptIde::Rebar() {
    const int chosen = scripts_->currentItem() == nullptr
                           ? -1
                           : scripts_->indexOfTopLevelItem(scripts_->currentItem());
    for (int at = 0; at < marks_.size(); at++) {
        QWidget* mark = marks_.at(at);
        const bool here = chosen >= 0 && scripts_->topLevelItem(chosen) != nullptr &&
                          scripts_->itemWidget(scripts_->topLevelItem(chosen), 0) == mark;
        mark->setStyleSheet(here ? QString("background: transparent; border-left: 2px solid %1;")
                                       .arg(Theme::Accent().name())
                                 : QString("background: transparent;"));
    }
}

void ScriptIde::Redot() {
    for (int at = 0; at < dots_.size() && at < static_cast<int>(shapes_.size()); at++) {
        const bool edited =
            changed_ && showing_ && listed_[static_cast<std::size_t>(at)] == *showing_;
        Paint(dots_[at], edited ? Theme::kAmber : InkFor(shapes_[static_cast<std::size_t>(at)]));
    }
}

void ScriptIde::Restate() {
    std::vector<IdeProblem> problems;
    for (const Document::ScriptProblem& one :
         Document::ScriptProblems(editor_->Source().toStdString(), known_)) {
        problems.push_back(
            IdeProblem{.said = QString::fromStdString(one.said),
                       .where = tr("line %1, column %2").arg(one.line).arg(one.column),
                       .line = static_cast<int>(one.line),
                       .column = static_cast<int>(one.column),
                       .length = static_cast<int>(one.length)});
    }
    ShowProblems(problems);
}

void ScriptIde::Moved(int line, int column) {
    caret_->setText(tr("Ln %1, Col %2").arg(line).arg(column));
}

void ScriptIde::Retab() {
    tab_dot_ = nullptr;
    while (tab_row_->count() > 1) {
        QLayoutItem* held = tab_row_->takeAt(0);
        if (held->widget() != nullptr) held->widget()->deleteLater();
        delete held;
    }
    for (std::size_t at = 0; at < open_.size(); at++) {
        const bool chosen = showing_ && open_.at(at) == *showing_;
        auto* tab = new QPushButton(open_names_.at(at), tabs_);
        tab->setObjectName(QStringLiteral("ide_tab_%1").arg(at));
        tab->setFlat(true);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setFixedHeight(kTabHeight);
        tab->setStyleSheet(
            QString("QPushButton { border: 0; border-right: 1px solid %1; padding: 0 14px; "
                    "text-align: left; font-family: %2; color: %3; background: %4; "
                    "border-bottom: 2px solid %5; }")
                .arg(Theme::kLine.name(), Theme::MonoFamily(),
                     chosen ? Theme::kText.name() : Theme::kFaint.name(),
                     chosen ? Theme::kPage.name() : Theme::kPanel.name(),
                     chosen ? Theme::Accent().name() : QString("transparent")));
        if (chosen) {
            tab_dot_ = Dot(Theme::kAmber, kTabDot);
            tab_dot_->setObjectName("ide_tab_edited");
            tab_dot_->setVisible(changed_);
            auto* marked = new QHBoxLayout(tab);
            marked->setContentsMargins(0, 0, 14, 0);
            marked->addStretch(1);
            marked->addWidget(tab_dot_, 0);
            tab->setMinimumWidth(tab->sizeHint().width() + kTabDot + 8);
        }
        const Document::ScriptPlace place = open_.at(at);
        connect(tab, &QPushButton::clicked, this, [this, place] { Q_EMIT ScriptChosen(place); });
        tab_row_->insertWidget(static_cast<int>(at), tab);
    }
}

QString ScriptIde::Crumb() const {
    QString where;
    if (showing_) {
        for (std::size_t at = 0; at < listed_.size(); at++) {
            if (listed_.at(at) == *showing_ && at < clips_.size()) where = clips_.at(at);
        }
    }
    QStringList walked{file_, animation_};
    if (!where.isEmpty()) walked.append(where);
    QString said;
    for (const QString& one : walked)
        said += QString("<span style=\"color:%1\">%2</span>  ›  ").arg(Theme::kFaint.name(), one);
    return said + QString("<span style=\"color:%1;font-weight:600\">%2</span>")
                      .arg(Theme::kText.name(), title_);
}

void ScriptIde::Retell() {
    crumb_->setText(file_.isEmpty() ? tr("No package open") : Crumb());
    bytes_->setText(changed_ ? tr("edited, not compiled")
                             : tr("%1, unchanged").arg(ScriptEditor::Bytes(bytes_written_)));
    summary_->setText(ScriptEditor::Counted(editor_->Source()) + QString("   ") +
                      ScriptEditor::Bytes(bytes_written_));
    trip_->setText(round_trips_ && !changed_ ? tr("round trips to the original") : QString());
}

}
