#include "state/boot_lifecycle.h"
#include "gpu_context.h"
#include "state/live_controls.h"
#include "support/log.h"
#include "avs_boot.h"
#include "afp_boot.h"
#include "game_runtime.h"
#include "render_backend.h"
#include "state/app_state.h"
#include "cli/cli.h"
#include "settings/settings.h"
#include "gui/gui_thread.h"
#include "gui/gui_window.h"
#include "app_globals.h"
#include "boot.h"
#include "render_loop.h"
#include "qpro_dll.h"
#include "cli/tool_command.h"
#include "tool_commands.h"
#include "qpro_extract.h"
#include <atomic>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <functional>
#include <utility>
#include <vector>
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")

namespace {

std::vector<std::string> Utf8Args(int& argc) {
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args_utf8;
    args_utf8.reserve(argc);
    for (int i = 0; i < argc; i++) {
        int const n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), n, nullptr, nullptr);
        if (!s.empty() && s.back() == '\0') s.pop_back();
        args_utf8.push_back(std::move(s));
    }
    LocalFree(static_cast<HLOCAL>(wargv));
    return args_utf8;
}

void SeedStateFromSettings(const Cli::Options& cli, const Settings::Config& settings,
                           const std::string& initial_dir, int initial_rw, int initial_rh) {
    App::Global().SetGameDir(initial_dir);
    App::Global().SetLoopMaster(settings.loop_master);
    App::Global().SetRootLoopMode(settings.root_loop_force ? App::State::RootLoopMode::Force
                                                           : App::State::RootLoopMode::Hold);
    App::Global().SetMasterScale(cli.master_scale > 0.0F ? cli.master_scale
                                                         : settings.master_scale);
    App::Global().SetRenderSize(initial_rw, initial_rh);
    App::Global().SetRenderFps(cli.render_fps > 0 ? cli.render_fps : settings.render_fps);
    std::string seed_slug;
    if (!cli.game_profile.empty()) {
        seed_slug = cli.game_profile;
    } else if (!settings.game_profile.empty()) {
        if (cli.game_dir.empty() || Settings::SameGameDir(settings.game_dir, cli.game_dir)) {
            seed_slug = settings.game_profile;
        } else {
            LOG("Main",
                "settings.ini profile '%s' was saved for '%s' - ignoring it for "
                "'%s', falling back to auto-detect (pass --profile to override)",
                settings.game_profile.c_str(), settings.game_dir.c_str(), cli.game_dir.c_str());
        }
    }
    App::Global().SetGameProfileSlug(seed_slug);
    App::Global().SetBootState(App::BootState::WaitingForDir);
}

void ForEachCsvToken(const std::string& csv, const std::function<void(const std::string&)>& fn) {
    std::string tok;
    for (size_t p = 0; p <= csv.size(); ++p) {
        if (p == csv.size() || csv[p] == ',') {
            fn(tok);
            tok.clear();
        } else {
            tok += csv[p];
        }
    }
}

void ParseQproPartsCsv(const std::string& csv, QproExtract::Options& o) {
    o.parts.head = o.parts.hand = o.parts.hair = o.parts.face = o.parts.body = o.parts.back = false;
    ForEachCsvToken(csv, [&](const std::string& tok) {
        if (tok == "head") {
            o.parts.head = true;
        } else if (tok == "hand") {
            o.parts.hand = true;
        } else if (tok == "hair") {
            o.parts.hair = true;
        } else if (tok == "face") {
            o.parts.face = true;
        } else if (tok == "body") {
            o.parts.body = true;
        } else if (tok == "back") {
            o.parts.back = true;
        } else if (!tok.empty()) {
            LOG("Qpro", "--qpro-parts: unknown category '%s' (ignored)", tok.c_str());
        }
    });
}

