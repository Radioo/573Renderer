#include "cli/tool_command.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

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

ToolCommand ParseGc2dSheet(std::span<const std::string> args, std::size_t i) {
    ToolCommand c;
    c.kind = ToolKind::Gc2dSheet;
    c.in_path = args[i + 1];
    c.out_path = (i + 2 < args.size() && args[i + 2][0] != '-') ? args[i + 2] : "gc2d_sheet";
    c.frames = 0;
    if (i + 3 < args.size() && args[i + 3][0] != '-') c.frames = ParseIntAtoiLike(args[i + 3]);
    return c;
}

std::vector<std::string> Positionals(std::span<const std::string> args, std::size_t i) {
    std::vector<std::string> out;
    for (std::size_t at = i + 1; at < args.size(); at++) {
        if (args[at].empty() || args[at][0] == '-') break;
        out.push_back(args[at]);
    }
    return out;
}

ToolCommand ParsePresetJob(std::span<const std::string> args, std::size_t i, ToolKind kind) {
    ToolCommand c;
    c.kind = kind;
    const std::size_t json = FindFlagWithValue(args, "--preset-json");
    if (json < args.size()) c.json_path = args[json + 1];

    const std::vector<std::string> positional = Positionals(args, i);
    std::size_t at = 0;
    c.in_path = (at < positional.size()) ? positional[at++] : "";
    if (c.json_path.empty() && at < positional.size()) c.preset_id = positional[at++];
    c.out_path = (at < positional.size()) ? positional[at++] : "preset_out.png";
    c.frames = (at < positional.size()) ? ParseIntAtoiLike(positional[at++]) : 1;
    for (std::size_t flag = 0; flag + 1 < args.size(); flag++) {
        if (args[flag] == "--preset-option") c.options.push_back(args[flag + 1]);
    }
    c.force = std::ranges::any_of(args, [](const std::string& arg) { return arg == "--force"; });
    const std::size_t bg = FindFlagWithValue(args, "--export-bg");
    if (bg < args.size()) {
        c.bg_transparent = (args[bg + 1] == "transparent");
        if (!c.bg_transparent) {
            c.bg_black = (args[bg + 1] == "0,0,0");
            std::size_t cursor = 0;
            const std::string& spec = args[bg + 1];
            for (float& channel : c.bg_rgb) {
                if (cursor >= spec.size()) break;
                const std::size_t comma = spec.find(',', cursor);
                channel = (float)ParseIntAtoiLike(spec.substr(cursor, comma - cursor)) / 255.0F;
                if (comma == std::string::npos) break;
                cursor = comma + 1;
            }
        }
    }
    return c;
}

ToolCommand ParsePresetExportJson(std::span<const std::string> args, std::size_t i) {
    ToolCommand c;
    c.kind = ToolKind::PresetExportJson;
    const std::vector<std::string> positional = Positionals(args, i);
    std::size_t at = 0;
    c.build = (at < positional.size()) ? positional[at++] : "";
    c.preset_id = (at < positional.size()) ? positional[at++] : "";
    if (at < positional.size()) {
        c.out_path = positional[at];
    } else if (!c.preset_id.empty()) {
        c.out_path = c.preset_id + ".json";
    }
    return c;
}

ToolCommand ParseOutDir(std::span<const std::string> args, std::size_t i, ToolKind kind) {
    ToolCommand c;
    c.kind = kind;
    const std::vector<std::string> positional = Positionals(args, i);
    if (!positional.empty()) c.out_path = positional.front();
    return c;
}

ToolCommand Retired(const std::string& flag) {
    ToolCommand c;
    c.kind = ToolKind::RetiredFlag;
    c.retired = flag;
    return c;
}

bool Mentions(std::span<const std::string> args, const std::string& flag) {
    return std::ranges::any_of(args, [&flag](const std::string& arg) { return arg == flag; });
}

}

ToolCommand ParseToolCommand(std::span<const std::string> args) {
    if (Mentions(args, "--preset-tweaks")) return Retired("--preset-tweaks");
    std::size_t i = FindFlagWithValue(args, "--preset-dump-defaults");
    if (i < args.size()) return ParseOutDir(args, i, ToolKind::PresetDumpDefaults);
    i = FindFlagWithValue(args, "--preset-export-json");
    if (i < args.size()) return ParsePresetExportJson(args, i);
    i = FindFlagWithValue(args, "--preset-validate");
    if (i < args.size()) return ParseSinglePath(args, i, ToolKind::PresetValidate);
    i = FindFlagWithValue(args, "--preset-export");
    if (i < args.size()) return ParsePresetJob(args, i, ToolKind::PresetExport);
    i = FindFlagWithValue(args, "--preset-test");
    if (i < args.size()) return ParsePresetJob(args, i, ToolKind::PresetTest);
    i = FindFlagWithValue(args, "--gc2d-sheet");
    if (i < args.size()) return ParseGc2dSheet(args, i);
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
