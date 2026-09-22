#include "editor_tool_strip.h"

#include "editor_commands.h"
#include "editor_icons.h"
#include "editor_theme.h"

#include <QAction>
#include <QFrame>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <utility>

namespace Editor {

ToolStrip::ToolStrip(Commands& commands, QWidget* parent) : QWidget(parent), commands_(commands) {
    setObjectName("tool_strip");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    setFixedWidth(Theme::kToolStripWidth);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(2);
    layout->addStretch();
}

void ToolStrip::Build() {
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout == nullptr || layout->count() > 1) return;
    delete layout->takeAt(0);
    const std::array<std::pair<QString, Icons::Glyph>, 5> tools{
        {{QStringLiteral("tool.select"), Icons::Glyph::Select},
         {QStringLiteral("tool.anchor"), Icons::Glyph::Anchor},
         {QStringLiteral("tool.pan"), Icons::Glyph::Pan},
         {QStringLiteral("tool.zoom"), Icons::Glyph::Zoom},
         {QStringLiteral("tool.sketch"), Icons::Glyph::Sketch}}};
    for (const auto& [id, glyph] : tools) {
        QAction* action = commands_.Action(id);
        if (action == nullptr) continue;
        if (id.endsWith("sketch")) {
            auto* rule = new QFrame;
            rule->setObjectName("tool_rule");
            rule->setFixedSize(Theme::kToolRuleWidth, 1);
            layout->addWidget(rule, 0, Qt::AlignHCenter);
            layout->addSpacing(5);
        }
        auto* button = new QToolButton;
        button->setObjectName("strip_" + id);
        button->setProperty("strip", true);
        button->setIcon(
            Icons::Toggling(glyph, Theme::kSoft, Theme::OnChosen(), Theme::kToolIconSide));
        button->setIconSize(QSize(Theme::kToolIconSide, Theme::kToolIconSide));
        button->setFixedSize(Theme::kToolButtonSide, Theme::kToolButtonSide);
        button->setToolTip(action->text().remove('&') + " (" +
                           action->shortcut().toString(QKeySequence::NativeText) + ")");
        button->setCheckable(true);
        button->setChecked(action->isChecked());
        connect(action, &QAction::toggled, button, &QToolButton::setChecked);
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        layout->addWidget(button, 0, Qt::AlignHCenter);
    }
    layout->addStretch();
}

}
