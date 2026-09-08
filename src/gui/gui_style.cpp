#include "gui_style.h"
#include "gui_dpi.h"
#include "../support/log.h"
#include "imgui.h"

#include <windows.h>

#include <initializer_list>
#include <optional>
#include <string>

namespace Gui {

namespace {

Fonts g_fonts;
std::optional<std::string> g_applied_accent_slug;

constexpr ImVec4 kGround{0.063F, 0.067F, 0.078F, 1.00F};
constexpr ImVec4 kRaised{0.086F, 0.094F, 0.114F, 1.00F};
constexpr ImVec4 kOverlay{0.075F, 0.082F, 0.098F, 0.98F};
constexpr ImVec4 kSunken{0.051F, 0.055F, 0.067F, 1.00F};
constexpr ImVec4 kLine{0.149F, 0.165F, 0.196F, 1.00F};
constexpr ImVec4 kText{0.902F, 0.910F, 0.929F, 1.00F};
constexpr ImVec4 kTextDim{0.490F, 0.518F, 0.580F, 1.00F};

ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return {a.x + ((b.x - a.x) * t), a.y + ((b.y - a.y) * t), a.z + ((b.z - a.z) * t),
            a.w + ((b.w - a.w) * t)};
}

struct ProfileAccent {
    const char* slug_prefix = nullptr;
    ImVec4 color;
};

constexpr ImVec4 kAccentDefault{0.369F, 0.561F, 0.847F, 1.00F};

constexpr ProfileAccent kProfileAccents[] = {
    {.slug_prefix = "sdvx", .color = ImVec4{0.141F, 0.776F, 0.878F, 1.00F}},
    {.slug_prefix = "iidx", .color = kAccentDefault},
    {.slug_prefix = "ddr", .color = ImVec4{0.878F, 0.698F, 0.247F, 1.00F}},
    {.slug_prefix = "gitadora", .color = ImVec4{0.878F, 0.384F, 0.333F, 1.00F}},
    {.slug_prefix = "jubeat", .color = ImVec4{0.725F, 0.761F, 0.808F, 1.00F}},
};

ImVec4 AccentForSlug(const std::string& slug) {
    for (const auto& pa : kProfileAccents) {
        if (slug.starts_with(pa.slug_prefix)) return pa.color;
    }
    return kAccentDefault;
}

void ApplyAccentColors(const ImVec4& accent) {
    ImVec4* c = ImGui::GetStyle().Colors;
    c[ImGuiCol_CheckMark] = Mix(accent, kText, 0.25F);
    c[ImGuiCol_SliderGrab] = Mix(accent, kGround, 0.15F);
    c[ImGuiCol_SliderGrabActive] = Mix(accent, kText, 0.25F);
    c[ImGuiCol_Button] = Mix(kRaised, accent, 0.28F);
    c[ImGuiCol_ButtonHovered] = Mix(kRaised, accent, 0.45F);
    c[ImGuiCol_ButtonActive] = Mix(kRaised, accent, 0.20F);
    c[ImGuiCol_Header] = Mix(kRaised, accent, 0.20F);
    c[ImGuiCol_HeaderHovered] = Mix(kRaised, accent, 0.32F);
    c[ImGuiCol_HeaderActive] = Mix(kRaised, accent, 0.26F);
    c[ImGuiCol_Tab] = kOverlay;
    c[ImGuiCol_TabHovered] = Mix(kRaised, accent, 0.32F);
    c[ImGuiCol_TabActive] = Mix(kRaised, accent, 0.22F);
    c[ImGuiCol_SeparatorHovered] = Mix(kLine, accent, 0.40F);
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = Mix(kLine, accent, 0.20F);
    c[ImGuiCol_ResizeGripHovered] = Mix(kLine, accent, 0.50F);
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_TextSelectedBg] = Mix(kGround, accent, 0.35F);
    c[ImGuiCol_PlotHistogram] = Mix(accent, kGround, 0.15F);
    c[ImGuiCol_PlotHistogramHovered] = accent;
    c[ImGuiCol_DragDropTarget] = accent;
    c[ImGuiCol_NavHighlight] = accent;
}

bool AddIconFont(ImGuiIO& io, const std::string& winroot) {
    static const ImWchar kIconRanges[] = {0xE700, 0xE950, 0};
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.GlyphOffset = ImVec2(0.0F, 2.0F);
    cfg.GlyphMinAdvanceX = 17.0F;
    const char* candidates[] = {"\\Fonts\\SegoeIcons.ttf", "\\Fonts\\segmdl2.ttf"};
    for (const char* rel : candidates) {
        std::string const path = winroot + rel;
        if (io.Fonts->AddFontFromFileTTF(path.c_str(), 15.0F, &cfg, kIconRanges) != nullptr) {
            LOG("Gui", "Icon font merged: %s", path.c_str());
            return true;
        }
    }
    LOG("Gui", "No Segoe icon font found; icon glyphs will render as fallback boxes");
    return false;
}

