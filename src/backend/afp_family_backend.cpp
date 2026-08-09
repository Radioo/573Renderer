#include "backend/afp_family_backend.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_host.h"

#include "afp_boot.h"
#include "app_globals.h"
#include "gpu_context.h"
#include "avs_boot.h"
#include "avs_funcs.h"
#include "backend/afp_profiles.h"
#include "backend/backend.h"
#include "cli/cli.h"
#include "formats/ddr_arc.h"
#include "game_profile.h"
#include "game_runtime.h"
#include "qpro/qpro_extract.h"
#include "qpro/qpro_scan.h"
#include "render_live.h"
#include "backend/afp_commands.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <algorithm>
#include <any>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>
#include <windows.h>

namespace Backend {

namespace {

bool FailBoot(std::string msg) {
    auto& state = App::Global();
    state.EndLoad();
    state.SetBootError(std::move(msg));
    state.SetBootState(App::BootState::Failed);
    return false;
}

void PublishSetupStage(const char* stage) {
    App::Global().UpdateLoadStage(stage);
}

std::string DiscoverDllDir(const std::string& game_dir, const AfpProfiles::AfpConfig& p) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (game_dir.empty()) return {};

    fs::path const root = fs::path(game_dir);
    const fs::path candidates[] = {
        root / "modules",
        root / "contents" / "modules",
        root,
    };
    const char* required[] = {p.avs_dll, p.afp_dll, p.afpu_dll};
    for (const auto& c : candidates) {
        bool all_present = true;
        for (const char* name : required) {
            if (name == nullptr) continue;
            if (!fs::exists(c / name, ec)) {
                all_present = false;
                break;
            }
        }
        if (all_present) {
            return c.string() + "\\";
        }
    }
    return {};
}

bool LoadAllDlls(const std::string& dll_dir, const AfpProfiles::AfpConfig& p, bool legacy_afp) {
    LOG("Init", "Loading DLLs from: %s (avs=%s afp=%s afpu=%s)", dll_dir.c_str(), p.avs_dll,
        p.afp_dll, (p.afpu_dll != nullptr) ? p.afpu_dll : "(none)");
    {
        std::string d = dll_dir;
        if (!d.empty() && (d.back() == '\\' || d.back() == '/')) d.pop_back();
        SetDllDirectoryA(d.c_str());
    }
    if (!g_avs_dll.Load((dll_dir + p.avs_dll).c_str())) return false;
    if (!g_afp_dll.Load((dll_dir + p.afp_dll).c_str())) return false;
    if (p.afpu_dll == nullptr) {
        LOG("Init", "Profile ships no afp-utils DLL (pre-afputils AFP generation)");
    } else if (!g_afpu_dll.Load((dll_dir + p.afpu_dll).c_str())) {
        return false;
    }
    const AvsOrdinals* avs_ord = &kAvsOrdinals217;
    const char* avs_ord_name = "avs 2.16.3/2.17";
    if (p.avs_generation == AfpProfiles::AvsGeneration::Avs2161) {
        avs_ord = &kAvsOrdinals2161;
        avs_ord_name = "avs 2.16.1";
    } else if (p.avs_generation == AfpProfiles::AvsGeneration::Avs2158) {
        avs_ord = &kAvsOrdinals2158;
        avs_ord_name = "avs 2.15.8";
    } else if (p.avs_generation == AfpProfiles::AvsGeneration::Avs2134) {
        avs_ord = &kAvsOrdinals2134;
        avs_ord_name = "avs 2.13.4";
    }
    LOG("Init", "AVS ordinal map: %s", avs_ord_name);
    if (!g_avs.Load(g_avs_dll, *avs_ord)) {
        LOG("Init", "FAILED to resolve AVS functions");
        return false;
    }
    if (legacy_afp) {
        LOG("Init", "Legacy AFP 2.13.7 (DDR) profile: DLLs loaded; afp/afpu "
                    "func resolve deferred to DdrAfp::Boot");
        return true;
    }
    if (!g_afp.Load(g_afp_dll)) {
        LOG("Init", "FAILED to resolve AFP functions");
        return false;
    }
    if (!g_afpu.Load(g_afpu_dll)) {
        LOG("Init", "FAILED to resolve AFPU functions");
        return false;
    }
    return true;
}

using ScanProgressFn =
    std::function<void(size_t scanned, size_t found, const std::string& cur_dir)>;

bool HasExt(const std::string& ext, const char* want3) {
    if (ext.size() != 4 || ext[0] != '.') return false;
    for (int i = 0; i < 3; i++) {
        char const c = ext[i + 1];
        char const lc = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        if (lc != want3[i]) return false;
    }
    return true;
}

void AppendArcIfsEntry(const std::filesystem::path& p, std::vector<App::State::IfsEntry>& out) {
    const auto toc = DdrArc::ReadToc(p.string());
    if (!toc || !toc->HasIfs()) return;
    for (const auto& en : toc->entries) {
        const std::string& nm = en.name;
        if (nm.size() <= 4 || !nm.ends_with(".ifs")) continue;
        App::State::IfsEntry e;
        e.name = nm;
        e.full_path = p.string();
        e.from_arc = true;
        out.push_back(std::move(e));
        break;
    }
}

struct TopDirTracker {
    std::string top0;
    std::string top1;

