#include "editor_window.h"

#include "editor_files.h"

#include "document/authored.h"
#include "support/expected.h"
#include "document/document.h"
#include "document/keyframes.h"
#include "document/project.h"
#include "document/project_export.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

std::string Window::TargetBuild() const {
    if (project_ && !project_->build.empty()) return project_->build;
    return std::string(kBuildSlug);
}

void Window::CreateProject() {
    if (!file_ || document_path_.isEmpty()) {
        ReportProblem(tr("Open an IFS before making a project for it"));
        return;
    }
    QSettings settings;
    const QString start = settings.value(kProjectDirKey).toString();
    const QString folder =
        QFileDialog::getExistingDirectory(this, tr("Choose a folder for the project"), start);
    if (folder.isEmpty()) return;
    const QString manifest =
        QString::fromStdString(Document::ProjectManifestPath(folder.toStdString()));
    if (QFileInfo::exists(manifest)) {
        ReportProblem(tr("%1 already holds a project").arg(folder));
        return;
    }
    Document::Project project{
        .build = TargetBuild(),
        .ifs_path = Document::StoredIfsPath(
            folder.toStdString(), QFileInfo(document_path_).absoluteFilePath().toStdString())};
    if (!WriteFileBytes(manifest, Document::WriteProject(project))) {
        ReportProblem(tr("%1 could not be written").arg(manifest));
        return;
    }
    settings.setValue(kProjectDirKey, folder);
    project_ = std::move(project);
    project_folder_ = folder;
    authored_.clear();
    RefreshState();
    statusBar()->showMessage(tr("Project made in %1").arg(folder));
}

void Window::ChooseProject() {
    QSettings settings;
    const QString start = settings.value(kProjectDirKey).toString();
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Open a project"), start);
    if (folder.isEmpty()) return;
    OpenProject(folder);
}

void Window::OpenProject(const QString& folder) {
    const QString manifest =
        QString::fromStdString(Document::ProjectManifestPath(folder.toStdString()));
    const std::vector<uint8_t> bytes = ReadFileBytes(manifest);
    if (bytes.empty()) {
        ReportProblem(tr("%1 holds no project").arg(folder));
        return;
    }
    auto project = Document::ReadProject(bytes);
    if (!project) {
        ReportProblem(QString::fromStdString(project.error()));
        return;
    }
    const QString ifs =
        QString::fromStdString(Document::ResolvedIfsPath(folder.toStdString(), *project));
    if (!QFileInfo::exists(ifs)) {
        ReportProblem(tr("The project names %1, which is not there").arg(ifs));
        return;
    }
    if (!OfferToSave()) return;
    OpenDocument(ifs);
    if (QFileInfo(document_path_).absoluteFilePath() != QFileInfo(ifs).absoluteFilePath()) return;
    QSettings().setValue(kProjectDirKey, folder);
    authored_ = project->content;
    project_ = std::move(*project);
    project_folder_ = folder;
    if (host_.Running()) StartHost(QSettings().value(kGameDirKey).toString());
    RefreshState();
    statusBar()->showMessage(tr("Project open in %1").arg(folder));
}

void Window::CloseProject() {
    if (!project_) return;
    authored_.clear();
    project_.reset();
    project_folder_.clear();
    RefreshState();
    ShowFrame();
    statusBar()->showMessage(tr("Project closed. The IFS is open on its own."));
}

const Document::AuthoredDepth* Window::AuthoredAt(uint16_t depth, uint32_t frame) const {
    for (const Document::AuthoredDepth& owned : authored_) {
        if (owned.animation == animation_path_ && owned.depth == depth &&
            frame >= owned.first_frame && frame <= owned.last_frame) {
            return &owned;
        }
    }
    return nullptr;
}

void Window::SaveProject() {
    if (!project_ || project_folder_.isEmpty()) return;
    project_->content = authored_;
    const QString manifest =
        QString::fromStdString(Document::ProjectManifestPath(project_folder_.toStdString()));
    if (!WriteFileBytes(manifest, Document::WriteProject(*project_)))
        ReportProblem(tr("%1 could not be written").arg(manifest));
}

void Window::ExportToPackage() {
    if (!file_ || !project_) return;
    project_->content = authored_;
    Document::File before = *file_;
    const auto exported = Document::ExportProject(*file_, *project_);
    if (!exported) {
        file_ = std::move(before);
        ReportProblem(QString::fromStdString(exported.error()));
        return;
    }
    history_.Record(tr("Export").toStdString(), std::move(before));
    SaveProject();
    RefreshState();
    Reload();
    ShowFrame();
    statusBar()->showMessage(tr("Exported %1 owned depth(s) into the IFS").arg(authored_.size()));
}

void Window::OwnSelectedDepth(uint32_t frame) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    auto owned =
        Document::OwnDepth(*animation, animation_path_, static_cast<uint16_t>(*depth_), frame);
    if (!owned) {
        ReportProblem(QString::fromStdString(owned.error()));
        return;
    }
    authored_.push_back(std::move(owned->authored));
    SaveProject();
    RefreshState();
    ShowFrame();
    statusBar()->showMessage(tr("Depth %1 is now the project's, from frame %2 to %3")
                                 .arg(*depth_)
                                 .arg(authored_.back().first_frame)
                                 .arg(authored_.back().last_frame));
}

void Window::DetachSelectedDepth() {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const Document::AuthoredDepth* owned = AuthoredAt(static_cast<uint16_t>(*depth_), frame_);
    if (owned == nullptr) return;
    const Document::AuthoredDepth taken = *owned;
    EditAnimation(tr("Detach depth %1").arg(*depth_), [taken](AfpAnimation::Animation& clip) {
        using Written = Support::Expected<void, std::string>;
        auto baked = Document::BakedFor(clip, taken);
        if (!baked) return Written(Support::Unexpected(baked.error()));
        return Document::WriteAuthored(clip, taken, *baked);
    });
    std::erase_if(authored_, [&taken](const Document::AuthoredDepth& one) {
        return one.animation == taken.animation && one.depth == taken.depth &&
               one.first_frame == taken.first_frame;
    });
    SaveProject();
    RefreshState();
    ShowFrame();
}
}
