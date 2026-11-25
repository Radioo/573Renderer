#include "judge_engine/judge_engine.h"

#include "afp_ddr.h"
#include "afp_boot.h"
#include "afp_ddr_funcs.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "backend/afp_profiles.h"
#include "engine_dlls.h"
#include "game_profile.h"
#include "judge/afp_pose.h"
#include "judge_hooks/afp_interp_hooks.h"
#include "render_backend.h"
#include "support/log.h"

#include <cstdint>
#include <d3d9.h>
#include <string>

namespace JudgeEngine {

namespace {

constexpr std::uint32_t kCurrentFrame = 0x1010;
constexpr std::uint32_t kTotalFrames = 0x1011;
constexpr int kVisibleAttribute = 1;

struct Open {
    std::string package_path;
    std::uint32_t authored = 0;
    std::uint32_t interpolated = 0;
    std::uint32_t root_mc = 0;
};

Open g_open;
bool g_booted = false;
D3D9State g_stage;

std::uint32_t CreatePlayingLayer(std::uint32_t stream_id, const std::string& clip) {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    const std::uint32_t layer = afp.LayerCreate(stream_id, clip.c_str());
    if (!afp.LayerValid(layer)) return 0;
    if (afp.afp_layer_play != nullptr) afp.afp_layer_play(layer, 1.0F);
    if (afp.afp_layer_set_attribute != nullptr)
        afp.afp_layer_set_attribute(layer, kVisibleAttribute, kVisibleAttribute);
    return layer;
}

void DestroyLayers() {
    if (g_open.interpolated != 0) AfpInterpHooks::SetLayerInterpolated(g_open.interpolated, false);
    DdrAfp::DestroyLayer(g_open.authored);
    DdrAfp::DestroyLayer(g_open.interpolated);
    g_open.authored = 0;
    g_open.interpolated = 0;
    g_open.root_mc = 0;
}

}

BootResult Boot(const std::string& game_dir, const std::string& profile_slug,
                IDirect3DDevice9* device, int stage_w, int stage_h) {
    BootResult result;
    const GameProfile::Profile* profile = profile_slug.empty() ? GameProfile::AutoDetect(game_dir)
                                                               : GameProfile::BySlug(profile_slug);
    if (profile == nullptr) {
        result.error = "No game profile matches " + game_dir + "; pass --profile.";
        return result;
    }
    const AfpProfiles::AfpConfig* cfg = AfpProfiles::For(profile->slug);
    if (cfg == nullptr || cfg->interp_layout == nullptr || !(cfg->fixed_step_seconds > 0.0F)) {
        result.error =
            std::string("The judge view needs a profile with an interpolation layout; ") +
            profile->slug + " has none.";
        return result;
    }
    AfpManager::SetActiveConfig(cfg);
    const std::string dll_dir = EngineDlls::DiscoverDllDir(game_dir, *cfg);
    if (dll_dir.empty()) {
        result.error = std::string("Could not find ") + cfg->afp_dll + " under " + game_dir + ".";
        return result;
    }
    if (!EngineDlls::Load(g_engine, dll_dir, *cfg, true)) {
        result.error = "The game DLLs failed to load from " + dll_dir + ".";
        return result;
    }
    if (!AvsManager::Boot(g_avs)) {
        result.error = "AVS failed to boot.";
        return result;
    }
    g_stage.device = device;
    g_stage.width = stage_w;
    g_stage.height = stage_h;
    if (!DdrAfp::Boot(g_afp_dll, g_afpu_dll, g_stage)) {
        result.error = "AFP failed to boot.";
        return result;
    }
    result.interp_problem = AfpInterpHooks::Install(
        reinterpret_cast<std::uintptr_t>(g_afp_dll.Module()), *cfg->interp_layout);
    if (!result.interp_problem.empty())
        LOG("Judge", "interpolation unavailable: %s", result.interp_problem.c_str());
    result.fixed_step_seconds = cfg->fixed_step_seconds;
    g_booted = true;
    return result;
}

std::string OpenClip(const std::string& package_path, const std::string& clip) {
    if (!g_booted) return "The engine is not booted.";
    DestroyLayers();
    if (package_path != g_open.package_path) {
        g_open.package_path.clear();
        if (!DdrAfp::LoadTxp2Streams(g_avs, package_path)) return "The package failed to load.";
        g_open.package_path = package_path;
    }
    const std::uint32_t stream = DdrAfp::ClipStreamId(clip);
    if (stream == 0) return "The package has no clip named " + clip + ".";
    g_open.authored = CreatePlayingLayer(stream, clip);
    g_open.interpolated = CreatePlayingLayer(stream, clip);
    if (g_open.authored == 0 || g_open.interpolated == 0) {
        DestroyLayers();
        return "afp could not create a layer for " + clip + ".";
    }
    if (AfpInterpHooks::Installed())
        AfpInterpHooks::SetLayerInterpolated(g_open.interpolated, true);
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    if (afp.afp_layer_mc_refer != nullptr)
        g_open.root_mc = static_cast<std::uint32_t>(afp.afp_layer_mc_refer(g_open.authored, ""));
    return {};
}

void Advance(float dt) {
    DdrAfp::Advance(dt);
}

void SetRenderTime(float seconds_since_advance) {
    AfpInterpHooks::SetRenderTime(seconds_since_advance);
}

void SetMatrixLerp(AfpPose::MatrixLerp mode) {
    AfpInterpHooks::SetMatrixLerp(mode);
}

void DrawSide(Side side) {
    const std::uint32_t layer = side == Side::Authored ? g_open.authored : g_open.interpolated;
    if (layer != 0) DdrAfp::Funcs().DisplayLayer(layer);
}

Playhead ReadPlayhead() {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    Playhead out;
    if (g_open.root_mc == 0 || afp.afp_mc_get_param == nullptr) return out;
    int frame = -1;
    int total = 0;
    afp.afp_mc_get_param(g_open.root_mc, kCurrentFrame, &frame);
    afp.afp_mc_get_param(g_open.root_mc, kTotalFrames, &total);
    out.frame = frame;
    out.total = total;
    return out;
}

AfpInterpHooks::LayerStats InterpolationStats() {
    return AfpInterpHooks::StatsFor(g_open.interpolated);
}

void Shutdown() {
    if (!g_booted) return;
    DestroyLayers();
    AfpInterpHooks::Uninstall();
    DdrAfp::Shutdown();
    AvsManager::Shutdown(g_avs);
    g_booted = false;
}

}