    void Enter(int depth, const std::filesystem::path& p) {
        if (depth == 0) {
            top0 = p.filename().string();
            top1.clear();
        } else {
            top1 = p.filename().string();
        }
    }

    [[nodiscard]] std::string Current() const { return top1.empty() ? top0 : (top0 + "/" + top1); }
};

struct ScanProgressThrottle {
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    bool first = true;

    void Tick(const ScanProgressFn& fn, size_t scanned, size_t found, const std::string& dir) {
        if (!fn) return;
        auto const now = std::chrono::steady_clock::now();
        if (first || now - last >= std::chrono::milliseconds(100)) {
            fn(scanned, found, dir);
            last = now;
            first = false;
        }
    }

    static void Final(const ScanProgressFn& fn, size_t scanned, size_t found,
                      const std::string& dir) {
        if (fn) fn(scanned, found, dir);
    }
};

std::vector<App::State::IfsEntry> ScanGameDir(const std::string& game_dir,
                                              const ScanProgressFn& on_progress, bool scan_arcs,
                                              bool scan_txp2) {
    std::vector<App::State::IfsEntry> out;
    namespace fs = std::filesystem;
    std::error_code ec;

    if (game_dir.empty() || !fs::exists(game_dir, ec)) return out;

    fs::path const root = fs::absolute(fs::path(game_dir), ec);

    size_t scanned = 0;
    TopDirTracker top;
    ScanProgressThrottle progress;

    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        ++scanned;
        const fs::path& p = it->path();

        if (it.depth() <= 1 && it->is_directory(ec)) top.Enter(it.depth(), p);

        if (it->is_regular_file(ec)) {
            std::string const ext = p.extension().string();
            if (HasExt(ext, "ifs")) {
                App::State::IfsEntry e;
                e.name = fs::relative(p, root, ec).string();
                e.full_path = p.string();
                out.push_back(std::move(e));
            }
            if (scan_arcs && HasExt(ext, "arc")) AppendArcIfsEntry(p, out);
            if (scan_txp2 && HasExt(ext, "bin")) {
                App::State::IfsEntry e;
                e.name = fs::relative(p, root, ec).string();
                e.full_path = p.string();
                out.push_back(std::move(e));
            }
        }

        progress.Tick(on_progress, scanned, out.size(), top.Current());
    }
    ScanProgressThrottle::Final(on_progress, scanned, out.size(), top.Current());

