#include "gui_preset_panel.h"

#include "game_fingerprint.h"
#include "imgui.h"
#include "gc2d/gc_host.h"
#include "preset/preset_host.h"
#include "preset/scene_preset.h"
#include "state/app_state.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Panels::PresetPanel {

namespace {

std::string g_scanned_dir;
std::string g_build_name;
std::string g_build_id;
std::vector<const Preset::Scene*> g_scenes;

void RescanIfNeeded() {
    const std::string dir = App::Global().GameDir();
    if (dir == g_scanned_dir) return;
    g_scanned_dir = dir;
    g_build_name.clear();
    g_build_id.clear();
    g_scenes.clear();
    const GameFingerprint::Match match = GameFingerprint::Identify(dir);
    if (match.build == nullptr) return;
    g_build_name = match.build->name;
    g_build_id = match.build->id;
    g_scenes = Preset::ForBuild(g_build_id);
}

void DrawOptions(const Preset::Scene& scene, const PresetHost::Status& status) {
    for (std::size_t i = 0; i < scene.options.size(); i++) {
        const Preset::Option& option = scene.options[i];
        if (i >= status.option_choices.size()) break;
        const int current = status.option_choices[i];
        std::string title(option.label);
        title += ": ";
        title += option.choices[(std::size_t)current].label;
        std::string tag("##presetoption");
        tag += option.id;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!ImGui::BeginCombo(tag.c_str(), title.c_str())) continue;
        for (std::size_t c = 0; c < option.choices.size(); c++) {
            const std::string choice(option.choices[c].label);
            if (ImGui::Selectable(choice.c_str(), std::cmp_equal(c, current))) {
                PresetHost::SetOption((int)i, (int)c);
            }
        }
        ImGui::EndCombo();
    }
}

void DrawSpriteFrames() {
    const std::vector<Gc2dHost::SpriteStatus> sprites = Gc2dHost::ListSprites();
    if (sprites.empty()) return;

    ImGui::Separator();
    ImGui::TextDisabled("2D layers");
    for (std::size_t i = 0; i < sprites.size(); i++) {
        const Gc2dHost::SpriteStatus& sprite = sprites[i];
        const std::string tag = std::to_string(i);
        if (sprite.scroll_wrap > 0) {
            int scroll = std::clamp(sprite.scroll, 0, sprite.scroll_wrap - 1);
            const std::string format =
                sprite.name + ": scrolled %d / " + std::to_string(sprite.scroll_wrap) + " px";
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::SliderInt(("##presetscroll" + tag).c_str(), &scroll, 0,
                                 sprite.scroll_wrap - 1, format.c_str())) {
                Gc2dHost::SetSpriteScroll((int)i, scroll);
            }
        }
        if (sprite.length <= 0) {
            if (sprite.scroll_wrap <= 0)
                ImGui::TextDisabled("%s (static cell)", sprite.name.c_str());
            continue;
        }
        if (sprite.frame < 0) {
            ImGui::TextDisabled("%s (past its end, not drawn)", sprite.name.c_str());
            continue;
        }
        const int last = sprite.length - 1;
        int frame = std::clamp(sprite.frame, 0, last);
        std::string format(sprite.name);
        format += ": frame %d / " + std::to_string(last);
        if (sprite.playhead >= sprite.length)
            format += " (playhead " + std::to_string(sprite.playhead) + ")";
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt(("##presetsprite" + tag).c_str(), &frame, 0, last, format.c_str())) {
            Gc2dHost::SetSpriteFrame((int)i, frame);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Each 2D layer runs on its own playhead, the way the game gives\n"
                          "every registered animation its own frame counter. Drag one to\n"
                          "scrub that layer without touching the others; playback resumes\n"
                          "from where you leave it.");
    }
}

void DrawCountdown(const PresetHost::Status& status) {
    if (status.countdown_start <= 0) return;
    int countdown = status.countdown;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##presetcountdown", &countdown, 0, status.countdown_start,
                         "time remain %d frames")) {
        PresetHost::SetCountdown(countdown);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "The screen timer the game counts down once per frame. Drag it below the ramp "
            "threshold to see the end-of-timer speed-up.");
    }
    ImGui::Text("model speed %.2f ticks/frame, alpha %.2f, blend %d", status.model_speed,
                status.model_alpha, status.blend_mode);
}

}

bool HasPresets() {
    RescanIfNeeded();
    return !g_scenes.empty();
}

void Render() {
    RescanIfNeeded();

    if (g_build_name.empty()) {
        ImGui::TextDisabled("No known build fingerprint under this directory.");
        return;
    }
    ImGui::TextDisabled("%s", g_build_name.c_str());
    ImGui::Separator();

    if (g_scenes.empty()) {
        ImGui::TextDisabled("No scene presets are registered for this build.");
        return;
    }

    const PresetHost::Status status = PresetHost::GetStatus();
    for (const auto* scene : g_scenes) {
        const std::string name(scene->name);
        const std::string id(scene->id);
        const bool active = (status.id == id);
        if (ImGui::RadioButton(name.c_str(), active) && !active) {
            PresetHost::Load(g_scanned_dir, *scene);
        }
    }

    if (!PresetHost::Active()) {
        ImGui::Separator();
        ImGui::TextDisabled("Pick a screen to reproduce it exactly as the game draws it.");
        return;
    }

    ImGui::Separator();
    if (ImGui::Button("Unload preset")) PresetHost::Unload();
    ImGui::SameLine();
    ImGui::TextDisabled("background layers only");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("A screen preset reproduces the screen's BACKGROUND so it can be "
                          "captured on its own. The game's chrome - titles, timers, lists, "
                          "instructions - is never part of a preset.");
    }
    for (const auto* scene : g_scenes) {
        if (status.id != std::string(scene->id)) continue;
        DrawOptions(*scene, status);
    }
    DrawCountdown(status);
    DrawSpriteFrames();
}

}
