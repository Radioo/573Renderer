#include "arc_extract.h"
#include "formats/ddr_arc.h"
#include "support/folder_job.h"
#include "support/log.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ArcExtract {
namespace {

namespace fs = std::filesystem;

Support::FolderJob<Status> g_job;

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
    scanned = Support::ScanFolder(
        root,
        [&](const fs::path& cur) {
            st.done_arcs = (int)arcs.size();
            st.current = cur.lexically_relative(root).string();
            g_job.Publish(st);
        },
        [&](auto& it) {
            std::error_code ec;
            if (!it->is_regular_file(ec)) return;
            std::string ext = it->path().extension().string();
            for (char& c : ext)
                c = (char)std::tolower((unsigned char)c);
            if (ext == ".arc") arcs.push_back(it->path());
        });
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
    std::vector<uint8_t> const bytes = Support::ReadFileBytes(arc);
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

    std::string const clean = Support::StripTrailingSlashes(std::move(folder));
    fs::path const root(clean);
    fs::path const out_root(clean + "_extracted");
    st.output_dir = out_root.string();
    g_job.Publish(st);

    std::error_code ec;
    if (clean.empty() || !fs::is_directory(root, ec)) {
        st.running = false;
        st.finished = true;
        st.error = "not a folder: " + clean;
        g_job.Publish(st);
        LOG("ArcExtract", "abort: %s", st.error.c_str());
        g_job.Finish();
        return;
    }

    st.current = "Scanning for .arc files...";
    g_job.Publish(st);
    long long scanned = 0;
    std::vector<fs::path> const arcs = CollectArcs(root, st, scanned);
    st.total_arcs = (int)arcs.size();
    st.done_arcs = 0;
    st.current.clear();
    g_job.Publish(st);
    LOG("ArcExtract", "scanned %lld entries, found %d .arc under '%s' -> '%s'", scanned,
        st.total_arcs, clean.c_str(), out_root.string().c_str());

    fs::create_directories(out_root, ec);

    for (const fs::path& arc : arcs) {
        st.current = arc.filename().string();
        g_job.Publish(st);
        ExtractOneArc(arc, root, out_root, st);
        st.done_arcs++;
        g_job.Publish(st);
    }

    st.running = false;
    st.finished = true;
    st.current.clear();
    g_job.Publish(st);
    LOG("ArcExtract", "done: wrote %d files from %d arcs (%d failed) -> %s", st.entries_written,
        st.done_arcs - st.failed_arcs, st.failed_arcs, out_root.string().c_str());
    g_job.Finish();
}

}

void Start(const std::string& folder) {
    g_job.Start(folder, Run);
}

bool IsRunning() {
    return g_job.IsRunning();
}

Status GetStatus() {
    return g_job.Get();
}

}
