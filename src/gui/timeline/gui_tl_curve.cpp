#include "gui_tl_internal.h"

#include "editor/curve_geometry.h"
#include "gui/gui_dpi.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/tween_edits.h"
#include "imgui.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_tween.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr float kPlotPadDips = 34.0F;
constexpr float kPlotTopDips = 26.0F;
constexpr float kTitleWDips = 260.0F;
constexpr float kPointHalfDips = 6.0F;
constexpr float kHandleHalfDips = 5.0F;

float PlotPad() {
    return Gui::Dpi::S(kPlotPadDips);
}

float PlotTop() {
    return Gui::Dpi::S(kPlotTopDips);
}

float TitleW() {
    return Gui::Dpi::S(kTitleWDips);
}

float PointHalf() {
    return Gui::Dpi::S(kPointHalfDips);
}

float HandleHalf() {
    return Gui::Dpi::S(kHandleHalfDips);
}

using Channel = Editor::CurveChannel;

std::string g_clip_id;
int g_channel = 0;
int g_drag_key = -1;
int g_drag_handle = -1;
Editor::CurveRange g_range;
bool g_range_held = false;

Doc::ParamValue WithComponent(Doc::ParamValue base, int component, double value) {
    if (std::holds_alternative<int>(base)) return (int)std::lround(value);
    if (auto* vector = std::get_if<Doc::Vec3>(&base)) {
        (*vector)[(std::size_t)std::clamp(component, 0, 2)] = value;
        return base;
    }
    return value;
}

Preset::Eval::TweenValue Underlying(const Doc::Document& document, const Doc::Clip& clip,
                                    const Channel& channel) {
    const std::optional<Doc::ParamValue> value =
        Editor::ResolvedFieldValue(document, clip.id, channel.field, clip.start);
    if (!value.has_value()) return {};
    return Preset::Eval::FromParamValue(*value);
}

void DrawPolyline(const Ctx& ctx, const Editor::CurveRect& rect, const Doc::Clip& clip,
                  const Channel& channel, int duration, ImU32 color, float thickness) {
    const std::vector<Editor::CurveSample> samples =
        Editor::CurveSamples(clip, channel, Underlying(*ctx.document, clip, channel), duration);
    std::vector<ImVec2> points;
    points.reserve(samples.size());
    for (const Editor::CurveSample& sample : samples) {
        points.emplace_back(Editor::CurveX(rect, duration, sample.frame),
                            Editor::CurveY(rect, g_range, sample.value));
    }
    if (points.size() < 2) return;
    ctx.draw->AddPolyline(points.data(), (int)points.size(), color, 0, thickness);
}

void DrawReference(const Ctx& ctx, const Editor::CurveRect& rect, const Doc::Clip& clip,
                   const Channel& channel, int duration) {
    const Editor::ClipRef ref = Editor::FindClip(*ctx.document, clip.id);
    if (!ref.Valid()) return;
    const std::string& target = ctx.document->tracks[(std::size_t)ref.track].target;
    const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    for (const Doc::Track& track : ctx.document->tracks) {
        if (track.target != target) continue;
        for (const Doc::Clip& other : track.clips) {
            if (other.id == clip.id || other.keys.empty()) continue;
            if (Editor::KeyValueOf(other.keys.front(), channel.field) == nullptr) continue;
            DrawPolyline(ctx, rect, other, channel, duration, muted, Gui::Dpi::S(1.0F));
        }
    }
}

void DrawGrid(const Ctx& ctx, const Editor::CurveRect& rect, int duration) {
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    ctx.draw->AddRect(ImVec2(rect.x0, rect.y0), ImVec2(rect.x1, rect.y1), border);
    std::array<char, 32> text = {};
    for (int i = 0; i <= 4; i++) {
        const float y = rect.y0 + ((rect.y1 - rect.y0) * (float)i * 0.25F);
        ctx.draw->AddLine(ImVec2(rect.x0, y), ImVec2(rect.x1, y), border, Gui::Dpi::S(1.0F));
        (void)snprintf(text.data(), text.size(), "%.3f", Editor::CurveValue(rect, g_range, y));
        ctx.draw->AddText(ImVec2(rect.x0 - PlotPad() + Gui::Dpi::S(2.0F), y - Gui::Dpi::S(7.0F)),
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), text.data());
    }
    for (int i = 0; i <= 4; i++) {
        const float x = rect.x0 + ((rect.x1 - rect.x0) * (float)i * 0.25F);
        ctx.draw->AddLine(ImVec2(x, rect.y0), ImVec2(x, rect.y1), border, Gui::Dpi::S(1.0F));
        (void)snprintf(text.data(), text.size(), "%d", (duration * i) / 4);
        const float width = ImGui::CalcTextSize(text.data()).x;
        const float at = (i == 4) ? (x - width - Gui::Dpi::S(2.0F)) : (x + Gui::Dpi::S(2.0F));
        ctx.draw->AddText(ImVec2(at, rect.y1 + Gui::Dpi::S(2.0F)),
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), text.data());
    }
}

