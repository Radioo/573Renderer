#include "editor_window.h"

#include "editor_busy.h"
#include "editor_jobs.h"

#include "editor_files.h"
#include "editor_viewport.h"

#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/expected.h"

#include <QColor>
#include <QChar>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QProgressDialog>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QSize>
#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace Editor {

namespace {

constexpr int kFrameDigits = 4;

QImage Opaque(const QImage& image) {
    QImage opaque(image.size(), QImage::Format_RGB32);
    opaque.fill(QColor(0, 0, 0));
    QPainter painter(&opaque);
    painter.drawImage(0, 0, image);
    return opaque;
}

}

Support::Expected<Window::ShownFrame, std::string> Window::ReadFrame() {
    auto frame = host_.Render();
    if (!frame) return Support::Unexpected(frame.error());
    if (!reader_) {
        auto reader = SharedTexture::Reader::Create();
        if (!reader) return Support::Unexpected(reader.error());
        reader_ = std::move(*reader);
    }
    const auto pixels = reader_->Read(frame->shared_handle, frame->width, frame->height);
    if (!pixels) return Support::Unexpected(pixels.error());
    const QImage image(pixels->data(), static_cast<int>(frame->width),
                       static_cast<int>(frame->height), QImage::Format_ARGB32);
    return ShownFrame{.image = image.copy(), .frame = *frame};
}

void Window::SaveFrameAs() {
    if (!HostReady() || animation_name_.empty() || stage_size_.isEmpty()) {
        ReportProblem(tr("Saving a frame needs the preview, so choose a game install first"));
        return;
    }
    QSettings settings;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the frame"),
                                                      settings.value(kDocumentDirKey).toString(),
                                                      tr("PNG images (*.png)"));
    if (path.isEmpty()) return;
    const auto resized = host_.Resize(static_cast<uint32_t>(stage_size_.width()),
                                      static_cast<uint32_t>(stage_size_.height()));
    auto shown =
        resized ? ReadFrame()
                : Support::Expected<ShownFrame, std::string>(Support::Unexpected(resized.error()));
    ResizeViewport();
    if (!shown) {
        ReportProblem(QString::fromStdString(shown.error()));
        return;
    }
    if (!Opaque(shown->image).save(path, "PNG")) {
        ReportProblem(tr("%1 could not be written").arg(path));
        return;
    }
    statusBar()->showMessage(tr("Saved frame %1 to %2").arg(shown->frame.frame).arg(path));
}

void Window::SaveFramesAs() {
    if (!HostReady() || animation_name_.empty() || stage_size_.isEmpty()) {
        ReportProblem(tr("Saving frames needs the preview, so choose a game install first"));
        return;
    }
    if (!OutlinesMatchView()) {
        ReportProblem(tr("The preview shows the whole animation while a sprite is being edited. "
                         "Show the sprite on its own to save its frames."));
        return;
    }
    const uint32_t count = ClipFrameCount();
    if (count == 0) return;
    const uint32_t first = work_area_ ? work_area_->first_frame : 0;
    const uint32_t last = work_area_ ? std::min(work_area_->last_frame, count - 1) : count - 1;
    const QSettings settings;
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Save frames %1 to %2 into").arg(first).arg(last),
        settings.value(kDocumentDirKey).toString());
    if (folder.isEmpty()) return;
    StopPlayback();
    const auto resized = host_.Resize(static_cast<uint32_t>(stage_size_.width()),
                                      static_cast<uint32_t>(stage_size_.height()));
    if (!resized) {
        ReportProblem(QString::fromStdString(resized.error()));
        return;
    }
    frames_ = FrameExport{.folder = folder,
                          .base = QString::fromStdString(animation_name_),
                          .frame = first,
                          .first = first,
                          .last = last,
                          .saved = 0,
                          .stop = false};
    ShowBusy(tr("Saving frames %1 to %2").arg(first).arg(last));
    busy_->AllowStopping(true);
    JobStarted();
    QTimer::singleShot(0, this, &Window::SaveNextFrame);
}

void Window::StopFrames() {
    if (frames_) frames_->stop = true;
}

void Window::SaveNextFrame() {
    if (!frames_) return;
    if (frames_->stop || frames_->frame > frames_->last) {
        FinishFrames(QString());
        return;
    }
    const auto total = static_cast<int>(frames_->last - frames_->first + 1);
    busy_->Move(frames_->saved, total,
                tr("Frame %1, %2 of %3").arg(frames_->frame).arg(frames_->saved + 1).arg(total));
    const auto sought = host_.Seek(frames_->frame);
    auto shown =
        sought ? ReadFrame()
               : Support::Expected<ShownFrame, std::string>(Support::Unexpected(sought.error()));
    if (!shown) {
        FinishFrames(QString::fromStdString(shown.error()));
        return;
    }
    const QString path = QDir(frames_->folder)
                             .filePath(QString("%1_%2.png")
                                           .arg(frames_->base)
                                           .arg(frames_->frame, kFrameDigits, 10, QChar('0')));
    frames_->frame++;
    frames_->saved++;
    JobStarted();
    Jobs::Start<QString>(
        this, pool_,
        [picture = Opaque(shown->image), path](QPromise<QString>& promise) {
            promise.addResult(picture.save(path, "PNG")
                                  ? QString()
                                  : QObject::tr("%1 could not be written").arg(path));
        },
        [this](const QString& refusal) {
            JobFinished();
            if (!refusal.isEmpty()) {
                FinishFrames(refusal);
                return;
            }
            SaveNextFrame();
        });
}

void Window::FinishFrames(const QString& refusal) {
    if (!frames_) return;
    const FrameExport done = *frames_;
    frames_.reset();
    busy_->AllowStopping(false);
    opening_ = false;
    JobFinished();
    centre_->setCurrentIndex(file_ ? 1 : 0);
    ResizeViewport();
    SeekViewport(symbol_shown_ ? frame_ : root_frame_);
    RefreshState();
    if (!refusal.isEmpty()) ReportProblem(refusal);
    const auto total = static_cast<int>(done.last - done.first + 1);
    statusBar()->showMessage(
        tr("Saved %1 of %2 frames to %3").arg(done.saved).arg(total).arg(done.folder), kNoticeMs);
}

}
