#pragma once

namespace Gui {

constexpr float kPaneLeftMin = 240.0f;
constexpr float kPaneCenterMin = 320.0f;
constexpr float kPaneRightMin = 280.0f;

constexpr float kPaneLeftDefault = 300.0f;
constexpr float kPaneRightDefault = 340.0f;

constexpr float kSplitterW = 6.0f;

constexpr float kTopBarH = 46.0f;
constexpr float kTimelineH = 76.0f;
constexpr float kPaneRowMinH = 120.0f;
constexpr float kStatusStripH = 28.0f;

constexpr int kMinClientW =
    (int)(kPaneLeftMin + kPaneCenterMin + kPaneRightMin + 2.0f * kSplitterW) + 80;
constexpr int kMinClientH = 600;

}
