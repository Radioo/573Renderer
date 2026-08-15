#pragma once

#include "preset/doc/preset_document.h"
#include "state/commands.h"

#include <memory>
#include <utility>
#include <variant>

namespace PresetCmd {

struct ReplaceDocument {
    std::shared_ptr<const Preset::Doc::Document> document;
};

struct Seek {
    int frame = 0;
};

struct SetPaused {
    bool paused = false;
};

struct SetLoop {
    bool loop = true;
};

struct SetOption {
    int option = 0;
    int choice = 0;
};

using Any = std::variant<ReplaceDocument, Seek, SetPaused, SetLoop, SetOption>;

inline App::Command Wrap(Any cmd) {
    return App::Cmd::BackendCommand{.payload = std::move(cmd)};
}

}
