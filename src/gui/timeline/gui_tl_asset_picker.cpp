#include "gui_tl_asset_picker.h"

#include "gui_tl_forms.h"
#include "gui/gui_dpi.h"
#include "imgui.h"
#include "native_dialog.h"
#include "preset/asset_index.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "preset/preset_layer_verdicts.h"
#include "state/app_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr std::size_t kInlineParts = 8;

std::string RelativeToGameDir(const std::string& picked) {
    std::string root = App::Global().GameDir();
    std::string dir = picked;
    for (char& c : root)
        if (c == '\\') c = '/';
    for (char& c : dir)
        if (c == '\\') c = '/';
    if (!root.empty() && dir.starts_with(root)) {
        dir.erase(0, root.size());
        while (!dir.empty() && dir.front() == '/')
            dir.erase(dir.begin());
    }
    return dir;
}

std::string AssetIdFromDir(const Doc::Document& document, const std::string& dir) {
    const std::size_t slash = dir.find_last_of('/');
    std::string base = (slash == std::string::npos) ? dir : dir.substr(slash + 1);
    if (base.empty()) base = "asset";
    std::string candidate = base;
    for (int counter = 2; counter < 1000; counter++) {
        const bool taken =
            std::ranges::any_of(document.assets, [&candidate](const Doc::Asset& asset) {
                return asset.id == candidate;
            });
        if (!taken) break;
        candidate = base + "_" + std::to_string(counter);
    }
    return candidate;
}

const Preset::AssetAnimation* FindAnimation(const Preset::AssetEntry* entry,
                                            const std::string& name) {
    if (entry == nullptr) return nullptr;
    for (const Preset::AssetAnimation& animation : entry->animations) {
        if (animation.name == name) return &animation;
    }
    return nullptr;
}

}

bool HasAssetField(Doc::CommandType type) {
    return Doc::FindField(Doc::FieldsFor(type), "asset") != nullptr;
}

std::string CommandAsset(const Doc::Command& command) {
    const Doc::FieldDesc* field = Doc::FindField(Doc::FieldsFor(Doc::TypeOf(command)), "asset");
    if (field == nullptr) return {};
    const Doc::ParamValue value = field->get(command);
    const auto* text = std::get_if<std::string>(&value);
    return (text != nullptr) ? *text : std::string{};
}

FormContext MakeFormContext(const Doc::Document& document, const Doc::Command& command,
                            const std::string& target) {
    FormContext context;
    context.assets = App::Global().GetPresetStatus().assets;
    context.asset_id = CommandAsset(command);
    context.target = target;
    for (const Doc::Asset& asset : document.assets)
        context.asset_ids.push_back(asset.id);
    return context;
}

FieldEvent DrawHiddenParts(const FormContext& context, Doc::Command& command) {
    auto* animate = std::get_if<Doc::SpriteAnimate>(&command);
    if (animate == nullptr) return FieldEvent::None;
    const Preset::AssetEntry* entry = FindAsset(context, context.asset_id);
    const Preset::AssetAnimation* animation = FindAnimation(entry, animate->animation);
    if (animation == nullptr || animation->parts.empty()) return FieldEvent::None;

    FieldEvent event = FieldEvent::None;
    const std::size_t count = animation->parts.size();
    ImGui::SetNextItemOpen(count <= kInlineParts, ImGuiCond_Once);
    const std::string header = "Hidden parts (" + std::to_string(count) + ")###tl_hidden_header";
    if (!ImGui::CollapsingHeader(header.c_str())) return event;
    for (const std::string& part : animation->parts) {
        bool hidden = std::ranges::find(animate->hidden_parts, part) != animate->hidden_parts.end();
        if (ImGui::Checkbox(("###tl_hidden_" + part).c_str(), &hidden)) {
            if (hidden) {
                animate->hidden_parts.push_back(part);
            } else {
                std::erase(animate->hidden_parts, part);
            }
            event = FieldEvent::Committed;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(part.c_str());
        ImGui::SameLine(0.0F, Gui::Dpi::S(10.0F));
        const Preset::LayerVerdict verdict =
            Preset::VerdictFor(entry != nullptr ? entry->dir : std::string{}, part);
        const std::string_view name = Preset::VerdictName(verdict);
        const ImVec4 color = (verdict == Preset::LayerVerdict::Chrome)
                                 ? ImVec4(1.0F, 0.55F, 0.45F, 1.0F)
                                 : ImVec4(0.62F, 0.72F, 0.62F, 1.0F);
        ImGui::TextColored(color, "%s", std::string(name).c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("docs/preset_layers.md classifies this layer as %s.",
                              std::string(name).c_str());
        }
    }
    return event;
}

FieldEvent DrawAssetTab(const FormContext& context, Doc::Document& document,
                        Doc::Command& command) {
    const Doc::FieldDesc* field = Doc::FindField(Doc::FieldsFor(Doc::TypeOf(command)), "asset");
    if (field == nullptr) return FieldEvent::None;

    FieldEvent event = DrawField(context, *field, command);
    const std::string asset_id = CommandAsset(command);
    const Preset::AssetEntry* entry = FindAsset(context, asset_id);

    const Doc::Asset* declared = nullptr;
    for (const Doc::Asset& asset : document.assets) {
        if (asset.id == asset_id) declared = &asset;
    }

    RowLabel("resolved dir");
    std::array<char, 256> dir_buffer = {};
    const std::string dir = declared != nullptr ? declared->dir : std::string("(not declared)");
    std::copy_n(dir.begin(), std::min(dir.size(), dir_buffer.size() - 1), dir_buffer.begin());
    ImGui::SetNextItemWidth(Gui::Dpi::S(260.0F));
    ImGui::InputText("###tl_asset_dir", dir_buffer.data(), dir_buffer.size(),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine(0.0F, Gui::Dpi::S(10.0F));
    if (entry != nullptr && entry->loaded) {
        ImGui::TextColored(ImVec4(0.50F, 0.92F, 0.65F, 1.0F), "loaded");
    } else {
        ImGui::TextColored(ImVec4(1.0F, 0.55F, 0.45F, 1.0F), "missing");
    }

    RowLabel("kind");
    ImGui::TextDisabled("%s",
                        declared != nullptr
                            ? std::string(Doc::kAssetKindNames[(std::size_t)declared->kind]).c_str()
                            : "-");

    for (const char* name : {"model", "cell", "animation"}) {
        const Doc::FieldDesc* named = Doc::FindField(Doc::FieldsFor(Doc::TypeOf(command)), name);
        if (named == nullptr) continue;
        FormContext refreshed = context;
        refreshed.asset_id = asset_id;
        event = std::max(event, DrawField(refreshed, *named, command));
    }

    if (!ImGui::Button("add asset...###tl_asset_add")) return event;
    const std::string picked = NativeDialog::BrowseForFolder(nullptr, App::Global().GameDir());
    if (picked.empty()) return event;
    const std::string picked_dir = RelativeToGameDir(picked);
    Doc::Asset added;
    added.id = AssetIdFromDir(document, picked_dir);
    added.dir = picked_dir;
    added.kind = Doc::TypeOf(command) == Doc::CommandType::ModelDraw ? Doc::AssetKind::Scene3d
                                                                     : Doc::AssetKind::Package2d;
    field->set(command, Doc::ParamValue{added.id});
    document.assets.push_back(std::move(added));
    return FieldEvent::Committed;
}

}