void ApplyKeyDrag(Ctx& ctx, const Doc::Clip& clip, const Channel& channel,
                  const Editor::CurveRect& rect, int duration) {
    if (g_drag_key < 0 || std::cmp_greater_equal(g_drag_key, clip.keys.size())) return;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const int at = (int)std::lround(Editor::CurveFrame(rect, duration, mouse.x));
    const double value = Editor::CurveValue(rect, g_range, mouse.y);
    const std::string id = clip.id;
    const int index = g_drag_key;
    const std::string field = channel.field;
    const int component = channel.component;
    const Doc::KeyValue* held = Editor::KeyValueOf(clip.keys[(std::size_t)index], field);
    const Doc::ParamValue base = (held != nullptr) ? held->value : Doc::ParamValue{0.0};
    const Doc::ParamValue next = WithComponent(base, component, value);
    ApplyEdit(ctx, [id, index, at, field, next](Doc::Document& document) {
        const bool moved = Editor::MoveKey(document, id, index, at);
        return Editor::SetKeyValue(document, id, index, field, next) || moved;
    });
}

void ApplyHandleDrag(Ctx& ctx, const Doc::Clip& clip, const Channel& channel,
                     const Editor::CurveRect& rect, int duration, int key) {
    if (std::cmp_greater_equal(key + 1, clip.keys.size())) return;
    const Doc::Key& from = clip.keys[(std::size_t)key];
    const Doc::Key& to = clip.keys[(std::size_t)key + 1];
    const Doc::KeyValue* a = Editor::KeyValueOf(from, channel.field);
    const Doc::KeyValue* b = Editor::KeyValueOf(to, channel.field);
    if (a == nullptr || b == nullptr || !from.cp.has_value()) return;

    const Editor::CurveSegment segment{.a_at = from.at,
                                       .b_at = to.at,
                                       .a_value = Editor::ChannelValue(a->value, channel.component),
                                       .b_value =
                                           Editor::ChannelValue(b->value, channel.component)};
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const std::array<double, 4> cp =
        Editor::BezierWithHandle(rect, g_range, duration, segment, *from.cp, g_drag_handle,
                                 Editor::CurvePoint{.x = mouse.x, .y = mouse.y});
    const std::string id = clip.id;
    ApplyEdit(ctx, [id, key, cp](Doc::Document& document) {
        return Editor::SetKeyBezier(document, id, key, cp);
    });
}

void DrawHandles(Ctx& ctx, const Doc::Clip& clip, const Channel& channel,
                 const Editor::CurveRect& rect, int duration) {
    const Editor::KeyRef selected = ctx.editor->SelectedKey();
    if (selected.clip_id != clip.id || selected.index < 0) return;
    if (std::cmp_greater_equal(selected.index + 1, clip.keys.size())) return;
    const Doc::Key& from = clip.keys[(std::size_t)selected.index];
    const Doc::Key& to = clip.keys[(std::size_t)selected.index + 1];
    if (from.ease != Doc::Ease::Bezier || !from.cp.has_value()) return;
    const Doc::KeyValue* a = Editor::KeyValueOf(from, channel.field);
    const Doc::KeyValue* b = Editor::KeyValueOf(to, channel.field);
    if (a == nullptr || b == nullptr) return;

    const Editor::CurveSegment segment{.a_at = from.at,
                                       .b_at = to.at,
                                       .a_value = Editor::ChannelValue(a->value, channel.component),
                                       .b_value =
                                           Editor::ChannelValue(b->value, channel.component)};
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    for (int handle = 0; handle < 2; handle++) {
        const Editor::CurvePoint point =
            Editor::BezierHandle(rect, g_range, duration, segment, *from.cp, handle);
        const float anchor_frame = (handle == 0) ? (float)segment.a_at : (float)segment.b_at;
        const double anchor_value = (handle == 0) ? segment.a_value : segment.b_value;
        ctx.draw->AddLine(ImVec2(Editor::CurveX(rect, duration, anchor_frame),
                                 Editor::CurveY(rect, g_range, anchor_value)),
                          ImVec2(point.x, point.y), accent, Gui::Dpi::S(1.0F));
        ctx.draw->AddCircle(ImVec2(point.x, point.y), HandleHalf(), accent, 0, Gui::Dpi::S(2.0F));

        ImGui::SetCursorScreenPos(ImVec2(point.x - HandleHalf(), point.y - HandleHalf()));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(("###tl_curve_handle_" + std::to_string(handle)).c_str(),
                               ImVec2(2.0F * HandleHalf(), 2.0F * HandleHalf()));
        if (ImGui::IsItemActivated()) {
            g_drag_handle = handle;
            ctx.editor->BeginGesture();
        }
        if (g_drag_handle == handle && ImGui::IsItemActive())
            ApplyHandleDrag(ctx, clip, channel, rect, duration, selected.index);
    }
}

