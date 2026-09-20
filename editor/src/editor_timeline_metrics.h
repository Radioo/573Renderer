#pragma once

#include <QColor>

#include <array>
#include <cstdint>

namespace Editor {

inline constexpr int kGutterWidth = 276;
inline constexpr int kRulerHeight = 30;
inline constexpr int kNotesHeight = 22;
inline constexpr int kRowsTop = kRulerHeight + kNotesHeight;
inline constexpr int kRowHeight = 22;
inline constexpr int kNoteRadius = 4;
inline constexpr int kNoteReach = 6;
inline constexpr int kBarInset = 2;
inline constexpr int kShortestNamedBar = 24;
inline constexpr int kEyeLeft = 8;
inline constexpr int kLockLeft = 27;
inline constexpr int kSwitchWidth = 13;
inline constexpr int kSoloLeft = 46;
inline constexpr int kNumberLeft = 76;
inline constexpr int kNumberRight = 104;
inline constexpr int kNameLeft = 112;
inline constexpr int kNamePadding = 3;
inline constexpr int kNamePixels = 11;
inline constexpr int kEmptyHeight = 80;
inline constexpr int kLabelReach = 30;
inline constexpr int kLabelGrab = 4;
inline constexpr int kKeyReach = 6;
inline constexpr int kKeyRadius = 4;
inline constexpr int kPropertyIndent = 8;
inline constexpr int kSpanDragThreshold = 4;
inline constexpr int kEdgeReach = 3;
inline constexpr double kSnapPixels = 8.0;
inline constexpr double kZoomStep = 1.25;
inline constexpr double kMostPixelsPerFrame = 48.0;
inline constexpr double kZoomSlack = 1e-9;
inline constexpr int kTickSpacing = 60;
inline constexpr int kTickHeight = 6;
inline constexpr std::array<uint32_t, 10> kTickSteps{1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};

inline const QColor kRuler(0x1a, 0x1c, 0x20);
inline const QColor kGutter(0x1a, 0x1c, 0x20);
inline const QColor kGutterLine(0x14, 0x15, 0x18);
inline const QColor kRow(0x14, 0x15, 0x18);
inline const QColor kGridLine(0x1a, 0x1c, 0x20);
inline const QColor kRulerLine(0x35, 0x39, 0x41);
inline const QColor kRulerText(0x8a, 0x90, 0x9b);
inline const QColor kSpanName(0xff, 0xff, 0xff, 0xd8);
inline const QColor kSwitchOn(0xb0, 0xb5, 0xbe);
inline const QColor kSwitchOff(0x45, 0x4a, 0x52);
inline const QColor kSoloOn(0x9c, 0xc8, 0xff);
inline const QColor kPropertyRow(0x1a, 0x1c, 0x20);
inline const QColor kSelectedRow(0x1b, 0x33, 0x50, 0x66);
inline const QColor kBar(0x2f, 0x55, 0x7c);
inline const QColor kBarTop(0x5d, 0x8f, 0xc4);
inline const QColor kImageBar(0x2f, 0x55, 0x7c);
inline const QColor kShapeBar(0x2b, 0x6a, 0x5b);
inline const QColor kShapeBarTop(0x4f, 0xb5, 0x9c);
inline const QColor kSpriteBar(0x5a, 0x47, 0x87);
inline const QColor kSpriteBarTop(0x91, 0x79, 0xd1);
inline const QColor kTextBar(0x8c, 0x6e, 0x3a);
inline const QColor kSpanMark(0xff, 0xff, 0xff, 0xaa);
inline const QColor kKeyedEdge(0xf2, 0xb8, 0x4b);
inline const QColor kHiddenBar(0x2b, 0x2f, 0x35);
inline const QColor kKey(0xd2, 0xd2, 0xd8);
inline const QColor kKeySelected(0xf2, 0xb8, 0x4b);
inline const QColor kKeyLine(0x45, 0x4a, 0x52);
inline const QColor kLabelMark(0xb0, 0xb5, 0xbe);
inline const QColor kLabelChip(0x2b, 0x2f, 0x35);
inline const QColor kPlayhead(0xff, 0x5d, 0x5d);
inline const QColor kWorkArea(0x4c, 0x9d, 0xff, 0x55);
inline const QColor kWorkAreaTint(0x4c, 0x9d, 0xff, 0x0d);
inline const QColor kNotesLane(0x1a, 0x1c, 0x20);
inline const QColor kNotesText(0x8a, 0x90, 0x9b);
inline const QColor kScriptMark(0x5d, 0x8f, 0xc4);
inline const QColor kCameraMark(0xf2, 0xb8, 0x4b);
inline const QColor kBarText(0xe7, 0xe8, 0xeb);

}
