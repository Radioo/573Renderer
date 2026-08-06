#include "render_loop_requests.h"

#include "app_globals.h"
#include "boot.h"
#include "export.h"
#include "game_runtime.h"
#include "qpro_extract.h"
#include "qpro_scan.h"
#include "render_live.h"
#include "state/afp_commands.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <any>
#include <string>
#include <variant>

namespace {

void HandleLoadContent(const App::Cmd::LoadContent& cmd) {
    LOG("Main", "Hot-swap requested: %s", cmd.path.c_str());
    App::Global().BeginLoad(cmd.path);
    App::Global().UpdateLoadStage("Unloading previous IFS");
    Runtime::Active().UnloadScene();
    App::Global().SetActiveIfs("");
    App::Status st = App::Global().GetStatus();
    st.current_ifs_path.clear();
    st.stream_id = Runtime::kModernNoStream;
    st.playing_animation.clear();
    st.active_label.clear();
    st.label_playback_active = false;
    st.last_error.clear();
    App::Global().SetStatus(st);

    if (!MountAndLoadIfs(cmd.path, cmd.from_arc)) {
        App::Status err = App::Global().GetStatus();
        err.last_error = "Failed to load " + cmd.path;
        App::Global().SetStatus(err);
    }
}

void HandleQproExtract(const AfpCmd::QproStartExtract& cmd) {
    QproExtract::Options o;
    o.game_dir = App::Global().GameDir();
    o.out_dir = cmd.out_dir;
    o.fps = cmd.fps;
    o.parts = cmd.parts;
    o.part_sel = cmd.part_sel;
    QproExtract::SetHueScopeEnabled(cmd.hue_scope);
    QproExtract::Run(o);
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
    void operator()(const AfpCmd::ToggleCompanion& cmd) const {
        if (cmd.index >= 0) Runtime::Active().ToggleCompanion(cmd.index);
    }
    void operator()([[maybe_unused]] const AfpCmd::ForceReplay& cmd) const {
        Runtime::Active().ForceReplayMaster();
    }
    void operator()([[maybe_unused]] const AfpCmd::QproStartScan& cmd) const {
        QproExtract::RunScan(App::Global().GameDir());
    }
    void operator()(const AfpCmd::QproStartExtract& cmd) const { HandleQproExtract(cmd); }
};

struct AppCommandVisitor {
    void operator()(const App::Cmd::BootGame& cmd) const {
        LOG("Main", "BootGame command ignored after boot (dir='%s')", cmd.game_dir.c_str());
    }
    void operator()(const App::Cmd::LoadContent& cmd) const {
        if (!cmd.path.empty()) HandleLoadContent(cmd);
    }
    void operator()(const App::Cmd::StartExport& cmd) const {
        Export::HandleStartRequest(cmd.req, g_engine, g_d3d);
    }
    void operator()([[maybe_unused]] const App::Cmd::CancelExport& cmd) const {
        Export::HandleCancelRequest(g_d3d);
    }
    void operator()(const App::Cmd::BackendCommand& cmd) const {
        const auto* afp = std::any_cast<AfpCmd::Any>(&cmd.payload);
        if (afp == nullptr) {
            LOG("Main", "BackendCommand with unknown payload type dropped");
            return;
        }
        std::visit(AfpCommandVisitor{}, *afp);
    }
};

}

void DispatchAppCommand(const App::Command& cmd) {
    std::visit(AppCommandVisitor{}, cmd);
}
