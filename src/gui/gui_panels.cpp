#include "gui_panels.h"
#include "gui_panels_internal.h"
#include "gui_export_panel.h"
#include "gui_loading_overlay.h"
#include "gui_setup_view.h"
#include "../state/app_state.h"
#include "../support/log.h"
#include "imgui.h"
#include "state/telemetry.h"
#include "state/ifs_catalog.h"
#include "state/boot_lifecycle.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cfloat>
#include <cstring>
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

namespace {
void RenderTopBar(const App::Status& status) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13F, 0.14F, 0.17F, 1.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 10));
    ImGui::BeginChild("topbar", ImVec2(0, 56), 1, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.75F, 0.85F, 1.0F, 1.0F), "573Renderer");
    ImGui::SameLine(0, 24);
    if (status.current_ifs_path.empty()) {
        ImGui::TextDisabled("no IFS loaded");
    } else {
        ImGui::TextDisabled("%s", PrettifyPath(status.current_ifs_path).c_str());
    }

    char fps_buf[24];
    snprintf(fps_buf, sizeof(fps_buf), "%.1f fps", status.fps_measured);
    float const fps_w = ImGui::CalcTextSize(fps_buf).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - fps_w - 28);
    ImGui::TextColored(ImVec4(0.55F, 0.75F, 0.95F, 1.0F), "%s", fps_buf);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
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
                App::Request r;
                r.load_new_ifs = true;
                r.ifs_path = node.entry->full_path;
                r.ifs_from_arc = node.entry->from_arc;
                state.PostRequest(std::move(r));
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

    ImGui::TextColored(ImVec4(0.92F, 0.92F, 0.93F, 1.00F), "IFS Files");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", list.size());
    ImGui::Separator();
    ImGui::Spacing();

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
    ImGui::InputTextWithHint("##ifsfilter", "Filter by path...", filter_buf, sizeof(filter_buf));
    std::string filter = filter_buf;
    for (auto& c : filter)
        c = (char)tolower((unsigned char)c);

    ImGui::Spacing();

    ImGui::BeginChild("ifs_scroll", ImVec2(0, 0), 0, ImGuiWindowFlags_HorizontalScrollbar);

    IfsTreeNode const tree = BuildIfsTree(list);
    const bool tree_small = list.size() <= 20;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0F);
    for (const auto& c : tree.children) {
        RenderIfsTreeNode(state, c, active_path, filter, tree_small);
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

namespace {
void DrawVariantBitmapCombo(App::IfsConfig& cfg, App::VariantSlot& slot) {
    const bool is_default = (!slot.bitmap.empty() && slot.bitmap == slot.default_bitmap);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* preview = slot.bitmap.empty() || is_default ? "(default)" : slot.bitmap.c_str();
    if (!ImGui::BeginCombo("##bitmap", preview)) return;
    if (ImGui::Selectable("(default)", slot.bitmap.empty() && !slot.bitmap_override)) {
        slot.bitmap.clear();
        slot.bitmap_override = false;
        App::Request r;
        r.force_replay = true;
        App::Global().PostRequest(std::move(r));
    }
    for (auto& b : cfg.bitmap_names) {
        bool const selected = (!is_default && slot.bitmap == b);
        if (ImGui::Selectable(b.c_str(), selected)) {
            slot.bitmap = b;
            slot.bitmap_override = true;
        }
        if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

void DrawVariantBitmapInput(App::VariantSlot& slot) {
    char buf[128] = {};
    size_t n = slot.bitmap.size();
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, slot.bitmap.data(), n);
    buf[n] = 0;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##bitmap", "bitmap name (blank = IFS default)", buf,
                                 sizeof(buf))) {
        slot.bitmap = buf;
        slot.bitmap_override = (buf[0] != '\0');
    }
}

void RenderVariantSlot(App::IfsConfig& cfg, App::VariantSlot& slot) {
    ImGui::PushID(slot.path.c_str());

    ImVec4 const dot_color =
        slot.is_valid ? ImVec4(0.50F, 0.85F, 0.50F, 1.0F) : ImVec4(0.70F, 0.70F, 0.70F, 0.5F);
    ImGui::TextColored(dot_color, "%s", slot.is_valid ? "*" : "o");
    ImGui::SameLine();
    ImGui::Text("%s", slot.path.c_str());
    if (!slot.is_valid) {
        ImGui::SameLine();
        ImGui::TextDisabled("(unresolved)");
    }

    ImGui::Indent(18.0F);
    ImGui::Checkbox("Visible", &slot.visible);

    if (!cfg.bitmap_names.empty()) {
        DrawVariantBitmapCombo(cfg, slot);
    } else {
        DrawVariantBitmapInput(slot);
    }
    ImGui::Unindent(18.0F);
    ImGui::Spacing();

    ImGui::PopID();
}
}

