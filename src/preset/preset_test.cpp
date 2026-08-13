#include "preset/preset_test.h"

#include "app_globals.h"
#include "backend/backend.h"
#include "export.h"
#include "game_fingerprint.h"
#include "game_profile.h"
#include "media/media_format.h"
#include "preset/preset_host.h"
#include "preset/scene_preset.h"
#include "render/stretch.h"
#include "render_backend.h"
#include "scene3d/scene3d_host.h"
#include "settings/settings.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/telemetry.h"
#include "support/crash_report.h"
#include "support/log.h"
#include "window.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace PresetTest {

namespace {

constexpr float kFrameSeconds = 1.0F / 60.0F;

const Preset::Scene* Choose(const std::string& game_dir, const std::string& preset_id) {
    const GameFingerprint::Match match = GameFingerprint::Identify(game_dir);
    if (match.build == nullptr) {
        LOG("PresetTest", "no known build fingerprint under %s", game_dir.c_str());
        return nullptr;
    }
    LOG("PresetTest", "identified %s via %s", match.build->name, match.file.c_str());

    const std::vector<const Preset::Scene*> scenes = Preset::ForBuild(match.build->id);
    if (scenes.empty()) {
        LOG("PresetTest", "no scene presets registered for %s", match.build->id);
        return nullptr;
    }
    if (preset_id.empty()) return scenes.front();
    for (const auto* scene : scenes) {
        if (scene->id == preset_id) return scene;
    }
    LOG("PresetTest", "build %s has no preset '%s'", match.build->id, preset_id.c_str());
    return nullptr;
}

int Prepare(const std::string& game_dir, const std::string& preset_id) {
    Support::InstallCrashReporter();
    const Preset::Scene* scene = Choose(game_dir, preset_id);
    if (scene == nullptr) return 2;

    const Settings::Config cfg = Settings::Load();
    const Stretch::Size present =
        Stretch::Present(scene->render_w, scene->render_h, cfg.stretch_16_9);
    HWND hwnd = AppWindow::Create(present.w, present.h);
    if (hwnd == nullptr) {
        LOG("PresetTest", "could not create the render window");
        return 3;
    }
    g_d3d.width = scene->render_w;
    g_d3d.height = scene->render_h;
    g_d3d.present_width = present.w;
    g_d3d.present_height = present.h;
    g_d3d.stretch_filter = (Stretch::Filter)std::clamp(cfg.stretch_filter, 0, 3);
    if (present.w != scene->render_w) {
        LOG("PresetTest", "presenting %dx%d stretched to %dx%d (%s)", scene->render_w,
            scene->render_h, present.w, present.h, Stretch::FilterName(g_d3d.stretch_filter));
    }
    if (!g_d3d.Init(hwnd)) {
        LOG("PresetTest", "D3D9 init failed");
        return 4;
    }

    if (!PresetHost::Load(game_dir, *scene)) return 5;
    return 0;
}

MediaSink::Format FormatFromPath(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return MediaSink::Format::AVIF;
    const std::string ext = path.substr(dot);
    for (int i = 0; i < MediaSink::kFormatCount; i++) {
        const MediaSink::Format f = MediaSink::FromIndex(i);
        if (ext == MediaSink::FormatExtension(f)) return f;
    }
    return MediaSink::Format::AVIF;
}

}

int Run(const std::string& game_dir, const std::string& preset_id, const std::string& out_png,
        int frames, int option) {
    const int rc = Prepare(game_dir, preset_id);
    if (rc != 0) return rc;
    PresetHost::SetOption(0, option);

    frames = (frames > 0) ? frames : 1;
    for (int i = 0; i < frames; i++) {
        AppWindow::PumpMessages();
        g_d3d.BeginFrame();
        PresetHost::RenderFrame(kFrameSeconds);
        g_d3d.EndFrame();
    }

    const PresetHost::Status status = PresetHost::GetStatus();
    float model_time = 0.0F;
    for (const auto& model : Scene3dHost::ListModels()) {
        if (model.visible) model_time = model.time;
    }
    LOG("PresetTest",
        "%d frames: countdown %d/%d, model speed %.3f, alpha %.3f, blend %d, model t=%.1f ticks",
        frames, status.countdown, status.countdown_start, status.model_speed, status.model_alpha,
        status.blend_mode, model_time);
    g_d3d.SaveBackBufferToFile(out_png.c_str());
    PresetHost::Unload();
    LOG("PresetTest", "done -> %s", out_png.c_str());
    return 0;
}

int RunExport(const std::string& game_dir, const std::string& preset_id,
              const std::string& out_path, int frames, int option) {
    const int rc = Prepare(game_dir, preset_id);
    if (rc != 0) return rc;
    PresetHost::SetOption(0, option);

    const GameProfile::Profile* profile = GameProfile::BySlug("iidx11");
    if (profile == nullptr || !Backend::CreateActive(*profile)) {
        LOG("PresetTest", "could not create the scene3d backend for the export driver");
        return 6;
    }

    App::ExportRequest req;
    req.output_path = out_path;
    req.fps = 60;
    req.quality = 60;
    req.max_frames = frames;
    req.bg_transparent = false;
    req.format = MediaSink::ToIndex(FormatFromPath(out_path));
    Export::HandleStartRequest(req, g_d3d);

    int guard = 0;
    const int guard_cap = (frames > 0 ? frames : PresetHost::NaturalFrames()) + 240;
    while (Export::IsCapturing() && guard++ < guard_cap) {
        AppWindow::PumpMessages();
        g_d3d.BeginFrame();
        PresetHost::RenderFrame(kFrameSeconds);
        Export::OnMainLoopTick(g_d3d);
        g_d3d.EndFrame();
    }

    const App::ExportState st = App::Global().GetExport();
    PresetHost::Unload();
    if (st.phase != App::ExportPhase::Done) {
        LOG("PresetTest", "export did not finish: %s", st.error.c_str());
        return 7;
    }
    LOG("PresetTest", "export done -> %s (%d frames)", st.output_path.c_str(), st.frames_captured);
    return 0;
}

}
