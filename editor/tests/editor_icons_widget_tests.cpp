#include <catch2/catch_test_macros.hpp>

#include "editor_icons.h"

#include <QColor>
#include <QImage>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace {

constexpr int kSide = 32;
constexpr int kSmallSide = 14;
constexpr int kInked = 32;
constexpr int kSolid = 255;

struct Drawing {
    int inked = 0;
    int stray = 0;
};

Drawing Look(const QImage& shown, const QColor& wanted) {
    Drawing drawing;
    for (int y = 0; y < shown.height(); y++) {
        for (int x = 0; x < shown.width(); x++) {
            const QColor found = shown.pixelColor(x, y);
            if (found.alpha() < kInked) continue;
            drawing.inked++;
            if (found.alpha() == kSolid && found.rgb() != wanted.rgb()) drawing.stray++;
        }
    }
    return drawing;
}

}

TEST_CASE("Every glyph draws its file and takes the colour it is asked for") {
    const QColor wanted(0xd0, 0x40, 0x20);
    for (const int side : {kSmallSide, kSide}) {
        QStringList blank;
        QStringList wrong;
        for (uint8_t at = 0; at <= static_cast<uint8_t>(Editor::Icons::Glyph::Trash); at++) {
            const auto glyph = static_cast<Editor::Icons::Glyph>(at);
            const QImage shown = Editor::Icons::Drawn(glyph, wanted, side).toImage();
            const Drawing drawing = Look(shown, wanted);
            if (drawing.inked == 0) blank.append(QString::number(at));
            if (drawing.stray > 0) wrong.append(QString::number(at));
        }
        CHECK(QString("%1: %2").arg(side).arg(blank.join(", ")).toStdString() ==
              QString("%1: ").arg(side).toStdString());
        CHECK(QString("%1: %2").arg(side).arg(wrong.join(", ")).toStdString() ==
              QString("%1: ").arg(side).toStdString());
    }
}