void RenderVariantEditor() {
    auto& state = App::Global();
    auto active = state.ActiveIfs();

    ImGui::TextColored(ImVec4(0.92F, 0.92F, 0.93F, 1.00F), "Variants");
    if (active.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(no IFS loaded)");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Select an IFS on the left to configure its clip "
                            "variants.");
        return;
    }

    auto& cfg = state.MutConfig(active);
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu slot%s)", cfg.slots.size(), cfg.slots.size() == 1 ? "" : "s");
    ImGui::Separator();
    ImGui::Spacing();

    if (cfg.slots.empty()) {
        ImGui::TextDisabled("No variant slots discovered yet.");
        ImGui::Spacing();
        ImGui::TextWrapped("Slots are probed from afplist.xml / texturelist.xml plus a "
                           "list of common clip names. If nothing resolves, this IFS may "
                           "not expose any named variant clips, or probing has not run yet "
                           "(it completes once the animation reaches a frame that places "
                           "them).");
        return;
    }

    ImGui::BeginChild("variants_scroll", ImVec2(0, -64), 0);
    for (auto& slot : cfg.slots)
        RenderVariantSlot(cfg, slot);
    ImGui::EndChild();
}

void RenderAddSlotForm() {
    auto& state = App::Global();
    auto active = state.ActiveIfs();
    if (active.empty()) return;

    ImGui::Separator();
    ImGui::TextDisabled("Add slot by clip path:");
    auto& cfg = state.MutConfig(active);
    static char buf[128] = {};
    ImGui::SetNextItemWidth(-120.0F);
    ImGui::InputTextWithHint("##new_slot", "e.g. coin", buf, sizeof(buf));
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(-FLT_MIN, 0))) {
        if (buf[0] != 0) {
            bool exists = false;
            for (auto& s : cfg.slots) {
                if (s.path == buf) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                App::VariantSlot s;
                s.path = buf;
                s.default_bitmap = buf;
                s.visible = true;
                s.is_valid = false;
                cfg.slots.push_back(std::move(s));
            }
            buf[0] = 0;
        }
    }
}

namespace {

void DrawLoopMasterControls() {
    bool loop = App::Global().GetLoopMaster();
    if (ImGui::Checkbox("Loop master animation", &loop)) {
        App::Global().SetLoopMaster(loop);
        App::SaveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When the master animation reaches the end of\n"
                          "its timeline, re-play it from frame 0. Useful\n"
                          "for short title clips that otherwise freeze\n"
                          "on their last authored frame.");
    }

    auto mode = App::Global().GetRootLoopMode();
    int idx = (mode == App::State::RootLoopMode::Force) ? 1 : 0;
    const char* kItems[2] = {"Auto-hold (default)", "Force loop"};
    ImGui::TextUnformatted("Loop root:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::Combo("##root_loop", &idx, kItems, 2)) {
        App::Global().SetRootLoopMode(idx == 1 ? App::State::RootLoopMode::Force
                                               : App::State::RootLoopMode::Hold);
        App::SaveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How a scene-BG root that reaches its end is driven.\n"
                          "Auto-hold (game default): mount once, let the root\n"
                          "  play once and HOLD while nested children keep running -\n"
                          "  e.g. select_bg_o_dimension_6th's o_kazari5 ornament\n"
                          "  rotates its full timeline instead of snapping back every\n"
                          "  140-frame root cycle.\n"
                          "Force loop: re-drive the root to frame 0 each cycle\n"
                          "  (ForceReplay + the continuous-loop flag sequence). Needed\n"
                          "  for one-shot masters (bg_common) that only loop this way,\n"
                          "  or to deliberately force-loop the whole clip.\n\n"
                          "Applies to the live preview now and to the next non-label\n"
                          "export. Persisted across restarts.");
    }
}

void DrawMasterScaleControls() {
    float scale = App::Global().GetMasterScale();
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::SliderFloat("Master scale", &scale, 0.25F, 4.0F, "%.2fx")) {
        App::Global().SetMasterScale(scale);
        App::SaveCurrentSettings();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1.0x##scale_reset")) {
        App::Global().SetMasterScale(1.0F);
        App::SaveCurrentSettings();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1.5x##scale_sdvx_old")) {
        App::Global().SetMasterScale(1.5F);
        App::SaveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Multiplies the master stream's transform matrix by the\n"
                          "given factor. Default 1.0x.\n\n"
                          "1.5x preview matches what SDVX 7's BG dispatcher applies to\n"
                          "SDVX-I-through-IV-era 720x1280 select_bg variants on a\n"
                          "1080x1920 game.");
    }
}

void DrawLayerList(App::State& state, const App::IfsConfig& cfg) {
    std::string const playing = state.GetStatus().playing_animation;

    ImGui::BeginChild("layer_scroll", ImVec2(0, 200.0F), 1);
    for (const auto& name : cfg.anim_names) {
        bool const is_playing = (name == playing);
        if (is_playing) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50F, 0.92F, 0.65F, 1.0F));
        }
        if (ImGui::Selectable(name.c_str(), is_playing, ImGuiSelectableFlags_SpanAllColumns)) {
            App::Request r;
            r.switch_animation = true;
            r.animation_name = name;
            state.PostRequest(std::move(r));
        }
        if (is_playing) ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

}