void ParseQproOnlyCsv(const std::string& csv, const std::string& gdir, QproExtract::Options& o) {
    QproDll::Parts const pp = QproDll::Read(gdir);
    for (int c = 0; c < (int)QproDll::Category::Count; ++c)
        o.part_sel.sel[c].assign(pp.of((QproDll::Category)c).size(), 0);
    int nsel = 0;
    ForEachCsvToken(csv, [&](const std::string& tok) {
        size_t const us = tok.find('_');
        if (us == std::string::npos) return;
        std::string const pre = tok.substr(0, us);
        std::string num;
        for (size_t j = us + 1; j < tok.size() && tok[j] >= '0' && tok[j] <= '9'; ++j)
            num += tok[j];
        int const idx = num.empty() ? -1 : atoi(num.c_str());
        int cat = -1;
        for (int c = 0; c < (int)QproDll::Category::Count; ++c)
            if (pre == QproDll::Prefix((QproDll::Category)c)) cat = c;
        if (cat >= 0 && idx >= 0 && std::cmp_less(idx, o.part_sel.sel[cat].size())) {
            o.part_sel.sel[cat][idx] = 1;
            ++nsel;
        } else if (!tok.empty()) {
            LOG("Qpro", "--qpro-only: '%s' not resolved (ignored)", tok.c_str());
        }
    });
    LOG("Qpro", "--qpro-only: %d part(s) selected", nsel);
}

bool WantsQproCliMode(const Cli::Options& cli) {
    return !cli.extract_qpro_dir.empty() || !cli.qpro_dump_ifs.empty() ||
           !cli.qpro_body_one.empty() || !cli.qpro_back_one.empty() || !cli.qpro_head_one.empty() ||
           !cli.qpro_hand_one.empty() || !cli.qpro_hair_one.empty() || !cli.qpro_face_one.empty() ||
           !cli.qpro_clip_one.empty() || !cli.qpro_hand_composite.empty() ||
           !cli.qpro_head_composite.empty() || !cli.qpro_back_composite.empty();
}

bool RunQproOneShot(const Cli::Options& cli, const std::string& gdir) {
    if (!cli.qpro_back_composite.empty()) {
        QproExtract::BackComposite(gdir, cli.qpro_back_composite);
    } else if (!cli.qpro_hand_composite.empty()) {
        QproExtract::HandComposite(gdir, cli.qpro_hand_composite);
    } else if (!cli.qpro_head_composite.empty()) {
        QproExtract::HeadComposite(gdir, cli.qpro_head_composite);
    } else if (!cli.qpro_clip_one.empty()) {
        std::string const arg = cli.qpro_clip_one;
        size_t const colon = arg.rfind(':');
        std::string const ifs = colon != std::string::npos ? arg.substr(0, colon) : arg;
        std::string const clip = colon != std::string::npos ? arg.substr(colon + 1) : "";
        QproExtract::ClipOne(gdir, ifs, clip);
    } else if (!cli.qpro_back_one.empty()) {
        QproExtract::BackOne(gdir, cli.qpro_back_one);
    } else if (!cli.qpro_head_one.empty()) {
        QproExtract::HeadOne(gdir, cli.qpro_head_one);
    } else if (!cli.qpro_hand_one.empty()) {
        QproExtract::HandOne(gdir, cli.qpro_hand_one);
    } else if (!cli.qpro_hair_one.empty()) {
        QproExtract::HairOne(gdir, cli.qpro_hair_one);
    } else if (!cli.qpro_face_one.empty()) {
        QproExtract::FaceOne(gdir, cli.qpro_face_one);
    } else if (!cli.qpro_body_one.empty()) {
        QproExtract::BodyOne(gdir, cli.qpro_body_one);
    } else if (!cli.qpro_dump_ifs.empty()) {
        std::string ip = cli.qpro_dump_ifs;
        if (!std::filesystem::path(ip).is_absolute())
            ip = (std::filesystem::path(gdir) / "data" / "graphic" / ip).string();
        QproExtract::DumpIfs(ip);
    } else {
        return false;
    }
    return true;
}

void RunQproCliMode(const Cli::Options& cli, const std::string& gdir) {
    QproExtract::SetHueScopeEnabled(!cli.qpro_no_hue_scope);
    if (RunQproOneShot(cli, gdir)) return;
    QproExtract::Options o;
    o.game_dir = gdir;
    o.out_dir = cli.extract_qpro_dir;
    if (cli.qpro_fps > 0) o.fps = cli.qpro_fps;
    if (!cli.qpro_parts.empty()) ParseQproPartsCsv(cli.qpro_parts, o);
    if (!cli.qpro_only.empty()) ParseQproOnlyCsv(cli.qpro_only, gdir, o);
    QproExtract::Result const r = QproExtract::Run(o);
    if (!r.error.empty()) LOG("Qpro", "ERROR: %s", r.error.c_str());
}

