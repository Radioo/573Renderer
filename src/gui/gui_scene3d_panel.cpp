#include "gui_scene3d_panel.h"

#include "imgui.h"
#include "scene3d/camera.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_host.h"

#include <cstddef>
#include <vector>

#include <algorithm>
#include <cfloat>

namespace Panels::Scene3dPanel {

namespace {

void DrawPlayback(const Scene3dHost::Status& st) {
    bool paused = st.paused;
    if (ImGui::Checkbox("Pause##s3d", &paused)) Scene3dHost::SetPaused(paused);
    ImGui::SameLine();
    float speed = st.speed;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("speed##s3d", &speed, 0.0F, 4.0F, "%.2fx")) {
        Scene3dHost::SetSpeed(speed);
    }

    float t = st.time;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##s3dtime", &t, 0.0F, std::max(st.max_time, 1.0F), "tick %.0f")) {
        Scene3dHost::SetTime(t);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Animation time in .x AnimationKey ticks.\n"
                          "The scene loops at %.0f ticks.",
                          st.max_time);
    }
}

const char* BlendLabel(int mode) {
    switch (mode) {
    case Scene3d::kBlendAlpha:
        return "alpha";
    case Scene3d::kBlendAdditive:
        return "additive";
    case Scene3d::kBlendSubtract:
        return "subtract";
    default:
        return "opaque";
    }
}

void DrawAnimationToggles(const Scene3dHost::Status& st) {
    bool models = st.animate_models;
    if (ImGui::Checkbox("Animate models##s3d", &models)) Scene3dHost::SetAnimateModels(models);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Freeze every frame transform where it is now.\n"
                          "Stops models spinning / drifting while the clock keeps running.");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!st.has_authored_camera || st.free_camera);
    bool camera = st.animate_camera;
    if (ImGui::Checkbox("Animate camera##s3d", &camera)) Scene3dHost::SetAnimateCamera(camera);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        if (!st.has_authored_camera) {
            ImGui::SetTooltip("This scene has no camera in its .x file.");
        } else if (st.free_camera) {
            ImGui::SetTooltip("Free camera is on, so the authored camera is not driving\n"
                              "the view. Turn free camera off to use this.");
        } else {
            ImGui::SetTooltip("Freeze the camera animated inside the .x file.\n"
                              "Stops the scene orbiting without pausing the models.");
        }
    }
}

void DrawModelList() {
    std::vector<Scene3dHost::ModelInfo> models = Scene3dHost::ListModels();
    if (models.empty()) return;
    if (!ImGui::CollapsingHeader("Models##s3d", ImGuiTreeNodeFlags_DefaultOpen)) return;

    for (size_t i = 0; i < models.size(); i++) {
        ImGui::PushID((int)i);
        bool vis = models[i].visible;
        if (ImGui::Checkbox("##vis", &vis)) Scene3dHost::SetModelVisible((int)i, vis);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ImGui::BeginCombo("##blend", BlendLabel(models[i].blend_mode))) {
            const int modes[] = {Scene3d::kBlendOpaque, Scene3d::kBlendAlpha,
                                 Scene3d::kBlendAdditive, Scene3d::kBlendSubtract};
            for (const int mode : modes) {
                const bool sel = models[i].blend_mode == mode;
                if (ImGui::Selectable(BlendLabel(mode), sel))
                    Scene3dHost::SetModelBlend((int)i, mode);
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(models[i].name.c_str());
        ImGui::PopID();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Blend mode comes from the game's per-screen setup code.\n"
                          "Override it here to inspect a layer.");
    }
}

void DrawCameraControls(const Scene3dHost::Status& st) {
    bool freecam = st.free_camera;
    if (ImGui::Checkbox("Free camera##s3d", &freecam)) Scene3dHost::SetFreeCamera(freecam);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("OFF uses the camera animated inside the .x file.\n"
                          "%s",
                          st.has_authored_camera
                              ? "This scene has one."
                              : "This scene has NO authored camera, so free look is\n"
                                "the only way to see it.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset view##s3d")) Scene3dHost::ResetCamera();

    auto& cam = Scene3dHost::MutCamera();
    ImGui::SetNextItemWidth(140);
    ImGui::DragFloat("move speed##s3d", &cam.speed, 0.05F, 0.01F, 10000.0F, "%.2f /s");

    ImGui::Spacing();
    ImGui::TextDisabled("Hold RIGHT MOUSE in the render window to look around.");
    ImGui::BulletText("W / A / S / D  move");
    ImGui::BulletText("E or Space  up,   Q or Ctrl  down");
    ImGui::BulletText("Shift  faster,   Alt  slower");
    ImGui::BulletText("Mouse wheel is free for the panel; use 'move speed' above");

    ImGui::Spacing();
    ImGui::Text("pos  %.1f, %.1f, %.1f", cam.x, cam.y, cam.z);
    ImGui::Text("yaw  %.1f deg   pitch %.1f deg", cam.yaw * 57.29578F, cam.pitch * 57.29578F);
}

}

void Render() {
    if (!Scene3dHost::Active()) {
        ImGui::TextDisabled("No 3D scene loaded.");
        ImGui::TextWrapped("Pick a folder marked [3D scene] in the Browse list. "
                           "IIDX 18 ships mode_bg, resort_st and boss_st.");
        return;
    }

    const Scene3dHost::Status st = Scene3dHost::GetStatus();
    ImGui::Text("%s", st.name.c_str());
    ImGui::TextDisabled("%d models, %d texture tiles, %d draw calls", st.models, st.tiles,
                        st.draw_calls);
    ImGui::Separator();
    DrawPlayback(st);
    DrawAnimationToggles(st);
    ImGui::Separator();
    DrawModelList();
    ImGui::Separator();
    DrawCameraControls(st);
}

}
