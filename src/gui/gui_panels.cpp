#include "gui_panels.h"
#include "gui_panels_internal.h"
#include "gui_export_panel.h"
#include "gui_icons.h"
#include "gui_layout_constants.h"
#include "gui_loading_overlay.h"
#include "gui_preset_library.h"
#include "gui_setup_view.h"
#include "gui_splitter.h"
#include "gui_style.h"
#include "gui_widgets.h"
#include "panel_registry.h"
#include "timeline/gui_timeline_editor.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_view.h"
#include "../game_profile.h"
#include "../native_dialog.h"
#include "../state/app_state.h"
#include "../state/commands.h"
#include "../support/log.h"
#include "imgui.h"
#include "state/boot_lifecycle.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Panels {

namespace {
std::string PrettifyPath(const std::string& in, size_t max_len = 72) {
    std::string s = in;
    for (auto& c : s)
        if (c == '\\') c = '/';
    if (s.size() <= max_len) return s;
    size_t const tail = max_len - 3;
    return "..." + s.substr(s.size() - tail);
}
}

struct IfsTreeNode {
    std::string segment;
    const App::State::IfsEntry* entry = nullptr;
    std::vector<IfsTreeNode> children;
    int file_count = 0;
};

namespace {
IfsTreeNode* FindOrAddChild(IfsTreeNode* parent, const std::string& segment, bool is_file) {
    for (auto& c : parent->children) {
        if (c.segment == segment && (c.entry != nullptr) == is_file) return &c;
    }
    parent->children.emplace_back();
    parent->children.back().segment = segment;
    return &parent->children.back();
}
}

namespace {
void SortTree(IfsTreeNode* n) {
    for (auto& c : n->children)
        SortTree(&c);
    std::ranges::sort(n->children, [](const IfsTreeNode& a, const IfsTreeNode& b) {
        const bool a_is_dir = a.entry == nullptr;
        const bool b_is_dir = b.entry == nullptr;
        if (a_is_dir != b_is_dir) return a_is_dir;
        return a.segment < b.segment;
    });
}
}

namespace {
int ComputeFileCounts(IfsTreeNode* n) {
    if (n->entry != nullptr) {
        n->file_count = 1;
        return 1;
    }
    int total = 0;
    for (auto& c : n->children)
        total += ComputeFileCounts(&c);
    n->file_count = total;
    return total;
}
}

namespace {
IfsTreeNode BuildIfsTree(const std::vector<App::State::IfsEntry>& list) {
    IfsTreeNode root;
    for (const auto& e : list) {
        std::string n = e.name;
        for (auto& c : n)
            if (c == '\\') c = '/';

        IfsTreeNode* cur = &root;
        size_t start = 0;
        while (start < n.size()) {
            size_t const slash = n.find('/', start);
            bool const last = slash == std::string::npos;
            std::string const seg = last ? n.substr(start) : n.substr(start, slash - start);
            if (!seg.empty()) {
                cur = FindOrAddChild(cur, seg, last);
                if (last) cur->entry = &e;
            }
            if (last) break;
            start = slash + 1;
        }
    }
    SortTree(&root);
    ComputeFileCounts(&root);
    return root;
}
}

namespace {
bool SubtreeMatchesFilter(const IfsTreeNode& n, const std::string& lower_filter) {
    if (lower_filter.empty()) return true;
    if (n.entry != nullptr) {
        std::string nm = n.entry->name;
        for (auto& c : nm)
            c = (char)tolower((unsigned char)c);
        return nm.find(lower_filter) != std::string::npos;
    }
    return std::ranges::any_of(
        n.children, [&](const auto& c) { return SubtreeMatchesFilter(c, lower_filter); });
}
}

