#include "state/boot_lifecycle.h"
#include <cfloat>
#include <utility>
#include "gui_setup_view.h"
#include "gui_window.h"
#include "../state/app_state.h"
#include "../arc_extract.h"
#include "../customize_extract.h"
#include "../game_profile.h"
#include "../native_dialog.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace Panels::Setup {

namespace {

char g_dir_buf[1024] = {};
std::string g_last_state_value;
bool g_initial_sync_done = false;

void PersistSetup(App::State& state, const char* dir_value) {
    if ((dir_value != nullptr) && state.GameDir() != dir_value) {
        state.SetGameDir(dir_value);
    }
    App::SaveCurrentSettings();
}

void SyncDirBufFromState(const std::string& cur_dir) {
    if (g_initial_sync_done && cur_dir == g_last_state_value) return;
    size_t n = cur_dir.size();
    n = std::min(n, sizeof(g_dir_buf) - 1);
    std::memcpy(g_dir_buf, cur_dir.c_str(), n);
    g_dir_buf[n] = '\0';
    g_initial_sync_done = true;
}

void DrawHeaderCard() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14F, 0.14F, 0.16F, 1.0F));
    ImGui::BeginChild("setup_card", ImVec2(0, 62), 1, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(ImVec4(0.75F, 0.85F, 1.0F, 1.0F), "573Renderer");
    ImGui::TextDisabled("Select a Konami game installation directory to begin.");
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void DrawErrorBanner(App::BootState bs, const std::string& err) {
    if (bs != App::BootState::Failed || err.empty()) return;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35F, 0.18F, 0.20F, 1.0F));
    ImGui::BeginChild("setup_err", ImVec2(0, 56), 1);
    ImGui::TextColored(ImVec4(1.00F, 0.85F, 0.60F, 1.0F), "Last attempt failed:");
    ImGui::TextWrapped("%s", err.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void DrawGameDirInput(App::State& state) {
    ImGui::Text("Game directory:");
    ImGui::SetNextItemWidth(-120.0F);
    if (ImGui::InputText("##gamedir", g_dir_buf, sizeof(g_dir_buf))) {
        PersistSetup(state, g_dir_buf);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(-FLT_MIN, 0))) {
        std::string const picked = NativeDialog::BrowseForFolder(Gui::GetHwnd(), g_dir_buf);
        if (!picked.empty()) {
            size_t n = picked.size();
            n = std::min(n, sizeof(g_dir_buf) - 1);
            std::memcpy(g_dir_buf, picked.c_str(), n);
            g_dir_buf[n] = '\0';
            PersistSetup(state, g_dir_buf);
        }
    }
    ImGui::Spacing();
}

