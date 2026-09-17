#include "editor_window.h"

#include "editor_files.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/outline.h"
#include "support/expected.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Editor {

namespace {

constexpr int kMostFrames = std::numeric_limits<uint16_t>::max();

std::optional<std::string> AnyAnimation(const std::vector<Document::Node>& nodes) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) return node.path;
        if (auto found = AnyAnimation(node.children)) return found;
    }
    return std::nullopt;
}

}

void Window::AddNewAnimation() {
    if (!file_) return;
    const std::optional<std::string> like =
        animation_path_.empty() ? AnyAnimation(file_->Nodes()) : animation_path_;
    if (!like) {
        ReportProblem(tr("This package has no animation to take a stage size and frame rate from"));
        return;
    }
    const auto template_animation = file_->ReadAnimation(*like);
    if (!template_animation) {
        ReportProblem(QString::fromStdString(template_animation.error()));
        return;
    }

    bool answered = false;
    const QString name = QInputDialog::getText(this, tr("New animation"), tr("Name"),
                                               QLineEdit::Normal, QString(), &answered);
    if (!answered) return;
    const int frames = QInputDialog::getInt(
        this, tr("New animation"), tr("Frames"),
        static_cast<int>(template_animation->root.frames.size()), 1, kMostFrames, 1, &answered);
    if (!answered) return;

    const std::string logical = name.toStdString();
    const std::string like_path = *like;
    std::string made;
    const bool added =
        EditDocument(tr("New animation %1").arg(name), [&logical, &like_path, frames,
                                                        &made](Document::File& document) {
            using Added = Support::Expected<void, std::string>;
            auto path = document.AddAnimation(logical, like_path, static_cast<uint32_t>(frames));
            if (!path) return Added(Support::Unexpected(path.error()));
            made = *path;
            return Added();
        });
    if (added) SelectEntry(QString::fromStdString(made));
}

void Window::DrawBackground(bool drawn) {
    QSettings().setValue(kBackgroundKey, drawn);
    if (!host_.Running()) return;
    const auto set = host_.SetBackgroundDrawn(drawn);
    if (!set) {
        ReportProblem(QString::fromStdString(set.error()));
        return;
    }
    RenderFrame();
}

void Window::RemoveAnimation(const std::string& path, const QString& name) {
    if (!file_) return;
    const bool owned =
        std::ranges::any_of(authored_, [&path](const Document::AuthoredDepth& depth) {
            return depth.animation == path;
        });
    if (owned) {
        ReportProblem(
            tr("The project owns depths in %1; detach them before removing it").arg(name));
        return;
    }
    const bool open = path == animation_path_;
    if (open) CloseAnimation();
    const bool removed =
        EditDocument(tr("Remove animation %1").arg(name),
                     [&path](Document::File& document) { return document.RemoveAnimation(path); });
    if (!removed && open) ShowSelectedEntry();
}

}
