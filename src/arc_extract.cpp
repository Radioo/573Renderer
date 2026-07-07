#include "arc_extract.h"
#include "formats/ddr_arc.h"
#include "support/log.h"

#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace ArcExtract {
namespace {

namespace fs = std::filesystem;

std::mutex g_mu;
Status g_status;
std::atomic<bool> g_running{false};

void Publish(const Status& s) {
    std::scoped_lock const lk(g_mu);
    g_status = s;
}

std::string EntryBasename(const std::string& name) {
    size_t const cut = name.find_last_of("/\\");
    std::string base = (cut == std::string::npos) ? name : name.substr(cut + 1);
    if (base.empty() || base == "." || base == "..") base = "entry";
    return base;
}

std::string ArcStem(const fs::path& arc) {
    std::string stem = arc.filename().string();
    if (stem.size() >= 4) {
        std::string tail = stem.substr(stem.size() - 4);
        for (char& c : tail)
            c = (char)std::tolower((unsigned char)c);
        if (tail == ".arc") stem.resize(stem.size() - 4);
    }
    return stem;
}

std::vector<uint8_t> ReadAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

fs::path UniquePath(const fs::path& desired) {
    std::error_code ec;
    if (!fs::exists(desired, ec)) return desired;
    fs::path const dir = desired.parent_path();
    std::string const stem = desired.stem().string();
    std::string const ext = desired.extension().string();
    for (int n = 2; n < 100000; n++) {
        std::string cand_name = stem;
        cand_name += "_";
        cand_name += std::to_string(n);
        cand_name += ext;
        fs::path cand = dir / cand_name;
        if (!fs::exists(cand, ec)) return cand;
    }
    return desired;
}

std::vector<fs::path> CollectArcs(const fs::path& root, Status& st, long long& scanned) {
    std::vector<fs::path> arcs;
    std::error_code ec;
    for (fs::recursive_directory_iterator
             it(root, fs::directory_options::skip_permission_denied, ec),
         end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (((++scanned) & 511) == 0) {
            st.done_arcs = (int)arcs.size();
            st.current = it->path().lexically_relative(root).string();
            Publish(st);
        }
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        for (char& c : ext)
            c = (char)std::tolower((unsigned char)c);
        if (ext == ".arc") arcs.push_back(it->path());
    }
    return arcs;
}

void WriteArcEntries(const std::vector<uint8_t>& bytes, const DdrArc::Toc& toc,
                     const fs::path& arc_dir, const std::string& arc_name, Status& st) {
    for (const DdrArc::Entry& e : toc.entries) {
        std::vector<uint8_t> data = DdrArc::DecompressEntry(bytes, e);
        if (data.empty() && e.decomp_size != 0) {
            LOG("ArcExtract", "  entry decompress failed: %s in %s", e.name.c_str(),
                arc_name.c_str());
            continue;
        }
        fs::path const dst = UniquePath(arc_dir / EntryBasename(e.name));
        std::ofstream o(dst, std::ios::binary | std::ios::trunc);
        if (!o) {
            LOG("ArcExtract", "  write failed: %s", dst.string().c_str());
            continue;
        }
        if (!data.empty())
            o.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        st.entries_written++;
    }
}

void ExtractOneArc(const fs::path& arc, const fs::path& root, const fs::path& out_root,
                   Status& st) {
    std::vector<uint8_t> const bytes = ReadAll(arc);
    DdrArc::Toc toc;
    if (bytes.empty() || !DdrArc::ParseToc(bytes, toc)) {
        st.failed_arcs++;
        LOG("ArcExtract", "skip (not a valid .arc): %s", arc.string().c_str());
        return;
    }

    std::error_code ec;
    fs::path rel_dir = fs::relative(arc.parent_path(), root, ec);
    if (ec) {
        ec.clear();
        rel_dir.clear();
    }
    fs::path arc_dir = out_root;
    if (!rel_dir.empty() && rel_dir != fs::path(".")) arc_dir /= rel_dir;
    arc_dir /= ArcStem(arc);
    fs::create_directories(arc_dir, ec);

    WriteArcEntries(bytes, toc, arc_dir, arc.filename().string(), st);
}

void Run(std::string folder) {
    Status st;
    st.running = true;

    std::string clean = std::move(folder);
    while (!clean.empty() && (clean.back() == '/' || clean.back() == '\\'))
        clean.pop_back();
    fs::path const root(clean);
    fs::path const out_root(clean + "_extracted");
    st.output_dir = out_root.string();
    Publish(st);

    std::error_code ec;
    if (clean.empty() || !fs::is_directory(root, ec)) {
        st.running = false;
        st.finished = true;
        st.error = "not a folder: " + clean;
        Publish(st);
        LOG("ArcExtract", "abort: %s", st.error.c_str());
        g_running = false;
        return;
    }

    st.current = "Scanning for .arc files...";
    Publish(st);
    long long scanned = 0;
    std::vector<fs::path> const arcs = CollectArcs(root, st, scanned);
    st.total_arcs = (int)arcs.size();
    st.done_arcs = 0;
    st.current.clear();
    Publish(st);
    LOG("ArcExtract", "scanned %lld entries, found %d .arc under '%s' -> '%s'", scanned,
        st.total_arcs, clean.c_str(), out_root.string().c_str());

    fs::create_directories(out_root, ec);

    for (const fs::path& arc : arcs) {
        st.current = arc.filename().string();
        Publish(st);
        ExtractOneArc(arc, root, out_root, st);
        st.done_arcs++;
        Publish(st);
    }

    st.running = false;
    st.finished = true;
    st.current.clear();
    Publish(st);
    LOG("ArcExtract", "done: wrote %d files from %d arcs (%d failed) -> %s", st.entries_written,
        st.done_arcs - st.failed_arcs, st.failed_arcs, out_root.string().c_str());
    g_running = false;
}

}

void Start(const std::string& folder) {
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) return;
    std::thread(Run, folder).detach();
}

bool IsRunning() {
    return g_running.load();
}

Status GetStatus() {
    std::scoped_lock const lk(g_mu);
    return g_status;
}

}
