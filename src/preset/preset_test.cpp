#include "preset/preset_test.h"

#include "app_globals.h"
#include "backend/backend.h"
#include "export.h"
#include "game_fingerprint.h"
#include "gc2d/gc_host.h"
#include "game_profile.h"
#include "media/media_format.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_registry.h"
#include "preset/doc/preset_validate.h"
#include "preset/preset_host.h"
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
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace PresetTest {

namespace {

namespace Doc = Preset::Doc;

constexpr float kFrameSeconds = 1.0F / 60.0F;

std::shared_ptr<const Doc::Document> FromFile(const std::string& path, bool force) {
    const Doc::Loaded loaded = Doc::LoadFile(path);
    if (!loaded.has_value()) {
        LOG("PresetTest", "%s: %s (line %d, column %d)", path.c_str(),
            loaded.error().message.c_str(), loaded.error().line, loaded.error().column);
        return nullptr;
    }
    int errors = 0;
    for (const Doc::Problem& problem : Doc::Validate(*loaded)) {
        const bool fatal = problem.severity == Doc::Severity::Error;
        errors += fatal ? 1 : 0;
        LOG("PresetTest", "  %s %s: %s", fatal ? "error" : "warning", problem.path.c_str(),
            problem.message.c_str());
    }
    if (errors > 0 && !force) {
        LOG("PresetTest", "%s has %d validation error(s); pass --force to load it anyway",
            path.c_str(), errors);
        return nullptr;
    }
    return std::make_shared<const Doc::Document>(*loaded);
}

std::shared_ptr<const Doc::Document> FromRegistry(const std::string& game_dir,
                                                  const std::string& preset_id) {
    const GameFingerprint::Match match = GameFingerprint::Identify(game_dir);
    if (match.build == nullptr) {
        LOG("PresetTest", "no known build fingerprint under %s", game_dir.c_str());
        return nullptr;
    }
    LOG("PresetTest", "identified %s via %s", match.build->name, match.file.c_str());

    Doc::Registry registry;
    registry.Load(Doc::UserRoot(), [](const Doc::ScanStatus& status) {
        LOG("PresetTest", "scanning user presets %d/%d: %s", status.done, status.total,
            status.current.c_str());
    });
    for (const Doc::Entry* entry : registry.Problems()) {
        LOG("PresetTest", "%s is not usable:", entry->path.c_str());
        for (const Doc::Problem& problem : entry->problems)
            LOG("PresetTest", "  %s: %s", problem.path.c_str(), problem.message.c_str());
    }

    const std::vector<const Doc::Entry*> entries = registry.ForBuild(match.build->id);
    if (entries.empty()) {
        LOG("PresetTest", "no scene presets registered for %s", match.build->id);
        return nullptr;
    }
    const Doc::Entry* chosen = preset_id.empty() ? entries.front() : nullptr;
    for (const Doc::Entry* entry : entries) {
        if (entry->document.id == preset_id) chosen = entry;
    }
    if (chosen == nullptr) {
        LOG("PresetTest", "build %s has no preset '%s'", match.build->id, preset_id.c_str());
        return nullptr;
    }
    return std::make_shared<const Doc::Document>(chosen->document);
}

std::shared_ptr<const Doc::Document> ChooseDocument(const Job& job) {
    if (!job.json_path.empty()) return FromFile(job.json_path, job.force);
    return FromRegistry(job.game_dir, job.preset_id);
}

int IndexOf(const std::string& text) {
    int value = 0;
    const char* end = text.data() + text.size();
    const std::from_chars_result parsed = std::from_chars(text.data(), end, value);
    return (parsed.ec == std::errc() && parsed.ptr == end) ? value : -1;
}

bool ApplyOptions(const Doc::Document& document, const std::vector<std::string>& specs) {
    for (const std::string& spec : specs) {
        const std::size_t equals = spec.find('=');
        if (equals == std::string::npos) {
            LOG("PresetTest", "--preset-option wants <option-id>=<choice>, got '%s'", spec.c_str());
            return false;
        }
        const std::string id = spec.substr(0, equals);
        const std::string wanted = spec.substr(equals + 1);
        int option = -1;
        for (std::size_t i = 0; i < document.options.size(); i++) {
            if (document.options[i].id == id) option = (int)i;
        }
        if (option < 0) {
            LOG("PresetTest", "preset '%s' has no option '%s'", document.id.c_str(), id.c_str());
            return false;
        }
        const Doc::OptionSpec& spec_of = document.options[(std::size_t)option];
        int choice = IndexOf(wanted);
        for (std::size_t i = 0; i < spec_of.choices.size(); i++) {
            if (spec_of.choices[i].label == wanted) choice = (int)i;
        }
        if (choice < 0 || (std::size_t)choice >= spec_of.choices.size()) {
            LOG("PresetTest", "option '%s' has no choice '%s'", id.c_str(), wanted.c_str());
            return false;
        }
        PresetHost::SetOption(option, choice);
    }
    return true;
}

int Prepare(const Doc::Document& document) {
    Support::InstallCrashReporter();
    const Settings::Config cfg = Settings::Load();
    const int width = document.render.width;
    const int height = document.render.height;
    const Stretch::Size present = Stretch::Present(width, height, cfg.stretch_16_9);
    HWND hwnd = AppWindow::Create(present.w, present.h);
    if (hwnd == nullptr) {
        LOG("PresetTest", "could not create the render window");
        return 3;
    }
    g_d3d.width = width;
    g_d3d.height = height;
    g_d3d.present_width = present.w;
    g_d3d.present_height = present.h;
    g_d3d.stretch_filter = (Stretch::Filter)std::clamp(cfg.stretch_filter, 0, 3);
    if (present.w != width) {
        LOG("PresetTest", "presenting %dx%d stretched to %dx%d (%s)", width, height, present.w,
            present.h, Stretch::FilterName(g_d3d.stretch_filter));
    }
    if (!g_d3d.Init(hwnd)) {
        LOG("PresetTest", "D3D9 init failed");
        return 4;
    }
    return 0;
}

struct Pose {
    std::string name;
    float time = 0.0F;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
};

std::vector<Pose> CapturePoses() {
    std::vector<Pose> poses;
    for (const auto& model : Scene3dHost::ListModels()) {
        if (!model.visible) continue;
        poses.push_back(Pose{.name = model.name,
                             .time = model.time,
                             .position = model.position,
                             .rotation = model.rotation,
                             .scale = model.scale});
    }
    return poses;
}

bool Differs(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    for (size_t i = 0; i < a.size(); i++) {
        if (std::abs(a[i] - b[i]) > 1e-5F) return true;
    }
    return false;
}

int ReportMotion(const std::vector<Pose>& first, const std::vector<Pose>& last) {
    if (first.empty()) {
        LOG("PresetTest", "no visible 3D model in this preset");
        return 0;
    }
    int moving = 0;
    for (size_t i = 0; i < first.size() && i < last.size(); i++) {
        const Pose& a = first[i];
        const Pose& b = last[i];
        const bool animates = std::abs(a.time - b.time) > 1e-4F;
        const bool moves = Differs(a.position, b.position) || Differs(a.rotation, b.rotation) ||
                           Differs(a.scale, b.scale);
        if (moves || animates) moving++;
        LOG("PresetTest", "  model %-10s transform %s, own animation %s", a.name.c_str(),
            moves ? "moves" : "static (clip driven)", animates ? "advances" : "FROZEN");
    }
    if (moving > 0) return 0;
    LOG("PresetTest",
        "FAILED: nothing moves. No visible model's transform changes and no model's own "
        "animation advances over %zu frame(s). Either the screen's per-frame transform update "
        "was missed, or the model's anim speed is zero. Find the update.",
        last.size());
    return 8;
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

int Run(const Job& job) {
    const std::shared_ptr<const Doc::Document> document = ChooseDocument(job);
    if (document == nullptr) return 2;
    const int rc = Prepare(*document);
    if (rc != 0) return rc;
    if (!PresetHost::LoadDocument(job.game_dir, document)) return 5;
    if (!ApplyOptions(*document, job.options)) return 9;

    const int frames = (job.frames > 0) ? job.frames : 1;
    std::vector<Pose> first;
    for (int i = 0; i < frames; i++) {
        AppWindow::PumpMessages();
        g_d3d.BeginFrame();
        PresetHost::RenderFrame(kFrameSeconds);
        g_d3d.EndFrame();
        if (i == 0) first = CapturePoses();
    }
    const int motion = ReportMotion(first, CapturePoses());

    const PresetHost::Status status = PresetHost::GetStatus();
    float model_time = 0.0F;
    for (const auto& model : Scene3dHost::ListModels()) {
        if (model.visible) model_time = model.time;
    }
    LOG("PresetTest", "%d frames of '%s': model speed %.3f, alpha %.3f, blend %d, model t=%.1f",
        frames, document->id.c_str(), status.model_speed, status.model_alpha, status.blend_mode,
        model_time);
    LOG("PresetTest", "  frame %d / %d, beat %d (+%d), pulse %.4f, jitter %+.6f, %d particle(s)",
        status.frame, status.length, status.beat, status.beat_since, status.pulse_scale,
        status.jitter, status.live_particles);
    LOG("PresetTest", "  2D: %d draw node(s) from %d placed layer(s)",
        Gc2dHost::GetStatus().draw_nodes, (int)Gc2dHost::ListSprites().size());
    g_d3d.SaveBackBufferToFile(job.out_path.c_str());
    PresetHost::Unload();
    LOG("PresetTest", "done -> %s", job.out_path.c_str());
    return motion;
}

int RunExport(const Job& job) {
    const std::shared_ptr<const Doc::Document> document = ChooseDocument(job);
    if (document == nullptr) return 2;
    const int rc = Prepare(*document);
    if (rc != 0) return rc;
    if (!PresetHost::LoadDocument(job.game_dir, document)) return 5;
    if (!ApplyOptions(*document, job.options)) return 9;

    const char* slug = GameFingerprint::ProfileSlugFor(document->build);
    const GameProfile::Profile* profile =
        (slug == nullptr) ? nullptr : GameProfile::BySlug(std::string(slug));
    if (profile == nullptr || !Backend::CreateActive(*profile)) {
        LOG("PresetTest", "no export profile for build '%s'", document->build.c_str());
        return 6;
    }

    App::ExportRequest req;
    req.output_path = job.out_path;
    req.fps = document->fps;
    req.quality = 60;
    req.max_frames = job.frames;
    req.bg_transparent = job.bg_transparent;
    req.bg_r = job.bg_rgb[0];
    req.bg_g = job.bg_rgb[1];
    req.bg_b = job.bg_rgb[2];
    req.format = MediaSink::ToIndex(FormatFromPath(job.out_path));
    Export::HandleStartRequest(req, g_d3d);

    int guard = 0;
    const int guard_cap = (job.frames > 0 ? job.frames : PresetHost::NaturalFrames()) + 240;
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
