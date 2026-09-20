#pragma once

#include <QColor>

#include <array>
#include <cstdint>

namespace Editor {

inline constexpr int kGutterWidth = 64;
inline constexpr int kRulerHeight = 26;
inline constexpr int kNotesHeight = 14;
inline constexpr int kRowsTop = kRulerHeight + kNotesHeight;
inline constexpr int kRowHeight = 16;
inline constexpr int kNoteRadius = 4;
inline constexpr int kNoteReach = 6;
inline constexpr int kBarInset = 2;
inline constexpr int kShortestNamedBar = 24;
inline constexpr int kEyeLeft = 3;
inline constexpr int kLockLeft = 17;
inline constexpr int kSwitchWidth = 12;
inline constexpr int kSoloLeft = 31;
inline constexpr int kNumberLeft = 45;
inline constexpr int kNamePadding = 3;
inline constexpr int kNamePixels = 10;
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

inline const QColor kRuler(58, 58, 62);
inline const QColor kRow(40, 40, 44);
inline const QColor kSpanName(240, 240, 244);
inline const QColor kSwitchOn(222, 222, 228);
inline const QColor kSwitchOff(96, 96, 104);
inline const QColor kPropertyRow(34, 34, 38);
inline const QColor kSelectedRow(44, 66, 88);
inline const QColor kBar(70, 128, 196);
inline const QColor kImageBar(56, 104, 158);
inline const QColor kShapeBar(46, 122, 104);
inline const QColor kSpriteBar(104, 84, 158);
inline const QColor kTextBar(140, 110, 58);
inline const QColor kSpanMark(255, 255, 255, 150);
inline const QColor kKeyedEdge(242, 184, 75);
inline const QColor kHiddenBar(92, 92, 98);
inline const QColor kKey(210, 210, 216);
inline const QColor kKeySelected(240, 190, 80);
inline const QColor kKeyLine(96, 96, 104);
inline const QColor kLabelMark(220, 180, 90);
inline const QColor kPlayhead(230, 90, 90);
inline const QColor kWorkArea(72, 96, 132);
inline const QColor kNotesLane(46, 46, 52);
inline const QColor kScriptMark(150, 200, 240);
inline const QColor kCameraMark(240, 190, 80);

}
