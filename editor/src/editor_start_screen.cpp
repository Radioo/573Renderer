#include "editor_start_screen.h"

#include "editor_commands.h"
#include "editor_icons.h"
#include "editor_theme.h"

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayoutItem>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>

#include <vector>

namespace Editor {

namespace {

constexpr int kLeftWidth = 260;
constexpr int kCardWidth = 320;
constexpr int kBigButton = 36;
constexpr int kRowHeight = 56;
constexpr int kThumbWidth = 80;
constexpr int kThumbHeight = 45;
constexpr int kDropHeight = 150;
constexpr int kDateWidth = 80;

QPixmap Thumbnail() {
    QPixmap drawn(kThumbWidth, kThumbHeight);
    drawn.fill(QColor(0x2f, 0x55, 0x7c));
    QPainter painter(&drawn);
    painter.setPen(QPen(QColor(0x5d, 0x8f, 0xc4, 0x55), 2));
    for (int at = -kThumbHeight; at < kThumbWidth; at += 7)
        painter.drawLine(at, kThumbHeight, at + kThumbHeight, 0);
    painter.setPen(QPen(QColor(0x5d, 0x8f, 0xc4, 0x88), 1));
    painter.drawRect(0, 0, kThumbWidth - 1, kThumbHeight - 1);
    return drawn;
}

QLabel* Said(const QString& text, const QColor& colour, int size, bool bold = false) {
    auto* label = new QLabel(text);
    QFont font(Theme::SansFamily());
    font.setPixelSize(size);
    font.setBold(bold);
    label->setFont(font);
    label->setStyleSheet(
        QString("color: %1; background: transparent;").arg(colour.name(QColor::HexRgb)));
    return label;
}

QLabel* Mono(const QString& text, const QColor& colour, int size) {
    auto* label = new QLabel(text);
    QFont font(Theme::MonoFamily());
    font.setPixelSize(size);
    label->setFont(font);
    label->setStyleSheet(
        QString("color: %1; background: transparent;").arg(colour.name(QColor::HexRgb)));
    return label;
}

QString Ago(const QDateTime& when) {
    if (!when.isValid()) return {};
    const qint64 days = when.date().daysTo(QDateTime::currentDateTime().date());
    if (days <= 0) return QObject::tr("today");
    if (days == 1) return QObject::tr("yesterday");
    if (days < 7) return QObject::tr("%1 days ago").arg(days);
    if (days < 14) return QObject::tr("last week");
    return when.date().toString("d MMM");
}

}

StartScreen::StartScreen(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), recent_(new QWidget), host_(new QLabel),
      install_(new QLabel), running_(new QLabel), build_(new QLabel), dot_(new QLabel) {
    setObjectName("start_screen");

    auto* left = new QVBoxLayout;
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(6);
    left->addWidget(Said(tr("IFS editor"), Theme::kText, 22, true));
    left->addSpacing(10);

    auto* open = new QPushButton;
    open->setObjectName("start_open");
    open->setFixedHeight(kBigButton);
    open->setCursor(Qt::PointingHandCursor);
    auto* open_line = new QHBoxLayout(open);
    open_line->setContentsMargins(9, 0, 9, 0);
    open_line->setSpacing(6);
    auto* open_icon = new QLabel;
    open_icon->setPixmap(Icons::Drawn(Icons::Glyph::File, Theme::kOnAccent, 15));
    open_line->addWidget(open_icon);
    open_line->addWidget(Said(tr("Open IFS"), Theme::kOnAccent, 13, true));
    open_line->addSpacing(4);
    open_line->addWidget(
        Mono(QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText).replace("+", " "),
             Theme::kQuietOnAccent, 11));
    open_line->addStretch();
    connect(open, &QPushButton::clicked, this,
            [this] { commands_.Run(QStringLiteral("file.open")); });
    left->addWidget(open);

