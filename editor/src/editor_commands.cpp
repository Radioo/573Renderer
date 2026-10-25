#include "editor_commands.h"

#include <QAction>
#include <QMenu>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <vector>
#include <utility>

namespace Editor {

Commands::Commands(QWidget* window) : QObject(window), window_(window) {
    setObjectName("commands");
}

QAction* Commands::Add(Command command) {
    QWidget* scope = command.scope != nullptr ? command.scope : window_;
    auto* action = new QAction(command.text, scope);
    action->setShortcut(command.keys);
    action->setShortcutContext(command.scope != nullptr ? Qt::WidgetShortcut : Qt::WindowShortcut);
    scope->addAction(action);
    const QString id = command.id;
    connect(action, &QAction::triggered, this, [this, id] { Run(id); });
    entries_.push_back(Entry{.id = std::move(command.id),
                             .brief = std::move(command.brief),
                             .action = action,
                             .run = std::move(command.run),
                             .refusal = std::move(command.refusal)});
    return action;
}

QAction* Commands::AddToggle(Toggle toggle) {
    auto* action = new QAction(toggle.text, window_);
    action->setShortcut(toggle.keys);
    action->setCheckable(true);
    action->setChecked(toggle.checked);
    window_->addAction(action);
    if (toggle.set) connect(action, &QAction::toggled, this, std::move(toggle.set));
    entries_.push_back(Entry{.id = std::move(toggle.id),
                             .action = action,
                             .run = [action] { action->toggle(); },
                             .refusal = {}});
    return action;
}

void Commands::ShowAvailabilityIn(QMenu* menu) {
    menu->setToolTipsVisible(true);
    auto before = std::make_shared<std::vector<std::pair<QAction*, bool>>>();
    connect(menu, &QMenu::aboutToShow, this, [this, menu, before] {
        before->clear();
        for (QAction* action : menu->actions()) {
            const Entry* entry = FindAction(action);
            if (entry == nullptr) continue;
            before->emplace_back(action, action->isEnabled());
            const std::optional<QString> refused = Refusal(entry->id);
            action->setEnabled(!refused);
            action->setToolTip(refused.value_or(QString()));
        }
    });
    connect(menu, &QMenu::aboutToHide, this, [before] {
        for (const auto& [action, enabled] : *before)
            action->setEnabled(enabled);
        before->clear();
    });
}

QAction* Commands::Action(const QString& id) const {
    const Entry* entry = Find(id);
    return entry != nullptr ? entry->action : nullptr;
}

QString Commands::Brief(const QString& id) const {
    const Entry* entry = Find(id);
    if (entry == nullptr) return {};
    if (!entry->brief.isEmpty()) return entry->brief;
    return QString(entry->action->text()).remove('&');
}

std::optional<QString> Commands::Refusal(const QString& id) const {
    const Entry* entry = Find(id);
    if (entry == nullptr || !entry->refusal) return std::nullopt;
    return entry->refusal();
}

std::vector<QString> Commands::Ids() const {
    std::vector<QString> ids;
    ids.reserve(entries_.size());
    for (const Entry& entry : entries_)
        ids.push_back(entry.id);
    return ids;
}

bool Commands::Run(const QString& id) {
    const Entry* entry = Find(id);
    if (entry == nullptr) return false;
    if (const std::optional<QString> refused = Refusal(id)) {
        emit Refused(*refused);
        return false;
    }
    entry->run();
    return true;
}

const Commands::Entry* Commands::Find(const QString& id) const {
    const auto at = std::ranges::find(entries_, id, &Entry::id);
    return at != entries_.end() ? &*at : nullptr;
}

const Commands::Entry* Commands::FindAction(const QAction* action) const {
    const auto at = std::ranges::find(entries_, action, &Entry::action);
    return at != entries_.end() ? &*at : nullptr;
}

}
