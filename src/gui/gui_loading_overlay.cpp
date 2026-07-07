#include "gui_loading_overlay.h"
#include "../state/boot_lifecycle.h"
#include "../support/log.h"
#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

namespace Panels::LoadingOverlay {

namespace {

void DrawSpinner(ImVec2 center, float radius, float t, ImU32 base_color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int kDots = 8;
    const float kDotRadius = radius * 0.22F;
    for (int i = 0; i < kDots; i++) {
        float const angle =
            ((float)i * (2.0F * std::numbers::pi_v<float> / (float)kDots)) + (t * 4.0F);
        float phase = fmodf((t * 1.6F) - ((float)i * (1.0F / (float)kDots)), 1.0F);
        if (phase < 0) phase += 1.0F;
        float const alpha = 0.20F + (0.80F * (1.0F - phase));
        ImU32 const col =
            (base_color & 0x00FFFFFFU) | ((ImU32)(alpha * 255.0F) << IM_COL32_A_SHIFT);
        ImVec2 const p{center.x + (cosf(angle) * radius), center.y + (sinf(angle) * radius)};
        dl->AddCircleFilled(p, kDotRadius, col, 12);
    }
}

void LogEdge(const App::LoadProgress& progress) {
    static bool prev_active = false;
    if (progress.active && !prev_active) {
        LOG("Gui",
            "Loading overlay ON (target='%s', stage='%s', "
            "expected=%d, loaded=%d, frac=%.2f)",
            progress.target.c_str(), progress.stage.c_str(), progress.textures_expected,
            progress.textures_loaded, progress.fraction);
    } else if (!progress.active && prev_active) {
        LOG("Gui", "Loading overlay OFF (final loaded=%d)", progress.textures_loaded);
    }
    prev_active = progress.active;
}

std::string ShortTarget(const std::string& target) {
    std::string s = target;
    for (auto& c : s)
        if (c == '\\') c = '/';
    size_t const slash = s.rfind('/');
    if (slash != std::string::npos) s = s.substr(slash + 1);
    return s;
}

void DrawProgressBar(ImVec2 origin, float fraction, float t) {
    const float kBarWidth = 280.0F;
    const float kBarHeight = 8.0F;
    ImGui::SetCursorScreenPos(origin);
    if (fraction >= 0.0F) {
        ImGui::ProgressBar(fraction, ImVec2(kBarWidth, kBarHeight), "");
        return;
    }
    ImVec2 const p0 = ImGui::GetCursorScreenPos();
    ImVec2 const p1 = ImVec2(p0.x + kBarWidth, p0.y + kBarHeight);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 34, 255), kBarHeight * 0.5F);
    float const u = fmodf(t * 0.8F, 1.0F);
    float const slab = kBarWidth * 0.25F;
    float const x = p0.x + ((kBarWidth + slab) * u) - slab;
    float const x0 = x < p0.x ? p0.x : x;
    float const x1 = x + slab > p1.x ? p1.x : x + slab;
    if (x1 > x0) {
        dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x1, p1.y), IM_COL32(100, 160, 255, 255),
                          kBarHeight * 0.5F);
    }
    ImGui::Dummy(ImVec2(kBarWidth, kBarHeight));
}

}

namespace {

void DrawProgressDetailLine(const App::LoadProgress& progress, const ImVec2& center) {
    char count[160];
    const char* line = nullptr;
    if (!progress.detail.empty()) {
        line = progress.detail.c_str();
    } else if (progress.textures_loaded > 0 || progress.textures_expected > 0) {
        if (progress.textures_expected > 0) {
            snprintf(count, sizeof(count), "Loaded %d / %d textures", progress.textures_loaded,
                     progress.textures_expected);
        } else {
            snprintf(count, sizeof(count), "Loaded %d texture%s", progress.textures_loaded,
                     progress.textures_loaded == 1 ? "" : "s");
        }
        line = count;
    }
    if (line != nullptr) {
        ImVec2 const cs = ImGui::CalcTextSize(line);
        ImGui::SetCursorScreenPos(ImVec2(center.x - (cs.x * 0.5F), center.y + 92.0F));
        ImGui::TextColored(ImVec4(0.65F, 0.75F, 0.90F, 0.85F), "%s", line);
    }
}

}

void Render(const App::LoadProgress& progress) {
    LogEdge(progress);
    if (!progress.active) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.75F);
    ImGui::Begin("##loading_overlay", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);

    ImVec2 const region = ImGui::GetContentRegionAvail();
    ImVec2 const cursor = ImGui::GetCursorScreenPos();
    ImVec2 const center{cursor.x + (region.x * 0.5F), cursor.y + (region.y * 0.5F)};

    auto t = (float)ImGui::GetTime();
    DrawSpinner(ImVec2(center.x, center.y - 28.0F), 22.0F, t, IM_COL32(255, 200, 100, 255));

    std::string const target = ShortTarget(progress.target);
    char title[160];
    if (!target.empty()) {
        snprintf(title, sizeof(title), "Loading %s", target.c_str());
    } else {
        snprintf(title, sizeof(title), "Loading");
    }
    ImVec2 const ts = ImGui::CalcTextSize(title);
    ImGui::SetCursorScreenPos(ImVec2(center.x - (ts.x * 0.5F), center.y + 18.0F));
    ImGui::TextColored(ImVec4(1.0F, 1.0F, 1.0F, 1.0F), "%s", title);

    if (!progress.stage.empty()) {
        ImVec2 const ss = ImGui::CalcTextSize(progress.stage.c_str());
        ImGui::SetCursorScreenPos(ImVec2(center.x - (ss.x * 0.5F), center.y + 44.0F));
        ImGui::TextColored(ImVec4(0.80F, 0.85F, 0.95F, 0.85F), "%s", progress.stage.c_str());
    }

    const float kBarWidth = 280.0F;
    DrawProgressBar(ImVec2(center.x - (kBarWidth * 0.5F), center.y + 72.0F), progress.fraction, t);

    DrawProgressDetailLine(progress, center);

    ImGui::End();
}

}
