#include "tool_commands.h"

#include "cli/tool_command.h"
#include "qpro_scan.h"
#include "support/log.h"
#include "afp_ddr_test.h"
#include "arc_extract.h"
#include "customize_extract.h"
#include "qpro_dll.h"

#include <fstream>
#include <ios>
#include <map>
#include <string>
#include <windows.h>

namespace ToolCommands {

namespace {

int RunDdrTest(const Cli::ToolCommand& cmd) {
    return DdrTest::Run(cmd.in_path, cmd.arc_path, cmd.out_path, cmd.frames);
}

int RunExtractArc(const Cli::ToolCommand& cmd) {
    ArcExtract::Start(cmd.in_path);
    int last_done = -1;
    while (ArcExtract::IsRunning()) {
        ArcExtract::Status const s = ArcExtract::GetStatus();
        if (s.total_arcs > 0 && s.done_arcs != last_done) {
            last_done = s.done_arcs;
            LOG("ArcExtract", "progress: %d/%d arcs, %d files written", s.done_arcs, s.total_arcs,
                s.entries_written);
        }
        Sleep(100);
    }
    ArcExtract::Status const s = ArcExtract::GetStatus();
    return s.error.empty() ? 0 : 2;
}

int RunExtractCustomize(const Cli::ToolCommand& cmd) {
    CustomizeExtract::Start(cmd.in_path);
    int last_done = -1;
    while (CustomizeExtract::IsRunning()) {
        CustomizeExtract::Status const s = CustomizeExtract::GetStatus();
        if (s.total > 0 && s.done != last_done) {
            last_done = s.done;
            LOG("Customize", "progress: %d/%d images", s.done, s.total);
        }
        Sleep(100);
    }
    CustomizeExtract::Status const s = CustomizeExtract::GetStatus();
    return s.error.empty() ? 0 : 2;
}

int RunExtractQproJson(const Cli::ToolCommand& cmd) {
    QproDll::Parts const parts = QproDll::Read(cmd.in_path);
    if (!parts.ok()) {
        LOG("Qpro", "ERROR: %s", parts.error.c_str());
        return 2;
    }
    std::string json = QproDll::ToJson(parts);
    std::ofstream f(cmd.out_path, std::ios::binary | std::ios::trunc);
    if (!f) {
        LOG("Qpro", "cannot write %s", cmd.out_path.c_str());
        return 2;
    }
    f.write(json.data(), (std::streamsize)json.size());
    for (int c = 0; c < (int)QproDll::Category::Count; ++c) {
        LOG("Qpro", "  %-10s %d", QproDll::JsonKey((QproDll::Category)c),
            (int)parts.of((QproDll::Category)c).size());
    }
    LOG("Qpro", "wrote %d parts -> %s (from %s)", parts.total(), cmd.out_path.c_str(),
        parts.dll_path.c_str());
    return 0;
}

int RunQproScan(const Cli::ToolCommand& cmd) {
    QproExtract::RunScan(cmd.in_path);
    QproExtract::ScanResult const sr = QproExtract::GetScanResult();
    if (!sr.error.empty()) {
        LOG("QproScan", "ERROR: %s", sr.error.c_str());
        return 2;
    }
    int cat_n[(int)QproDll::Category::Count] = {0};
    for (const auto& p : sr.parts)
        cat_n[p.cat]++;
    for (int c = 0; c < (int)QproDll::Category::Count; ++c)
        LOG("QproScan", "  %-10s %d", QproDll::JsonKey((QproDll::Category)c), cat_n[c]);
    std::map<std::string, int> by_date;
    for (const auto& p : sr.parts)
        by_date[p.date.empty() ? "(no date)" : p.date]++;
    for (const auto& kv : by_date)
        LOG("QproScan", "  date %-14s %d parts", kv.first.c_str(), kv.second);
    LOG("QproScan", "total %zu parts across %zu date group(s)", sr.parts.size(), by_date.size());
    return 0;
}

}

int Run(const Cli::ToolCommand& cmd) {
    switch (cmd.kind) {
    case Cli::ToolKind::DdrTest:
        return RunDdrTest(cmd);
    case Cli::ToolKind::ExtractArc:
        return RunExtractArc(cmd);
    case Cli::ToolKind::ExtractCustomize:
        return RunExtractCustomize(cmd);
    case Cli::ToolKind::ExtractQproJson:
        return RunExtractQproJson(cmd);
    case Cli::ToolKind::QproScan:
        return RunQproScan(cmd);
    case Cli::ToolKind::None:
    default:
        return 0;
    }
}

}
