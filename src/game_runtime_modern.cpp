#include "game_runtime_internal.h"

#include "afp_boot.h"
#include "game_runtime.h"
#include "afp_funcs.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "export.h"
#include "game_profile.h"
#include "ifs_inspect.h"
#include "mc_control.h"
#include "render_backend.h"
#include "render_seh.h"
#include "state/app_state.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <windows.h>

namespace Runtime {

namespace {

void ApplySceneAtlasFilters() {
    AfpD3D9::ClearAtlasFilterQueue();
    auto atlas_filters = IfsInspect::ReadAtlasFilters(g_avs);
    for (auto& af : atlas_filters)
        AfpD3D9::EnqueueAtlasFilter(af.mag_filter_d3d, af.min_filter_d3d);
    if (atlas_filters.empty()) return;
    LOG("Load", "Per-atlas sampler filters from texturelist.xml:");
    for (size_t i = 0; i < atlas_filters.size(); i++) {
        auto& af = atlas_filters[i];
        auto filter_name = [](unsigned v) -> const char* {
            if (v == 1) return "POINT";
            if (v == 2) return "LINEAR";
            return "(default)";
        };
        const char* mag = filter_name(af.mag_filter_d3d);
        const char* min = filter_name(af.min_filter_d3d);
        LOG("Load", "  atlas %zu: mag=%s min=%s", i, mag, min);
    }
}

void PublishSceneStatus(App::State& state, const std::string& ifs_path,
                        const std::string& mount_path) {
    App::Status st = state.GetStatus();
    st.current_ifs_path = ifs_path;
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(mount_path, ec);
        st.ifs_size_bytes = ec ? 0 : (uint64_t)sz;
        st.load_time_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    }
    st.stream_id = AfpManager::StreamId();
    st.playing_animation = AfpManager::AnimName();
    st.labels.clear();
    for (auto& l : AfpManager::EnumerateLabels(g_afp))
        st.labels.push_back({.name = l.name, .frame = l.frame});
    state.SetStatus(st);
}

void PublishReplayedStatus(uint32_t sid) {
    App::Status st = App::Global().GetStatus();
    st.stream_id = sid;
    st.playing_animation = AfpManager::AnimName();
    App::Global().SetStatus(st);
}

void LogAfpuWorldMatOnce() {
    static bool logged_pre_state = false;
    if (logged_pre_state) return;
    logged_pre_state = true;
    HMODULE afpu_mod = GetModuleHandleA("afp-utils.dll");
    const auto& off = GameProfile::ActiveOffsets();
    if ((afpu_mod != nullptr) && (off.afpu_world_mat_type != 0U) && (off.afpu_world_mat != 0U)) {
        uint8_t const mat_type = *((uint8_t*)afpu_mod + off.afpu_world_mat_type);
        auto* mat = (float*)((uint8_t*)afpu_mod + off.afpu_world_mat);
        LOG("AFP", "afpu world_mat_type byte = %u, mat[0..7] = %f %f %f %f %f %f %f %f",
            (unsigned)mat_type, mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7]);
    }
}

void UnloadAllCompanions(App::IfsConfig& cfg) {
    for (auto& c : cfg.companions) {
        if (c.loaded) {
            LOG("Main",
                "toggle_companion: unload '%s' "
                "(pkg_id=0x%08x)",
                c.display_name.c_str(), c.pkg_id);
            AfpManager::UnloadCompanion(g_engine, c.pkg_id);
            c.pkg_id = 0;
            c.loaded = false;
        }
    }
}

void LoadCompanionAt(App::IfsConfig& cfg, int idx) {
    auto& c = cfg.companions[idx];
    std::string pkg_name;
    {
        namespace fs = std::filesystem;
        pkg_name = fs::path(c.path).stem().string();
    }
    LOG("Main",
        "toggle_companion: load '%s' "
        "(pkg_name='%s')",
        c.display_name.c_str(), pkg_name.c_str());
    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, c.path, pkg_name);
    if (pkg != 0U) {
        c.pkg_id = pkg;
        c.loaded = true;
    } else {
        App::Status st = App::Global().GetStatus();
        st.last_error = "Failed to load companion " + c.display_name;
        App::Global().SetStatus(st);
    }
}

void ReplayMasterForBindings() {
    std::string const& anim = AfpManager::AnimName();
    if (anim.empty()) return;
    LOG("Main",
        "toggle_companion: replay master "
        "'%s' so new bindings resolve",
        anim.c_str());
    AfpManager::ForceReplay(g_engine);
    App::Status st = App::Global().GetStatus();
    st.stream_id = AfpManager::StreamId();
    st.playing_animation = AfpManager::AnimName();
    App::Global().SetStatus(st);
}

}

