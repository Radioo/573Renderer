#include "customize_extract.h"
#include "formats/ddr_arc.h"
#include "support/log.h"

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

namespace CustomizeExtract {
namespace {

namespace fs = std::filesystem;

std::mutex g_mu;
Status g_status;
std::atomic<bool> g_running{false};

void Publish(const Status& s) {
    std::scoped_lock const lk(g_mu);
    g_status = s;
}

struct Mapped {
    std::string subdir;
    std::string filename;
};

bool StartsWith(const std::string& s, const char* p) {
    size_t const n = std::strlen(p);
    return s.size() >= n && std::memcmp(s.data(), p, n) == 0;
}
bool EndsWith(const std::string& s, const char* p) {
    size_t const n = std::strlen(p);
    return s.size() >= n && std::memcmp(s.data() + s.size() - n, p, n) == 0;
}

bool NormalizeId(const std::string& d, std::string& out) {
    if (d.empty()) return false;
    for (char const c : d)
        if (std::isdigit((unsigned char)c) == 0) return false;
    long const v = std::strtol(d.c_str(), nullptr, 10);
    out = std::to_string(v);
    return true;
}

bool TryMiddle(const std::string& name, size_t plen, size_t slen, const char* subdir,
               const char* suffix_out, Mapped& m) {
    if (name.size() <= plen + slen) return false;
    std::string const mid = name.substr(plen, name.size() - plen - slen);
    std::string id;
    if (!NormalizeId(mid, id)) return false;
    m.subdir = subdir;
    m.filename = id + suffix_out;
    return true;
}

bool ParseAssetName(const std::string& name, Mapped& m) {
    if (StartsWith(name, "appeal_board_") && EndsWith(name, "_result"))
        return TryMiddle(name, 13, 7, "appeal_boards", ".png", m);
    if (StartsWith(name, "character_") && EndsWith(name, "_1p"))
        return TryMiddle(name, 10, 3, "characters", "_left.png", m);
    if (StartsWith(name, "character_") && EndsWith(name, "_2p"))
        return TryMiddle(name, 10, 3, "characters", "_right.png", m);
    if (StartsWith(name, "lane_cover_single_"))
        return TryMiddle(name, 18, 0, "lane_covers_sp", ".png", m);
    if (StartsWith(name, "lane_cover_double_"))
        return TryMiddle(name, 18, 0, "lane_covers_dp", ".png", m);
    if (StartsWith(name, "lane_single_"))
        return TryMiddle(name, 12, 0, "lane_backgrounds_sp", ".png", m);
    if (StartsWith(name, "lane_double_"))
        return TryMiddle(name, 12, 0, "lane_backgrounds_dp", ".png", m);
    return false;
}

uint32_t Crc32(const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
                c = ((c & 1) != 0U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++)
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFU;
}

void PutU32BE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

void PutChunk(std::vector<uint8_t>& out, const char type[4], const uint8_t* data, size_t len) {
    PutU32BE(out, (uint32_t)len);
    size_t const crc_start = out.size();
    out.insert(out.end(), type, type + 4);
    if (len != 0U) out.insert(out.end(), data, data + len);
    PutU32BE(out, Crc32(out.data() + crc_start, 4 + len));
}

bool MinifyPng(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    static const uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (in.size() < 8 || std::memcmp(in.data(), kSig, 8) != 0) return false;

    out.clear();
    out.insert(out.end(), kSig, kSig + 8);

    std::vector<uint8_t> idat;
    bool saw_ihdr = false;
    size_t i = 8;
    while (i + 8 <= in.size()) {
        uint32_t const len = ((uint32_t)in[i] << 24) | ((uint32_t)in[i + 1] << 16) |
                             ((uint32_t)in[i + 2] << 8) | (uint32_t)in[i + 3];
        const char* type = reinterpret_cast<const char*>(&in[i + 4]);
        size_t const data_pos = i + 8;
        if (data_pos + (size_t)len + 4 > in.size()) break;
        const uint8_t* data = &in[data_pos];

        if (std::memcmp(type, "IHDR", 4) == 0) {
            PutChunk(out, "IHDR", data, len);
            saw_ihdr = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            PutChunk(out, "PLTE", data, len);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            PutChunk(out, "tRNS", data, len);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + len);
        }

        i = data_pos + len + 4;
        if (std::memcmp(type, "IEND", 4) == 0) break;
    }

