#include "boot.h"

#include "app_globals.h"
#include "backend/backend.h"
#include "cli/cli.h"
#include "game_profile.h"
#include "game_runtime.h"
#include "render/stretch.h"
#include "render_backend.h"
#include "settings/settings.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"
#include "state/ifs_catalog.h"
#include "support/log.h"
#include "window.h"

#include <string>
#include <utility>
#include <windows.h>

namespace {

bool FailBoot(App::State& state, std::string msg) {
    state.EndLoad();
    state.SetBootError(std::move(msg));
    state.SetBootState(App::BootState::Failed);
    return false;
}

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

bool CreateRenderWindowAndDevice(App::State& state, int render_w, int render_h) {
    g_d3d.width = render_w > 0 ? render_w : 1920;
    g_d3d.height = render_h > 0 ? render_h : 1080;
    const bool stretch = App::Global().GetStretchWide();
    const Stretch::Size present = Stretch::Present(g_d3d.width, g_d3d.height, stretch);
    g_d3d.present_width = present.w;
    g_d3d.present_height = present.h;
    g_d3d.stretch_filter = App::Global().GetStretchFilter();
    LOG("Boot", "Render resolution: %dx%d, presented at %dx%d (%s)", g_d3d.width, g_d3d.height,
        present.w, present.h,
        (present.w != g_d3d.width) ? Stretch::FilterName(g_d3d.stretch_filter) : "1:1");
    HWND hwnd = AppWindow::Create(present.w, present.h);
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
    cfg.stretch_16_9 = App::Global().GetStretchWide();
    cfg.stretch_filter = (int)App::Global().GetStretchFilter();
    cfg.game_profile = profile.slug;
    Settings::SaveAtomic(cfg);
    App::Global().SetGameProfileSlug(profile.slug);
}

}

bool BootFromGameDir(HINSTANCE hInstance, const std::string& game_dir, bool want_render_window,
                     bool load_boot_ifses, int render_w, int render_h,
                     const std::string& profile_slug, bool size_explicit, const Cli::Options* cli) {
    auto& state = App::Global();
    state.SetBootState(App::BootState::Booting);
    state.BeginLoad(game_dir);

    const GameProfile::Profile* profile = ResolveBootProfile(state, game_dir, profile_slug);
    if (profile == nullptr) return false;
    if (!Backend::CreateActive(*profile)) {
        return FailBoot(state, std::string("Unknown backend id '") + profile->backend_id +
                                   "' for profile '" + profile->slug + "'.");
    }
    state.SetActiveBackendId(Backend::Active()->Id());

    int width = render_w;
    int height = render_h;
    if (!size_explicit && profile->default_render_w > 0 && profile->default_render_h > 0) {
        width = profile->default_render_w;
        height = profile->default_render_h;
    }
    state.SetRenderSize(width, height);

    if (want_render_window) {
        state.UpdateLoadStage("Creating render window");
        if (!CreateRenderWindowAndDevice(state, width, height)) return false;
    }

    Backend::BootEnv env;
    env.game_dir = game_dir;
    env.profile = profile;
    env.cli = cli;
    env.load_boot_content = load_boot_ifses;
    if (!Backend::Active()->Boot(env)) return false;

    SaveBootSettings(game_dir, *profile);
    state.SetGameDir(game_dir);
    state.SetBootError({});
    state.EndLoad();
    state.SetBootState(App::BootState::Ready);

    Backend::Active()->StartContentScan();

    (void)hInstance;
    return true;
}

bool MountAndLoadIfs(const std::string& ifs_path, bool from_arc) {
    return Backend::Active()->LoadContent(ifs_path, from_arc);
}

namespace {

void ApplyLiveOverrideCliOpts(const Cli::Options& opts) {
    if (!opts.start_paused && !opts.filter_enabled && !opts.show_mc_names) return;
    App::Global().MutateLiveOverrides([&opts](App::State::LiveOverrides& lo) {
        if (opts.start_paused) lo.paused = true;
        if (opts.filter_enabled) lo.filter_enabled = true;
        if (opts.show_mc_names) {
            lo.show_mc_names = true;
            lo.mc_name_type = (opts.mc_name_type != 0) ? 1 : 0;
        }
    });
}

}

void ApplyCliOverrides(const Cli::Options& opts) {
    if (opts.afp_speed > 0.0F && Runtime::Active().SetGlobalSpeed(g_afp, opts.afp_speed)) {
        LOG("Main", "afp global speed set to %.3f (--afp-speed)", opts.afp_speed);
    }
    if (opts.continuous_loop_mode != 0) {
        App::Global().MutateLiveOverrides([&opts](App::State::LiveOverrides& lo) {
            lo.continuous_loop_mode = opts.continuous_loop_mode;
        });
    }
    if (opts.root_loop_mode == 0) {
        App::Global().SetRootLoopMode(App::State::RootLoopMode::Hold);
    } else if (opts.root_loop_mode == 1) {
        App::Global().SetRootLoopMode(App::State::RootLoopMode::Force);
    }
    ApplyLiveOverrideCliOpts(opts);
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