bool ModernRuntime::IsBooted() {
    return AfpManager::IsBooted();
}

std::string ModernRuntime::ActiveClipName() {
    return AfpManager::AnimName();
}

bool ModernRuntime::HasRenderableScene(uint32_t modern_stream_id) {
    return modern_stream_id != kModernNoStream;
}

void ModernRuntime::RenderFrame([[maybe_unused]] float dt, uint32_t modern_stream_id,
                                int frame_count) {
    if (!AfpManager::IsBooted() || (g_afp.afp_do_sort_render == nullptr) ||
        modern_stream_id == kModernNoStream)
        return;
    LogAfpuWorldMatOnce();
    static bool logged_render_fault = false;
    const RenderSeh::FaultReport sr = RenderSeh::SafeCallSortRender(g_afp.afp_do_sort_render);
    if (sr.faulted && !logged_render_fault) {
        RenderSeh::LogFault("afp_do_sort_render", frame_count, sr);
        logged_render_fault = true;
    }
}

void ModernRuntime::ReprobeVariantSlots(uint32_t modern_stream_id) {
    const std::string active = App::Global().ActiveIfs();
    if (!active.empty() && modern_stream_id != kModernNoStream)
        IfsInspect::ProbeSlots(g_afp, modern_stream_id, App::Global().MutConfig(active));
}

void ModernRuntime::ApplyContinuousLoop(uint32_t modern_stream_id, int continuous_mode) {
    if (modern_stream_id != kModernNoStream && (g_afp.afp_set_flag_mask != nullptr) &&
        (flag_dance_done_for_ != modern_stream_id || flag_dance_mode_seen_ != continuous_mode)) {
        if (continuous_mode == 1) {
            g_afp.afp_set_flag_mask(modern_stream_id, 0x200, 0x0);
            g_afp.afp_set_flag_mask(modern_stream_id, 0x1, 0x0);
            g_afp.afp_set_flag_mask(modern_stream_id, 0x1000, 0x1000);
            g_afp.afp_set_flag_mask(modern_stream_id, 0x1, 0x1);
            LOG("Live", "applied continuous-loop flag sequence on stream 0x%08x", modern_stream_id);
        } else if (continuous_mode == -1) {
            g_afp.afp_set_flag_mask(modern_stream_id, 0x1000, 0x0);
            LOG("Live", "cleared continuous-loop on stream 0x%08x", modern_stream_id);
        }
        flag_dance_done_for_ = modern_stream_id;
        flag_dance_mode_seen_ = continuous_mode;
    }
    if (continuous_mode == 1 && modern_stream_id != kModernNoStream &&
        (g_afp.afp_set_flag_mask != nullptr)) {
        g_afp.afp_set_flag_mask(modern_stream_id, 0x1, 0x1);
    }
}

void ModernRuntime::ApplyMasterScale(uint32_t modern_stream_id) {
    const float scale = App::Global().GetMasterScale();
    const bool stream_changed = (modern_stream_id != master_scale_sid_);
    const bool scale_changed = (scale != master_scale_last_);
    if (modern_stream_id != kModernNoStream && (g_afp.afp_mc_get != nullptr) &&
        (g_afp.afp_mc_get_id_by_path != nullptr) && (stream_changed || scale_changed)) {
        int const root_mc = g_afp.afp_mc_get_id_by_path(modern_stream_id, "");
        if (root_mc > 0) {
            float xy[2] = {scale, scale};
            constexpr uint32_t kOpSetXyScale = 0x1003;
            constexpr uint32_t kOpInvalidate = 0x101E;
            g_afp.afp_mc_get(root_mc, kOpSetXyScale, (intptr_t)(uintptr_t)xy);
            g_afp.afp_mc_get(root_mc, kOpInvalidate, 1);
        }
        master_scale_last_ = scale;
        master_scale_sid_ = modern_stream_id;
    }
}

void ModernRuntime::ApplyVariantSlots(uint32_t modern_stream_id) {
    auto active = App::Global().ActiveIfs();
    if (active.empty() || modern_stream_id == kModernNoStream) return;
    auto& cfg = App::Global().MutConfig(active);
    for (auto& slot : cfg.slots) {
        if (!slot.is_valid && (g_afp.afp_mc_get_id_by_path != nullptr)) {
            int const id = g_afp.afp_mc_get_id_by_path(modern_stream_id, slot.path.c_str());
            slot.is_valid = (id >= 0);
        }
        if (!slot.is_valid) continue;
        if (slot.bitmap_override && !slot.bitmap.empty()) {
            McControl::SetClipBitmap(g_afp, modern_stream_id, slot.path.c_str(),
                                     slot.bitmap.c_str());
        }
        McControl::SetClipVisible(g_afp, modern_stream_id, slot.path.c_str(), slot.visible);
    }
}