void DrawGameProfilePicker(App::State& state) {
    const auto& profiles = GameProfile::All();
    std::string const current_slug = state.GetGameProfileSlug();

    const GameProfile::Profile* auto_pick = GameProfile::AutoDetect(g_dir_buf);
    char auto_label[160];
    if (auto_pick != nullptr) {
        snprintf(auto_label, sizeof(auto_label), "Auto (matches: %s)", auto_pick->name);
    } else {
        snprintf(auto_label, sizeof(auto_label), "%s",
                 "Auto (no match - select a profile; boot requires one)");
    }

    int sel_idx = 0;
    if (!current_slug.empty()) {
        for (size_t i = 0; i < profiles.size(); i++) {
            if (current_slug == profiles[i].slug) {
                sel_idx = (int)(i + 1);
                break;
            }
        }
    }

    ImGui::Text("Game profile:");
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* preview = sel_idx == 0 ? auto_label : profiles[sel_idx - 1].name;
    if (ImGui::BeginCombo("##game_profile", preview)) {
        if (ImGui::Selectable(auto_label, sel_idx == 0)) {
            state.SetGameProfileSlug("");
            PersistSetup(state, g_dir_buf);
        }
        for (size_t i = 0; i < profiles.size(); i++) {
            bool const selected = (std::cmp_equal(sel_idx, (i + 1)));
            if (ImGui::Selectable(profiles[i].name, selected)) {
                state.SetGameProfileSlug(profiles[i].slug);
                PersistSetup(state, g_dir_buf);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Selects which game-specific DLL ordinals + boot-call\n"
                          "gating to use. Konami ships per-game builds of\n"
                          "afp-core.dll that share the export-name scheme but\n"
                          "diverge in which ordinal points to which function.\n"
                          "Auto-detect picks based on the game directory path;\n"
                          "override here if the detection is incorrect.");
    }
    ImGui::Spacing();
}

void DrawRenderFps(App::State& state) {
    int fps = state.GetRenderFps();

    ImGui::Text("Frame rate:");
    ImGui::SetNextItemWidth(120.0F);
    bool changed = ImGui::InputInt("##render_fps", &fps, 5, 30, ImGuiInputTextFlags_AutoSelectAll);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Frames per second the preview renders + advances the\n"
                          "animation at. Animation SPEED is unchanged (dt = 1/fps),\n"
                          "so this is purely smoothness vs CPU. 60 matches DDR's\n"
                          "authored rate; 120 is the historical default. Video export\n"
                          "uses its own fps in the Export panel, independent of this.");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("fps");

    ImGui::SameLine();
    const int kQuick[] = {30, 60, 120, 144};
    for (int const q : kQuick) {
        char b[24];
        snprintf(b, sizeof(b), "%d##fps_%d", q, q);
        if (ImGui::SmallButton(b)) {
            fps = q;
            changed = true;
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();

    if (changed) {
        fps = std::clamp(fps, 1, 1000);
        state.SetRenderFps(fps);
        PersistSetup(state, g_dir_buf);
    }
    ImGui::Spacing();
}

struct RenderPreset {
    const char* label;
    int w;
    int h;
};

void DrawRenderPresetCombo(App::State& state, int rw, int rh) {
    static const RenderPreset kPresets[] = {
        {.label = "3840x2160 (4K)", .w = 3840, .h = 2160},
        {.label = "1920x1080", .w = 1920, .h = 1080},
        {.label = "1280x720", .w = 1280, .h = 720},
        {.label = "1080x1920", .w = 1080, .h = 1920},
        {.label = "720x1280", .w = 720, .h = 1280},
        {.label = "520x704 (qpro avatar)", .w = 520, .h = 704},
        {.label = "Custom", .w = 0, .h = 0},
    };
    const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
    const int kCustomIdx = kPresetCount - 1;

    static int shown_idx = -1;
    static int last_rw = -1;
    static int last_rh = -1;

    int auto_idx = kCustomIdx;
    for (int i = 0; i < kPresetCount; i++) {
        if (kPresets[i].w == rw && kPresets[i].h == rh) {
            auto_idx = i;
            break;
        }
    }
    if (shown_idx < 0 || rw != last_rw || rh != last_rh) {
        shown_idx = auto_idx;
    }
    last_rw = rw;
    last_rh = rh;

    ImGui::Text("Render resolution:");
    ImGui::SetNextItemWidth(-120.0F);
    if (ImGui::BeginCombo("##render_preset", kPresets[shown_idx].label)) {
        for (int i = 0; i < kPresetCount; i++) {
            bool const selected = (i == shown_idx);
            if (ImGui::Selectable(kPresets[i].label, selected)) {
                shown_idx = i;
                if (kPresets[i].w > 0) {
                    state.SetRenderSize(kPresets[i].w, kPresets[i].h);
                    last_rw = kPresets[i].w;
                    last_rh = kPresets[i].h;
                    PersistSetup(state, g_dir_buf);
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Native screen size of the game whose IFSes you're\n"
                          "loading. Select a preset or enter a custom WxH below.");
    }
}

void DrawRenderResolution(App::State& state) {
    int rw = 0;
    int rh = 0;
    state.GetRenderSize(rw, rh);

    DrawRenderPresetCombo(state, rw, rh);

    int w_val = rw;
    int h_val = rh;
    const float input_w = 100.0F;
    ImGui::SetNextItemWidth(input_w);
    bool const w_changed =
        ImGui::InputInt("##render_w", &w_val, 0, 0, ImGuiInputTextFlags_AutoSelectAll);
    ImGui::SameLine();
    ImGui::TextUnformatted("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(input_w);
    bool const h_changed =
        ImGui::InputInt("##render_h", &h_val, 0, 0, ImGuiInputTextFlags_AutoSelectAll);
    ImGui::SameLine();
    ImGui::TextDisabled("(width x height, pixels)");
    if (w_changed || h_changed) {
        w_val = std::clamp(w_val, 64, 8192);
        h_val = std::clamp(h_val, 64, 8192);
        state.SetRenderSize(w_val, h_val);
        PersistSetup(state, g_dir_buf);
    }
    ImGui::Spacing();
}

void DrawArcExtractor() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Tools");
    ImGui::TextWrapped("Extract .arc archives: recursively unpacks every .arc under a folder "
                       "into a new \"<folder>_extracted\" folder next to it.");

    ArcExtract::Status const st = ArcExtract::GetStatus();

    ImGui::BeginDisabled(st.running);
    if (ImGui::Button("Extract .arc files...", ImVec2(200, 0))) {
        std::string const picked = NativeDialog::BrowseForFolder(Gui::GetHwnd(), g_dir_buf);
        if (!picked.empty()) ArcExtract::Start(picked);
    }
    ImGui::EndDisabled();

    if (st.running) {
        if (st.total_arcs == 0) {
            char ov[48];
            snprintf(ov, sizeof(ov), "Scanning... %d found", st.done_arcs);
            ImGui::ProgressBar(-1.0F * (float)ImGui::GetTime(), ImVec2(-FLT_MIN, 0), ov);
            if (!st.current.empty()) ImGui::TextDisabled("%s", st.current.c_str());
        } else {
            float const frac = (float)st.done_arcs / (float)st.total_arcs;
            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%d / %d", st.done_arcs, st.total_arcs);
            ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0), overlay);
            ImGui::TextDisabled("Extracting: %s", st.current.c_str());
        }
    } else if (st.finished) {
        if (!st.error.empty()) {
            ImGui::TextColored(ImVec4(1.0F, 0.6F, 0.5F, 1.0F), "Failed: %s", st.error.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6F, 0.9F, 0.6F, 1.0F), "Done: %d files from %d archives%s",
                               st.entries_written, st.done_arcs - st.failed_arcs,
                               (st.failed_arcs != 0) ? "" : ".");
            if (st.failed_arcs != 0) {
                ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.5F, 1.0F),
                                   " (%d file(s) skipped - not valid .arc)", st.failed_arcs);
            }
            ImGui::TextWrapped("Output: %s", st.output_dir.c_str());
        }
    }
    ImGui::Spacing();
}

