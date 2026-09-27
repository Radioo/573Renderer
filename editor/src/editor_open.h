#pragma once

#include "document/document.h"

#include <QImage>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

namespace Editor {

struct AnimationRow {
    QString name;
    QString path;
    QString frames;
    QString stage;
    QString rate;
    QString detail;
};

struct ImageRow {
    QString name;
    QString path;
    QString size;
    QString detail;
    QImage picture;
};

struct PackageRows {
    std::vector<AnimationRow> animations;
    std::vector<ImageRow> images;
};

struct OpenedPackage {
    std::optional<Document::File> file;
    QString refusal;
    QString first_animation;
    QStringList problems;
    PackageRows rows;
    int entries = 0;
};

using Moved = std::function<void(int, int, QString)>;

[[nodiscard]] PackageRows ReadRows(Document::File& file, const Moved& moved);

[[nodiscard]] OpenedPackage OpenPackage(const QString& path, const Moved& moved);

}
