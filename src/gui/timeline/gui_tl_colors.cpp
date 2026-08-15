#include "gui_tl_internal.h"

#include "imgui.h"
#include "preset/doc/preset_enum_names.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

enum class Legend : std::uint8_t {
    Sprite,
    Scroll,
    Emitter,
    Model,
    Tween,
    Motion,
    Camera,
    Light,
    Param,
    Render,
    Rhythm,
    Option,
    Count,
};

constexpr std::array<ImU32, (std::size_t)Legend::Count> kLight = {
    IM_COL32(0x2F, 0x6F, 0xB0, 0xFF), IM_COL32(0x3C, 0x8B, 0x93, 0xFF),
    IM_COL32(0xB0, 0x5A, 0x2A, 0xFF), IM_COL32(0x6A, 0x4D, 0xA8, 0xFF),
    IM_COL32(0x8C, 0x3F, 0x86, 0xFF), IM_COL32(0xA3, 0x46, 0x5C, 0xFF),
    IM_COL32(0x2C, 0x7A, 0x5E, 0xFF), IM_COL32(0x8A, 0x77, 0x22, 0xFF),
    IM_COL32(0x55, 0x5F, 0x6B, 0xFF), IM_COL32(0x40, 0x5A, 0x80, 0xFF),
    IM_COL32(0x9A, 0x5B, 0x1E, 0xFF), IM_COL32(0x6E, 0x6A, 0x2E, 0xFF)};

constexpr std::array<ImU32, (std::size_t)Legend::Count> kDark = {
    IM_COL32(0x64, 0xA6, 0xE8, 0xFF), IM_COL32(0x64, 0xC4, 0xCC, 0xFF),
    IM_COL32(0xF0, 0x92, 0x5A, 0xFF), IM_COL32(0xA8, 0x92, 0xE8, 0xFF),
    IM_COL32(0xDD, 0x82, 0xD2, 0xFF), IM_COL32(0xE8, 0x86, 0x9C, 0xFF),
    IM_COL32(0x5C, 0xC4, 0x9A, 0xFF), IM_COL32(0xD8, 0xC0, 0x54, 0xFF),
    IM_COL32(0xA6, 0xB0, 0xBE, 0xFF), IM_COL32(0x86, 0xA4, 0xD8, 0xFF),
    IM_COL32(0xE8, 0xA4, 0x5C, 0xFF), IM_COL32(0xC6, 0xC0, 0x70, 0xFF)};

constexpr std::array<Legend, 16> kByCommand = {
    Legend::Sprite, Legend::Sprite, Legend::Scroll, Legend::Emitter, Legend::Model, Legend::Tween,
    Legend::Motion, Legend::Camera, Legend::Camera, Legend::Light,   Legend::Param, Legend::Render,
    Legend::Rhythm, Legend::Rhythm, Legend::Rhythm, Legend::Option};

bool DarkTheme() {
    const ImVec4 background = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    return (background.x + background.y + background.z) < 1.5F;
}

ImU32 Pick(Legend slot) {
    const auto index = (std::size_t)slot;
    return DarkTheme() ? kDark[index] : kLight[index];
}

}

ImU32 CommandColor(Doc::CommandType type) {
    return Pick(kByCommand[(std::size_t)type]);
}

ImU32 TrackKindColor(Doc::TrackKind kind) {
    switch (kind) {
    case Doc::TrackKind::Sprite:
        return Pick(Legend::Sprite);
    case Doc::TrackKind::Model:
        return Pick(Legend::Model);
    case Doc::TrackKind::Camera:
        return Pick(Legend::Camera);
    case Doc::TrackKind::Light:
        return Pick(Legend::Light);
    case Doc::TrackKind::Fx:
        return Pick(Legend::Emitter);
    case Doc::TrackKind::Scene:
        break;
    }
    return Pick(Legend::Rhythm);
}

const char* KindBadge(Doc::TrackKind kind) {
    switch (kind) {
    case Doc::TrackKind::Sprite:
        return "2D";
    case Doc::TrackKind::Model:
        return "3D";
    case Doc::TrackKind::Camera:
        return "CAM";
    case Doc::TrackKind::Light:
        return "LIGHT";
    case Doc::TrackKind::Fx:
        return "FX";
    case Doc::TrackKind::Scene:
        break;
    }
    return "SCENE";
}

std::string Ellipsized(const std::string& text, float width) {
    if (width <= 0.0F) return {};
    if (ImGui::CalcTextSize(text.c_str()).x <= width) return text;
    const char* dots = "...";
    const float dots_w = ImGui::CalcTextSize(dots).x;
    if (dots_w > width) return {};
    std::string out = text;
    while (!out.empty() && ImGui::CalcTextSize(out.c_str()).x + dots_w > width)
        out.pop_back();
    out += dots;
    return out;
}

}
