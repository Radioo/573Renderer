#include "game_runtime.h"

#include "afp_boot.h"
#include "afp_ddr.h"
#include "afp_funcs.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "export.h"
#include "ifs_inspect.h"
#include "render_backend.h"
#include "state/app_state.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace Runtime {

namespace {

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

}

class ModernRuntime : public IGameRuntime {
public:
    bool IsBooted() override { return AfpManager::IsBooted(); }

    std::string ActiveClipName() override { return AfpManager::AnimName(); }

    bool HasRenderableScene(uint32_t modern_stream_id) override {
        return modern_stream_id != kModernNoStream;
    }

    void RenderFrame([[maybe_unused]] float dt) override {}

    void ReprobeVariantSlots(uint32_t modern_stream_id) override {
        const std::string active = App::Global().ActiveIfs();
        if (!active.empty() && modern_stream_id != kModernNoStream)
            IfsInspect::ProbeSlots(g_afp, modern_stream_id, App::Global().MutConfig(active));
    }

    bool LoadScene(const std::string& mount_path, const std::string& ifs_path) override {
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
            LOG("Inspect", "IFS '%s': %zu locale companion(s) found next to base",
                cfg.filename.c_str(), cfg.companions.size());
        }

        state.SetActiveIfs(basename);
        PublishSceneStatus(state, ifs_path, mount_path);
        state.EndLoad();
        return true;
    }

    void UnloadScene() override {
        AfpManager::UnloadPackages(g_engine);
        if (g_avs.avs_fs_umount != nullptr) {
            g_avs.avs_fs_umount("/afp/packages");
            g_avs.avs_fs_umount("/data");
        }
    }

    void Shutdown() override { AfpManager::Shutdown(g_engine); }

    uint32_t ActiveClipId(uint32_t modern_stream_id) override { return modern_stream_id; }

    bool HaveActiveClip(uint32_t modern_stream_id) override {
        return modern_stream_id != kModernNoStream;
    }

    bool ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                      uint32_t* raw_loop_count) override {
        if (modern_stream_id == kModernNoStream) return false;
        return AfpManager::ReadMcPlayhead(g_afp, cur, total, raw_loop_count);
    }

    bool ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h) override {
        if ((g_afp.afp_get_layer_info == nullptr) || modern_stream_id == kModernNoStream)
            return false;
        uint8_t info[64] = {};
        if (g_afp.afp_get_layer_info(modern_stream_id, info) < 0) return false;
        const auto* u16 = reinterpret_cast<const uint16_t*>(info);
        if (w != nullptr) *w = u16[8];
        if (h != nullptr) *h = u16[9];
        return true;
    }

    bool ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur, uint32_t* raw_total,
                          uint32_t* flags0) override {
        if ((g_afp.afp_get_layer_info == nullptr) || modern_stream_id == kModernNoStream)
            return false;
        uint8_t info[64] = {};
        if (g_afp.afp_get_layer_info(modern_stream_id, info) < 0) return false;
        const auto* w = reinterpret_cast<const uint32_t*>(info);
        if (flags0 != nullptr) *flags0 = w[1];
        if (raw_total != nullptr) *raw_total = w[12];
        if (raw_cur != nullptr) *raw_cur = w[13];
        return true;
    }

    bool ReadComplete(const AfpFuncs& afp, [[maybe_unused]] uint32_t stream_id) override {
        return AfpManager::IsMasterComplete(afp);
    }

    std::vector<Label> EnumerateLabels(const AfpFuncs& afp,
                                       [[maybe_unused]] uint32_t stream_id) override {
        std::vector<Label> out;
        for (auto& l : AfpManager::EnumerateLabels(afp))
            out.push_back({.name = l.name, .frame = l.frame});
        return out;
    }

    void SetPaused(const AfpFuncs& afp, bool paused) override {
        AfpManager::SetStreamPaused(afp, paused);
    }

    bool SeekFrame(const AfpFuncs& afp, int frame) override {
        AfpManager::SeekFrame(afp, frame);
        return true;
    }

    bool GotoLabel(const AfpFuncs& afp, const std::string& name) override {
        AfpManager::GotoLabel(afp, name);
        return true;
    }

    void SwitchAnimation(const std::string& name, const std::string& label) override {
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

    RootRedrive MaybeRedriveRootLoop(uint32_t stream_id, int loop_cooldown, int frames_since_switch,
                                     int trim_frames) override {
        if (App::Global().GetRootLoopMode() == App::State::RootLoopMode::Hold) return {};
        if (trim_frames > 0 && stream_id != kModernNoStream && loop_cooldown == 0 &&
            frames_since_switch >= trim_frames) {
            AfpManager::ForceReplay(g_engine);
            const uint32_t sid = AfpManager::StreamId();
            App::Status st = App::Global().GetStatus();
            st.stream_id = sid;
            st.playing_animation = AfpManager::AnimName();
            App::Global().SetStatus(st);
            return {.replayed = true, .reset_flag_dance = true, .new_stream_id = sid};
        }
        if (App::Global().GetLoopMaster() && stream_id != kModernNoStream && loop_cooldown == 0 &&
            AfpManager::IsMasterComplete(g_afp) && !Export::IsCapturing()) {
            AfpManager::ForceReplay(g_engine);
            const uint32_t sid = AfpManager::StreamId();
            App::Status st = App::Global().GetStatus();
            st.stream_id = sid;
            st.playing_animation = AfpManager::AnimName();
            App::Global().SetStatus(st);
            return {.replayed = true, .reset_flag_dance = false, .new_stream_id = sid};
        }
        return {};
    }

    bool SetGlobalSpeed(const AfpFuncs& afp, float speed) override {
        if (afp.afp_set_global_speed == nullptr) return false;
        afp.afp_set_global_speed(speed);
        return true;
    }

    bool SupportsLiveExtras() override { return true; }

    bool IsLegacyDdr() override { return false; }
};

