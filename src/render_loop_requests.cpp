#include "render_loop_requests.h"

#include "afp_boot.h"
#include "app_globals.h"
#include "boot.h"
#include "export.h"
#include "game_runtime.h"
#include "qpro_extract.h"
#include "qpro_scan.h"
#include "render_live.h"
#include "state/app_state.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

namespace {

void HandleHotSwap(const App::Request& req) {
    LOG("Main", "Hot-swap requested: %s", req.ifs_path.c_str());
    App::Global().BeginLoad(req.ifs_path);
    App::Global().UpdateLoadStage("Unloading previous IFS");
    Runtime::Active().UnloadScene();
    App::Global().SetActiveIfs("");
    App::Status st = App::Global().GetStatus();
    st.current_ifs_path.clear();
    st.stream_id = 0xFFFFFFFC;
    st.playing_animation.clear();
    st.active_label.clear();
    st.label_playback_active = false;
    st.last_error.clear();
    App::Global().SetStatus(st);

    if (!MountAndLoadIfs(req.ifs_path, req.ifs_from_arc)) {
        App::Status err = App::Global().GetStatus();
        err.last_error = "Failed to load " + req.ifs_path;
        App::Global().SetStatus(err);
    }
}

void HandleQproExtract(const App::Request& req) {
    QproExtract::Options o;
    o.game_dir = App::Global().GameDir();
    o.out_dir = req.qpro_out_dir;
    o.fps = req.qpro_fps;
    o.parts = req.qpro_parts;
    o.part_sel = req.qpro_part_sel;
    QproExtract::SetHueScopeEnabled(req.qpro_hue_scope);
    QproExtract::Run(o);
}

void HandleForceReplay() {
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

void HandleGotoLabel(const App::Request& req) {
    RenderLive::Inspect::GotoLabel(g_afp, req.goto_label_name);
    App::Status st = App::Global().GetStatus();
    st.active_label = req.goto_label_name;
    st.label_playback_active = !req.goto_label_name.empty();
    App::Global().SetStatus(st);
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

void HandleToggleCompanion(const App::Request& req) {
    auto active = App::Global().ActiveIfs();
    if (active.empty()) {
        LOG("Main", "toggle_companion: no active IFS, ignoring");
        return;
    }
    auto& cfg = App::Global().MutConfig(active);
    int const idx = req.companion_index;
    if (idx < 0 || std::cmp_greater_equal(idx, cfg.companions.size())) {
        LOG("Main",
            "toggle_companion: index %d out of "
            "range (size=%zu)",
            idx, cfg.companions.size());
        return;
    }
    bool const will_load = !cfg.companions[idx].loaded;

    App::Global().BeginLoad(cfg.companions[idx].display_name);
    App::Global().UpdateLoadStage(will_load ? "Loading companion IFS" : "Unloading companion");

    UnloadAllCompanions(cfg);
    if (will_load) LoadCompanionAt(cfg, idx);
    ReplayMasterForBindings();
    App::Global().EndLoad();
}

}

void DispatchAppRequest(const App::Request& req_obj) {
    const App::Request* req = &req_obj;
    if (req->load_new_ifs && !req->ifs_path.empty()) {
        HandleHotSwap(*req);
    } else if (req->start_export) {
        Export::HandleStartRequest(*req, g_engine, g_d3d);
    } else if (req->cancel_export) {
        Export::HandleCancelRequest(g_d3d);
    } else if (req->start_qpro_extract) {
        HandleQproExtract(*req);
    } else if (req->start_qpro_scan) {
        QproExtract::RunScan(App::Global().GameDir());
    } else if (req->force_replay) {
        HandleForceReplay();
    } else if (req->switch_animation && !req->animation_name.empty()) {
        Runtime::Active().SwitchAnimation(req->animation_name, req->animation_label);
    } else if (req->goto_label) {
        HandleGotoLabel(*req);
    } else if (req->seek_frame) {
        RenderLive::HandleSeekRequest(*req, g_afp);
    } else if (req->set_paused) {
        RenderLive::HandlePauseRequest(*req, g_afp);
    } else if (req->toggle_companion && req->companion_index >= 0) {
        HandleToggleCompanion(*req);
    }
}