void DrawPoints(Ctx& ctx, const Doc::Clip& clip, const Channel& channel,
                const Editor::CurveRect& rect, int duration) {
    const Preset::Eval::TweenValue underlying = Underlying(*ctx.document, clip, channel);
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    for (std::size_t i = 0; i < clip.keys.size(); i++) {
        const Doc::Key& key = clip.keys[i];
        const Doc::KeyValue* held = Editor::KeyValueOf(key, channel.field);
        const double value = (held != nullptr)
                                 ? Editor::ChannelValue(held->value, channel.component)
                                 : Editor::CurveSampleAt(clip, channel, underlying, key.at);
        const float x = Editor::CurveX(rect, duration, key.at);
        const float y = Editor::CurveY(rect, g_range, value);
        const bool selected =
            ctx.editor->SelectedKey() == Editor::KeyRef{.clip_id = clip.id, .index = (int)i};
        if (held != nullptr) {
            ctx.draw->AddQuadFilled(ImVec2(x, y - PointHalf()), ImVec2(x + PointHalf(), y),
                                    ImVec2(x, y + PointHalf()), ImVec2(x - PointHalf(), y),
                                    selected ? text : accent);
        } else {
            ctx.draw->AddQuad(ImVec2(x, y - PointHalf()), ImVec2(x + PointHalf(), y),
                              ImVec2(x, y + PointHalf()), ImVec2(x - PointHalf(), y),
                              ImGui::GetColorU32(ImGuiCol_TextDisabled), Gui::Dpi::S(1.0F));
        }

        ImGui::SetCursorScreenPos(ImVec2(x - PointHalf(), y - PointHalf()));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(("###tl_curve_key_" + std::to_string(i)).c_str(),
                               ImVec2(2.0F * PointHalf(), 2.0F * PointHalf()));
        if (ImGui::IsItemActivated()) {
            ctx.editor->SelectKey(clip.id, (int)i);
            g_drag_key = (int)i;
            g_range_held = true;
            ctx.editor->BeginGesture();
        }
        if (std::cmp_equal(g_drag_key, i) && ImGui::IsItemActive())
            ApplyKeyDrag(ctx, clip, channel, rect, duration);
    }
}

void EndDrags(Ctx& ctx) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
    if (g_drag_key >= 0 || g_drag_handle >= 0) ctx.editor->EndGesture();
    g_drag_key = -1;
    g_drag_handle = -1;
    g_range_held = false;
}

