#pragma once

#include <cstdint>
#include "state/commands.h"

#include <array>
#include <span>
#include <string>

namespace Cli {

enum class ToolKind : std::uint8_t {
    None,
    DdrTest,
    ExtractArc,
    ExtractCustomize,
    ExtractQproJson,
    QproScan,
    Scene3dTest,
    Gc2dSheet,
    PresetTest,
    PresetExport,
};

struct ToolCommand {
    ToolKind kind = ToolKind::None;
    std::string in_path;
    std::string arc_path;
    std::string out_path;
    int frames = 120;
    int option = 0;
    bool bg_transparent = App::ExportRequest{}.bg_transparent;
    bool bg_black = false;
    std::array<float, 3> bg_rgb = {App::ExportRequest{}.bg_r, App::ExportRequest{}.bg_g,
                                   App::ExportRequest{}.bg_b};
    std::string tweaks_path;
};

[[nodiscard]] ToolCommand ParseToolCommand(std::span<const std::string> args);

}
