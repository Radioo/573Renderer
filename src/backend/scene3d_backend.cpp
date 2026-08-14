#include "backend/scene3d_backend.h"

#include "backend/backend.h"
#include "cli/cli.h"
#include "export.h"
#include "export_capture.h"
#include "export_internal.h"
#include "game_revision.h"
#include "gc2d/gc_host.h"
#include "gc2d/gc_package.h"
#include "loop/cli_autopilot.h"
#include "preset/preset_host.h"
#include "render_backend.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_host.h"
#include "state/app_state.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <any>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <cstdint>
#include <vector>

namespace Backend {

namespace {

class Scene3dCaptureDriver final : public Export::ICaptureDriver {
public:
    void BeginCapture(Export::Session& sess) override {
        planned_frames_ = 0;
        int preset_frames = 0;
        int package_frames = 0;
        if (PresetHost::Active()) {
            PresetHost::Restart();
            preset_frames = PresetHost::NaturalFrames();
        } else if (Gc2dHost::Active()) {
            Gc2dHost::SetFrame(0);
            package_frames = Gc2dHost::GetStatus().length;
        } else {
            Export::FailSession(sess, "Load a screen preset or a 2D package first - the scene "
                                      "browser has no timeline the exporter can bound on.");
            return;
        }

        planned_frames_ = Export::PlannedFrames(sess.max_frames, preset_frames, package_frames);
        if (planned_frames_ <= 0) {
            Export::FailSession(sess, "This screen has no countdown and no looping animation - "
                                      "turn on 'Limit frames' to set a length.");
            return;
        }
        LOG("Export", "scene export: %d frames (%s)", planned_frames_,
            (sess.max_frames > 0) ? "frame limit"
            : (preset_frames > 0) ? "one full screen timeline"
                                  : "one full animation loop");
    }

    void TickCapture(Export::Session& sess, D3D9State& d3d) override {
        static std::vector<uint8_t> bgra;
        int w = 0;
        int h = 0;
        if (!d3d.ReadPresentBGRA(bgra, w, h)) {
            Export::FailSession(sess, "D3D9 presented-frame readback failed");
            return;
        }
        Export::SubmitOneFrame(sess, bgra.data(), w, h);
        if (!sess.active) return;
        if (sess.frames_captured >= planned_frames_) {
            Export::FinishAndEncode(sess);
            return;
        }
        Export::PublishCapturing(sess);
    }

    void EndCapture(Export::Session& sess) override { (void)sess; }

    [[nodiscard]] Export::Capabilities Caps() const override {
        return Export::Capabilities{
            .loop_count = false,
            .blend_seam = false,
            .transparent_bg = false,
            .natural_end = "one full run of the screen's timeline, or of the animation's loop"};
    }

private:
    int planned_frames_ = 0;
};

void ScanScenes(const std::string& game_dir) noexcept {
    auto& st = App::Global();
    try {
        std::vector<App::State::IfsEntry> out;
        std::error_code ec;
        const std::filesystem::path root(game_dir);
        for (const auto& e : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec) break;
            if (!e.is_directory(ec)) continue;
            const std::string dir = e.path().string();
            const bool is_scene = Scene3d::IsSceneDir(dir);
            const bool is_gc = Gc2d::IsPackageDir(dir);
            if (!is_scene && !is_gc) continue;
            App::State::IfsEntry entry;
            entry.full_path = dir;
            entry.name = std::filesystem::relative(e.path(), root, ec).string() +
                         (is_scene ? "  [3D scene]" : "  [2D package]");
            out.push_back(std::move(entry));
            st.SetIfsScanStatus("Found " + std::to_string(out.size()) + " packages...");
        }
        LOG("Boot", "Found %zu renderable packages under %s", out.size(), game_dir.c_str());
        st.SetAvailableIfs(std::move(out));
    } catch (...) {
        LOG("Boot", "3D scene scan thread: unexpected exception");
    }
    st.SetIfsScanStatus("");
    st.SetIfsScanning(false);
}

class Scene3dBackend final : public IBackend {
public:
    [[nodiscard]] const char* Id() const override { return "scene3d"; }

    bool Boot(const BootEnv& env) override {
        game_dir_ = env.game_dir;
        cli_ = env.cli;
        const std::string rev = GameRevision::LatestRevisionDir(game_dir_);
        if (!rev.empty()) LOG("Boot", "scene3d backend: active revision %s", rev.c_str());
        LOG("Boot", "scene3d backend ready (no engine DLLs are needed for model scenes)");
        return true;
    }

    void Shutdown() override {
        PresetHost::Unload();
        Scene3dHost::Unload();
        Gc2dHost::Unload();
    }

    [[nodiscard]] bool ContentReady() const override { return true; }

    void StartContentScan() override {
        auto& state = App::Global();
        state.SetIfsScanning(true);
        state.SetIfsScanStatus("Scanning for 3D model scenes...");
        std::thread(ScanScenes, game_dir_).detach();
    }

    bool LoadContent(const std::string& path, bool from_arc) override {
        (void)from_arc;
        auto& state = App::Global();
        PresetHost::Unload();
        state.BeginLoad(path);
        bool ok = false;
        if (Scene3d::IsSceneDir(path)) {
            Gc2dHost::Unload();
            ok = Scene3dHost::Load(path);
        } else if (Gc2d::IsPackageDir(path)) {
            Scene3dHost::Unload();
            ok = Gc2dHost::Load(path);
            if (ok && cli_ != nullptr && !cli_->animation_name.empty())
                Gc2dHost::SelectAnimation(cli_->animation_name);
        }
        state.EndLoad();
        return ok;
    }

    void UnloadContent() override {
        PresetHost::Unload();
        Scene3dHost::Unload();
        Gc2dHost::Unload();
    }

    void AdvanceFrame(float dt, int frame_count, bool exporting) override {
        (void)dt;
        (void)frame_count;
        (void)exporting;
        const bool live = Scene3dHost::Active() || Gc2dHost::Active();
        std::string playing;
        if (PresetHost::Active()) {
            playing = PresetHost::GetStatus().id;
        } else if (Gc2dHost::Active()) {
            playing = Gc2dHost::GetStatus().animation;
        }
        App::Status st = App::Global().GetStatus();
        if (st.scene_loaded != live || st.playing_animation != playing) {
            st.scene_loaded = live;
            st.playing_animation = playing;
            App::Global().SetStatus(st);
        }
    }

    void RenderScene(float dt, int frame_count) override {
        (void)frame_count;
        if (PresetHost::Active()) {
            PresetHost::RenderFrame(dt);
            return;
        }
        Scene3dHost::RenderFrame(dt);
        Gc2dHost::RenderFrame(dt);
    }

    void FillAutopilotInputs(Loop::AutopilotInputs& in) override {
        const bool live = Scene3dHost::Active() || Gc2dHost::Active();
        in.clip_live = live;
        in.scene_renderable = live;
        const bool wanted = (cli_ != nullptr) && !cli_->animation_name.empty();
        in.anim_name_matches = !wanted || Gc2dHost::GetStatus().animation == cli_->animation_name;
        in.active_clip_matches = in.anim_name_matches;
    }

    void BindSubmonitor() override {}

    bool HandleCommand(const std::any& payload) override {
        (void)payload;
        return false;
    }

    Export::ICaptureDriver& ExportDriver() override { return capture_; }

private:
    std::string game_dir_;
    const Cli::Options* cli_ = nullptr;
    Scene3dCaptureDriver capture_;
};

}

std::unique_ptr<IBackend> MakeScene3dBackend() {
    return std::make_unique<Scene3dBackend>();
}

}
