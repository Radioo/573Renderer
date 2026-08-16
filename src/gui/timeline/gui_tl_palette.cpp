#include "gui_tl_modals.h"

#include "editor/command_palette.h"
#include "editor/options_model.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "gui_tl_forms.h"
#include "gui_tl_internal.h"
#include "imgui.h"
#include "preset/asset_index.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "state/app_state.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <string_view>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr const char* kPaletteTitle = "Add command";
constexpr const char* kTrackTitle = "Add track";

std::string g_track_id;
int g_frame = 0;
bool g_palette_requested = false;
std::array<char, 64> g_filter = {};
int g_highlight = 0;

std::string g_track_selected;
bool g_track_requested = false;
int g_track_kind = (int)Doc::TrackKind::Model;
int g_track_asset = 0;
std::string g_track_target;
std::array<char, 64> g_track_name = {};
bool g_track_below = true;

const char* SectionLabel(Editor::PaletteSection section) {
    switch (section) {
    case Editor::PaletteSection::Track:
        return "This track";
    case Editor::PaletteSection::OtherTracks:
        return "Other tracks (creates a track)";
    case Editor::PaletteSection::Document:
    default:
        return "Document";
    }
}

void Insert(Doc::CommandType type) {
    Editor::State& editor = Editor::Global();
    const std::string track = g_track_id;
    const int frame = g_frame;
    std::string created;
    editor.Apply([track, frame, type, &created](Doc::Document& document) {
        created = Editor::InsertPaletteCommand(document, track, type, frame);
        return !created.empty();
    });
    ImGui::CloseCurrentPopup();
    if (created.empty()) return;
    editor.Select(created);
    RequestClipModal(created, false);
}

void DrawEntry(const Editor::PaletteEntry& entry, bool highlighted) {
    const std::string id =
        "###tl_palette_" + std::string(Doc::kCommandTypeNames[(std::size_t)entry.type]);
    ImGui::BeginDisabled(!entry.enabled);
    const ImU32 chip = CommandColor(entry.type);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(10.0F, 1.0F));
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(origin.x, origin.y + 2.0F),
                                              ImVec2(origin.x + 6.0F, origin.y + 16.0F), chip);
    ImGui::SameLine();
    if (ImGui::Selectable((entry.name + id).c_str(), highlighted)) Insert(entry.type);
    if (highlighted && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
    ImGui::EndDisabled();
    ImGui::SameLine(0.0F, 12.0F);
    if (entry.enabled) {
        ImGui::TextDisabled("%s", entry.summary.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0F, 0.55F, 0.45F, 1.0F), "%s", entry.refusal.c_str());
    }
}

int StepHighlight(const std::vector<Editor::PaletteEntry>& entries, int from, int step) {
    const int count = (int)entries.size();
    if (count == 0) return 0;
    for (int i = 1; i <= count; i++) {
        const int at = (((from + (i * step)) % count) + count) % count;
        if (entries[(std::size_t)at].enabled) return at;
    }
    return from;
}

bool MoveHighlight(const std::vector<Editor::PaletteEntry>& entries) {
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
        g_highlight = StepHighlight(entries, g_highlight, 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
        g_highlight = StepHighlight(entries, g_highlight, -1);
    }
    if (!ImGui::IsKeyPressed(ImGuiKey_Enter, false) || entries.empty()) return false;
    if (!entries[(std::size_t)g_highlight].enabled) return false;
    Insert(entries[(std::size_t)g_highlight].type);
    return true;
}

void DrawEntries(const std::vector<Editor::PaletteEntry>& entries) {
    Editor::PaletteSection section = Editor::PaletteSection::Document;
    bool started = false;
    for (std::size_t i = 0; i < entries.size(); i++) {
        const Editor::PaletteEntry& entry = entries[i];
        if (!started || entry.section != section) {
            section = entry.section;
            started = true;
            ImGui::SeparatorText(SectionLabel(section));
        }
        DrawEntry(entry, std::cmp_equal(i, g_highlight));
    }
    if (entries.empty()) ImGui::TextDisabled("no command matches that filter");
}

void DrawAddOption(const std::vector<Editor::PaletteEntry>& entries, std::string_view filter) {
    const std::string_view name = "Add option";
    if (!filter.empty() && name.find(filter) == std::string_view::npos) return;
    const bool in_section =
        !entries.empty() && entries.back().section == Editor::PaletteSection::Document;
    if (!in_section) ImGui::SeparatorText(SectionLabel(Editor::PaletteSection::Document));
    const bool picked = ImGui::Selectable("Add option###tl_palette_add_option");
    ImGui::SameLine(0.0F, 12.0F);
    ImGui::TextDisabled("appends to document.options; not a clip and not a track");
    if (!picked) return;

    Editor::State& editor = Editor::Global();
    int index = -1;
    editor.Apply([&index](Doc::Document& document) {
        index = Editor::AddOption(document);
        return index >= 0;
    });
    ImGui::CloseCurrentPopup();
    if (index < 0) return;
    editor.PostRequest(
        Editor::Request{.kind = Editor::RequestKind::OptionProperties, .index = index});
}

int ClampHighlight(const std::vector<Editor::PaletteEntry>& entries, int wanted) {
    if (entries.empty()) return 0;
    const int at = std::clamp(wanted, 0, (int)entries.size() - 1);
    if (entries[(std::size_t)at].enabled) return at;
    return StepHighlight(entries, at, 1);
}

void DrawFreeTargetRow() {
    RowLabel("target");
    ImGui::SetNextItemWidth(-30.0F);
    std::array<char, 64> buffer = {};
    std::copy_n(g_track_target.begin(), std::min(g_track_target.size(), buffer.size() - 1),
                buffer.begin());
    if (ImGui::InputText("###tl_track_target", buffer.data(), buffer.size())) {
        g_track_target = buffer.data();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0F, 0.75F, 0.35F, 1.0F), "asset not loaded");
}