std::string NormalizeSlashes(std::string s) {
    for (auto& c : s)
        if (c == '\\') c = '/';
    return s;
}

bool IfsEntryMatches(const std::string& name_norm, const std::string& needle) {
    if (name_norm == needle || name_norm == needle + ".ifs") return true;
    size_t const slash = name_norm.find_last_of('/');
    std::string const tail = slash == std::string::npos ? name_norm : name_norm.substr(slash + 1);
    return tail == needle || tail == needle + ".ifs";
}

std::string ResolveStartupIfs(App::State& state, const std::string& requested,
                              bool& startup_from_arc) {
    std::string startup_ifs = requested;
    startup_from_arc = false;
    if (!startup_ifs.empty() && !std::filesystem::path(startup_ifs).is_absolute()) {
        state.WaitForIfsScan();
        auto list = state.ListAvailableIfs();
        std::string const needle = NormalizeSlashes(startup_ifs);
        for (auto& e : list) {
            if (!IfsEntryMatches(NormalizeSlashes(e.name), needle)) continue;
            startup_ifs = e.full_path;
            startup_from_arc = e.from_arc;
            break;
        }
    }
    if (!startup_from_arc && startup_ifs.size() > 4 && startup_ifs.ends_with(".arc"))
        startup_from_arc = true;
    return startup_ifs;
}

bool ParseCliOrReport(int argc, char** argv, const std::vector<std::string>& args_utf8,
                      Cli::Options& cli, int& exit_rc) {
    bool want_gui = true;
    for (const auto& a : args_utf8) {
        if (a == "--no-gui" || a == "--headless") {
            want_gui = false;
            break;
        }
    }
    std::string cli_err;
    if (Cli::Parse(argc, argv, cli, cli_err)) return true;
    if (!cli_err.empty()) {
        LOG("Cli", "%s", cli_err.c_str());
        if (want_gui)
            MessageBoxA(nullptr, cli_err.c_str(), "573Renderer: argument error", MB_ICONERROR);
    }
    Log::Shutdown();
    exit_rc = cli_err.empty() ? 0 : 1;
    return false;
}

void PostInitialBootRequest(const std::string& game_dir, int rw, int rh) {
    App::Request r;
    r.set_game_dir = true;
    r.game_dir = game_dir;
    r.render_width = rw;
    r.render_height = rh;
    r.game_profile = App::Global().GetGameProfileSlug();
    App::Global().PostRequest(std::move(r));
}

void RaiseGuiWindow() {
    HWND gh = Gui::GetHwnd();
    if (gh == nullptr) return;
    ShowWindow(gh, SW_SHOW);
    BringWindowToTop(gh);
    SetForegroundWindow(gh);
    LOG("Main", "Raised control window to foreground after boot (hwnd=%p)", gh);
}

void InstallGpuStateHooks() {
    g_gpu.on_texture_created = [] { App::Global().BumpTexturesLoaded(); };
    g_gpu.query_crop_overlay = [](bool* pick, int* x, int* y, int* w, int* h) {
        auto& st = App::Global();
        *pick = st.GetCropPickMode();
        const App::CropRect r = st.GetCropRect();
        *x = r.x;
        *y = r.y;
        *w = r.w;
        *h = r.h;
    };
}

bool WaitForFirstBoot(HINSTANCE hInstance, const Cli::Options& cli, bool have_gui,
                      App::State& state, int& exit_rc) {
    exit_rc = -1;
    while (!state.ShouldExit().load(std::memory_order_acquire)) {
        auto req = state.TakeRequest();
        if (req && req->set_game_dir && !req->game_dir.empty()) {
            LOG("Boot", "Set-game-dir request for '%s'", req->game_dir.c_str());
            int rw = req->render_width;
            int rh = req->render_height;
            if (rw <= 0 || rh <= 0) state.GetRenderSize(rw, rh);
            std::string slug = req->game_profile;
            if (slug.empty()) slug = state.GetGameProfileSlug();
            if (BootFromGameDir(hInstance, req->game_dir, !cli.headless, cli.boot_ifses, rw, rh,
                                slug)) {
                return true;
            }
            if (!have_gui) {
                LOG("Main", "Boot failed with no GUI to retry from: %s",
                    state.GetBootError().c_str());
                Log::Shutdown();
                exit_rc = 1;
                return false;
            }
        } else if (cli.headless) {
            LOG("Main", "--headless without --game-dir: nothing to do, exiting.");
            Log::Shutdown();
            if (have_gui) GuiThread::Stop();
            exit_rc = 1;
            return false;
        }
        Sleep(30);
    }
    return false;
}

}

