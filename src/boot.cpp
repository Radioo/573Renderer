#include "state/boot_lifecycle.h"
#include "support/log.h"
#include "window.h"
#include "support/dll_loader.h"
#include "avs_funcs.h"
#include "afp_funcs.h"
#include "afpu_funcs.h"
#include "avs_boot.h"
#include "afp_boot.h"
#include "render_backend.h"
#include "mc_control.h"
#include "state/app_state.h"
#include "cli/cli.h"
#include "settings/settings.h"
#include "app_globals.h"
#include <algorithm>
#include <string>
#include <cstddef>
#include <system_error>
#include <ios>
#include <cstdio>
#include <thread>
#include "boot.h"
#include "game_profile.h"
#include "formats/ddr_arc.h"
#include "afp_ddr.h"
#include "game_runtime.h"
#include <fstream>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <functional>
#include <utility>
#include <vector>
#include <winbase.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")

namespace {
bool LoadAllDlls(const std::string& dll_dir, const GameProfile::Profile& p) {
    LOG("Init", "Loading DLLs from: %s (avs=%s afp=%s afpu=%s)", dll_dir.c_str(), p.avs_dll,
        p.afp_dll, p.afpu_dll);
    {
        std::string d = dll_dir;
        if (!d.empty() && (d.back() == '\\' || d.back() == '/')) d.pop_back();
        SetDllDirectoryA(d.c_str());
    }
    if (!g_avs_dll.Load((dll_dir + p.avs_dll).c_str())) return false;
    if (!g_afp_dll.Load((dll_dir + p.afp_dll).c_str())) return false;
    if (!g_afpu_dll.Load((dll_dir + p.afpu_dll).c_str())) return false;
    if (!g_avs.Load(g_avs_dll)) {
        LOG("Init", "FAILED to resolve AVS functions");
        return false;
    }
    if (p.legacy_afp) {
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
}

using ScanProgressFn =
    std::function<void(size_t scanned, size_t found, const std::string& cur_dir)>;

namespace {
namespace {

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
    DdrArc::Toc toc;
    if (!DdrArc::ReadToc(p.string(), toc) || !toc.HasIfs()) return;
    for (const auto& en : toc.entries) {
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
                                              const ScanProgressFn& on_progress = nullptr,
                                              bool scan_arcs = false) {
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
        }

        progress.Tick(on_progress, scanned, out.size(), top.Current());
    }
    ScanProgressThrottle::Final(on_progress, scanned, out.size(), top.Current());

    std::ranges::sort(out, [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}
}

namespace {
std::string DiscoverDllDir(const std::string& game_dir, const GameProfile::Profile& p) {
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
}

namespace {
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
}

bool MountAndLoadIfs(const std::string& ifs_path, bool from_arc) {
    auto& state = App::Global();
    state.BeginLoad(ifs_path);

    std::string mount_path = ifs_path;
    if (from_arc) {
        state.UpdateLoadStage("Decompressing .arc");
        std::string inner_name;
        std::vector<uint8_t> const ifs_bytes = DdrArc::ExtractFirstIfs(ifs_path, inner_name);
        if (ifs_bytes.empty()) {
            LOG("Init", "arc has no .ifs / decompress failed: %s", ifs_path.c_str());
            state.EndLoad();
            return false;
        }
        mount_path = WriteTempIfs(inner_name, ifs_bytes);
        if (mount_path.empty()) {
            LOG("Init", "failed to stage temp .ifs from %s", ifs_path.c_str());
            state.EndLoad();
            return false;
        }
        LOG("Init", "arc %s -> %s (%zu-byte inner .ifs)", ifs_path.c_str(), mount_path.c_str(),
            ifs_bytes.size());
    }

    return Runtime::Active().LoadScene(mount_path, ifs_path);
}

void ApplyVariants(uint32_t stream_id) {
    auto active = App::Global().ActiveIfs();
    if (active.empty() || stream_id == 0xFFFFFFFC) return;
    auto& cfg = App::Global().MutConfig(active);
    for (auto& slot : cfg.slots) {
        if (!slot.is_valid && (g_afp.afp_mc_get_id_by_path != nullptr)) {
            int const id = g_afp.afp_mc_get_id_by_path(stream_id, slot.path.c_str());
            slot.is_valid = (id >= 0);
        }
        if (!slot.is_valid) continue;
        if (slot.bitmap_override && !slot.bitmap.empty())
            McControl::SetClipBitmap(g_afp, stream_id, slot.path.c_str(), slot.bitmap.c_str());
        McControl::SetClipVisible(g_afp, stream_id, slot.path.c_str(), slot.visible);
    }
}

void ApplySubLayerVisibility(uint32_t stream_id) {
    auto active = App::Global().ActiveIfs();
    if (active.empty() || stream_id == 0xFFFFFFFC) return;
    const auto overrides = App::Global().GetSublayerOverrides(active);
    for (const auto& ov : overrides)
        McControl::SetClipVisible(g_afp, stream_id, ov.first.c_str(), ov.second);
}

namespace {
void PublishSetupStage(const char* stage, float frac = -1.0F) {
    App::Global().UpdateLoadStage(stage, frac);
}
}

namespace {
bool FailBoot(App::State& state, std::string msg) {
    state.EndLoad();
    state.SetBootError(std::move(msg));
    state.SetBootState(App::BootState::Failed);
    return false;
}
}

namespace {

void ScanThreadBody(const std::string& game_dir, bool scan_arcs) noexcept {
    try {
        auto& st = App::Global();
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
            scan_arcs);
        LOG("Boot", "Found %zu IFS files under %s", ifs_list.size(), game_dir.c_str());
        st.SetAvailableIfs(std::move(ifs_list));
        st.SetIfsScanStatus("");
        st.SetIfsScanning(false);
    } catch (...) {
        LOG("Boot", "IFS scan thread: unexpected exception");
    }
}

}

namespace {

const GameProfile::Profile* ResolveBootProfile(App::State& state, const std::string& game_dir,
                                               const std::string& profile_slug) {
    std::string known_slugs;
    for (const auto& p : GameProfile::All()) {
        if (!known_slugs.empty()) known_slugs += ", ";
        known_slugs += p.slug;
    }
    if (!profile_slug.empty()) {
        const GameProfile::Profile* profile = GameProfile::BySlug(profile_slug);
        if (profile == nullptr) {
            FailBoot(state, "Unknown game profile '" + profile_slug +
                                "'. Known profiles: " + known_slugs +
                                ". Fix --profile / the Setup screen selection, or leave "
                                "it empty to auto-detect from the game dir path.");
            return nullptr;
        }
        LOG("Boot", "Game profile: %s [slug=%s] (source: explicit)", profile->name, profile->slug);
        return profile;
    }
    const GameProfile::Profile* profile = GameProfile::AutoDetect(game_dir);
    if (profile == nullptr) {
        FailBoot(state, "Couldn't auto-detect a game profile from '" + game_dir +
                            "' (no known substring in the path). Pass --profile <slug> or "
                            "pick a profile in the Setup screen. Known profiles: " +
                            known_slugs + ".");
        return nullptr;
    }
    LOG("Boot", "Game profile: %s [slug=%s] (source: auto-detect)", profile->name, profile->slug);
    return profile;
}

bool CreateRenderWindowAndDevice(App::State& state, int render_w, int render_h, bool legacy_ddr) {
    g_d3d.width = render_w > 0 ? render_w : 1920;
    g_d3d.height = render_h > 0 ? render_h : 1080;
    if (legacy_ddr) {
        g_d3d.width = 1280;
        g_d3d.height = 720;
    }
    LOG("Boot", "Render resolution: %dx%d", g_d3d.width, g_d3d.height);
    HWND hwnd = AppWindow::Create(g_d3d.width, g_d3d.height);
    if ((hwnd == nullptr) || !g_d3d.Init(hwnd)) {
        return FailBoot(state, "Render window / D3D9 init failed.");
    }
    int rt_w = 0;
    int rt_h = 0;
    g_d3d.GetOffscreenSize(rt_w, rt_h);
    AppWindow::SetRenderRtSize(rt_w, rt_h);
    return true;
}

void SaveBootSettings(const std::string& game_dir, const GameProfile::Profile& profile) {
    Settings::Config cfg;
    cfg.game_dir = game_dir;
    cfg.loop_master = App::Global().GetLoopMaster();
    cfg.render_width = g_d3d.width;
    cfg.render_height = g_d3d.height;
    cfg.render_fps = App::Global().GetRenderFps();
    cfg.game_profile = profile.slug;
    Settings::SaveAtomic(cfg);
    App::Global().SetGameProfileSlug(profile.slug);
}

}

namespace {

bool BootAfpLayer(App::State& state, const GameProfile::Profile& profile, bool legacy_ddr) {
    PublishSetupStage("Booting AFP");
    if (legacy_ddr) {
        if (!DdrAfp::Boot(g_afp_dll, g_afpu_dll, g_d3d)) {
            return FailBoot(state, "DDR AFP 2.13.7 boot failed "
                                   "(libafp-win64 / libafputils-win64).");
        }
        DdrAfp::SetTimeScale(profile.time_scale);
    } else {
        AfpManager::Boot(g_engine, g_d3d);
    }
    return true;
}

void LoadPersistentIfses(const std::string& game_dir, bool load_boot_ifses) {
    if (!load_boot_ifses || !AfpManager::IsBooted()) return;
    PublishSetupStage("Loading persistent IFSes");
    std::filesystem::path data_root = std::filesystem::path(game_dir) / "data";
    std::error_code ec;
    if (!std::filesystem::exists(data_root, ec)) data_root = std::filesystem::path(game_dir);
    AfpManager::LoadBootIfses(g_engine, data_root.string());
}

void FinishBootAndStartScan(App::State& state, const std::string& game_dir,
                            const GameProfile::Profile& profile) {
    SaveBootSettings(game_dir, profile);

    state.SetGameDir(game_dir);
    state.SetBootError({});
    state.EndLoad();
    state.SetBootState(App::BootState::Ready);

    bool const scan_arcs = profile.scan_arc_containers;
    state.SetIfsScanning(true);
    state.SetIfsScanStatus("Scanning for IFS files...");
    std::thread(ScanThreadBody, game_dir, scan_arcs).detach();
}

}

bool BootFromGameDir(HINSTANCE hInstance, const std::string& game_dir, bool want_render_window,
                     bool load_boot_ifses, int render_w, int render_h,
                     const std::string& profile_slug) {
    auto& state = App::Global();
    state.SetBootState(App::BootState::Booting);
    state.BeginLoad(game_dir);

    const GameProfile::Profile* profile = ResolveBootProfile(state, game_dir, profile_slug);
    if (profile == nullptr) return false;
    AfpManager::SetActiveProfile(profile);
    const bool legacy_ddr = profile->legacy_afp;
    Runtime::SelectRuntime(legacy_ddr);
    App::Global().SetIsDdrMode(legacy_ddr);

    PublishSetupStage("Locating game DLLs");
    std::string const dll_dir = DiscoverDllDir(game_dir, *profile);
    if (dll_dir.empty()) {
        return FailBoot(state, "Couldn't find avs2-core.dll / afp-core.dll / afp-utils.dll "
                               "under the selected directory. Expected them in `modules/`.");
    }
    LOG("Boot", "DLLs found in: %s", dll_dir.c_str());

    PublishSetupStage("Loading DLLs");
    if (!LoadAllDlls(dll_dir, *profile)) {
        return FailBoot(state, "Failed to load one or more DLLs from " + dll_dir +
                                   ". The game may be a different version than expected.");
    }

    PublishSetupStage("Booting AVS");
    if (!AvsManager::Boot(g_avs)) {
        return FailBoot(state, "AVS boot failed (avs2-core.dll). Check the log for details.");
    }

    if (want_render_window) {
        PublishSetupStage("Creating render window");
        if (!CreateRenderWindowAndDevice(state, render_w, render_h, legacy_ddr)) return false;
    }

    if (!BootAfpLayer(state, *profile, legacy_ddr)) return false;

    LoadPersistentIfses(game_dir, load_boot_ifses);

    FinishBootAndStartScan(state, game_dir, *profile);

    (void)hInstance;
    return true;
}

void ApplyCliOverrides(const Cli::Options& opts) {
    if (opts.afp_speed > 0.0F && Runtime::Active().SetGlobalSpeed(g_afp, opts.afp_speed)) {
        LOG("Main", "afp global speed set to %.3f (--afp-speed)", opts.afp_speed);
    }
    if (opts.continuous_loop_mode != 0) {
        App::State::LiveOverrides lo = App::Global().GetLiveOverrides();
        lo.continuous_loop_mode = opts.continuous_loop_mode;
        App::Global().SetLiveOverrides(lo);
    }
    if (opts.root_loop_mode == 0) {
        App::Global().SetRootLoopMode(App::State::RootLoopMode::Hold);
    } else if (opts.root_loop_mode == 1) {
        App::Global().SetRootLoopMode(App::State::RootLoopMode::Force);
    }
    {
        App::State::LiveOverrides lo = App::Global().GetLiveOverrides();
        bool touched = false;
        if (opts.start_paused) {
            lo.paused = true;
            touched = true;
        }
        if (opts.filter_enabled) {
            lo.filter_enabled = true;
            touched = true;
        }
        if (opts.show_mc_names) {
            lo.show_mc_names = true;
            lo.mc_name_type = (opts.mc_name_type != 0) ? 1 : 0;
            touched = true;
        }
        if (touched) App::Global().SetLiveOverrides(lo);
    }
    auto active = App::Global().ActiveIfs();
    if (active.empty()) return;
    auto& cfg = App::Global().MutConfig(active);
    for (const auto& ov : opts.slot_overrides) {
        App::VariantSlot* target = nullptr;
        for (auto& s : cfg.slots) {
            if (s.path == ov.path) {
                target = &s;
                break;
            }
        }
        if (target == nullptr) {
            cfg.slots.push_back({});
            target = &cfg.slots.back();
            target->path = ov.path;
            target->default_bitmap = ov.path;
        }
        target->bitmap = ov.bitmap;
        target->visible = ov.visible;
        target->bitmap_override = !ov.bitmap.empty();
    }
    for (const auto& ov : opts.sublayer_overrides)
        App::Global().SetSublayerOverride(active, ov.path, ov.visible);
}