void ModernRuntime::ApplySublayerOverrides(uint32_t modern_stream_id) {
    auto active = App::Global().ActiveIfs();
    if (active.empty() || modern_stream_id == kModernNoStream) return;
    const auto overrides = App::Global().GetSublayerOverrides(active);
    for (const auto& ov : overrides)
        McControl::SetClipVisible(g_afp, modern_stream_id, ov.first.c_str(), ov.second);
}

void ModernRuntime::ForceReplayMaster() {
    LOG("Main", "force_replay requested (variant default pick)");
    if (AfpManager::ForceReplay(g_engine)) {
        App::Status st = App::Global().GetStatus();
        st.stream_id = AfpManager::StreamId();
        st.playing_animation = AfpManager::AnimName();
        st.active_label.clear();
        st.label_playback_active = false;
        App::Global().SetStatus(st);
    }
}

void ModernRuntime::ToggleCompanion(int companion_index) {
    auto active = App::Global().ActiveIfs();
    if (active.empty()) {
        LOG("Main", "toggle_companion: no active IFS, ignoring");
        return;
    }
    auto& cfg = App::Global().MutConfig(active);
    if (companion_index < 0 || std::cmp_greater_equal(companion_index, cfg.companions.size())) {
        LOG("Main",
            "toggle_companion: index %d out of "
            "range (size=%zu)",
            companion_index, cfg.companions.size());
        return;
    }
    bool const will_load = !cfg.companions[companion_index].loaded;

    App::Global().BeginLoad(cfg.companions[companion_index].display_name);
    App::Global().UpdateLoadStage(will_load ? "Loading companion IFS" : "Unloading companion");

    UnloadAllCompanions(cfg);
    if (will_load) LoadCompanionAt(cfg, companion_index);
    ReplayMasterForBindings();
    App::Global().EndLoad();
}

bool ModernRuntime::LoadScene(const std::string& mount_path, const std::string& ifs_path) {
    auto& state = App::Global();
    state.UpdateLoadStage("Mounting IFS");
    if (!AvsManager::MountIfs(g_avs, g_avs_dll, mount_path)) {
        LOG("Init", "Mount failed for %s", mount_path.c_str());
        state.EndLoad();
        return false;
    }
    std::string const basename = std::filesystem::path(mount_path).filename().string();
    std::string pkg_hint = basename;
    if (pkg_hint.size() > 4 && pkg_hint.ends_with(".ifs")) {
        pkg_hint.resize(pkg_hint.size() - 4);
    }

    int const expected = IfsInspect::CountExpectedTextures(g_avs);
    LOG("Load", "Expecting %d texture atlas(es) in %s", expected, basename.c_str());
    if (expected > 0) state.SetTexturesExpected(expected);

    ApplySceneAtlasFilters();

    state.UpdateLoadStage("Loading package (textures, clips, geometry)");
    if (!AfpManager::LoadPackages(g_engine, pkg_hint)) {
        LOG("Init", "LoadPackages failed for %s", ifs_path.c_str());
        state.EndLoad();
        return false;
    }

    state.UpdateLoadStage("Inspecting IFS dictionary");
    auto& cfg = App::Global().MutConfig(basename);
    IfsInspect::LoadDictionary(g_avs, cfg);

    cfg.companions = IfsInspect::FindCompanions(ifs_path);
    if (!cfg.companions.empty()) {
        LOG("Inspect", "IFS '%s': %zu locale companion(s) found next to base", cfg.filename.c_str(),
            cfg.companions.size());
    }

    state.SetActiveIfs(basename);
    PublishSceneStatus(state, ifs_path, mount_path);
    state.EndLoad();
    return true;
}

void ModernRuntime::UnloadScene() {
    AfpManager::UnloadPackages(g_engine);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
}

void ModernRuntime::Shutdown() {
    AfpManager::Shutdown(g_engine);
}

uint32_t ModernRuntime::ActiveClipId(uint32_t modern_stream_id) {
    return modern_stream_id;
}

bool ModernRuntime::HaveActiveClip(uint32_t modern_stream_id) {
    return modern_stream_id != kModernNoStream;
}

bool ModernRuntime::ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                                 uint32_t* raw_loop_count) {
    if (modern_stream_id == kModernNoStream) return false;
    return AfpManager::ReadMcPlayhead(g_afp, cur, total, raw_loop_count);
}

