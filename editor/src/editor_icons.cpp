#include "editor_icons.h"

#include <QByteArray>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QSvgRenderer>

namespace Editor::Icons {

namespace {

QString Body(Glyph glyph) {
    switch (glyph) {
    case Glyph::Undo:
        return R"(<path d="M9 7L4 12l5 5"/><path d="M4 7h10a6 6 0 0 1 0 12h-2"/>)";
    case Glyph::Redo:
        return R"(<path d="M15 7l5 5-5 5"/><path d="M20 7H10a6 6 0 0 0 0 12h2"/>)";
    case Glyph::Clock:
        return R"(<circle cx="12" cy="12" r="8.5"/><path d="M12 7v5l3.5 2"/>)";
    case Glyph::Search:
        return R"(<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5L21 21"/>)";
    case Glyph::File:
        return R"(<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4"/>)";
    case Glyph::Folder:
        return R"(<path d="M3 6h7l2 2h9v11H3z"/>)";
    case Glyph::Upload:
        return R"(<path d="M12 15V3M7.5 7.5L12 3l4.5 4.5"/><path d="M4 13v7h16v-7"/>)";
    case Glyph::Save:
        return R"(<path d="M5 4h11l3 3v13H5z"/><path d="M8 4v5h7V4M8 20v-6h8v6"/>)";
    case Glyph::Chevron:
        return R"(<path d="M6 9l6 6 6-6"/>)";
    case Glyph::Preview:
        return R"(<rect x="6" y="6" width="12" height="12"/>)"
               R"(<path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>)";
    case Glyph::Plus:
        return R"(<path d="M12 5v14M5 12h14"/>)";
    case Glyph::Minus:
        return R"(<path d="M5 12h14"/>)";
    case Glyph::Cross:
        return R"(<path d="M6 6l12 12M18 6L6 18"/>)";
    case Glyph::Dots:
        return R"(<circle cx="5" cy="12" r="1.3" fill="%1" stroke="none"/>)"
               R"(<circle cx="12" cy="12" r="1.3" fill="%1" stroke="none"/>)"
               R"(<circle cx="19" cy="12" r="1.3" fill="%1" stroke="none"/>)";
    case Glyph::Frame:
        return R"(<path d="M3 7h13v10H3z"/><path d="M16 10l5-3v10l-5-3"/>)";
    case Glyph::Eye:
        return R"(<path d="M2.5 12s3.5-6.5 9.5-6.5 9.5 6.5 9.5 6.5-3.5 6.5-9.5 )"
               R"(6.5S2.5 12 2.5 12z"/><circle cx="12" cy="12" r="2.8"/>)";
    case Glyph::Lock:
        return R"(<rect x="5" y="11" width="14" height="9"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>)";
    case Glyph::Solo:
        return R"(<circle cx="12" cy="12" r="5"/>)";
    case Glyph::Rulers:
        return R"(<rect x="3" y="7" width="18" height="10"/>)"
               R"(<path d="M7 7v4M11 7v3M15 7v4M19 7v3"/>)";
    case Glyph::Snap:
        return R"(<path d="M6 4v8a6 6 0 0 0 12 0V4h-4v8a2 2 0 0 1-4 0V4z"/>)"
               R"(<path d="M6 8h4M14 8h4"/>)";
    case Glyph::Onion:
        return R"(<rect x="3" y="7" width="11" height="11" opacity=".45"/>)"
               R"(<rect x="7" y="4" width="11" height="11" opacity=".7"/>)"
               R"(<rect x="10" y="9" width="11" height="11"/>)";
    case Glyph::Path:
        return R"(<circle cx="5" cy="18" r="1.6"/><circle cx="19" cy="6" r="1.6"/>)"
               R"(<path d="M6.5 17C10 16 9 9 12 8s4 0 5.5-1" stroke-dasharray="2 2.2"/>)";
    case Glyph::Background:
        return R"(<rect x="4" y="4" width="16" height="16"/><path d="M4 14l6-5 10 8"/>)";
    case Glyph::Fit:
        return R"(<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/>)";
    case Glyph::Select:
        return R"(<path d="M5 3l14 8-6 1.5L10 19z" fill="%1" stroke-linejoin="round"/>)";
    case Glyph::Anchor:
        return R"(<circle cx="12" cy="12" r="4"/><path d="M12 2v6M12 16v6M2 12h6M16 12h6"/>)";
    case Glyph::Pan:
        return R"(<path d="M8 12V5.5a1.5 1.5 0 0 1 3 0V11M11 10V4.5a1.5 1.5 0 0 1 3 0V11M14 )"
               R"(10.5V6a1.5 1.5 0 0 1 3 0v8c0 4-2.5 7-6.5 7-3 0-4.5-1.5-6-4l-2-3.5a1.5 1.5 0 )"
               R"(0 1 2.5-1.5L8 14"/>)";
    case Glyph::Zoom:
        return R"(<circle cx="10.5" cy="10.5" r="6"/>)"
               R"(<path d="M15 15l5.5 5.5M8 10.5h5M10.5 8v5"/>)";
    case Glyph::Sketch:
        return R"(<path d="M3 17c3-1 4-5 7-6s4 3 7 2 3-5 4-7"/>)"
               R"(<circle cx="21" cy="6" r="1.4" fill="%1"/>)";
    case Glyph::First:
        return R"(<path d="M5 5v14"/><path d="M19 6l-7 6 7 6z"/><path d="M12 6l-6 6 6 6"/>)";
    case Glyph::Previous:
        return R"(<path d="M17 6l-8 6 8 6z"/><path d="M7 6v12"/>)";
    case Glyph::Play:
        return R"(<path d="M8 5.5v13l10.5-6.5z" fill="%1" stroke="none"/>)";
    case Glyph::Next:
        return R"(<path d="M7 6l8 6-8 6z"/><path d="M17 6v12"/>)";
    case Glyph::Last:
        return R"(<path d="M19 5v14"/><path d="M5 6l7 6-7 6z"/><path d="M12 6l6 6-6 6"/>)";
    case Glyph::Key:
        return R"(<path d="M12 7l5 5-5 5-5-5z" fill="%1"/>)";
    case Glyph::Loop:
        return R"(<path d="M4 12a6 6 0 0 1 6-6h8l-3-3M20 12a6 6 0 0 1-6 6H6l3 3"/>)";
    case Glyph::Trash:
        return R"(<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/>)";
    }
    return {};
}

}

QPixmap Drawn(Glyph glyph, const QColor& colour, int side) {
    const QString body = Body(glyph).replace("%1", colour.name(QColor::HexRgb));
    const QString document =
        QString(R"(<svg xmlns="http://www.w3.org/2000/svg" width="%1" height="%1" )"
                R"(viewBox="0 0 24 24" fill="none" stroke="%2" stroke-width="1.6" )"
                R"(stroke-linecap="round" stroke-linejoin="round">%3</svg>)")
            .arg(side)
            .arg(colour.name(QColor::HexRgb), body);
    QSvgRenderer renderer(document.toUtf8());
    QPixmap drawn(side, side);
    drawn.fill(Qt::transparent);
    QPainter painter(&drawn);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, side, side));
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
