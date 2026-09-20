#include "editor_tool_strip.h"

#include "editor_commands.h"

#include <QAction>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace Editor {

ToolStrip::ToolStrip(Commands& commands, QWidget* parent) : QWidget(parent), commands_(commands) {
    setObjectName("tool_strip");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 4, 2, 4);
    layout->setSpacing(2);
    layout->addStretch();
}

void ToolStrip::Build() {
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout == nullptr || layout->count() > 1) return;
    delete layout->takeAt(0);
    for (const auto& [id, text] : {std::pair{QStringLiteral("tool.select"), tr("Select")},
                                   std::pair{QStringLiteral("tool.anchor"), tr("Anchor")},
                                   std::pair{QStringLiteral("tool.pan"), tr("Pan")},
                                   std::pair{QStringLiteral("tool.zoom"), tr("Zoom")},
                                   std::pair{QStringLiteral("tool.sketch"), tr("Sketch")}}) {
        QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        auto* button = new QToolButton;
        button->setObjectName("strip_" + id);
        button->setText(text);
        button->setToolTip(action->text().remove('&') + " (" +
                           action->shortcut().toString(QKeySequence::NativeText) + ")");
        button->setCheckable(true);
        button->setChecked(action->isChecked());
        connect(action, &QAction::toggled, button, &QToolButton::setChecked);
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        layout->addWidget(button);
    }
    layout->addStretch();
}

}
