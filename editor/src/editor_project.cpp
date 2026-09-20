#include "editor_window.h"

#include "editor_jobs.h"

#include "editor_drift_sheet.h"

#include "editor_files.h"

#include "document/authored.h"
#include "support/expected.h"
#include "document/document.h"
#include "document/keyframes.h"
#include "document/project.h"
#include "document/project_drift.h"
#include "document/project_export.h"
#include "document/script_source.h"

#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QFileInfo>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <cstddef>
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

QString Window::SuggestedProjectFolder() const {
    const QFileInfo ifs(document_path_);
    return ifs.dir().filePath(ifs.completeBaseName() + " project");
}

bool Window::OpenOrMakeProject(const QString& folder) {
    const QString manifest =
        QString::fromStdString(Document::ProjectManifestPath(folder.toStdString()));
    if (!QFileInfo::exists(manifest)) return MakeProjectIn(folder);
    const auto project = Document::ReadProject(ReadFileBytes(manifest));
    if (!project) {
        ReportProblem(QString::fromStdString(project.error()));
        return false;
    }
    QSettings().setValue(kProjectDirKey, folder);
    authored_ = project->content;
    project_ = *project;
    project_folder_ = folder;
    RefreshState();
    return true;
}

bool Window::MakeProjectIn(const QString& folder) {
    const QString manifest =
        QString::fromStdString(Document::ProjectManifestPath(folder.toStdString()));
    if (QFileInfo::exists(manifest)) {
        ReportProblem(tr("%1 already holds a project").arg(folder));
        return false;
    }
    if (!QDir().mkpath(folder)) {
        ReportProblem(tr("%1 could not be made").arg(folder));
        return false;
    }
    Document::Project project{
        .build = TargetBuild(),
        .ifs_path = Document::StoredIfsPath(
            folder.toStdString(), QFileInfo(document_path_).absoluteFilePath().toStdString())};
    if (!WriteFileBytes(manifest, Document::WriteProject(project))) {
        ReportProblem(tr("%1 could not be written").arg(manifest));
        return false;
    }
    QSettings().setValue(kProjectDirKey, folder);
    project_ = std::move(project);
    project_folder_ = folder;
    authored_.clear();
    RefreshState();
    return true;
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
    OfferToSave([this, ifs, folder, kept = std::move(*project)](bool go) mutable {
        if (!go) return;
        OpenDocument(ifs);
        FinishProjectOpen(ifs, folder, std::move(kept));
    });
}

void Window::FinishProjectOpen(const QString& ifs, const QString& folder,
                               Document::Project project) {
    if (Loading()) {
        QTimer::singleShot(0, this, [this, ifs, folder, kept = std::move(project)]() mutable {
            FinishProjectOpen(ifs, folder, std::move(kept));
        });
        return;
    }
    if (QFileInfo(document_path_).absoluteFilePath() != QFileInfo(ifs).absoluteFilePath()) return;
    QSettings().setValue(kProjectDirKey, folder);
    authored_ = project.content;
    project_ = std::move(project);
    project_folder_ = folder;
    if (host_.Running()) StartHost(QSettings().value(kGameDirKey).toString());
    RefreshState();
    statusBar()->showMessage(tr("Project open in %1").arg(folder), kNoticeMs);
    ReportDrift();
}

void Window::ReportDrift() {
    if (!file_ || !project_) return;
    const std::vector<Document::DriftedEntry> drift = Document::ProjectDrift(*file_, *project_);
    if (drift.empty()) return;

    std::vector<DriftRow> rows;
    rows.reserve(drift.size());
    for (const Document::DriftedEntry& entry : drift) {
        rows.push_back(DriftRow{.path = QString::fromStdString(entry.path),
                                .missing = entry.kind == Document::DriftKind::Missing});
    }
    DriftSheet sheet(rows, this);
    const int answered = sheet.exec();
    statusBar()->showMessage(tr("%1 entry(s) had changed since the last export").arg(drift.size()));
    if (answered != QDialog::Accepted) return;
    for (const QString& path : sheet.Kept())
        Document::KeepIfsVersion(*project_, path.toStdString());
    authored_ = project_->content;
    SaveProject();
    RefreshState();
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
    const std::optional<std::size_t> at = AuthoredIndexAt(depth, frame);
    return at ? &authored_[*at] : nullptr;
}

void Window::SaveProject() {
    if (!project_ || project_folder_.isEmpty()) return;
    project_->content = authored_;
    const QString manifest =
        QString::fromStdString(Document::ProjectManifestPath(project_folder_.toStdString()));
    if (!WriteFileBytes(manifest, Document::WriteProject(*project_)))
        ReportProblem(tr("%1 could not be written").arg(manifest));
}

namespace {

Support::Expected<Document::LoadedImage, std::string> LoadImage(const QString& path) {
    QImage picture(path);
    if (picture.isNull())
        return Support::Unexpected(path.toStdString() + " is not an image Qt can read");
    picture = picture.convertToFormat(QImage::Format_ARGB32);
    Document::LoadedImage out{.width = static_cast<uint32_t>(picture.width()),
                              .height = static_cast<uint32_t>(picture.height()),
                              .bgra = {}};
    out.bgra.reserve(static_cast<std::size_t>(picture.width()) * picture.height() * 4);
    for (int y = 0; y < picture.height(); y++) {
        const auto* row = picture.constScanLine(y);
        out.bgra.insert(out.bgra.end(), row,
                        row + static_cast<std::ptrdiff_t>(picture.width()) * 4);
    }
    return out;
}

}

