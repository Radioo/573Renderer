#include "backend/scene3d_backend.h"

#include "backend/backend.h"
#include "cli/cli.h"
#include "export_capture.h"
#include "game_revision.h"
#include "gc2d/gc_host.h"
#include "gc2d/gc_package.h"
#include "loop/cli_autopilot.h"
#include "render_backend.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_host.h"
#include "state/app_state.h"
#include "support/log.h"

#include <any>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace Backend {

namespace {

class Scene3dCaptureDriver final : public Export::ICaptureDriver {
public:
    void BeginCapture(Export::Session& sess) override { (void)sess; }
    void TickCapture(Export::Session& sess, D3D9State& d3d) override {
        (void)sess;
        (void)d3d;
    }
    void EndCapture(Export::Session& sess) override { (void)sess; }
};

void ScanScenes(const std::string& game_dir) noexcept {
    try {
        auto& st = App::Global();
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
        st.SetIfsScanStatus("");
        st.SetIfsScanning(false);
    } catch (...) {
        LOG("Boot", "3D scene scan thread: unexpected exception");
    }
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
        Scene3dHost::Unload();
        Gc2dHost::Unload();
    }

    void AdvanceFrame(float dt, int frame_count, bool exporting) override {
        (void)dt;
        (void)frame_count;
        (void)exporting;
    }

    void RenderScene(float dt, int frame_count) override {
        (void)frame_count;
        Scene3dHost::RenderFrame(dt);
        Gc2dHost::RenderFrame(dt);
    }

    void FillAutopilotInputs(Loop::AutopilotInputs& in) override {
        const bool live = Scene3dHost::Active() || Gc2dHost::Active();
        in.clip_live = live;
        in.scene_renderable = live;
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
