#include "editor_icons.h"

#include <QPainter>
#include <QPixmap>
#include <QRectF>
#include <QString>
#include <QSvgRenderer>

namespace Editor::Icons {

namespace {

QString Named(Glyph glyph) {
    switch (glyph) {
    case Glyph::Undo:
        return "undo-2";
    case Glyph::Redo:
        return "redo-2";
    case Glyph::Clock:
        return "history";
    case Glyph::Search:
        return "search";
    case Glyph::File:
        return "file";
    case Glyph::Folder:
        return "folder";
    case Glyph::Upload:
        return "upload";
    case Glyph::Save:
        return "save";
    case Glyph::Chevron:
        return "chevron-down";
    case Glyph::Preview:
        return "monitor-play";
    case Glyph::Plus:
        return "plus";
    case Glyph::Minus:
        return "minus";
    case Glyph::Cross:
        return "x";
    case Glyph::Dots:
        return "ellipsis";
    case Glyph::Frame:
        return "camera";
    case Glyph::Eye:
        return "eye";
    case Glyph::Lock:
        return "lock";
    case Glyph::Solo:
        return "circle-dot";
    case Glyph::Rulers:
        return "ruler";
    case Glyph::Snap:
        return "magnet";
    case Glyph::Onion:
        return "layers-2";
    case Glyph::Path:
        return "spline";
    case Glyph::Background:
        return "image";
    case Glyph::Fit:
        return "maximize";
    case Glyph::Select:
        return "mouse-pointer-2";
    case Glyph::Anchor:
        return "crosshair";
    case Glyph::Pan:
        return "hand";
    case Glyph::Zoom:
        return "zoom-in";
    case Glyph::Sketch:
        return "signature";
    case Glyph::First:
        return "skip-back";
    case Glyph::Previous:
        return "step-back";
    case Glyph::Play:
        return "play";
    case Glyph::Next:
        return "step-forward";
    case Glyph::Last:
        return "skip-forward";
    case Glyph::Key:
        return "diamond";
    case Glyph::Loop:
        return "repeat";
    case Glyph::Trash:
        return "trash-2";
    }
    return {};
}

}

QPixmap Drawn(Glyph glyph, const QColor& colour, int side) {
    QPixmap drawn(side, side);
    drawn.fill(Qt::transparent);
    QSvgRenderer renderer(QString(":/icons/%1.svg").arg(Named(glyph)));
    QPainter painter(&drawn);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, side, side));
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(drawn.rect(), colour);
    return drawn;
}

QIcon Of(Glyph glyph, const QColor& colour, int side) {
    QIcon icon(Drawn(glyph, colour, side));
    icon.addPixmap(Drawn(glyph, colour, side * 2));
    return icon;
}

QIcon Toggling(Glyph glyph, const QColor& off, const QColor& on, int side) {
    QIcon icon = Of(glyph, off, side);
    icon.addPixmap(Drawn(glyph, on, side), QIcon::Normal, QIcon::On);
    icon.addPixmap(Drawn(glyph, on, side * 2), QIcon::Normal, QIcon::On);
    return icon;
}

}