ImFont* AddFirstAvailable(ImGuiIO& io, const std::string& winroot,
                          std::initializer_list<const char*> rel_paths, float size_px,
                          const ImWchar* ranges) {
    for (const char* rel : rel_paths) {
        std::string const path = winroot + rel;
        ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), size_px, nullptr, ranges);
        if (f != nullptr) {
            LOG("Gui", "Font loaded: %s @ %.0fpx", path.c_str(), size_px);
            return f;
        }
    }
    return nullptr;
}

}

void PushHeaderFont() {
    ImGui::PushFont(g_fonts.header, g_fonts.header != nullptr ? g_fonts.header->LegacySize : 0.0F);
}

void PushMonoFont() {
    ImGui::PushFont(g_fonts.mono, g_fonts.mono != nullptr ? g_fonts.mono->LegacySize : 0.0F);
}

void LoadFonts() {
    static const ImWchar kTextRanges[] = {
        0x0020, 0x00FF, 0x2010, 0x2027, 0x2030, 0x205E, 0,
    };

    ImGuiIO& io = ImGui::GetIO();

    char winroot_buf[MAX_PATH] = {};
    UINT const n = GetWindowsDirectoryA(winroot_buf, sizeof(winroot_buf));
    if (n == 0 || n >= sizeof(winroot_buf)) {
        LOG("Gui", "GetWindowsDirectory failed; using default ImGui font");
        g_fonts.base = io.Fonts->AddFontDefault();
        g_fonts.header = g_fonts.base;
        g_fonts.mono = g_fonts.base;
        return;
    }
    std::string const winroot(winroot_buf);

    g_fonts.base = AddFirstAvailable(io, winroot, {"\\Fonts\\segoeui.ttf", "\\Fonts\\consola.ttf"},
                                     16.0F, kTextRanges);
    if (g_fonts.base == nullptr) {
        LOG("Gui", "Could not load Segoe UI or Consolas; using ImGui default");
        g_fonts.base = io.Fonts->AddFontDefault();
        g_fonts.header = g_fonts.base;
        g_fonts.mono = g_fonts.base;
        return;
    }
    AddIconFont(io, winroot);

    g_fonts.header = AddFirstAvailable(
        io, winroot, {"\\Fonts\\seguisb.ttf", "\\Fonts\\segoeuib.ttf"}, 18.0F, kTextRanges);
    if (g_fonts.header == nullptr) g_fonts.header = g_fonts.base;

    g_fonts.mono = AddFirstAvailable(io, winroot, {"\\Fonts\\consola.ttf"}, 13.0F, kTextRanges);
    if (g_fonts.mono == nullptr) g_fonts.mono = g_fonts.base;
}

void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowRounding = 0.0F;
    s.FrameRounding = 0.0F;
    s.GrabRounding = 0.0F;
    s.ScrollbarRounding = 0.0F;
    s.TabRounding = 0.0F;
    s.PopupRounding = 0.0F;
    s.ChildRounding = 0.0F;
    s.WindowBorderSize = 0.0F;
    s.FrameBorderSize = 0.0F;
    s.PopupBorderSize = 1.0F;
    s.ChildBorderSize = 1.0F;
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.FramePadding = ImVec2(9, 5);
    s.WindowPadding = ImVec2(12, 10);
    s.ScrollbarSize = 12.0F;
    s.GrabMinSize = 10.0F;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = kGround;
    c[ImGuiCol_ChildBg] = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);
    c[ImGuiCol_PopupBg] = kOverlay;
    c[ImGuiCol_Border] = kLine;
    c[ImGuiCol_FrameBg] = kRaised;
    c[ImGuiCol_FrameBgHovered] = Mix(kRaised, kText, 0.06F);
    c[ImGuiCol_FrameBgActive] = Mix(kRaised, kText, 0.10F);
    c[ImGuiCol_TitleBg] = kSunken;
    c[ImGuiCol_TitleBgActive] = kSunken;
    c[ImGuiCol_TitleBgCollapsed] = kSunken;
    c[ImGuiCol_MenuBarBg] = kSunken;
    c[ImGuiCol_ScrollbarBg] = kSunken;
    c[ImGuiCol_ScrollbarGrab] = kLine;
    c[ImGuiCol_ScrollbarGrabHovered] = Mix(kLine, kText, 0.15F);
    c[ImGuiCol_ScrollbarGrabActive] = Mix(kLine, kText, 0.25F);
    c[ImGuiCol_Separator] = kLine;
    c[ImGuiCol_TableHeaderBg] = kOverlay;
    c[ImGuiCol_TableBorderStrong] = kLine;
    c[ImGuiCol_TableBorderLight] = Mix(kLine, kGround, 0.40F);
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kTextDim;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02F, 0.02F, 0.03F, 0.72F);

    g_applied_accent_slug.reset();
    ApplyAccentColors(kAccentDefault);

    s.ScaleAllSizes(Dpi::Scale());
    s.FontScaleDpi = Dpi::Scale();
}

void ApplyAccentForProfile(const std::string& slug) {
    if (g_applied_accent_slug.has_value() && *g_applied_accent_slug == slug) return;
    g_applied_accent_slug = slug;
    ApplyAccentColors(AccentForSlug(slug));
}

}
