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
};

struct ToolCommand {
    ToolKind kind = ToolKind::None;
    std::string in_path;
    std::string arc_path;
    std::string out_path;
    int frames = 120;
};

[[nodiscard]] ToolCommand ParseToolCommand(std::span<const std::string> args);

}
