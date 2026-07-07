#include "qpro_scan.h"

#include "qpro_dll.h"
#include "support/log.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>
#include <utility>

namespace QproExtract {
namespace {

namespace fs = std::filesystem;

std::mutex g_scan_mu;
ScanResult g_scan;

fs::path IfsPathOf(const std::string& game_dir, const std::string& ifs) {
    return fs::path(game_dir) / "data" / "graphic" / ifs;
}

std::string FileDate(const fs::path& p) {
    std::error_code ec;
    auto ft = fs::last_write_time(p, ec);
    if (ec) return {};
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ft);
    std::time_t const tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm{};
    if (localtime_s(&tm, &tt) != 0) return {};
    char buf[16];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm) == 0) return {};
    return buf;
}

}

void MarkScanRunning() {
    std::scoped_lock const lk(g_scan_mu);
    g_scan.running = true;
    g_scan.done = false;
    g_scan.error.clear();
}

ScanResult GetScanResult() {
    std::scoped_lock const lk(g_scan_mu);
    return g_scan;
}

void RunScan(const std::string& game_dir) {
    ScanResult r;
    r.running = true;

    QproDll::Parts const parts = QproDll::Read(game_dir);
    if (!parts.ok()) {
        std::scoped_lock const lk(g_scan_mu);
        g_scan.running = false;
        g_scan.done = true;
        g_scan.error = parts.error;
        g_scan.parts.clear();
        g_scan.generation++;
        return;
    }

    for (int c = 0; c < (int)QproDll::Category::Count; ++c) {
        const std::vector<QproDll::Part>& list = parts.of((QproDll::Category)c);
        const char* prefix = QproDll::Prefix((QproDll::Category)c);
        for (size_t i = 0; i < list.size(); ++i) {
            ScanPart sp;
            sp.cat = c;
            sp.idx = (int)i;
            sp.label = std::string(prefix) + "_" + std::to_string(i);
            sp.ifs = list[i].ifs;
            fs::path const p = IfsPathOf(game_dir, list[i].ifs);
            std::error_code ec;
            sp.exists = fs::is_regular_file(p, ec);
            sp.date = sp.exists ? FileDate(p) : std::string();
            r.parts.push_back(std::move(sp));
        }
    }

    LOG("QproScan", "scanned %zu parts from %s", r.parts.size(), parts.dll_path.c_str());

    std::scoped_lock const lk(g_scan_mu);
    g_scan.running = false;
    g_scan.done = true;
    g_scan.error.clear();
    g_scan.parts = std::move(r.parts);
    g_scan.generation++;
}

}