namespace {
void RenderIfsTreeNode(App::State& state, const IfsTreeNode& node, const std::string& active_path,
                       const std::string& lower_filter, bool tree_small) {
    if (!SubtreeMatchesFilter(node, lower_filter)) return;

    if (node.entry != nullptr) {
        bool const is_active = (node.entry->full_path == active_path);
        ImGui::PushID(node.entry->full_path.c_str());
        if (ImGui::Selectable(node.segment.c_str(), is_active,
                              ImGuiSelectableFlags_SpanAllColumns)) {
            if (!is_active) {
                state.PostCommand(App::Cmd::LoadContent{.path = node.entry->full_path,
                                                        .from_arc = node.entry->from_arc});
                LOG("Gui", "Loading IFS from tree selection: '%s'", node.entry->full_path.c_str());
            }
        }
        ImGui::PopID();
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!lower_filter.empty() || tree_small) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    ImGui::PushID(node.segment.c_str());
    bool const open =
        ImGui::TreeNodeEx("##dir", flags, "%s   (%d)", node.segment.c_str(), node.file_count);
    if (open) {
        for (const auto& c : node.children) {
            RenderIfsTreeNode(state, c, active_path, lower_filter, tree_small);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}
}

void RenderIfsPicker() {
    auto& state = App::Global();
    auto list = state.ListAvailableIfs();
    std::string const active_path = state.GetStatus().current_ifs_path;

    char suffix[32];
    snprintf(suffix, sizeof(suffix), "(%zu)", list.size());
    Gui::SectionHeader(ICON_FOLDER, "Browse", suffix);

    if (state.IsIfsScanning()) {
        std::string const s = state.GetIfsScanStatus();
        ImGui::TextColored(ImVec4(0.65F, 0.75F, 0.90F, 0.9F), "%s",
                           s.empty() ? "Scanning for IFS files..." : s.c_str());
        return;
    }
    if (list.empty()) {
        ImGui::TextDisabled("No .ifs files found under the game dir.");
        return;
    }

    static char filter_buf[128] = {};
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##ifsfilter", "filter by path...", filter_buf, sizeof(filter_buf));
    std::string filter = filter_buf;
    for (auto& c : filter)
        c = (char)tolower((unsigned char)c);

    ImGui::Spacing();

    ImGui::BeginChild("ifs_scroll", ImVec2(0, 0), 0, ImGuiWindowFlags_HorizontalScrollbar);

    IfsTreeNode const tree = BuildIfsTree(list);
    const bool tree_small = list.size() <= 20;
    for (const auto& c : tree.children) {
        RenderIfsTreeNode(state, c, active_path, filter, tree_small);
    }
    ImGui::EndChild();
}

namespace {

int g_main_view = 0;

void ClampPaneWidths(float avail_w, float sw, float& left_w, float& right_w, float& center_w) {
    left_w = std::max(left_w, Gui::kPaneLeftMin);
    right_w = std::max(Gui::kPaneRightMin, right_w);
    center_w = avail_w - left_w - right_w - (2.0F * sw);
    if (center_w < Gui::kPaneCenterMin) {
        float deficit = Gui::kPaneCenterMin - center_w;
        const float room_r = right_w - Gui::kPaneRightMin;
        const float take_r = room_r < deficit ? room_r : deficit;
        if (take_r > 0.0F) {
            right_w -= take_r;
            deficit -= take_r;
        }
        if (deficit > 0.0F) {
            const float room_l = left_w - Gui::kPaneLeftMin;
            const float take_l = room_l < deficit ? room_l : deficit;
            if (take_l > 0.0F) left_w -= take_l;
        }
        center_w = avail_w - left_w - right_w - (2.0F * sw);
        center_w = std::max(center_w, 1.0F);
    }
}

void RenderTopBar(const App::Status& status) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 8));
    ImGui::BeginChild("topbar", ImVec2(0, Gui::kTopBarH), 1, ImGuiWindowFlags_NoScrollbar);