void Window::AddProjectImage() {
    if (!project_ || project_folder_.isEmpty()) return;
    const QString picked =
        QFileDialog::getOpenFileName(this, tr("Add an image to the project"), QString(),
                                     tr("Images (*.png *.bmp *.jpg);;All files (*)"));
    if (picked.isEmpty()) return;
    bool answered = false;
    const QString name =
        QInputDialog::getText(this, tr("Add an image to the project"), tr("Image name"),
                              QLineEdit::Normal, QFileInfo(picked).completeBaseName(), &answered);
    if (!answered || name.isEmpty()) return;
    for (const Document::SourceImage& image : project_->images) {
        if (image.name == name.toStdString()) {
            ReportProblem(tr("The project already owns an image called %1").arg(name));
            return;
        }
    }

    const QString sources =
        project_folder_ + "/" +
        QString::fromUtf8(Document::kProjectSourceDirectory.data(),
                          static_cast<int>(Document::kProjectSourceDirectory.size()));
    if (!QDir().mkpath(sources)) {
        ReportProblem(tr("%1 could not be made").arg(sources));
        return;
    }
    const QString file = QString::fromStdString(std::string(Document::kProjectSourceDirectory)) +
                         "/" + name + "." + QFileInfo(picked).suffix();
    const QString target = project_folder_ + "/" + file;
    QFile::remove(target);
    if (!QFile::copy(picked, target)) {
        ReportProblem(tr("%1 could not be copied into the project").arg(picked));
        return;
    }
    project_->images.push_back(
        Document::SourceImage{.name = name.toStdString(), .file = file.toStdString()});
    SaveProject();
    RefreshState();
    statusBar()->showMessage(tr("The project owns %1 image(s)").arg(project_->images.size()));
}

void Window::EditOwnedScript() {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const Document::AuthoredDepth* owned = AuthoredAt(static_cast<uint16_t>(*depth_), frame_);
    if (owned == nullptr) return;
    QString source = QString::fromStdString(owned->script.value_or(std::string()));
    if (source.isEmpty()) {
        const auto animation = file_->ReadAnimation(animation_path_);
        if (animation) {
            const auto baked = Document::BakedFor(*animation, *owned);
            if (baked && baked->create.clip_actions &&
                !baked->create.clip_actions->events.empty()) {
                const std::optional<std::string> text = Document::ScriptSourceText(
                    *animation, baked->create.clip_actions->events.front().bytecode);
                if (text) source = QString::fromStdString(*text);
            }
        }
    }

    bool answered = false;
    const QString edited = QInputDialog::getMultiLineText(
        this, tr("The script of depth %1").arg(*depth_),
        tr("One aeplib call per line, such as gotoAndPlay(\"loop\")"), source, &answered);
    if (!answered) return;

    const std::string taken = edited.trimmed().toStdString();
    if (!EditAuthored(
            tr("Script of depth %1").arg(*depth_), [&taken](Document::AuthoredDepth& one) {
                one.script = taken.empty() ? std::nullopt : std::optional<std::string>(taken);
                return Support::Expected<void, std::string>{};
            })) {
        return;
    }
    statusBar()->showMessage(taken.empty()
                                 ? tr("Depth %1 keeps the script it was given").arg(*depth_)
                                 : tr("The project writes the script of depth %1").arg(*depth_));
}

void Window::ExportToPackage() {
    if (!file_ || !project_) return;
    project_->content = authored_;
    const QString folder = project_folder_;
    ShowBusy(tr("Exporting into %1").arg(QFileInfo(document_path_).fileName()));
    JobStarted();
    Jobs::Start<ExportedProject>(
        this, pool_,
        [copy = *file_, project = *project_, folder](QPromise<ExportedProject>& promise) mutable {
            ExportedProject made;
            const auto exported =
                Document::ExportProject(copy, project, [folder](const std::string& file) {
                    return LoadImage(QString::fromStdString(
                        Document::ProjectSourcePath(folder.toStdString(), file)));
                });
            if (!exported) {
                made.refusal = QString::fromStdString(exported.error());
                promise.addResult(std::move(made));
                return;
            }
            made.file = std::move(copy);
            made.project = std::move(project);
            promise.addResult(std::move(made));
        },
        [this](ExportedProject made) {
            opening_ = false;
            JobFinished();
            centre_->setCurrentIndex(file_ ? 1 : 0);
            if (!made.file) {
                ReportProblem(made.refusal);
                RefreshState();
                return;
            }
            Document::File before = *file_;
            file_ = std::move(*made.file);
            project_ = std::move(*made.project);
            history_.Record(tr("Export").toStdString(),
                            Document::Snapshot{.file = std::move(before), .authored = authored_});
            SaveProject();
            RefreshState();
            Reload();
            ShowFrame();
            ShowResult(tr("Exported %1 owned depth(s) into the IFS").arg(authored_.size()), true);
        });
}

void Window::OwnSelectedDepth(uint32_t frame) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    if (!project_ && !OpenOrMakeProject(SuggestedProjectFolder())) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    auto owned = Document::OwnDepth(*animation, clip_, animation_path_,
                                    static_cast<uint16_t>(*depth_), frame);
    if (!owned) {
        ReportProblem(QString::fromStdString(owned.error()));
        return;
    }
    history_.Record(tr("Own depth %1").arg(*depth_).toStdString(),
                    Document::Snapshot{.file = *file_, .authored = authored_});
    authored_.push_back(std::move(owned->authored));
    SaveProject();
    RefreshState();
    ShowFrame();
    ShowResult(tr("Depth %1 is now the project's, from frame %2 to %3")
                   .arg(*depth_)
                   .arg(authored_.back().first_frame)
                   .arg(authored_.back().last_frame),
               true);
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
    std::erase_if(authored_, [&taken](const Document::AuthoredDepth& one) { return one == taken; });
    SaveProject();
    RefreshState();
    ShowFrame();
}
}