void DrawToolbar(Ctx& ctx, const Doc::Clip& clip, const std::vector<Channel>& channels) {
    ImGui::TextUnformatted("Curve editor");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Ellipsized(clip.id, TitleW()).c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(Gui::Dpi::S(160.0F));
    std::vector<const char*> names;
    names.reserve(channels.size());
    for (const Channel& channel : channels)
        names.push_back(channel.label.c_str());
    ImGui::Combo("###tl_curve_channel", &g_channel, names.data(), (int)names.size());

    const Editor::KeyRef selected = ctx.editor->SelectedKey();
    if (selected.clip_id == clip.id && selected.index >= 0) {
        for (std::size_t i = 0; i < Doc::kEaseNames.size(); i++) {
            ImGui::SameLine();
            std::string button(Doc::kEaseNames[i]);
            button.append("###tl_curve_ease_").append(Doc::kEaseNames[i]);
            if (!ImGui::Button(button.c_str())) continue;
            const std::string id = clip.id;
            const int index = selected.index;
            const auto ease = (Doc::Ease)i;
            ApplyEdit(ctx, [id, index, ease](Doc::Document& document) {
                return Editor::SetKeyEase(document, id, index, ease);
            });
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close###tl_curve_close")) CloseCurveEditor();
}

void DrawPlayhead(const Ctx& ctx, const Doc::Clip& clip, const Channel& channel,
                  const Editor::CurveRect& rect, int duration) {
    const int local = ctx.status.frame - clip.start;
    if (local < 0 || local > duration) return;
    const float x = Editor::CurveX(rect, duration, local);
    ctx.draw->AddLine(ImVec2(x, rect.y0), ImVec2(x, rect.y1), ImGui::GetColorU32(ImGuiCol_Text),
                      Gui::Dpi::S(2.0F));
    const double value =
        Editor::CurveSampleAt(clip, channel, Underlying(*ctx.document, clip, channel), local);
    std::array<char, 64> text = {};
    (void)snprintf(text.data(), text.size(), "%s %.4f at %d", channel.label.c_str(), value, local);
    ctx.draw->AddText(ImVec2(x + Gui::Dpi::S(4.0F), rect.y0 + Gui::Dpi::S(2.0F)),
                      ImGui::GetColorU32(ImGuiCol_Text), text.data());
}

}

void RequestCurveEditor(std::string clip_id) {
    g_clip_id = std::move(clip_id);
    g_channel = 0;
    g_drag_key = -1;
    g_drag_handle = -1;
    g_range_held = false;
}

void CloseCurveEditor() {
    g_clip_id.clear();
}

bool CurveEditorOpen(const Ctx& ctx) {
    if (g_clip_id.empty() || ctx.document == nullptr) return false;
    return Editor::ClipById(*ctx.document, g_clip_id) != nullptr;
}

void DrawCurveEditor(Ctx& ctx) {
    const Doc::Clip* clip = Editor::ClipById(*ctx.document, g_clip_id);
    if (clip == nullptr) return;
    const std::vector<Channel> channels = Editor::ChannelsOf(*clip);
    ImGui::SetCursorScreenPos(ImVec2(ctx.header_x, ctx.lanes_top - RulerH() + Gui::Dpi::S(2.0F)));
    if (channels.empty()) {
        ImGui::TextDisabled("This clip has no keyed values yet. Add one in the Tween tab.");
        ImGui::SameLine();
        if (ImGui::Button("Close###tl_curve_close")) CloseCurveEditor();
        return;
    }
    g_channel = std::clamp(g_channel, 0, (int)channels.size() - 1);
    if (ctx.editor->SelectedKey().clip_id != clip->id) ctx.editor->SelectKey(clip->id, 0);
    DrawToolbar(ctx, *clip, channels);

    const Channel& channel = channels[(std::size_t)g_channel];
    const int duration = std::max(1, Editor::ClipDuration(*ctx.document, clip->id));
    const Editor::CurveRect rect{.x0 = ctx.header_x + PlotPad(),
                                 .y0 = ctx.lanes_top + PlotTop(),
                                 .x1 = ctx.lane_x + ctx.lane_w - Gui::Dpi::S(8.0F),
                                 .y1 = std::max(ctx.lanes_top + PlotTop() + Gui::Dpi::S(32.0F),
                                                ctx.lanes_bottom - PlotPad())};
    if (!g_range_held) {
        g_range = Editor::CurveAutoRange(*clip, channel, Underlying(*ctx.document, *clip, channel),
                                         duration);
    }

    DrawGrid(ctx, rect, duration);
    DrawReference(ctx, rect, *clip, channel, duration);
    for (const Channel& other : channels) {
        if (other.label == channel.label) continue;
        DrawPolyline(ctx, rect, *clip, other, duration, ImGui::GetColorU32(ImGuiCol_TextDisabled),
                     Gui::Dpi::S(1.0F));
    }
    DrawPolyline(ctx, rect, *clip, channel, duration, ImGui::GetColorU32(ImGuiCol_CheckMark),
                 Gui::Dpi::S(2.0F));
    DrawPlayhead(ctx, *clip, channel, rect, duration);
    DrawPoints(ctx, *clip, channel, rect, duration);
    DrawHandles(ctx, *clip, channel, rect, duration);
    EndDrags(ctx);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) CloseCurveEditor();
}

}
