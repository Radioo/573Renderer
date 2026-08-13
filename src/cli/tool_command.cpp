#include "cli/tool_command.h"

#include <charconv>
#include <cstddef>
#include <memory>
#include <span>
#include <string>

namespace Cli {

namespace {

int ParseIntAtoiLike(const std::string& v) {
    int out = 0;
    std::from_chars(v.data(), std::to_address(v.end()), out);
    return out;
}

std::size_t FindFlagWithValue(std::span<const std::string> args, const std::string& flag) {
    for (std::size_t i = 0; i + 1 < args.size(); i++) {
        if (args[i] == flag) return i;
    }
    return args.size();
}

ToolCommand ParseDdrTest(std::span<const std::string> args, std::size_t i) {
    ToolCommand c;
    c.kind = ToolKind::DdrTest;
    c.in_path = args[i + 1];
    c.arc_path = (i + 2 < args.size()) ? args[i + 2] : "";
    c.out_path = (i + 3 < args.size()) ? args[i + 3] : "ddr_out.png";
    if (i + 4 < args.size()) c.frames = ParseIntAtoiLike(args[i + 4]);
    return c;
}

ToolCommand ParseSinglePath(std::span<const std::string> args, std::size_t i, ToolKind kind) {
    ToolCommand c;
    c.kind = kind;
    c.in_path = args[i + 1];
    return c;
}

ToolCommand ParseExtractQproJson(std::span<const std::string> args, std::size_t i) {
    ToolCommand c;
    c.kind = ToolKind::ExtractQproJson;
    c.in_path = args[i + 1];
    c.out_path = (i + 2 < args.size() && !args[i + 2].empty() && args[i + 2][0] != '-')
                     ? args[i + 2]
                     : "2dx_qpro.json";
    return c;
}

ToolCommand ParseScene3dTest(std::span<const std::string> args, std::size_t i) {
    ToolCommand c;
    c.kind = ToolKind::Scene3dTest;
    c.in_path = args[i + 1];
    c.out_path = (i + 2 < args.size() && args[i + 2][0] != '-') ? args[i + 2] : "scene3d_out.png";
    if (i + 3 < args.size() && args[i + 3][0] != '-') c.frames = ParseIntAtoiLike(args[i + 3]);
    return c;
}

ToolCommand ParsePresetJob(std::span<const std::string> args, std::size_t i, ToolKind kind) {
    ToolCommand c;
    c.kind = kind;
    c.in_path = args[i + 1];
    c.arc_path = (i + 2 < args.size() && args[i + 2][0] != '-') ? args[i + 2] : "";
    c.out_path = (i + 3 < args.size() && args[i + 3][0] != '-') ? args[i + 3] : "preset_out.png";
    c.frames = 1;
    if (i + 4 < args.size() && args[i + 4][0] != '-') c.frames = ParseIntAtoiLike(args[i + 4]);
    if (i + 5 < args.size() && args[i + 5][0] != '-') c.option = ParseIntAtoiLike(args[i + 5]);
    return c;
}

}

ToolCommand ParseToolCommand(std::span<const std::string> args) {
    std::size_t i = FindFlagWithValue(args, "--preset-export");
    if (i < args.size()) return ParsePresetJob(args, i, ToolKind::PresetExport);
    i = FindFlagWithValue(args, "--preset-test");
    if (i < args.size()) return ParsePresetJob(args, i, ToolKind::PresetTest);
    i = FindFlagWithValue(args, "--scene3d-test");
    if (i < args.size()) return ParseScene3dTest(args, i);
    i = FindFlagWithValue(args, "--ddr-test");
    if (i < args.size()) return ParseDdrTest(args, i);
    i = FindFlagWithValue(args, "--extract-arc");
    if (i < args.size()) return ParseSinglePath(args, i, ToolKind::ExtractArc);
    i = FindFlagWithValue(args, "--extract-customize");
    if (i < args.size()) return ParseSinglePath(args, i, ToolKind::ExtractCustomize);
    i = FindFlagWithValue(args, "--extract-qpro-json");
    if (i < args.size()) return ParseExtractQproJson(args, i);
    i = FindFlagWithValue(args, "--qpro-scan");
    if (i < args.size()) return ParseSinglePath(args, i, ToolKind::QproScan);
    return {};
}

}
