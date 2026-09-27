#include "editor_window.h"

#include "editor_busy.h"
#include "editor_files.h"
#include "editor_jobs.h"
#include "editor_layout.h"
#include "editor_notices.h"
#include "editor_open.h"
#include "editor_viewport.h"

#include "document/history.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QTimer>

#include <utility>

namespace Editor {

void Window::ChooseDocument() {
    OfferToSave([this](bool go) {
        if (go) AskForDocument();
    });
}

void Window::AskForDocument() {
    const QSettings settings;
    const QString start = settings.value(kDocumentDirKey).toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open an IFS"), start,
                                                      tr("IFS files (*.ifs);;All files (*)"));
    if (path.isEmpty()) return;
    OpenDocument(path);
}

bool Window::Loading() const {
    return jobs_ > 0 || (resize_timer_ != nullptr && resize_timer_->isActive());
}

void Window::JobStarted() {
    jobs_++;
}

void Window::JobFinished() {
    if (jobs_ > 0) jobs_--;
}

void Window::ShowBusy(const QString& what) {
    opening_ = true;
    busy_->Say(what);
    centre_->setCurrentWidget(busy_);
    RefreshState();
}

void Window::OpenDocument(const QString& path) {
    StopPlayback();
    ShowBusy(tr("Opening %1").arg(QFileInfo(path).fileName()));
    JobStarted();
    Jobs::Start<OpenedPackage>(
        this, pool_,
        [path](QPromise<OpenedPackage>& promise) {
            promise.addResult(OpenPackage(path, [&promise](int done, int total, QString detail) {
                promise.setProgressRange(0, total);
                promise.setProgressValueAndText(done, detail);
            }));
        },
        [this, path](OpenedPackage opened) { FinishOpen(path, std::move(opened)); },
        [this](int done, int total, const QString& detail) { busy_->Move(done, total, detail); });
}

void Window::FinishOpen(const QString& path, OpenedPackage opened) {
    opening_ = false;
    JobFinished();
    if (!opened.file) {
        centre_->setCurrentIndex(file_ ? 1 : 0);
        ReportProblem(opened.refusal);
        RefreshState();
        return;
    }
    QSettings().setValue(kDocumentDirKey, QFileInfo(path).absolutePath());
    file_ = std::move(*opened.file);
    history_.Clear();
    copied_span_.reset();
    viewport_->ClearGuides();
    hidden_.clear();
    locked_.clear();
    document_path_ = path;
    package_name_ = QFileInfo(path).completeBaseName().toStdString();
    RememberRecent(path);
    CloseAnimation();
    rows_ = std::move(opened.rows);
    FillTree();
    if (!opened.first_animation.isEmpty()) SelectEntry(opened.first_animation);
    RefreshState();
    if (opened.problems.isEmpty()) {
        statusBar()->showMessage(tr("%1 entries").arg(opened.entries), kNoticeMs);
        return;
    }
    statusBar()->showMessage(tr("%1 problems in the package, the first is: %2")
                                 .arg(opened.problems.size())
                                 .arg(opened.problems.front()),
                             kProblemMs);
}

void Window::ReloadRows() {
    if (!file_) return;
    JobStarted();
    Jobs::Start<PackageRows>(
        this, pool_,
        [copy = *file_](QPromise<PackageRows>& promise) mutable {
            promise.addResult(ReadRows(copy, [&promise](int done, int total, QString detail) {
                promise.setProgressRange(0, total);
                promise.setProgressValueAndText(done, detail);
            }));
        },
        [this](PackageRows rows) {
            JobFinished();
            rows_ = std::move(rows);
            FillTree();
        });
}

void Window::Save(std::function<void(bool)> then) {
    if (!file_) {
        if (then) then(true);
        return;
    }
    if (document_path_.isEmpty()) {
        SaveAs(std::move(then));
        return;
    }
    WriteDocument(document_path_, std::move(then));
}

void Window::WriteDocument(const QString& path, std::function<void(bool)> then) {
    ShowBusy(tr("Saving %1").arg(QFileInfo(path).fileName()));
    JobStarted();
    Jobs::Start<QString>(
        this, pool_,
        [copy = *file_, path](QPromise<QString>& promise) {
            const auto encoded = copy.Encode();
            if (!encoded) {
                promise.addResult(QString::fromStdString(encoded.error()));
                return;
            }
            promise.addResult(WriteFileBytes(path, *encoded)
                                  ? QString()
                                  : QObject::tr("%1 could not be written").arg(path));
        },
        [this, path, then = std::move(then)](const QString& refusal) {
            opening_ = false;
            JobFinished();
            centre_->setCurrentIndex(file_ ? 1 : 0);
            if (!refusal.isEmpty()) {
                ReportProblem(refusal);
                RefreshState();
                if (then) then(false);
                return;
            }
            document_path_ = path;
            history_.MarkSaved();
            RefreshState();
            statusBar()->showMessage(tr("Saved %1").arg(path), kNoticeMs);
            if (then) then(true);
        });
}

void Window::SaveAs(std::function<void(bool)> then) {
    if (!file_) {
        if (then) then(true);
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the IFS"), document_path_,
                                                      tr("IFS files (*.ifs);;All files (*)"));
    if (path.isEmpty()) {
        if (then) then(false);
        return;
    }
    WriteDocument(path, std::move(then));
}

void Window::OfferToSave(std::function<void(bool)> then) {
    if (!file_ || history_.Saved()) {
        then(true);
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("IFS Editor"),
        tr("%1 has unsaved changes.").arg(QFileInfo(document_path_).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) {
        then(false);
        return;
    }
    if (answer == QMessageBox::Discard) {
        then(true);
        return;
    }
    Save(std::move(then));
}

}
