#pragma once

#include <QColor>
#include <QString>

class QApplication;

namespace Editor::Theme {

inline const QColor kPage(0x0c, 0x0d, 0x0f);
inline const QColor kPanel(0x14, 0x15, 0x18);
inline const QColor kField(0x1a, 0x1c, 0x20);
inline const QColor kLine(0x28, 0x2b, 0x31);
inline const QColor kEdge(0x35, 0x39, 0x41);
inline const QColor kText(0xe7, 0xe8, 0xeb);
inline const QColor kSoft(0xb0, 0xb5, 0xbe);
inline const QColor kFaint(0x8a, 0x90, 0x9b);
inline const QColor kAccent(0x4c, 0x9d, 0xff);
inline const QColor kOnAccent(0x06, 0x12, 0x1f);
inline const QColor kChosen(0x1b, 0x33, 0x50);
inline const QColor kAmber(0xf2, 0xb8, 0x4b);
inline const QColor kGreen(0x3e, 0xcf, 0x8e);
inline const QColor kOnChosen(0x9c, 0xc8, 0xff);

inline constexpr int kBaseSize = 12;
inline constexpr int kBarHeight = 44;
inline constexpr int kStatusHeight = 24;
inline constexpr int kControlHeight = 28;
inline constexpr int kBarControl = 28;
inline constexpr int kSearchWidth = 380;
inline constexpr int kToolStripWidth = 40;
inline constexpr int kToolButtonSide = 32;
inline constexpr int kToolIconSide = 17;
inline constexpr int kToolRuleWidth = 24;
inline constexpr int kStageBarHeight = 36;
inline constexpr int kStageIconSide = 16;
inline constexpr int kStageButtonSide = 28;
inline constexpr int kPanelStripHeight = 30;

[[nodiscard]] QString SansFamily();

[[nodiscard]] QString MonoFamily();

void Apply(QApplication& app);

[[nodiscard]] QString DockStyle();

}