    auto* project = new QPushButton;
    project->setObjectName("start_open_project");
    project->setFixedHeight(kBigButton);
    project->setCursor(Qt::PointingHandCursor);
    auto* project_line = new QHBoxLayout(project);
    project_line->setContentsMargins(9, 0, 9, 0);
    project_line->setSpacing(6);
    auto* project_icon = new QLabel;
    project_icon->setPixmap(Icons::Drawn(Icons::Glyph::Folder, Theme::kText, 15));
    project_line->addWidget(project_icon);
    project_line->addWidget(Said(tr("Open project"), Theme::kText, 13));
    project_line->addStretch();
    connect(project, &QPushButton::clicked, this,
            [this] { commands_.Run(QStringLiteral("project.open")); });
    left->addWidget(project);

    auto* drop = new QFrame;
    drop->setObjectName("start_drop");
    drop->setFixedHeight(kDropHeight);
    auto* drop_line = new QVBoxLayout(drop);
    drop_line->setSpacing(8);
    drop_line->addStretch();
    auto* drop_icon = new QLabel;
    drop_icon->setPixmap(Icons::Drawn(Icons::Glyph::Upload, Theme::kFaint, 22));
    drop_line->addWidget(drop_icon, 0, Qt::AlignHCenter);
    drop_line->addWidget(Said(tr("Drop an .ifs or a project folder anywhere"), Theme::kFaint, 12),
                         0, Qt::AlignHCenter);
    drop_line->addStretch();
    left->addSpacing(16);
    left->addWidget(drop);
    left->addStretch();

    auto* middle = new QVBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(Said(tr("Recent"), Theme::kSoft, 13, true));
    middle->addSpacing(8);
    rows_ = new QVBoxLayout(recent_);
    rows_->setContentsMargins(0, 0, 0, 0);
    rows_->setSpacing(0);
    middle->addWidget(recent_);
    middle->addStretch();

    auto* card = new QFrame;
    card->setObjectName("start_card");
    card->setFixedWidth(kCardWidth);
    auto* inside = new QVBoxLayout(card);
    inside->setContentsMargins(14, 14, 14, 14);
    inside->setSpacing(8);
    auto* heading = new QHBoxLayout;
    heading->setSpacing(8);
    auto* preview_icon = new QLabel;
    preview_icon->setPixmap(Icons::Drawn(Icons::Glyph::Preview, Theme::kText, 16));
    heading->addWidget(preview_icon);
    heading->addWidget(Said(tr("Preview"), Theme::kText, 12, true));
    heading->addStretch();
    inside->addLayout(heading);
    inside->addWidget(Said(tr("Game install"), Theme::kFaint, 11));
    install_->setObjectName("start_install");
    QFont mono(Theme::MonoFamily());
    mono.setPixelSize(12);
    install_->setFont(mono);
    install_->setWordWrap(true);
    inside->addWidget(install_);
    auto* state = new QHBoxLayout;
    state->setSpacing(6);
    dot_->setObjectName("start_dot");
    dot_->setFixedSize(7, 7);
    state->addWidget(dot_);
    running_->setObjectName("start_running");
    state->addWidget(running_, 1);
    inside->addLayout(state);
    build_->setObjectName("start_build");
    build_->setWordWrap(true);
    inside->addWidget(build_);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    auto* change = new QPushButton(tr("Change install"));
    change->setObjectName("start_change_install");
    change->setFixedHeight(Theme::kControlHeight);
    connect(change, &QPushButton::clicked, this,
            [this] { commands_.Run(QStringLiteral("file.game_install")); });
    actions->addWidget(change);
    actions->addStretch();
    inside->addLayout(actions);
    auto* rule = new QFrame;
    rule->setObjectName("start_rule");
    rule->setFixedHeight(1);
    inside->addWidget(rule);
    host_->setObjectName("start_host");
    host_->setWordWrap(true);
    host_->setText(tr("Without an install everything except the picture still works: the "
                      "timeline, the inspector and every edit."));
    inside->addWidget(host_);
    inside->addStretch();

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(70, 60, 70, 60);
    grid->setHorizontalSpacing(40);
    grid->setVerticalSpacing(0);
    auto* left_held = new QWidget;
    left_held->setLayout(left);
    left_held->setFixedWidth(kLeftWidth);
    grid->addWidget(left_held, 0, 0, Qt::AlignTop);
    auto* middle_held = new QWidget;
    middle_held->setLayout(middle);
    grid->addWidget(middle_held, 0, 1);
    grid->addWidget(card, 0, 2, Qt::AlignTop);
    grid->setColumnStretch(1, 1);
}

