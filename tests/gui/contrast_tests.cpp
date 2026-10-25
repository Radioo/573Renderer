#include "gui_style.h"
#include "imgui.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double kLeastContrast = 4.5;
constexpr double kLowChannel = 0.03928;
constexpr double kFlatSlope = 12.92;
constexpr double kCurveShift = 0.055;
constexpr double kCurveScale = 1.055;
constexpr double kCurvePower = 2.4;
constexpr double kRedShare = 0.2126;
constexpr double kGreenShare = 0.7152;
constexpr double kBlueShare = 0.0722;
constexpr double kContrastOffset = 0.05;

struct Surface {
    const char* name;
    ImGuiCol_ which;
};

constexpr std::array<Surface, 14> kSurfaces{
    {{.name = "WindowBg", .which = ImGuiCol_WindowBg},
     {.name = "PopupBg", .which = ImGuiCol_PopupBg},
     {.name = "FrameBg", .which = ImGuiCol_FrameBg},
     {.name = "FrameBgHovered", .which = ImGuiCol_FrameBgHovered},
     {.name = "FrameBgActive", .which = ImGuiCol_FrameBgActive},
     {.name = "TitleBg", .which = ImGuiCol_TitleBg},
     {.name = "MenuBarBg", .which = ImGuiCol_MenuBarBg},
     {.name = "TableHeaderBg", .which = ImGuiCol_TableHeaderBg},
     {.name = "Button", .which = ImGuiCol_Button},
     {.name = "ButtonHovered", .which = ImGuiCol_ButtonHovered},
     {.name = "Header", .which = ImGuiCol_Header},
     {.name = "HeaderHovered", .which = ImGuiCol_HeaderHovered},
     {.name = "Tab", .which = ImGuiCol_Tab},
     {.name = "TabActive", .which = ImGuiCol_TabActive}}};

double Channel(float value) {
    const auto part = static_cast<double>(value);
    if (part <= kLowChannel) return part / kFlatSlope;
    return std::pow((part + kCurveShift) / kCurveScale, kCurvePower);
}

double Luminance(const ImVec4& colour) {
    return (kRedShare * Channel(colour.x)) + (kGreenShare * Channel(colour.y)) +
           (kBlueShare * Channel(colour.z));
}

ImVec4 Over(const ImVec4& top, const ImVec4& under) {
    const float alpha = top.w;
    return {(top.x * alpha) + (under.x * (1.0F - alpha)),
            (top.y * alpha) + (under.y * (1.0F - alpha)),
            (top.z * alpha) + (under.z * (1.0F - alpha)), 1.0F};
}

double Contrast(const ImVec4& text, const ImVec4& behind) {
    const double lit = Luminance(text);
    const double under = Luminance(behind);
    const double brighter = std::max(lit, under);
    const double darker = std::min(lit, under);
    return (brighter + kContrastOffset) / (darker + kContrastOffset);
}

struct Session {
    Session() { ImGui::CreateContext(); }
    ~Session() { ImGui::DestroyContext(); }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;
};

std::vector<std::string> FaintPairs(const std::string& slug) {
    Gui::ApplyStyle();
    Gui::ApplyAccentForProfile(slug);
    const ImGuiStyle& style = ImGui::GetStyle();
    const std::span<const ImVec4, ImGuiCol_COUNT> colours(style.Colors);
    std::vector<std::string> faint;
    for (const Surface& surface : kSurfaces) {
        const ImVec4 behind = Over(colours[static_cast<std::size_t>(surface.which)],
                                   colours[static_cast<std::size_t>(ImGuiCol_WindowBg)]);
        const double ratio = Contrast(colours[static_cast<std::size_t>(ImGuiCol_Text)], behind);
        if (ratio >= kLeastContrast) continue;
        faint.push_back(slug + " text on " + surface.name + " is " + std::to_string(ratio) + ":1");
    }
    return faint;
}

}

TEST_CASE("Every profile's accent keeps the renderer's words readable on what they sit on") {
    const Session ui;
    std::vector<std::string> faint;
    for (const std::string& slug : {"iidx33", "sdvx7", "ddrworld", "gitadora", "jubeat", "popn9"}) {
        for (std::string& said : FaintPairs(slug))
            faint.push_back(std::move(said));
    }
    CHECK(faint.empty());
    for (const std::string& said : faint)
        FAIL_CHECK(said);
}
