#include "editor_window.h"

#include "editor_files.h"

#include "document/document.h"
#include "support/expected.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QSettings>
#include <QStatusBar>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

namespace {

struct Picture {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgra;
};

std::optional<Picture> ReadPicture(const QString& path) {
    QImage picture(path);
    if (picture.isNull()) return std::nullopt;
    picture = picture.convertToFormat(QImage::Format_ARGB32);
    Picture read{.width = static_cast<uint32_t>(picture.width()),
                 .height = static_cast<uint32_t>(picture.height()),
                 .bgra = {}};
    read.bgra.reserve(static_cast<std::size_t>(picture.width()) * picture.height() * 4);
    for (int y = 0; y < picture.height(); y++) {
        const auto* row = picture.constScanLine(y);
        read.bgra.insert(read.bgra.end(), row,
                         row + static_cast<std::ptrdiff_t>(picture.width()) * 4);
    }
    return read;
}

QString AskForPicture(QWidget* parent, const QString& title) {
    return QFileDialog::getOpenFileName(parent, title, QString(),
                                        QObject::tr("Images (*.png *.bmp *.jpg);;All files (*)"));
}

}

void Window::AddImageFromFile() {
    const QString file = AskForPicture(this, tr("Add an image"));
    if (file.isEmpty()) return;
    auto picture = ReadPicture(file);
    if (!picture) {
        ReportProblem(tr("%1 is not an image Qt can read").arg(file));
        return;
    }
    const std::string logical = QFileInfo(file).completeBaseName().toStdString();
    EditDocument(tr("Add image %1").arg(QString::fromStdString(logical)),
                 [&logical, &picture](Document::File& document) {
                     return document.AddImage(logical, picture->width, picture->height,
                                              picture->bgra);
                 });
}

void Window::ReplaceImageWithPicture(const QString& name) {
    const QString file = AskForPicture(this, tr("Replace %1").arg(name));
    if (file.isEmpty()) return;
    auto picture = ReadPicture(file);
    if (!picture) {
        ReportProblem(tr("%1 is not an image Qt can read").arg(file));
        return;
    }
    const std::string image = name.toStdString();
    EditDocument(tr("Replace %1").arg(name), [&image, &picture](Document::File& document) {
        return document.ReplaceImage(image, picture->width, picture->height, picture->bgra);
    });
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
