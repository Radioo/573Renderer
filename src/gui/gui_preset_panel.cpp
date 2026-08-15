#include "gui_preset_panel.h"

#include "game_fingerprint.h"
#include "imgui.h"
#include "gc2d/gc_host.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_registry.h"
#include "preset/preset_host.h"
#include "state/app_state.h"

#include <cfloat>
#include <memory>
#include <string>
#include <vector>

namespace Panels::PresetPanel {

namespace {

std::string g_scanned_dir;
std::string g_build_name;
std::string g_build_id;
Preset::Doc::Registry g_registry;
std::vector<const Preset::Doc::Entry*> g_screens;

void RescanIfNeeded() {
    const std::string dir = App::Global().GameDir();
    if (dir == g_scanned_dir) return;
    g_scanned_dir = dir;
    g_build_name.clear();
    g_build_id.clear();
    g_screens.clear();
    const GameFingerprint::Match match = GameFingerprint::Identify(dir);
    if (match.build == nullptr) return;
    g_build_name = match.build->name;
    g_build_id = match.build->id;
    App::State& state = App::Global();
    state.BeginLoad("Scene presets");
    g_registry.Load(Preset::Doc::UserRoot(), [&state](const Preset::Doc::ScanStatus& status) {
        const float done = (status.total > 0) ? (float)status.done / (float)status.total : 1.0F;
        state.UpdateLoadStage(status.current, done);
    });
    state.EndLoad();
    g_screens = g_registry.ForBuild(g_build_id);
}

void DrawSpriteFrames() {
    const std::vector<Gc2dHost::SpriteStatus> sprites = Gc2dHost::ListSprites();
    if (sprites.empty()) return;

    ImGui::Separator();
    ImGui::TextDisabled("2D layers");
    for (const Gc2dHost::SpriteStatus& sprite : sprites) {
        if (sprite.scroll_wrap > 0) {
            ImGui::TextDisabled("%s: scrolled %d / %d px", sprite.name.c_str(), sprite.scroll,
                                sprite.scroll_wrap);
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
        ImGui::TextDisabled("%s: frame %d / %d (playhead %d)", sprite.name.c_str(), sprite.frame,
                            sprite.length - 1, sprite.playhead);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Each 2D layer runs on its own playhead, the way the game gives\n"
                          "every registered animation its own frame counter. The preset\n"
                          "timeline owns those clocks now, so they are shown, not dragged;\n"
                          "move the screen's playhead instead.");
    }
}

void DrawTimeline(const PresetHost::Status& status) {
    ImGui::Text("frame %d / %d at %d fps%s", status.frame, status.length, status.fps,
                status.playing ? "" : " (paused)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The document's frame axis. Playback delivers one document frame per "
                          "1/fps second of wall time no matter what the display runs at.");
    }
    ImGui::Text("beat %d (+%d), pulse %.3f, jitter %+.4f, %d particle(s)", status.beat,
                status.beat_since, status.pulse_scale, status.jitter, status.live_particles);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "The beat index the screen derives from its frame counter, the frames since that "
            "index last changed, the scale the beat pulse is applying right now, the position "
            "offset it drew this frame, and how many particles are still alive.");
    }
    if (status.problems <= 0) return;
    ImGui::TextDisabled("%d validation error(s) - those clips are skipped", status.problems);
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
    return !g_screens.empty();
}

void Render() {
    RescanIfNeeded();

    if (g_build_name.empty()) {
        ImGui::TextDisabled("No known build fingerprint under this directory.");
        return;
    }
    ImGui::TextDisabled("%s", g_build_name.c_str());
    ImGui::Separator();

    if (g_screens.empty()) {
        ImGui::TextDisabled("No scene presets are registered for this build.");
        return;
    }

    const PresetHost::Status status = PresetHost::GetStatus();
    for (const Preset::Doc::Entry* entry : g_screens) {
        const std::string& name = entry->document.name;
        const bool active = (status.id == entry->document.id);
        if (ImGui::RadioButton(name.c_str(), active) && !active) {
            App::State& state = App::Global();
            state.BeginLoad(name);
            auto document = std::make_shared<const Preset::Doc::Document>(entry->document);
            PresetHost::LoadDocument(g_scanned_dir, document,
                                     [&state](const std::string& stage, float done) {
                                         state.UpdateLoadStage(stage, done);
                                     });
            state.EndLoad();
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
    DrawTimeline(status);
    DrawCountdown(status);
    DrawSpriteFrames();
}

}