    ImGui::AlignTextToFramePadding();
    Gui::PushHeaderFont();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "573Renderer");
    ImGui::PopFont();

    static std::vector<const Gui::PanelDesc*> tabs;
    Gui::CollectActivePanels(Gui::PanelSlot::MainTab, tabs);
    if (tabs.size() > 1) {
        ImGui::SameLine(0.0F, 18.0F);
        for (size_t i = 0; i < tabs.size(); i++) {
            if (i > 0) ImGui::SameLine(0.0F, 2.0F);
            const bool active = std::cmp_equal(i, g_main_view);
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                ImGui::GetStyleColorVec4(active ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg));
            if (ImGui::Button(tabs[i]->tab_label)) g_main_view = (int)i;
            ImGui::PopStyleColor();
        }
    }
    if (std::cmp_greater_equal(g_main_view, tabs.size())) g_main_view = 0;

    ImGui::SameLine(0.0F, 18.0F);
    Gui::PushMonoFont();
    if (status.current_ifs_path.empty()) {
        ImGui::TextDisabled("no IFS loaded");
    } else {
        ImGui::TextDisabled("%s", PrettifyPath(status.current_ifs_path).c_str());
    }
    ImGui::PopFont();

    char fps_buf[24];
    snprintf(fps_buf, sizeof(fps_buf), "%.1f fps", status.fps_measured);
    float const fps_w = ImGui::CalcTextSize(fps_buf).x;
    float const export_w = ImGui::CalcTextSize(ICON_EXPORT "  Export...").x + 22.0F;
    ImGui::SameLine(ImGui::GetWindowWidth() - fps_w - export_w - 44.0F);
    ImGui::BeginDisabled(!status.scene_loaded);
    if (ImGui::Button(ICON_EXPORT "  Export...")) {
        Export::RequestOpen();
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Export the playing animation to video / image (Ctrl+E).");
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - fps_w - 18.0F);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", fps_buf);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DrawExportStatusTag(App::State& state) {
    App::ExportState const ex = state.GetExport();
    const char* label = nullptr;
    ImVec4 color{};
    char buf[64];
    switch (ex.phase) {
    case App::ExportPhase::Capturing:
        snprintf(buf, sizeof(buf), "capturing %d%s", ex.frames_captured,
                 ex.using_hardware ? " [NVENC]" : "");
        label = buf;
        color = ImVec4(1.0F, 0.85F, 0.3F, 1.0F);
        break;
    case App::ExportPhase::Encoding:
        snprintf(buf, sizeof(buf), "encoding %d frames...", ex.frames_captured);
        label = buf;
        color = ImVec4(1.0F, 0.85F, 0.3F, 1.0F);
        break;
    case App::ExportPhase::Done:
        label = "export done";
        color = ImVec4(0.50F, 0.92F, 0.65F, 1.0F);
        break;
    case App::ExportPhase::Failed:
        label = "export failed";
        color = ImVec4(1.0F, 0.45F, 0.45F, 1.0F);
        break;
    case App::ExportPhase::Idle:
        return;
    }

    ImGui::SameLine(0.0F, 14.0F);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    if (ImGui::SmallButton(label)) Export::RequestOpen();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        if (ex.phase == App::ExportPhase::Done) {
            ImGui::SetTooltip("%s\nClick to open the export dialog.", ex.output_path.c_str());
        } else if (ex.phase == App::ExportPhase::Failed) {
            ImGui::SetTooltip("%s\nClick to open the export dialog.", ex.error.c_str());
        } else {
            ImGui::SetTooltip("Click to open the export dialog.");
        }
    }

    if (ex.phase != App::ExportPhase::Done || ex.output_path.empty()) return;
    ImGui::SameLine(0.0F, 6.0F);
    if (ImGui::SmallButton("Open folder")) NativeDialog::RevealInFileManager(ex.output_path);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show %s in Explorer.", ex.output_path.c_str());
    }
}

void RenderViewportPane() {
    static std::vector<const Gui::PanelDesc*> center;
    Gui::CollectActivePanels(Gui::PanelSlot::CenterPane, center);
    if (center.empty()) {
        RenderScenePane();
    } else {
        center.front()->draw();
    }
}

float ClampEditorHeight(float available) {
    Editor::View& view = Editor::Global().MutView();
    const float room =
        available - Gui::kPaneRowMinH - Gui::kSplitterW - (2.0F * ImGui::GetStyle().ItemSpacing.y);
    view.height =
        std::clamp(view.height, Editor::kEditorHeightMin, std::max(Editor::kEditorHeightMin, room));
    return view.height;
}

void RenderEditorDock(float width, float row_h) {
    Editor::View& view = Editor::Global().MutView();
    float row = row_h;
    Gui::HSplitter("##split_timeline", width, Gui::kSplitterW, &row, &view.height,
                   Gui::kPaneRowMinH, Editor::kEditorHeightMin, Editor::kEditorHeightDefault);
    Timeline::Render(view.height);
}

