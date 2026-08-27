#pragma once

#include <cstdint>
#include "state/commands.h"

#include <array>
#include <span>
#include <string>
#include <vector>

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
    PresetDumpDefaults,
    PresetExportJson,
    PresetValidate,
    RetiredFlag,
};

struct ToolCommand {
    ToolKind kind = ToolKind::None;
    std::string in_path;
    std::string arc_path;
    std::string out_path;
    int frames = 120;
    bool bg_transparent = App::ExportRequest{}.bg_transparent;
    bool bg_black = false;
    std::array<float, 3> bg_rgb = {App::ExportRequest{}.bg_r, App::ExportRequest{}.bg_g,
                                   App::ExportRequest{}.bg_b};
    std::string build;
    std::string preset_id;
    std::string json_path;
    std::vector<std::string> options;
    bool force = false;
    std::string retired;
};

[[nodiscard]] ToolCommand ParseToolCommand(std::span<const std::string> args);

}