std::vector<std::string> AssetIds(const Doc::Document& document) {
    std::vector<std::string> ids;
    ids.reserve(document.assets.size());
    for (const Doc::Asset& asset : document.assets)
        ids.push_back(asset.id);
    return ids;
}

std::vector<std::string> TargetNames(const Preset::AssetIndex& index, const std::string& asset_id,
                                     Doc::TrackKind kind) {
    for (const Preset::AssetEntry& entry : index.assets) {
        if (entry.id != asset_id) continue;
        if (kind == Doc::TrackKind::Model) return entry.models;
        std::vector<std::string> names = entry.cells;
        for (const Preset::AssetAnimation& animation : entry.animations)
            names.push_back(animation.name);
        return names;
    }
    return {};
}

void DrawTargetRow(const std::vector<std::string>& assets, Doc::TrackKind kind) {
    const std::shared_ptr<const Preset::AssetIndex> index = App::Global().GetPresetStatus().assets;
    const std::vector<std::string> names =
        (assets.empty() || index == nullptr)
            ? std::vector<std::string>{}
            : TargetNames(*index, assets[(std::size_t)g_track_asset], kind);
    if (names.empty()) {
        DrawFreeTargetRow();
        return;
    }
    int selected = 0;
    for (std::size_t i = 0; i < names.size(); i++) {
        if (names[i] == g_track_target) selected = (int)i;
    }
    DrawEnumRow("###tl_track_target", "target", selected, names);
    g_track_target = names[(std::size_t)selected];
}

}

void ResetPalette() {
    g_palette_requested = false;
    g_track_requested = false;
    g_track_target.clear();
}

void RequestPalette(std::string track_id, int frame) {
    g_track_id = std::move(track_id);
    g_frame = frame;
    g_filter = {};
    g_highlight = 0;
    g_palette_requested = true;
}

void RenderPalette() {
    const Editor::State& editor = Editor::Global();
    if (g_palette_requested) {
        ImGui::OpenPopup(kPaletteTitle);
        g_palette_requested = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(ImVec2(720, 0), ImVec2(720, viewport->WorkSize.y - 48.0F));
    if (!ImGui::BeginPopupModal(kPaletteTitle, nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNavInputs)) {
        return;
    }
    if (!editor.Loaded()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const Doc::Document& document = editor.Document();
    const int track_index = Editor::FindTrack(document, g_track_id);
    ImGui::TextDisabled("track %s at frame %d",
                        track_index >= 0 ? document.tracks[(std::size_t)track_index].name.c_str()
                                         : "(none, a new one will be created)",
                        g_frame);

    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool typed = ImGui::InputTextWithHint("###tl_palette_filter", "filter commands",
                                                g_filter.data(), g_filter.size());

    const std::vector<Editor::PaletteEntry> entries =
        Editor::PaletteEntries(document, g_track_id, g_frame, g_filter.data());
    if (typed) g_highlight = 0;
    g_highlight = ClampHighlight(entries, g_highlight);
    if (MoveHighlight(entries)) {
        ImGui::EndPopup();
        return;
    }
    DrawEntries(entries);
    DrawAddOption(entries, g_filter.data());

    ImGui::Separator();
    if (ImGui::Button("Cancel###tl_palette_cancel") ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void RequestTrackModal(std::string selected_track_id) {
    g_track_selected = std::move(selected_track_id);
    g_track_requested = true;
    g_track_target.clear();
    g_track_name = {};
}

void RenderTrackModal() {
    Editor::State& editor = Editor::Global();
    if (g_track_requested) {
        ImGui::OpenPopup(kTrackTitle);
        g_track_requested = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 0), ImVec2(520, viewport->WorkSize.y - 48.0F));
    if (!ImGui::BeginPopupModal(kTrackTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!editor.Loaded()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const Doc::Document& document = editor.Document();
    std::vector<std::string> kinds;
    kinds.reserve(Doc::kTrackKindNames.size());
    for (const std::string_view name : Doc::kTrackKindNames)
        kinds.emplace_back(name);
    DrawEnumRow("###tl_track_kind", "kind", g_track_kind, kinds);

    const std::vector<std::string> assets = AssetIds(document);
    if (!assets.empty()) {
        g_track_asset = std::clamp(g_track_asset, 0, (int)assets.size() - 1);
        DrawEnumRow("###tl_track_asset", "asset", g_track_asset, assets);
    }

    const auto kind = (Doc::TrackKind)g_track_kind;
    if (Doc::HasTarget(kind)) DrawTargetRow(assets, kind);

    RowLabel("name");
    ImGui::SetNextItemWidth(-30.0F);
    ImGui::InputTextWithHint("###tl_track_name", g_track_target.c_str(), g_track_name.data(),
                             g_track_name.size());

    RowLabel("insert");
    ImGui::Checkbox("###tl_track_below", &g_track_below);
    ImGui::SameLine();
    ImGui::TextDisabled("below the selected track");

    ImGui::Separator();
    const bool escape =
        !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (ImGui::Button("Cancel###tl_track_cancel") || escape) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button("Add track###tl_track_add")) {
        const Editor::TrackSpec spec{.kind = kind,
                                     .target = g_track_target,
                                     .name = g_track_name.data(),
                                     .below_selected = g_track_below};
        const std::string selected = g_track_selected;
        editor.Apply([spec, selected](Doc::Document& mutable_document) {
            return !Editor::InsertTrack(mutable_document, spec, selected).empty();
        });
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}