void RenderStatusStripImpl(const App::Status& status) {
    auto& state = App::Global();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 4));
    ImGui::BeginChild("status_strip", ImVec2(0, Gui::kStatusStripH), 0,
                      ImGuiWindowFlags_NoScrollbar);

    Gui::PushMonoFont();

    const GameProfile::Profile* profile = GameProfile::BySlug(state.GetGameProfileSlug());
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", profile != nullptr ? profile->name : "no profile");

    int rw = 0;
    int rh = 0;
    state.GetRenderSize(rw, rh);
    ImGui::SameLine(0.0F, 14.0F);
    ImGui::TextDisabled("%dx%d", rw, rh);

    ImGui::SameLine(0.0F, 14.0F);
    if (status.last_error.empty()) {
        ImGui::TextColored(ImVec4(0.50F, 0.92F, 0.65F, 1.0F), "render ok");
    } else {
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.45F, 1.0F), "%s",
                           PrettifyPath(status.last_error, 60).c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", status.last_error.c_str());
    }

    DrawExportStatusTag(state);

    auto live = state.GetLiveState();
    if (live.have_file_info) {
        char ver[32];
        snprintf(ver, sizeof(ver), "afp %u.%u.%u", (live.afp_ver >> 16) & 0xFFFF,
                 (live.afp_ver >> 8) & 0xFF, live.afp_ver & 0xFF);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(ver).x - 18.0F);
        ImGui::TextDisabled("%s", ver);
    }

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

}

void RenderRendererView() {
    static float left_w = Gui::kPaneLeftDefault;
    static float right_w = Gui::kPaneRightDefault;

    const bool editor = Timeline::Active();
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float sw = Gui::kSplitterW;
    const float editor_h = editor ? ClampEditorHeight(avail_h) : 0.0F;
    const float row_h = editor ? (avail_h - editor_h - sw - (2.0F * spacing))
                               : (avail_h - Gui::kTimelineH - spacing);

    float center_w = 0.0F;
    ClampPaneWidths(avail_w, sw, left_w, right_w, center_w);

    ImGui::BeginChild("pane_left", ImVec2(left_w, row_h), 0);
    RenderIfsPicker();
    ImGui::EndChild();

    ImGui::SameLine(0.0F, 0.0F);
    {
        float c = center_w;
        Gui::VSplitter("##split_l", sw, row_h, &left_w, &c, Gui::kPaneLeftMin, Gui::kPaneCenterMin);
    }
    ImGui::SameLine(0.0F, 0.0F);

    ImGui::BeginChild("pane_center", ImVec2(center_w, row_h), 0);
    RenderViewportPane();
    ImGui::EndChild();

    ImGui::SameLine(0.0F, 0.0F);
    {
        float c = center_w;
        Gui::VSplitter("##split_r", sw, row_h, &c, &right_w, Gui::kPaneCenterMin,
                       Gui::kPaneRightMin);
    }
    ImGui::SameLine(0.0F, 0.0F);

    ImGui::BeginChild("pane_right", ImVec2(right_w, row_h), 0);
    RenderInspectorPane();
    ImGui::EndChild();

    if (editor) {
        RenderEditorDock(avail_w, row_h);
        return;
    }
    RenderTimelineDock();
}

namespace {
void RenderReadyView() {
    auto status = App::Global().GetStatus();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    RenderTopBar(status);
    ImGui::Spacing();

    static std::vector<const Gui::PanelDesc*> tabs;
    Gui::CollectActivePanels(Gui::PanelSlot::MainTab, tabs);
    if (!tabs.empty()) {
        int const view = std::clamp(g_main_view, 0, (int)tabs.size() - 1);
        const float content_h =
            ImGui::GetContentRegionAvail().y - Gui::kStatusStripH - ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("main_view", ImVec2(0, content_h), 0);
        tabs[(size_t)view]->draw();
        ImGui::EndChild();
    }

    RenderStatusStripImpl(status);
    Export::RenderModal();
    Timeline::RenderModals();
    PresetLibrary::RenderModals();

    ImGui::End();
    ImGui::PopStyleVar();
}
}

void Build() {
    Gui::ApplyAccentForProfile(App::Global().GetGameProfileSlug());

    App::BootState const bs = App::Global().GetBootState();
    auto progress = App::Global().GetLoadProgress();

    if (bs == App::BootState::Ready) {
        RenderReadyView();
    } else {
        Panels::Setup::RenderView();
    }

    Panels::LoadingOverlay::Render(progress);
}

}