void DrawCustomizeExtractor() {
    ImGui::Spacing();
    ImGui::TextWrapped("Extract customize images: select a folder of raw game data (.arc "
                       "files); unpacks appeal boards, characters and lane art into a "
                       "\"customize_assets\" folder, renamed by id and losslessly optimised.");

    CustomizeExtract::Status const st = CustomizeExtract::GetStatus();

    ImGui::BeginDisabled(st.running);
    if (ImGui::Button("Extract customize images...", ImVec2(220, 0))) {
        std::string const picked = NativeDialog::BrowseForFolder(Gui::GetHwnd(), g_dir_buf);
        if (!picked.empty()) CustomizeExtract::Start(picked);
    }
    ImGui::EndDisabled();

    if (st.running) {
        if (st.total == 0) {
            char ov[48];
            snprintf(ov, sizeof(ov), "Scanning... %d found", st.done);
            ImGui::ProgressBar(-1.0F * (float)ImGui::GetTime(), ImVec2(-FLT_MIN, 0), ov);
            if (!st.current.empty()) ImGui::TextDisabled("%s", st.current.c_str());
        } else {
            float const frac = (float)st.done / (float)st.total;
            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%d / %d", st.done, st.total);
            ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0), overlay);
            ImGui::TextDisabled("Processing: %s", st.current.c_str());
        }
    } else if (st.finished) {
        if (!st.error.empty()) {
            ImGui::TextColored(ImVec4(1.0F, 0.6F, 0.5F, 1.0F), "Failed: %s", st.error.c_str());
        } else if (st.total == 0) {
            ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.5F, 1.0F),
                               "No customize .arc files found in the selected folder.");
        } else {
            double const saved_kb = (double)(st.bytes_in - st.bytes_out) / 1024.0;
            ImGui::TextColored(ImVec4(0.6F, 0.9F, 0.6F, 1.0F),
                               "Done: %d images (%d optimized), saved %.0f KB", st.written,
                               st.optimized, saved_kb);
            if (st.failed != 0)
                ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.5F, 1.0F), " (%d failed)", st.failed);
            ImGui::TextWrapped("Output: %s", st.output_dir.c_str());
        }
    }
    ImGui::Spacing();
}

void DrawLoadButton(App::State& state, App::BootState bs) {
    const bool boot_in_flight = (bs == App::BootState::Booting);
    ImGui::BeginDisabled(boot_in_flight || g_dir_buf[0] == '\0');
    if (ImGui::Button("Load", ImVec2(160, 32))) {
        App::Request r;
        r.set_game_dir = true;
        r.game_dir = g_dir_buf;
        state.GetRenderSize(r.render_width, r.render_height);
        state.PostRequest(std::move(r));
        state.SetBootError({});
    }
    ImGui::EndDisabled();
    if (boot_in_flight) {
        ImGui::SameLine();
        ImGui::TextDisabled("Booting...");
    }
}

}

void RenderView() {
    auto& state = App::Global();
    App::BootState const bs = state.GetBootState();
    std::string const cur_dir = state.GameDir();
    std::string const err = state.GetBootError();

    SyncDirBufFromState(cur_dir);
    g_last_state_value = cur_dir;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##setup", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    DrawHeaderCard();
    DrawErrorBanner(bs, err);
    DrawGameDirInput(state);
    DrawGameProfilePicker(state);
    DrawRenderFps(state);
    DrawRenderResolution(state);
    DrawLoadButton(state, bs);
    DrawArcExtractor();
    DrawCustomizeExtractor();

    ImGui::End();
}

}
