#include "editor_window.h"

#include "editor_files.h"
#include "editor_viewport.h"

#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/expected.h"

#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QSize>
#include <QStatusBar>
#include <QString>

#include <cstdint>
#include <string>
#include <utility>

namespace Editor {

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
    if (!host_.Running() || animation_name_.empty() || stage_size_.isEmpty()) {
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
    QImage opaque(shown->image.size(), QImage::Format_RGB32);
    opaque.fill(QColor(0, 0, 0));
    {
        QPainter painter(&opaque);
        painter.drawImage(0, 0, shown->image);
    }
    if (!opaque.save(path, "PNG")) {
        ReportProblem(tr("%1 could not be written").arg(path));
        return;
    }
    statusBar()->showMessage(tr("Saved frame %1 to %2").arg(shown->frame.frame).arg(path));
}

void Window::SaveImageAs(const QString& name) {
    if (!file_) return;
    const auto read = file_->ReadImage(name.toStdString());
    if (!read) {
        ReportProblem(QString::fromStdString(read.error()));
        return;
    }
    const QSettings settings;
    const QString start = QDir(settings.value(kDocumentDirKey).toString()).filePath(name + ".png");
    const QString path = QFileDialog::getSaveFileName(this, tr("Save %1").arg(name), start,
                                                      tr("PNG images (*.png)"));
    if (path.isEmpty()) return;
    const QImage image(read->bgra.data(), static_cast<int>(read->width),
                       static_cast<int>(read->height), QImage::Format_ARGB32);
    if (!image.save(path, "PNG")) {
        ReportProblem(tr("%1 could not be written").arg(path));
        return;
    }
    statusBar()->showMessage(tr("Saved %1 to %2").arg(name, path));
}

}
