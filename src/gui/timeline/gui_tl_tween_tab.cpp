#include "gui_tl_forms.h"

#include "editor/preset_editor_state.h"
#include "editor/tween_edits.h"
#include "imgui.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "preset/eval/eval_tween.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

std::string ValueSummary(const Doc::Key& key) {
    std::string out;
    for (const Doc::KeyValue& value : key.values) {
        if (!out.empty()) out += ", ";
        out += value.id;
    }
    return out.empty() ? std::string("-") : out;
}

std::string ReachedText(const std::vector<Doc::Key>& keys, std::size_t index) {
    if (index == 0) return {};
    const Doc::Key& previous = keys[index - 1];
    if (previous.ease != Doc::Ease::SineDeg) return {};
    const float factor =
        Preset::Eval::EaseFactor(previous, keys[index].at, previous.at, keys[index].at);
    std::array<char, 48> text = {};
    (void)snprintf(text.data(), text.size(), "reaches %.2f of the way", (double)factor);
    return text.data();
}

int SelectedIndex(const Doc::Clip& clip) {
    const Editor::KeyRef key = Editor::Global().SelectedKey();
    if (key.clip_id != clip.id) return clip.keys.empty() ? -1 : 0;
    if (key.index < 0 || std::cmp_greater_equal(key.index, clip.keys.size())) return 0;
    return key.index;
}