    if (!saw_ihdr || idat.empty()) return false;
    PutChunk(out, "IDAT", idat.data(), idat.size());
    PutChunk(out, "IEND", nullptr, 0);
    return true;
}

std::vector<uint8_t> ReadAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

bool WriteAll(const fs::path& p, const uint8_t* data, size_t len) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    if (!o) return false;
    if (len != 0U) o.write(reinterpret_cast<const char*>(data), (std::streamsize)len);
    return o.good();
}

std::vector<uint8_t> ExtractPngFromArc(const fs::path& arc) {
    std::vector<uint8_t> bytes = ReadAll(arc);
    if (bytes.empty()) return {};
    DdrArc::Toc toc;
    if (!DdrArc::ParseToc(bytes, toc) || toc.entries.empty()) return {};
    const DdrArc::Entry* hit = nullptr;
    for (const DdrArc::Entry& e : toc.entries) {
        if (e.name.size() < 4) continue;
        std::string tail = e.name.substr(e.name.size() - 4);
        for (char& c : tail)
            c = (char)std::tolower((unsigned char)c);
        if (tail == ".png") {
            hit = &e;
            break;
        }
    }
    if (hit == nullptr) hit = toc.entries.data();
    return DdrArc::DecompressEntry(bytes, *hit);
}

struct Job {
    fs::path arc;
    fs::path dst;
};

std::vector<Job> CollectJobs(const fs::path& root, const fs::path& out_root, Status& st,
                             long long& scanned) {
    std::vector<Job> jobs;
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
            st.done = (int)jobs.size();
            st.current = it->path().lexically_relative(root).string();
            Publish(st);
        }
        if (it->is_directory(ec)) {
            if (it->path().filename() == "customize_assets") it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        std::string const fn = it->path().filename().string();
        std::string ext = it->path().extension().string();
        for (char& c : ext)
            c = (char)std::tolower((unsigned char)c);
        if (ext != ".arc") continue;
        Mapped m;
        if (!ParseAssetName(fn.substr(0, fn.size() - 4), m)) continue;
        jobs.push_back({.arc = it->path(), .dst = out_root / m.subdir / m.filename});
    }
    return jobs;
}

void ProcessJob(const Job& j, Status& st) {
    std::error_code ec;
    fs::create_directories(j.dst.parent_path(), ec);
    std::vector<uint8_t> const png = ExtractPngFromArc(j.arc);
    std::vector<uint8_t> mini;
    bool const minified = !png.empty() && MinifyPng(png, mini) && mini.size() <= png.size();

    const std::vector<uint8_t>& chosen = minified ? mini : png;
    bool const wrote = !chosen.empty() && WriteAll(j.dst, chosen.data(), chosen.size());

    if (wrote) {
        st.written++;
        st.bytes_in += (long long)png.size();
        st.bytes_out += (long long)chosen.size();
        if (minified && mini.size() < png.size()) st.optimized++;
    } else {
        st.failed++;
        LOG("Customize", "  FAILED (no PNG in arc?): %s", j.arc.string().c_str());
    }
}

void Run(std::string folder) {
    Status st;
    st.running = true;

    std::string clean = std::move(folder);
    while (!clean.empty() && (clean.back() == '/' || clean.back() == '\\'))
        clean.pop_back();
    fs::path const root(clean);
    fs::path const out_root = root / "customize_assets";
    st.output_dir = out_root.string();
    Publish(st);

    std::error_code ec;
    if (clean.empty() || !fs::is_directory(root, ec)) {
        st.running = false;
        st.finished = true;
        st.error = "not a folder: " + clean;
        Publish(st);
        LOG("Customize", "abort: %s", st.error.c_str());
        g_running = false;
        return;
    }

    st.current = "Scanning for customize .arc files...";
    Publish(st);

    long long scanned = 0;
    std::vector<Job> const jobs = CollectJobs(root, out_root, st, scanned);
    st.total = (int)jobs.size();
    st.done = 0;
    st.current.clear();
    Publish(st);
    LOG("Customize", "scanned %lld entries, found %d customize .arc under '%s' -> '%s'", scanned,
        st.total, clean.c_str(), out_root.string().c_str());

    for (const Job& j : jobs) {
        st.current = j.arc.filename().string();
        Publish(st);
        ProcessJob(j, st);
        st.done++;
        Publish(st);
    }

    st.running = false;
    st.finished = true;
    st.current.clear();
    Publish(st);
    LOG("Customize", "done: %d written (%d optimized, %d failed), %lld -> %lld bytes -> %s",
        st.written, st.optimized, st.failed, st.bytes_in, st.bytes_out, out_root.string().c_str());
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
