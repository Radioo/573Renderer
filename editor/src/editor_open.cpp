#include "editor_open.h"

#include "editor_files.h"
#include "editor_rows.h"
#include "editor_thumbnail.h"

#include "document/animation_settings.h"
#include "document/playback.h"

#include <QChar>
#include <QObject>

#include <cstdint>
#include <string>
#include <utility>

namespace Editor {

namespace {

constexpr int kCrossGlyph = 0x00D7;

int CountNodes(const std::vector<Document::Node>& nodes) {
    int total = 0;
    for (const Document::Node& node : nodes)
        total += 1 + CountNodes(node.children);
    return total;
}

std::vector<const Document::Node*> ChildrenWith(const std::vector<Document::Node>& nodes,
                                                Document::Role role) {
    std::vector<const Document::Node*> found;
    for (const Document::Node& node : nodes) {
        for (const Document::Node& child : node.children) {
            if (child.role == role) found.push_back(&child);
        }
    }
    return found;
}

AnimationRow ReadAnimationRow(Document::File& file, const Document::Node& node) {
    AnimationRow row{.name = QString::fromStdString(node.name),
                     .path = QString::fromStdString(node.path),
                     .frames = {},
                     .stage = {},
                     .rate = {},
                     .detail = {}};
    const auto animation = file.ReadAnimation(node.path);
    if (!animation) return row;
    const Document::StageSize stage = Document::StageSizeOf(*animation);
    const QString rate = QString::number(Document::FrameRate(*animation), 'g', 4);
    row.frames = QString::number(animation->root.frames.size());
    row.stage = QObject::tr("%1x%2").arg(stage.width).arg(stage.height);
    row.rate = rate;
    row.detail = QObject::tr("%1 frames, %2 %5 %3, %4 fps")
                     .arg(animation->root.frames.size())
                     .arg(stage.width)
                     .arg(stage.height)
                     .arg(rate)
                     .arg(QChar(kCrossGlyph));
    return row;
}

ImageRow ReadImageRow(Document::File& file, const Document::Node& node) {
    ImageRow row{.name = QString::fromStdString(node.name),
                 .path = QString::fromStdString(node.path),
                 .size = {},
                 .detail = {},
                 .picture = {}};
    const auto pixels = file.ReadImage(node.name);
    if (!pixels) return row;
    row.size = QObject::tr("%1x%2").arg(pixels->width).arg(pixels->height);
    row.detail =
        QObject::tr("%1 %3 %2").arg(pixels->width).arg(pixels->height).arg(QChar(kCrossGlyph));
    row.picture =
        Thumbnail(pixels->bgra, static_cast<int>(pixels->width), static_cast<int>(pixels->height),
                  QSize(Rows::kThumbWidth, Rows::kThumbHeight));
    return row;
}

}

PackageRows ReadRows(Document::File& file, const Moved& moved) {
    PackageRows rows;
    const std::vector<const Document::Node*> animations =
        ChildrenWith(file.Nodes(), Document::Role::Animation);
    const std::vector<const Document::Node*> images =
        ChildrenWith(file.Nodes(), Document::Role::Texture);
    const int total = static_cast<int>(animations.size() + images.size());
    int done = 0;
    for (const Document::Node* node : animations) {
        moved(done++, total, QObject::tr("Reading %1").arg(QString::fromStdString(node->name)));
        rows.animations.push_back(ReadAnimationRow(file, *node));
    }
    for (const Document::Node* node : images) {
        moved(done++, total, QObject::tr("Reading %1").arg(QString::fromStdString(node->name)));
        rows.images.push_back(ReadImageRow(file, *node));
    }
    moved(total, total, QString());
    return rows;
}

OpenedPackage OpenPackage(const QString& path, const Moved& moved) {
    OpenedPackage opened;
    moved(0, 0, QObject::tr("Reading the file"));
    const std::vector<uint8_t> bytes = ReadFileBytes(path);
    if (bytes.empty()) {
        opened.refusal = QObject::tr("%1 is empty or cannot be read").arg(path);
        return opened;
    }
    moved(0, 0, QObject::tr("Reading the package"));
    auto file = Document::File::Open(bytes);
    if (!file) {
        opened.refusal = QString::fromStdString(file.error());
        return opened;
    }
    opened.file = std::move(*file);
    for (const std::string& problem : opened.file->Problems())
        opened.problems.append(QString::fromStdString(problem));
    opened.entries = CountNodes(opened.file->Nodes());
    opened.rows = ReadRows(*opened.file, moved);
    if (!opened.rows.animations.empty())
        opened.first_animation = opened.rows.animations.front().path;
    return opened;
}

}