void StartScreen::ShowRecent(const std::vector<RecentFile>& recent) {
    while (QLayoutItem* taken = rows_->takeAt(0)) {
        if (QWidget* widget = taken->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete taken;
    }
    if (recent.empty()) {
        auto* nothing = Said(tr("Nothing opened yet"), Theme::kFaint, 12);
        nothing->setObjectName("recent_empty");
        rows_->addWidget(nothing);
        return;
    }
    for (const RecentFile& one : recent) {
        auto* row = new QPushButton;
        row->setObjectName("recent_row");
        row->setProperty("path", one.path);
        row->setFixedHeight(kRowHeight);
        row->setCursor(Qt::PointingHandCursor);
        const QString path = one.path;
        connect(row, &QPushButton::clicked, this, [this, path] { emit FileAsked(path); });

        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(10, 0, 10, 0);
        line->setSpacing(12);
        auto* shown = new QLabel;
        shown->setFixedSize(kThumbWidth, kThumbHeight);
        shown->setPixmap(Thumbnail());
        line->addWidget(shown);

        auto* named = new QVBoxLayout;
        named->setContentsMargins(0, 0, 0, 0);
        named->setSpacing(1);
        const QFileInfo about(one.path);
        auto* title = Said(about.fileName(), Theme::kText, 13);
        title->setObjectName("recent_name");
        named->addWidget(title);
        QString under = one.folder.section('/', -2);
        if (one.animations >= 0) under += tr(", %1 animation(s)").arg(one.animations);
        auto* sub = Said(under, Theme::kFaint, 11);
        sub->setObjectName("recent_detail");
        named->addWidget(sub);
        line->addLayout(named, 1);

        if (one.project) {
            auto* mark = new QLabel;
            mark->setPixmap(Icons::Drawn(Icons::Glyph::Folder, Theme::kAmber, 12));
            line->addWidget(mark);
            auto* badge = Said(tr("project"), Theme::kAmber, 11);
            badge->setObjectName("recent_project");
            line->addWidget(badge);
        }
        auto* when = Said(Ago(about.lastModified()), Theme::kFaint, 11);
        when->setFixedWidth(kDateWidth);
        when->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(when);
        rows_->addWidget(row);
    }
}

void StartScreen::ShowHost(const HostCard& host) {
    const bool known = !host.install.isEmpty();
    install_->setText(known ? host.install : tr("None chosen"));
    install_->setStyleSheet(QString("color: %1; background: transparent;")
                                .arg((known ? Theme::kText : Theme::kFaint).name()));
    dot_->setStyleSheet(
        QString("background: %1;").arg((host.running ? Theme::kGreen : Theme::kEdge).name()));
    running_->setText(host.running ? tr("Preview host ready, afp and avs DLLs loaded")
                                   : tr("Preview host not running"));
    running_->setStyleSheet(
        QString("color: %1; background: transparent;").arg(Theme::kText.name()));
    build_->setText(known ? tr("Target build: %1. Saved files must load on it.").arg(host.build)
                          : tr("Choose an install to see the stage."));
    build_->setStyleSheet(
        QString("color: %1; background: transparent; font-size: 11px;").arg(Theme::kFaint.name()));
}

}