class DdrRuntime : public IGameRuntime {
public:
    bool IsBooted() override { return DdrAfp::IsBooted(); }

    std::string ActiveClipName() override { return DdrAfp::ActiveClip(); }

    bool HasRenderableScene([[maybe_unused]] uint32_t stream_id) override {
        return DdrAfp::IsBooted() && DdrAfp::LayerId() != 0;
    }

    void RenderFrame(float dt) override {
        if (DdrAfp::IsBooted()) DdrAfp::RenderFrame(dt);
    }

    void ReprobeVariantSlots([[maybe_unused]] uint32_t stream_id) override {}

    bool LoadScene(const std::string& mount_path, const std::string& ifs_path) override {
        auto& state = App::Global();
        if (g_avs.avs_fs_umount != nullptr) {
            g_avs.avs_fs_umount("/afp/packages");
            g_avs.avs_fs_umount("/data");
        }
        std::string const ddr_base = std::filesystem::path(mount_path).filename().string();
        std::string ddr_pkg = ddr_base;
        if (ddr_pkg.size() > 4 && ddr_pkg.ends_with(".ifs")) ddr_pkg.resize(ddr_pkg.size() - 4);
        state.UpdateLoadStage("Loading DDR package (AFP 2.13.7)");
        bool const ok = DdrAfp::LoadIfs(g_avs, g_avs_dll, mount_path, ddr_pkg);
        if (ok) {
            auto& cfg = state.MutConfig(ddr_base);
            cfg.filename = ddr_base;
            cfg.anim_names = DdrAfp::ClipNames();
            state.SetActiveIfs(ddr_base);
            App::Status st = state.GetStatus();
            st.current_ifs_path = ifs_path;
            st.stream_id = DdrAfp::LayerId();
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

    void UnloadScene() override {}

    void Shutdown() override { DdrAfp::Shutdown(); }

    uint32_t ActiveClipId([[maybe_unused]] uint32_t stream_id) override {
        return DdrAfp::LayerId();
    }

    bool HaveActiveClip([[maybe_unused]] uint32_t stream_id) override {
        return DdrAfp::LayerId() != 0;
    }

    bool ReadPlayhead([[maybe_unused]] uint32_t stream_id, uint32_t* cur, uint32_t* total,
                      uint32_t* raw_loop_count) override {
        return DdrAfp::ReadPlayhead(cur, total, raw_loop_count);
    }

    bool ReadSize([[maybe_unused]] uint32_t stream_id, uint32_t* w, uint32_t* h) override {
        return DdrAfp::ReadSize(w, h);
    }

    bool ReadRawLayerInfo([[maybe_unused]] uint32_t stream_id, [[maybe_unused]] uint32_t* cur,
                          [[maybe_unused]] uint32_t* total,
                          [[maybe_unused]] uint32_t* flags) override {
        return false;
    }

    bool ReadComplete([[maybe_unused]] const AfpFuncs& afp,
                      [[maybe_unused]] uint32_t stream_id) override {
        return false;
    }

    std::vector<Label> EnumerateLabels([[maybe_unused]] const AfpFuncs& afp,
                                       [[maybe_unused]] uint32_t stream_id) override {
        std::vector<Label> out;
        for (auto& l : DdrAfp::EnumerateLabels())
            out.push_back({.name = l.name, .frame = l.frame});
        return out;
    }

    void SetPaused([[maybe_unused]] const AfpFuncs& afp, bool paused) override {
        DdrAfp::SetPaused(paused);
    }

    bool SeekFrame([[maybe_unused]] const AfpFuncs& afp, int frame) override {
        return DdrAfp::SeekFrame(frame);
    }

    bool GotoLabel([[maybe_unused]] const AfpFuncs& afp, const std::string& name) override {
        return DdrAfp::GotoLabel(name);
    }

    void SwitchAnimation(const std::string& name,
                         [[maybe_unused]] const std::string& alt_name) override {
        LOG("Main", "DDR switch-clip requested: '%s'", name.c_str());
        App::Status st = App::Global().GetStatus();
        if (DdrAfp::SwitchClip(name)) {
            st.stream_id = DdrAfp::LayerId();
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

    RootRedrive MaybeRedriveRootLoop([[maybe_unused]] uint32_t stream_id, [[maybe_unused]] int cur,
                                     [[maybe_unused]] int total,
                                     [[maybe_unused]] int loops) override {
        return {};
    }

    bool SetGlobalSpeed([[maybe_unused]] const AfpFuncs& afp,
                        [[maybe_unused]] float speed) override {
        return false;
    }

    bool SupportsLiveExtras() override { return false; }

    bool IsLegacyDdr() override { return true; }
};

ModernRuntime g_modern;
DdrRuntime g_ddr;
IGameRuntime* g_active = &g_modern;

}

void SelectRuntime(bool legacy_ddr) {
    g_active =
        legacy_ddr ? static_cast<IGameRuntime*>(&g_ddr) : static_cast<IGameRuntime*>(&g_modern);
}

IGameRuntime& Active() {
    return *g_active;
}

}
