#include "editor_start_screen.h"

#include "editor_commands.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>

#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kPathRole = Qt::UserRole;

}

StartScreen::StartScreen(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), recent_(new QListWidget), host_(new QLabel) {
    setObjectName("start_screen");
    auto* title = new QLabel(tr("IFS Editor"));
    title->setObjectName("start_title");
    QFont big = title->font();
    big.setPointSizeF(big.pointSizeF() + 8);
    big.setBold(true);
    title->setFont(big);

    auto* buttons = new QHBoxLayout;
    for (const auto& [id, text] :
         {std::pair{QStringLiteral("file.open"), tr("Open IFS...")},
          std::pair{QStringLiteral("project.open"), tr("Open project...")},
          std::pair{QStringLiteral("project.new"), tr("New project...")}}) {
        auto* button = new QPushButton(text);
        button->setObjectName("start_" + id);
        connect(button, &QPushButton::clicked, this, [this, id] { commands_.Run(id); });
        buttons->addWidget(button);
    }
    buttons->addStretch();

    recent_->setObjectName("start_recent");
    connect(recent_, &QListWidget::itemActivated, this, [this](const QListWidgetItem* item) {
        emit FileAsked(item->data(kPathRole).toString());
    });
    connect(recent_, &QListWidget::itemDoubleClicked, this, [this](const QListWidgetItem* item) {
        emit FileAsked(item->data(kPathRole).toString());
    });

    host_->setObjectName("start_host");
    host_->setWordWrap(true);
    auto* choose = new QPushButton(tr("Choose game install..."));
    choose->setObjectName("start_file.game_install");
    connect(choose, &QPushButton::clicked, this,
            [this] { commands_.Run(QStringLiteral("file.game_install")); });
    auto* card = new QHBoxLayout;
    card->addWidget(host_, 1);
    card->addWidget(choose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addWidget(title);
    layout->addLayout(buttons);
    layout->addWidget(new QLabel(tr("Recent files")));
    layout->addWidget(recent_, 1);
    layout->addLayout(card);
    layout->addWidget(new QLabel(
        tr("An IFS or a project folder dropped on the window opens it. Without a game install "
           "everything but the picture still works: the package, the timeline, the inspector and "
           "the project are all read from the file itself.")));
}

void StartScreen::ShowRecent(const std::vector<RecentFile>& recent) {
    recent_->clear();
    for (const RecentFile& one : recent) {
        const QString badge = one.project ? tr(", project") : QString();
        const QString counted =
            one.animations < 0 ? QString() : tr(", %1 animation(s)").arg(one.animations);
        auto* item = new QListWidgetItem(
            tr("%1%2%3\n%4").arg(one.path.section('/', -1), counted, badge, one.folder), recent_);
        item->setData(kPathRole, one.path);
    }
    if (recent_->count() != 0) return;
    auto* nothing = new QListWidgetItem(tr("Nothing opened yet"), recent_);
    nothing->setFlags(Qt::NoItemFlags);
}

void StartScreen::ShowHost(const HostCard& host) {
    if (host.install.isEmpty()) {
        host_->setText(tr("No game install chosen, so there is no picture of the stage."));
        return;
    }
    host_->setText(
        host.running
            ? tr("Preview running from %1, built as %2").arg(host.install, host.build)
            : tr("Preview stopped. Install %1, built as %2").arg(host.install, host.build));
}

}
