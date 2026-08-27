#include "gui_tl_modals.h"

#include "editor/document_edits.h"
#include "editor/library_model.h"
#include "editor/preset_editor_state.h"
#include "gui_tl_forms.h"
#include "gui/gui_dpi.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <string_view>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr const char* kTitle = "Document properties";
constexpr const char* kFpsTitle = "Convert fps";
constexpr float kModalWidth = 640.0F;
constexpr float kScreenMargin = 80.0F;

bool g_requested = false;
bool g_fresh = false;
bool g_focus_name = false;
std::vector<std::string> g_taken;
int g_base_undo = 0;
bool g_gesture = false;
int g_target_fps = 60;
int g_rounding = 0;
bool g_fps_requested = false;

FieldEvent TextRow(const char* id, const char* label, std::string& value, std::size_t limit) {
    RowLabel(label);
    std::vector<char> buffer(limit, '\0');
    std::copy_n(value.begin(), std::min(value.size(), limit - 1), buffer.begin());
    ImGui::SetNextItemWidth(Gui::Dpi::S(-30.0F));
    const bool changed = ImGui::InputText(id, buffer.data(), buffer.size());
    if (changed) value = buffer.data();
    if (ImGui::IsItemDeactivatedAfterEdit()) return FieldEvent::Committed;
    return changed ? FieldEvent::Changed : FieldEvent::None;
}

FieldEvent DrawGeneral(Doc::Document& document) {
    if (g_focus_name) {
        ImGui::SetKeyboardFocusHere();
        g_focus_name = false;
    }
    FieldEvent event = TextRow("###tl_doc_name", "name", document.name, 96);

    RowLabel("id");
    if (g_fresh) {
        ImGui::TextDisabled("made from the name when you press Done");
    } else {
        ImGui::TextDisabled("%s (read-only)", document.id.c_str());
    }
    RowLabel("build");
    ImGui::TextDisabled("%s (read-only)", document.build.c_str());

    event = std::max(event, TextRow("###tl_doc_notes", "notes", document.notes, 512));

    RowLabel("fps");
    ImGui::TextDisabled("%d", document.fps);
    ImGui::SameLine();
    if (ImGui::SmallButton("Convert fps...###tl_doc_convert_fps")) {
        g_target_fps = document.fps;
        g_fps_requested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rescale every clip edge, key, marker and the length to another fps.\n"
                          "Frames are integers, so the rounding is explicit.");
    }

    bool automatic = !document.length.has_value();
    RowLabel("auto length");
    if (ImGui::Checkbox("###tl_doc_auto", &automatic)) {
        document.length = automatic ? std::optional<int>{} : std::optional<int>{600};
        event = FieldEvent::Committed;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("max clip end, else the longest content tail");
    if (!automatic) {
        int length = document.length.value_or(600);
        const FieldEvent length_event = DrawIntRow("###tl_doc_length", "length", length);
        if (length_event != FieldEvent::None) document.length = std::max(1, length);
        event = std::max(event, length_event);
    }

    event = std::max(event, DrawIntRow("###tl_doc_seed", "rng seed", document.rng_seed));
    return event;
}

FieldEvent DrawRender(Doc::Document& document) {
    FieldEvent event = DrawIntRow("###tl_doc_width", "width", document.render.width);
    event = std::max(event, DrawIntRow("###tl_doc_height", "height", document.render.height));

    RowLabel("opaque");
    if (ImGui::Checkbox("###tl_doc_opaque", &document.render.opaque)) event = FieldEvent::Committed;
    ImGui::SameLine();
    ImGui::TextDisabled("the screen has no transparent background");

    int shading = (int)document.render.shading;
    std::vector<std::string> names;
    names.reserve(Doc::kShadingNames.size());
    for (const std::string_view name : Doc::kShadingNames)
        names.emplace_back(name);
    const FieldEvent shade = DrawEnumRow("###tl_doc_shading", "shading", shading, names);
    if (shade != FieldEvent::None) document.render.shading = (Doc::Shading)shading;
    event = std::max(event, shade);

    event = std::max(event, DrawIntRow("###tl_doc_split", "sprite split priority",
                                       document.render.sprite_split_priority));
    event = std::max(event, DrawVec3Row("###tl_doc_clear", "clear colour",
                                        document.render.clear_color, 0.005F, "rgb"));
    return event;
}