bool ModernRuntime::ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h) {
    if ((g_afp.afp_get_layer_info == nullptr) || modern_stream_id == kModernNoStream) return false;
    uint8_t info[64] = {};
    if (g_afp.afp_get_layer_info(modern_stream_id, info) < 0) return false;
    const auto* u16 = reinterpret_cast<const uint16_t*>(info);
    if (w != nullptr) *w = u16[8];
    if (h != nullptr) *h = u16[9];
    return true;
}

bool ModernRuntime::ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur,
                                     uint32_t* raw_total, uint32_t* flags0) {
    if ((g_afp.afp_get_layer_info == nullptr) || modern_stream_id == kModernNoStream) return false;
    uint8_t info[64] = {};
    if (g_afp.afp_get_layer_info(modern_stream_id, info) < 0) return false;
    const auto* w = reinterpret_cast<const uint32_t*>(info);
    if (flags0 != nullptr) *flags0 = w[1];
    if (raw_total != nullptr) *raw_total = w[12];
    if (raw_cur != nullptr) *raw_cur = w[13];
    return true;
}

bool ModernRuntime::ReadComplete(const AfpFuncs& afp, [[maybe_unused]] uint32_t modern_stream_id) {
    return AfpManager::IsMasterComplete(afp);
}

std::vector<Label> ModernRuntime::EnumerateLabels(const AfpFuncs& afp,
                                                  [[maybe_unused]] uint32_t modern_stream_id) {
    std::vector<Label> out;
    for (auto& l : AfpManager::EnumerateLabels(afp))
        out.push_back({.name = l.name, .frame = l.frame});
    return out;
}

void ModernRuntime::SetPaused(const AfpFuncs& afp, bool paused) {
    AfpManager::SetStreamPaused(afp, paused);
}

bool ModernRuntime::SeekFrame(const AfpFuncs& afp, int frame) {
    AfpManager::SeekFrame(afp, frame);
    return true;
}

bool ModernRuntime::GotoLabel(const AfpFuncs& afp, const std::string& name) {
    AfpManager::GotoLabel(afp, name);
    return true;
}

void ModernRuntime::SwitchAnimation(const std::string& name, const std::string& label) {
    LOG("Main", "Switch-animation requested: '%s'", name.c_str());
    bool ok = false;
    if (name == AfpManager::AnimName()) {
        ok = AfpManager::ForceReplay(g_engine);
    } else {
        ok = AfpManager::SwitchAnimation(g_engine, name);
    }
    if (ok) {
        if (!label.empty()) AfpManager::GotoLabel(g_afp, label);
        App::Status st = App::Global().GetStatus();
        st.stream_id = AfpManager::StreamId();
        st.playing_animation = AfpManager::AnimName();
        st.active_label = label;
        st.label_playback_active = !label.empty();
        st.last_error.clear();
        st.labels.clear();
        for (auto& l : AfpManager::EnumerateLabels(g_afp))
            st.labels.push_back({.name = l.name, .frame = l.frame});
        App::Global().SetStatus(st);
    } else {
        App::Status st = App::Global().GetStatus();
        st.last_error = "Couldn't switch to '" + name + "'";
        App::Global().SetStatus(st);
    }
}

RootRedrive ModernRuntime::MaybeRedriveRootLoop(uint32_t stream_id, int loop_cooldown,
                                                int frames_since_switch, int trim_frames) {
    if (App::Global().GetRootLoopMode() == App::State::RootLoopMode::Hold) return {};
    if (trim_frames > 0 && stream_id != kModernNoStream && loop_cooldown == 0 &&
        frames_since_switch >= trim_frames) {
        AfpManager::ForceReplay(g_engine);
        const uint32_t sid = AfpManager::StreamId();
        PublishReplayedStatus(sid);
        flag_dance_done_for_ = kModernNoStream;
        return {.replayed = true, .new_stream_id = sid};
    }
    if (App::Global().GetLoopMaster() && stream_id != kModernNoStream && loop_cooldown == 0 &&
        AfpManager::IsMasterComplete(g_afp) && !Export::IsCapturing()) {
        AfpManager::ForceReplay(g_engine);
        const uint32_t sid = AfpManager::StreamId();
        PublishReplayedStatus(sid);
        return {.replayed = true, .new_stream_id = sid};
    }
    return {};
}

bool ModernRuntime::SetGlobalSpeed(const AfpFuncs& afp, float speed) {
    if (afp.afp_set_global_speed == nullptr) return false;
    afp.afp_set_global_speed(speed);
    return true;
}

bool ModernRuntime::SupportsLiveExtras() {
    return true;
}

bool ModernRuntime::IsLegacyDdr() {
    return false;
}

}
