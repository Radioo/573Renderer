#include "render_loop_requests.h"

#include "app_globals.h"
#include "backend/backend.h"
#include "export.h"
#include "game_runtime.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <string>
#include <variant>

namespace {

void HandleLoadContent(const App::Cmd::LoadContent& cmd) {
    LOG("Main", "Hot-swap requested: %s", cmd.path.c_str());
    App::Global().BeginLoad(cmd.path);
    App::Global().UpdateLoadStage("Unloading previous IFS");
    Backend::Active()->UnloadContent();
    App::Global().SetActiveIfs("");
    App::Status st = App::Global().GetStatus();
    st.current_ifs_path.clear();
    st.stream_id = Runtime::kModernNoStream;
    st.scene_loaded = false;
    st.playing_animation.clear();
    st.active_label.clear();
    st.label_playback_active = false;
    st.overlay_ifs.clear();
    st.last_error.clear();
    App::Global().SetStatus(st);

    if (!Backend::Active()->LoadContent(cmd.path, cmd.from_arc)) {
        App::Status err = App::Global().GetStatus();
        if (err.last_error.empty()) err.last_error = "Failed to load " + cmd.path;
        App::Global().SetStatus(err);
    }
}

struct AppCommandVisitor {
    void operator()(const App::Cmd::BootGame& cmd) const {
        LOG("Main", "BootGame command ignored after boot (dir='%s')", cmd.game_dir.c_str());
    }
    void operator()(const App::Cmd::LoadContent& cmd) const {
        if (!cmd.path.empty()) HandleLoadContent(cmd);
    }
    void operator()(const App::Cmd::StartExport& cmd) const {
        Export::HandleStartRequest(cmd.req, g_d3d);
    }
    void operator()([[maybe_unused]] const App::Cmd::CancelExport& cmd) const {
        Export::HandleCancelRequest(g_d3d);
    }
    void operator()(const App::Cmd::BackendCommand& cmd) const {
        Backend::Active()->HandleCommand(cmd.payload);
    }
};

}

void DispatchAppCommand(const App::Command& cmd) {
    std::visit(AppCommandVisitor{}, cmd);
}
