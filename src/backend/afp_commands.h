#pragma once

#include "qpro_model.h"
#include "state/commands.h"

#include <string>
#include <utility>
#include <variant>

namespace AfpCmd {

struct SwitchAnimation {
    std::string name;
    std::string label;
};

struct GotoLabel {
    std::string name;
};

struct SeekFrame {
    int frame = 0;
};

struct SetPaused {
    bool paused = false;
};

struct ToggleCompanion {
    int index = -1;
};

struct ForceReplay {};

struct QproStartScan {};

struct QproStartExtract {
    std::string out_dir;
    QproModel::PartSelection part_sel;
    QproModel::CategorySel parts;
    int fps = 60;
    bool hue_scope = true;
};

using Any = std::variant<SwitchAnimation, GotoLabel, SeekFrame, SetPaused, ToggleCompanion,
                         ForceReplay, QproStartScan, QproStartExtract>;

inline App::Command Wrap(Any cmd) {
    return App::Cmd::BackendCommand{.payload = std::move(cmd)};
}

}
