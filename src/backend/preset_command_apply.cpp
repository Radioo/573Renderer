#include "backend/preset_command_apply.h"

#include "preset/preset_host.h"
#include "state/preset_commands.h"

#include <any>
#include <variant>

namespace Backend {

bool ApplyPresetCommand(const std::any& payload) {
    const auto* command = std::any_cast<PresetCmd::Any>(&payload);
    if (command == nullptr) return false;

    if (const auto* replace = std::get_if<PresetCmd::ReplaceDocument>(command)) {
        PresetHost::ReplaceDocument(replace->document);
        return true;
    }
    if (const auto* seek = std::get_if<PresetCmd::Seek>(command)) {
        PresetHost::Seek(seek->frame);
        return true;
    }
    if (const auto* paused = std::get_if<PresetCmd::SetPaused>(command)) {
        PresetHost::SetPaused(paused->paused);
        return true;
    }
    if (const auto* loop = std::get_if<PresetCmd::SetLoop>(command)) {
        PresetHost::SetLoop(loop->loop);
        return true;
    }
    if (const auto* preview = std::get_if<PresetCmd::PreviewLayer>(command)) {
        PresetHost::RequestPreview(preview->asset, preview->animation, preview->hidden_parts,
                                   preview->samples);
        return true;
    }
    const auto& option = std::get<PresetCmd::SetOption>(*command);
    PresetHost::SetOption(option.option, option.choice);
    return true;
}

}