    std::ranges::sort(out, [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

void AppendSceneDirs(const std::string& game_dir, std::vector<App::State::IfsEntry>& out) {
    std::error_code ec;
    const std::filesystem::path root(game_dir);
    if (!std::filesystem::is_directory(root, ec)) return;
    size_t found = 0;
    for (const auto& e : std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (!e.is_directory(ec)) continue;
        const std::string dir = e.path().string();
        if (!Scene3d::IsSceneDir(dir)) continue;
        App::State::IfsEntry entry;
        entry.full_path = dir;
        entry.name = std::filesystem::relative(e.path(), root, ec).string() + "  [3D scene]";
        out.push_back(std::move(entry));
        found++;
    }
    if (found != 0) LOG("Boot", "Found %zu 3D model scenes", found);
}

void ScanThreadBody(const std::string& game_dir, bool scan_arcs, bool scan_txp2) noexcept {
    auto& st = App::Global();
    try {
        auto ifs_list = ScanGameDir(
            game_dir,
            [&st](size_t scanned, size_t found, const std::string& cur_dir) {
                char s[160];
                if (!cur_dir.empty()) {
                    snprintf(s, sizeof(s), "Scanning %s  -  %zu files, %zu .ifs found",
                             cur_dir.c_str(), scanned, found);
                } else {
                    snprintf(s, sizeof(s), "Scanning  -  %zu files, %zu .ifs found", scanned,
                             found);
                }
                st.SetIfsScanStatus(s);
            },
            scan_arcs, scan_txp2);
        AppendSceneDirs(game_dir, ifs_list);
        LOG("Boot", "Found %zu IFS files under %s", ifs_list.size(), game_dir.c_str());
        st.SetAvailableIfs(std::move(ifs_list));
    } catch (...) {
        LOG("Boot", "IFS scan thread: unexpected exception");
    }
    st.SetIfsScanStatus("");
    st.SetIfsScanning(false);
}

std::string WriteTempIfs(const std::string& inner_name, const std::vector<uint8_t>& bytes) {
    namespace fs = std::filesystem;
    char tmp[MAX_PATH];
    DWORD const n = GetTempPathA(MAX_PATH, tmp);
    if (n == 0 || n > MAX_PATH) return {};
    std::string const dir_name = "573renderer_ifs_" + std::to_string(GetCurrentProcessId());
    fs::path const dir = fs::path(std::string(tmp, n)) / dir_name;
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::string base = inner_name;
    size_t const slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    if (base.empty()) base = "arc.ifs";
    fs::path const out = dir / base;
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    if (!o) return {};
    if (!bytes.empty())
        o.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    return out.string();
}

void HandleQproExtract(const AfpCmd::QproStartExtract& cmd) {
    QproExtract::Options o;
    o.game_dir = App::Global().GameDir();
    o.out_dir = cmd.out_dir;
    o.fps = cmd.fps;
    o.parts = cmd.parts;
    o.part_sel = cmd.part_sel;
    QproExtract::SetHueScopeEnabled(cmd.hue_scope);
    QproExtract::Run(g_engine, g_gpu.d3d, o);
}

void HandleGotoLabel(const AfpCmd::GotoLabel& cmd) {
    Runtime::Active().GotoLabel(g_afp, cmd.name);
    App::Status st = App::Global().GetStatus();
    st.active_label = cmd.name;
    st.label_playback_active = !cmd.name.empty();
    App::Global().SetStatus(st);
}

struct AfpCommandVisitor {
    void operator()(const AfpCmd::SwitchAnimation& cmd) const {
        if (!cmd.name.empty()) Runtime::Active().SwitchAnimation(cmd.name, cmd.label);
    }
    void operator()(const AfpCmd::GotoLabel& cmd) const { HandleGotoLabel(cmd); }
    void operator()(const AfpCmd::SeekFrame& cmd) const {
        RenderLive::HandleSeekRequest(cmd.frame, g_afp);
    }
    void operator()(const AfpCmd::SetPaused& cmd) const {
        RenderLive::HandlePauseRequest(cmd.paused, g_afp);
    }
    void operator()([[maybe_unused]] const AfpCmd::ForceReplay& cmd) const {
        Runtime::Active().ForceReplayMaster();
    }
    void operator()([[maybe_unused]] const AfpCmd::QproStartScan& cmd) const {
        QproExtract::RunScan(App::Global().GameDir());
    }
    void operator()(const AfpCmd::QproStartExtract& cmd) const { HandleQproExtract(cmd); }
};

}

bool AfpFamilyBackend::Boot(const BootEnv& env) {
    profile_ = env.profile;
    cfg_ = AfpProfiles::For(profile_->slug);
    if (cfg_ == nullptr) {
        return FailBoot(std::string("No AFP engine config for profile '") + profile_->slug +
                        "' (backend " + Id() + ").");
    }
    cli_ = env.cli;
    game_dir_ = env.game_dir;
    if (cli_ != nullptr) {
        sm_dissolve_ = Loop::DissolveCycler(cli_->submonitor_loop_frames);
        sm_fade_ = Loop::FadeCycler(cli_->submonitor_fade_frames, cli_->submonitor_dwell_frames);
    }
    AfpManager::SetActiveConfig(cfg_);

    PublishSetupStage("Locating game DLLs");
    std::string const dll_dir = DiscoverDllDir(env.game_dir, *cfg_);
    if (dll_dir.empty()) {
        return FailBoot("Couldn't find avs2-core.dll / afp-core.dll / afp-utils.dll "
                        "under the selected directory. Expected them in `modules/`.");
    }
    LOG("Boot", "DLLs found in: %s", dll_dir.c_str());

    PublishSetupStage("Loading DLLs");
    if (!LoadAllDlls(dll_dir, *cfg_, Runtime::Active().IsLegacyDdr())) {
        return FailBoot("Failed to load one or more DLLs from " + dll_dir +
                        ". The game may be a different version than expected.");
    }

    PublishSetupStage("Booting AVS");
    if (!AvsManager::Boot(g_avs)) {
        return FailBoot("AVS boot failed (avs2-core.dll). Check the log for details.");
    }

    return BootEngine(env);
}

void AfpFamilyBackend::Shutdown() {
    Runtime::Active().Shutdown();
    AvsManager::Shutdown(g_avs);
}

bool AfpFamilyBackend::ContentReady() const {
    return Runtime::Active().IsBooted();
}

void AfpFamilyBackend::StartContentScan() {
    auto& state = App::Global();
    bool const scan_arcs = cfg_->scan_arc_containers;
    bool const scan_txp2 = cfg_->scan_txp2_packages;
    state.SetIfsScanning(true);
    state.SetIfsScanStatus(scan_txp2 ? "Scanning for TXP2 packages..."
                                     : "Scanning for IFS files...");
    std::thread(ScanThreadBody, game_dir_, scan_arcs, scan_txp2).detach();
}

bool AfpFamilyBackend::LoadContent(const std::string& path, bool from_arc) {
    auto& state = App::Global();
    state.BeginLoad(path);

    if (Scene3d::IsSceneDir(path)) {
        state.UpdateLoadStage("Loading 3D scene");
        const bool ok = Scene3dHost::Load(path);
        state.EndLoad();
        return ok;
    }
    Scene3dHost::Unload();

    std::string mount_path = path;
    if (from_arc) {
        state.UpdateLoadStage("Decompressing .arc");
        std::string inner_name;
        const auto ifs_bytes = DdrArc::ExtractFirstIfs(path, inner_name);
        if (!ifs_bytes || ifs_bytes->empty()) {
            LOG("Init", "arc extract failed: %s",
                !ifs_bytes ? ifs_bytes.error().c_str() : "empty .ifs entry");
            state.EndLoad();
            return false;
        }
        mount_path = WriteTempIfs(inner_name, *ifs_bytes);
        if (mount_path.empty()) {
            LOG("Init", "failed to stage temp .ifs from %s", path.c_str());
            state.EndLoad();
            return false;
        }
        LOG("Init", "arc %s -> %s (%zu-byte inner .ifs)", path.c_str(), mount_path.c_str(),
            ifs_bytes->size());
    }

    return Runtime::Active().LoadScene(mount_path, path);
}

void AfpFamilyBackend::UnloadContent() {
    Runtime::Active().UnloadScene();
}

bool AfpFamilyBackend::HandleCommand(const std::any& payload) {
    const auto* afp = std::any_cast<AfpCmd::Any>(&payload);
    if (afp == nullptr) {
        LOG("Main", "BackendCommand with unknown payload type dropped");
        return false;
    }
    std::visit(AfpCommandVisitor{}, *afp);
    return true;
}

}
