#include "editor_window.h"

#include "editor_files.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/unused_definitions.h"
#include "support/expected.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kMostFrames = std::numeric_limits<uint16_t>::max();

void CollectAnimations(const std::vector<Document::Node>& nodes,
                       std::vector<const Document::Node*>& found) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) found.push_back(&node);
        CollectAnimations(node.children, found);
    }
}

std::vector<const Document::Node*> Animations(const Document::File& file) {
    std::vector<const Document::Node*> found;
    CollectAnimations(file.Nodes(), found);
    return found;
}

}

std::optional<Window::AnimationSource> Window::ChooseAnimationSource() {
    const std::vector<const Document::Node*> own = Animations(*file_);
    if (!own.empty()) {
        return AnimationSource{.other = std::nullopt,
                               .path =
                                   animation_path_.empty() ? own.front()->path : animation_path_};
    }
    const QString chosen = QFileDialog::getOpenFileName(
        this, tr("Take the new animation's stage, frame rate and library from"), QString(),
        tr("IFS files (*.ifs);;All files (*)"));
    if (chosen.isEmpty()) return std::nullopt;
    auto other = Document::File::Open(ReadFileBytes(chosen));
    if (!other) {
        ReportProblem(QString::fromStdString(other.error()));
        return std::nullopt;
    }
    const std::vector<const Document::Node*> theirs = Animations(*other);
    if (theirs.empty()) {
        ReportProblem(tr("%1 has no animation either").arg(chosen));
        return std::nullopt;
    }
    QStringList names;
    for (const Document::Node* node : theirs)
        names.append(QString::fromStdString(node->name));
    bool answered = false;
    const QString picked = QInputDialog::getItem(this, tr("New animation"), tr("Copy from"), names,
                                                 0, false, &answered);
    if (!answered) return std::nullopt;
    const std::string path = theirs[static_cast<std::size_t>(names.indexOf(picked))]->path;
    return AnimationSource{.other = std::move(*other), .path = path};
}

void Window::AddNewAnimation() {
    if (!file_) return;
    std::optional<AnimationSource> source = ChooseAnimationSource();
    if (!source) return;
    const Document::File& from = source->other ? *source->other : *file_;
    const auto template_animation = from.ReadAnimation(source->path);
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
    std::string made;
    const bool added =
        EditDocument(tr("New animation %1").arg(name),
                     [&logical, &source, frames, &made](Document::File& document) {
                         using Added = Support::Expected<void, std::string>;
                         const Document::File& like = source->other ? *source->other : document;
                         auto path = document.AddAnimation(logical, like, source->path,
                                                           static_cast<uint32_t>(frames));
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

bool Window::ProjectOwnsDepthsIn(const std::string& path) const {
    return std::ranges::any_of(authored_, [&path](const Document::AuthoredDepth& depth) {
        return depth.animation == path;
    });
}

void Window::RenameAnimationEntry(const std::string& path, const QString& name) {
    if (!file_) return;
    if (ProjectOwnsDepthsIn(path)) {
        ReportProblem(
            tr("The project owns depths in %1; detach them before renaming it").arg(name));
        return;
    }
    bool answered = false;
    const QString wanted = QInputDialog::getText(this, tr("Rename %1").arg(name), tr("Name"),
                                                 QLineEdit::Normal, name, &answered);
    if (!answered || wanted == name) return;
    const std::string text = wanted.toStdString();
    std::string renamed;
    const bool open = path == animation_path_;
    if (open) CloseAnimation();
    if (!EditDocument(tr("Rename %1 to %2").arg(name, wanted),
                      [&path, &text, &renamed](Document::File& document) {
                          using Renamed = Support::Expected<void, std::string>;
                          auto moved = document.RenameAnimation(path, text);
                          if (!moved) return Renamed(Support::Unexpected(moved.error()));
                          renamed = std::move(*moved);
                          return Renamed();
                      })) {
        if (open) ShowSelectedEntry();
        return;
    }
    for (std::vector<Document::DepthInClip>* view : {&hidden_, &locked_}) {
        for (Document::DepthInClip& entry : *view) {
            if (entry.animation == path) entry.animation = renamed;
        }
    }
    SelectEntry(QString::fromStdString(renamed));
}

void Window::RemoveAnimation(const std::string& path, const QString& name) {
    if (!file_) return;
    if (ProjectOwnsDepthsIn(path)) {
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

void Window::RemoveUnusedDefinitionsFrom(const std::string& path, const QString& name) {
    if (!file_) return;
    if (ProjectOwnsDepthsIn(path)) {
        ReportProblem(tr("The project owns depths in %1; detach them before removing definitions "
                         "from it")
                          .arg(name));
        return;
    }
    Document::File tidied = *file_;
    const auto removed = Document::RemoveUnusedDefinitions(tidied, path);
    if (!removed) {
        ReportProblem(QString::fromStdString(removed.error()));
        return;
    }
    if (removed->empty()) {
        statusBar()->showMessage(tr("Nothing in %1 is unused").arg(name));
        return;
    }
    if (!EditDocument(tr("Remove unused definitions from %1").arg(name),
                      [&tidied](Document::File& document) {
                          document = std::move(tidied);
                          return Support::Expected<void, std::string>();
                      })) {
        return;
    }
    if (path == animation_path_) {
        RefillClipsKeepingChoice();
        ShowFrame();
    }
    statusBar()->showMessage(
        tr("Removed %n unused definition(s) from %1", nullptr, static_cast<int>(removed->size()))
            .arg(name));
}

}
