#pragma once

namespace Gui {

constexpr float kPaneLeftMin = 240.0f;
constexpr float kPaneCenterMin = 320.0f;
constexpr float kPaneRightMin = 300.0f;

constexpr float kPaneLeftDefault = 320.0f;
constexpr float kPaneRightDefault = 380.0f;

constexpr float kSplitterW = 6.0f;

constexpr int kMinClientW =
    (int)(kPaneLeftMin + kPaneCenterMin + kPaneRightMin + 2.0f * kSplitterW) + 80;
constexpr int kMinClientH = 600;

}