FieldEvent DrawKeyList(Doc::Document& document, const Doc::Clip& clip, int selected) {
    FieldEvent event = FieldEvent::None;
    if (!ImGui::BeginTable("###tl_tween_keys", 5, ImGuiTableFlags_Borders)) return event;
    ImGui::TableSetupColumn("at");
    ImGui::TableSetupColumn("ease to next");
    ImGui::TableSetupColumn("values");
    ImGui::TableSetupColumn("reached");
    ImGui::TableSetupColumn("");
    ImGui::TableHeadersRow();
    for (std::size_t i = 0; i < clip.keys.size(); i++) {
        const Doc::Key& key = clip.keys[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const std::string row = std::to_string(key.at) + "###tl_tween_key_" + std::to_string(i);
        if (ImGui::Selectable(row.c_str(), std::cmp_equal(i, selected),
                              ImGuiSelectableFlags_SpanAllColumns)) {
            Editor::Global().SelectKey(clip.id, (int)i);
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(std::string(Doc::kEaseNames[(std::size_t)key.ease]).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ValueSummary(key).c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", ReachedText(clip.keys, i).c_str());
        ImGui::TableNextColumn();
        if (ImGui::SmallButton(("x###tl_tween_del_key_" + std::to_string(i)).c_str())) {
            Editor::DeleteKey(document, clip.id, (int)i);
            event = FieldEvent::Committed;
        }
    }
    ImGui::EndTable();
    return event;
}

FieldEvent DrawEaseRow(Doc::Document& document, const std::string& clip_id, int index,
                       const Doc::Key& key) {
    std::vector<std::string> names;
    names.reserve(Doc::kEaseNames.size());
    for (const std::string_view name : Doc::kEaseNames)
        names.emplace_back(name);
    int ease = (int)key.ease;
    FieldEvent event = DrawEnumRow("###tl_tween_ease", "ease to next", ease, names);
    if (event != FieldEvent::None) Editor::SetKeyEase(document, clip_id, index, (Doc::Ease)ease);

    if (key.ease == Doc::Ease::SineDeg) {
        double rate = key.rate_deg.value_or(0.0);
        const FieldEvent rate_event =
            DrawDoubleRow("###tl_tween_rate", "rate", rate, 0.01F, "deg/frame");
        if (rate_event != FieldEvent::None) Editor::SetKeyRate(document, clip_id, index, rate);
        event = std::max(event, rate_event);
    }
    if (key.ease != Doc::Ease::Bezier) return event;

    std::array<double, 4> cp = key.cp.value_or(std::array<double, 4>{0.42, 0.0, 0.58, 1.0});
    RowLabel("control points");
    ImGui::SetNextItemWidth(-30.0F);
    if (ImGui::DragScalarN("###tl_tween_cp", ImGuiDataType_Double, cp.data(), 4, 0.005F)) {
        Editor::SetKeyBezier(document, clip_id, index, cp);
        event = std::max(event, FieldEvent::Changed);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) event = FieldEvent::Committed;
    return event;
}

FieldEvent DrawValueRow(Doc::Document& document, const std::string& clip_id, int index,
                        const Editor::TweenField& field, const Doc::FieldDesc& desc,
                        const Doc::KeyValue& held) {
    const std::string id = "###tl_tween_field_" + field.id;
    const std::string label = field.id;
    Doc::ParamValue value = held.value;
    FieldEvent event = FieldEvent::None;
    if (auto* vector = std::get_if<Doc::Vec3>(&value)) {
        event =
            DrawVec3Row(id.c_str(), label.c_str(), *vector, 0.01F, std::string(desc.unit).c_str());
    } else if (auto* scalar = std::get_if<double>(&value)) {
        event = DrawDoubleRow(id.c_str(), label.c_str(), *scalar, 0.01F,
                              std::string(desc.unit).c_str());
    } else if (auto* integer = std::get_if<int>(&value)) {
        if (desc.kind == Doc::FieldKind::Enum) {
            std::vector<std::string> names;
            names.reserve(desc.enum_names.size());
            for (const std::string_view name : desc.enum_names)
                names.emplace_back(name);
            event = DrawEnumRow(id.c_str(), label.c_str(), *integer, names);
        } else {
            event = DrawIntRow(id.c_str(), label.c_str(), *integer);
        }
    }
    if (event != FieldEvent::None)
        Editor::SetKeyValue(document, clip_id, index, field.id, std::move(value));

    ImGui::SameLine();
    if (!ImGui::SmallButton(("x###tl_tween_unset_" + field.id).c_str())) return event;
    Editor::UnsetKeyValue(document, clip_id, index, field.id);
    return FieldEvent::Committed;
}

FieldEvent DrawFields(Doc::Document& document, const Doc::Clip& clip, int index, int playhead) {
    FieldEvent event = FieldEvent::None;
    const std::vector<Editor::TweenField> fields = Editor::TweenFields(document, clip.id);
    const Doc::Key& key = clip.keys[(std::size_t)index];
    for (const Editor::TweenField& field : fields) {
        const Doc::FieldDesc* desc =
            Doc::FindField(Doc::KeyFieldsFor(Doc::TypeOf(clip.command)), field.id);
        if (desc == nullptr) continue;
        const Doc::KeyValue* held = Editor::KeyValueOf(key, field.id);
        if (held != nullptr) {
            event = std::max(event, DrawValueRow(document, clip.id, index, field, *desc, *held));
            continue;
        }
        RowLabel(field.id.c_str());
        ImGui::TextDisabled("(not keyed)");
        ImGui::SameLine();
        if (!ImGui::SmallButton(("+ add###tl_tween_add_" + field.id).c_str())) continue;
        const std::optional<Doc::ParamValue> value =
            Editor::ResolvedFieldValue(document, clip.id, field.id, playhead);
        if (!value.has_value()) continue;
        Editor::SetKeyValue(document, clip.id, index, field.id, *value);
        event = FieldEvent::Committed;
    }
    return event;
}

}

FieldEvent DrawTweenTab(Doc::Document& document, const std::string& clip_id, int playhead) {
    const Doc::Clip* clip = Editor::ClipById(document, clip_id);
    if (clip == nullptr) return FieldEvent::None;
    const int duration = Editor::ClipDuration(document, clip_id);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("keys hold at (frames from the clip start), the ease that reaches the next "
                       "key, and the values they name. Clip duration %d frames.",
                       duration);
    ImGui::PopStyleColor();

    FieldEvent event = DrawKeyList(document, *clip, SelectedIndex(*clip));
    if (event != FieldEvent::None) return event;

    const int local = playhead - clip->start;
    const bool reachable = local >= 0 && local <= duration;
    ImGui::BeginDisabled(!reachable);
    if (ImGui::Button("+ key at playhead###tl_tween_add_key")) {
        const int index = Editor::AddKeyAt(document, clip_id, local);
        if (index >= 0) {
            Editor::Global().SelectKey(clip_id, index);
            ImGui::EndDisabled();
            return FieldEvent::Committed;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (reachable) {
        ImGui::TextDisabled("playhead frame %d, clip-relative %d", playhead, local);
    } else {
        ImGui::TextDisabled("playhead frame %d is outside this clip (clip-relative %d)", playhead,
                            local);
    }

    clip = Editor::ClipById(document, clip_id);
    const int index = SelectedIndex(*clip);
    if (index < 0) {
        ImGui::TextDisabled("This clip has no keys yet.");
        return event;
    }
    ImGui::SeparatorText(("Selected key: " + std::to_string(index)).c_str());

    int at = clip->keys[(std::size_t)index].at;
    const FieldEvent at_event = DrawIntRow("###tl_tween_at", "at", at);
    if (at_event != FieldEvent::None) Editor::MoveKey(document, clip_id, index, at);
    event = std::max(event, at_event);

    clip = Editor::ClipById(document, clip_id);
    event = std::max(event, DrawEaseRow(document, clip_id, index, clip->keys[(std::size_t)index]));
    clip = Editor::ClipById(document, clip_id);
    event = std::max(event, DrawFields(document, *clip, index, playhead));

    if (ImGui::Button("Open curve editor###tl_tween_curve")) {
        Editor::Global().PostRequest(
            Editor::Request{.kind = Editor::RequestKind::CurveEditor, .clip_id = clip_id});
    }
    ImGui::SameLine();
    ImGui::TextDisabled("C on a selected clip opens the same editor");
    return event;
}

}
