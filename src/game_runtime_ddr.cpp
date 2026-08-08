#include "game_runtime_internal.h"

#include "afp_ddr.h"
#include "game_runtime.h"
#include "app_globals.h"
#include "state/app_state.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Runtime {

bool DdrRuntime::IsBooted() {
    return DdrAfp::IsBooted();
}

std::string DdrRuntime::ActiveClipName() {
    return DdrAfp::ActiveClip();
}

bool DdrRuntime::HasRenderableScene([[maybe_unused]] uint32_t modern_stream_id) {
    return DdrAfp::IsBooted() && DdrAfp::LayerId() != 0;
}

void DdrRuntime::RenderFrame(float dt, [[maybe_unused]] uint32_t modern_stream_id,
                             [[maybe_unused]] int frame_count) {
    if (DdrAfp::IsBooted()) DdrAfp::RenderFrame(dt);
}

void DdrRuntime::ReprobeVariantSlots([[maybe_unused]] uint32_t modern_stream_id) {}

void DdrRuntime::ApplyContinuousLoop([[maybe_unused]] uint32_t modern_stream_id,
                                     [[maybe_unused]] int continuous_mode) {}

void DdrRuntime::ApplyMasterScale([[maybe_unused]] uint32_t modern_stream_id) {}

void DdrRuntime::ApplyVariantSlots([[maybe_unused]] uint32_t modern_stream_id) {}

void DdrRuntime::ApplySublayerOverrides([[maybe_unused]] uint32_t modern_stream_id) {}

void DdrRuntime::ForceReplayMaster() {
    LOG("Main", "force_replay ignored: DDR backend has no modern master stream");
}

bool DdrRuntime::LoadScene(const std::string& mount_path, const std::string& ifs_path) {
    auto& state = App::Global();
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    std::string const ddr_base = std::filesystem::path(mount_path).filename().string();
    std::string ddr_pkg = ddr_base;
    if (ddr_pkg.size() > 4 && ddr_pkg.ends_with(".ifs")) ddr_pkg.resize(ddr_pkg.size() - 4);
    const bool is_txp2 = ddr_base.size() > 4 && ddr_base.ends_with(".bin");
    state.UpdateLoadStage(is_txp2 ? "Loading TXP2 package" : "Loading DDR package (AFP 2.13.7)");
    bool const ok = is_txp2 ? DdrAfp::LoadTxp2(g_avs, mount_path)
                            : DdrAfp::LoadIfs(g_avs, g_avs_dll, mount_path, ddr_pkg);
    if (ok) {
        auto& cfg = state.MutConfig(ddr_base);
        cfg.filename = ddr_base;
        cfg.anim_names = DdrAfp::ClipNames();
        state.SetActiveIfs(ddr_base);
        App::Status st = state.GetStatus();
        st.current_ifs_path = ifs_path;
        st.stream_id = DdrAfp::LayerId();
        st.scene_loaded = true;
        st.playing_animation = DdrAfp::ActiveClip();
        st.labels.clear();
        for (auto& l : DdrAfp::EnumerateLabels())
            st.labels.push_back({.name = l.name, .frame = l.frame});
        {
            std::string ld;
            for (auto& l : st.labels)
                ld += " " + l.name + "@" + std::to_string(l.frame);
            LOG("Init", "DDR labels (%zu):%s", st.labels.size(), ld.c_str());
        }
        state.SetStatus(st);
    } else {
        LOG("Init", "DDR LoadIfs failed for %s", mount_path.c_str());
    }
    state.EndLoad();
    return ok;
}

void DdrRuntime::UnloadScene() {}

void DdrRuntime::Shutdown() {
    DdrAfp::Shutdown();
}

uint32_t DdrRuntime::ActiveClipId([[maybe_unused]] uint32_t modern_stream_id) {
    return DdrAfp::LayerId();
}

bool DdrRuntime::HaveActiveClip([[maybe_unused]] uint32_t modern_stream_id) {
    return DdrAfp::LayerId() != 0;
}

bool DdrRuntime::ReadPlayhead([[maybe_unused]] uint32_t modern_stream_id, uint32_t* cur,
                              uint32_t* total, uint32_t* raw_loop_count) {
    return DdrAfp::ReadPlayhead(cur, total, raw_loop_count);
}

bool DdrRuntime::ReadSize([[maybe_unused]] uint32_t modern_stream_id, uint32_t* w, uint32_t* h) {
    return DdrAfp::ReadSize(w, h);
}

bool DdrRuntime::ReadRawLayerInfo([[maybe_unused]] uint32_t modern_stream_id,
                                  [[maybe_unused]] uint32_t* raw_cur,
                                  [[maybe_unused]] uint32_t* raw_total,
                                  [[maybe_unused]] uint32_t* flags0) {
    return false;
}

bool DdrRuntime::ReadComplete([[maybe_unused]] const AfpFuncs& afp,
                              [[maybe_unused]] uint32_t modern_stream_id) {
    return false;
}

std::vector<Label> DdrRuntime::EnumerateLabels([[maybe_unused]] const AfpFuncs& afp,
                                               [[maybe_unused]] uint32_t modern_stream_id) {
    std::vector<Label> out;
    for (auto& l : DdrAfp::EnumerateLabels())
        out.push_back({.name = l.name, .frame = l.frame});
    return out;
}

void DdrRuntime::SetPaused([[maybe_unused]] const AfpFuncs& afp, bool paused) {
    DdrAfp::SetPaused(paused);
}

bool DdrRuntime::SeekFrame([[maybe_unused]] const AfpFuncs& afp, int frame) {
    return DdrAfp::SeekFrame(frame);
}

bool DdrRuntime::GotoLabel([[maybe_unused]] const AfpFuncs& afp, const std::string& name) {
    return DdrAfp::GotoLabel(name);
}

void DdrRuntime::SwitchAnimation(const std::string& name,
                                 [[maybe_unused]] const std::string& label) {
    LOG("Main", "DDR switch-clip requested: '%s'", name.c_str());
    App::Status st = App::Global().GetStatus();
    if (DdrAfp::SwitchClip(name)) {
        st.stream_id = DdrAfp::LayerId();
        st.scene_loaded = true;
        st.playing_animation = DdrAfp::ActiveClip();
        st.active_label.clear();
        st.label_playback_active = false;
        st.labels.clear();
        for (auto& l : DdrAfp::EnumerateLabels())
            st.labels.push_back({.name = l.name, .frame = l.frame});
        st.last_error.clear();
    } else {
        st.last_error = "Couldn't switch to clip '" + name + "'";
    }
    App::Global().SetStatus(st);
}

RootRedrive DdrRuntime::MaybeRedriveRootLoop([[maybe_unused]] uint32_t stream_id,
                                             [[maybe_unused]] int loop_cooldown,
                                             [[maybe_unused]] int frames_since_switch,
                                             [[maybe_unused]] int trim_frames) {
    return {};
}

bool DdrRuntime::SetGlobalSpeed([[maybe_unused]] const AfpFuncs& afp,
                                [[maybe_unused]] float speed) {
    return false;
}

bool DdrRuntime::SupportsLiveExtras() {
    return false;
}

bool DdrRuntime::IsLegacyDdr() {
    return true;
}

}
