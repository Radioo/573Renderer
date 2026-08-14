#pragma once

#include <cstdint>
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
};

[[nodiscard]] ToolCommand ParseToolCommand(std::span<const std::string> args);

}