FieldEvent DrawCamera(Doc::Document& document) {
    Doc::CameraSpec& camera = document.camera;
    FieldEvent event = DrawVec3Row("###tl_doc_eye", "eye", camera.eye, 0.01F, "world");
    event = std::max(event, DrawVec3Row("###tl_doc_at", "at", camera.at, 0.01F, "world"));
    event = std::max(event, DrawVec3Row("###tl_doc_up", "up", camera.up, 0.01F, ""));
    event = std::max(event, DrawDoubleRow("###tl_doc_fov", "fov y", camera.fov_y, 0.005F, "rad"));
    event = std::max(event, DrawDoubleRow("###tl_doc_near", "near z", camera.near_z, 0.001F, ""));
    event = std::max(event, DrawDoubleRow("###tl_doc_far", "far z", camera.far_z, 0.5F, ""));

    RowLabel("aspect");
    bool automatic = camera.aspect.automatic;
    if (ImGui::Checkbox("###tl_doc_aspect_auto", &automatic)) {
        camera.aspect.automatic = automatic;
        event = FieldEvent::Committed;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("auto from the render size");
    if (!camera.aspect.automatic) {
        event = std::max(event, DrawDoubleRow("###tl_doc_aspect", "aspect value",
                                              camera.aspect.value, 0.001F, ""));
    }
    return event;
}

FieldEvent DrawLights(Doc::Document& document) {
    FieldEvent event = FieldEvent::None;
    for (std::size_t i = 0; i < document.lights.size(); i++) {
        ImGui::PushID((int)i);
        Doc::LightSpec& light = document.lights[i];
        ImGui::SeparatorText(("light " + std::to_string(i)).c_str());
        event = std::max(
            event, DrawVec3Row("###tl_doc_light_dir", "direction", light.direction, 0.01F, ""));
        event = std::max(
            event, DrawVec3Row("###tl_doc_light_diffuse", "diffuse", light.diffuse, 0.01F, "rgb"));
        event = std::max(event, DrawVec3Row("###tl_doc_light_specular", "specular", light.specular,
                                            0.01F, "rgb"));
        event = std::max(
            event, DrawVec3Row("###tl_doc_light_ambient", "ambient", light.ambient, 0.01F, "rgb"));
        if (ImGui::SmallButton("remove###tl_doc_light_remove")) {
            document.lights.erase(document.lights.begin() + (long long)i);
            ImGui::PopID();
            return FieldEvent::Committed;
        }
        ImGui::PopID();
    }
    if (ImGui::Button("+ light###tl_doc_light_add")) {
        document.lights.push_back(Doc::LightSpec{});
        event = FieldEvent::Committed;
    }
    return event;
}

FieldEvent DrawTabs(Doc::Document& working) {
    FieldEvent event = FieldEvent::None;
    if (!ImGui::BeginTabBar("###tl_doc_tabs")) return event;
    if (ImGui::BeginTabItem("General###tl_doc_tab_general")) {
        event = std::max(event, DrawGeneral(working));
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Render###tl_doc_tab_render")) {
        event = std::max(event, DrawRender(working));
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Camera###tl_doc_tab_camera")) {
        event = std::max(event, DrawCamera(working));
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Lights###tl_doc_tab_lights")) {
        event = std::max(event, DrawLights(working));
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    return event;
}

void EndGesture(Editor::State& editor) {
    if (!g_gesture) return;
    editor.EndGesture();
    g_gesture = false;
}

void Commit(Editor::State& editor, const Doc::Document& working, FieldEvent event) {
    if (event == FieldEvent::None) return;
    if (event == FieldEvent::Changed && !g_gesture) {
        editor.BeginGesture();
        g_gesture = true;
    }
    editor.Apply([&working](Doc::Document& document) {
        document = working;
        return true;
    });
    if (event == FieldEvent::Committed && g_gesture) {
        editor.EndGesture();
        g_gesture = false;
    }
}

void RenderFpsModal(Editor::State& editor) {
    if (g_fps_requested) {
        ImGui::OpenPopup(kFpsTitle);
        g_fps_requested = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    if (!ImGui::BeginPopupModal(kFpsTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    RowLabel("current fps");
    ImGui::TextDisabled("%d", editor.Document().fps);
    DrawIntRow("###tl_fps_target", "target fps", g_target_fps);
    DrawEnumRow("###tl_fps_rounding", "rounding", g_rounding, {"nearest", "floor", "ceil"});

    const Editor::FpsPreview preview =
        Editor::PreviewConvertFps(editor.Document(), std::max(1, g_target_fps));
    ImGui::TextDisabled("%d clip edge(s) and %d key(s) move by a fraction of a frame",
                        preview.edges, preview.keys);

    if (ImGui::Button("Cancel###tl_fps_cancel")) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button("Convert###tl_fps_convert")) {
        const int target = std::max(1, g_target_fps);
        const auto rounding = (Editor::Rounding)g_rounding;
        editor.Apply([target, rounding](Doc::Document& document) {
            return Editor::ConvertFps(document, target, rounding);
        });
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}

void ResetDocumentModal() {
    if (g_gesture) {
        Editor::Global().EndGesture();
        g_gesture = false;
    }
    g_requested = false;
    g_fps_requested = false;
    g_fresh = false;
    g_focus_name = false;
    g_taken.clear();
}

void RequestDocumentModal() {
    g_requested = true;
    g_fresh = false;
    g_base_undo = Editor::Global().UndoDepth();
}

void RequestNewDocumentModal(std::vector<std::string> taken_ids) {
    g_requested = true;
    g_fresh = true;
    g_focus_name = true;
    g_taken = std::move(taken_ids);
    g_base_undo = Editor::Global().UndoDepth();
}

void RenderDocumentModal() {
    Editor::State& editor = Editor::Global();
    if (g_requested) {
        ImGui::OpenPopup(kTitle);
        g_requested = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(
        Gui::Dpi::S(kModalWidth, 0.0F),
        ImVec2(Gui::Dpi::S(kModalWidth), viewport->WorkSize.y - Gui::Dpi::S(kScreenMargin)));
    if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!editor.Loaded()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    Doc::Document working = editor.Document();
    ImGui::TextDisabled("every field here is one undo entry, applied live to the viewport");
    ImGui::Separator();

    Commit(editor, working, DrawTabs(working));

    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && editor.UndoDepth() > g_base_undo) {
        editor.Undo();
    }

    ImGui::Separator();
    const bool escape = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
                        !ImGui::GetIO().WantTextInput &&
                        ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (ImGui::Button("Cancel###tl_doc_cancel") || escape) {
        while (editor.UndoDepth() > g_base_undo)
            editor.Undo();
        editor.ClearRedo();
        EndGesture(editor);
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Done###tl_doc_done")) {
        if (g_fresh) {
            const std::vector<std::string> taken = g_taken;
            editor.Apply([&taken](Doc::Document& document) {
                document.id = Editor::UniqueId(Editor::Slug(document.name), taken);
                return true;
            });
            g_fresh = false;
        }
        editor.CollapseUndo(g_base_undo);
        EndGesture(editor);
        ImGui::CloseCurrentPopup();
    }
    RenderFpsModal(editor);
    ImGui::EndPopup();
}

}