void RenderLayersPanel() {
    auto& state = App::Global();
    auto active = state.ActiveIfs();
    if (active.empty()) {
        ImGui::TextDisabled("Select an IFS to list its layers.");
        return;
    }

    auto& cfg = state.MutConfig(active);
    if (cfg.anim_names.empty()) {
        ImGui::TextDisabled("No layers listed in afplist.xml.");
        return;
    }

    DrawLoopMasterControls();
    DrawMasterScaleControls();

    ImGui::Spacing();
    ImGui::TextDisabled("(click to play / replay)");
    DrawLayerList(state, cfg);
}

namespace {

void DrawIfsSummaryCard(const App::IfsConfig& cfg) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14F, 0.15F, 0.18F, 1.0F));
    ImGui::BeginChild("ifs_summary", ImVec2(0, 78), 1, ImGuiWindowFlags_NoScrollbar);
    if (ImGui::BeginTable("summary_tbl", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Layers");
        ImGui::TextColored(ImVec4(0.80F, 0.90F, 1.0F, 1.0F), "%zu", cfg.anim_names.size());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("Bitmaps");
        ImGui::TextColored(ImVec4(0.80F, 0.90F, 1.0F, 1.0F), "%zu", cfg.bitmap_names.size());

        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Variant slots");
        ImGui::TextColored(ImVec4(0.80F, 0.90F, 1.0F, 1.0F), "%zu", cfg.slots.size());
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

const char* CompanionLocaleName(const std::string& sfx) {
    if (sfx == "_j") return "Japanese";
    if (sfx == "_a") return "Asian";
    if (sfx == "_k") return "Korean";
    return "Other";
}

void DrawCompanionRow(App::State& state, const App::CompanionIfs& c, size_t i, int loaded_idx) {
    const char* action_hint = nullptr;
    if (c.loaded) {
        action_hint = "   (active - click to clear)";
    } else if (loaded_idx >= 0) {
        action_hint = "   (click to switch)";
    } else {
        action_hint = "   (click to load)";
    }

    char label[256];
    snprintf(label, sizeof(label), "[%s]  %s%s", CompanionLocaleName(c.suffix),
             c.display_name.c_str(), action_hint);

    ImVec4 const text_col =
        c.loaded ? ImVec4(0.80F, 0.95F, 0.85F, 1.0F) : ImVec4(0.72F, 0.74F, 0.78F, 0.95F);
    ImGui::PushStyleColor(ImGuiCol_Text, text_col);
    ImGui::PushID((int)i);
    if (ImGui::Selectable(label, c.loaded)) {
        App::Request r;
        r.toggle_companion = true;
        r.companion_index = (int)i;
        state.PostRequest(std::move(r));
    }
    ImGui::PopID();
    ImGui::PopStyleColor();
}

void DrawLocaleOverlayCard(App::State& state, const App::IfsConfig& cfg) {
    int loaded_idx = -1;
    for (size_t i = 0; i < cfg.companions.size(); i++) {
        if (cfg.companions[i].loaded) {
            loaded_idx = (int)i;
            break;
        }
    }

    ImGui::Text("Locale overlay");
    ImGui::SameLine();
    if (loaded_idx >= 0) {
        ImGui::TextDisabled("(%s active - click again to clear)",
                            cfg.companions[loaded_idx].display_name.c_str());
    } else {
        ImGui::TextDisabled("(%zu available - click one to overlay)", cfg.companions.size());
    }
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14F, 0.15F, 0.18F, 1.0F));
    float const row_h = ImGui::GetTextLineHeightWithSpacing() + 2.0F;
    auto rows = (float)cfg.companions.size();
    float const box_h = ((rows < 6.0F ? rows : 6.0F) * row_h) + 12.0F;
    ImGui::BeginChild("companions_card", ImVec2(0, box_h), 1, 0);
    for (size_t i = 0; i < cfg.companions.size(); i++)
        DrawCompanionRow(state, cfg.companions[i], i, loaded_idx);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

}

void RenderIfsInfoPane() {
    auto& state = App::Global();
    auto active = state.ActiveIfs();

    ImGui::TextColored(ImVec4(0.92F, 0.92F, 0.93F, 1.00F), "IFS Info");
    if (active.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(no selection)");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("Select an IFS file from the tree on the left "
                           "to see its atlases, bitmaps, and available "
                           "animations.");
        return;
    }

    auto& cfg = state.MutConfig(active);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", active.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    DrawIfsSummaryCard(cfg);
    ImGui::Spacing();

    if (!cfg.companions.empty()) DrawLocaleOverlayCard(state, cfg);

    ImGui::Spacing();
    Panels::Export::RenderPanel();
}

namespace {
void RenderReadyView() {
    auto status = App::Global().GetStatus();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    RenderTopBar(status);
    ImGui::Spacing();

    ImGuiTabBarFlags const tab_flags =
        ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
    if (ImGui::BeginTabBar("##main_tabs", tab_flags)) {
        if (ImGui::BeginTabItem("Renderer")) {
            RenderRendererTabBody();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("qpro")) {
            RenderQproTabBody();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
}

void Build() {
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