namespace {

void ShutdownEngineStack() {
    AfpManager::Shutdown(g_engine);
    AvsManager::Shutdown(g_avs);
    g_d3d.Shutdown();
    Log::Shutdown();
}

int RunQproCliAndExit(const Cli::Options& cli, const std::string& initial_dir, bool have_gui) {
    std::string const gdir = !cli.game_dir.empty() ? cli.game_dir : initial_dir;
    RunQproCliMode(cli, gdir);
    ShutdownEngineStack();
    if (have_gui) GuiThread::Stop();
    return 0;
}

std::vector<std::string> CollectCliArgs(int& argc, std::vector<char*>& argv_ptrs) {
    std::vector<std::string> args_utf8 = Utf8Args(argc);
    argv_ptrs.reserve(argc);
    for (auto& s : args_utf8)
        argv_ptrs.push_back(s.data());
    return args_utf8;
}

bool StartGuiIfWanted(HINSTANCE hInstance, const Cli::Options& cli) {
    if (cli.no_gui || cli.headless) return false;
    bool const have_gui = GuiThread::Start(hInstance);
    if (!have_gui) LOG("Main", "GUI thread launch failed - continuing without it");
    return have_gui;
}

void MountStartupContent(App::State& state, const Cli::Options& cli) {
    bool startup_from_arc = false;
    std::string const startup_ifs = ResolveStartupIfs(state, cli.startup_ifs, startup_from_arc);
    bool const afp_ready = Runtime::Active().IsBooted();
    if (!startup_ifs.empty() && afp_ready) {
        MountAndLoadIfs(startup_ifs, startup_from_arc);
        ApplyCliOverrides(cli);
    }
}

}

int WINAPI WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                   [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nShowCmd) {
    SetProcessDPIAware();
    Log::Init();

    int argc = 0;
    std::vector<char*> argv_ptrs;
    std::vector<std::string> args_utf8 = CollectCliArgs(argc, argv_ptrs);

    const Cli::ToolCommand tool = Cli::ParseToolCommand(args_utf8);
    if (tool.kind != Cli::ToolKind::None) {
        int const rc = ToolCommands::Run(tool);
        Log::Shutdown();
        return rc;
    }

    Cli::Options cli;
    int cli_rc = -1;
    if (!ParseCliOrReport(argc, argv_ptrs.data(), args_utf8, cli, cli_rc)) return cli_rc;

    Settings::Config const settings = Settings::Load();
    std::string const initial_dir = !cli.game_dir.empty() ? cli.game_dir : settings.game_dir;
    int const initial_rw = cli.render_width > 0 ? cli.render_width : settings.render_width;
    int const initial_rh = cli.render_height > 0 ? cli.render_height : settings.render_height;
    SeedStateFromSettings(cli, settings, initial_dir, initial_rw, initial_rh);

    const bool have_gui = StartGuiIfWanted(hInstance, cli);

    if (!cli.game_dir.empty()) PostInitialBootRequest(cli.game_dir, initial_rw, initial_rh);

    auto& state = App::Global();
    int wait_rc = -1;
    bool const booted = WaitForFirstBoot(hInstance, cli, have_gui, state, wait_rc);
    if (!booted && wait_rc >= 0) return wait_rc;

    if (!booted) {
        LOG("Shutdown", "Exited before boot - cleaning up GUI and leaving.");
        if (have_gui) GuiThread::Stop();
        Log::Shutdown();
        return 0;
    }

    if (WantsQproCliMode(cli)) return RunQproCliAndExit(cli, initial_dir, have_gui);

    MountStartupContent(state, cli);

    if (cli.headless) {
        LOG("Main", "Headless mode - exiting after init.");
        ShutdownEngineStack();
        return 0;
    }

    if (have_gui) RaiseGuiWindow();
    InstallGpuStateHooks();

    return RunRenderLoop(cli, have_gui);
}
