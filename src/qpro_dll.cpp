#include "qpro_dll.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <ios>
#include <iterator>
#include <system_error>
#include <unordered_set>
#include <vector>
#include <utility>

namespace QproDll {
namespace {

namespace fs = std::filesystem;

constexpr int kMinRun = 16;
constexpr uint64_t kMaxFlag = 0x10000;
constexpr size_t kMaxName = 96;

struct Section {
    uint32_t rva;
    uint32_t raw_off;
    uint32_t raw_size;
};

struct Image {
    std::vector<uint8_t> bytes;
    uint64_t imagebase = 0;
    std::vector<Section> sections;
    std::string error;
};

uint16_t RdU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
uint32_t RdU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint64_t RdU64(const uint8_t* p) {
    return (uint64_t)RdU32(p) | ((uint64_t)RdU32(p + 4) << 32);
}

std::vector<uint8_t> ReadFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

Image LoadImage(const std::string& dll_path) {
    Image img;
    img.bytes = ReadFile(dll_path);
    const std::vector<uint8_t>& b = img.bytes;
    if (b.size() < 0x40) {
        img.error = "file too small / unreadable: " + dll_path;
        return img;
    }
    if (b[0] != 'M' || b[1] != 'Z') {
        img.error = "not a PE (no MZ): " + dll_path;
        return img;
    }

    uint32_t const e_lfanew = RdU32(&b[0x3C]);
    if ((size_t)e_lfanew + 24 > b.size()) {
        img.error = "bad e_lfanew";
        return img;
    }
    if (std::memcmp(&b[e_lfanew], "PE\0\0", 4) != 0) {
        img.error = "bad PE signature";
        return img;
    }

    uint32_t const coff = e_lfanew + 4;
    uint16_t const nsec = RdU16(&b[coff + 2]);
    uint16_t const optsz = RdU16(&b[coff + 16]);
    uint32_t const opt = coff + 20;
    if ((size_t)opt + optsz > b.size()) {
        img.error = "truncated optional header";
        return img;
    }

    uint16_t const magic = RdU16(&b[opt]);
    if (magic == 0x20B) {
        img.imagebase = RdU64(&b[opt + 24]);
    } else if (magic == 0x10B) {
        img.imagebase = RdU32(&b[opt + 28]);
    } else {
        img.error = "unknown optional-header magic";
        return img;
    }

    uint32_t const sec0 = opt + optsz;
    for (uint16_t i = 0; i < nsec; ++i) {
        uint32_t const s = sec0 + ((uint32_t)i * 40);
        if ((size_t)s + 40 > b.size()) break;
        Section sec{};
        sec.rva = RdU32(&b[s + 12]);
        sec.raw_size = RdU32(&b[s + 16]);
        sec.raw_off = RdU32(&b[s + 20]);
        img.sections.push_back(sec);
    }
    return img;
}

int64_t VaToOff(const Image& img, uint64_t va) {
    if (va < img.imagebase) return -1;
    uint64_t const rva = va - img.imagebase;
    for (const Section& s : img.sections) {
        if (s.raw_size == 0) continue;
        if (rva >= s.rva && rva < (uint64_t)s.rva + s.raw_size) {
            uint64_t const off = (uint64_t)s.raw_off + (rva - s.rva);
            if (off + 1 <= img.bytes.size()) return (int64_t)off;
        }
    }
    return -1;
}

std::string ReadCStr(const Image& img, uint64_t va) {
    int64_t const off = VaToOff(img, va);
    if (off < 0) return {};
    std::string out;
    for (size_t i = 0; i <= kMaxName; ++i) {
        if ((size_t)off + i >= img.bytes.size()) return {};
        uint8_t const c = img.bytes[off + i];
        if (c == 0) return out;
        if (c < 0x20 || c > 0x7E) return {};
        out.push_back((char)c);
    }
    return {};
}

bool EndsWith(const std::string& s, const char* suf) {
    size_t const n = std::strlen(suf);
    return s.size() >= n && std::memcmp(s.data() + s.size() - n, suf, n) == 0;
}

int Classify(const std::string& ifs) {
    std::string stem = ifs.substr(0, ifs.size() - 4);
    size_t e = stem.size();
    while (e > 0 && stem[e - 1] >= '0' && stem[e - 1] <= '9')
        --e;
    const std::string base = stem.substr(0, e);
    if (EndsWith(base, "_body")) return (int)Category::Body;
    if (EndsWith(base, "_hand")) return (int)Category::Hand;
    if (EndsWith(base, "_face")) return (int)Category::Face;
    if (EndsWith(base, "_hair")) return (int)Category::Hair;
    if (EndsWith(base, "_head")) return (int)Category::Head;
    if (EndsWith(base, "_bg")) return (int)Category::Back;
    return -1;
}

struct Rec {
    uint64_t va;
    std::string ifs;
    uint8_t flag0;
};

bool ValidRecord(const Image& img, uint64_t q0, uint64_t q1, std::string& out) {
    if (q1 >= kMaxFlag) return false;
    std::string s = ReadCStr(img, q0);
    if (s.size() < 7) return false;
    if (!s.starts_with("qp_")) return false;
    if (!EndsWith(s, ".ifs")) return false;
    out = std::move(s);
    return true;
}

}

int Parts::total() const {
    int n = 0;
    for (const auto& cat : cats)
        n += (int)cat.size();
    return n;
}

const char* JsonKey(Category c) {
    switch (c) {
    case Category::Body:
        return "bodyParts";
    case Category::Hand:
        return "handParts";
    case Category::Face:
        return "faceParts";
    case Category::Hair:
        return "hairParts";
    case Category::Head:
        return "headParts";
    case Category::Back:
        return "backParts";
    default:
        return "";
    }
}

const char* Prefix(Category c) {
    switch (c) {
    case Category::Body:
        return "body";
    case Category::Hand:
        return "hand";
    case Category::Face:
        return "face";
    case Category::Hair:
        return "hair";
    case Category::Head:
        return "head";
    case Category::Back:
        return "back";
    default:
        return "";
    }
}

namespace {

struct RecordScan {
    std::vector<Rec> recs;
    std::unordered_set<uint64_t> present;
};

void ScanRecords(const Image& img, RecordScan& out) {
    for (const Section& s : img.sections) {
        if (s.raw_size < 16) continue;
        size_t end = (size_t)s.raw_off + s.raw_size;
        end = std::min(end, img.bytes.size());
        for (size_t off = s.raw_off; off + 16 <= end; off += 8) {
            uint64_t const q0 = RdU64(&img.bytes[off]);
            uint64_t const q1 = RdU64(&img.bytes[off + 8]);
            std::string name;
            if (!ValidRecord(img, q0, q1, name)) continue;
            uint64_t const va = img.imagebase + s.rva + (off - s.raw_off);
            out.recs.push_back({.va = va, .ifs = std::move(name), .flag0 = (uint8_t)(q1 & 0xFF)});
            out.present.insert(va);
        }
    }
}

std::unordered_set<uint64_t> KeepMinRuns(const RecordScan& scan) {
    std::unordered_set<uint64_t> kept;
    for (const Rec& r : scan.recs) {
        if (scan.present.contains(r.va - 16)) continue;
        size_t len = 0;
        for (uint64_t v = r.va; scan.present.contains(v); v += 16)
            ++len;
        if ((int)len < kMinRun) continue;
        for (uint64_t v = r.va; scan.present.contains(v); v += 16)
            kept.insert(v);
    }
    return kept;
}

void BinByCategory(const RecordScan& scan, const std::unordered_set<uint64_t>& kept, Parts& parts) {
    std::vector<std::pair<uint64_t, Part>> bins[(int)Category::Count];
    for (const Rec& r : scan.recs) {
        if (!kept.contains(r.va)) continue;
        int const cat = Classify(r.ifs);
        if (cat < 0) continue;
        bins[cat].emplace_back(r.va, Part{.ifs = r.ifs, .flag0 = r.flag0});
    }
    for (int c = 0; c < (int)Category::Count; ++c) {
        std::ranges::sort(bins[c], [](const auto& a, const auto& b) { return a.first < b.first; });
        for (auto& kv : bins[c])
            parts.cats[c].push_back(std::move(kv.second));
    }
}

}

Parts Read(const std::string& dll_or_game_dir) {
    Parts parts;

    fs::path const in(dll_or_game_dir);
    std::error_code ec;
    fs::path dll = in;
    if (fs::is_directory(in, ec)) dll = in / "modules" / "bm2dx.dll";
    parts.dll_path = dll.string();
    if (!fs::is_regular_file(dll, ec)) {
        parts.error = "bm2dx.dll not found at: " + parts.dll_path;
        return parts;
    }

    Image const img = LoadImage(parts.dll_path);
    if (!img.error.empty()) {
        parts.error = img.error;
        return parts;
    }

    RecordScan scan;
    ScanRecords(img, scan);
    std::unordered_set<uint64_t> const kept = KeepMinRuns(scan);
    BinByCategory(scan, kept, parts);

    int missing = 0;
    for (const auto& cat : parts.cats)
        if (cat.empty()) ++missing;
    if (missing == (int)Category::Count) parts.error = "no qpro arrays found (DLL layout changed?)";

    return parts;
}

namespace {

void AppendEscaped(std::string& out, const std::string& s) {
    for (char const c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
}

}

std::string ToJson(const Parts& p) {
    std::string o;
    o.reserve(((size_t)p.total() * 48) + 64);
    o += "{\n";
    for (int c = 0; c < (int)Category::Count; ++c) {
        const std::vector<Part>& list = p.cats[c];
        o += "    \"";
        o += JsonKey((Category)c);
        o += "\": ";
        if (list.empty()) {
            o += "[]";
        } else {
            o += "[\n";
            for (size_t i = 0; i < list.size(); ++i) {
                o += "        [\n            \"\",\n            \"";
                AppendEscaped(o, list[i].ifs);
                o += "\"\n        ]";
                o += (i + 1 < list.size()) ? ",\n" : "\n";
            }
            o += "    ]";
        }
        o += (c + 1 < (int)Category::Count) ? ",\n" : "\n";
    }
    o += "}";
    return o;
}

}
